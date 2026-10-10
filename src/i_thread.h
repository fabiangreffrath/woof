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

#ifndef I_THREAD_H
#define I_THREAD_H

#include <stdint.h>

typedef struct SDL_Thread thread_t;
typedef int (*threadfunc_t)(void *data);

thread_t *I_ThreadCreate(threadfunc_t runfunc, void *userdata);

void I_ThreadDestroy(thread_t *thread);

void I_ThreadJoin(thread_t *thread);

int I_ThreadGetHardwareCount(void);

void I_Yield(void);

typedef struct SDL_Semaphore semaphore_t;

semaphore_t *I_SemaphoreCreate(int32_t initialcount);

void I_SemaphoreAcquire(semaphore_t *sem);

void I_SemaphoreRelease(semaphore_t *sem);

typedef struct SDL_AtomicInt atomic_t;
typedef int atomicval_t;

atomicval_t I_AtomicLoad(atomic_t *atomic);

atomicval_t I_AtomicExchange(atomic_t *atomic, atomicval_t val);

atomicval_t I_AtomicIncrement(atomic_t *atomic, atomicval_t val);

atomicval_t I_AtomicDecrement(atomic_t *atomic, atomicval_t val);

//
// Job system for the multithreaded renderer.
//
// A pool of persistent worker threads, each with its own job queue. Jobs are
// { function pointer, userdata } pairs. I_JobsAdd() dispatches round-robin
// over the active workers (or runs the job inline when no workers are
// active), I_JobsFlush() blocks until every submitted job has finished.
// Workers sleep on semaphores between jobs - no spin waiting.
//
// All calls must be made from the same (main) thread.
//

typedef void (*jobfunc_t)(void *userdata);

void I_JobsInit(int numthreads);

void I_JobsShutdown(void);

void I_JobsAdd(jobfunc_t func, void *userdata);

void I_JobsFlush(void);

// Restrict dispatch to the first numactive workers; 0 makes I_JobsAdd()
// run jobs inline. Already queued jobs still execute and are drained by
// I_JobsFlush().
void I_JobsSetActive(int numactive);

int I_JobsNumThreads(void);

#endif
