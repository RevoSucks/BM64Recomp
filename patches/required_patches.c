#include "patches.h"
#include "misc_funcs.h"
#include "PR/os_pi.h"

int dummy = 1;
int dummyBSS;

extern s32 D_8001C3F8;
extern OSIoMesg D_8001C400;
extern OSMesgQueue D_8001C418;

extern void osYieldThread(void);

extern void func_800007F0(s32 id, void* vAddr);

RECOMP_PATCH void load_from_rom_to_addr(void* vAddr, s32 size, u32 devAddr) {
    if (D_8001C3F8 != 0) {
        do {
            osYieldThread();
        } while (D_8001C3F8 != 0);
    }
    D_8001C3F8 = 1;

    // osWritebackDCache(vAddr, size);
    // osInvalDCache(vAddr, size);
    // osInvalICache(vAddr, size);
    
    recomp_load_overlays((u32) devAddr, (void*) vAddr, size);

    osPiStartDma_recomp(&D_8001C400, 0, 0, devAddr, vAddr, (u32) size, &D_8001C418);
    
    // yield_self_1ms();

    osRecvMesg(&D_8001C418, NULL, 1);
    D_8001C3F8 = 0;
}

RECOMP_PATCH void func_8000064C(void* vramAddr, u32 size, u32 devAddr) {
    recomp_load_overlays(devAddr, vramAddr, size); //@recomp patch
    osPiStartDma_recomp(&D_8001C400, 0, 0, devAddr, vramAddr, size, &D_8001C418);
}

RECOMP_PATCH void func_8000059C(s32 devAddr, u32 size, void* vramAddr) {
    if (D_8001C3F8 != 0) {
        do {
            osYieldThread();
        } while (D_8001C3F8 != 0);
    }
    D_8001C3F8 = 1;
    //osWritebackDCache(vramAddr, devAddr);
    recomp_load_overlays(devAddr, vramAddr, size); //@recomp patch
    osPiStartDma_recomp(&D_8001C400, 0, 1, (u32) devAddr, vramAddr, size, &D_8001C418);
    osRecvMesg(&D_8001C418, NULL, 1);
    D_8001C3F8 = 0;
}

RECOMP_PATCH void func_80000524(u32 devAddr, u32 size, void* vramAddr) {
    recomp_load_overlays(devAddr, vramAddr, size); //@recomp patch
    osPiStartDma_recomp(&D_8001C400, 0, 1, devAddr, vramAddr, size, &D_8001C418);
}

RECOMP_PATCH void func_800004D0(void* vramAddr, u32 size, u32 devAddr, s32 arg3) {
    recomp_load_overlays(devAddr, vramAddr, size); //@recomp patch
    osPiStartDma_recomp(&D_8001C400, 0, arg3, devAddr, vramAddr, size, &D_8001C418);
}

RECOMP_PATCH void func_8000083C(s32 id, void *vAddr, s32 arg) {
    void (*volatile localarg)(int);
    // its also possible to match without fake code by omitting arg1 passed to func_800007F0. which would be UB
    func_800007F0(id, vAddr);
    (localarg = vAddr)(arg);
    if(!vAddr) {} // fake check to bump regalloc. see above note
}

extern u32 D_8001BFC0[];
extern void n64main(void);

extern u32 D_80042000[];

extern u32 zerojump_ROM_START;
extern u32 zerojump_ROM_END;

RECOMP_PATCH void set_zero_vaddr_tlb(void) {
    //load_from_rom_to_addr(D_80042000, (u32)&zerojump_ROM_END - (u32)&zerojump_ROM_START, (u32)&zerojump_ROM_START);
    // Not used.
    //osMapTLB(0, 0, NULL, (u32) (((u32) (&D_80042000)) - 0x80000000), -1, -1);
    D_8001BFC0[0] = 0x80019f80; // this feels dirty hardcoding it, but whatever.
}
