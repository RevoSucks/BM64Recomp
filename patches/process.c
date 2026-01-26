// process.c
#include "patches.h"
#include "process.h"
#include "process_funcs.h"

// ============================================================================
// Helper: Yield to scheduler with a reason
// ============================================================================

static s32 yield_to_scheduler(s32 reason) {
    D_802AC344->yield_value = reason;
    return recomp_process_yield(reason);
}

// ============================================================================
// Initialization
// ============================================================================

RECOMP_PATCH void HuPrcInit(void) {
    s32 i;
    struct Process *ptr;

    // Initialize host-side coroutine system FIRST
    recomp_process_init();

    gProcessCount = 0;
    D_802AC35C = 0;
    D_802AC360 = 0;
    D_802AC340 = NULL;
    D_802AC344 = NULL;

    // Initialize head nodes
    D_802AB200.id = 0x51;
    HuPrcClear(&D_802AB200);
    D_802ABAA0.id = 0x52;
    HuPrcClear(&D_802ABAA0);
    
    // Initialize process pool
    ptr = &D_80063370[0];
    for (i = 0; i < 0x50; i++, ptr++) {
        ptr->id = i;
        ptr->unk18 = (i >> 0x1F);
        ptr->unk1C = i;
        HuPrcClear(ptr);
    }

    // Link free list
    HuPrcLink(&D_802ABAA0, &D_80063370[0]);

    ptr = &D_80063370[0];
    for (i = 0; i < 0x4F; i++) {
        HuPrcLink(ptr, ptr + 1);
        ptr++;
    }

    HuPrcInitDebug();
}

// ============================================================================
// Process termination
// ============================================================================

RECOMP_PATCH void HuPrcEnd(void) {
    s32 id = D_802AC344->id;
    HuPrcTerminate(id);
    // HuPrcTerminate will yield if we terminated ourselves
}

// ============================================================================
// Main scheduler
// ============================================================================

RECOMP_PATCH void HuPrcCall(void) {
    struct Process *process;
    s32 yield_reason;

    D_802AC35C = 1;

    // Update sleep timers for all sleeping processes
    for (process = D_802AB200.next; process != NULL; process = process->next) {
        if (process->exec_mode == 2) {
            if (D_802AC35C < process->sleep_time) {
                process->sleep_time -= D_802AC35C;
            } else {
                process->sleep_time = 0;
                process->exec_mode = 1;
            }
        }
    }

    D_802AC344 = &D_802AB200;
    D_802AC348 = NULL;

    // Main scheduler loop - iterate through all processes
    while (1) {
        // Advance to next process
        D_802AC348 = D_802AC344;
        D_802AC344 = D_802AC344->next;

        // End of process list - scheduler cycle complete
        if (D_802AC344 == NULL) {
            return;
        }

        // Only run processes that are ready (exec_mode == 1)
        if (D_802AC344->exec_mode != 1) {
            continue;
        }

        // Create native coroutine on first run
        if (!D_802AC344->coro_created) {
            // Calculate initial MIPS stack pointer (stack grows downward)
            // sp should point to the bottom of the stack area (highest address)
            u32 mips_sp = (u32)D_802AC344->sptop + D_802AC344->stackSize;
            
            recomp_process_coro_create(
                D_802AC344->id,
                D_802AC344->func,
                D_802AC344->stackSize,
                mips_sp
            );
            D_802AC344->coro_created = 1;
        }

        // Switch to this process and wait for it to yield
        yield_reason = recomp_process_switch_to(D_802AC344->id, 1);

        // Handle special yield reasons
        if (yield_reason == YIELD_STACKCHECK) {
            // Process requested a stack check
            s32 ret = HuPrcStackCheck(D_802AC344);
            if (ret == 0) {
                ret = -1;
            }
            // Store result and resume process
            D_802AC344->yield_value = ret;
            yield_reason = recomp_process_switch_to(D_802AC344->id, ret);
        }
        
        // For YIELD_NORMAL and YIELD_TERMINATE, continue to next process
        // The process state (exec_mode, sleep_time) is already updated by the process itself
    }
}

// ============================================================================
// Stack checking (adapted for native execution)
// ============================================================================

RECOMP_PATCH s32 HuPrcStackCheck(struct Process *process) {
    // In native recompiled code, we don't have direct access to MIPS stack
    // This function is mainly for debugging - return a safe value
    
    func_80297D38((char*)D_802A2E80, process->id);
    func_80297D38((char*)D_802A2E8C, process->pri);
    func_80297D38((char*)D_802A2E98, process->func);

    // These values are placeholders since native stack is different
    D_802AC34C = (s32)process->sptop;
    D_802AC350 = (u32)(process->sptop + process->stackSize);
    D_802AC354 = D_802AC350 - 0x100;

    func_80297D38((char*)D_802A2EA8, D_802AC354);
    func_80297D38((char*)D_802A2EB8, D_802AC34C);
    func_80297D38((char*)D_802A2EC8, D_802AC350);

    // Return "plenty of stack remaining"
    return process->stackSize / 2;
}

// ============================================================================
// Yield functions
// ============================================================================

RECOMP_PATCH s32 HuPrcUserDataGet(void) {
    return D_802AC344->user_data;
}

RECOMP_PATCH s32 HuPrcYieldSave(void) {
    s32 result;
    yield_to_scheduler(YIELD_STACKCHECK);
    result = D_802AC344->yield_value;
    return result;
}

RECOMP_PATCH s32 HuPrcWaitCond(s32 arg0, s32 arg1, s32 arg2) {
    s32 result;

    if (arg2 != 1) {
        return func_80297D90(arg0, arg1, 0);
    }

    // Loop until condition is met, yielding each iteration
    while ((result = func_80297D90(arg0, arg1, 0)) != 0) {
        yield_to_scheduler(YIELD_NORMAL);
    }
    return result;
}

RECOMP_PATCH void HuPrcVSleep(void) {
    yield_to_scheduler(YIELD_NORMAL);
}

RECOMP_PATCH void HuPrcSleep1(void) {
    HuPrcSleep(1);
}

RECOMP_PATCH s32 HuPrcSleep(s32 arg0) {
    s32 result = HuPrcSleepById(D_802AC344->id, arg0);
    yield_to_scheduler(YIELD_NORMAL);
    return result;
}

// ============================================================================
// Process priority functions
// ============================================================================

RECOMP_PATCH s32 HuPrcGetPriById(s32 arg0) {
    struct Process *ptr = D_802ABA9C;

    while (ptr != NULL) {
        if (arg0 == ptr->id) {
            return ptr->pri;
        }
        ptr = ptr->next;
    }
    return -1;
}

RECOMP_PATCH u16 HuPrcPriGet(void) {
    return D_802AC344->pri;
}

// ============================================================================
// Process termination
// ============================================================================

RECOMP_PATCH s32 HuPrcKillRange(s32 arg0, s32 arg1) {
    struct Process *ptr = D_802ABA9C;
    s32 count = 0;

    while (ptr != NULL) {
        if ((ptr->pri >= arg0) && (arg1 >= ptr->pri)) {
            ptr = ptr->prev;
            HuPrcTerminate(ptr->next->id);
            count++;
        }
        ptr = ptr->next;
    }

    return count;
}

RECOMP_PATCH s32 HuPrcTerminate(s32 id) {
    struct Process *process = HuPrcFindById(id);

    if (process == NULL) {
        // Tried to destroy null process - infinite loop (original behavior)
        while(1)
            ;
    }

    struct Process *prev = process->prev;
    s32 terminated_self = (process == D_802AC344);

    // Destroy the native coroutine if it was created
    // (but not our own while we're still in it)
    if (process->coro_created && !terminated_self) {
        recomp_process_coro_destroy(process->id);
    }
    process->coro_created = 0;

    // Update process state
    process->exec_mode = 0;
    HuPrcUnlink(process);
    HuPrcLink(&D_802ABAA0, process);
    gProcessCount--;

    // If we terminated ourselves, yield back to scheduler
    if (terminated_self) {
        D_802AC344 = prev;
        yield_to_scheduler(YIELD_TERMINATE);
        // Should never return here
    }

    return process->id;
}

// ============================================================================
// Sleep/wake functions
// ============================================================================

RECOMP_PATCH s32 HuPrcSleepRange(s32 arg0, s32 arg1, u32 arg2) {
    struct Process *ptr = D_802ABA9C;
    s32 count = 0;

    while (ptr != NULL) {
        if ((ptr->pri >= arg0) && (arg1 >= ptr->pri)) {
            ptr = ptr->prev;
            HuPrcSleepById(ptr->next->id, arg2);
            ptr = ptr->next;
            count++;
        }
        ptr = ptr->next;
    }
    return count;
}

RECOMP_PATCH s32 HuPrcSleepById(u16 arg0, s32 arg1) {
    struct Process *ptr = HuPrcFindById(arg0);

    if (ptr == NULL) {
        return -1;
    }
    
    if (arg1 != 0) {
        ptr->exec_mode = 2;     // Sleeping (timed)
        ptr->sleep_time = arg1;
    } else {
        ptr->exec_mode = 3;     // Paused
    }
    return ptr->id;
}

RECOMP_PATCH s32 HuPrcWakeupRange(s32 arg0, s32 arg1) {
    struct Process *ptr = D_802ABA9C;
    s32 count = 0;

    while (ptr != NULL) {
        if ((ptr->pri >= arg0) && (arg1 >= ptr->pri)) {
            ptr = ptr->prev;
            HuPrcWakeup(ptr->next->id);
            ptr = ptr->next;
            count++;
        }
        ptr = ptr->next;
    }
    return count;
}

RECOMP_PATCH s32 HuPrcWakeup(u16 arg0) {
    s32 result = -1;
    struct Process *proc = HuPrcFindById(arg0);

    if (proc != NULL) {
        switch (proc->exec_mode) {
            case 2:  // Was sleeping (timed)
                proc->sleep_time = 0;
                proc->exec_mode = 1;  // Now running
                break;
            case 3:  // Was paused
                if (proc->sleep_time != 0) {
                    proc->exec_mode = 2;  // Resume timed sleep
                } else {
                    proc->exec_mode = 1;  // Now running
                }
                break;
        }
        result = proc->id;
    }
    return result;
}

// ============================================================================
// Process lookup
// ============================================================================

RECOMP_PATCH struct Process *HuPrcFindById(u16 arg0) {
    struct Process *ptr = D_802AB200.next;

    D_802AC340 = &D_802AB200;
    while (ptr != NULL) {
        if (ptr->id == arg0) {
            return ptr;
        }
        D_802AC340 = ptr;
        ptr = ptr->next;
    }
    return NULL;
}

RECOMP_PATCH s32 HuPrcCurrentIdGet(void) {
    if (D_802AC344 == NULL) {
        return -1;
    }
    return D_802AC344->id;
}

// ============================================================================
// Process creation
// ============================================================================

RECOMP_PATCH void HuPrcCreateChecked(void *func, s32 arg1, void *arg2, s32 arg3, u16 pri) {
    if ((pri < 0x110) || (pri >= 0x3D0)) {
        while(1)
            ;
    }
    HuPrcCreate(func, arg1, arg2, arg3, pri);
}

RECOMP_PATCH s32 HuPrcCreate(void *func, s32 arg1, void *stack, s32 stackSize, u16 pri) {
    struct Process *process;
    struct Process *temp_v0;
    struct Process *insert_after;

    // Check process limit
    if (gProcessCount >= 0x50) {
        return -1;
    }

    process = HuPrcAlloc();
    if (process == NULL) {
        return -1;
    }

    // Validate stack settings
    if ((stack != NULL) && (stackSize == 0)) {
        return -1;
    }

    HuPrcClear(process);
    process->pri = pri;
    process->func = (u32)func;
    process->user_data = arg1;
    process->exec_mode = 1;  // Ready to run

    // Set up stack
    if (stack != NULL) {
        process->sptop = stack;
        process->stackSize = stackSize;
    } else {
        process->sptop = process->stack;
        process->stackSize = 0x800;
    }

    // Find insertion position based on priority
    temp_v0 = HuPrcFindInsertPos(process->pri);
    if (temp_v0 == NULL) {
        insert_after = D_802AC340;
    } else {
        insert_after = temp_v0->prev;
    }

    HuPrcLink(insert_after, process);

    // Native coroutine will be created lazily when first scheduled
    process->coro_created = 0;
    process->yield_value = 0;

    gProcessCount++;
    return process->id;
}

RECOMP_PATCH struct Process *HuPrcAlloc(void) {
    struct Process *proc = D_802AC33C;

    if (proc == NULL) {
        return NULL;
    }
    HuPrcUnlink(proc);
    return proc;
}

// ============================================================================
// Linked list operations
// ============================================================================

RECOMP_PATCH void HuPrcUnlink(struct Process *arg0) {
    struct Process *next = arg0->next;

    if (next != NULL) {
        next->prev = arg0->prev;
        arg0->prev->next = arg0->next;
    } else {
        arg0->prev->next = NULL;
    }
}

RECOMP_PATCH void HuPrcClear(struct Process *arg0) {
    arg0->pri = 0;
    arg0->exec_mode = 0;
    arg0->sleep_time = 0;
    arg0->prev = NULL;
    arg0->next = NULL;
    arg0->coro_created = 0;
    arg0->yield_value = 0;
}

RECOMP_PATCH struct Process *HuPrcFindInsertPos(u16 arg0) {
    struct Process *ptr = D_802AB200.next;

    D_802AC340 = &D_802AB200;
    while (ptr != NULL) {
        if (arg0 < ptr->pri) {
            return ptr;
        }
        D_802AC340 = ptr;
        ptr = ptr->next;
    }
    return NULL;
}

RECOMP_PATCH void HuPrcLink(struct Process *arg0, struct Process *arg1) {
    struct Process *temp_v0;

    arg1->prev = arg0;
    arg1->next = arg0->next;
    arg0->next = arg1;
    temp_v0 = arg1->next;
    if (temp_v0 != NULL) {
        temp_v0->prev = arg1;
    }
}

RECOMP_PATCH void HuPrcInitDebug(void) {
    func_80297D30(0x19, &D_802A0100);
}