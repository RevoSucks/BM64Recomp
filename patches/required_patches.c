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

extern s32 D_802A5328;
extern s32 D_802A5324;

extern u8 gOverlaySizes[][2];
extern u8 D_8019C0D0[][3];

typedef int (*OverlayCallback)(void);

void func_80226368(s32);
void func_80297D58(s32, s32);
void func_80297D50(s32, s32);
void func_80292BD0(s32 arg0, u8* arg1, s32 arg2);
void func_80297D60(s32, s32);
void func_802266E8(s32);
s32 func_80226604(s32, s32);
s32 func_802266DC(s32);
void func_8022616C(void*, s32, s32, s32);


u32 gGlobalOverlayROM_Base   = 0x00000000; // the currently set ROM offset of the OVL being loaded.
u32 gGlobalOverlayROM_Offset = 0x00000000; // the offset of the overlay

extern u32 D_802AC5DC;

// -------------------------
extern int func_ovl3_1_80043000(void); // ovl_3_subovl_1 (Copyright Screen)
extern int func_ovl2_5_80043000(void); // ovl_2_subovl_5 (Intro) ?

RECOMP_PATCH void func_8022691C(s32 arg0) {
    s32 sp44;
    s32 sp40;
    s32 sp3C;
    s32 temp_a0;
    s32 temp_v0;
    s32 target;
    s32 pad[4];

    if (arg0 == D_802A5328) {
        return;
    }

    D_802A5328 = arg0;
    temp_a0 = (gOverlaySizes[arg0][0] << 17) + (gOverlaySizes[arg0][1] << 11);
    if (temp_a0 != D_802A5324) {
        D_802A5324 = temp_a0;
        func_802266E8(temp_a0);
    }
    sp44 = func_80226604(0, 1);
    sp40 = func_802266DC(sp44);
    func_8022616C((s32* )D_8019C0D0, 1, sp40, sp44);
    target = D_8019C0D0[0][0];
    for (sp40 = 0; sp40 < target; sp40++) {
        sp3C = (D_8019C0D0[sp40][1] << 8) + D_8019C0D0[sp40][2];
        if (sp3C == arg0) {
            sp3C = D_8019C0D0[sp40 + 1][0];
            break;
        }
    }
    func_80226368(sp44);
    sp44 = func_80226604(sp3C, 1);
    func_8022616C(&sp40, 4, 1, sp44);
    func_80297D58(0x80043000, 0x20000);
    func_80297D50(0x80043000, 0x20000);
    func_80292BD0(sp44, (u8*)0x80043000, sp40);
    func_80297D60(0x80043000, 0x20000);
    func_80226368(sp44);
    if (D_802A5324 != 0x300000) {
        D_802A5324 = 0x300000;
        func_802266E8(0x300000);
    }
    // We dont need to store the variable, as we are interpreting the file ID later when the overlay gets executed.
    //D_802A532C = ((OverlayCallback)0x80043000)();

    switch(D_802AC5DC) {
        case 0x00:
            recomp_load_overlays(0x008257F0, (void*)0x80043000, 0x00002F80);
            return; // Copyright
        case 0x1B:
            recomp_load_overlays(0x0081AA10, (void*)0x80043000, 0x000068D0);
            return; // Intro
    }
}

extern u32 D_802A1D90;
extern u8 *D_802B0310;
extern u16 D_802B0314;

extern void func_8029ADCC(void*);
extern s32 func_80226304(s32);
u8* Libc_Memset(u8* arg0, u8* arg1, s32 arg2);

extern u8 *func_802998EC(int);

RECOMP_PATCH void func_80292BD0(s32 arg0, u8* arg1, s32 arg2) {
    s32 size;
    s32 i;
    u16 var_s5;
    s32 temp_s4;
    u8* sp44;
    s32 temp_v0_3;

    sp44 = arg1;
    D_802A1D90 = 1;
    D_802B0310 = func_802998EC(0x400);
    D_802B0314 = 0x3BE;
    Libc_Memset(D_802B0310, 0, 0x400);
    var_s5 = 0;
    while (arg2 != 0) {
        var_s5 >>= 1;
        if (!(var_s5 & 0x100)) {
            var_s5 = func_80226304(arg0) | 0xFF00;
        }
        if (var_s5 & 1) {
            D_802B0310[D_802B0314] = (*arg1++) = func_80226304(arg0);
            arg2 -= 1;
            D_802B0314++;
            D_802B0314 &= 0x3FF;
        } else {
            temp_s4 = func_80226304(arg0);
            temp_v0_3 = func_80226304(arg0);
            temp_s4 |= ((temp_v0_3 & ~0x3F) * 4);
            gGlobalOverlayROM_Offset = temp_s4;
            size = (temp_v0_3 & 0x3F) + 3;
            for (i = 0; i < size; i++) {
                D_802B0310[D_802B0314] = (*arg1++) = D_802B0310[(temp_s4 + i) & 0x3FF];
                D_802B0314++;
                D_802B0314 &= 0x3FF;
            }
            arg2 -= i;
        }
    }
    func_80297D60(((u32) (sp44 + 0xF) >> 4) * 0x10, ((u32) (arg2 + 0xF) >> 4) * 0x10);
    func_8029ADCC(D_802B0310);
    D_802A1D90 = 0;
}

extern int (*D_802A532C)();

extern void func_ovl3_1_80044108();
extern void func_ovl2_5_80047AF4();

// execute overlay
RECOMP_PATCH void func_802268F4(void) {
    switch(D_802AC5DC) {
        case 0x00: 
            func_ovl3_1_80044108();
            return; // Copyright
        case 0x1B: 
            func_ovl2_5_80047AF4(); 
            return; // Intro
        default: // unsupported overlay.
            while(1)
               ;
    }
    //D_802A532C();
}
