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

thread_t *I_ThreadCreate(threadfunc_t runfunc, const char *name, void *userdata);

void I_ThreadDestroy(thread_t *thread);

void I_ThreadJoin(thread_t *thread);

int I_ThreadGetHardwareCount(void);

void I_Yield(void);

typedef struct SDL_Semaphore semaphore_t;

semaphore_t *I_SemaphoreCreate(int32_t initialcount);

void I_SemaphoreAcquire(semaphore_t *sem);

void I_SemaphoreRelease(semaphore_t *sem);

typedef struct { int value; } atomicint_t;

int I_AtomicLoad(atomicint_t *atomic);

int I_AtomicExchange(atomicint_t *atomic, int val);

int I_AtomicIncrement(atomicint_t *atomic, int val);

int I_AtomicDecrement(atomicint_t *atomic, int val);

typedef struct SDL_Mutex mutex_t;

mutex_t *I_MutexCreate(void);

void I_MutexLock(mutex_t *mutex);

void I_MutexUnlock(mutex_t *mutex);

void I_MutexDestroy(mutex_t *mutex);

void I_SetCurrentThreadPriority(void);

#endif
