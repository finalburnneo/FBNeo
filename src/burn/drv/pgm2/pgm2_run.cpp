// pgm2_run.cpp - PGM2 (IGS036) platform CPU / memory / I/O emulation
//
// Hardware reference (from MAME pgm2.cpp):
//   CPU:   ARM946E-S (IGS036) @ ~100 MHz
//   Video: IGS037 sprite/tilemap chip
//   Sound: YMZ774-S (embedded)
//
// Memory map (verified from MAME pgm2_state::pgm2_map):
//   0x00000000-0x00003FFF  Internal boot ROM (16 KB)
//   0x02000000-0x0200FFFF  Battery-backed SRAM (64 KB)
//   0x10000000-0x10FFFFFF  External Flash ROM (up to 16 MB)
//   0x20000000-0x2007FFFF  Main RAM (512 KB)
//   0x30000000-0x30001FFF  Sprite OAM / move RAM (8 KB, matches MAME pgm2_map)
//   0x30020000-0x30021FFF  BG videoram (8 KB, matches MAME pgm2_map)
//   0x30040000-0x30045FFF  FG (text) videoram (24 KB)
//   0x30060000-0x30063FFF  Sprite palette (16 KB, 32-bit xRGB888)
//   0x30080000-0x30081FFF  BG palette (8 KB, 32-bit xRGB888)
//   0x300A0000-0x300A07FF  TX/FG palette (2 KB, 32-bit xRGB888)
//   0x300C0000-0x300C01FF  Sprite zoom table (512 B)
//   0x300E0000-0x300E03FF  Line scroll RAM
//   0x30100000-0x301000FF  Shared RAM (MCU)
//   0x30120000-0x3012003F  Video registers (bgscroll, fgscroll, vidmode…)
//   0x40000000-0x40000003  YMZ774 I/O
//   0x03900000-0x03900003  INPUTS0 (32-bit, 4 players + coins)
//   0x03A00000-0x03A00003  INPUTS1 (32-bit, starts + service)
//
// DIP / IRQ:
//   VBLANK → IRQ 12 via ARM AIC (simulated as ARM7_IRQ_LINE)

#include "pgm2.h"
#include "pgm2_crypt.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

// ---------------------------------------------------------------------------
// PGM2 Logging System
// ---------------------------------------------------------------------------
#define PGM2_LOG_ENABLED 0

// Unified category logging macro
#if PGM2_LOG_ENABLED
    #define PGM2_LOG(category, format, ...) bprintf(PRINT_NORMAL, _T("[PGM2][%s] " format _T("\n")), _T(category), ##__VA_ARGS__)
#else
    #define PGM2_LOG(category, format, ...) ((void)0)
#endif

// Logging categories
#define PGM2_LOG_SYS     "SYS"
#define PGM2_LOG_CARD    "CARD"
#define PGM2_LOG_BOOT    "BOOT"
#define PGM2_LOG_AIC     "AIC"
#define PGM2_LOG_MCU     "MCU"
#define PGM2_LOG_PIO     "PIO"
#define PGM2_LOG_MODULE  "MODULE"
#define PGM2_LOG_FRAME   "FRAME"
#define PGM2_LOG_DIAG    "DIAG"

// ---------------------------------------------------------------------------
// Public memory areas
// ---------------------------------------------------------------------------
UINT8 *Pgm2IntROM        = NULL;
INT32  Pgm2IntROMLen     = 0;
UINT8 *Pgm2ArmROM        = NULL;
UINT8 *Pgm2ArmRAM        = NULL;
UINT8 *Pgm2SprROM        = NULL;
UINT8 *Pgm2TileROM       = NULL;
UINT8 *Pgm2SndROM        = NULL;
UINT8 *Pgm2PalRAM        = NULL;
UINT8 *Pgm2VidRAM        = NULL;
UINT8 *Pgm2SharedRAM     = NULL;
UINT8 *Pgm2ExtRAM        = NULL;
UINT8 *Pgm2Cards[4]      = {NULL, NULL, NULL, NULL};

INT32  Pgm2ArmROMLen  = 0;
INT32  Pgm2BgROMLen   = 0;
INT32  Pgm2SprROMLen  = 0;
INT32  Pgm2TileROMLen = 0;
INT32  Pgm2SndROMLen  = 0;

UINT8  Pgm2Input[8]       = { 0 };
UINT8  Pgm2InputPort0[32] = { 0 };
UINT8  Pgm2InputPort1[32] = { 0 };
UINT8  Pgm2Dip[1]         = { 0xff };
UINT8  Pgm2Reset           = 0;

// Encryption support (runtime decryption via internal boot ROM)
static UINT8 *Pgm2ArmROMEncrypted = NULL;     // encrypted backup for reset
static INT32  Pgm2HasDecrypted = 0;            // prevents double decryption
static UINT8  Pgm2EncryptTable[0x100] = {0};  // captured from writes to 0xFFFFFC00
static INT32  Pgm2RtcBase = 0;
static INT32  Pgm2EncWriteCount = 0;
static INT32  Pgm2VideoRegLogCount = 0;
static INT32  Pgm2SharedLogCount = 0;
static INT32  Pgm2McuReadLogCount = 0;
static INT32  Pgm2PostDecryptFrameCount = 0;
INT32  Pgm2RtcFrameCounter = 0;
static UINT32 Pgm2IntRomOriginal3C44 = 0;     // original instruction at 0x3C44 before NOP patch
static UINT32 Pgm2IntRomOriginal27F0 = 0;     // original instruction at 0x27F0 before delay patch
static UINT32 Pgm2IntRomOriginal27FC = 0;     // original instruction at 0x27FC before delay patch
static INT32  Pgm2IntRomNeedRestore = 0;       // 1 = need to restore all patches after boot passes them
static INT32  Pgm2ArmROMFileLen = 0;          // actual ROM file size (e.g. 0x800000)
static INT32  Pgm2CardRomIndex = -1;
static INT32  Pgm2SramRomIndex = 10;
static INT32  Pgm2PerSlotCardIndex[4] = {-1, -1, -1, -1};
static INT32  Pgm2CardLogCount = 0;

static const INT32 PGM2_NUM_CARD_SLOTS = 4;
static const INT32 PGM2_CARD_SIZE = 0x108;
static const INT32 PGM2_CARD_DATA_SIZE = 0x100;
static bool Pgm2CardAuthenticated[4] = {false, false, false, false};
INT32  Pgm2MaxCardSlots = 0;
INT32  Pgm2ActiveCardSlot = 0;
bool   Pgm2CardInserted[4] = {false, false, false, false};

// Speed hack variables
static UINT32 Pgm2SpeedHackAddr = 0;
static UINT32 Pgm2SpeedHackPC[4] = {0};

// RAM/ROM board (ddpdojt) — 2MB of writable RAM at 0x10000000, ROM at 0x10200000
static UINT8 *Pgm2RomBoardRAM = NULL;
static INT32  Pgm2RomBoardRAMSize = 0;
static INT32  Pgm2DecryptWordOffset = 0;

// Sprite layout offsets (set by ROM loader, used by pgm2_draw.cpp)
INT32  Pgm2MaskROMOffset  = 0;   // byte offset in SprROM where mask data starts
INT32  Pgm2MaskROMLen     = 0;   // total mask data bytes
INT32  Pgm2ColourROMOffset = 0;  // byte offset in SprROM where colour data starts

// KOV3 module support
static INT32  Pgm2HasKov3Module = 0;
static INT32  Pgm2HasDecryptedKov3Module = 0;
static const UINT8 *Pgm2ModuleKey = NULL;
static const UINT8 *Pgm2ModuleSum = NULL;
static UINT32 Pgm2ModuleAddrXor = 0;
static UINT16 Pgm2ModuleDataXor = 0;
static INT32  Pgm2ModuleClkCnt = 0;
static INT32  Pgm2ModulePrevState = 0;
static INT32  Pgm2ModuleInLatch = 0;
static INT32  Pgm2ModuleOutLatch = 0;
static UINT8  Pgm2ModuleRcvBuf[10] = {0};
static UINT8  Pgm2ModuleSendBuf[10] = {0};
static INT32  Pgm2ModuleSumRead = 0;
static UINT32 Pgm2PioOutData = 0;


// Per-game refresh rate (default 59.08 Hz; kof98umh uses 59.19 Hz per MAME pgm2_lores)
static double Pgm2RefreshRate = 59.08;

// Callbacks (set by game-specific driver)
void (*pPgm2InitCallback)()                          = NULL;
void (*pPgm2ResetCallback)()                         = NULL;
INT32 (*pPgm2ScanCallback)(INT32 nAction, INT32 *)   = NULL;

void pgm2SetStorageRomIndices(INT32 cardRomIndex, INT32 sramRomIndex)
{
    Pgm2CardRomIndex = cardRomIndex;
    Pgm2SramRomIndex = sramRomIndex;
}

void pgm2SetCardRomIndex(INT32 slot, INT32 index)
{
    if (slot >= 0 && slot < 4) Pgm2PerSlotCardIndex[slot] = index;
}

void pgm2SetMaxCardSlots(INT32 count)
{
    Pgm2MaxCardSlots = count;
}

INT32 pgm2GetCardRomTemplate(UINT8* buffer, INT32 maxSize)
{
    if (Pgm2CardRomIndex < 0 || !buffer || maxSize < PGM2_CARD_SIZE) return 0;
    struct BurnRomInfo ri;
    memset(&ri, 0, sizeof(ri));
    if (BurnDrvGetRomInfo(&ri, Pgm2CardRomIndex) != 0 || ri.nLen <= 0) return 0;
    memset(buffer, 0xFF, maxSize);
    BurnLoadRom(buffer, Pgm2CardRomIndex, 1);
    // Ensure security area defaults for short ROMs
    if ((INT32)ri.nLen <= PGM2_CARD_DATA_SIZE) {
        buffer[PGM2_CARD_DATA_SIZE + 4] = 0x07;  // sec[0] = 3 auth attempts
    }
    return PGM2_CARD_SIZE;
}

void pgm2SetSpeedhack(UINT32 addr, UINT32 pc1, UINT32 pc2, UINT32 pc3, UINT32 pc4)
{
    Pgm2SpeedHackAddr = addr & ~3;
    Pgm2SpeedHackPC[0] = pc1;
    Pgm2SpeedHackPC[1] = pc2;
    Pgm2SpeedHackPC[2] = pc3;
    Pgm2SpeedHackPC[3] = pc4;
}

void pgm2EnableKov3Module(const UINT8 *key, const UINT8 *sum, UINT32 addrXor, UINT16 dataXor)
{
    Pgm2HasKov3Module = 1;
    Pgm2ModuleKey = key;
    Pgm2ModuleSum = sum;
    Pgm2ModuleAddrXor = addrXor;
    Pgm2ModuleDataXor = dataXor;
}

void pgm2DisableKov3Module()
{
    Pgm2HasKov3Module = 0;
    Pgm2ModuleKey = NULL;
    Pgm2ModuleSum = NULL;
    Pgm2ModuleAddrXor = 0;
    Pgm2ModuleDataXor = 0;
}

void pgm2SetRamRomBoard(INT32 ramSize)
{
    Pgm2RomBoardRAMSize = ramSize;
    // ROM starts at word offset ramSize/2 within the combined program space.
    // MAME: decrypter_rom(mainrom, mainrom_bytes, romboard_ram.bytes())
    Pgm2DecryptWordOffset = ramSize / 2;
}

void pgm2SetRefreshRate(double hz)
{
    Pgm2RefreshRate = hz;
}

// ---------------------------------------------------------------------------
// KOV3 module (FPGA serial + special ROM read/write)
// ---------------------------------------------------------------------------
static void pgm2DecryptKov3Module(UINT32 addrXor, UINT16 dataXor)
{
    if (!Pgm2ArmROM || Pgm2ArmROMLen <= 0) return;
    UINT16 *rom = (UINT16*)Pgm2ArmROM;
    INT32 nWords = Pgm2ArmROMLen / 2;

    // MAME algorithm: address scramble + unconditional data XOR
    // buffer[i] = src[i ^ addrxor] ^ dataxor; memcpy back
    UINT16 *buffer = (UINT16*)BurnMalloc(Pgm2ArmROMLen);
    if (!buffer) return;

    for (INT32 i = 0; i < nWords; i++)
        buffer[i] = rom[i ^ addrXor] ^ dataXor;

    memcpy(rom, buffer, Pgm2ArmROMLen);
    BurnFree(buffer);

    Pgm2HasDecryptedKov3Module = 1;
    PGM2_LOG(PGM2_LOG_MODULE, "kov3 module decrypted addrXor=%06X dataXor=%04X", addrXor, dataXor);
}

static void pgm2ModuleDataW(INT32 state)
{
    Pgm2ModuleInLatch = state ? 1 : 0;
}

static void pgm2ModuleClkW(INT32 state)
{
    if (Pgm2ModulePrevState != state && !state) {
        if (Pgm2ModuleClkCnt < 80) {
            INT32 offs = Pgm2ModuleClkCnt / 8;
            INT32 bit = (Pgm2ModuleClkCnt & 7) ^ 7;
            Pgm2ModuleRcvBuf[offs] &= ~(1 << bit);
            Pgm2ModuleRcvBuf[offs] |= (UINT8)(Pgm2ModuleInLatch << bit);
            Pgm2ModuleClkCnt++;
            if (Pgm2ModuleClkCnt >= 80 && Pgm2ModuleKey) {
                switch (Pgm2ModuleRcvBuf[0]) {
                case 0x0d: // init/status check
                    Pgm2ModuleSendBuf[0] = Pgm2ModuleSendBuf[1] = Pgm2ModuleSendBuf[2] = Pgm2ModuleSendBuf[3] = 0xa3;
                    Pgm2ModuleSendBuf[4] = Pgm2ModuleSendBuf[5] = Pgm2ModuleSendBuf[6] = Pgm2ModuleSendBuf[7] = 0x6d;
                    PGM2_LOG(PGM2_LOG_MODULE, "FPGA command 0x0d (init) -> A3A3A3A36D6D6D6D");
                    break;
                case 0x25: // get key
                    for (INT32 i = 0; i < 8; i++)
                        Pgm2ModuleSendBuf[i] = Pgm2ModuleKey[i ^ 3] ^ Pgm2ModuleRcvBuf[i + 1];
                    PGM2_LOG(PGM2_LOG_MODULE, "FPGA command 0x25 (get key) -> %02X %02X %02X %02X %02X %02X %02X %02X",
                        Pgm2ModuleSendBuf[0], Pgm2ModuleSendBuf[1], Pgm2ModuleSendBuf[2], Pgm2ModuleSendBuf[3],
                        Pgm2ModuleSendBuf[4], Pgm2ModuleSendBuf[5], Pgm2ModuleSendBuf[6], Pgm2ModuleSendBuf[7]);
                    break;
                default:
                    PGM2_LOG(PGM2_LOG_MODULE, "unknown FPGA command %02X", Pgm2ModuleRcvBuf[0]);
                    break;
                }
                Pgm2ModuleSendBuf[8] = 0;
                for (INT32 i = 0; i < 8; i++)
                    Pgm2ModuleSendBuf[8] += Pgm2ModuleSendBuf[i];
            }
        } else {
            INT32 offs = (Pgm2ModuleClkCnt - 80) / 8;
            INT32 bit = (Pgm2ModuleClkCnt & 7) ^ 7;
            Pgm2ModuleOutLatch = (Pgm2ModuleSendBuf[offs] >> bit) & 1;
            Pgm2ModuleClkCnt++;
            if (Pgm2ModuleClkCnt >= 152)
                Pgm2ModuleClkCnt = 0;
        }
    }
    Pgm2ModulePrevState = state;
}

static UINT16 pgm2ModuleRomR(UINT32 offset)
{
    if (Pgm2ModuleSumRead && offset > 0 && offset < 5) {
        if (offset == 4) Pgm2ModuleSumRead = 0;
        UINT32 offs = ((offset - 1) * 2) ^ 2;
        return (UINT16)((Pgm2ModuleSum[offs] ^ Pgm2ModuleKey[offs]) |
                        ((Pgm2ModuleSum[offs + 1] ^ Pgm2ModuleKey[offs + 1]) << 8));
    }
    if (!Pgm2ArmROM) return 0;
    return ((UINT16*)Pgm2ArmROM)[offset];
}

static void pgm2ModuleRomW(UINT32 offset, UINT16 data)
{
    if (!Pgm2ModuleKey) return;
    UINT16 dec_val = (UINT16)(((Pgm2ModuleKey[0] | (Pgm2ModuleKey[1] << 8) | (Pgm2ModuleKey[2] << 16)) >> 6) & 0xffff);
    if (data == dec_val) {
        pgm2DecryptKov3Module(Pgm2ModuleAddrXor, Pgm2ModuleDataXor);
    } else {
        switch (data) {
        case 0x00c2:
            Pgm2ModuleSumRead = 1;
            break;
        case 0x0084:
            break;
        default:
            break;
        }
    }
}

static inline bool pgm2CardPresent(UINT8 slot)
{
    return (slot & 3) < PGM2_NUM_CARD_SLOTS && Pgm2Cards[slot & 3] != NULL && Pgm2CardInserted[slot & 3];
}

static inline UINT8 pgm2CardReadData(UINT8 slot, UINT32 offset)
{
    UINT8 s = slot & 3;
    if (!Pgm2Cards[s] || offset >= PGM2_CARD_DATA_SIZE) return 0xff;
    return Pgm2Cards[s][offset];
}

static inline void pgm2CardWriteData(UINT8 slot, UINT32 offset, UINT8 data)
{
    UINT8 s = slot & 3;
    if (!Pgm2Cards[s] || offset >= PGM2_CARD_DATA_SIZE) return;
    if (!Pgm2CardAuthenticated[s]) return;
    if (offset < 0x20) {
        UINT8 protByte = Pgm2Cards[s][PGM2_CARD_DATA_SIZE + (offset >> 3)];
        if (!(protByte & (1 << (offset & 7)))) return;
    }
    Pgm2Cards[s][offset] = data;
}

static inline UINT8 pgm2CardReadProt(UINT8 slot, UINT32 offset)
{
    UINT8 s = slot & 3;
    if (!Pgm2Cards[s] || offset >= 4) return 0xff;
    return Pgm2Cards[s][PGM2_CARD_DATA_SIZE + offset];
}

static inline void pgm2CardWriteProt(UINT8 slot, UINT32 offset, UINT8 data)
{
    UINT8 s = slot & 3;
    if (!Pgm2Cards[s] || offset >= 4) return;
    if (!Pgm2CardAuthenticated[s]) return;
    Pgm2Cards[s][PGM2_CARD_DATA_SIZE + offset] &= data;
}

static inline UINT8 pgm2CardReadSec(UINT8 slot, UINT32 offset)
{
    UINT8 s = slot & 3;
    if (!Pgm2Cards[s] || offset >= 4) return 0xff;
    if (!Pgm2CardAuthenticated[s]) return 0xff;
    return Pgm2Cards[s][PGM2_CARD_DATA_SIZE + 4 + offset];
}

static inline void pgm2CardWriteSec(UINT8 slot, UINT32 offset, UINT8 data)
{
    UINT8 s = slot & 3;
    if (!Pgm2Cards[s] || offset >= 4) return;
    if (!Pgm2CardAuthenticated[s]) return;
    Pgm2Cards[s][PGM2_CARD_DATA_SIZE + 4 + offset] = data;
}

static void pgm2CardAuth(UINT8 slot, UINT8 p1, UINT8 p2, UINT8 p3)
{
    UINT8 s = slot & 3;
    if (!Pgm2Cards[s]) return;
    UINT8 *sec = Pgm2Cards[s] + PGM2_CARD_DATA_SIZE + 4;
    if (sec[0] & 7) {
        if (sec[1] == p1 && sec[2] == p2 && sec[3] == p3) {
            Pgm2CardAuthenticated[s] = true;
            sec[0] = 7;
        } else {
            Pgm2CardAuthenticated[s] = false;
            sec[0] >>= 1;
        }
    }
}

static void pgm2DoDecrypt(const char* source)
{
    if (Pgm2HasDecrypted) return;  // prevent double decryption (MAME: m_has_decrypted)

    // Log full 256-byte encryption key for debugging
    PGM2_LOG(PGM2_LOG_SYS, "decrypt trigger %s romLen=0x%X fileLen=0x%X", source, Pgm2ArmROMLen, Pgm2ArmROMFileLen);
    for (int row = 0; row < 256; row += 16) {
        PGM2_LOG(PGM2_LOG_SYS, "  key[%02X]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            row,
            Pgm2EncryptTable[row+0],  Pgm2EncryptTable[row+1],  Pgm2EncryptTable[row+2],  Pgm2EncryptTable[row+3],
            Pgm2EncryptTable[row+4],  Pgm2EncryptTable[row+5],  Pgm2EncryptTable[row+6],  Pgm2EncryptTable[row+7],
            Pgm2EncryptTable[row+8],  Pgm2EncryptTable[row+9],  Pgm2EncryptTable[row+10], Pgm2EncryptTable[row+11],
            Pgm2EncryptTable[row+12], Pgm2EncryptTable[row+13], Pgm2EncryptTable[row+14], Pgm2EncryptTable[row+15]);
    }

    // MAME encryption_do_w: decrypt ROM (and ROM-board RAM if present).
    // For RAM/ROM board (ddpdojt), the MCU copies encrypted data from ROM
    // (0x10200000) to RAM (0x10000000) before triggering decrypt, so both
    // buffers need decrypting.  MAME decrypts RAM at offset 0 and ROM at
    // offset = ramSize/2 (word index within the combined program space).
    if (Pgm2RomBoardRAM && Pgm2RomBoardRAMSize > 0) {
        PGM2_LOG(PGM2_LOG_SYS, "decrypting RAM board %d bytes (0x%X) offset=0", Pgm2RomBoardRAMSize, Pgm2RomBoardRAMSize);
        pgm2_igs036_decrypt(Pgm2RomBoardRAM, Pgm2RomBoardRAMSize, Pgm2EncryptTable, 0);
    }
    if (Pgm2ArmROM && Pgm2ArmROMLen > 0) {
        // Decrypt only the actual ROM file size, matching MAME's m_mainrom.bytes()
        INT32 decryptLen = (Pgm2ArmROMFileLen > 0) ? Pgm2ArmROMFileLen : Pgm2ArmROMLen;
        PGM2_LOG(PGM2_LOG_SYS, "decrypting ROM %d bytes (0x%X) offset=%d", decryptLen, decryptLen, Pgm2DecryptWordOffset);
        pgm2_igs036_decrypt(Pgm2ArmROM, decryptLen, Pgm2EncryptTable, Pgm2DecryptWordOffset);
    }

    Pgm2PostDecryptFrameCount = 0;
    Pgm2VideoRegLogCount = 0;
    Pgm2SharedLogCount = 0;
    Pgm2McuReadLogCount = 0;
    Pgm2CardLogCount = 0;

    // Boot ROM verification bypass — dynamic pattern scanner.
    //
    // The internal boot ROM has a sequence: BL<verify>, BL<init>, B<continue>
    // at game-specific addresses. The verify function loops forever without
    // full CP15 MPU emulation. We scan the ROM for this BL,BL,B pattern
    // and NOP only the first BL (verify), keeping BL<init> (AIC setup) and
    // B<continue> (mode/stack setup, jump to external ROM) intact.
    //
    // orleg2 example: 0x3C44=BL(verify), 0x3C48=BL(init), 0x3C4C=B(continue)
    // kov2nl etc.: different addresses (0x3C44 area is zeros in their ROMs)
    if (0 && Pgm2IntROM && Pgm2IntROMLen >= 0x100) {
        INT32 patchAddr = -1;
        // Scan the ROM for pattern: BL(xxx), BL(xxx), B(xxx) — three consecutive words
        // BL encoding: 0xEB______ (condition AL=0xE, opcode=101, L=1)
        // B  encoding: 0xEA______ (condition AL=0xE, opcode=101, L=0)
        for (INT32 off = 0x20; off < Pgm2IntROMLen - 12; off += 4) {
            UINT32 w0 = *(UINT32*)(Pgm2IntROM + off);
            UINT32 w1 = *(UINT32*)(Pgm2IntROM + off + 4);
            UINT32 w2 = *(UINT32*)(Pgm2IntROM + off + 8);
            if ((w0 & 0xFF000000) == 0xEB000000 &&  // BL (verify)
                (w1 & 0xFF000000) == 0xEB000000 &&  // BL (init)
                (w2 & 0xFF000000) == 0xEA000000) {  // B  (continue)
                patchAddr = off;
                break;
            }
        }
        {
            if (patchAddr >= 0) {
                Pgm2IntRomOriginal3C44 = *(UINT32*)(Pgm2IntROM + patchAddr);
                *(UINT32*)(Pgm2IntROM + patchAddr) = 0xE1A00000; // NOP verify BL
                PGM2_LOG(PGM2_LOG_BOOT, "Patched verify BL at 0x%04X (was %08X) -> NOP", patchAddr, Pgm2IntRomOriginal3C44);
                PGM2_LOG(PGM2_LOG_BOOT, "  init BL at 0x%04X: %08X", patchAddr + 4, *(UINT32*)(Pgm2IntROM + patchAddr + 4));
                PGM2_LOG(PGM2_LOG_BOOT, "  cont B  at 0x%04X: %08X", patchAddr + 8, *(UINT32*)(Pgm2IntROM + patchAddr + 8));
            } else {
                PGM2_LOG(PGM2_LOG_BOOT, "No BL,BL,B pattern found in internal ROM — no verify patch applied");
            }
            // Dump exception vectors
            PGM2_LOG(PGM2_LOG_BOOT, "Exception vectors:");
            for (int i = 0; i < 0x20; i += 4) {
                UINT32 instr = *(UINT32*)(Pgm2IntROM + i);
                PGM2_LOG(PGM2_LOG_BOOT, "  0x%04X: %08X", i, instr);
            }
            // Dump code around 0xE0-0xFF (potential reset/abort handler)
            PGM2_LOG(PGM2_LOG_BOOT, "IntROM 0xE0-0xFF:");
            for (int i = 0xE0; i < 0x100 && i < Pgm2IntROMLen; i += 4) {
                UINT32 instr = *(UINT32*)(Pgm2IntROM + i);
                PGM2_LOG(PGM2_LOG_BOOT, "  0x%04X: %08X", i, instr);
            }
        }
    }

    Pgm2HasDecrypted = 1;
}

// ---------------------------------------------------------------------------
// Video RAM layout (sub-regions within Pgm2VidRAM 0x130000 bytes total)
// Offsets are relative to Pgm2VidRAM base pointer.
// ---------------------------------------------------------------------------
// 0x00000 - 0x01FFF  Sprite OAM    (0x30000000 - 0x30001FFF, 8KB matches MAME)
// 0x20000 - 0x21FFF  BG videoram  (0x30020000 - 0x30021FFF, 8KB matches MAME)
// 0x40000 - 0x45FFF  FG videoram (0x30040000 - 0x30045FFF)
// 0x60000 - 0x63FFF  Sprite palette 32-bit xRGB888 (0x30060000)
// 0x80000 - 0x81FFF  BG palette   (0x30080000)
// 0xA0000 - 0xA07FF  TX palette   (0x300A0000)
// 0xC0000 - 0xC01FF  Sprite zoom table (0x300C0000)
// 0xE0000 - 0xE03FF  Line RAM     (0x300E0000)
// 0x100000- 0x1000FF Shared RAM   (0x30100000)
// 0x120000- 0x12003F Video regs   (0x30120000)
#define PGM2_VIDRAM_SIZE   0x130000

// Handy sub-pointers (set after allocation)
UINT8  *Pgm2SpOAM      = NULL;  // sprite OAM at vidram+0x00000
UINT8  *Pgm2BgVRAM     = NULL;  // BG  vidram+0x20000
UINT8  *Pgm2FgVRAM     = NULL;  // FG  vidram+0x40000
UINT32 *Pgm2SpPal      = NULL;  // sprite palette vidram+0x60000
UINT32 *Pgm2BgPal      = NULL;  // bg palette     vidram+0x80000
UINT32 *Pgm2TxPal      = NULL;  // tx palette     vidram+0xA0000
UINT32 *Pgm2SpZoom     = NULL;  // sprite zoom    vidram+0xC0000
UINT8  *Pgm2LineRAM    = NULL;  // line scroll    vidram+0xE0000
UINT8  *Pgm2SharedRAM2 = NULL;  // MCU shared     vidram+0x100000
UINT32 *Pgm2VideoRegs  = NULL;  // video regs     vidram+0x120000
// bgscroll  = VideoRegs[0] (0x30120000)
// fgscroll  = VideoRegs[2] (0x30120008)
// vidmode   = VideoRegs[3] (0x3012000C)

// Minimal MCU HLE state (mapped at 0x03600000-0x036bffff).
static UINT32 Pgm2McuRegs[8] = { 0 };
static UINT32 Pgm2McuResult0 = 0;
static UINT32 Pgm2McuResult1 = 0;
static UINT8  Pgm2McuLastCmd = 0;
static UINT8  Pgm2ShareBank  = 0;
static UINT8  Pgm2McuIrq3    = 0;
static INT32  Pgm2McuDoneCountdown = 0;

// ---------------------------------------------------------------------------
// ARM AIC (Advanced Interrupt Controller) — AT91 compatible
// Aligned with MAME's atmel_arm_aic device implementation.
// Mapped at 0xFFFFF000-0xFFFFF14B (31-bit: 0x7FFFF000-0x7FFFF14B)
// ---------------------------------------------------------------------------
static UINT32 Pgm2AicSmr[32]   = {0};   // Source Mode Registers   (0x00-0x7C)
static UINT32 Pgm2AicSvr[32]   = {0};   // Source Vector Registers (0x80-0xFC)
static UINT32 Pgm2AicPending   = 0;     // Pending interrupt bitmask
static UINT32 Pgm2AicEnabled   = 0;     // Enabled interrupt bitmask
static UINT32 Pgm2AicSpuIvr    = 0;     // Spurious Interrupt Vector
static UINT32 Pgm2AicFastIrqs  = 1;     // Fast-forcing (FIQ) bitmask (source 0 default)
static UINT32 Pgm2AicDebug     = 0;     // Debug Control Register
static INT32  Pgm2AicStatus    = 0;     // AIC_ISR: source number of current IRQ
static UINT32 Pgm2AicCoreStatus = 0;    // AIC_CISR: bit1=nIRQ, bit0=nFIQ
static INT32  Pgm2AicLevelStack[9] = {-1,0,0,0,0,0,0,0,0}; // Priority nesting stack (sentinel=-1, matches MAME)
static INT32  Pgm2AicLvlIdx    = 0;     // Current stack index

// Get current interrupt priority level from stack
static inline INT32 pgm2AicGetLevel()
{
    return Pgm2AicLevelStack[Pgm2AicLvlIdx];
}

// Push a new priority level onto the stack
static inline void pgm2AicPushLevel(INT32 level)
{
    if (Pgm2AicLvlIdx < 8) {
        Pgm2AicLvlIdx++;
        Pgm2AicLevelStack[Pgm2AicLvlIdx] = level;
    }
}

// Pop the top priority level from the stack
static inline void pgm2AicPopLevel()
{
    if (Pgm2AicLvlIdx > 0)
        Pgm2AicLvlIdx--;
}

// Count leading zeros (for finding highest-numbered set bit)
static inline INT32 pgm2Clz32(UINT32 x)
{
    if (x == 0) return 32;
    INT32 n = 0;
    if ((x & 0xFFFF0000) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF000000) == 0) { n +=  8; x <<=  8; }
    if ((x & 0xF0000000) == 0) { n +=  4; x <<=  4; }
    if ((x & 0xC0000000) == 0) { n +=  2; x <<=  2; }
    if ((x & 0x80000000) == 0) { n +=  1; }
    return n;
}

// Re-evaluate whether the CPU IRQ/FIQ lines should be asserted.
// Matches MAME's arm_aic_device::check_irqs() / set_lines().
static void pgm2AicCheckIrqs()
{
    Pgm2AicCoreStatus = 0;

    // Normal IRQ: only assert if there's a pending interrupt with priority
    // strictly greater than the current nesting level (matches MAME).
    UINT32 mask = Pgm2AicPending & Pgm2AicEnabled & ~Pgm2AicFastIrqs;
    if (mask) {
        INT32 pri = pgm2AicGetLevel();
        UINT32 tmp = mask;
        while (tmp) {
            INT32 idx = 31 - pgm2Clz32(tmp);
            if ((INT32)(Pgm2AicSmr[idx] & 7) > pri) {
                Pgm2AicCoreStatus |= 2;
                break;
            }
            tmp ^= (1u << idx);
        }
    }

    // FIQ check (not used by PGM2, but kept for completeness)
    if (Pgm2AicPending & Pgm2AicEnabled & Pgm2AicFastIrqs)
        Pgm2AicCoreStatus |= 1;

    // Assert/deassert the CPU IRQ line based on core status
    if (Pgm2AicCoreStatus & 2) {
        Arm9SetIRQLine(ARM7_IRQ_LINE, CPU_IRQSTATUS_ACK);
    } else {
        Arm9SetIRQLine(ARM7_IRQ_LINE, CPU_IRQSTATUS_NONE);
    }
}

static void pgm2AicSetIrq(int source, int state)
{
    if (source < 0 || source > 31) return;

    // Debug: trace all IRQ3 transitions (MCU interrupt)
    if (source == 3) {
        static INT32 irq3TraceCount = 0;
        if (irq3TraceCount < 200) {
            UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
            PGM2_LOG(PGM2_LOG_SYS, "IRQ3 %s pending=%08X enabled=%08X coreStatus=%X pc=%08X mcuStatus=%08X countdown=%d",
                state ? "ASSERT" : "CLEAR", Pgm2AicPending, Pgm2AicEnabled, Pgm2AicCoreStatus, pc,
                Pgm2McuRegs[3], Pgm2McuDoneCountdown);
            irq3TraceCount++;
        }
    }

    if (state) {
        Pgm2AicPending |= (1u << source);
    } else {
        // Level-triggered sources: clear pending on CLEAR_LINE
        // (MAME: only clears if SMR bit 6 set, i.e. level-sensitive)
        if (Pgm2AicSmr[source] & 0x40)
            Pgm2AicPending &= ~(1u << source);
    }
    pgm2AicCheckIrqs();
}

static UINT32 pgm2AicRead(UINT32 offset)
{
    if (offset < 0x80) return Pgm2AicSmr[offset / 4];
    if (offset < 0x100) return Pgm2AicSvr[(offset - 0x80) / 4];

    switch (offset) {
        case 0x100: // AIC_IVR — Interrupt Vector Register (has side effects!)
        {
            UINT32 mask = Pgm2AicPending & Pgm2AicEnabled & ~Pgm2AicFastIrqs;
            UINT32 result = Pgm2AicSpuIvr;
            INT32 midx = -1;
            if (mask) {
                // Find highest-priority pending interrupt (>= current level)
                INT32 pri = pgm2AicGetLevel();
                UINT32 tmp = mask;
                while (tmp) {
                    INT32 idx = 31 - pgm2Clz32(tmp);
                    if ((INT32)(Pgm2AicSmr[idx] & 7) >= pri) {
                        midx = idx;
                        pri = Pgm2AicSmr[idx] & 7;
                    }
                    tmp ^= (1u << idx);
                }
                if (midx >= 0) {
                    Pgm2AicStatus = midx;
                    result = Pgm2AicSvr[midx];
                    // Push current priority level (nesting)
                    pgm2AicPushLevel(Pgm2AicSmr[midx] & 7);
                    // Edge-triggered: auto-clear pending on IVR read
                    if (Pgm2AicSmr[midx] & 0x20)
                        Pgm2AicPending ^= (1u << midx);
                }
            }
            // IVR trace
            {
                static INT32 ivrLogCount = 0;
                if (ivrLogCount < 60) {
                    UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
                    UINT32 cpsr = Arm9DbgGetCPSR();
                    UINT32 sp = Arm9DbgGetRegister(eR13);
                    UINT32 lr = Arm9DbgGetRegister(eR14);
                    UINT32 spsr = Arm9DbgGetRegister(eSPSR_IRQ);
                    UINT32 sp_irq = Arm9DbgGetRegister(eR13_IRQ);
                    PGM2_LOG(PGM2_LOG_AIC, "IVR read pc=%08X result=%08X midx=%d mask=%08X lvl=%d frame=%d cpsr=%08X sp=%08X lr=%08X sp_irq=%08X spsr_irq=%08X",
                        pc, result, midx, mask, pgm2AicGetLevel(), Pgm2RtcFrameCounter, cpsr, sp, lr, sp_irq, spsr);
                    ivrLogCount++;
                }
            }
            Pgm2AicCoreStatus &= ~2;
            // Match MAME: only update IRQ line here, do NOT re-evaluate (set_lines, not check_irqs)
            Arm9SetIRQLine(ARM7_IRQ_LINE, (Pgm2AicCoreStatus & 2) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
            return result;
        }
        case 0x104: // AIC_FVR
        {
            UINT32 fmask = Pgm2AicPending & Pgm2AicEnabled & Pgm2AicFastIrqs;
            return fmask ? Pgm2AicSvr[0] : Pgm2AicSpuIvr;
        }
        case 0x108: return (UINT32)Pgm2AicStatus; // AIC_ISR
        case 0x10C: return Pgm2AicPending;  // AIC_IPR
        case 0x110: return Pgm2AicEnabled;  // AIC_IMR
        case 0x114: return Pgm2AicCoreStatus; // AIC_CISR
        case 0x148: return Pgm2AicFastIrqs; // AIC_FFSR
        default: return 0;
    }
}

static void pgm2AicWrite(UINT32 offset, UINT32 data)
{

    if (offset < 0x80) { Pgm2AicSmr[offset / 4] = data; return; }
    if (offset < 0x100) { Pgm2AicSvr[(offset - 0x80) / 4] = data; return; }

    switch (offset) {
        case 0x120: // AIC_IECR
        {
            UINT32 newBits = data & ~Pgm2AicEnabled;
            Pgm2AicEnabled |= data;
            if (newBits) {
                PGM2_LOG(PGM2_LOG_AIC, "IECR enable new=%08X total=%08X frame=%d", newBits, Pgm2AicEnabled, Pgm2RtcFrameCounter);
            }
            break;
        }
        case 0x124: // AIC_IDCR
        {
            Pgm2AicEnabled &= ~data;
            if (data == 0xFFFFFFFF) {
                UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
                UINT32 cpsr = Arm9DbgGetCPSR();
                UINT32 lr = Arm9DbgGetRegister(eR14);
                UINT32 sp = Arm9DbgGetRegister(eR13);
                static INT32 idcrLogCount = 0;
                if (idcrLogCount < 20) {
                    PGM2_LOG(PGM2_LOG_AIC, "IDCR=FFFFFFFF DISABLE ALL pc=%08X cpsr=%08X mode=%02X lr=%08X sp=%08X frame=%d",
                        pc, cpsr, cpsr & 0x1F, lr, sp, Pgm2RtcFrameCounter);
                    idcrLogCount++;
                }
            }
            break;
        }
        case 0x128: // AIC_ICCR
        {
            static INT32 iccrLogCount = 0;
            if (iccrLogCount < 60) {
                UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
                PGM2_LOG(PGM2_LOG_AIC, "ICCR data=%08X pc=%08X lvl=%d frame=%d",
                        data, pc, pgm2AicGetLevel(), Pgm2RtcFrameCounter);
                iccrLogCount++;
            }
            Pgm2AicPending &= ~data;
            break;
        }
        case 0x12C: Pgm2AicPending |= data; break;           // AIC_ISCR
        case 0x130: // AIC_EOICR
        {
            static INT32 eoicrLogCount = 0;
            if (eoicrLogCount < 60) {
                UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
                PGM2_LOG(PGM2_LOG_AIC, "EOICR pc=%08X lvl=%d->%d frame=%d",
                    pc, pgm2AicGetLevel(), Pgm2AicLvlIdx > 0 ? Pgm2AicLevelStack[Pgm2AicLvlIdx - 1] : -1, Pgm2RtcFrameCounter);
                eoicrLogCount++;
            }
            pgm2AicPopLevel();
            break;
        }
        case 0x134: Pgm2AicSpuIvr = data; break;             // AIC_SPU
        case 0x138: Pgm2AicDebug = data; break;               // AIC_DCR
        case 0x140: Pgm2AicFastIrqs |= data; break;          // AIC_FFER
        case 0x144: Pgm2AicFastIrqs &= ~data;
                    Pgm2AicFastIrqs |= 1; break;             // AIC_FFDR (source 0 always fast)
    }
    pgm2AicCheckIrqs();
}

static inline UINT8 pgm2SharedReadByte(UINT32 addr)
{
    UINT32 lo = addr - 0x30100000;
    if (lo > 0xff) return 0xff;

    // umask32(0x00ff00ff): only byte lanes 0/2 are connected => even addresses.
    if (lo & 1) return 0xff;

    UINT32 offs = (lo >> 1) & 0x7f;
    return Pgm2SharedRAM2[offs + ((Pgm2ShareBank & 1) * 0x80)];
}

static inline void pgm2SharedWriteByte(UINT32 addr, UINT8 data)
{
    UINT32 lo = addr - 0x30100000;
    if (lo > 0xff) return;
    if (lo & 1) return;

    UINT32 offs = (lo >> 1) & 0x7f;
    Pgm2SharedRAM2[offs + ((Pgm2ShareBank & 1) * 0x80)] = data;
}

static void pgm2McuCommand(bool isCommand)
{
    if (isCommand) {
        UINT8 cmd = (UINT8)(Pgm2McuRegs[0] & 0xff);
        UINT8 arg1 = (UINT8)((Pgm2McuRegs[0] >> 8) & 0xff);
        UINT8 arg2 = (UINT8)((Pgm2McuRegs[0] >> 16) & 0xff);
        UINT8 arg3 = (UINT8)((Pgm2McuRegs[0] >> 24) & 0xff);
        UINT32 status = 0x00f70000; // accepted

        // MCU diagnostic
        PGM2_LOG(PGM2_LOG_MCU, "CMD cmd=%02X arg=%02X,%02X,%02X reg0=%08X reg1=%08X frame=%d",
            cmd, arg1, arg2, arg3, Pgm2McuRegs[0], Pgm2McuRegs[1], Pgm2RtcFrameCounter);


        PGM2_LOG(PGM2_LOG_SYS, "MCU cmd=%02X reg0=%08X reg1=%08X sharebank=%u", cmd, Pgm2McuRegs[0], Pgm2McuRegs[1], Pgm2ShareBank & 1);

        Pgm2McuLastCmd = cmd;
        // DO NOT set Pgm2McuResult0/1 here — MAME sets them per-command.
        // F6 (get-result) must read results from the PREVIOUS command.

        switch (cmd)
        {
            case 0xf6: // return previously prepared result data
                // MAME: writes FULL result0 to reg[3], result1 to reg[4],
                // clears last_cmd so ACK is a no-op.
                Pgm2McuRegs[3] = Pgm2McuResult0;
                Pgm2McuRegs[4] = Pgm2McuResult1;
                Pgm2McuLastCmd = 0;
            break;

            case 0xe0: // startup/self-test echo
                Pgm2McuResult0 = Pgm2McuRegs[0];
                Pgm2McuResult1 = Pgm2McuRegs[1];
            break;

            case 0xe1: // shared RAM fill helper used by boot code
                // MCU sees the opposite bank while CPU accesses the selected one.
                if (Pgm2SharedRAM2 && arg2 == 2) {
                    memset(Pgm2SharedRAM2 + ((~Pgm2ShareBank & 1) * 0x80), arg3, 0x80);
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc0: // insert card / check card presence
            case 0xc1: // check ready/busy
                if (!pgm2CardPresent(arg1)) {
                    status = 0x00f40000;
                }
                if (Pgm2CardLogCount < 200) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD cmd=%02X slot=%d present=%d"), cmd, arg1 & 3, pgm2CardPresent(arg1) ? 1 : 0);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc2: // read card data to shared RAM
                for (INT32 i = 0; i < arg3; i++) {
                    if (!pgm2CardPresent(arg1)) {
                        status = 0x00f40000;
                        break;
                    }
                    Pgm2SharedRAM2[i + ((~Pgm2ShareBank & 1) * 0x80)] = pgm2CardReadData(arg1, arg2 + i);
                }
                if (Pgm2CardLogCount < 200) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD read slot=%d offset=%02X len=%d first=%02X"), arg1 & 3, arg2, arg3, arg3 > 0 ? pgm2CardReadData(arg1, arg2) : 0);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc3: // save shared RAM to card data
                for (INT32 i = 0; i < arg3; i++) {
                    if (!pgm2CardPresent(arg1)) {
                        status = 0x00f40000;
                        break;
                    }
                    pgm2CardWriteData(arg1, arg2 + i, Pgm2SharedRAM2[i + ((~Pgm2ShareBank & 1) * 0x80)]);
                }
                if (Pgm2CardLogCount < 200) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD write slot=%d offset=%02X len=%d auth=%d"), arg1 & 3, arg2, arg3, Pgm2CardAuthenticated[arg1 & 3] ? 1 : 0);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc4: // read security bytes 1..3
                if (pgm2CardPresent(arg1)) {
                    Pgm2McuResult1 = pgm2CardReadSec(arg1, 1) | (pgm2CardReadSec(arg1, 2) << 8) | (pgm2CardReadSec(arg1, 3) << 16);
                } else {
                    status = 0x00f40000;
                }
                if (Pgm2CardLogCount < 200) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD read_sec slot=%d result=%06X auth=%d"), arg1 & 3, Pgm2McuResult1 & 0xFFFFFF, Pgm2CardAuthenticated[arg1 & 3] ? 1 : 0);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc5: // write security byte
                if (pgm2CardPresent(arg1)) {
                    pgm2CardWriteSec(arg1, arg2 & 3, arg3);
                } else {
                    status = 0x00f40000;
                }
                if (Pgm2CardLogCount < 200) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD write_sec slot=%d idx=%d val=%02X auth=%d"), arg1 & 3, arg2 & 3, arg3, Pgm2CardAuthenticated[arg1 & 3] ? 1 : 0);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc6: // write protection byte
                if (pgm2CardPresent(arg1)) {
                    pgm2CardWriteProt(arg1, arg2 & 3, arg3);
                } else {
                    status = 0x00f40000;
                }
                if (Pgm2CardLogCount < 200) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD write_prot slot=%d idx=%d val=%02X auth=%d"), arg1 & 3, arg2 & 3, arg3, Pgm2CardAuthenticated[arg1 & 3] ? 1 : 0);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc7: // read protection bytes
                if (pgm2CardPresent(arg1)) {
                    Pgm2McuResult1 = pgm2CardReadProt(arg1, 0) | (pgm2CardReadProt(arg1, 1) << 8) | (pgm2CardReadProt(arg1, 2) << 16) | (pgm2CardReadProt(arg1, 3) << 24);
                } else {
                    status = 0x00f40000;
                }
                if (Pgm2CardLogCount < 200) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD read_prot slot=%d result=%08X"), arg1 & 3, Pgm2McuResult1);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc8: // write one card data byte
                if (pgm2CardPresent(arg1)) {
                    pgm2CardWriteData(arg1, arg2, arg3);
                } else {
                    status = 0x00f40000;
                }
                if (Pgm2CardLogCount < 200) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD write_byte slot=%d offset=%02X val=%02X auth=%d"), arg1 & 3, arg2, arg3, Pgm2CardAuthenticated[arg1 & 3] ? 1 : 0);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            case 0xc9: // card authentication (SLE4442 PSC verification)
                if (pgm2CardPresent(arg1)) {
                    pgm2CardAuth(arg1, arg2, arg3, Pgm2McuRegs[1] & 0xff);
                } else {
                    status = 0x00f40000;
                }
                if (Pgm2CardLogCount < 200) {
                    UINT8 s = arg1 & 3;
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD auth slot=%d psc=%02X,%02X,%02X result=%d sec0=%02X"), s, arg2, arg3, Pgm2McuRegs[1] & 0xff,
                        Pgm2CardAuthenticated[s] ? 1 : 0,
                        Pgm2Cards[s] ? Pgm2Cards[s][PGM2_CARD_DATA_SIZE + 4] : 0);
                    Pgm2CardLogCount++;
                }
                Pgm2McuResult0 = cmd;
            break;

            default:
                // Match MAME: unknown commands report error status (0xF4).
                status = 0x00f40000;
                Pgm2McuResult0 = cmd;
            break;
        }

        // Status is exposed in bits [23:16].
        Pgm2McuRegs[3] = (Pgm2McuRegs[3] & 0xff00ffff) | status;

        // MAME defers IRQ3 via a 1ms timer (mcu_timer → mcu_interrupt).
        // We approximate the delay with a scanline countdown; IRQ3 is
        // asserted only when the countdown expires (in pgm2Frame), NOT
        // immediately — the game must have time to finish setting up AIC
        // SVR vectors before the first MCU interrupt fires.
        Pgm2McuDoneCountdown = 8;
    } else {
        // ACK: MAME sets status F2 and starts another short timer (100μs),
        // then clears IRQ3 immediately on the ACK write.
        if (Pgm2McuLastCmd) {
            Pgm2McuRegs[3] = (Pgm2McuRegs[3] & 0xff00ffff) | 0x00f20000;
            Pgm2McuLastCmd = 0;
            // Start a short countdown for phase-2 IRQ (results ready)
            Pgm2McuDoneCountdown = 2;
        }
        Pgm2McuIrq3 = 0;
        pgm2AicSetIrq(3, 0);
        PGM2_LOG(PGM2_LOG_MCU, "MCU ack/done status=%08X result0=%08X result1=%08X", Pgm2McuRegs[3], Pgm2McuResult0, Pgm2McuResult1);
    }
}

static inline UINT32 pgm2McuRead(INT32 reg)
{
    if (Pgm2McuReadLogCount < 24) {
        PGM2_LOG(PGM2_LOG_MCU, "MCU reg%d read=%08X", reg & 7, Pgm2McuRegs[reg & 7]);
        Pgm2McuReadLogCount++;
    }
    return Pgm2McuRegs[reg & 7];
}

static inline void pgm2McuWrite(INT32 reg, UINT32 data)
{
    reg &= 7;
    Pgm2McuRegs[reg] = data;

    if (reg == 2 || reg == 5) {
        PGM2_LOG(PGM2_LOG_MCU, "MCU reg%d write=%08X reg0=%08X reg1=%08X reg3=%08X", reg, data, Pgm2McuRegs[0], Pgm2McuRegs[1], Pgm2McuRegs[3]);
    }

    if (reg == 2 && data) {
        pgm2McuCommand(true);
    }

    if (reg == 5 && data) {
        pgm2McuCommand(false);
        Pgm2McuRegs[5] = 0;
    }
}

static void pgm2SetVidSubPtrs()
{
    Pgm2SpOAM      = Pgm2VidRAM + 0x00000;
    Pgm2BgVRAM     = Pgm2VidRAM + 0x20000;
    Pgm2FgVRAM     = Pgm2VidRAM + 0x40000;
    Pgm2SpPal      = (UINT32*)(Pgm2VidRAM + 0x60000);
    Pgm2BgPal      = (UINT32*)(Pgm2VidRAM + 0x80000);
    Pgm2TxPal      = (UINT32*)(Pgm2VidRAM + 0xA0000);
    Pgm2SpZoom     = (UINT32*)(Pgm2VidRAM + 0xC0000);
    Pgm2LineRAM    = Pgm2VidRAM + 0xE0000;
    Pgm2SharedRAM2 = Pgm2VidRAM + 0x100000;
    Pgm2VideoRegs  = (UINT32*)(Pgm2VidRAM + 0x120000);
}

// ---------------------------------------------------------------------------
// Memory helpers — match MAME pgm2_map() address ranges
// ---------------------------------------------------------------------------
static inline UINT32 pgm2ReadLongDirect(UINT32 addr)
{
    addr &= 0x7FFFFFFF; // IGS036 uses 31-bit address bus; mask bit 31

    // External Flash ROM 0x10000000-0x10FFFFFF (pgm2_rom_map)
    if (addr >= 0x10000000 && addr <= 0x10FFFFFF) {
        UINT32 off = addr - 0x10000000;
        UINT32 b0 = (off + 0 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 0] : 0;
        UINT32 b1 = (off + 1 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 1] : 0;
        UINT32 b2 = (off + 2 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 2] : 0;
        UINT32 b3 = (off + 3 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 3] : 0;
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    // Main RAM 0x20000000-0x2007FFFF
    if (addr >= 0x20000000 && addr <= 0x2007FFFF) {
        UINT32 v = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(Pgm2ArmRAM + (addr - 0x20000000)));
        // Speed hack: detect busy-loop reads and eat remaining cycles
        if (Pgm2SpeedHackAddr && (addr & ~3) == Pgm2SpeedHackAddr) {
            UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
            bool pcMatch = false;
            for (INT32 i = 0; i < 4; i++) {
                if (Pgm2SpeedHackPC[i] && pc == Pgm2SpeedHackPC[i]) {
                    pcMatch = true;
                    break;
                }
            }
            if (pcMatch && (addr - 0x20000000) <= 0x7FFF8) {
                UINT32 next = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(Pgm2ArmRAM + (addr - 0x20000000) + 4));
                if (v == 0 && next == 0) {
                    Arm9RunEndEatCycles();
                }
            }
        }
        return v;
    }

    // Sprite OAM 0x30000000-0x30001FFF (8KB, matches MAME pgm2_map)
    if (addr >= 0x30000000 && addr <= 0x30001FFF) {
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(Pgm2SpOAM + (addr - 0x30000000)));
    }

    // BG videoram 0x30020000-0x30021FFF (8KB, matches MAME pgm2_map)
    if (addr >= 0x30020000 && addr <= 0x30021FFF) {
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(Pgm2BgVRAM + (addr - 0x30020000)));
    }

    // FG videoram 0x30040000-0x30045FFF
    if (addr >= 0x30040000 && addr <= 0x30045FFF) {
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(Pgm2FgVRAM + (addr - 0x30040000)));
    }

    // Sprite palette 0x30060000-0x30063FFF
    if (addr >= 0x30060000 && addr <= 0x30063FFF) {
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)((UINT8*)Pgm2SpPal + (addr - 0x30060000)));
    }

    // BG palette 0x30080000-0x30081FFF
    if (addr >= 0x30080000 && addr <= 0x30081FFF) {
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)((UINT8*)Pgm2BgPal + (addr - 0x30080000)));
    }

    // TX palette 0x300A0000-0x300A07FF
    if (addr >= 0x300A0000 && addr <= 0x300A07FF) {
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)((UINT8*)Pgm2TxPal + (addr - 0x300A0000)));
    }

    // Sprite zoom 0x300C0000-0x300C01FF
    if (addr >= 0x300C0000 && addr <= 0x300C01FF) {
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)((UINT8*)Pgm2SpZoom + (addr - 0x300C0000)));
    }

    // Line RAM 0x300E0000-0x300E03FF (mirrored)
    if (addr >= 0x300E0000 && addr <= 0x300EFFFF) {
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(Pgm2LineRAM + ((addr - 0x300E0000) & 0x3FF)));
    }

    // Shared RAM 0x30100000-0x301000FF (banked, byte-lane masked)
    if (addr >= 0x30100000 && addr <= 0x301000FF)
    {
        UINT32 val = (UINT32)pgm2SharedReadByte((addr & ~3) + 0)
             | ((UINT32)pgm2SharedReadByte((addr & ~3) + 1) << 8)
             | ((UINT32)pgm2SharedReadByte((addr & ~3) + 2) << 16)
             | ((UINT32)pgm2SharedReadByte((addr & ~3) + 3) << 24);
#if PGM2_LOG_ENABLED
        if (Pgm2SharedLogCount < 24) {
            PGM2_LOG(PGM2_LOG_SYS, "shared read addr=%08X val=%08X bank=%u", addr & ~3, val, Pgm2ShareBank & 1);
            Pgm2SharedLogCount++;
        }
#endif
        return val;
    }

    // Video regs 0x30120000-0x3012003F
    if (addr >= 0x30120000 && addr <= 0x3012003F)
    {
        UINT32 val = BURN_ENDIAN_SWAP_INT32(*(UINT32*)((UINT8*)Pgm2VideoRegs + (addr - 0x30120000)));
#if PGM2_LOG_ENABLED
        if (Pgm2VideoRegLogCount < 24) {
            PGM2_LOG(PGM2_LOG_SYS, "vreg read addr=%08X off=%02X val=%08X", addr, addr - 0x30120000, val);
            Pgm2VideoRegLogCount++;
        }
#endif
        return val;
    }

    // YMZ774 0x40000000-0x40000003
    if (addr >= 0x40000000 && addr <= 0x40000003) {
        UINT8 b0 = ymz774_read(0);
        UINT8 b1 = ymz774_read(1);
        UINT8 b2 = ymz774_read(2);
        UINT8 b3 = ymz774_read(3);
        return b0 | ((UINT32)b1 << 8) | ((UINT32)b2 << 16) | ((UINT32)b3 << 24);
    }

    // unk_startup_r 0x4000000C (MAME: pgm2_state::unk_startup_r)
    if (addr >= 0x4000000C && addr <= 0x4000000F) {
        return 0x00000180;
    }

    // INPUTS0 0x03900000
    if (addr >= 0x03900000 && addr <= 0x03900003) {
        // 4 players, each 8 bits, packed in 32-bit register (active low)
        return ~((UINT32)Pgm2Input[0] | ((UINT32)Pgm2Input[1] << 8) |
                 ((UINT32)Pgm2Input[2] << 16) | ((UINT32)Pgm2Input[3] << 24));
    }

    // INPUTS1 0x03A00000 (starts, coins, service)
    if (addr >= 0x03A00000 && addr <= 0x03A00003) {
        return ~((UINT32)Pgm2Input[4] | ((UINT32)Pgm2Input[5] << 8) |
                 ((UINT32)Pgm2Input[6] << 16) | ((UINT32)Pgm2Input[7] << 24));
    }

    // SRAM (battery) 0x02000000-0x0200FFFF
    if (addr >= 0x02000000 && addr <= 0x0200FFFF)
        return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(Pgm2ExtRAM + (addr - 0x02000000)));

    // MPU MCU registers: 128KB per register, reg = byte_offset >> 17
    // MAME: (word_offset >> 15) & 7, equivalent to (byte_offset >> 17) & 7
    if (addr >= 0x03600000 && addr <= 0x036BFFFF)
    {
        return pgm2McuRead((addr - 0x03600000) >> 17);
    }

    // Real Time Timer. MAME returns machine().time().seconds().
    // Advance this from emulated frame time so timeout loops can progress.
    if (addr == 0x7FFFFD28)
    {
        return (UINT32)(Pgm2RtcBase + (Pgm2RtcFrameCounter / 60));
    }

    // IGS036 internal peripherals (high addresses, bit 31 masked by ARM9 to 0x7Fxxxxxx)
    // Encryption table reads 0xFFFFFC00-0xFFFFFCFF => 0x7FFFFC00-0x7FFFFCFF
    if (addr >= 0x7FFFFC00 && addr <= 0x7FFFFCFF) {
        UINT32 off = addr - 0x7FFFFC00;
        return (UINT32)Pgm2EncryptTable[(off + 0) & 0xFF]
             | ((UINT32)Pgm2EncryptTable[(off + 1) & 0xFF] << 8)
             | ((UINT32)Pgm2EncryptTable[(off + 2) & 0xFF] << 16)
             | ((UINT32)Pgm2EncryptTable[(off + 3) & 0xFF] << 24);
    }
    // unk_startup_r at 0xfffffa0c is observed by the ARM core as 0x7ffffa0c.
    if (addr == 0x7FFFFA0C) {
        return 0x00000180;
    }

    // AIC and other internal peripheral ranges
    if (addr >= 0x7FFFFA00 && addr <= 0x7FFFFA0B) {
        return 0;
    }
    if (addr >= 0x7FFFF000 && addr <= 0x7FFFF14B)
    {
        return pgm2AicRead(addr - 0x7FFFF000);
    }
    // PIO read (kov3 module GPIO)
    if (addr == 0x7FFFF43C && Pgm2HasKov3Module) {
#if PGM2_LOG_ENABLED
        static INT32 pioRdLogCount = 0;
        if (pioRdLogCount < 50) {
            UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
            PGM2_LOG(PGM2_LOG_PIO, "READ PDSR addr=%08X pc=%08X frame=%d outLatch=%d",
                addr, pc, Pgm2RtcFrameCounter, Pgm2ModuleOutLatch);
            pioRdLogCount++;
        }
#endif
        return (UINT32)(Pgm2ModuleOutLatch ? (1 << 8) : 0);
    }
    if (addr >= 0x7FFFF000 && addr <= 0x7FFFFFFF)
    {
        return 0;
    }

    // Log unmapped reads in the video/peripheral range during boot
#if PGM2_LOG_ENABLED
    if (Pgm2HasDecrypted && addr >= 0x30000000 && addr <= 0x30FFFFFF) {
        static INT32 unmapLogCount = 0;
        if (unmapLogCount < 50) {
            UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
            PGM2_LOG(PGM2_LOG_SYS, "UNMAPPED READ addr=%08X pc=%08X", addr, pc);
            unmapLogCount++;
        }
    }
#endif

    return 0x00000000;
}

static UINT8 pgm2ReadByte(UINT32 addr)
{
    addr &= 0x7FFFFFFF; // IGS036 uses 31-bit address bus; mask bit 31

    // YMZ774 0x40000000-0x40000003 (byte access)
    if (addr >= 0x40000000 && addr <= 0x40000003) {
        return ymz774_read(addr & 3);
    }

    // KOV3 module ROM read intercept (first 16 bytes)
    if (Pgm2HasKov3Module && addr >= 0x10000000 && addr <= 0x1000000F) {
        UINT32 halfOff = (addr - 0x10000000) >> 1;
        UINT16 w = pgm2ModuleRomR(halfOff);
        return (addr & 1) ? (UINT8)(w >> 8) : (UINT8)(w & 0xFF);
    }
    if (addr >= 0x10000000 && addr <= 0x10FFFFFF) {
        UINT32 off = addr - 0x10000000;
        UINT8 v = (off < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off] : 0;
        return v;
    }

    UINT32 word = pgm2ReadLongDirect(addr & ~3);
    return (word >> ((addr & 3) * 8)) & 0xFF;
}

static UINT16 pgm2ReadWord(UINT32 addr)
{
    addr &= 0x7FFFFFFF; // IGS036 uses 31-bit address bus; mask bit 31

    // YMZ774 0x40000000-0x40000003 (word access)
    if (addr >= 0x40000000 && addr <= 0x40000003) {
        UINT32 off = addr & 3;
        return ymz774_read(off) | ((UINT16)ymz774_read(off + 1) << 8);
    }

    // KOV3 module ROM read intercept (first 16 bytes)
    if (Pgm2HasKov3Module && addr >= 0x10000000 && addr <= 0x1000000F) {
        UINT32 offset = (addr - 0x10000000) >> 1;
        return pgm2ModuleRomR(offset);
    }

    if (addr >= 0x10000000 && addr <= 0x10FFFFFF) {
        UINT32 off = addr - 0x10000000;
        UINT32 b0 = (off + 0 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 0] : 0;
        UINT32 b1 = (off + 1 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 1] : 0;
        return (UINT16)(b0 | (b1 << 8));
    }

    UINT32 word = pgm2ReadLongDirect(addr & ~3);
    return (word >> ((addr & 2) * 8)) & 0xFFFF;
}

static UINT32 pgm2ReadLong(UINT32 addr)
{
    addr &= 0x7FFFFFFF; // IGS036 uses 31-bit address bus; mask bit 31
    // KOV3 module ROM read intercept (first 16 bytes, 32-bit composed from two 16-bit reads)
    if (Pgm2HasKov3Module && addr >= 0x10000000 && addr <= 0x1000000F) {
        UINT32 halfOff = (addr - 0x10000000) >> 1;
        UINT16 lo = pgm2ModuleRomR(halfOff);
        UINT16 hi = pgm2ModuleRomR(halfOff + 1);
        return (UINT32)lo | ((UINT32)hi << 16);
    }
    if (addr >= 0x10000000 && addr <= 0x10FFFFFF) {
        UINT32 off = addr - 0x10000000;
        UINT32 b0 = (off + 0 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 0] : 0;
        UINT32 b1 = (off + 1 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 1] : 0;
        UINT32 b2 = (off + 2 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 2] : 0;
        UINT32 b3 = (off + 3 < (UINT32)Pgm2ArmROMLen) ? Pgm2ArmROM[off + 3] : 0;
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    return pgm2ReadLongDirect(addr);
}

static void pgm2WriteLongDirect(UINT32 addr, UINT32 data)
{
    addr &= 0x7FFFFFFF; // IGS036 uses 31-bit address bus; mask bit 31
    UINT32 d = BURN_ENDIAN_SWAP_INT32(data);

    // KOV3 module ROM write intercept (split 32-bit into two 16-bit writes)
    if (Pgm2HasKov3Module && addr >= 0x10000000 && addr <= 0x107FFFFF) {
        {
            static INT32 modRomWrLogCount = 0;
            if (modRomWrLogCount < 20) {
                UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
                PGM2_LOG(PGM2_LOG_MODULE, "ROM_WRITE addr=%08X data=%08X pc=%08X frame=%d",
                    addr, data, pc, Pgm2RtcFrameCounter);
                modRomWrLogCount++;
            }
        }
        UINT32 halfOff = (addr - 0x10000000) >> 1;
        pgm2ModuleRomW(halfOff, (UINT16)(data & 0xFFFF));
        pgm2ModuleRomW(halfOff + 1, (UINT16)((data >> 16) & 0xFFFF));
        return;
    }

    // Main RAM
    if (addr >= 0x20000000 && addr <= 0x2007FFFF) {
        *(UINT32*)(Pgm2ArmRAM + (addr - 0x20000000)) = d;
        return;
    }
    // Sprite OAM
    if (addr >= 0x30000000 && addr <= 0x30001FFF) {
        *(UINT32*)(Pgm2SpOAM + (addr - 0x30000000)) = d;
        return;
    }
    // BG videoram (8KB, matches MAME pgm2_map)
    if (addr >= 0x30020000 && addr <= 0x30021FFF) {
        *(UINT32*)(Pgm2BgVRAM + (addr - 0x30020000)) = d;
        return;
    }
    // FG videoram
    if (addr >= 0x30040000 && addr <= 0x30045FFF) {
        *(UINT32*)(Pgm2FgVRAM + (addr - 0x30040000)) = d;
        return;
    }
    // Sprite palette
    if (addr >= 0x30060000 && addr <= 0x30063FFF) {
        *(UINT32*)((UINT8*)Pgm2SpPal + (addr - 0x30060000)) = d;
        return;
    }
    // BG palette
    if (addr >= 0x30080000 && addr <= 0x30081FFF) {
        *(UINT32*)((UINT8*)Pgm2BgPal + (addr - 0x30080000)) = d;
        return;
    }
    // TX palette
    if (addr >= 0x300A0000 && addr <= 0x300A07FF) {
        *(UINT32*)((UINT8*)Pgm2TxPal + (addr - 0x300A0000)) = d;
        return;
    }
    // Sprite zoom table
    if (addr >= 0x300C0000 && addr <= 0x300C01FF) {
        *(UINT32*)((UINT8*)Pgm2SpZoom + (addr - 0x300C0000)) = d;
        return;
    }
    // Line RAM
    if (addr >= 0x300E0000 && addr <= 0x300EFFFF) {
        *(UINT32*)(Pgm2LineRAM + ((addr - 0x300E0000) & 0x3FF)) = d;
        return;
    }
    // Shared RAM (banked, byte-lane masked)
    if (addr >= 0x30100000 && addr <= 0x301000FF) {
        if (Pgm2SharedLogCount < 24) {
            PGM2_LOG(PGM2_LOG_SYS, "shared write addr=%08X data=%08X bank=%u", addr & ~3, data, Pgm2ShareBank & 1);
            Pgm2SharedLogCount++;
        }
        pgm2SharedWriteByte(addr + 0, (UINT8)(data >> 0));
        pgm2SharedWriteByte(addr + 1, (UINT8)(data >> 8));
        pgm2SharedWriteByte(addr + 2, (UINT8)(data >> 16));
        pgm2SharedWriteByte(addr + 3, (UINT8)(data >> 24));
        return;
    }
    // Video regs
    if (addr >= 0x30120000 && addr <= 0x3012003F) {
        if (Pgm2VideoRegLogCount < 48) {
            PGM2_LOG(PGM2_LOG_SYS, "vreg write addr=%08X off=%02X data=%08X", addr, addr - 0x30120000, data);
            Pgm2VideoRegLogCount++;
        }

        // 0x30120018 = vbl ack
        if ((addr & ~3) == 0x30120018) {
            pgm2AicSetIrq(12, 0);
        }

        // 0x30120032 = shared RAM bank select
        if ((addr & ~3) == 0x30120030) {
            // Can arrive from 8/16/32-bit paths; keep whichever half carries bit0.
            if (data & 0xffff0000) Pgm2ShareBank = (UINT8)((data >> 16) & 1);
            else                   Pgm2ShareBank = (UINT8)(data & 1);
        }

        *(UINT32*)((UINT8*)Pgm2VideoRegs + (addr - 0x30120000)) = d;
        return;
    }
    // YMZ774
    if (addr >= 0x40000000 && addr <= 0x40000003) {
        ymz770_write(0, (UINT8)(data));
        ymz770_write(1, (UINT8)(data >> 8));
        ymz770_write(2, (UINT8)(data >> 16));
        ymz770_write(3, (UINT8)(data >> 24));
        return;
    }
    // SRAM
    if (addr >= 0x02000000 && addr <= 0x0200FFFF) {
        *(UINT32*)(Pgm2ExtRAM + (addr - 0x02000000)) = d;
        return;
    }
    // MPU MCU registers: 128KB per register, reg = byte_offset >> 17
    if (addr >= 0x03600000 && addr <= 0x036BFFFF)
    {
        pgm2McuWrite((addr - 0x03600000) >> 17, data);
        return;
    }
    // Encryption table writes 0xFFFFFC00-0xFFFFFCFF => 0x7FFFFC00-0x7FFFFCFF
    if (addr >= 0x7FFFFC00 && addr <= 0x7FFFFCFF) {
        UINT32 off = addr - 0x7FFFFC00;
        // Store 4 bytes little-endian (ARM9 byte order)
        Pgm2EncryptTable[(off + 0) & 0xFF] = (UINT8)(data);
        Pgm2EncryptTable[(off + 1) & 0xFF] = (UINT8)(data >> 8);
        Pgm2EncryptTable[(off + 2) & 0xFF] = (UINT8)(data >> 16);
        Pgm2EncryptTable[(off + 3) & 0xFF] = (UINT8)(data >> 24);
        if (Pgm2EncWriteCount < 16) {
            PGM2_LOG(PGM2_LOG_SYS, "enc write32 off=%02X data=%08X", off, data);
        }
        Pgm2EncWriteCount++;
        return;
    }
    // encryption_do_w: write to 0xFFFFFA08 => 0x7FFFFA08 triggers ROM decryption
    if (addr >= 0x7FFFFA00 && addr <= 0x7FFFFA0F) {
        if (addr == 0x7FFFFA08 && Pgm2ArmROM && Pgm2ArmROMLen > 0) {
            pgm2DoDecrypt("32-bit");
        }
        return;
    }
    // AIC registers
    if (addr >= 0x7FFFF000 && addr <= 0x7FFFF14B) {
        {
            static INT32 aicWriteLogCount = 0;
            if (aicWriteLogCount < 500) {
                UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
                PGM2_LOG(PGM2_LOG_AIC, "WRITE32 addr=%08X off=%03X data=%08X pc=%08X frame=%d",
                    addr, addr - 0x7FFFF000, data, pc, Pgm2RtcFrameCounter);
                aicWriteLogCount++;
            }
        }
        pgm2AicWrite(addr - 0x7FFFF000, data);
        return;
    }
    // PIO SODR/CODR (kov3 module GPIO) — also log any PIO-range writes for diag
    if (addr >= 0x7FFFF400 && addr <= 0x7FFFF5FF) {
        static INT32 pioLogCount = 0;
        if (pioLogCount < 100) {
            UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
            PGM2_LOG(PGM2_LOG_PIO, "WRITE addr=%08X data=%08X pc=%08X frame=%d hasModule=%d",
                addr, data, pc, Pgm2RtcFrameCounter, Pgm2HasKov3Module);
            pioLogCount++;
        }
    }
    if (Pgm2HasKov3Module && (addr == 0x7FFFF430 || addr == 0x7FFFF434)) {
        if (addr == 0x7FFFF430) {
            Pgm2PioOutData |= data;
        } else {
            Pgm2PioOutData &= ~data;
        }
        pgm2ModuleDataW((Pgm2PioOutData & 0x100) ? 1 : 0);
        pgm2ModuleClkW((Pgm2PioOutData & 0x200) ? 1 : 0);
        return;
    }
    // All other IGS036 peripherals — silently ignore
    // Log unmapped writes in the video/peripheral range during boot
    if (Pgm2HasDecrypted && addr >= 0x30000000 && addr <= 0x30FFFFFF) {
        static INT32 unmapWrLogCount = 0;
        if (unmapWrLogCount < 50) {
            UINT32 pc = Arm9DbgGetPC() & 0x7fffffff;
            PGM2_LOG(PGM2_LOG_SYS, "UNMAPPED WRITE addr=%08X data=%08X pc=%08X", addr, data, pc);
            unmapWrLogCount++;
        }
    }
}

static void pgm2WriteByte(UINT32 addr, UINT8 data)
{
    addr &= 0x7FFFFFFF; // IGS036 uses 31-bit address bus; mask bit 31

    // YMZ774 0x40000000-0x40000003 (byte access — avoid RMW corruption)
    if (addr >= 0x40000000 && addr <= 0x40000003) {
        ymz770_write(addr & 3, data);
        return;
    }

    // Encryption table is byte-addressable. Do not route through 32-bit RMW.
    if (addr >= 0x7FFFFC00 && addr <= 0x7FFFFCFF) {
        Pgm2EncryptTable[addr - 0x7FFFFC00] = data;
        if (Pgm2EncWriteCount < 16) {
            PGM2_LOG(PGM2_LOG_SYS, "enc write8 off=%02X data=%02X", addr - 0x7FFFFC00, data);
        }
        Pgm2EncWriteCount++;
        return;
    }

    // Decrypt trigger is edge-sensitive on exact register write.
    if (addr == 0x7FFFFA08) {
        pgm2DoDecrypt("8-bit");
        return;
    }
    if (addr >= 0x7FFFFA00 && addr <= 0x7FFFFA0F) {
        return;
    }

    if (addr >= 0x30100000 && addr <= 0x301000FF) {
        pgm2SharedWriteByte(addr, data);
        return;
    }

    // MCU register windows are 32-bit registers; combine byte writes directly
    // to match MAME's COMBINE_DATA behavior for partial accesses.
    if (addr >= 0x03600000 && addr <= 0x036BFFFF) {
        INT32 reg = (addr - 0x03600000) >> 17;
        UINT32 shift = (addr & 3) * 8;
        UINT32 cur = pgm2McuRead(reg);
        UINT32 val = (cur & ~(0xFFu << shift)) | ((UINT32)data << shift);
        pgm2McuWrite(reg, val);
        return;
    }

    UINT32 aligned = addr & ~3;
    UINT32 shift   = (addr & 3) * 8;
    UINT32 old     = BURN_ENDIAN_SWAP_INT32(pgm2ReadLongDirect(aligned));
    UINT32 mask    = 0xFF << shift;
    pgm2WriteLongDirect(aligned, (old & ~mask) | ((UINT32)data << shift));
}

static void pgm2WriteWord(UINT32 addr, UINT16 data)
{
    addr &= 0x7FFFFFFF; // IGS036 uses 31-bit address bus; mask bit 31

    // YMZ774 0x40000000-0x40000003 (word access — avoid RMW corruption)
    if (addr >= 0x40000000 && addr <= 0x40000003) {
        UINT32 off = addr & 3;
        ymz770_write(off, (UINT8)(data));
        ymz770_write(off + 1, (UINT8)(data >> 8));
        return;
    }

    // KOV3 module ROM write intercept
    if (Pgm2HasKov3Module && addr >= 0x10000000 && addr <= 0x107FFFFF) {
        pgm2ModuleRomW((addr - 0x10000000) >> 1, data);
        return;
    }

    // Encryption table is byte-addressable. Preserve 16-bit semantics.
    if (addr >= 0x7FFFFC00 && addr <= 0x7FFFFCFF) {
        UINT32 off = addr - 0x7FFFFC00;
        if (off < 0x100) Pgm2EncryptTable[off] = (UINT8)(data >> 0);
        if (off + 1 < 0x100) Pgm2EncryptTable[off + 1] = (UINT8)(data >> 8);
        if (Pgm2EncWriteCount < 16) {
            PGM2_LOG(PGM2_LOG_SYS, "enc write16 off=%02X data=%04X", off, data);
        }
        Pgm2EncWriteCount++;
        return;
    }

    if (addr == 0x7FFFFA08) {
        pgm2DoDecrypt("16-bit");
        return;
    }
    if (addr >= 0x7FFFFA00 && addr <= 0x7FFFFA0F) {
        return;
    }

    if (addr == 0x30120032) {
        Pgm2ShareBank = (UINT8)(data & 1);
    }

    // MCU register windows are 32-bit registers; combine halfword writes
    // directly to avoid endian conversion artifacts in generic RMW path.
    if (addr >= 0x03600000 && addr <= 0x036BFFFF) {
        INT32 reg = (addr - 0x03600000) >> 17;
        UINT32 shift = (addr & 2) * 8;
        UINT32 cur = pgm2McuRead(reg);
        UINT32 val = (cur & ~(0xFFFFu << shift)) | ((UINT32)data << shift);
        pgm2McuWrite(reg, val);
        return;
    }

    UINT32 aligned = addr & ~3;
    UINT32 shift   = (addr & 2) * 8;
    UINT32 old     = BURN_ENDIAN_SWAP_INT32(pgm2ReadLongDirect(aligned));
    UINT32 mask    = 0xFFFF << shift;
    pgm2WriteLongDirect(aligned, (old & ~mask) | ((UINT32)data << shift));
}

static void pgm2WriteLong(UINT32 addr, UINT32 data)
{
    addr &= 0x7FFFFFFF; // IGS036 uses 31-bit address bus; mask bit 31
    pgm2WriteLongDirect(addr, data);
}

// ---------------------------------------------------------------------------
// Arm9TotalCycles wrapper for YMZ timing
// ---------------------------------------------------------------------------
static INT32 pgm2CpuCycles()
{
    return Arm9TotalCycles();
}

// ---------------------------------------------------------------------------
// pgm2Init  -- called by game-specific driver after loading ROMs + callbacks
// ---------------------------------------------------------------------------
INT32 pgm2Init()
{
    BurnSetRefreshRate(Pgm2RefreshRate);
    PGM2_LOG(PGM2_LOG_SYS, "==== PGM2 init ====");
    Pgm2RtcBase = (INT32)time(NULL);

    // Main RAM 0x20000000 - 512 KB
    Pgm2ArmRAM    = (UINT8*)BurnMalloc(0x80000);
    if (Pgm2ArmRAM) memset(Pgm2ArmRAM, 0, 0x80000);

    // Video RAM block (all 0x30xxxxxx sub-regions packed)
    Pgm2VidRAM    = (UINT8*)BurnMalloc(PGM2_VIDRAM_SIZE);
    if (Pgm2VidRAM) memset(Pgm2VidRAM, 0, PGM2_VIDRAM_SIZE);

    // Battery SRAM 0x02000000 - 64 KB
    Pgm2ExtRAM    = (UINT8*)BurnMalloc(0x10000);
    if (Pgm2ExtRAM) memset(Pgm2ExtRAM, 0, 0x10000);

    if (Pgm2CardRomIndex >= 0) {
        if (Pgm2MaxCardSlots <= 0) Pgm2MaxCardSlots = 4; // default: 4 slots
        for (int i = 0; i < PGM2_NUM_CARD_SLOTS; i++) {
            Pgm2Cards[i] = (UINT8*)BurnMalloc(PGM2_CARD_SIZE);
            if (Pgm2Cards[i]) memset(Pgm2Cards[i], 0xff, PGM2_CARD_SIZE);
            Pgm2CardAuthenticated[i] = false;
            Pgm2CardInserted[i] = false;  // cards start not-inserted; user must create/select+insert
        }
    }

    // RAM/ROM board (ddpdojt): writable RAM before ROM
    if (Pgm2RomBoardRAMSize > 0) {
        Pgm2RomBoardRAM = (UINT8*)BurnMalloc(Pgm2RomBoardRAMSize);
        if (Pgm2RomBoardRAM) memset(Pgm2RomBoardRAM, 0, Pgm2RomBoardRAMSize);
    }

    // Legacy pointers no longer used but keep non-NULL for safety
    Pgm2PalRAM    = NULL;   // palette data now inside VidRAM sub-regions
    Pgm2SharedRAM = NULL;   // shared MCU RAM inside VidRAM

    if (!Pgm2ArmRAM || !Pgm2VidRAM || !Pgm2ExtRAM || (Pgm2CardRomIndex >= 0 && !Pgm2Cards[0]))
        return 1;

    // Set up sub-pointers into the VidRAM block
    pgm2SetVidSubPtrs();

    memset(Pgm2McuRegs, 0, sizeof(Pgm2McuRegs));
    Pgm2McuResult0 = 0;
    Pgm2McuResult1 = 0;
    Pgm2McuLastCmd = 0;
    Pgm2ShareBank = 0;
    Pgm2McuIrq3 = 0;
    Pgm2McuDoneCountdown = 0;
    Pgm2EncWriteCount = 0;
    Pgm2VideoRegLogCount = 0;
    Pgm2SharedLogCount = 0;
    Pgm2McuReadLogCount = 0;
    Pgm2CardLogCount = 0;
    Pgm2PostDecryptFrameCount = 0;
    Pgm2RtcFrameCounter = 0;
    Pgm2IntRomOriginal3C44 = 0;
    Pgm2IntRomOriginal27F0 = 0;
    Pgm2IntRomOriginal27FC = 0;
    Pgm2IntRomNeedRestore = 0;

    // AIC state
    memset(Pgm2AicSmr, 0, sizeof(Pgm2AicSmr));
    memset(Pgm2AicSvr, 0, sizeof(Pgm2AicSvr));
    memset(Pgm2AicLevelStack, 0, sizeof(Pgm2AicLevelStack));
    Pgm2AicLevelStack[0] = -1;  // sentinel: any priority > -1
    Pgm2AicLvlIdx = 0;
    Pgm2AicPending = 0;
    Pgm2AicEnabled = 0;
    Pgm2AicSpuIvr = 0;
    Pgm2AicFastIrqs = 1;  // source 0 defaults to FIQ
    Pgm2AicDebug = 0;
    Pgm2AicStatus = 0;
    Pgm2AicCoreStatus = 0;

    // Game-specific ROM load + decrypt callback
    if (pPgm2InitCallback)
        pPgm2InitCallback();
	

    // Detect actual ROM file size for correct decrypt length and MAP_ROM range
    Pgm2ArmROMFileLen = Pgm2ArmROMLen;  // default to buffer size
    if (Pgm2ArmROM && Pgm2ArmROMLen > 0) {
        struct BurnRomInfo armri;
        memset(&armri, 0, sizeof(armri));
        if (BurnDrvGetRomInfo(&armri, 1) == 0 && armri.nLen > 0) {
            Pgm2ArmROMFileLen = armri.nLen;
            PGM2_LOG(PGM2_LOG_SYS, "arm rom file size=%d (0x%X) buffer=%d (0x%X)", armri.nLen, armri.nLen, Pgm2ArmROMLen, Pgm2ArmROMLen);
            // Fill unloaded tail with 0x00 to match MAME's ROM_REGION default zero-fill.
            // Module XOR operates on the full buffer; fill value matters for correct XOR results.
            if (armri.nLen < Pgm2ArmROMLen) {
                memset(Pgm2ArmROM + armri.nLen, 0x00, Pgm2ArmROMLen - armri.nLen);
            }
        }
    }

    if (Pgm2Cards[0] && Pgm2CardRomIndex >= 0) {
        // Load per-slot card ROMs first
        for (int i = 0; i < PGM2_NUM_CARD_SLOTS; i++) {
            if (Pgm2PerSlotCardIndex[i] >= 0) {
                struct BurnRomInfo ri;
                memset(&ri, 0, sizeof(ri));
                if (BurnDrvGetRomInfo(&ri, Pgm2PerSlotCardIndex[i]) == 0 && ri.nLen == PGM2_CARD_SIZE) {
                    if (BurnLoadRom(Pgm2Cards[i], Pgm2PerSlotCardIndex[i], 1) == 0) {
                        PGM2_LOG(PGM2_LOG_CARD, _T("CARD init: loaded slot %d from rom index=%d len=%d"), i, Pgm2PerSlotCardIndex[i], ri.nLen);
                    }
                }
            }
        }
        // Load slot 0 template (if not already loaded per-slot)
        if (Pgm2PerSlotCardIndex[0] < 0) {
            struct BurnRomInfo ri;
            memset(&ri, 0, sizeof(ri));
            if (BurnDrvGetRomInfo(&ri, Pgm2CardRomIndex) == 0 && ri.nLen == PGM2_CARD_SIZE) {
                if (BurnLoadRom(Pgm2Cards[0], Pgm2CardRomIndex, 1) == 0) {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD init: loaded default card rom index=%d len=%d"), Pgm2CardRomIndex, ri.nLen);
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD init: data[0..7]=%02X %02X %02X %02X %02X %02X %02X %02X"),
                        Pgm2Cards[0][0], Pgm2Cards[0][1], Pgm2Cards[0][2], Pgm2Cards[0][3],
                        Pgm2Cards[0][4], Pgm2Cards[0][5], Pgm2Cards[0][6], Pgm2Cards[0][7]);
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD init: prot=%02X %02X %02X %02X sec=%02X %02X %02X %02X"),
                        Pgm2Cards[0][0x100], Pgm2Cards[0][0x101], Pgm2Cards[0][0x102], Pgm2Cards[0][0x103],
                        Pgm2Cards[0][0x104], Pgm2Cards[0][0x105], Pgm2Cards[0][0x106], Pgm2Cards[0][0x107]);
                } else {
                    PGM2_LOG(PGM2_LOG_CARD, _T("CARD init: default card rom index=%d not present, keeping blank card"), Pgm2CardRomIndex);
                    Pgm2Cards[0][PGM2_CARD_DATA_SIZE + 4] = 0x07;
                }
            } else {
                PGM2_LOG(PGM2_LOG_CARD, _T("CARD init: no default card configured for rom index=%d riLen=%d, keeping blank card"), Pgm2CardRomIndex, ri.nLen);
                Pgm2Cards[0][PGM2_CARD_DATA_SIZE + 4] = 0x07;
            }
        }
        // Copy slot 0 template only to slots that weren't loaded per-slot
        for (int i = 1; i < PGM2_NUM_CARD_SLOTS; i++) {
            if (Pgm2PerSlotCardIndex[i] < 0 && Pgm2Cards[i]) {
                memcpy(Pgm2Cards[i], Pgm2Cards[0], PGM2_CARD_SIZE);
            }
        }
    }

    if (Pgm2ExtRAM && Pgm2SramRomIndex >= 0) {
        struct BurnRomInfo ri;
        memset(&ri, 0, sizeof(ri));
        if (BurnDrvGetRomInfo(&ri, Pgm2SramRomIndex) == 0 && ri.nLen > 0 && ri.nLen <= 0x10000) {
            if (BurnLoadRom(Pgm2ExtRAM, Pgm2SramRomIndex, 1) == 0) {
                PGM2_LOG(PGM2_LOG_SYS, "loaded optional sram rom index=%d len=%d", Pgm2SramRomIndex, ri.nLen);
            } else {
                PGM2_LOG(PGM2_LOG_SYS, "optional sram rom index=%d not present, using zero-filled sram", Pgm2SramRomIndex);
            }
        }
    }

    // KOV3 module XOR is NOT applied at init time.
    // On real hardware, the FPGA starts locked; the boot ROM checks the ORIGINAL
    // ROM data for integrity. The boot ROM then triggers IGS036 decrypt, and
    // later the game code writes dec_val to unlock the module XOR.
    // This matches MAME's machine_reset flow (m_has_decrypted_kov3_module = false).

    // Save encrypted backup of ARM9 ROM for reset (internal boot ROM re-decrypts on each reset)
    if (Pgm2ArmROM && Pgm2ArmROMLen > 0) {
        Pgm2ArmROMEncrypted = (UINT8*)BurnMalloc(Pgm2ArmROMLen);
        if (Pgm2ArmROMEncrypted)
            memcpy(Pgm2ArmROMEncrypted, Pgm2ArmROM, Pgm2ArmROMLen);
    }

    Pgm2HasDecrypted = 0;

    // Initialize KOV3 module clock counter to 151 (matches MAME machine_reset).
    // This skips the false clock pulse that occurs during GPIO initialization.
    if (Pgm2HasKov3Module) 
	{
        Pgm2ModuleClkCnt = 151;
    }

    // Patch the internal boot ROM delay loop at 0x27F0-0x27FC.
    // The delay function spins R0 down to zero. Some callers enter at 0x27F0,
    // others at 0x27F4 (bypassing the first instruction).
    // Patch both the entry NOP and the BNE branch to guarantee immediate exit.
    if (Pgm2IntROM && Pgm2IntROMLen > 0x2800) {
        UINT32 insn0 = *(UINT32*)(Pgm2IntROM + 0x27F0);
        UINT32 insn3 = *(UINT32*)(Pgm2IntROM + 0x27FC);
        if (insn0 == 0xE1A00000) {  // Verify entry is NOP
            Pgm2IntRomOriginal27F0 = insn0;
            *(UINT32*)(Pgm2IntROM + 0x27F0) = 0xE3A00000;  // MOV R0, #0
            PGM2_LOG(PGM2_LOG_SYS, "patched delay entry at 0x27F0: NOP -> MOV R0, #0");
        }
        if (insn3 == 0x1AFFFFFC) {  // Verify BNE 0x27F4
            Pgm2IntRomOriginal27FC = insn3;
            *(UINT32*)(Pgm2IntROM + 0x27FC) = 0xE1A00000;  // NOP
            PGM2_LOG(PGM2_LOG_SYS, "patched delay branch at 0x27FC: BNE -> NOP");
        }
    }

    // Init ARM9 CPU
    Arm9Init(0);
    Arm9Open(0);

    // Map internal boot ROM at 0x00000000 (ARM9 reset vector)
    if (Pgm2IntROM && Pgm2IntROMLen > 0) {
        Arm9MapMemory(Pgm2IntROM, 0x00000000, (UINT32)(Pgm2IntROMLen - 1), MAP_ROM);
        // Dump exception vector table for debugging
        if (Pgm2IntROMLen >= 0x20) {
            PGM2_LOG(PGM2_LOG_SYS, "exception vectors: [00]=%08X [04]=%08X [08]=%08X [0C]=%08X [10]=%08X [14]=%08X [18]=%08X [1C]=%08X",
                *(UINT32*)(Pgm2IntROM + 0x00), *(UINT32*)(Pgm2IntROM + 0x04),
                *(UINT32*)(Pgm2IntROM + 0x08), *(UINT32*)(Pgm2IntROM + 0x0C),
                *(UINT32*)(Pgm2IntROM + 0x10), *(UINT32*)(Pgm2IntROM + 0x14),
                *(UINT32*)(Pgm2IntROM + 0x18), *(UINT32*)(Pgm2IntROM + 0x1C));
        }
    }
    // External Flash ROM at 0x10000000 — map full 16MB buffer as MAP_ROM.
    // MAME: map(0x10000000, 0x10ffffff).rom().region("mainrom", 0)  (full 16MB region)
    // The full buffer (not just the ROM file portion) must be mapped because module XOR
    // operates on the entire buffer and the boot ROM may read from the upper region.
    if (Pgm2ArmROM && Pgm2ArmROMLen > 0) {
        UINT32 romEnd = 0x10000000 + (UINT32)Pgm2ArmROMLen - 1;
        if (romEnd > 0x10FFFFFF) romEnd = 0x10FFFFFF;

        if (Pgm2HasKov3Module) {
            // For KOV3: the first page (0x10000000-0x10000FFF) must NOT be mapped
            // for READ, so that data reads go through the handler where pgm2ModuleRomR
            // can intercept checksum reads (offsets 1-4). Map it as FETCH only.
            // MAME: map(0x10000000, 0x107fffff).r(module_rom_r).w(module_rom_w)
            //   — both reads AND writes go through handlers in MAME.
            Arm9MapMemory(Pgm2ArmROM, 0x10000000, 0x10000FFF, (1 << 2)); // FETCH only
            Arm9MapMemory(Pgm2ArmROM + 0x1000, 0x10001000, romEnd, MAP_ROM);
            PGM2_LOG(PGM2_LOG_SYS, "mapped ext ROM: 0x10000000-0x10000FFF FETCH-only, 0x10001000-0x%08X MAP_ROM", romEnd);
        } else if (Pgm2RomBoardRAMSize > 0 && Pgm2RomBoardRAM) {
            // RAM/ROM board (ddpdojt): writable RAM at 0x10000000, ROM at 0x10000000+ramSize
            UINT32 ramEnd = 0x10000000 + (UINT32)Pgm2RomBoardRAMSize - 1;
            UINT32 romStart = 0x10000000 + (UINT32)Pgm2RomBoardRAMSize;
            UINT32 romMapEnd = romStart + (UINT32)Pgm2ArmROMLen - 1;
            if (romMapEnd > 0x10FFFFFF) romMapEnd = 0x10FFFFFF;
            Arm9MapMemory(Pgm2RomBoardRAM, 0x10000000, ramEnd, MAP_RAM);
            Arm9MapMemory(Pgm2ArmROM, romStart, romMapEnd, MAP_ROM);
            PGM2_LOG(PGM2_LOG_SYS, "mapped RAM/ROM board: RAM 0x10000000-0x%08X, ROM 0x%08X-0x%08X", ramEnd, romStart, romMapEnd);
        } else {
            Arm9MapMemory(Pgm2ArmROM, 0x10000000, romEnd, MAP_ROM);
            PGM2_LOG(PGM2_LOG_SYS, "mapped ext ROM at 0x10000000-0x%08X as MAP_ROM (buffer=%08X)", romEnd, Pgm2ArmROMLen);
        }
    }
    // Map main RAM at 0x20000000.
    // Reads from directly mapped pages bypass pgm2ReadLongDirect, so the
    // speedhack page must remain read-unmapped or the busy-loop interception
    // never triggers.
    if (Pgm2SpeedHackAddr >= 0x20000000 && Pgm2SpeedHackAddr <= 0x2007FFFF) {
        const UINT32 ramStart = 0x20000000;
        const UINT32 ramEnd = 0x2007FFFF;
        const UINT32 hackPageStart = Pgm2SpeedHackAddr & ~0xFFF;
        const UINT32 hackPageEnd = hackPageStart + 0xFFF;

        if (hackPageStart > ramStart) {
            Arm9MapMemory(Pgm2ArmRAM, ramStart, hackPageStart - 1, MAP_RAM);
        }
        // Map the speedhack page as WRITE-only so reads go through the callback
        Arm9MapMemory(Pgm2ArmRAM + (hackPageStart - ramStart), hackPageStart, hackPageEnd, MAP_WRITE);
        if (hackPageEnd < ramEnd) {
            Arm9MapMemory(Pgm2ArmRAM + (hackPageEnd + 1 - ramStart), hackPageEnd + 1, ramEnd, MAP_RAM);
        }
        PGM2_LOG(PGM2_LOG_SYS, "speedhack active addr=%08X pcs=%08X,%08X,%08X,%08X page=%08X-%08X read-unmapped",
            Pgm2SpeedHackAddr,
            Pgm2SpeedHackPC[0], Pgm2SpeedHackPC[1], Pgm2SpeedHackPC[2], Pgm2SpeedHackPC[3],
            hackPageStart, hackPageEnd);
    } else {
        Arm9MapMemory(Pgm2ArmRAM, 0x20000000, 0x2007FFFF, MAP_RAM);
    }
    // Map SRAM at 0x02000000
    Arm9MapMemory(Pgm2ExtRAM, 0x02000000, 0x0200FFFF, MAP_RAM);

    // Map video RAM directly for performance (boot ROM fills FG videoram extensively)
    Arm9MapMemory(Pgm2SpOAM,  0x30000000, 0x30001FFF, MAP_RAM);  // Sprite OAM 8KB
    Arm9MapMemory(Pgm2BgVRAM, 0x30020000, 0x30021FFF, MAP_RAM);  // BG videoram 8KB
    Arm9MapMemory(Pgm2FgVRAM, 0x30040000, 0x30045FFF, MAP_RAM);  // FG videoram 24KB
    Arm9MapMemory((UINT8*)Pgm2SpPal, 0x30060000, 0x30063FFF, MAP_RAM); // Sprite palette 16KB
    Arm9MapMemory((UINT8*)Pgm2BgPal, 0x30080000, 0x30081FFF, MAP_RAM); // BG palette 8KB

    // All other I/O goes through callbacks
    Arm9SetReadByteHandler(pgm2ReadByte);
    Arm9SetReadWordHandler(pgm2ReadWord);
    Arm9SetReadLongHandler(pgm2ReadLong);
    Arm9SetWriteByteHandler(pgm2WriteByte);
    Arm9SetWriteWordHandler(pgm2WriteWord);
    Arm9SetWriteLongHandler(pgm2WriteLong);

    Arm9Reset();
    Arm9Close();

    // Init YMZ774 sound
    if (Pgm2SndROM && Pgm2SndROMLen > 0) {
        ymz774_init(Pgm2SndROM, Pgm2SndROMLen);
        ymz770_set_buffered(pgm2CpuCycles, 100000000);
    }

    // Init video
    pgm2InitDraw();

    return 0;
}
INT32 pgm2DoReset()
{
	if (Pgm2ArmROMEncrypted && Pgm2ArmROM)
		memcpy(Pgm2ArmROM, Pgm2ArmROMEncrypted, Pgm2ArmROMLen);

	Pgm2HasDecryptedKov3Module = 0;
	Pgm2HasDecrypted = 0;
	Pgm2ModuleClkCnt = 151; // MAME: prevents false clock pulse during GPIO init
	Pgm2ModulePrevState = 0;
	Pgm2ModuleInLatch = 0;
	Pgm2ModuleOutLatch = 0;
	Pgm2ModuleSumRead = 0;
	Pgm2PioOutData = 0;
	memset(Pgm2ModuleRcvBuf, 0, sizeof(Pgm2ModuleRcvBuf));
	memset(Pgm2ModuleSendBuf, 0, sizeof(Pgm2ModuleSendBuf));
	Arm9Open(0);
	Arm9Reset();
	Arm9Close();
	ymz770_reset();
	Pgm2Reset = 0;
	Pgm2RtcFrameCounter = 0;
	HiscoreReset();
	if(pPgm2ResetCallback)
		pPgm2ResetCallback();
	return 0;
}
INT32 pgm2Exit()
{
    Arm9Open(0);
    Arm9Exit();

    ymz770_exit();
    pgm2ExitDraw();

    BurnFree(Pgm2IntROM);   Pgm2IntROM  = NULL;
    BurnFree(Pgm2ArmROM);   Pgm2ArmROM  = NULL;
    BurnFree(Pgm2ArmROMEncrypted); Pgm2ArmROMEncrypted = NULL;
    BurnFree(Pgm2ArmRAM);   Pgm2ArmRAM  = NULL;
    BurnFree(Pgm2SprROM);   Pgm2SprROM  = NULL;
    BurnFree(Pgm2TileROM);  Pgm2TileROM = NULL;
    BurnFree(Pgm2SndROM);   Pgm2SndROM  = NULL;
    BurnFree(Pgm2VidRAM);   Pgm2VidRAM  = NULL;
    BurnFree(Pgm2ExtRAM);   Pgm2ExtRAM  = NULL;
    for (int i = 0; i < PGM2_NUM_CARD_SLOTS; i++) {
        BurnFree(Pgm2Cards[i]); Pgm2Cards[i] = NULL;
    }
    BurnFree(Pgm2RomBoardRAM); Pgm2RomBoardRAM = NULL;
    // Pgm2PalRAM / Pgm2SharedRAM point inside VidRAM, already freed above
    Pgm2PalRAM    = NULL;
    Pgm2SharedRAM = NULL;

    // Clear sub-pointers
    Pgm2SpOAM = Pgm2BgVRAM = Pgm2FgVRAM = Pgm2LineRAM = Pgm2SharedRAM2 = NULL;
    Pgm2SpPal = Pgm2BgPal = Pgm2TxPal = Pgm2SpZoom = Pgm2VideoRegs = NULL;

    PGM2_LOG(PGM2_LOG_SYS, "==== PGM2 exit ====");

    pPgm2InitCallback = NULL;
    pPgm2ResetCallback = NULL;
    pPgm2ScanCallback = NULL;
    Pgm2CardRomIndex = -1;
    for (int i = 0; i < 4; i++) Pgm2PerSlotCardIndex[i] = -1;
    memset(Pgm2CardAuthenticated, 0, sizeof(Pgm2CardAuthenticated));
    Pgm2SramRomIndex = 10;
    Pgm2MaxCardSlots = 0;
    Pgm2ActiveCardSlot = 0;

    // Reset speed hack
    Pgm2SpeedHackAddr = 0;
    memset(Pgm2SpeedHackPC, 0, sizeof(Pgm2SpeedHackPC));

    // Reset RAM/ROM board
    Pgm2RomBoardRAMSize = 0;
    Pgm2DecryptWordOffset = 0;
    Pgm2RefreshRate = 59.08;  // reset to default

    // Reset sprite layout
    Pgm2MaskROMOffset = 0;
    Pgm2MaskROMLen = 0;
    Pgm2ColourROMOffset = 0;

    // Reset kov3 module state
    Pgm2HasKov3Module = 0;
    Pgm2HasDecryptedKov3Module = 0;
    Pgm2ModuleKey = NULL;
    Pgm2ModuleSum = NULL;
    Pgm2ModuleAddrXor = 0;
    Pgm2ModuleDataXor = 0;
    Pgm2ModuleClkCnt = 0;
    Pgm2ModulePrevState = 0;
    Pgm2ModuleInLatch = 0;
    Pgm2ModuleOutLatch = 0;
    Pgm2ModuleSumRead = 0;
    Pgm2PioOutData = 0;
    memset(Pgm2ModuleRcvBuf, 0, sizeof(Pgm2ModuleRcvBuf));
    memset(Pgm2ModuleSendBuf, 0, sizeof(Pgm2ModuleSendBuf));

    return 0;
}

// ---------------------------------------------------------------------------
// Pack per-bit input arrays into byte registers for hardware read
static void pgm2MakeInputs()
{
    memset(Pgm2Input, 0, sizeof(Pgm2Input));
    for (INT32 i = 0; i < 32; i++) {
        Pgm2Input[i / 8] |= (Pgm2InputPort0[i] & 1) << (i & 7);
    }
    for (INT32 i = 0; i < 24; i++) {
        Pgm2Input[4 + i / 8] |= (Pgm2InputPort1[i] & 1) << (i & 7);
    }
    // DIP switches: pre-invert because the read handler applies ~ to the
    // whole 32-bit value.  ~(~Pgm2Dip[0]) restores the correct polarity.
    Pgm2Input[7] = ~Pgm2Dip[0];
}

// pgm2Frame
// ---------------------------------------------------------------------------
INT32 pgm2Frame()
{
    if (Pgm2Reset) 
	{
        pgm2DoReset();
    }

    pgm2MakeInputs();

    Pgm2RtcFrameCounter++;

    // Boot PC diagnostic: log to file so we can see output even if game crashes
    // Log first 10 frames, then every 100th frame, plus detect key transitions
    if (PGM2_LOG_ENABLED)
    {
        static UINT32 prevPcRegion = 0xFFFFFFFF;
        Arm9Open(0);
        UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
        UINT32 cpsr = Arm9DbgGetCPSR();
        UINT32 lr = Arm9DbgGetRegister(eR14);
        UINT32 pcRegion = pc >> 24; // 0=intROM, 0x10=extROM, 0x20=RAM, etc.
        bool logThis = (Pgm2RtcFrameCounter <= 10) ||
                       (Pgm2RtcFrameCounter % 100 == 0) ||
                       (pcRegion != prevPcRegion) ||
                       (pc >= 0x3C40 && pc <= 0x3C50) ||
                       (pc >= 0x3760 && pc <= 0x3770);
        if (logThis) {
            UINT32 sp_irq = Arm9DbgGetRegister(eR13_IRQ);
            UINT32 lr_irq = Arm9DbgGetRegister(eR14_IRQ);
            UINT32 sp_svc = Arm9DbgGetRegister(eR13_SVC);
            PGM2_LOG(PGM2_LOG_FRAME, "frame %d START pc=%08X cpsr=%08X lr=%08X aicEn=%08X aicPend=%08X lvl=%d sp_irq=%08X lr_irq=%08X sp_svc=%08X",
                Pgm2RtcFrameCounter, pc, cpsr, lr,
                Pgm2AicEnabled, Pgm2AicPending, pgm2AicGetLevel(),
                sp_irq, lr_irq, sp_svc);
        }
        prevPcRegion = pcRegion;
        Arm9Close();
    }

    // 100 MHz ARM9 / Pgm2RefreshRate fps / 264 total lines (224 active + 40 vblank)
    static const INT32 PGM2_TOTAL_LINES  = 264;
    static const INT32 PGM2_VBLANK_START = 224;
    static const INT32 PGM2_CPU_HZ       = 100000000;
    INT32 nCyclesPerLine = (INT32)((double)PGM2_CPU_HZ / ((double)PGM2_TOTAL_LINES * Pgm2RefreshRate));

    Arm9Open(0);
    Arm9NewFrame();

    for (INT32 i = 0; i < PGM2_TOTAL_LINES; i++) {
        Arm9Run(nCyclesPerLine);
        // We approximate with scanline countdowns.  When the countdown expires
        // we assert IRQ3 — matching MAME's mcu_interrupt() callback.
        if (Pgm2McuDoneCountdown > 0) {
            Pgm2McuDoneCountdown--;
            if (Pgm2McuDoneCountdown == 0) {
                Pgm2McuIrq3 = 1;
                pgm2AicSetIrq(3, 1);
                PGM2_LOG(PGM2_LOG_MCU, "IRQ3 fire frame=%d status=%08X", Pgm2RtcFrameCounter, Pgm2McuRegs[3]);
                PGM2_LOG(PGM2_LOG_SYS, "MCU timer-fire IRQ3 assert status=%08X result0=%08X result1=%08X", Pgm2McuRegs[3], Pgm2McuResult0, Pgm2McuResult1);
            }
        }

        // MCU IRQ3 is NOT periodic — it fires only in response to MCU commands.
        // MAME's m_mcu_timer is a one-shot timer started by mcu_command() and
        // fires mcu_interrupt() which asserts IRQ3.  We approximate the delay
        // with scanline countdowns; IRQ3 fires only when the countdown expires.

        // VBLANK IRQ handling:
        // MAME: IRQ12 is cleared ONLY by game writing to 0x30120018 (vbl_ack_w).
        // However, our AIC pgm2AicSetIrq(12,0) only clears pending if SMR bit 6
        // (level-sensitive) is set.  If the game configured IRQ12 as edge-triggered,
        // the vbl_ack_w path won't clear the pending bit correctly.
        // Workaround: also clear at line 0 so the interrupt doesn't stick forever.
        if (i == 0) {
            Pgm2AicPending &= ~(1u << 12);
            pgm2AicCheckIrqs();
        }
        if (i == PGM2_VBLANK_START) {
            // Snapshot OAM at vblank start (matches MAME's sprite buffer copy)
            pgm2SnapshotOam();
            // Snapshot scroll/lineram BEFORE the IRQ fires, so pgm2DoDraw
            // renders with the same values MAME's screen_update would see
            // (i.e., before the vblank handler updates them for next frame).
            pgm2SnapshotScroll();
            pgm2AicSetIrq(12, 1);
        }
    }

    if (Pgm2HasDecrypted) 
	{
        Pgm2PostDecryptFrameCount++;
    }

    // Boot diagnostic: frame completed
    {
        UINT32 pc = Arm9DbgGetPC() & 0x7FFFFFFF;
        bool logThis = (Pgm2RtcFrameCounter <= 10) ||
                       (Pgm2RtcFrameCounter % 100 == 0);
        if (logThis) {
            PGM2_LOG(PGM2_LOG_FRAME, "frame %d END pc=%08X", Pgm2RtcFrameCounter, pc);
        }
        // One-time video memory check
        if (Pgm2RtcFrameCounter == 500 || Pgm2RtcFrameCounter == 1500 || Pgm2RtcFrameCounter == 3000) {
            // Check sprite OAM for non-zero
            INT32 sprNonZero = 0;
            for (INT32 j = 0; j < 0x2000; j += 4) {
                if (*(UINT32*)(Pgm2SpOAM + j) != 0) sprNonZero++;
            }
            // Check BG VRAM for non-zero
            INT32 bgNonZero = 0;
            for (INT32 j = 0; j < 0x2000; j += 4) {
                if (*(UINT32*)(Pgm2BgVRAM + j) != 0) bgNonZero++;
            }
            // Check FG VRAM for non-zero
            INT32 fgNonZero = 0;
            for (INT32 j = 0; j < 0x6000; j += 4) {
                if (*(UINT32*)(Pgm2FgVRAM + j) != 0) fgNonZero++;
            }
            // Check sprite palette
            INT32 spPalNonZero = 0;
            for (INT32 j = 0; j < 0x4000; j += 4) {
                if (*(UINT32*)((UINT8*)Pgm2SpPal + j) != 0) spPalNonZero++;
            }
            // Check main RAM usage
            INT32 ramNonZero = 0;
            for (INT32 j = 0; j < 0x80000; j += 4) {
                if (*(UINT32*)(Pgm2ArmRAM + j) != 0) ramNonZero++;
            }
            // Video regs
            UINT32 *vr = (UINT32*)Pgm2VideoRegs;
            PGM2_LOG(PGM2_LOG_DIAG, "frame %d: sprOAM_nz=%d bgVRAM_nz=%d fgVRAM_nz=%d spPal_nz=%d mainRAM_nz=%d",
                Pgm2RtcFrameCounter, sprNonZero, bgNonZero, fgNonZero, spPalNonZero, ramNonZero);
            PGM2_LOG(PGM2_LOG_DIAG, "videoRegs: %08X %08X %08X %08X %08X %08X %08X %08X",
                vr[0], vr[1], vr[2], vr[3], vr[4], vr[5], vr[6], vr[7]);
            PGM2_LOG(PGM2_LOG_DIAG, "videoRegs+: %08X %08X %08X %08X %08X %08X %08X %08X",
                vr[8], vr[9], vr[10], vr[11], vr[12], vr[13], vr[14], vr[15]);
        }
    }

    Arm9Close();

    // Render after all scanlines (using snapshotted OAM from vblank start)
    if (pBurnDraw) {
        pgm2DoDraw();
    }

    // Update audio
    if (pBurnSoundOut)
        ymz770_update(pBurnSoundOut, nBurnSoundLen);

    return 0;
}

// ---------------------------------------------------------------------------
// pgm2Scan - state save/load
// ---------------------------------------------------------------------------
INT32 pgm2Scan(INT32 nAction, INT32 *pnMin)
{
    struct BurnArea ba;

    if (pnMin) *pnMin = 0x029702;

    if (nAction & ACB_MEMORY_RAM) {
        memset(&ba, 0, sizeof(ba));

        ba.Data   = Pgm2ArmRAM;
        ba.nLen   = 0x80000;
        ba.szName = "Main RAM";
        BurnAcb(&ba);

        ba.Data   = Pgm2VidRAM;
        ba.nLen   = PGM2_VIDRAM_SIZE;
        ba.szName = "Video RAM";
        BurnAcb(&ba);

        ba.Data   = Pgm2ExtRAM;
        ba.nLen   = 0x10000;
        ba.szName = "Battery SRAM";
        BurnAcb(&ba);

        if (Pgm2RomBoardRAM && Pgm2RomBoardRAMSize > 0) {
            ba.Data   = Pgm2RomBoardRAM;
            ba.nLen   = Pgm2RomBoardRAMSize;
            ba.szName = "ROM Board RAM";
            BurnAcb(&ba);
        }
    }

    // Auto-save/load Battery SRAM (battery-backed, persists on real hardware)
    if (nAction & ACB_NVRAM) {
        memset(&ba, 0, sizeof(ba));
        ba.Data   = Pgm2ExtRAM;
        ba.nLen   = 0x10000;
        ba.szName = "Battery SRAM NV";
        BurnAcb(&ba);
    }

    // Auto-save/load IC card data via NVRAM mechanism (4 slots)
    if ((nAction & ACB_NVRAM) && Pgm2CardRomIndex >= 0) {
        char* cardNames[4] = {
            (char*)"IC Card P1", (char*)"IC Card P2", (char*)"IC Card P3", (char*)"IC Card P4"
        };
        for (int i = 0; i < PGM2_NUM_CARD_SLOTS; i++) {
            if (Pgm2Cards[i]) {
                memset(&ba, 0, sizeof(ba));
                ba.Data   = Pgm2Cards[i];
                ba.nLen   = PGM2_CARD_SIZE;
                ba.szName = cardNames[i];
                BurnAcb(&ba);
            }
        }
    }

    if ((nAction & ACB_MEMCARD) && Pgm2CardRomIndex >= 0) {
        INT32 slot = Pgm2ActiveCardSlot;
        if (slot < 0 || slot >= PGM2_NUM_CARD_SLOTS) slot = 0;

        // Handle card insertion/ejection actions for active slot
        if (nAction & ACB_MEMCARD_ACTION) {
            if (nAction & ACB_WRITE) {
                if (!Pgm2Cards[slot]) {
                    Pgm2Cards[slot] = (UINT8*)BurnMalloc(PGM2_CARD_SIZE);
                }
                Pgm2CardAuthenticated[slot] = false;
                Pgm2CardInserted[slot] = true;  // mark card as present
            }
            if (nAction & ACB_READ) {
                Pgm2CardAuthenticated[slot] = false;
                Pgm2CardInserted[slot] = false;  // mark card as ejected
            }
        }

        // Report active slot card area (for MemCardGetSize and menu operations)
        memset(&ba, 0, sizeof(ba));
        if (Pgm2Cards[slot]) {
            ba.Data = Pgm2Cards[slot];
        }
        ba.nLen   = PGM2_CARD_SIZE;
        ba.szName = "PGM2 IC Card";
        BurnAcb(&ba);

        // Fix blank cards after insert
        if ((nAction & ACB_MEMCARD_ACTION) && (nAction & ACB_WRITE) && Pgm2Cards[slot]) {
            bool allZero = true;
            for (int i = 0; i < PGM2_CARD_SIZE; i++) {
                if (Pgm2Cards[slot][i] != 0) { allZero = false; break; }
            }
            if (allZero) {
                memset(Pgm2Cards[slot], 0xFF, PGM2_CARD_SIZE);
                Pgm2Cards[slot][PGM2_CARD_DATA_SIZE + 4] = 0x07;
            }
        }
    }

    if (nAction & ACB_DRIVER_DATA) {
        Arm9Open(0);
        Arm9Scan(nAction);
        Arm9Close();

        ymz770_scan(nAction, pnMin);

        SCAN_VAR(Pgm2McuRegs);
        SCAN_VAR(Pgm2McuResult0);
        SCAN_VAR(Pgm2McuResult1);
        SCAN_VAR(Pgm2McuLastCmd);
        SCAN_VAR(Pgm2ShareBank);
        SCAN_VAR(Pgm2McuIrq3);
        SCAN_VAR(Pgm2McuDoneCountdown);
        SCAN_VAR(Pgm2CardAuthenticated);
        SCAN_VAR(Pgm2CardInserted);

        SCAN_VAR(Pgm2AicSmr);
        SCAN_VAR(Pgm2AicSvr);
        SCAN_VAR(Pgm2AicPending);
        SCAN_VAR(Pgm2AicEnabled);
        SCAN_VAR(Pgm2AicSpuIvr);
        SCAN_VAR(Pgm2AicFastIrqs);
        SCAN_VAR(Pgm2AicDebug);
        SCAN_VAR(Pgm2AicStatus);
        SCAN_VAR(Pgm2AicCoreStatus);
        SCAN_VAR(Pgm2AicLevelStack);
        SCAN_VAR(Pgm2AicLvlIdx);

        SCAN_VAR(Pgm2HasDecrypted);
        SCAN_VAR(Pgm2EncryptTable);

        if (Pgm2HasKov3Module) 
		{
            SCAN_VAR(Pgm2HasDecryptedKov3Module);
            SCAN_VAR(Pgm2ModuleClkCnt);
            SCAN_VAR(Pgm2ModulePrevState);
            SCAN_VAR(Pgm2ModuleInLatch);
            SCAN_VAR(Pgm2ModuleOutLatch);
            SCAN_VAR(Pgm2ModuleRcvBuf);
            SCAN_VAR(Pgm2ModuleSendBuf);
            SCAN_VAR(Pgm2ModuleSumRead);
            SCAN_VAR(Pgm2PioOutData);
        }
    }

    // After loading a savestate, restore encrypted ROM and re-decrypt if needed
    // (MAME: device_post_load — module XOR first, then table decrypt)
    if (nAction & ACB_WRITE) {
        if (Pgm2ArmROMEncrypted && Pgm2ArmROM)
            memcpy(Pgm2ArmROM, Pgm2ArmROMEncrypted, Pgm2ArmROMLen);
        if (Pgm2HasDecryptedKov3Module) {
            pgm2DecryptKov3Module(Pgm2ModuleAddrXor, Pgm2ModuleDataXor);
        }
        if (Pgm2HasDecrypted) {
            if (Pgm2ArmROM && Pgm2ArmROMLen > 0) {
                INT32 decryptLen = (Pgm2ArmROMFileLen > 0) ? Pgm2ArmROMFileLen : Pgm2ArmROMLen;
                pgm2_igs036_decrypt(Pgm2ArmROM, decryptLen, Pgm2EncryptTable, Pgm2DecryptWordOffset);
            }
        }
    }

    if (pPgm2ScanCallback)
        pPgm2ScanCallback(nAction, pnMin);

    pgm2ScanDraw(nAction);

    return 0;
}
