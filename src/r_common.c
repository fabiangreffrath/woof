//
// Copyright(C) 2026 Roman Fomin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "doomtype.h"
#include "i_thread.h"
#include "i_system.h"
#include "m_misc.h"
#include "r_common.h"

#include <stdlib.h>

//
// Job system
//
// Each worker owns a fixed-size ring queue. There is exactly one producer
// (the main thread), so the claim/publish sequence needs no CAS: writepos is
// bumped to reserve a slot, the job is stored, then committed is published.
// SDL semaphore operations act as memory barriers between producer and
// consumer, so the job payload written before I_SemaphoreRelease() is
// visible after I_SemaphoreAcquire().
//

#define JOB_QUEUE_SIZE 128
#define JOB_QUEUE_MASK (JOB_QUEUE_SIZE - 1)
#define MAX_JOB_THREADS 32

typedef struct job_s
{
    jobfunc_t func;
    void *userdata;
} job_t;

typedef struct jobthread_s
{
    job_t queue[JOB_QUEUE_SIZE];
    atomicint_t writepos;   // next slot the producer will claim
    atomicint_t committed;  // one past the last published slot
    atomicint_t readpos;    // next slot the worker will consume
    atomicint_t terminate;
    semaphore_t *jobsem; // posted once per queued job
    thread_t *thread;
    int index;
} jobthread_t;

static jobthread_t jobpool[MAX_JOB_THREADS];
static int numjobthreads;   // threads created
static int numactivejobs;   // round-robin window (I_JobsSetActive)
static int nextjobthread;

static semaphore_t *donesem; // worker posts once per completed job
static int pendingjobs;     // jobs submitted since last flush (main thread)

static int JobThreadRun(void *data)
{
    jobthread_t *worker = data;

    for (;;)
    {
        I_SemaphoreAcquire(worker->jobsem);

        if (I_AtomicLoad(&worker->terminate))
        {
            return 0;
        }

        const int pos = I_AtomicLoad(&worker->readpos);

        // The producer only releases jobsem after publishing committed, so
        // the slot is ready here.
        const job_t job = worker->queue[pos & JOB_QUEUE_MASK];
        I_AtomicIncrement(&worker->readpos, 1);

        job.func(job.userdata);

        I_SemaphoreRelease(donesem);
    }
}

void R_JobsInit(int numthreads)
{
    if (numthreads < 0)
    {
        numthreads = 0;
    }
    if (numthreads > MAX_JOB_THREADS)
    {
        numthreads = MAX_JOB_THREADS;
    }

    donesem = I_SemaphoreCreate(0);
    pendingjobs = 0;
    nextjobthread = 0;

    for (int i = 0; i < numthreads; i++)
    {
        jobthread_t *worker = &jobpool[i];
        worker->index = i;
        I_AtomicExchange(&worker->writepos, 0);
        I_AtomicExchange(&worker->committed, 0);
        I_AtomicExchange(&worker->readpos, 0);
        I_AtomicExchange(&worker->terminate, 0);
        worker->jobsem = I_SemaphoreCreate(0);
        char name[8];
        M_snprintf(name, sizeof(name), "Job %d", i);
        worker->thread = I_ThreadCreate(JobThreadRun, name, worker);
    }

    numjobthreads = numthreads;
    numactivejobs = numthreads;
}

void R_JobsShutdown(void)
{
    if (!numjobthreads)
    {
        return;
    }

    // Flush anything still queued, then wake every worker so it can see the
    // terminate flag.
    R_JobsFlush();

    for (int i = 0; i < numjobthreads; i++)
    {
        I_AtomicExchange(&jobpool[i].terminate, 1);
        I_SemaphoreRelease(jobpool[i].jobsem);
    }

    for (int i = 0; i < numjobthreads; i++)
    {
        I_ThreadJoin(jobpool[i].thread);
    }

    numjobthreads = 0;
    numactivejobs = 0;
}

void R_JobsAdd(jobfunc_t func, void *userdata)
{
    if (numactivejobs <= 0)
    {
        func(userdata);
        return;
    }

    jobthread_t *worker = &jobpool[nextjobthread];
    if (++nextjobthread >= numactivejobs)
    {
        nextjobthread = 0;
    }

    // Wait for free space if the worker has not caught up yet.
    while (I_AtomicLoad(&worker->writepos) - I_AtomicLoad(&worker->readpos)
           >= JOB_QUEUE_SIZE)
    {
        I_Yield();
    }

    const int pos = I_AtomicIncrement(&worker->writepos, 1);
    worker->queue[pos & JOB_QUEUE_MASK].func = func;
    worker->queue[pos & JOB_QUEUE_MASK].userdata = userdata;
    I_AtomicExchange(&worker->committed, pos + 1);

    pendingjobs++;
    I_SemaphoreRelease(worker->jobsem);
}

void R_JobsFlush(void)
{
    while (pendingjobs > 0)
    {
        I_SemaphoreAcquire(donesem);
        pendingjobs--;
    }
}

void R_JobsSetActive(int numactive)
{
    if (numactive < 0)
    {
        numactive = 0;
    }
    if (numactive > numjobthreads)
    {
        numactive = numjobthreads;
    }
    numactivejobs = numactive;
    if (nextjobthread >= numactivejobs && numactivejobs > 0)
    {
        nextjobthread = 0;
    }
}

//
// Frame scratch allocator
// Each thread (main + workers) gets its own bump-pointer arena.
// All allocations are freed at once by I_FrameScratchReset() at frame start.
//

static struct {
    byte *base;
    int size;
    int offset;
} framescratch[MAX_JOB_THREADS + 1];  // +1 for main thread

static int framescratch_initialized = 0;

int R_JobsNumThreads(void)
{
    return numjobthreads;
}

void R_FrameScratchInit(int bytes_per_thread)
{
    if (framescratch_initialized)
    {
        return;
    }

    // Main thread is index 0, workers are 1..numjobthreads
    const int total_threads = numjobthreads + 1;

    for (int i = 0; i < total_threads; i++)
    {
        framescratch[i].base = malloc(bytes_per_thread);
        if (!framescratch[i].base)
        {
            I_Error("Failed to allocate %d bytes for thread %d", bytes_per_thread, i);
        }
        framescratch[i].size = bytes_per_thread;
        framescratch[i].offset = 0;
    }

    framescratch_initialized = 1;
}

void R_FrameScratchShutdown(void)
{
    if (!framescratch_initialized)
    {
        return;
    }

    const int total_threads = numjobthreads + 1;

    for (int i = 0; i < total_threads; i++)
    {
        free(framescratch[i].base);
        framescratch[i].base = NULL;
        framescratch[i].size = 0;
        framescratch[i].offset = 0;
    }

    framescratch_initialized = 0;
}

// Called by main thread at start of frame, and by each worker at start of its job
void R_FrameScratchReset(void)
{
    // Main thread always has index 0
    // Workers get their index from thread-local storage
    extern THREADLOCAL int r_worker_scratch_index;
    int idx = r_worker_scratch_index;

    if (idx < 0 || idx > numjobthreads)
    {
        idx = 0; // fallback to main thread
    }

    framescratch[idx].offset = 0;
}

// Aligned allocation from current thread's frame scratch
void *R_FrameScratchAlloc(int bytes, int align)
{
    int idx = r_worker_scratch_index;

    if (idx < 0 || idx > numjobthreads)
    {
        idx = 0;
    }

    byte *ptr = framescratch[idx].base + framescratch[idx].offset;

    // Align up
    uintptr_t aligned = ((uintptr_t)ptr + align - 1) & ~(align - 1);
    int needed = (aligned - (uintptr_t)framescratch[idx].base) + bytes;

    if (needed > framescratch[idx].size)
    {
        I_Error("Thread %d scratch exhausted (%d/%d)", idx, needed,
                framescratch[idx].size);
    }

    framescratch[idx].offset = needed;
    return (void *)aligned;
}
