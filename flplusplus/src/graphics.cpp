#include "graphics.h"
#include "patch.h"
#include "offsets.h"
#include "config.h"
#include "Common.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

config::ConfigData& cfg = config::get_config();

#define MIN_DETAIL_SCALE 1.0f
#define MAX_DETAIL_SCALE 1000000.0f

int patch_lodranges(const float* scale)
{
    if(*scale < MIN_DETAIL_SCALE || *scale > MAX_DETAIL_SCALE) return 0;

    if(*scale == 1) {
        //1x lods
        unsigned char og1[5] = { 0x83, 0xFE, 0x08, 0x7D, 0x16 };
        patch::patch_bytes(OF_LODS_P1, (void*)og1, 5);
        patch::patch_uint16(OF_LODS_P2, 0x9090);
        patch::patch_uint16(OF_LODS_P3, 0x9090);
        patch::patch_x3(OF_LODS_P4, 0x90, 0x90, 0x90);
        //original draw distances
        patch::patch_float(OF_REN_DIST0, 10000.0);
        return 1;
    }
    //distances
    float ren_dist0 = 10000.0;

    unsigned char patch1[5] = { 0xE8, 0x7B, 0xFF, 0xFF, 0xFF };
    patch::patch_bytes(OF_LODS_P1, (void*)patch1, 5);
    patch::patch_uint16(OF_LODS_P2, 0x0DD8);
    patch::patch_uint8(OF_LODS_P3 + 1, 0xC3);

    // Write the pointer to the scale factor for the fmul st(0) instruction
    patch::patch_uint32(OF_LODS_P4, (UINT) scale);

    ren_dist0 *= *scale;

    patch::patch_float(OF_REN_DIST0, ren_dist0);
    return 1;
}

float __fastcall multiply_pbubble_float(INI_Reader* reader_ptr, PVOID _edx, UINT index)
{
    INI_Reader *reader = reader_ptr;
    return reader->get_value_float(index) * cfg.pbubblescale;
}

float __fastcall multiply_characterdetail_float(INI_Reader* reader_ptr, PVOID _edx, UINT index)
{
    INI_Reader *reader = reader_ptr;
    return reader->get_value_float(index) * cfg.characterdetailscale;
}

bool patch_pbubble()
{
    if (cfg.pbubblescale < MIN_DETAIL_SCALE || cfg.pbubblescale > MAX_DETAIL_SCALE)
        return false;

    static UINT multiplyPbubblePtr = (UINT) &multiply_pbubble_float;

    patch::patch_uint32(OF_PBUBBLE_GET_VALUE0, (UINT) &multiplyPbubblePtr);
    patch::patch_uint32(OF_PBUBBLE_GET_VALUE1, (UINT) &multiplyPbubblePtr);

    float ren_dist1 = 20000.0f;

    ren_dist1 *= cfg.pbubblescale;

    if (ren_dist1 > 40000.0f)
        ren_dist1 = 40000.0f;

    patch::patch_float(OF_REN_DIST1, ren_dist1);

    return true;
}

bool patch_characterdetail()
{
    if (cfg.characterdetailscale < MIN_DETAIL_SCALE || cfg.characterdetailscale > MAX_DETAIL_SCALE)
        return false;

    auto common = (DWORD) GetModuleHandleA("common.dll");

    UINT multiplyCharacterDetailPtr = (UINT) &multiply_characterdetail_float;
    UINT detailSwitchAddr = common + F_OF_BODYPART_DETAILSWITCH_GET_VALUE;

    patch::patch_uint32(detailSwitchAddr, multiplyCharacterDetailPtr - detailSwitchAddr - 4);

    return true;
}

void graphics::init(bool version11)
{
    patch::patch_uint8(OF_VIDEODIALOG, 0x33); //disable unsupported video dialog
    patch::patch_uint16(OF_MAXTEXSIZE, 0x2000); //texture size bug fix
    //replace "Vibrocentric" string
    //FL tries to load this font over Agency FB, screws up UI if it finds it
    //if you have a font named '\b' you have big problems
    const char *garbageFont = "\b\0";
    auto common = (DWORD) GetModuleHandleA("common.dll");
    unsigned int address = common + (version11 ? F_OF_VIBROCENTRICFONT_V11 : F_OF_VIBROCENTRICFONT_V10);
    patch::patch_bytes(address, (void*)garbageFont, 2);
    //lod 0
    patch_lodranges(&cfg.lodscale);

    patch_pbubble();
    patch_characterdetail();
}
