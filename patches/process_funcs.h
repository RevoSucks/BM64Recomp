// process_api.h
#ifndef __PROCESS_API_H__
#define __PROCESS_API_H__

#include "patch_helpers.h"

// ============================================================================
// Yield reasons (must match host side)
// ============================================================================

#define YIELD_NORMAL     1
#define YIELD_STACKCHECK 2
#define YIELD_TERMINATE  3

// ============================================================================
// Core coroutine API
// ============================================================================

// Initialize the process/coroutine system
// Must be called before any other process functions
DECLARE_FUNC(void, recomp_process_init);

// Create a native coroutine for a process
// process_id:  Unique ID for this process
// entry_func:  MIPS VRAM address of the process entry function
// stack_size:  Requested stack size (will be increased if too small)
// mips_sp:     Initial MIPS stack pointer value
DECLARE_FUNC(void, recomp_process_coro_create, u32 process_id, u32 entry_func, u32 stack_size, u32 mips_sp);

// Destroy a process's coroutine
// process_id: ID of the process whose coroutine should be destroyed
DECLARE_FUNC(void, recomp_process_coro_destroy, u32 process_id);

// Yield from current process back to scheduler
// reason:  Why we're yielding (YIELD_NORMAL, YIELD_STACKCHECK, YIELD_TERMINATE)
// Returns: Value passed by scheduler when resuming
DECLARE_FUNC(s32, recomp_process_yield, s32 reason);

// Switch from scheduler to a process
// process_id:    ID of the process to switch to
// resume_value:  Value to pass to the process when it resumes
// Returns:       Yield reason from the process when it yields back
DECLARE_FUNC(s32, recomp_process_switch_to, u32 process_id, s32 resume_value);

// Check if a process's coroutine has been started
// process_id: ID of the process to check
// Returns:    1 if started, 0 if not
DECLARE_FUNC(s32, recomp_process_is_started, u32 process_id);

// Mark a process's coroutine as started
// process_id: ID of the process to mark
DECLARE_FUNC(void, recomp_process_set_started, u32 process_id);

// ============================================================================
// Debug/query API
// ============================================================================

// Get the number of active process coroutines
DECLARE_FUNC(s32, recomp_process_get_count);

// Check if the process system has been initialized
DECLARE_FUNC(s32, recomp_process_is_initialized);

// Get the currently running process ID
DECLARE_FUNC(u32, recomp_process_get_current_id);

#endif // __PROCESS_API_H__