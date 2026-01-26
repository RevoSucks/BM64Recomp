// process_coroutines.cpp

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <array>

// ============================================================================
// Configuration
// ============================================================================

// Use minicoro as the coroutine backend
#define MINICORO_IMPL
#include "minicoro.h"

// Maximum number of concurrent processes (matches game's process pool size)
static constexpr size_t MAX_PROCESSES = 0x50;

// Minimum native stack size for coroutines (native code needs more than MIPS)
static constexpr size_t MIN_NATIVE_STACK = 0x80000;  // 512KB

// Enable debug logging
#define PROCESS_DEBUG

// ============================================================================
// Yield reasons (must match MIPS side in process_funcs.h)
// ============================================================================

static constexpr int32_t YIELD_NORMAL     = 1;
static constexpr int32_t YIELD_STACKCHECK = 2;
static constexpr int32_t YIELD_TERMINATE  = 3;

// ============================================================================
// Debug logging
// ============================================================================

#ifdef PROCESS_DEBUG
static void process_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[Process] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(args);
}
#else
#define process_log(...) ((void)0)
#endif

// ============================================================================
// Process slot state
// ============================================================================

enum class ProcessState {
    Free,       // Slot is available
    Created,    // Coroutine created but not yet started
    Running,    // Coroutine is actively executing
    Suspended,  // Coroutine yielded, waiting to resume
    Dead        // Coroutine finished, awaiting cleanup
};

struct ProcessSlot {
    ProcessState state = ProcessState::Free;
    uint32_t process_id = 0;
    uint32_t entry_func = 0;        // MIPS VRAM address of entry function
    uint32_t mips_sp = 0;           // Initial MIPS stack pointer
    mco_coro* coro = nullptr;       // Native coroutine handle
    recomp_context ctx;             // MIPS register context for this process
    
    void reset() {
        if (coro != nullptr) {
            mco_destroy(coro);
            coro = nullptr;
        }
        state = ProcessState::Free;
        process_id = 0;
        entry_func = 0;
        mips_sp = 0;
        std::memset(&ctx, 0, sizeof(ctx));
    }
};

// ============================================================================
// Global state
// ============================================================================

// Fixed array of process slots - stable memory addresses
static std::array<ProcessSlot, MAX_PROCESSES> g_slots;

// Currently executing process slot (nullptr when in scheduler)
static ProcessSlot* g_current_slot = nullptr;

// Communication between scheduler and processes
static int32_t g_yield_reason = 0;
static int32_t g_resume_value = 0;

// Entry context for new coroutines
static uint8_t* g_entry_rdram = nullptr;

// System initialization flag
static bool g_initialized = false;

// ============================================================================
// Slot management
// ============================================================================

static ProcessSlot* find_slot_by_id(uint32_t process_id) {
    for (auto& slot : g_slots) {
        if (slot.state != ProcessState::Free && slot.process_id == process_id) {
            return &slot;
        }
    }
    return nullptr;
}

static ProcessSlot* find_free_slot() {
    for (auto& slot : g_slots) {
        if (slot.state == ProcessState::Free) {
            return &slot;
        }
    }
    return nullptr;
}

// ============================================================================
// Coroutine entry point
// ============================================================================

static void process_coro_entry(mco_coro* co) {
    (void)co;
    
    // Get our slot from the global pointer (set before mco_resume)
    ProcessSlot* slot = g_current_slot;
    if (slot == nullptr) {
        fprintf(stderr, "[Process] FATAL: process_coro_entry called with null g_current_slot\n");
        g_yield_reason = YIELD_TERMINATE;
        mco_yield(mco_running());
        return;
    }
    
    uint8_t* rdram = g_entry_rdram;
    if (rdram == nullptr) {
        fprintf(stderr, "[Process] FATAL: process_coro_entry called with null g_entry_rdram\n");
        g_yield_reason = YIELD_TERMINATE;
        mco_yield(mco_running());
        return;
    }
    
    // Look up the entry function
    recomp_func_t* entry_func = get_function(static_cast<int32_t>(slot->entry_func));
    if (entry_func == nullptr) {
        fprintf(stderr, "[Process] ERROR: Could not find function at 0x%08X for process %u\n",
                slot->entry_func, slot->process_id);
        g_yield_reason = YIELD_TERMINATE;
        mco_yield(mco_running());
        return;
    }
    
    // Initialize MIPS context
    slot->ctx.r29 = slot->mips_sp;  // Stack pointer
    slot->ctx.mips3_float_mode = 0;
    slot->ctx.f_odd = &slot->ctx.f0.u32h;
    
    process_log("Process %u starting at 0x%08X (sp=0x%08X)", 
                slot->process_id, slot->entry_func, slot->mips_sp);
    
    slot->state = ProcessState::Running;
    
    // Call the process entry function
    // This will run until the process yields or terminates
    entry_func(rdram, &slot->ctx);
    
    // Process returned without explicitly terminating
    process_log("Process %u returned normally", slot->process_id);
    slot->state = ProcessState::Dead;
    g_yield_reason = YIELD_TERMINATE;
    mco_yield(mco_running());
    
    // Safety: should never reach here
    while (1) {
        mco_yield(mco_running());
    }
}

// ============================================================================
// API: Initialize process system
// ============================================================================

extern "C" void recomp_process_init(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    
    process_log("Initializing process system");
    
    // Reset all slots
    for (auto& slot : g_slots) {
        slot.reset();
    }
    
    g_current_slot = nullptr;
    g_yield_reason = 0;
    g_resume_value = 0;
    g_entry_rdram = nullptr;
    g_initialized = true;
    
    process_log("Process system initialized with %zu slots", MAX_PROCESSES);
}

// ============================================================================
// API: Create a coroutine for a process
// ============================================================================

extern "C" void recomp_process_coro_create(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    
    uint32_t process_id = _arg<0, uint32_t>(rdram, ctx);
    uint32_t entry_func = _arg<1, uint32_t>(rdram, ctx);
    uint32_t stack_size = _arg<2, uint32_t>(rdram, ctx);
    uint32_t mips_sp = _arg<3, uint32_t>(rdram, ctx);
    
    process_log("Creating coroutine: pid=%u entry=0x%08X stack=%u mips_sp=0x%08X",
                process_id, entry_func, stack_size, mips_sp);
    
    // Check if this process already has a slot
    ProcessSlot* slot = find_slot_by_id(process_id);
    
    if (slot != nullptr) {
        // Reuse existing slot - destroy old coroutine first
        process_log("Reusing slot for process %u (old state=%d)", 
                    process_id, static_cast<int>(slot->state));
        
        if (slot->coro != nullptr) {
            mco_destroy(slot->coro);
            slot->coro = nullptr;
        }
    } else {
        // Allocate a new slot
        slot = find_free_slot();
        if (slot == nullptr) {
            fprintf(stderr, "[Process] ERROR: No free slots for process %u\n", process_id);
            return;
        }
    }
    
    // Ensure adequate native stack size
    size_t native_stack = stack_size;
    if (native_stack < MIN_NATIVE_STACK) {
        native_stack = MIN_NATIVE_STACK;
    }
    
    // Create the coroutine
    mco_desc desc = mco_desc_init(process_coro_entry, native_stack);
    mco_result res = mco_create(&slot->coro, &desc);
    
    if (res != MCO_SUCCESS) {
        fprintf(stderr, "[Process] ERROR: Failed to create coroutine for process %u: %s\n",
                process_id, mco_result_description(res));
        slot->reset();
        return;
    }
    
    // Initialize slot
    slot->state = ProcessState::Created;
    slot->process_id = process_id;
    slot->entry_func = entry_func;
    slot->mips_sp = mips_sp;
    std::memset(&slot->ctx, 0, sizeof(slot->ctx));
    
    process_log("Coroutine created for process %u in slot %zu", 
                process_id, slot - g_slots.data());
}

// ============================================================================
// API: Destroy a process's coroutine
// ============================================================================

extern "C" void recomp_process_coro_destroy(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    
    uint32_t process_id = _arg<0, uint32_t>(rdram, ctx);
    
    process_log("Destroying coroutine for process %u", process_id);
    
    ProcessSlot* slot = find_slot_by_id(process_id);
    if (slot == nullptr) {
        process_log("No slot found for process %u", process_id);
        return;
    }
    
    // Don't destroy the currently running coroutine
    if (slot == g_current_slot) {
        process_log("Cannot destroy currently running process %u", process_id);
        return;
    }
    
    slot->reset();
    process_log("Process %u destroyed", process_id);
}

// ============================================================================
// API: Yield from current process back to scheduler
// ============================================================================

extern "C" void recomp_process_yield(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    
    int32_t reason = _arg<0, int32_t>(rdram, ctx);
    
    ProcessSlot* slot = g_current_slot;
    if (slot == nullptr) {
        fprintf(stderr, "[Process] ERROR: yield called with no current process\n");
        _return(ctx, static_cast<int32_t>(-1));
        return;
    }
    
    process_log("Process %u yielding with reason %d", slot->process_id, reason);
    
    // Update state
    if (reason == YIELD_TERMINATE) {
        slot->state = ProcessState::Dead;
    } else {
        slot->state = ProcessState::Suspended;
    }
    
    // Store yield reason for scheduler
    g_yield_reason = reason;
    
    // Yield back to scheduler
    mco_yield(mco_running());
    
    // Resumed by scheduler
    slot->state = ProcessState::Running;
    
    process_log("Process %u resumed with value %d", slot->process_id, g_resume_value);
    
    _return(ctx, g_resume_value);
}

// ============================================================================
// API: Switch from scheduler to a process
// ============================================================================

extern "C" void recomp_process_switch_to(uint8_t* rdram, recomp_context* ctx) {
    uint32_t process_id = _arg<0, uint32_t>(rdram, ctx);
    int32_t resume_value = _arg<1, int32_t>(rdram, ctx);
    
    ProcessSlot* slot = find_slot_by_id(process_id);
    if (slot == nullptr) {
        fprintf(stderr, "[Process] ERROR: switch_to called for unknown process %u\n", process_id);
        _return(ctx, static_cast<int32_t>(YIELD_TERMINATE));
        return;
    }
    
    if (slot->coro == nullptr) {
        fprintf(stderr, "[Process] ERROR: process %u has no coroutine\n", process_id);
        _return(ctx, static_cast<int32_t>(YIELD_TERMINATE));
        return;
    }
    
    // Check coroutine state
    mco_state coro_state = mco_status(slot->coro);
    if (coro_state == MCO_DEAD) {
        process_log("Process %u coroutine is dead", process_id);
        _return(ctx, static_cast<int32_t>(YIELD_TERMINATE));
        return;
    }
    
    process_log("Switching to process %u with value %d", process_id, resume_value);
    
    // Set up context for the process
    g_current_slot = slot;
    g_entry_rdram = rdram;
    g_resume_value = resume_value;
    g_yield_reason = 0;
    
    // Resume the coroutine
    mco_result res = mco_resume(slot->coro);
    
    // Clear current slot (we're back in scheduler)
    g_current_slot = nullptr;
    
    if (res != MCO_SUCCESS) {
        fprintf(stderr, "[Process] ERROR: mco_resume failed for process %u: %s\n",
                process_id, mco_result_description(res));
        slot->state = ProcessState::Dead;
        _return(ctx, static_cast<int32_t>(YIELD_TERMINATE));
        return;
    }
    
    process_log("Process %u yielded with reason %d", process_id, g_yield_reason);
    
    // Return the yield reason to the scheduler
    _return(ctx, g_yield_reason);
}

// ============================================================================
// API: Check if a process coroutine has been started
// ============================================================================

extern "C" void recomp_process_is_started(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    
    uint32_t process_id = _arg<0, uint32_t>(rdram, ctx);
    
    ProcessSlot* slot = find_slot_by_id(process_id);
    if (slot == nullptr) {
        _return(ctx, static_cast<int32_t>(0));
        return;
    }
    
    // Process is "started" if it's past the Created state
    int32_t started = (slot->state != ProcessState::Free && 
                       slot->state != ProcessState::Created) ? 1 : 0;
    _return(ctx, started);
}

// ============================================================================
// API: Mark a process coroutine as started  
// ============================================================================

extern "C" void recomp_process_set_started(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    
    uint32_t process_id = _arg<0, uint32_t>(rdram, ctx);
    
    ProcessSlot* slot = find_slot_by_id(process_id);
    if (slot != nullptr && slot->state == ProcessState::Created) {
        slot->state = ProcessState::Suspended;
    }
}

// ============================================================================
// API: Get process context pointer
// Note: This returns a native pointer in r2/r3 (lo/hi halves) since
// MIPS only supports 32-bit return values.
// ============================================================================

extern "C" void recomp_process_get_ctx(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    
    uint32_t process_id = _arg<0, uint32_t>(rdram, ctx);
    
    ProcessSlot* slot = find_slot_by_id(process_id);
    if (slot == nullptr) {
        ctx->r2 = 0;
        ctx->r3 = 0;
        return;
    }
    
    // Return native pointer as two 32-bit halves
    uintptr_t ptr = reinterpret_cast<uintptr_t>(&slot->ctx);
    ctx->r2 = static_cast<uint64_t>(ptr & 0xFFFFFFFF);         // Low 32 bits
    ctx->r3 = static_cast<uint64_t>((ptr >> 32) & 0xFFFFFFFF); // High 32 bits
}

// ============================================================================
// Stub implementations for setjmp/longjmp
// These are never actually called in the recompiled code but may be
// referenced by function tables.
// ============================================================================

extern "C" void longjmp_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    fprintf(stderr, "[Process] ERROR: longjmp_recomp called - this should not happen!\n");
}

extern "C" void setjmp_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    fprintf(stderr, "[Process] ERROR: setjmp_recomp called - this should not happen!\n");
}
