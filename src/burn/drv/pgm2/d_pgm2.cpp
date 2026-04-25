// d_pgm2.cpp - IGS PGM2 (IGS036) driver definitions for FBNeo

#include "pgm2.h"
#include "pgm2_crypt.h"

static struct BurnInputInfo pgm2InputList[] = {
    { "P1 Coin",          BIT_DIGITAL, Pgm2InputPort1 + 14, "p1 coin"   },
    { "P1 Start",         BIT_DIGITAL, Pgm2InputPort1 + 10, "p1 start"  },
    { "P1 Up",            BIT_DIGITAL, Pgm2InputPort0 + 0,  "p1 up"     },
    { "P1 Down",          BIT_DIGITAL, Pgm2InputPort0 + 1,  "p1 down"   },
    { "P1 Left",          BIT_DIGITAL, Pgm2InputPort0 + 2,  "p1 left"   },
    { "P1 Right",         BIT_DIGITAL, Pgm2InputPort0 + 3,  "p1 right"  },
    { "P1 Button 1",      BIT_DIGITAL, Pgm2InputPort0 + 4,  "p1 fire 1" },
    { "P1 Button 2",      BIT_DIGITAL, Pgm2InputPort0 + 5,  "p1 fire 2" },
    { "P1 Button 3",      BIT_DIGITAL, Pgm2InputPort0 + 6,  "p1 fire 3" },
    { "P1 Button 4",      BIT_DIGITAL, Pgm2InputPort0 + 7,  "p1 fire 4" },

    { "P2 Coin",          BIT_DIGITAL, Pgm2InputPort1 + 15, "p2 coin"   },
    { "P2 Start",         BIT_DIGITAL, Pgm2InputPort1 + 11, "p2 start"  },
    { "P2 Up",            BIT_DIGITAL, Pgm2InputPort0 + 10, "p2 up"     },
    { "P2 Down",          BIT_DIGITAL, Pgm2InputPort0 + 11, "p2 down"   },
    { "P2 Left",          BIT_DIGITAL, Pgm2InputPort0 + 12, "p2 left"   },
    { "P2 Right",         BIT_DIGITAL, Pgm2InputPort0 + 13, "p2 right"  },
    { "P2 Button 1",      BIT_DIGITAL, Pgm2InputPort0 + 14, "p2 fire 1" },
    { "P2 Button 2",      BIT_DIGITAL, Pgm2InputPort0 + 15, "p2 fire 2" },
    { "P2 Button 3",      BIT_DIGITAL, Pgm2InputPort0 + 16, "p2 fire 3" },
    { "P2 Button 4",      BIT_DIGITAL, Pgm2InputPort0 + 17, "p2 fire 4" },

    { "P3 Coin",          BIT_DIGITAL, Pgm2InputPort1 + 16, "p3 coin"   },
    { "P3 Start",         BIT_DIGITAL, Pgm2InputPort1 + 12, "p3 start"  },
    { "P3 Up",            BIT_DIGITAL, Pgm2InputPort0 + 20, "p3 up"     },
    { "P3 Down",          BIT_DIGITAL, Pgm2InputPort0 + 21, "p3 down"   },
    { "P3 Left",          BIT_DIGITAL, Pgm2InputPort0 + 22, "p3 left"   },
    { "P3 Right",         BIT_DIGITAL, Pgm2InputPort0 + 23, "p3 right"  },
    { "P3 Button 1",      BIT_DIGITAL, Pgm2InputPort0 + 24, "p3 fire 1" },
    { "P3 Button 2",      BIT_DIGITAL, Pgm2InputPort0 + 25, "p3 fire 2" },
    { "P3 Button 3",      BIT_DIGITAL, Pgm2InputPort0 + 26, "p3 fire 3" },
    { "P3 Button 4",      BIT_DIGITAL, Pgm2InputPort0 + 27, "p3 fire 4" },

    { "P4 Coin",          BIT_DIGITAL, Pgm2InputPort1 + 17, "p4 coin"   },
    { "P4 Start",         BIT_DIGITAL, Pgm2InputPort1 + 13, "p4 start"  },
    { "P4 Up",            BIT_DIGITAL, Pgm2InputPort1 + 0,  "p4 up"     },
    { "P4 Down",          BIT_DIGITAL, Pgm2InputPort1 + 1,  "p4 down"   },
    { "P4 Left",          BIT_DIGITAL, Pgm2InputPort1 + 2,  "p4 left"   },
    { "P4 Right",         BIT_DIGITAL, Pgm2InputPort1 + 3,  "p4 right"  },
    { "P4 Button 1",      BIT_DIGITAL, Pgm2InputPort1 + 4,  "p4 fire 1" },
    { "P4 Button 2",      BIT_DIGITAL, Pgm2InputPort1 + 5,  "p4 fire 2" },
    { "P4 Button 3",      BIT_DIGITAL, Pgm2InputPort1 + 6,  "p4 fire 3" },
    { "P4 Button 4",      BIT_DIGITAL, Pgm2InputPort1 + 7,  "p4 fire 4" },

    { "Test Key P1 & P2", BIT_DIGITAL, Pgm2InputPort1 + 18, "diag"      },
    { "Test Key P3 & P4", BIT_DIGITAL, Pgm2InputPort1 + 19, "diag 2"    },
    { "Service P1 & P2",  BIT_DIGITAL, Pgm2InputPort1 + 20, "service"   },
    { "Service P3 & P4",  BIT_DIGITAL, Pgm2InputPort1 + 21, "service 2" },
    { "ResetGame",        BIT_DIGITAL, &Pgm2Reset,          "reset"     },
    { "Dip A",            BIT_DIPSWITCH, Pgm2Dip + 0,       "dip"       },
    { "Dip B",            BIT_DIPSWITCH, &CardlessHack,     "dip"       },
};
STDINPUTINFO(pgm2)

static struct BurnDIPInfo pgm2DIPList[] = {
    DIP_OFFSET(0x2d)
    { 0x00, 0xff, 0xff, 0xff, NULL },
    { 0   , 0xfe, 0   ,    2, "Service Mode" },
    { 0x00, 0x01, 0x01, 0x01, "Off" },
    { 0x00, 0x01, 0x01, 0x00, "On" },
    { 0   , 0xfe, 0   ,    2, "Music" },
    { 0x00, 0x01, 0x02, 0x00, "Off" },
    { 0x00, 0x01, 0x02, 0x02, "On" },
    { 0   , 0xfe, 0   ,    2, "Voice" },
    { 0x00, 0x01, 0x04, 0x00, "Off" },
    { 0x00, 0x01, 0x04, 0x04, "On" },
    { 0   , 0xfe, 0   ,    2, "Free Play" },
    { 0x00, 0x01, 0x08, 0x08, "Off" },
    { 0x00, 0x01, 0x08, 0x00, "On" },
    { 0   , 0xfe, 0   ,    2, "Stop" },
    { 0x00, 0x01, 0x10, 0x10, "Off" },
    { 0x00, 0x01, 0x10, 0x00, "On" },
    { 0   , 0xfe, 0   ,    2, "Unused SW1:6" },
    { 0x00, 0x01, 0x20, 0x20, "Off" },
    { 0x00, 0x01, 0x20, 0x00, "On" },
    { 0   , 0xfe, 0   ,    2, "Unused SW1:7" },
    { 0x00, 0x01, 0x40, 0x40, "Off" },
    { 0x00, 0x01, 0x40, 0x00, "On" },
    { 0   , 0xfe, 0   ,    2, "Debug" },
    { 0x00, 0x01, 0x80, 0x80, "Off" },
    { 0x00, 0x01, 0x80, 0x00, "On" },
    { 0   , 0xfe, 0   ,    2, "Cardless Mode" },
    { 0x01, 0x01, 0x01, 0x00, "Off" },
    { 0x01, 0x01, 0x01, 0x01, "On" },
};
STDDIPINFO(pgm2)

static bool pgm2LoadRom(UINT8 **pBuf, INT32 *pLen, INT32 nLen, INT32 nRomIdx)
{
    *pLen = nLen;
    *pBuf = (UINT8*)BurnMalloc(nLen);
    if (!*pBuf) return false;
    memset(*pBuf, 0x00, nLen);
    return BurnLoadRom(*pBuf, nRomIdx, 1) == 0;
}

static void orleg2LoadRoms()
{
    if (!pgm2LoadRom(&Pgm2IntROM,  &Pgm2IntROMLen,  0x0004000, 0)) return;
    if (!pgm2LoadRom(&Pgm2ArmROM,  &Pgm2ArmROMLen,  0x1000000, 1)) return;
    if (!pgm2LoadRom(&Pgm2TileROM, &Pgm2TileROMLen, 0x0200000, 2)) return;

    Pgm2SndROMLen = 0x1000000;
    Pgm2SndROM = (UINT8*)BurnMalloc(Pgm2SndROMLen);
    if (!Pgm2SndROM) return;
    memset(Pgm2SndROM, 0x00, Pgm2SndROMLen);
    if (BurnLoadRomExt(Pgm2SndROM, 9, 1, LD_BYTESWAP)) return;

    Pgm2BgROMLen = 0x1000000;
    Pgm2MaskROMOffset  = 0x1000000;
    Pgm2MaskROMLen     = 0x2000000;
    Pgm2ColourROMOffset = 0x3000000;
    Pgm2SprROMLen = 0x1000000 + 0x2000000 + 0x4000000;
    Pgm2SprROM = (UINT8*)BurnMalloc(Pgm2SprROMLen);
    if (!Pgm2SprROM) return;
    memset(Pgm2SprROM, 0x00, Pgm2SprROMLen);
    BurnLoadRomExt(Pgm2SprROM + 0x0000000, 3, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x0000002, 4, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x1000000, 5, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x1000002, 6, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x3000000, 7, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x3000002, 8, 4, LD_GROUP(2));
    pgm2_decode_sprite_data(0x1000000, 0x2000000, 0x3000000, 0x4000000);
}

static void orleg2CardLoadRoms()
{
    if (!pgm2LoadRom(&Pgm2IntROM,  &Pgm2IntROMLen,  0x0004000, 0)) return;
    if (!pgm2LoadRom(&Pgm2ArmROM,  &Pgm2ArmROMLen,  0x1000000, 2)) return;
    if (!pgm2LoadRom(&Pgm2TileROM, &Pgm2TileROMLen, 0x0200000, 3)) return;

    Pgm2SndROMLen = 0x1000000;
    Pgm2SndROM = (UINT8*)BurnMalloc(Pgm2SndROMLen);
    if (!Pgm2SndROM) return;
    memset(Pgm2SndROM, 0x00, Pgm2SndROMLen);
    if (BurnLoadRomExt(Pgm2SndROM, 10, 1, LD_BYTESWAP)) return;

    Pgm2BgROMLen = 0x1000000;
    Pgm2MaskROMOffset  = 0x1000000;
    Pgm2MaskROMLen     = 0x2000000;
    Pgm2ColourROMOffset = 0x3000000;
    Pgm2SprROMLen = 0x1000000 + 0x2000000 + 0x4000000;
    Pgm2SprROM = (UINT8*)BurnMalloc(Pgm2SprROMLen);
    if (!Pgm2SprROM) return;
    memset(Pgm2SprROM, 0x00, Pgm2SprROMLen);
    BurnLoadRomExt(Pgm2SprROM + 0x0000000, 4, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x0000002, 5, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x1000000, 6, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x1000002, 7, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x3000000, 8, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x3000002, 9, 4, LD_GROUP(2));
    pgm2_decode_sprite_data(0x1000000, 0x2000000, 0x3000000, 0x4000000);
}

static void kov2nlLoadRoms()
{
    if (!pgm2LoadRom(&Pgm2IntROM,  &Pgm2IntROMLen,  0x0004000, 0)) return;
    if (!pgm2LoadRom(&Pgm2ArmROM,  &Pgm2ArmROMLen,  0x1000000, 2)) return;
    if (!pgm2LoadRom(&Pgm2TileROM, &Pgm2TileROMLen, 0x0200000, 3)) return;

    Pgm2SndROMLen = 0x2000000;
    Pgm2SndROM = (UINT8*)BurnMalloc(Pgm2SndROMLen);
    if (!Pgm2SndROM) return;
    memset(Pgm2SndROM, 0x00, Pgm2SndROMLen);
    if (BurnLoadRomExt(Pgm2SndROM, 10, 1, LD_BYTESWAP)) return;

    Pgm2BgROMLen = 0x1000000;
    Pgm2MaskROMOffset  = 0x1000000;
    Pgm2MaskROMLen     = 0x2000000;
    Pgm2ColourROMOffset = 0x3000000;
    Pgm2SprROMLen = 0x1000000 + 0x2000000 + 0x4000000;
    Pgm2SprROM = (UINT8*)BurnMalloc(Pgm2SprROMLen);
    if (!Pgm2SprROM) return;
    memset(Pgm2SprROM, 0x00, Pgm2SprROMLen);
    BurnLoadRomExt(Pgm2SprROM + 0x0000000, 4, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x0000002, 5, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x1000000, 6, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x1000002, 7, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x3000000, 8, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x3000002, 9, 4, LD_GROUP(2));
    pgm2_decode_sprite_data(0x1000000, 0x2000000, 0x3000000, 0x4000000);
}

static void kov3LoadRoms()
{
    if (!pgm2LoadRom(&Pgm2IntROM,  &Pgm2IntROMLen,  0x0004000, 0)) return;
    if (!pgm2LoadRom(&Pgm2ArmROM,  &Pgm2ArmROMLen,  0x1000000, 1)) return;
    if (!pgm2LoadRom(&Pgm2TileROM, &Pgm2TileROMLen, 0x0200000, 2)) return;

    Pgm2SndROMLen = 0x4000000;
    Pgm2SndROM = (UINT8*)BurnMalloc(Pgm2SndROMLen);
    if (!Pgm2SndROM) return;
    memset(Pgm2SndROM, 0x00, Pgm2SndROMLen);
    if (BurnLoadRomExt(Pgm2SndROM, 9, 1, LD_BYTESWAP)) return;

    Pgm2BgROMLen = 0x2000000;
    Pgm2MaskROMOffset  = 0x2000000;
    Pgm2MaskROMLen     = 0x4000000;
    Pgm2ColourROMOffset = 0x6000000;
    Pgm2SprROMLen = 0x2000000 + 0x4000000 + 0x8000000;
    Pgm2SprROM = (UINT8*)BurnMalloc(Pgm2SprROMLen);
    if (!Pgm2SprROM) return;
    memset(Pgm2SprROM, 0x00, Pgm2SprROMLen);
    BurnLoadRomExt(Pgm2SprROM + 0x0000000, 3, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x0000002, 4, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x2000000, 5, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x2000002, 6, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x6000000, 7, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x6000002, 8, 4, LD_GROUP(2));
    pgm2_decode_sprite_data(0x2000000, 0x4000000, 0x6000000, 0x8000000);
}

static INT32 orleg2InitCommon()
{
    pgm2SetStorageRomIndices(-1, 10);
    pgm2SetArmRomIndex(1);
    pgm2SetSpeedhack(0, 0x20020114, 0x1002faec, 0x1002f9b8, 0, 0);
    pPgm2InitCallback = orleg2LoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

static INT32 orleg2CardInitCommon()
{
    pgm2SetStorageRomIndices(1, 11);
    pgm2SetMaxCardSlots(4);
    pgm2SetCardRomIndex(0, 1);
    pgm2SetCardRomIndex(1, 1);
    pgm2SetCardRomIndex(2, 1);
    pgm2SetCardRomIndex(3, 1);
    pgm2SetArmRomIndex(2);
    pgm2SetSpeedhack(0, 0x20020114, 0x1002faec, 0x1002f9b8, 0, 0);
    pPgm2InitCallback = orleg2CardLoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

static INT32 kov2nlInitCommon()
{
    pgm2SetStorageRomIndices(1, 11);
    pgm2SetMaxCardSlots(4);
    pgm2SetCardRomIndex(0, 1);
    pgm2SetCardRomIndex(1, 1);
    pgm2SetCardRomIndex(2, 1);
    pgm2SetCardRomIndex(3, 1);
    pgm2SetArmRomIndex(2);
    pgm2SetSpeedhack(0, 0x20020470, 0x10053a94, 0x1005332c, 0x1005327c, 0);
    pPgm2InitCallback = kov2nlLoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

static const UINT8 kov3_104_key[8] = { 0x40, 0xac, 0x30, 0x00, 0x47, 0x49, 0x00, 0x00 };
static const UINT8 kov3_104_sum[8] = { 0xeb, 0x7d, 0x8d, 0x90, 0x2c, 0xf4, 0x09, 0x82 };
static const UINT8 kov3_102_key[8] = { 0x49, 0xac, 0xb0, 0xec, 0x47, 0x49, 0x95, 0x38 };
static const UINT8 kov3_102_sum[8] = { 0x09, 0xbd, 0xf1, 0x31, 0xe6, 0xf0, 0x65, 0x2b };
static const UINT8 kov3_101_key[8] = { 0xc1, 0x2c, 0xc1, 0xe5, 0x3c, 0xc1, 0x59, 0x9e };
static const UINT8 kov3_101_sum[8] = { 0xf2, 0xb2, 0xf0, 0x89, 0x37, 0xf2, 0xc7, 0x0b };
static const UINT8 kov3_100_key[8] = { 0x40, 0xac, 0x30, 0x00, 0x47, 0x49, 0x00, 0x00 };
static const UINT8 kov3_100_sum[8] = { 0x96, 0xf0, 0x91, 0xe1, 0xb3, 0xf1, 0xef, 0x90 };

static INT32 kov3Init()
{
    pgm2SetStorageRomIndices(10, 11);
    pgm2SetMaxCardSlots(2);
    pgm2SetCardRomIndex(0, 10);
    pgm2SetCardRomIndex(1, 10);
    pgm2EnableKov3Module(kov3_104_key, kov3_104_sum, 0x18ec71, 0xb89d);
    pgm2SetSpeedhack(0, 0x200000b4, 0x1000729a, 0x1000729e, 0, 0);
    pPgm2InitCallback = kov3LoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

static INT32 kov3_102Init()
{
    pgm2SetStorageRomIndices(10, 11);
    pgm2SetMaxCardSlots(2);
    pgm2SetCardRomIndex(0, 10);
    pgm2SetCardRomIndex(1, 10);
    pgm2EnableKov3Module(kov3_102_key, kov3_102_sum, 0x021d37, 0x81d0);
    pgm2SetSpeedhack(0, 0x200000b4, 0x1000729a, 0x1000729e, 0, 0);
    pPgm2InitCallback = kov3LoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

static INT32 kov3_101Init()
{
    pgm2SetStorageRomIndices(10, 11);
    pgm2SetMaxCardSlots(2);
    pgm2SetCardRomIndex(0, 10);
    pgm2SetCardRomIndex(1, 10);
    pgm2EnableKov3Module(kov3_101_key, kov3_101_sum, 0x000000, 0xffff);
    pgm2SetSpeedhack(0, 0x200000b4, 0x1000729a, 0x1000729e, 0, 0);
    pPgm2InitCallback = kov3LoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

static INT32 kov3_100Init()
{
    pgm2SetStorageRomIndices(10, 11);
    pgm2SetMaxCardSlots(2);
    pgm2SetCardRomIndex(0, 10);
    pgm2SetCardRomIndex(1, 10);
    pgm2EnableKov3Module(kov3_100_key, kov3_100_sum, 0x3e8aa8, 0xc530);
    pgm2SetSpeedhack(0, 0x200000b4, 0x1000729a, 0x1000729e, 0, 0);
    pPgm2InitCallback = kov3LoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

// ddpdojt ROM loading
static void ddpdojtLoadRoms()
{
    if (!pgm2LoadRom(&Pgm2IntROM,  &Pgm2IntROMLen,  0x0004000, 0)) return;
    if (!pgm2LoadRom(&Pgm2ArmROM,  &Pgm2ArmROMLen,  0x0200000, 1)) return;
    if (!pgm2LoadRom(&Pgm2TileROM, &Pgm2TileROMLen, 0x0200000, 2)) return;

    Pgm2SndROMLen = 0x1000000;
    Pgm2SndROM = (UINT8*)BurnMalloc(Pgm2SndROMLen);
    if (!Pgm2SndROM) return;
    memset(Pgm2SndROM, 0x00, Pgm2SndROMLen);
    if (BurnLoadRomExt(Pgm2SndROM, 9, 1, LD_BYTESWAP)) return;

    // BG: bgl(0x1000000)+bgh(0x1000000) = 0x2000000 interleaved
    // Mask: mapl0(0x800000)+maph0(0x800000) = 0x1000000 interleaved
    // Colour: spa0(0x1000000)+spb0(0x1000000) = 0x2000000 interleaved
    Pgm2BgROMLen = 0x2000000;
    Pgm2MaskROMOffset  = 0x2000000;
    Pgm2MaskROMLen     = 0x1000000;
    Pgm2ColourROMOffset = 0x3000000;
    Pgm2SprROMLen = 0x2000000 + 0x1000000 + 0x2000000;
    Pgm2SprROM = (UINT8*)BurnMalloc(Pgm2SprROMLen);
    if (!Pgm2SprROM) return;
    memset(Pgm2SprROM, 0x00, Pgm2SprROMLen);
    BurnLoadRomExt(Pgm2SprROM + 0x0000000, 3, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x0000002, 4, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x2000000, 5, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x2000002, 6, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x3000000, 7, 4, LD_GROUP(2));
    BurnLoadRomExt(Pgm2SprROM + 0x3000002, 8, 4, LD_GROUP(2));
    pgm2_decode_sprite_data(0x2000000, 0x1000000, 0x3000000, 0x2000000);
}

static INT32 ddpdojtInit()
{
    pgm2SetStorageRomIndices(-1, 10);
    pgm2SetRamRomBoard(0x200000);  // 2MB RAM before ROM
	pgm2SetSpeedhack(0, 0x20000060, 0x10001a7e, 0, 0, 0);
	pgm2SetSpeedhack(1, 0x20021e04, 0x1008fefe, 0x1008fbe8, 0, 0);
    pPgm2InitCallback = ddpdojtLoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

// kof98umh ROM loading
static void kof98umhLoadRoms()
{
    if (!pgm2LoadRom(&Pgm2IntROM,  &Pgm2IntROMLen,  0x0004000, 0)) return;
    if (!pgm2LoadRom(&Pgm2ArmROM,  &Pgm2ArmROMLen,  0x1000000, 1)) return;
    if (!pgm2LoadRom(&Pgm2TileROM, &Pgm2TileROMLen, 0x0200000, 2)) return;

    // Two sound ROMs concatenated
    Pgm2SndROMLen = 0x8000000;
    Pgm2SndROM = (UINT8*)BurnMalloc(Pgm2SndROMLen);
    if (!Pgm2SndROM) return;
    memset(Pgm2SndROM, 0x00, Pgm2SndROMLen);
    if (BurnLoadRomExt(Pgm2SndROM + 0x0000000, 9,  1, LD_BYTESWAP)) return;
    if (BurnLoadRomExt(Pgm2SndROM + 0x4000000, 10, 1, LD_BYTESWAP)) return;

    // No BG tiles (MAME: bgtile region is ERASE00 with nothing loaded)
    // Mask: mapl0(0x4000000)+maph0(0x4000000) = 0x8000000 interleaved
    // Colour: spa0+spb0 at 0, spa2+spb2 at 0x10000000 = 0x18000000 with gap
    Pgm2BgROMLen = 0;
    Pgm2MaskROMOffset  = 0;
    Pgm2MaskROMLen     = 0x8000000;
    Pgm2ColourROMOffset = 0x8000000;
    Pgm2SprROMLen = 0x8000000 + 0x18000000;
    Pgm2SprROM = (UINT8*)BurnMalloc(Pgm2SprROMLen);
    if (!Pgm2SprROM) return;
    memset(Pgm2SprROM, 0x00, Pgm2SprROMLen);
    BurnLoadRomExt(Pgm2SprROM + 0x0000000, 3, 4, LD_GROUP(2));  // mapl0
    BurnLoadRomExt(Pgm2SprROM + 0x0000002, 4, 4, LD_GROUP(2));  // maph0
    BurnLoadRomExt(Pgm2SprROM + 0x8000000, 5, 4, LD_GROUP(2));  // spa0
    BurnLoadRomExt(Pgm2SprROM + 0x8000002, 6, 4, LD_GROUP(2));  // spb0
    // spa1/spb1 unpopulated (gap at offset 0x10000000)
    BurnLoadRomExt(Pgm2SprROM + 0x18000000, 7, 4, LD_GROUP(2)); // spa2
    BurnLoadRomExt(Pgm2SprROM + 0x18000002, 8, 4, LD_GROUP(2)); // spb2
    pgm2_decode_sprite_data(0x0, 0x8000000, 0x8000000, 0x18000000);
}

static INT32 kof98umhInit()
{
    pgm2SetStorageRomIndices(-1, 11);
    pgm2SetSpeedhack(0, 0x20000060, 0x100028f6, 0, 0, 0);
    pgm2SetRefreshRate(15625.0 / 264.0);  // ~59.19 Hz (MAME pgm2_lores config)
    pPgm2InitCallback = kof98umhLoadRoms;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    return pgm2Init();
}

// orleg2 family
#define ORLEG2_INTERNAL_CHINA \
    { "xyj2_igs036_china.rom",       0x0004000, 0xbcce7641, BRF_PRG | BRF_ESS }, \
    { "blank_orleg2_china_card.pg2", 0x0000108, 0xdc29556f, BRF_OPT },
#define ORLEG2_INTERNAL_HONGKONG \
    { "xyj2_hk.igs036",               0x0004000, 0xee7343c6, BRF_PRG | BRF_ESS }, \
    { "blank_orleg2_taiwan_card.pg2", 0x0000108, 0xcff88f98, BRF_OPT },
#define ORLEG2_INTERNAL_TAIWAN \
    { "xyj2_tw.igs036",               0x0004000, 0x3b8a6703, BRF_PRG | BRF_ESS }, \
    { "blank_orleg2_taiwan_card.pg2", 0x0000108, 0xcff88f98, BRF_OPT },
#define ORLEG2_INTERNAL_OVERSEAS \
    { "ol2_fa.igs036",                0x0004000, 0xcc4d398a, BRF_PRG | BRF_ESS },
#define ORLEG2_INTERNAL_JAPAN \
    { "ol2_a10.igs036",               0x0004000, 0x69375284, BRF_PRG | BRF_ESS },

#define ORLEG2_PROGRAM_104(prefix, extension) \
    { #prefix "_v104" #extension ".u7", 0x0800000, 0x7c24a4f5, BRF_PRG | BRF_ESS }, // V104 08-03-03 13:25:37
#define ORLEG2_PROGRAM_103(prefix, extension) \
    { #prefix "_v103" #extension ".u7", 0x0800000, 0x21c1fae8, BRF_PRG | BRF_ESS }, // V103 08-01-30 14:45:17
#define ORLEG2_PROGRAM_101(prefix, extension) \
    { #prefix "_v101" #extension ".u7", 0x0800000, 0x45805b53, BRF_PRG | BRF_ESS }, // V101 07-12-24 09:32:32

#define ORLEG2_COMMON_ROMS \
    { "ig-a_text.u4",  0x0200000, 0xfa444c32, BRF_GRA }, \
    { "ig-a_bgl.u35",  0x0800000, 0x083a8315, BRF_GRA }, \
    { "ig-a_bgh.u36",  0x0800000, 0xe197221d, BRF_GRA }, \
    { "ig-a_bml.u12",  0x1000000, 0x113a331c, BRF_GRA }, \
    { "ig-a_bmh.u16",  0x1000000, 0xfbf411c8, BRF_GRA }, \
    { "ig-a_cgl.u18",  0x2000000, 0x43501fa6, BRF_GRA }, \
    { "ig-a_cgh.u26",  0x2000000, 0x7051d020, BRF_GRA }, \
    { "ig-a_sp.u2",    0x1000000, 0x8250688c, BRF_SND }, \
    { "xyj2_nvram",    0x0010000, 0xccccc71c, BRF_OPT },

static struct BurnRomInfo orleg2RomDesc[] = {
    ORLEG2_INTERNAL_OVERSEAS
    ORLEG2_PROGRAM_104(ol2,fa)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2)
STD_ROM_FN(orleg2)

static struct BurnRomInfo orleg2_103RomDesc[] = {
    ORLEG2_INTERNAL_OVERSEAS
    ORLEG2_PROGRAM_103(ol2,fa)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_103)
STD_ROM_FN(orleg2_103)

static struct BurnRomInfo orleg2_101RomDesc[] = {
    ORLEG2_INTERNAL_OVERSEAS
    ORLEG2_PROGRAM_101(ol2,fa)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_101)
STD_ROM_FN(orleg2_101)

static struct BurnRomInfo orleg2_104cnRomDesc[] = {
    ORLEG2_INTERNAL_CHINA
    ORLEG2_PROGRAM_104(xyj2,cn)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_104cn)
STD_ROM_FN(orleg2_104cn)

static struct BurnRomInfo orleg2_103cnRomDesc[] = {
    ORLEG2_INTERNAL_CHINA
    ORLEG2_PROGRAM_103(xyj2,cn)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_103cn)
STD_ROM_FN(orleg2_103cn)

static struct BurnRomInfo orleg2_101cnRomDesc[] = {
    ORLEG2_INTERNAL_CHINA
    ORLEG2_PROGRAM_101(xyj2,cn)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_101cn)
STD_ROM_FN(orleg2_101cn)

static struct BurnRomInfo orleg2_104hkRomDesc[] = {
    ORLEG2_INTERNAL_HONGKONG
    ORLEG2_PROGRAM_104(xyj2,hk)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_104hk)
STD_ROM_FN(orleg2_104hk)

static struct BurnRomInfo orleg2_103hkRomDesc[] = {
    ORLEG2_INTERNAL_HONGKONG
    ORLEG2_PROGRAM_103(xyj2,hk)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_103hk)
STD_ROM_FN(orleg2_103hk)

static struct BurnRomInfo orleg2_101hkRomDesc[] = {
    ORLEG2_INTERNAL_HONGKONG
    ORLEG2_PROGRAM_101(xyj2,hk)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_101hk)
STD_ROM_FN(orleg2_101hk)

static struct BurnRomInfo orleg2_104jpRomDesc[] = {
    ORLEG2_INTERNAL_JAPAN
    ORLEG2_PROGRAM_104(ol2,a10)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_104jp)
STD_ROM_FN(orleg2_104jp)

static struct BurnRomInfo orleg2_103jpRomDesc[] = {
    ORLEG2_INTERNAL_JAPAN
    ORLEG2_PROGRAM_103(ol2,a10)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_103jp)
STD_ROM_FN(orleg2_103jp)

static struct BurnRomInfo orleg2_101jpRomDesc[] = {
    ORLEG2_INTERNAL_JAPAN
    ORLEG2_PROGRAM_101(ol2,a10)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_101jp)
STD_ROM_FN(orleg2_101jp)

static struct BurnRomInfo orleg2_104twRomDesc[] = {
    ORLEG2_INTERNAL_TAIWAN
    ORLEG2_PROGRAM_104(xyj2,tw)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_104tw)
STD_ROM_FN(orleg2_104tw)

static struct BurnRomInfo orleg2_103twRomDesc[] = {
    ORLEG2_INTERNAL_TAIWAN
    ORLEG2_PROGRAM_103(xyj2,tw)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_103tw)
STD_ROM_FN(orleg2_103tw)

static struct BurnRomInfo orleg2_101twRomDesc[] = {
    ORLEG2_INTERNAL_TAIWAN
    ORLEG2_PROGRAM_101(xyj2,tw)
    ORLEG2_COMMON_ROMS
};
STD_ROM_PICK(orleg2_101tw)
STD_ROM_FN(orleg2_101tw)

// kov2nl family
#define KOV2NL_INTERNAL_CHINA \
	{ "gsyx_igs036_china.rom",          0x0004000, 0xe09fe4ce, BRF_PRG | BRF_ESS }, \
	{ "blank_gsyx_china.pg2",           0x0000108, 0x02842ae8, BRF_OPT },
#define KOV2NL_INTERNAL_TAIWAN \
	{ "kov2nl_igs036_taiwan.rom",       0x0004000, 0xb3ca3124, BRF_PRG | BRF_ESS }, \
	{ "blank_kov2nl_overseas_card.pg2", 0x0000108, 0x1155f01f, BRF_OPT },
#define KOV2NL_INTERNAL_JAPAN \
	{ "kov2nl_igs036_japan.rom",        0x0004000, 0x46344f1a, BRF_PRG | BRF_ESS }, \
	{ "blank_kov2nl_japan_card.pg2",    0x0000108, 0x0d63cb64, BRF_OPT },
#define KOV2NL_INTERNAL_KOREA \
	{ "kov2nl_igs036_korea.rom",        0x0004000, 0x15619af0, BRF_PRG | BRF_ESS }, \
	{ "blank_kov2nl_overseas_card.pg2", 0x0000108, 0x1155f01f, BRF_OPT },
#define KOV2NL_INTERNAL_HONGKONG \
	{ "kov2nl_igs036_hongkong.rom",     0x0004000, 0x76b9b527, BRF_PRG | BRF_ESS }, \
	{ "blank_gsyx_china.pg2",           0x0000108, 0x02842ae8, BRF_OPT },
#define KOV2NL_INTERNAL_OVERSEA \
	{ "kov2nl_igs036_oversea.rom",      0x0004000, 0x25ec60cd, BRF_PRG | BRF_ESS }, \
	{ "blank_kov2nl_overseas_card.pg2", 0x0000108, 0x1155f01f, BRF_OPT },

#define KOV2NL_PROGRAM_302(prefix, extension) \
	{ #prefix "_v302" #extension ".u7",            0x0800000, 0xb19cf540, BRF_PRG | BRF_ESS }, // V302 08-12-03 15:27:34
#define KOV2NL_PROGRAM_301(prefix, extension) \
	{ #prefix "_v301" #extension ".u7",            0x0800000, 0xc4595c2c, BRF_PRG | BRF_ESS }, // V301 08-09-09 09:44:53
#define KOV2NL_PROGRAM_300(prefix, extension) \
	{ #prefix "_v300" #extension ".u7",            0x0800000, 0x08da7552, BRF_PRG | BRF_ESS }, // V300 08-08-06 18:21:23

#define KOV2NL_COMMON_ROMS \
	{ "ig-a3_text.u4",             0x0200000, 0x214530ff, BRF_GRA }, \
	{ "ig-a3_bgl.u35",             0x0800000, 0x2d46b1f6, BRF_GRA }, \
	{ "ig-a3_bgh.u36",             0x0800000, 0xdf710c36, BRF_GRA }, \
	{ "ig-a3_bml.u12",             0x1000000, 0x0bf63836, BRF_GRA }, \
	{ "ig-a3_bmh.u16",             0x1000000, 0x4a378542, BRF_GRA }, \
	{ "ig-a3_cgl.u18",             0x2000000, 0x8d923e1f, BRF_GRA }, \
	{ "ig-a3_cgh.u26",             0x2000000, 0x5b6fbf3f, BRF_GRA }, \
	{ "ig-a3_sp.u37",              0x2000000, 0x45cdf422, BRF_SND }, \
	{ "gsyx_nvram",                0x0010000, 0x22400c16, BRF_OPT },

static struct BurnRomInfo kov2nlRomDesc[] = {
	KOV2NL_INTERNAL_OVERSEA
	KOV2NL_PROGRAM_302(kov2nl, fa)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl)
STD_ROM_FN(kov2nl)

static struct BurnRomInfo kov2nl_301RomDesc[] = {
	KOV2NL_INTERNAL_OVERSEA
	KOV2NL_PROGRAM_301(kov2nl, fa)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_301)
STD_ROM_FN(kov2nl_301)

static struct BurnRomInfo kov2nl_300RomDesc[] = {
	KOV2NL_INTERNAL_OVERSEA
	KOV2NL_PROGRAM_300(kov2nl, fa)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_300)
STD_ROM_FN(kov2nl_300)

static struct BurnRomInfo kov2nl_302cnRomDesc[] = {
	KOV2NL_INTERNAL_CHINA
	KOV2NL_PROGRAM_302(gsyx, cn)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_302cn)
STD_ROM_FN(kov2nl_302cn)

static struct BurnRomInfo kov2nl_301cnRomDesc[] = {
	KOV2NL_INTERNAL_CHINA
	KOV2NL_PROGRAM_301(gsyx, cn)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_301cn)
STD_ROM_FN(kov2nl_301cn)

static struct BurnRomInfo kov2nl_300cnRomDesc[] = {
	KOV2NL_INTERNAL_CHINA
	KOV2NL_PROGRAM_300(gsyx, cn)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_300cn)
STD_ROM_FN(kov2nl_300cn)

static struct BurnRomInfo kov2nl_302hkRomDesc[] = {
	KOV2NL_INTERNAL_HONGKONG
	KOV2NL_PROGRAM_302(kov2nl, hk)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_302hk)
STD_ROM_FN(kov2nl_302hk)

static struct BurnRomInfo kov2nl_301hkRomDesc[] = {
	KOV2NL_INTERNAL_HONGKONG
	KOV2NL_PROGRAM_301(kov2nl, hk)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_301hk)
STD_ROM_FN(kov2nl_301hk)

static struct BurnRomInfo kov2nl_300hkRomDesc[] = {
	KOV2NL_INTERNAL_HONGKONG
	KOV2NL_PROGRAM_300(kov2nl, hk)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_300hk)
STD_ROM_FN(kov2nl_300hk)

static struct BurnRomInfo kov2nl_302jpRomDesc[] = {
	KOV2NL_INTERNAL_JAPAN
	KOV2NL_PROGRAM_302(kov2nl, jp)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_302jp)
STD_ROM_FN(kov2nl_302jp)

static struct BurnRomInfo kov2nl_301jpRomDesc[] = {
	KOV2NL_INTERNAL_JAPAN
	KOV2NL_PROGRAM_301(kov2nl, jp)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_301jp)
STD_ROM_FN(kov2nl_301jp)

static struct BurnRomInfo kov2nl_300jpRomDesc[] = {
	KOV2NL_INTERNAL_JAPAN
	KOV2NL_PROGRAM_300(kov2nl, jp)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_300jp)
STD_ROM_FN(kov2nl_300jp)

static struct BurnRomInfo kov2nl_302twRomDesc[] = {
	KOV2NL_INTERNAL_TAIWAN
	KOV2NL_PROGRAM_302(kov2nl, tw)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_302tw)
STD_ROM_FN(kov2nl_302tw)

static struct BurnRomInfo kov2nl_301twRomDesc[] = {
	KOV2NL_INTERNAL_TAIWAN
	KOV2NL_PROGRAM_301(kov2nl, tw)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_301tw)
STD_ROM_FN(kov2nl_301tw)

static struct BurnRomInfo kov2nl_300twRomDesc[] = {
	KOV2NL_INTERNAL_TAIWAN
	KOV2NL_PROGRAM_300(kov2nl, tw)
	KOV2NL_COMMON_ROMS
};
STD_ROM_PICK(kov2nl_300tw)
STD_ROM_FN(kov2nl_300tw)

// kov3 family
static struct BurnRomInfo kov3RomDesc[] = {
    { "kov3_igs036_china.rom", 0x0004000, 0xc7d33764, BRF_PRG | BRF_ESS },
    { "kov3_v104cn_raw.bin",   0x0800000, 0x1b5cbd24, BRF_PRG | BRF_ESS },
    { "kov3_text.u1",          0x0200000, 0x198b52d6, BRF_GRA },
    { "kov3_bgl.u6",           0x1000000, 0x49a4c5bc, BRF_GRA },
    { "kov3_bgh.u7",           0x1000000, 0xadc1aff1, BRF_GRA },
    { "kov3_mapl0.u15",        0x2000000, 0x9e569bf7, BRF_GRA },
    { "kov3_maph0.u16",        0x2000000, 0x6f200ad8, BRF_GRA },
    { "kov3_spa0.u17",         0x4000000, 0x3a1e58a9, BRF_GRA },
    { "kov3_spb0.u10",         0x4000000, 0x90396065, BRF_GRA },
    { "kov3_wave0.u13",        0x4000000, 0xaa639152, BRF_SND },
    { "blank_kov3_china_card.pg2", 0x0000108, 0xbd5a968f, BRF_OPT },
    { "kov3_sram",             0x0010000, 0xd9608102, BRF_OPT },
};
STD_ROM_PICK(kov3)
STD_ROM_FN(kov3)

static struct BurnRomInfo kov3_102RomDesc[] = {
    { "kov3_igs036_china.rom", 0x0004000, 0xc7d33764, BRF_PRG | BRF_ESS },
    { "kov3_v102cn_raw.bin",   0x0800000, 0x61d0dabd, BRF_PRG | BRF_ESS },
    { "kov3_text.u1",          0x0200000, 0x198b52d6, BRF_GRA },
    { "kov3_bgl.u6",           0x1000000, 0x49a4c5bc, BRF_GRA },
    { "kov3_bgh.u7",           0x1000000, 0xadc1aff1, BRF_GRA },
    { "kov3_mapl0.u15",        0x2000000, 0x9e569bf7, BRF_GRA },
    { "kov3_maph0.u16",        0x2000000, 0x6f200ad8, BRF_GRA },
    { "kov3_spa0.u17",         0x4000000, 0x3a1e58a9, BRF_GRA },
    { "kov3_spb0.u10",         0x4000000, 0x90396065, BRF_GRA },
    { "kov3_wave0.u13",        0x4000000, 0xaa639152, BRF_SND },
    { "blank_kov3_china_card.pg2", 0x0000108, 0xbd5a968f, BRF_OPT },
    { "kov3_sram",             0x0010000, 0xd9608102, BRF_OPT },
};
STD_ROM_PICK(kov3_102)
STD_ROM_FN(kov3_102)

static struct BurnRomInfo kov3_101RomDesc[] = {
    { "kov3_igs036_china.rom", 0x0004000, 0xc7d33764, BRF_PRG | BRF_ESS },
    { "kov3_v101.bin",         0x0800000, 0xd6664449, BRF_PRG | BRF_ESS },
    { "kov3_text.u1",          0x0200000, 0x198b52d6, BRF_GRA },
    { "kov3_bgl.u6",           0x1000000, 0x49a4c5bc, BRF_GRA },
    { "kov3_bgh.u7",           0x1000000, 0xadc1aff1, BRF_GRA },
    { "kov3_mapl0.u15",        0x2000000, 0x9e569bf7, BRF_GRA },
    { "kov3_maph0.u16",        0x2000000, 0x6f200ad8, BRF_GRA },
    { "kov3_spa0.u17",         0x4000000, 0x3a1e58a9, BRF_GRA },
    { "kov3_spb0.u10",         0x4000000, 0x90396065, BRF_GRA },
    { "kov3_wave0.u13",        0x4000000, 0xaa639152, BRF_SND },
    { "blank_kov3_china_card.pg2", 0x0000108, 0xbd5a968f, BRF_OPT },
    { "kov3_sram",             0x0010000, 0xd9608102, BRF_OPT },
};
STD_ROM_PICK(kov3_101)
STD_ROM_FN(kov3_101)

static struct BurnRomInfo kov3_100RomDesc[] = {
    { "kov3_igs036_china.rom", 0x0004000, 0xc7d33764, BRF_PRG | BRF_ESS },
    { "kov3_v100cn_raw.bin",   0x0800000, 0x93bca924, BRF_PRG | BRF_ESS },
    { "kov3_text.u1",          0x0200000, 0x198b52d6, BRF_GRA },
    { "kov3_bgl.u6",           0x1000000, 0x49a4c5bc, BRF_GRA },
    { "kov3_bgh.u7",           0x1000000, 0xadc1aff1, BRF_GRA },
    { "kov3_mapl0.u15",        0x2000000, 0x9e569bf7, BRF_GRA },
    { "kov3_maph0.u16",        0x2000000, 0x6f200ad8, BRF_GRA },
    { "kov3_spa0.u17",         0x4000000, 0x3a1e58a9, BRF_GRA },
    { "kov3_spb0.u10",         0x4000000, 0x90396065, BRF_GRA },
    { "kov3_wave0.u13",        0x4000000, 0xaa639152, BRF_SND },
    { "blank_kov3_china_card.pg2", 0x0000108, 0xbd5a968f, BRF_OPT },
    { "kov3_sram",             0x0010000, 0xd9608102, BRF_OPT },
};
STD_ROM_PICK(kov3_100)
STD_ROM_FN(kov3_100)

static struct BurnRomInfo ddpdojtRomDesc[] = {
    { "ddpdoj_igs036_china.rom", 0x0004000, 0x5db91464, BRF_PRG | BRF_ESS },
    { "ddpdoj_v201cn.u4",        0x0200000, 0x89e4b760, BRF_PRG | BRF_ESS },
    { "ddpdoj_text.u1",          0x0200000, 0xf18141d1, BRF_GRA },
    { "ddpdoj_bgl.u23",          0x1000000, 0xff65fdab, BRF_GRA },
    { "ddpdoj_bgh.u24",          0x1000000, 0xbb84d2a6, BRF_GRA },
    { "ddpdoj_mapl0.u13",        0x0800000, 0xbcfbb0fc, BRF_GRA },
    { "ddpdoj_maph0.u15",        0x0800000, 0x0cc75d4e, BRF_GRA },
    { "ddpdoj_spa0.u9",          0x1000000, 0x1232c1b4, BRF_GRA },
    { "ddpdoj_spb0.u18",         0x1000000, 0x6a9e2cbf, BRF_GRA },
    { "ddpdoj_wave0.u12",        0x1000000, 0x2b71a324, BRF_SND },
    { "ddpdojh_sram",            0x0010000, 0xaf99e304, BRF_OPT },
};
STD_ROM_PICK(ddpdojt)
STD_ROM_FN(ddpdojt)

static struct BurnRomInfo kof98umhRomDesc[] = {
    { "kof98umh_internal_rom.bin", 0x0004000, 0x3ed2e50f, BRF_PRG | BRF_ESS },
    { "kof98umh_v100cn.u4",        0x1000000, 0x2ea91e3b, BRF_PRG | BRF_ESS },
    { "ig-d3_text.u1",             0x0200000, 0x9a0ea82e, BRF_GRA },
    { "ig-d3_mapl0.u13",           0x4000000, 0x5571d63e, BRF_GRA },
    { "ig-d3_maph0.u15",           0x4000000, 0x0da7b1b8, BRF_GRA },
    { "ig-d3_spa0.u9",             0x4000000, 0xcfef8f7d, BRF_GRA },
    { "ig-d3_spb0.u18",            0x4000000, 0xf199d5c8, BRF_GRA },
    { "ig-d3_spa2.u10",            0x4000000, 0x03bfd35c, BRF_GRA },
    { "ig-d3_spb2.u20",            0x4000000, 0x9aaa840b, BRF_GRA },
    { "ig-d3_wave0.u12",           0x4000000, 0xedf2332d, BRF_SND },
    { "ig-d3_wave1.u11",           0x4000000, 0x62321b20, BRF_SND },
    { "kof98umh_sram",             0x0010000, 0x60460ed9, BRF_OPT },
};
STD_ROM_PICK(kof98umh)
STD_ROM_FN(kof98umh)

// BurnDriver declarations
struct BurnDriver BurnDrvorleg2 = {
    "orleg2", NULL, NULL, NULL, "2007",
    "Oriental Legend 2 (V104, Overseas)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 2, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2RomInfo, orleg2RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2InitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_103 = {
    "orleg2_103", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V103, Overseas)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_103RomInfo, orleg2_103RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2InitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_101 = {
    "orleg2_101", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V101, Overseas)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_101RomInfo, orleg2_101RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2InitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_104cn = {
    "orleg2_104cn", "orleg2", NULL, NULL, "2007",
    "Xiyou Shi E Zhuan 2 (V104, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_104cnRomInfo, orleg2_104cnRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_103cn = {
    "orleg2_103cn", "orleg2", NULL, NULL, "2007",
    "Xiyou Shi E Zhuan 2 (V103, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_103cnRomInfo, orleg2_103cnRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_101cn = {
    "orleg2_101cn", "orleg2", NULL, NULL, "2007",
    "Xiyou Shi E Zhuan 2 (V101, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_101cnRomInfo, orleg2_101cnRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_104hk = {
    "orleg2_104hk", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V104, Hong Kong)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_104hkRomInfo, orleg2_104hkRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_103hk = {
    "orleg2_103hk", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V103, Hong Kong)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_103hkRomInfo, orleg2_103hkRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_101hk = {
    "orleg2_101hk", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V101, Hong Kong)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_101hkRomInfo, orleg2_101hkRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_104jp = {
    "orleg2_104jp", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V104, Japan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_104jpRomInfo, orleg2_104jpRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2InitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_103jp = {
    "orleg2_103jp", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V103, Japan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_103jpRomInfo, orleg2_103jpRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2InitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_101jp = {
    "orleg2_101jp", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V101, Japan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_101jpRomInfo, orleg2_101jpRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2InitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_104tw = {
    "orleg2_104tw", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V104, Taiwan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_104twRomInfo, orleg2_104twRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_103tw = {
    "orleg2_103tw", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V103, Taiwan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_103twRomInfo, orleg2_103twRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvorleg2_101tw = {
    "orleg2_101tw", "orleg2", NULL, NULL, "2007",
    "Oriental Legend 2 (V101, Taiwan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, orleg2_101twRomInfo, orleg2_101twRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    orleg2CardInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl = {
    "kov2nl", NULL, NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V302, Overseas)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nlRomInfo, kov2nlRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_301 = {
    "kov2nl_301", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V301, Overseas)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_301RomInfo, kov2nl_301RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_300 = {
    "kov2nl_300", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V300, Overseas)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_300RomInfo, kov2nl_300RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_302cn = {
    "kov2nl_302cn", "kov2nl", NULL, NULL, "2008",
    "Sanguo Zhan Ji 2 Gaishi Yingxiong (V302, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_302cnRomInfo, kov2nl_302cnRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_301cn = {
    "kov2nl_301cn", "kov2nl", NULL, NULL, "2008",
    "Sanguo Zhan Ji 2 Gaishi Yingxiong (V301, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_301cnRomInfo, kov2nl_301cnRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_300cn = {
    "kov2nl_300cn", "kov2nl", NULL, NULL, "2008",
    "Sanguo Zhan Ji 2 Gaishi Yingxiong (V300, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_300cnRomInfo, kov2nl_300cnRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_302hk = {
    "kov2nl_302hk", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V302, Hong Kong)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_302hkRomInfo, kov2nl_302hkRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_301hk = {
    "kov2nl_301hk", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V301, Hong Kong)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_301hkRomInfo, kov2nl_301hkRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_300hk = {
    "kov2nl_300hk", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V300, Hong Kong)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_300hkRomInfo, kov2nl_300hkRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_302jp = {
    "kov2nl_302jp", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V302, Japan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_302jpRomInfo, kov2nl_302jpRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_301jp = {
    "kov2nl_301jp", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V301, Japan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_301jpRomInfo, kov2nl_301jpRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_300jp = {
    "kov2nl_300jp", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V300, Japan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_300jpRomInfo, kov2nl_300jpRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_302tw = {
    "kov2nl_302tw", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V302, Taiwan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_302twRomInfo, kov2nl_302twRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_301tw = {
    "kov2nl_301tw", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V301, Taiwan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_301twRomInfo, kov2nl_301twRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov2nl_300tw = {
    "kov2nl_300tw", "kov2nl", NULL, NULL, "2008",
    "Knights of Valour 2 New Legend (V300, Taiwan)\0", NULL,
    "IGS", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov2nl_300twRomInfo, kov2nl_300twRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov2nlInitCommon, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 448, 224, 4, 3
};

struct BurnDriver BurnDrvkov3 = {
    "kov3", NULL, NULL, NULL, "2011",
    "Knights of Valour 3 (V104, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov3RomInfo, kov3RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov3Init, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 512, 240, 4, 3
};

struct BurnDriver BurnDrvkov3_102 = {
    "kov3_102", "kov3", NULL, NULL, "2011",
    "Knights of Valour 3 (V102, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov3_102RomInfo, kov3_102RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov3_102Init, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 512, 240, 4, 3
};

struct BurnDriver BurnDrvkov3_101 = {
    "kov3_101", "kov3", NULL, NULL, "2011",
    "Knights of Valour 3 (V101, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov3_101RomInfo, kov3_101RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov3_101Init, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 512, 240, 4, 3
};

struct BurnDriver BurnDrvkov3_100 = {
    "kov3_100", "kov3", NULL, NULL, "2011",
    "Knights of Valour 3 (V100, China)\0", NULL,
    "IGS (Huatong license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_IGS_PGM2, GBF_SCRFIGHT, 0,
    NULL, kov3_100RomInfo, kov3_100RomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kov3_100Init, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 512, 240, 4, 3
};

struct BurnDriver BurnDrvddpdojt = {
    "ddpdojt", NULL, NULL, NULL, "2010",
    "DoDonPachi Dai-Ou-Jou Tamashii (V201, China)\0", NULL,
    "IGS / Cave (Tong Li Animation license)", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IGS_PGM2, GBF_VERSHOOT, FBF_DONPACHI,
    NULL, ddpdojtRomInfo, ddpdojtRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    ddpdojtInit, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 224, 448, 3, 4
};

struct BurnDriver BurnDrvkof98umh = {
    "kof98umh", NULL, NULL, NULL, "2009",
    "The King of Fighters '98: Ultimate Match HERO (China, V100)\0", NULL,
    "IGS / SNK Playmore / New Channel", "PGM2",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 2, HARDWARE_IGS_PGM2, GBF_VSFIGHT, FBF_KOF,
    NULL, kof98umhRomInfo, kof98umhRomName, NULL, NULL, NULL, NULL,
    pgm2InputInfo, pgm2DIPInfo,
    kof98umhInit, pgm2Exit, pgm2Frame, pgm2DoDraw, pgm2Scan,
    NULL, 0x4000, 320, 240, 4, 3
};
