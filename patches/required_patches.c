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

extern Gfx D_8029F718[];
extern Gfx D_8029F6D8[];
extern Gfx D_200000[];

extern u8 *D_802A5390;
extern u32 D_802A5388;

extern s32 D_80100000;
extern u8 D_8014C8D0[];
extern s32 D_8029F5E0;
extern s32 D_8029F5F8;
extern s32 D_802A5368;
extern Gfx* gMasterDisplayList; // D_802A5390


extern u8 D_8029F810[];
extern u8 D_8029F7F8[];

struct UnkStruct800BD5B0 D_800BD770;

struct UnkStruct800BD5B0 {
    Vp unk_00;
    u32 unk10;
    u32 unk14;
    Mtx unk_18;
    u32 unk58;
    f32 unk5C;
    f32 unk60;
    f32 unk64;
    f32 unk68;
};

struct UnkStruct802A538C {
    char pad[0x40];  
};

extern struct UnkStruct800BD5B0 D_800BD5B0[];
extern Mtx* D_802A538C;
extern Mtx D_802A53D8;

extern u16 D_802A53D0;

typedef struct {
  unsigned char	col[3];		/* diffuse light value (rgba) */
  char 		pad1;
  unsigned char	colc[3];	/* copy of diffuse light value (rgba) */
  char 		pad2;
  signed char	dir[3];		/* direction of light (normalized) */
  char 		pad3;
} NewLight_t;

typedef struct {
  unsigned char	col[3];		/* ambient light value (rgba) */
  char 		pad1;
  unsigned char	colc[3];	/* copy of ambient light value (rgba) */
  char 		pad2;
} NewAmbient_t;

typedef union {
    NewLight_t	l;
} NewLight;

typedef union {
    NewAmbient_t	l;
} NewAmbient;

typedef struct {
    NewAmbient	a;
    NewLight	l[2];
} NewLights2;

// hack, due to long width issues
extern NewLights2 D_8029F800 __attribute__((aligned(8)));

void *Libc_Memcpy(void *dest, void *source, s32 c);

extern void *func_80227678(u32);
void func_80297D60(void *, s32);

typedef unsigned int uintptr_t;

struct UnkStruct802AC5C0 {
    u16 unk0;
    u16 unk2;
    u16 unk4;
    u16 unk6;
    u16 unk8;
    u16 unkA;
    u16 unkC;
    u16 unkE;
    char pad10[0x4C];
    f32 unk5C;
    f32 unk60;
    f32 unk64;
    f32 unk68;
};

extern s32 D_802A53BC;
extern s32 D_802A53C0;
extern s32 D_802A53C4;
extern s32 D_802A53C8;

s32 HuPrcCreate(s32*, s32, s32, s32, s32);                  /* extern */
s32 func_802276FC(s32);                                 /* extern */
void *func_8022A6C4(void *, s32);                        /* extern */
struct UnkStruct802B0308_Inner *func_8026CE28(s32);                        /* extern */
extern s32 D_802A1B50;

typedef struct {
    int Unk00;     // Offset: 0x00
    int UnkFlags1;     // Offset: 0x04
    int FileID;     // Offset: 0x08
    int UnkFlags2;     // Offset: 0x0C
    int UnkFlags3;     // Offset: 0x10
    int UnkFlags4;     // Offset: 0x14
    float ScrollXSpeed;     // Offset: 0x18
    float ScrollYSpeed;     // Offset: 0x1C
    float ScrollX;     // Offset: 0x20
    float ScrollY;     // Offset: 0x24
    float OffsetX;     // Offset: 0x28
    float OffsetY;     // Offset: 0x2C
    float ResX;     // Offset: 0x30
    float ResY;     // Offset: 0x34
    float ShiftX;     // Offset: 0x38
    float ShiftY;     // Offset: 0x3C
    float ScaleX;     // Offset: 0x40
    float ScaleY;     // Offset: 0x44
    float ImageWidth;     // Offset: 0x48
    float ImageHeight;     // Offset: 0x4C
} SkyBox;

extern SkyBox D_802B0260;

struct UnkStruct802B0300_Inner {
    char pad[0x10];
    u16 unk10;
    u16 unk12;
};

struct UnkStruct802B0300 {
    struct UnkStruct802B0300_Inner *unk0;
};

struct UnkStruct802B0308_Inner {
    char pad[0xC];
    u32 unkC;
    f32 unk10;
    f32 unk14;
    f32 unk18;
    f32 unk1C;
    f32 unk20;
    f32 unk24;
};

struct UnkStruct802B0308 {
    struct UnkStruct802B0308_Inner *unk0;
};

extern struct UnkStruct802B0300 D_802B0300[];
extern struct UnkStruct802B0308 D_802B0308[];

extern void func_80287904();

RECOMP_EXPORT void recomp_enable_rects_extended(Gfx **gfx) {
    recomp_printf("(E) Master Display List: 0x%08X\n", gMasterDisplayList);
    gEXSetRectAspect((*gfx++), G_EX_ASPECT_STRETCH);
}

RECOMP_EXPORT void recomp_disable_rects_extended(Gfx **gfx) {
    recomp_printf("(D) Master Display List: 0x%08X\n", gMasterDisplayList);
    gEXSetRectAspect((*gfx++), G_EX_ASPECT_AUTO);
}

// init_skybox
RECOMP_PATCH SkyBox* func_80287DC0(s32 fileID) {
    SkyBox* skybox = &D_802B0260;
    s32 i;

    for(i = 0; i < 2; i++) {
        // find a non-used skybox to init in.
        if (skybox->UnkFlags1 == 0) {
            recomp_printf("Skybox File ID being processed: %d\n", fileID);
            skybox->Unk00 = i;
            skybox->FileID = fileID;
            skybox->UnkFlags1 = (s32) (skybox->UnkFlags1 | 0x80000000);
            skybox->UnkFlags2 = 0x400;
            skybox->UnkFlags3 = 0x40;
            D_802B0300[skybox->Unk00].unk0 = func_8026CE28(fileID);
            D_802B0308[skybox->Unk00].unk0 = func_8022A6C4(D_802B0300[skybox->Unk00].unk0, skybox->UnkFlags3);
            skybox->UnkFlags2 |= D_802B0308[skybox->Unk00].unk0->unkC;
            recomp_printf("Skybox Width: 0x%08X\n", D_802B0300[skybox->Unk00].unk0->unk10);
            skybox->ImageWidth = D_802B0300[skybox->Unk00].unk0->unk10;
            recomp_printf("Skybox Height: 0x%08X\n", D_802B0300[skybox->Unk00].unk0->unk12);
            skybox->ImageHeight = D_802B0300[skybox->Unk00].unk0->unk12;
            D_802B0308[skybox->Unk00].unk0->unk10 = skybox->OffsetX;
            D_802B0308[skybox->Unk00].unk0->unk14 = skybox->OffsetY;
            D_802B0308[skybox->Unk00].unk0->unk18 = skybox->ResX;
            D_802B0308[skybox->Unk00].unk0->unk1C = skybox->ResY;
            D_802B0308[skybox->Unk00].unk0->unk20 = 0;
            D_802B0308[skybox->Unk00].unk0->unk24 = 0;
            if (D_802A1B50 == -1) {
                D_802A1B50 = HuPrcCreate(&func_80287904, 0, 0, 0, 0x403);
                func_802276FC(0);
            }
            return skybox;
        }
        skybox++;
    }
    return NULL;
}

// 8, 6, 304, 228
RECOMP_PATCH void func_80227708(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    arg0 = 0;
    arg1 = 0;
    arg2 = 320;
    arg3 = 240;
    D_802A53BC = arg0;
    D_802A53C0 = arg1;
    D_802A53C4 = (arg0 + arg2);
    D_802A53C8 = (arg1 + arg3);
}

RECOMP_PATCH void func_80227D50(struct UnkStruct802AC5C0* arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4) {
    arg1 = 0;
    arg2 = 0;
    arg3 = 320;
    arg4 = 240;
    arg0->unk0 = (arg3 << 1);
    arg0->unk2 = (arg4 << 1);
    arg0->unk4 = 0x1FF;
    arg0->unk6 = 0;
    arg0->unk8 = (((arg1 << 1) + arg3) << 1);
    arg0->unkA = (((arg2 << 1) + arg4) << 1);
    arg0->unkC = 0x1FF;
    arg0->unkE = 0;
    arg0->unk5C = arg1;
    arg0->unk60 = arg2;
    arg0->unk64 = ((arg1 + arg3));
    arg0->unk68 = ((arg2 + arg4));
}

enum OverlayIDLoaded {
    UNSUPPORTED_OVERLAY = -1,
    // ovlbank 1
    LOADED_OVL_1_SUBOVL_1 = 1,
    LOADED_OVL_1_SUBOVL_2 = 2,
    LOADED_OVL_1_SUBOVL_3 = 3,
    LOADED_OVL_1_SUBOVL_4 = 4,
    // ovlbank 2
    LOADED_OVL_2_SUBOVL_1 = 5,
    LOADED_OVL_2_SUBOVL_2 = 6,
    LOADED_OVL_2_SUBOVL_3 = 7,
    LOADED_OVL_2_SUBOVL_4 = 8,
    LOADED_OVL_2_SUBOVL_5 = 9,
    LOADED_OVL_2_SUBOVL_6 = 10,
    // ovlbank 3
    LOADED_OVL_3_SUBOVL_1 = 11,
    LOADED_OVL_3_SUBOVL_2 = 12,
    LOADED_OVL_3_SUBOVL_3 = 13,
    LOADED_OVL_3_SUBOVL_4 = 14,
    LOADED_OVL_3_SUBOVL_5 = 15,
    LOADED_OVL_3_SUBOVL_6 = 16,
    LOADED_OVL_3_SUBOVL_7 = 17,
    // ovlbank 4
    LOADED_OVL_4_SUBOVL_1 = 18,
    LOADED_OVL_4_SUBOVL_2 = 19,
    LOADED_OVL_4_SUBOVL_3 = 20,
    LOADED_OVL_4_SUBOVL_4 = 21,
    LOADED_OVL_4_SUBOVL_5 = 22,
    LOADED_OVL_4_SUBOVL_6 = 23,
    LOADED_OVL_4_SUBOVL_7 = 24,
    LOADED_OVL_4_SUBOVL_8 = 25,
    LOADED_OVL_4_SUBOVL_9 = 26,
    LOADED_OVL_4_SUBOVL_10 = 27,
    // ovlbank 5
    LOADED_OVL_5_SUBOVL_1 = 28,
    LOADED_OVL_5_SUBOVL_2 = 29,
    LOADED_OVL_5_SUBOVL_3 = 30,
    LOADED_OVL_5_SUBOVL_4 = 31,
    LOADED_OVL_5_SUBOVL_5 = 32,
    LOADED_OVL_5_SUBOVL_6 = 33,
    LOADED_OVL_5_SUBOVL_7 = 34,
    // ovlbank 6
    LOADED_OVL_6_SUBOVL_1 = 35,
    LOADED_OVL_6_SUBOVL_2 = 36,
    LOADED_OVL_6_SUBOVL_3 = 37,
    LOADED_OVL_6_SUBOVL_4 = 38,
    LOADED_OVL_6_SUBOVL_5 = 39,
    LOADED_OVL_6_SUBOVL_6 = 40,
    LOADED_OVL_6_SUBOVL_7 = 41,
    LOADED_OVL_6_SUBOVL_8 = 42,
    LOADED_OVL_6_SUBOVL_9 = 43,
    LOADED_OVL_6_SUBOVL_10 = 44,
    LOADED_OVL_6_SUBOVL_11 = 45,
    LOADED_OVL_6_SUBOVL_12 = 46,
    LOADED_OVL_6_SUBOVL_13 = 47,
    LOADED_OVL_6_SUBOVL_14 = 48,
    LOADED_OVL_6_SUBOVL_15 = 49,
    // ovlbank 7
    LOADED_OVL_7_SUBOVL_1 = 50,
    LOADED_OVL_7_SUBOVL_2 = 51,
    LOADED_OVL_7_SUBOVL_3 = 52,
    LOADED_OVL_7_SUBOVL_4 = 53,
    LOADED_OVL_7_SUBOVL_5 = 54,
    LOADED_OVL_7_SUBOVL_6 = 55,
    LOADED_OVL_7_SUBOVL_7 = 56,
    LOADED_OVL_7_SUBOVL_8 = 57,
    LOADED_OVL_7_SUBOVL_9 = 58,
    LOADED_OVL_7_SUBOVL_10 = 59,
    // ovlbank 8
    LOADED_OVL_8_SUBOVL_1 = 60,
    LOADED_OVL_8_SUBOVL_2 = 61,
    LOADED_OVL_8_SUBOVL_3 = 62,
    LOADED_OVL_8_SUBOVL_4 = 63,
    LOADED_OVL_8_SUBOVL_5 = 64,
    LOADED_OVL_8_SUBOVL_6 = 65,
    LOADED_OVL_8_SUBOVL_7 = 66,
    LOADED_OVL_8_SUBOVL_8 = 67,
    LOADED_OVL_8_SUBOVL_9 = 68,
    LOADED_OVL_8_SUBOVL_10 = 69,
    LOADED_OVL_8_SUBOVL_11 = 70,
    LOADED_OVL_8_SUBOVL_12 = 71,
    LOADED_OVL_8_SUBOVL_13 = 72,
    // ovlbank 9
    LOADED_OVL_9_SUBOVL_1 = 73,
    LOADED_OVL_9_SUBOVL_2 = 74,
    LOADED_OVL_9_SUBOVL_3 = 75,
    LOADED_OVL_9_SUBOVL_4 = 76,
    LOADED_OVL_9_SUBOVL_5 = 77,
    LOADED_OVL_9_SUBOVL_6 = 78,
    // ovlbank 10
    LOADED_OVL_10_SUBOVL_1 = 79,
    LOADED_OVL_10_SUBOVL_2 = 80,
    // ovlbank 11
    LOADED_OVL_11_SUBOVL_1 = 81,
    LOADED_OVL_11_SUBOVL_2 = 82,
    // ovlbank 12
    LOADED_OVL_12_SUBOVL_1 = 83,
    LOADED_OVL_12_SUBOVL_2 = 84,
    // ovlbank 13
    LOADED_OVL_13_SUBOVL_1 = 85,
    // ovlbank 14
    LOADED_OVL_14_SUBOVL_1 = 86,
    LOADED_OVL_14_SUBOVL_2 = 87,
    LOADED_OVL_14_SUBOVL_3 = 88,
    LOADED_OVL_14_SUBOVL_4 = 89,
    LOADED_OVL_14_SUBOVL_5 = 90,
    LOADED_OVL_14_SUBOVL_6 = 91,
    LOADED_OVL_14_SUBOVL_7 = 92,
    LOADED_OVL_14_SUBOVL_8 = 93,
    LOADED_OVL_14_SUBOVL_9 = 94,
    LOADED_OVL_14_SUBOVL_10 = 95,
};

static enum OverlayIDLoaded gOverlayID = UNSUPPORTED_OVERLAY;

s32 func_8022979C(void*, s32, s32);                       /* extern */
s32 func_8022984C(void*, s32);                          /* extern */
s32 func_80229F74(void*, s32);                          /* extern */
void func_80231408(void*, s32);                          /* extern */
s32 func_802444B8(s32, s32);                            /* extern */
extern f32 D_802A3164;
extern s32 D_802AC89C;
extern s32 D_802AC8A0;

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

    // are we loading one of the overlays? Set the ID.
    switch(devAddr) {
        case 0x00122016: recomp_load_overlays(0x00800010, (void*) 0x80043000, 0x000019E0); gOverlayID = LOADED_OVL_1_SUBOVL_1; break;
        case 0x00122B34: recomp_load_overlays(0x008019F0, (void*) 0x80043000, 0x00001AD0); gOverlayID = LOADED_OVL_1_SUBOVL_2; break;
        case 0x001236E2: recomp_load_overlays(0x008034C0, (void*) 0x80043000, 0x00001A50); gOverlayID = LOADED_OVL_1_SUBOVL_3; break;
        case 0x00124264: recomp_load_overlays(0x00804F10, (void*) 0x80043000, 0x00001A40); gOverlayID = LOADED_OVL_1_SUBOVL_4; break;
        case 0x0014201C: recomp_load_overlays(0x00806970, (void*) 0x80043000, 0x00005CE0); gOverlayID = LOADED_OVL_2_SUBOVL_1; break;
        case 0x00145A94: recomp_load_overlays(0x0080C650, (void*) 0x80043000, 0x00005500); gOverlayID = LOADED_OVL_2_SUBOVL_2; break;
        case 0x00148D98: recomp_load_overlays(0x00811B50, (void*) 0x80043000, 0x00005800); gOverlayID = LOADED_OVL_2_SUBOVL_3; break;
        case 0x0014C38E: recomp_load_overlays(0x00817350, (void*) 0x80043000, 0x000036C0); gOverlayID = LOADED_OVL_2_SUBOVL_4; break;
        case 0x0014E26A: recomp_load_overlays(0x0081AA10, (void*) 0x80043000, 0x000068D0); gOverlayID = LOADED_OVL_2_SUBOVL_5; break;
        case 0x001521AC: recomp_load_overlays(0x008212E0, (void*) 0x80043000, 0x000044F0); gOverlayID = LOADED_OVL_2_SUBOVL_6; break;
        case 0x0016201E: recomp_load_overlays(0x008257F0, (void*) 0x80043000, 0x00002F80); gOverlayID = LOADED_OVL_3_SUBOVL_1; break;
        case 0x00163B62: recomp_load_overlays(0x00828770, (void*) 0x80043000, 0x00007380); gOverlayID = LOADED_OVL_3_SUBOVL_2; break;
        case 0x0016808C: recomp_load_overlays(0x0082FAF0, (void*) 0x80043000, 0x000062E0); gOverlayID = LOADED_OVL_3_SUBOVL_3; break;
        case 0x0016B954: recomp_load_overlays(0x00835DD0, (void*) 0x80043000, 0x00007290); gOverlayID = LOADED_OVL_3_SUBOVL_4; break;
        case 0x0016F620: recomp_load_overlays(0x0083D060, (void*) 0x80043000, 0x000037C0); gOverlayID = LOADED_OVL_3_SUBOVL_5; break;
        case 0x0017157E: recomp_load_overlays(0x00840820, (void*) 0x80043000, 0x00007590); gOverlayID = LOADED_OVL_3_SUBOVL_6; break;
        case 0x00175AE4: recomp_load_overlays(0x00847DB0, (void*) 0x80043000, 0x00003DF0); gOverlayID = LOADED_OVL_3_SUBOVL_7; break;
        case 0x00182028: recomp_load_overlays(0x0084BBC0, (void*) 0x80043000, 0x00007980); gOverlayID = LOADED_OVL_4_SUBOVL_1; break;
        case 0x001866D6: recomp_load_overlays(0x00853540, (void*) 0x80043000, 0x000070D0); gOverlayID = LOADED_OVL_4_SUBOVL_2; break;
        case 0x0018A962: recomp_load_overlays(0x0085A610, (void*) 0x80043000, 0x00002670); gOverlayID = LOADED_OVL_4_SUBOVL_3; break;
        case 0x0018BBB2: recomp_load_overlays(0x0085CC80, (void*) 0x80043000, 0x00002DD0); gOverlayID = LOADED_OVL_4_SUBOVL_4; break;
        case 0x0018D4BC: recomp_load_overlays(0x0085FA50, (void*) 0x80043000, 0x00002A60); gOverlayID = LOADED_OVL_4_SUBOVL_5; break;
        case 0x0018E9AC: recomp_load_overlays(0x008624B0, (void*) 0x80043000, 0x00006370); gOverlayID = LOADED_OVL_4_SUBOVL_6; break;
        case 0x0019247A: recomp_load_overlays(0x00868820, (void*) 0x80043000, 0x00004950); gOverlayID = LOADED_OVL_4_SUBOVL_7; break;
        case 0x00194D3C: recomp_load_overlays(0x0086D170, (void*) 0x80043000, 0x000025A0); gOverlayID = LOADED_OVL_4_SUBOVL_8; break;
        case 0x00196044: recomp_load_overlays(0x0086F710, (void*) 0x80043000, 0x00004AE0); gOverlayID = LOADED_OVL_4_SUBOVL_9; break;
        case 0x00198A78: recomp_load_overlays(0x008741F0, (void*) 0x80043000, 0x00003670); gOverlayID = LOADED_OVL_4_SUBOVL_10; break;
        case 0x001A201E: recomp_load_overlays(0x00877880, (void*) 0x80043000, 0x00004AD0); gOverlayID = LOADED_OVL_5_SUBOVL_1; break;
        case 0x001A48B4: recomp_load_overlays(0x0087C350, (void*) 0x80043000, 0x00004A00); gOverlayID = LOADED_OVL_5_SUBOVL_2; break;
        case 0x001A75E2: recomp_load_overlays(0x00880D50, (void*) 0x80043000, 0x00005140); gOverlayID = LOADED_OVL_5_SUBOVL_3; break;
        case 0x001AA720: recomp_load_overlays(0x00885E90, (void*) 0x80043000, 0x00004020); gOverlayID = LOADED_OVL_5_SUBOVL_4; break;
        case 0x001AC9C6: recomp_load_overlays(0x00889EB0, (void*) 0x80043000, 0x00003130); gOverlayID = LOADED_OVL_5_SUBOVL_5; break;
        case 0x001AE5AA: recomp_load_overlays(0x0088CFE0, (void*) 0x80043000, 0x000045B0); gOverlayID = LOADED_OVL_5_SUBOVL_6; break;
        case 0x001B0CD4: recomp_load_overlays(0x00891590, (void*) 0x80043000, 0x000036E0); gOverlayID = LOADED_OVL_5_SUBOVL_7; break;
        case 0x001C2036: recomp_load_overlays(0x00894CA0, (void*) 0x80043000, 0x00003CE0); gOverlayID = LOADED_OVL_6_SUBOVL_1; break;
        case 0x001C4300: recomp_load_overlays(0x00898980, (void*) 0x80043000, 0x00003760); gOverlayID = LOADED_OVL_6_SUBOVL_2; break;
        case 0x001C61CA: recomp_load_overlays(0x0089C0E0, (void*) 0x80043000, 0x00003C10); gOverlayID = LOADED_OVL_6_SUBOVL_3; break;
        case 0x001C83A8: recomp_load_overlays(0x0089FCF0, (void*) 0x80043000, 0x00003F00); gOverlayID = LOADED_OVL_6_SUBOVL_4; break;
        case 0x001CA71A: recomp_load_overlays(0x008A3BF0, (void*) 0x80043000, 0x00003210); gOverlayID = LOADED_OVL_6_SUBOVL_5; break;
        case 0x001CC23C: recomp_load_overlays(0x008A6E00, (void*) 0x80043000, 0x00002360); gOverlayID = LOADED_OVL_6_SUBOVL_6; break;
        case 0x001CD2FC: recomp_load_overlays(0x008A9160, (void*) 0x80043000, 0x00002360); gOverlayID = LOADED_OVL_6_SUBOVL_7; break;
        case 0x001CE3BA: recomp_load_overlays(0x008AB4C0, (void*) 0x80043000, 0x00002360); gOverlayID = LOADED_OVL_6_SUBOVL_8; break;
        case 0x001CF478: recomp_load_overlays(0x008AD820, (void*) 0x80043000, 0x00002F90); gOverlayID = LOADED_OVL_6_SUBOVL_9; break;
        case 0x001D0D7A: recomp_load_overlays(0x008B07B0, (void*) 0x80043000, 0x00002560); gOverlayID = LOADED_OVL_6_SUBOVL_10; break;
        case 0x001D1F36: recomp_load_overlays(0x008B2D10, (void*) 0x80043000, 0x000034C0); gOverlayID = LOADED_OVL_6_SUBOVL_11; break;
        case 0x001D3AF4: recomp_load_overlays(0x008B61D0, (void*) 0x80043000, 0x000032B0); gOverlayID = LOADED_OVL_6_SUBOVL_12; break;
        case 0x001D5620: recomp_load_overlays(0x008B9480, (void*) 0x80043000, 0x00003D50); gOverlayID = LOADED_OVL_6_SUBOVL_13; break;
        case 0x001D7808: recomp_load_overlays(0x008BD1D0, (void*) 0x80043000, 0x00002360); gOverlayID = LOADED_OVL_6_SUBOVL_14; break;
        case 0x001D88C6: recomp_load_overlays(0x008BF530, (void*) 0x80043000, 0x00003900); gOverlayID = LOADED_OVL_6_SUBOVL_15; break;
        case 0x001E2028: recomp_load_overlays(0x008C2E50, (void*) 0x80043000, 0x00005720); gOverlayID = LOADED_OVL_7_SUBOVL_1; break;
        case 0x001E5596: recomp_load_overlays(0x008C8570, (void*) 0x80043000, 0x00005D00); gOverlayID = LOADED_OVL_7_SUBOVL_2; break;
        case 0x001E8DE4: recomp_load_overlays(0x008CE270, (void*) 0x80043000, 0x00004090); gOverlayID = LOADED_OVL_7_SUBOVL_3; break;
        case 0x001EB36E: recomp_load_overlays(0x008D2300, (void*) 0x80043000, 0x00005A70); gOverlayID = LOADED_OVL_7_SUBOVL_4; break;
        case 0x001EEA8A: recomp_load_overlays(0x008D7D70, (void*) 0x80043000, 0x000034E0); gOverlayID = LOADED_OVL_7_SUBOVL_5; break;
        case 0x001F0882: recomp_load_overlays(0x008DB250, (void*) 0x80043000, 0x00001940); gOverlayID = LOADED_OVL_7_SUBOVL_6; break;
        case 0x001F136E: recomp_load_overlays(0x008DCB90, (void*) 0x80043000, 0x00002E30); gOverlayID = LOADED_OVL_7_SUBOVL_7; break;
        case 0x001F2B8A: recomp_load_overlays(0x008DF9C0, (void*) 0x80043000, 0x00002DA0); gOverlayID = LOADED_OVL_7_SUBOVL_8; break;
        case 0x001F42F8: recomp_load_overlays(0x008E2760, (void*) 0x80043000, 0x00003200); gOverlayID = LOADED_OVL_7_SUBOVL_9; break;
        case 0x001F5DBE: recomp_load_overlays(0x008E5960, (void*) 0x80043000, 0x00003AE0); gOverlayID = LOADED_OVL_7_SUBOVL_10; break;
        case 0x00202030: recomp_load_overlays(0x008E9470, (void*) 0x80043000, 0x000042F0); gOverlayID = LOADED_OVL_8_SUBOVL_1; break;
        case 0x002049BC: recomp_load_overlays(0x008ED760, (void*) 0x80043000, 0x00002D30); gOverlayID = LOADED_OVL_8_SUBOVL_2; break;
        case 0x00206284: recomp_load_overlays(0x008F0490, (void*) 0x80043000, 0x00003FB0); gOverlayID = LOADED_OVL_8_SUBOVL_3; break;
        case 0x00208A5C: recomp_load_overlays(0x008F4440, (void*) 0x80043000, 0x00002D10); gOverlayID = LOADED_OVL_8_SUBOVL_4; break;
        case 0x0020A324: recomp_load_overlays(0x008F7150, (void*) 0x80043000, 0x00003EE0); gOverlayID = LOADED_OVL_8_SUBOVL_5; break;
        case 0x0020CAC8: recomp_load_overlays(0x008FB030, (void*) 0x80043000, 0x00007B60); gOverlayID = LOADED_OVL_8_SUBOVL_6; break;
        case 0x002118BC: recomp_load_overlays(0x00902B90, (void*) 0x80043000, 0x00006B80); gOverlayID = LOADED_OVL_8_SUBOVL_7; break;
        case 0x0021617A: recomp_load_overlays(0x00909710, (void*) 0x80043000, 0x000066C0); gOverlayID = LOADED_OVL_8_SUBOVL_8; break;
        case 0x0021A53E: recomp_load_overlays(0x0090FDD0, (void*) 0x80043000, 0x000069B0); gOverlayID = LOADED_OVL_8_SUBOVL_9; break;
        case 0x0021EA7C: recomp_load_overlays(0x00916780, (void*) 0x80043000, 0x000040A0); gOverlayID = LOADED_OVL_8_SUBOVL_10; break;
        case 0x00221272: recomp_load_overlays(0x0091A820, (void*) 0x80043000, 0x000036A0); gOverlayID = LOADED_OVL_8_SUBOVL_11; break;
        case 0x00223304: recomp_load_overlays(0x0091DEC0, (void*) 0x80043000, 0x00005C30); gOverlayID = LOADED_OVL_8_SUBOVL_12; break;
        case 0x00226FE4: recomp_load_overlays(0x00923AF0, (void*) 0x80043000, 0x00007B50); gOverlayID = LOADED_OVL_8_SUBOVL_13; break;
        case 0x0024201C: recomp_load_overlays(0x0092B660, (void*) 0x80043000, 0x000034C0); gOverlayID = LOADED_OVL_9_SUBOVL_1; break;
        case 0x00243BAC: recomp_load_overlays(0x0092EB20, (void*) 0x80043000, 0x00002170); gOverlayID = LOADED_OVL_9_SUBOVL_2; break;
        case 0x00244B56: recomp_load_overlays(0x00930C90, (void*) 0x80043000, 0x00002EA0); gOverlayID = LOADED_OVL_9_SUBOVL_3; break;
        case 0x00246372: recomp_load_overlays(0x00933B30, (void*) 0x80043000, 0x00002070); gOverlayID = LOADED_OVL_9_SUBOVL_4; break;
        case 0x0024726C: recomp_load_overlays(0x00935BA0, (void*) 0x80043000, 0x00007BF0); gOverlayID = LOADED_OVL_9_SUBOVL_5; break;
        case 0x0024BD9E: recomp_load_overlays(0x0093D790, (void*) 0x80043000, 0x00002F80); gOverlayID = LOADED_OVL_9_SUBOVL_6; break;
        case 0x00262010: recomp_load_overlays(0x00940720, (void*) 0x80043000, 0x00009200); gOverlayID = LOADED_OVL_10_SUBOVL_1; break;
        case 0x00267B30: recomp_load_overlays(0x00949920, (void*) 0x80043000, 0x0000AC90); gOverlayID = LOADED_OVL_10_SUBOVL_2; break;
        case 0x00282010: recomp_load_overlays(0x009545C0, (void*) 0x80043000, 0x0000A4A0); gOverlayID = LOADED_OVL_11_SUBOVL_1; break;
        case 0x00288D88: recomp_load_overlays(0x0095EA60, (void*) 0x80043000, 0x0000D270); gOverlayID = LOADED_OVL_11_SUBOVL_2; break;
        case 0x002A2010: recomp_load_overlays(0x0096BCE0, (void*) 0x80043000, 0x0000AF30); gOverlayID = LOADED_OVL_12_SUBOVL_1; break;
        case 0x002A94A6: recomp_load_overlays(0x00976C10, (void*) 0x80043000, 0x0000E320); gOverlayID = LOADED_OVL_12_SUBOVL_2; break;
        case 0x002C200C: recomp_load_overlays(0x00984F40, (void*) 0x80043000, 0x000022A0); gOverlayID = LOADED_OVL_13_SUBOVL_1; break;
        case 0x002E2028: recomp_load_overlays(0x00987200, (void*) 0x80043000, 0x00002800); gOverlayID = LOADED_OVL_14_SUBOVL_1; break;
        case 0x002E35C0: recomp_load_overlays(0x00989A00, (void*) 0x80043000, 0x00001DC0); gOverlayID = LOADED_OVL_14_SUBOVL_2; break;
        case 0x002E4438: recomp_load_overlays(0x0098B7C0, (void*) 0x80043000, 0x00001E40); gOverlayID = LOADED_OVL_14_SUBOVL_3; break;
        case 0x002E52E2: recomp_load_overlays(0x0098D600, (void*) 0x80043000, 0x00001E30); gOverlayID = LOADED_OVL_14_SUBOVL_4; break;
        case 0x002E6188: recomp_load_overlays(0x0098F430, (void*) 0x80043000, 0x00001E50); gOverlayID = LOADED_OVL_14_SUBOVL_5; break;
        case 0x002E7044: recomp_load_overlays(0x00991280, (void*) 0x80043000, 0x00002840); gOverlayID = LOADED_OVL_14_SUBOVL_6; break;
        case 0x002E85FE: recomp_load_overlays(0x00993AC0, (void*) 0x80043000, 0x00002780); gOverlayID = LOADED_OVL_14_SUBOVL_7; break;
        case 0x002E9B68: recomp_load_overlays(0x00996240, (void*) 0x80043000, 0x00002C60); gOverlayID = LOADED_OVL_14_SUBOVL_8; break;
        case 0x002EB3EA: recomp_load_overlays(0x00998EA0, (void*) 0x80043000, 0x00003390); gOverlayID = LOADED_OVL_14_SUBOVL_9; break;
        case 0x002ED178: recomp_load_overlays(0x0099C230, (void*) 0x80043000, 0x000036F0); gOverlayID = LOADED_OVL_14_SUBOVL_10; break;
        default:
            recomp_load_overlays(devAddr, (void*) vAddr, size);
            break;
    }

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
void func_802266E8(s32);
s32 func_80226604(s32, s32);
s32 func_802266DC(s32);
void func_8022616C(void*, s32, s32, s32);


u32 gGlobalOverlayROM_Base   = 0x00000000; // the currently set ROM offset of the OVL being loaded.
u32 gGlobalOverlayROM_Offset = 0x00000000; // the offset of the overlay

extern u32 D_802AC5D4;
extern u32 D_802AC5DC;

// -------------------------
extern int func_ovl3_1_80043000(void); // ovl_3_subovl_1 (Copyright Screen)
extern int func_ovl2_5_80043000(void); // ovl_2_subovl_5 (Intro) ?

/*
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

/*
    switch(gOverlayID) {
        case LOADED_OVL_1_SUBOVL_1:   recomp_load_overlays(0x800010, (void*)0x80043000, 0x000019E0); break;
        case LOADED_OVL_1_SUBOVL_2:   recomp_load_overlays(0x8019F0, (void*)0x80043000, 0x00001AD0); break;
        case LOADED_OVL_1_SUBOVL_3:   recomp_load_overlays(0x8034C0, (void*)0x80043000, 0x00001A50); break;
        case LOADED_OVL_1_SUBOVL_4:   recomp_load_overlays(0x804F10, (void*)0x80043000, 0x00001A40); break;
        case LOADED_OVL_2_SUBOVL_1:   recomp_load_overlays(0x806970, (void*)0x80043000, 0x00005CE0); break;
        case LOADED_OVL_2_SUBOVL_2:   recomp_load_overlays(0x80C650, (void*)0x80043000, 0x00005500); break;
        case LOADED_OVL_2_SUBOVL_3:   recomp_load_overlays(0x811B50, (void*)0x80043000, 0x00005800); break;
        case LOADED_OVL_2_SUBOVL_4:   recomp_load_overlays(0x817350, (void*)0x80043000, 0x000036C0); break;
        case LOADED_OVL_2_SUBOVL_5:   recomp_load_overlays(0x81AA10, (void*)0x80043000, 0x000068D0); break;
        case LOADED_OVL_2_SUBOVL_6:   recomp_load_overlays(0x8212E0, (void*)0x80043000, 0x000044F0); break;
        case LOADED_OVL_3_SUBOVL_1:   recomp_load_overlays(0x8257F0, (void*)0x80043000, 0x00002F80); break;
        case LOADED_OVL_3_SUBOVL_2:   recomp_load_overlays(0x828770, (void*)0x80043000, 0x00007380); break;
        case LOADED_OVL_3_SUBOVL_3:   recomp_load_overlays(0x82FAF0, (void*)0x80043000, 0x000062E0); break;
        case LOADED_OVL_3_SUBOVL_4:   recomp_load_overlays(0x835DD0, (void*)0x80043000, 0x00007290); break;
        case LOADED_OVL_3_SUBOVL_5:   recomp_load_overlays(0x83D060, (void*)0x80043000, 0x000037C0); break;
        case LOADED_OVL_3_SUBOVL_6:   recomp_load_overlays(0x840820, (void*)0x80043000, 0x00007590); break;
        case LOADED_OVL_3_SUBOVL_7:   recomp_load_overlays(0x847DB0, (void*)0x80043000, 0x00003DF0); break;
        case LOADED_OVL_4_SUBOVL_1:   recomp_load_overlays(0x84BBC0, (void*)0x80043000, 0x00007980); break;
        case LOADED_OVL_4_SUBOVL_2:   recomp_load_overlays(0x853540, (void*)0x80043000, 0x000070D0); break;
        case LOADED_OVL_4_SUBOVL_3:   recomp_load_overlays(0x85A610, (void*)0x80043000, 0x00002670); break;
        case LOADED_OVL_4_SUBOVL_4:   recomp_load_overlays(0x85CC80, (void*)0x80043000, 0x00002DD0); break;
        case LOADED_OVL_4_SUBOVL_5:   recomp_load_overlays(0x85FA50, (void*)0x80043000, 0x00002A60); break;
        case LOADED_OVL_4_SUBOVL_6:   recomp_load_overlays(0x8624B0, (void*)0x80043000, 0x00006370); break;
        case LOADED_OVL_4_SUBOVL_7:   recomp_load_overlays(0x868820, (void*)0x80043000, 0x00004950); break;
        case LOADED_OVL_4_SUBOVL_8:   recomp_load_overlays(0x86D170, (void*)0x80043000, 0x000025A0); break;
        case LOADED_OVL_4_SUBOVL_9:   recomp_load_overlays(0x86F710, (void*)0x80043000, 0x00004AE0); break;
        case LOADED_OVL_4_SUBOVL_10:  recomp_load_overlays(0x8741F0, (void*)0x80043000, 0x00003670); break;
        case LOADED_OVL_5_SUBOVL_1:   recomp_load_overlays(0x877880, (void*)0x80043000, 0x00004AD0); break;
        case LOADED_OVL_5_SUBOVL_2:   recomp_load_overlays(0x87C350, (void*)0x80043000, 0x00004A00); break;
        case LOADED_OVL_5_SUBOVL_3:   recomp_load_overlays(0x880D50, (void*)0x80043000, 0x00005140); break;
        case LOADED_OVL_5_SUBOVL_4:   recomp_load_overlays(0x885E90, (void*)0x80043000, 0x00004020); break;
        case LOADED_OVL_5_SUBOVL_5:   recomp_load_overlays(0x889EB0, (void*)0x80043000, 0x00003130); break;
        case LOADED_OVL_5_SUBOVL_6:   recomp_load_overlays(0x88CFE0, (void*)0x80043000, 0x000045B0); break;
        case LOADED_OVL_5_SUBOVL_7:   recomp_load_overlays(0x891590, (void*)0x80043000, 0x000036E0); break;
        case LOADED_OVL_6_SUBOVL_1:   recomp_load_overlays(0x894CA0, (void*)0x80043000, 0x00003CE0); break;
        case LOADED_OVL_6_SUBOVL_2:   recomp_load_overlays(0x898980, (void*)0x80043000, 0x00003760); break;
        case LOADED_OVL_6_SUBOVL_3:   recomp_load_overlays(0x89C0E0, (void*)0x80043000, 0x00003C10); break;
        case LOADED_OVL_6_SUBOVL_4:   recomp_load_overlays(0x89FCF0, (void*)0x80043000, 0x00003F00); break;
        case LOADED_OVL_6_SUBOVL_5:   recomp_load_overlays(0x8A3BF0, (void*)0x80043000, 0x00003210); break;
        case LOADED_OVL_6_SUBOVL_6:   recomp_load_overlays(0x8A6E00, (void*)0x80043000, 0x00002360); break;
        case LOADED_OVL_6_SUBOVL_7:   recomp_load_overlays(0x8A9160, (void*)0x80043000, 0x00002360); break;
        case LOADED_OVL_6_SUBOVL_8:   recomp_load_overlays(0x8AB4C0, (void*)0x80043000, 0x00002360); break;
        case LOADED_OVL_6_SUBOVL_9:   recomp_load_overlays(0x8AD820, (void*)0x80043000, 0x00002F90); break;
        case LOADED_OVL_6_SUBOVL_10:  recomp_load_overlays(0x8B07B0, (void*)0x80043000, 0x00002560); break;
        case LOADED_OVL_6_SUBOVL_11:  recomp_load_overlays(0x8B2D10, (void*)0x80043000, 0x000034C0); break;
        case LOADED_OVL_6_SUBOVL_12:  recomp_load_overlays(0x8B61D0, (void*)0x80043000, 0x000032B0); break;
        case LOADED_OVL_6_SUBOVL_13:  recomp_load_overlays(0x8B9480, (void*)0x80043000, 0x00003D50); break;
        case LOADED_OVL_6_SUBOVL_14:  recomp_load_overlays(0x8BD1D0, (void*)0x80043000, 0x00002360); break;
        case LOADED_OVL_6_SUBOVL_15:  recomp_load_overlays(0x8BF530, (void*)0x80043000, 0x00003900); break;
        case LOADED_OVL_7_SUBOVL_1:   recomp_load_overlays(0x8C2E50, (void*)0x80043000, 0x00005720); break;
        case LOADED_OVL_7_SUBOVL_2:   recomp_load_overlays(0x8C8570, (void*)0x80043000, 0x00005D00); break;
        case LOADED_OVL_7_SUBOVL_3:   recomp_load_overlays(0x8CE270, (void*)0x80043000, 0x00004090); break;
        case LOADED_OVL_7_SUBOVL_4:   recomp_load_overlays(0x8D2300, (void*)0x80043000, 0x00005A70); break;
        case LOADED_OVL_7_SUBOVL_5:   recomp_load_overlays(0x8D7D70, (void*)0x80043000, 0x000034E0); break;
        case LOADED_OVL_7_SUBOVL_6:   recomp_load_overlays(0x8DB250, (void*)0x80043000, 0x00001940); break;
        case LOADED_OVL_7_SUBOVL_7:   recomp_load_overlays(0x8DCB90, (void*)0x80043000, 0x00002E30); break;
        case LOADED_OVL_7_SUBOVL_8:   recomp_load_overlays(0x8DF9C0, (void*)0x80043000, 0x00002DA0); break;
        case LOADED_OVL_7_SUBOVL_9:   recomp_load_overlays(0x8E2760, (void*)0x80043000, 0x00003200); break;
        case LOADED_OVL_7_SUBOVL_10:  recomp_load_overlays(0x8E5960, (void*)0x80043000, 0x00003AE0); break;
        case LOADED_OVL_8_SUBOVL_1:   recomp_load_overlays(0x8E9470, (void*)0x80043000, 0x000042F0); break;
        case LOADED_OVL_8_SUBOVL_2:   recomp_load_overlays(0x8ED760, (void*)0x80043000, 0x00002D30); break;
        case LOADED_OVL_8_SUBOVL_3:   recomp_load_overlays(0x8F0490, (void*)0x80043000, 0x00003FB0); break;
        case LOADED_OVL_8_SUBOVL_4:   recomp_load_overlays(0x8F4440, (void*)0x80043000, 0x00002D10); break;
        case LOADED_OVL_8_SUBOVL_5:   recomp_load_overlays(0x8F7150, (void*)0x80043000, 0x00003EE0); break;
        case LOADED_OVL_8_SUBOVL_6:   recomp_load_overlays(0x8FB030, (void*)0x80043000, 0x00007B60); break;
        case LOADED_OVL_8_SUBOVL_7:   recomp_load_overlays(0x902B90, (void*)0x80043000, 0x00006B80); break;
        case LOADED_OVL_8_SUBOVL_8:   recomp_load_overlays(0x909710, (void*)0x80043000, 0x000066C0); break;
        case LOADED_OVL_8_SUBOVL_9:   recomp_load_overlays(0x90FDD0, (void*)0x80043000, 0x000069B0); break;
        case LOADED_OVL_8_SUBOVL_10:  recomp_load_overlays(0x916780, (void*)0x80043000, 0x000040A0); break;
        case LOADED_OVL_8_SUBOVL_11:  recomp_load_overlays(0x91A820, (void*)0x80043000, 0x000036A0); break;
        case LOADED_OVL_8_SUBOVL_12:  recomp_load_overlays(0x91DEC0, (void*)0x80043000, 0x00005C30); break;
        case LOADED_OVL_8_SUBOVL_13:  recomp_load_overlays(0x923AF0, (void*)0x80043000, 0x00007B50); break;
        case LOADED_OVL_9_SUBOVL_1:   recomp_load_overlays(0x92B660, (void*)0x80043000, 0x000034C0); break;
        case LOADED_OVL_9_SUBOVL_2:   recomp_load_overlays(0x92EB20, (void*)0x80043000, 0x00002170); break;
        case LOADED_OVL_9_SUBOVL_3:   recomp_load_overlays(0x930C90, (void*)0x80043000, 0x00002EA0); break;
        case LOADED_OVL_9_SUBOVL_4:   recomp_load_overlays(0x933B30, (void*)0x80043000, 0x00002070); break;
        case LOADED_OVL_9_SUBOVL_5:   recomp_load_overlays(0x935BA0, (void*)0x80043000, 0x00007BF0); break;
        case LOADED_OVL_9_SUBOVL_6:   recomp_load_overlays(0x93D790, (void*)0x80043000, 0x00002F80); break;
        case LOADED_OVL_10_SUBOVL_1:  recomp_load_overlays(0x940720, (void*)0x80043000, 0x00009200); break;
        case LOADED_OVL_10_SUBOVL_2:  recomp_load_overlays(0x949920, (void*)0x80043000, 0x0000AC90); break;
        case LOADED_OVL_11_SUBOVL_1:  recomp_load_overlays(0x9545C0, (void*)0x80043000, 0x0000A4A0); break;
        case LOADED_OVL_11_SUBOVL_2:  recomp_load_overlays(0x95EA60, (void*)0x80043000, 0x0000D270); break;
        case LOADED_OVL_12_SUBOVL_1:  recomp_load_overlays(0x96BCE0, (void*)0x80043000, 0x0000AF30); break;
        case LOADED_OVL_12_SUBOVL_2:  recomp_load_overlays(0x976C10, (void*)0x80043000, 0x0000E320); break;
        case LOADED_OVL_13_SUBOVL_1:  recomp_load_overlays(0x984F40, (void*)0x80043000, 0x000022A0); break;
        case LOADED_OVL_14_SUBOVL_1:  recomp_load_overlays(0x987200, (void*)0x80043000, 0x00002800); break;
        case LOADED_OVL_14_SUBOVL_2:  recomp_load_overlays(0x989A00, (void*)0x80043000, 0x00001DC0); break;
        case LOADED_OVL_14_SUBOVL_3:  recomp_load_overlays(0x98B7C0, (void*)0x80043000, 0x00001E40); break;
        case LOADED_OVL_14_SUBOVL_4:  recomp_load_overlays(0x98D600, (void*)0x80043000, 0x00001E30); break;
        case LOADED_OVL_14_SUBOVL_5:  recomp_load_overlays(0x98F430, (void*)0x80043000, 0x00001E50); break;
        case LOADED_OVL_14_SUBOVL_6:  recomp_load_overlays(0x991280, (void*)0x80043000, 0x00002840); break;
        case LOADED_OVL_14_SUBOVL_7:  recomp_load_overlays(0x993AC0, (void*)0x80043000, 0x00002780); break;
        case LOADED_OVL_14_SUBOVL_8:  recomp_load_overlays(0x996240, (void*)0x80043000, 0x00002C60); break;
        case LOADED_OVL_14_SUBOVL_9:  recomp_load_overlays(0x998EA0, (void*)0x80043000, 0x00003390); break;
        case LOADED_OVL_14_SUBOVL_10: recomp_load_overlays(0x99C230, (void*)0x80043000, 0x000036F0); break;
        case UNSUPPORTED_OVERLAY:
        default: // unsupported overlay.
            while(1)
                ;
    }

}
*/

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
    func_80297D60((void*)(((u32) (sp44 + 0xF) >> 4) * 0x10), ((u32) (arg2 + 0xF) >> 4) * 0x10);
    func_8029ADCC(D_802B0310);
    D_802A1D90 = 0;
}

extern int (*D_802A532C)();

extern void func_ovl1_1_80043010(void); 
extern void func_ovl1_2_800430A0(void); 
extern void func_ovl1_3_80043010(void); 
extern void func_ovl1_4_80043010(void); 
extern void func_ovl2_1_80046C88(void); 
extern void func_ovl2_2_8004664C(void); 
extern void func_ovl2_3_80046A24(void); 
extern void func_ovl2_4_800445F8(void); 
extern void func_ovl2_5_80047AF4(void); 
extern void func_ovl2_6_80044F68(void); 
extern void func_ovl3_1_80044108(void); 
extern void func_ovl3_2_800478EC(void); 
extern void func_ovl3_3_80047198(void); 
extern void func_ovl3_4_80043110(void); 
extern void func_ovl3_5_80043E04(void); 
extern void func_ovl3_6_80043C48(void); 
extern void func_ovl3_7_80043010(void); 
extern void func_ovl4_1_80047074(void); 
extern void func_ovl4_2_8004639C(void); 
extern void func_ovl4_3_8004391C(void); 
extern void func_ovl4_4_80043ACC(void); 
extern void func_ovl4_5_80043970(void); 
extern void func_ovl4_6_80045364(void); 
extern void func_ovl4_7_800445B4(void); 
extern void func_ovl4_8_80043888(void); 
extern void func_ovl4_9_80043FB4(void); 
extern void func_ovl4_10_80043450(void);
// TODO

// No longer necessary.
/*
RECOMP_PATCH void func_802268F4(void) {
    switch(gOverlayID) {
        case LOADED_OVL_1_SUBOVL_1:   func_ovl1_1_80043010(); return;
        case LOADED_OVL_1_SUBOVL_2:   func_ovl1_2_800430A0(); return;
        case LOADED_OVL_1_SUBOVL_3:   func_ovl1_3_80043010(); return;
        case LOADED_OVL_1_SUBOVL_4:   func_ovl1_4_80043010(); return;
        case LOADED_OVL_2_SUBOVL_1:   func_ovl2_1_80046C88(); return;
        case LOADED_OVL_2_SUBOVL_2:   func_ovl2_2_8004664C(); return;
        case LOADED_OVL_2_SUBOVL_3:   func_ovl2_3_80046A24(); return;
        case LOADED_OVL_2_SUBOVL_4:   func_ovl2_4_800445F8(); return;
        case LOADED_OVL_2_SUBOVL_5:   func_ovl2_5_80047AF4(); return;
        case LOADED_OVL_2_SUBOVL_6:   func_ovl2_6_80044F68(); return;
        case LOADED_OVL_3_SUBOVL_1:   func_ovl3_1_80044108(); return;
        case LOADED_OVL_3_SUBOVL_2:   func_ovl3_2_800478EC(); return;
        case LOADED_OVL_3_SUBOVL_3:   func_ovl3_3_80047198(); return;
        case LOADED_OVL_3_SUBOVL_4:   func_ovl3_4_80043110(); return;
        case LOADED_OVL_3_SUBOVL_5:   func_ovl3_5_80043E04(); return;
        case LOADED_OVL_3_SUBOVL_6:   func_ovl3_6_80043C48(); return;
        case LOADED_OVL_3_SUBOVL_7:   func_ovl3_7_80043010(); return;
        case LOADED_OVL_4_SUBOVL_1:   func_ovl4_1_80047074(); return;
        case LOADED_OVL_4_SUBOVL_2:   func_ovl4_2_8004639C(); return;
        case LOADED_OVL_4_SUBOVL_3:   func_ovl4_3_8004391C(); return;
        case LOADED_OVL_4_SUBOVL_4:   func_ovl4_4_80043ACC(); return;
        case LOADED_OVL_4_SUBOVL_5:   func_ovl4_5_80043970(); return;
        case LOADED_OVL_4_SUBOVL_6:   func_ovl4_6_80045364(); return;
        case LOADED_OVL_4_SUBOVL_7:   func_ovl4_7_800445B4(); return;
        case LOADED_OVL_4_SUBOVL_8:   func_ovl4_8_80043888(); return;
        case LOADED_OVL_4_SUBOVL_9:   func_ovl4_9_80043FB4(); return;
        case LOADED_OVL_4_SUBOVL_10:  func_ovl4_10_80043450(); return;
        case LOADED_OVL_5_SUBOVL_1:   func_ovl5_1_80045A00(); return;
        case LOADED_OVL_5_SUBOVL_2:   func_ovl5_2_80044A50(); return;
        case LOADED_OVL_5_SUBOVL_3:   func_ovl5_3_80044CBC(); return;
        case LOADED_OVL_5_SUBOVL_4:   func_ovl5_4_80045160(); return;
        case LOADED_OVL_5_SUBOVL_5:   func_ovl5_5_80043A94(); return;
        case LOADED_OVL_5_SUBOVL_6:   func_ovl5_6_80045510(); return;
        case LOADED_OVL_5_SUBOVL_7:   func_ovl5_7_80043660(); return;
        case LOADED_OVL_6_SUBOVL_1:   func_ovl6_1_8004448C(); return;
        case LOADED_OVL_6_SUBOVL_2:   func_ovl6_2_80043A50(); return;
        case LOADED_OVL_6_SUBOVL_3:   func_ovl6_3_80043E2C(); return;
        case LOADED_OVL_6_SUBOVL_4:   func_ovl6_4_80043FB8(); return;
        case LOADED_OVL_6_SUBOVL_5:   func_ovl6_5_80043C48(); return;
        case LOADED_OVL_6_SUBOVL_6:   return;
        case LOADED_OVL_6_SUBOVL_7:   return;
        case LOADED_OVL_6_SUBOVL_8:   return;
        case LOADED_OVL_6_SUBOVL_9:   return;
        case LOADED_OVL_6_SUBOVL_10:  return;
        case LOADED_OVL_6_SUBOVL_11:  return;
        case LOADED_OVL_6_SUBOVL_12:  return;
        case LOADED_OVL_6_SUBOVL_13:  return;
        case LOADED_OVL_6_SUBOVL_14:  return;
        case LOADED_OVL_6_SUBOVL_15:  return;
        case LOADED_OVL_7_SUBOVL_1:   return;
        case LOADED_OVL_7_SUBOVL_2:   return;
        case LOADED_OVL_7_SUBOVL_3:   return;
        case LOADED_OVL_7_SUBOVL_4:   return;
        case LOADED_OVL_7_SUBOVL_5:   return;
        case LOADED_OVL_7_SUBOVL_6:   return;
        case LOADED_OVL_7_SUBOVL_7:   return;
        case LOADED_OVL_7_SUBOVL_8:   return;
        case LOADED_OVL_7_SUBOVL_9:   return;
        case LOADED_OVL_7_SUBOVL_10:  return;
        case LOADED_OVL_8_SUBOVL_1:   return;
        case LOADED_OVL_8_SUBOVL_2:   return;
        case LOADED_OVL_8_SUBOVL_3:   return;
        case LOADED_OVL_8_SUBOVL_4:   return;
        case LOADED_OVL_8_SUBOVL_5:   return;
        case LOADED_OVL_8_SUBOVL_6:   return;
        case LOADED_OVL_8_SUBOVL_7:   return;
        case LOADED_OVL_8_SUBOVL_8:   return;
        case LOADED_OVL_8_SUBOVL_9:   return;
        case LOADED_OVL_8_SUBOVL_10:  return;
        case LOADED_OVL_8_SUBOVL_11:  return;
        case LOADED_OVL_8_SUBOVL_12:  return;
        case LOADED_OVL_8_SUBOVL_13:  return;
        case LOADED_OVL_9_SUBOVL_1:   return;
        case LOADED_OVL_9_SUBOVL_2:   return;
        case LOADED_OVL_9_SUBOVL_3:   return;
        case LOADED_OVL_9_SUBOVL_4:   return;
        case LOADED_OVL_9_SUBOVL_5:   return;
        case LOADED_OVL_9_SUBOVL_6:   return;
        case LOADED_OVL_10_SUBOVL_1:  return;
        case LOADED_OVL_10_SUBOVL_2:  return;
        case LOADED_OVL_11_SUBOVL_1:  return;
        case LOADED_OVL_11_SUBOVL_2:  return;
        case LOADED_OVL_12_SUBOVL_1:  return;
        case LOADED_OVL_12_SUBOVL_2:  return;
        case LOADED_OVL_13_SUBOVL_1:  return;
        case LOADED_OVL_14_SUBOVL_1:  return;
        case LOADED_OVL_14_SUBOVL_2:  return;
        case LOADED_OVL_14_SUBOVL_3:  return;
        case LOADED_OVL_14_SUBOVL_4:  return;
        case LOADED_OVL_14_SUBOVL_5:  return;
        case LOADED_OVL_14_SUBOVL_6:  return;
        case LOADED_OVL_14_SUBOVL_7:  return;
        case LOADED_OVL_14_SUBOVL_8:  return;
        case LOADED_OVL_14_SUBOVL_9:  return;
        case LOADED_OVL_14_SUBOVL_10: return;
        case UNSUPPORTED_OVERLAY:
        default: // unsupported overlay.
            while(1)
                ;
    }
    //D_802A532C();
}
*/

extern Gfx D_8029F718[];
extern Gfx D_8029F6D8[];
extern Gfx D_200000[];

extern u8 *D_802A5390;
extern u32 D_802A5388;

extern s32 D_80100000;
extern u8 D_8014C8D0[];
extern s32 D_8029F5E0;
extern s32 D_8029F5F8;
extern s32 D_802A5368;
extern Gfx* gMasterDisplayList; // D_802A5390

void func_802272B0(Gfx** gfxP);

RECOMP_PATCH Gfx *func_80227464(void) {
    D_802A5388 = D_8029F5E0;
    D_802A538C = &D_8014C8D0[D_802A5388 * 0x27000];
    gMasterDisplayList = (Gfx *)((u8*)(D_802A538C) + 0x24000);
    D_802A5368 = 0;

    gEXEnable(gMasterDisplayList++);

    gSPSegment(gMasterDisplayList++, 0x00, 0x00000000);
    gSPSegment(gMasterDisplayList++, 0x01, (void* ) (((u32)D_802A5388 * 0x25800) + 0x80000000 + (u8*)&D_80100000));
    gSPDisplayList(gMasterDisplayList++, D_8029F718);
    if (D_8029F5F8 != 0) {
        gDPSetColorDither(gMasterDisplayList++, G_CD_DISABLE);
    } else {
        gDPSetColorDither(gMasterDisplayList++, G_CD_MAGICSQ);
    }
    gSPDisplayList(gMasterDisplayList++, D_8029F6D8);
    gDPSetDepthImage(gMasterDisplayList++, D_200000);
    gDPPipeSync(gMasterDisplayList++);
    gDPSetScissor(gMasterDisplayList++, G_SC_NON_INTERLACE, 8, 6, 311, 233);
    func_802272B0(&gMasterDisplayList);
    gSPSetGeometryMode(gMasterDisplayList++, G_ZBUFFER | G_CULL_BACK | G_LIGHTING);
    gDPSetRenderMode(gMasterDisplayList++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    return gMasterDisplayList;
}

extern u8 D_1000000[];
extern u8 D_8029F7A0[];

extern s32 D_802A53C0;
extern s32 D_802A53C4;
extern s32 D_802A53C8;
extern s32 D_802A53CC;

extern s32 D_8029F5E4;

extern s32 D_8029F5EC; // r
extern s32 D_8029F5F0; // g
extern s32 D_8029F5F4; // b

extern s32 D_802A53C4;
extern s32 D_802A53C8;

extern s32 D_802A53BC;

/*
RECOMP_PATCH void func_802272B0(Gfx** gfxP) {
    Gfx *gfx = *gfxP;

    gSPDisplayList(gfx++, D_8029F7A0);
    gDPFillRectangle(gfx++, D_802A53BC, D_802A53C0, D_802A53C4 - 1, D_802A53C8 - 1);
    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 424, D_1000000);
    if (D_8029F5E4 != 0) {
        gDPSetFillColor(gfx++, (GPACK_RGBA5551(D_8029F5EC, D_8029F5F0, D_8029F5F4, 1) << 0x10) | GPACK_RGBA5551(D_8029F5EC, D_8029F5F0, D_8029F5F4, 1));
        gDPFillRectangle(gfx++, D_802A53BC, D_802A53C0, D_802A53C4 - 1, D_802A53C8 - 1);
        gDPPipeSync(gfx++);
    }
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    *gfxP = gfx;
}
*/

extern s32 D_8029F5EC;
extern s32 D_8029F5F0;
extern s32 D_8029F5F4;

// set color
RECOMP_PATCH void func_802276E0(s32 arg0, s32 arg1, s32 arg2) {
    D_8029F5EC = arg0;
    D_8029F5F0 = arg1;
    D_8029F5F4 = arg2;
}
