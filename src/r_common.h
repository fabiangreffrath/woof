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

#ifndef R_COMMON_H
#define R_COMMON_H

// [MT] Maximum number of parallel render contexts (column slabs).
#define MAX_RENDER_CONTEXTS 16

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

void R_JobsInit(int numthreads);

void R_JobsShutdown(void);

void R_JobsAdd(jobfunc_t func, void *userdata);

void R_JobsFlush(void);

// Restrict dispatch to the first numactive workers; 0 makes I_JobsAdd()
// run jobs inline. Already queued jobs still execute and are drained by
// I_JobsFlush().

void R_JobsSetActive(int numactive);

int R_JobsNumThreads(void);

//
// Frame scratch allocator (per-thread bump pointer)
// All allocations are freed at once by I_FrameScratchReset() at frame start.
// Workers must call I_FrameScratchReset() before starting their frame work.
//

extern THREADLOCAL int r_worker_scratch_index;

void R_FrameScratchInit(int bytes_per_thread);

void R_FrameScratchShutdown(void);

void R_FrameScratchReset(void);

void *R_FrameScratchAlloc(int bytes, int align);

#endif
