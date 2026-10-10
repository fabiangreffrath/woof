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

#include "i_thread.h"

#include <SDL3/SDL.h>

thread_t *I_ThreadCreate(threadfunc_t runfunc, void *userdata)
{
    return SDL_CreateThread(runfunc, NULL, userdata);
}

void I_ThreadDestroy(thread_t *thread)
{
    if (thread)
    {
        SDL_DetachThread(thread);
    }
}

void I_ThreadJoin(thread_t *thread)
{
    SDL_WaitThread(thread, NULL);
}

int I_ThreadGetHardwareCount(void)
{
    return SDL_GetNumLogicalCPUCores();
}

void I_Yield(void)
{
    // SDL_Delay(0) yields the remainder of the current time-slice.
    SDL_Delay(0);
}

semaphore_t *I_SemaphoreCreate(int32_t initialcount)
{
    // SDL_CreateSemaphore takes a Uint32 initial value.  A negative
    // initial count is clamped to 0.
    Uint32 init = (initialcount < 0) ? 0u : (Uint32)initialcount;
    return SDL_CreateSemaphore(init);
}

void I_SemaphoreAcquire(semaphore_t *sem)
{
    // SDL_WaitSemaphore suspends the calling thread until the semaphore has
    // a positive value, then atomically decrements it.
    SDL_WaitSemaphore(sem);
}

void I_SemaphoreRelease(semaphore_t *sem)
{
    // SDL_SignalSemaphore atomically increments the semaphore and wakes a
    // waiter.
    SDL_SignalSemaphore(sem);
}

atomicval_t I_AtomicLoad(atomic_t *atomic)
{
    return SDL_GetAtomicInt(atomic);
}

atomicval_t I_AtomicExchange(atomic_t *atomic, atomicval_t val)
{
    return SDL_SetAtomicInt(atomic, val);
}

atomicval_t I_AtomicIncrement(atomic_t *atomic, atomicval_t val)
{
    return SDL_AddAtomicInt(atomic, val);
}

atomicval_t I_AtomicDecrement(atomic_t *atomic, atomicval_t val)
{
    return SDL_AddAtomicInt(atomic, -val);
}

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
    atomic_t writepos;   // next slot the producer will claim
    atomic_t committed;  // one past the last published slot
    atomic_t readpos;    // next slot the worker will consume
    atomic_t terminate;
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

void I_JobsInit(int numthreads)
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
        worker->thread = I_ThreadCreate(JobThreadRun, worker);
    }

    numjobthreads = numthreads;
    numactivejobs = numthreads;
}

void I_JobsShutdown(void)
{
    if (!numjobthreads)
    {
        return;
    }

    // Flush anything still queued, then wake every worker so it can see the
    // terminate flag.
    I_JobsFlush();

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

void I_JobsAdd(jobfunc_t func, void *userdata)
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

void I_JobsFlush(void)
{
    while (pendingjobs > 0)
    {
        I_SemaphoreAcquire(donesem);
        pendingjobs--;
    }
}

void I_JobsSetActive(int numactive)
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

int I_JobsNumThreads(void)
{
    return numjobthreads;
}
