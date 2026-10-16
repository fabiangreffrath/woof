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
#include <stdint.h>

thread_t *I_ThreadCreate(threadfunc_t runfunc, const char *name, void *userdata)
{
    return SDL_CreateThread(runfunc, name, userdata);
}

void I_ThreadDestroy(thread_t *thread)
{
    SDL_DetachThread(thread);
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

int I_AtomicLoad(atomicint_t *atomic)
{
    return SDL_GetAtomicInt((SDL_AtomicInt *)atomic);
}

int I_AtomicExchange(atomicint_t *atomic, int val)
{
    return SDL_SetAtomicInt((SDL_AtomicInt *)atomic, val);
}

int I_AtomicIncrement(atomicint_t *atomic, int val)
{
    return SDL_AddAtomicInt((SDL_AtomicInt *)atomic, val);
}

int I_AtomicDecrement(atomicint_t *atomic, int val)
{
    return SDL_AddAtomicInt((SDL_AtomicInt *)atomic, -val);
}
