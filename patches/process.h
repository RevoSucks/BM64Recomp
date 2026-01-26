#ifndef __BM64_PROCESS_H__
#define __BM64_PROCESS_H__

#include "patches.h"
#include "process_funcs.h"

// Process structure - modified to remove jmpBuf
typedef struct Process {
    /* 0x000 */ u16 id;
    /* 0x002 */ u16 pri;
    /* 0x004 */ u32 exec_mode;       // 0=dead, 1=running, 2=sleeping(timed), 3=paused
    /* 0x008 */ u32 sleep_time;
    /* 0x00C */ u32 func;
    /* 0x010 */ u32 user_data;
    /* 0x014 */ u32 unk14;
    /* 0x018 */ u32 unk18;
    /* 0x01C */ u32 unk1C;
    /* 0x020 */ u32 stackSize;
    /* 0x024 */ u8 *sptop;
    /* 0x028 */ u8 stack[0x800];
    
    // Replacing jmpBuf (was 0x70 bytes) with coroutine state
    /* 0x828 */ s32 coro_created;    // Has native coroutine been created?
    /* 0x82C */ s32 yield_value;     // Value passed during yield/resume
    /* 0x830 */ u8 _padding[0x68];   // Maintain struct layout
    
    /* 0x898 */ struct Process *prev;
    /* 0x89C */ struct Process *next;
} Process;

// Debug format strings
extern u8 D_802A2E80[];  // "id    : %d"
extern u8 D_802A2E8C[];  // "pri   : %d"
extern u8 D_802A2E98[];  // "func  : %08lx"
extern u8 D_802A2EA8[];  // "sp    : %08lx"
extern u8 D_802A2EB8[];  // "sptop : %08lx"
extern u8 D_802A2EC8[];  // "spbtm : %08lx"

// External functions
extern void func_80297D38(char *str, ...);
extern s32 func_80297D90(s32, s32, s32);
extern void func_80297D30(s32, void*);

// Process pool
extern struct Process D_80063370[];

// Global state
extern s32 gProcessCount;
extern s32 D_802AC360;
extern u8 D_802A0100[];

extern struct Process D_802AB200;
extern struct Process D_802ABAA0;
extern struct Process *D_802ABA9C;

extern struct Process *D_802AC33C;
extern struct Process *D_802AC340;
extern struct Process *D_802AC344;           // Current process
extern struct Process *D_802AC348;
extern s32 D_802AC34C;
extern u32 D_802AC350;
extern s32 D_802AC354;
extern u32 D_802AC35C;

// Function declarations
void HuPrcInit(void);
void HuPrcEnd(void);
void HuPrcCall(void);
s32 HuPrcStackCheck(struct Process *process);
s32 HuPrcUserDataGet(void);
s32 HuPrcYieldSave(void);
s32 HuPrcWaitCond(s32 arg0, s32 arg1, s32 arg2);
void HuPrcVSleep(void);
void HuPrcSleep1(void);
s32 HuPrcSleep(s32 arg0);
s32 HuPrcGetPriById(s32 arg0);
u16 HuPrcPriGet(void);
s32 HuPrcKillRange(s32 arg0, s32 arg1);
s32 HuPrcTerminate(s32 id);
s32 HuPrcSleepRange(s32 arg0, s32 arg1, u32 arg2);
s32 HuPrcSleepById(u16 arg0, s32 arg1);
s32 HuPrcWakeupRange(s32 arg0, s32 arg1);
s32 HuPrcWakeup(u16 arg0);
struct Process *HuPrcFindById(u16 arg0);
s32 HuPrcCurrentIdGet(void);
void HuPrcCreateChecked(void *func, s32 arg1, void* arg2, s32 arg3, u16 pri);
s32 HuPrcCreate(void *func, s32 arg1, void* arg2, s32 arg3, u16 pri);
struct Process *HuPrcAlloc(void);
void HuPrcUnlink(struct Process* arg0);
void HuPrcClear(struct Process* arg0);
struct Process *HuPrcFindInsertPos(u16 arg0);
void HuPrcLink(struct Process* arg0, struct Process* arg1);
void HuPrcInitDebug(void);

#endif