#include "patches.h"
#include "misc_funcs.h"
#include "PR/os_pi.h"

int dummy = 1;
int dummyBSS;

extern s32 D_8001C3F8;
extern OSIoMesg D_8001C400;
extern OSMesgQueue D_8001C418;

extern void osYieldThread(void);

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

    osPiStartDma(&D_8001C400, 0, 0, devAddr, vAddr, (u32) size, &D_8001C418);
    
    // yield_self_1ms();

    osRecvMesg(&D_8001C418, NULL, 1);
    D_8001C3F8 = 0;
}
