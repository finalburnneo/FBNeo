#include "pgm.h"
#include "arm7_intf.h"
#include "bitswap.h"

static unsigned char *USER1;
static unsigned short *sharedprotram;

static int ptr;

int pstarsScan(int nAction,int */*pnMin*/);
int killbldScan(int nAction,int */*pnMin*/);
int kov_asic27Scan(int nAction,int */*pnMin*/);
int asic3Scan(int nAction,int */*pnMin*/);
int asic27AScan(int nAction,int *);
int oldsScan(int nAction, int */*pnMin*/);
int kovsh_asic27aScan(int nAction,int *);
int svg_asic27aScan(int nAction,int *);
int oldsplus_asic27aScan(int nAction, int */*pnMin*/);

//-----------------------------------------------------------------------------------------------------
// ASIC3 - Oriental Legends

static unsigned char asic3_reg, asic3_latch[3], asic3_x, asic3_y, asic3_z, asic3_h1, asic3_h2;
static unsigned short asic3_hold;

static unsigned int bt(unsigned int v, int bit)
{
        return (v & (1<<bit)) != 0;
}

static void asic3_compute_hold()
{
        // The mode is dependant on the region
        static int modes[4] = { 1, 1, 3, 2 };
        int mode = modes[PgmInput[7] & 3];

        switch(mode) {
        case 1:
                asic3_hold =
                        (asic3_hold << 1)
                        ^0x2bad
                        ^bt(asic3_hold, 15)^bt(asic3_hold, 10)^bt(asic3_hold, 8)^bt(asic3_hold, 5)
                        ^bt(asic3_z, asic3_y)
                        ^(bt(asic3_x, 0) << 1)^(bt(asic3_x, 1) << 6)^(bt(asic3_x, 2) << 10)^(bt(asic3_x, 3) << 14);
                break;
        case 2:
                asic3_hold =
                        (asic3_hold << 1)
                        ^0x2bad
                        ^bt(asic3_hold, 15)^bt(asic3_hold, 7)^bt(asic3_hold, 6)^bt(asic3_hold, 5)
                        ^bt(asic3_z, asic3_y)
                        ^(bt(asic3_x, 0) << 4)^(bt(asic3_x, 1) << 6)^(bt(asic3_x, 2) << 10)^(bt(asic3_x, 3) << 12);
                break;
        case 3:
                asic3_hold =
                        (asic3_hold << 1)
                        ^0x2bad
                        ^bt(asic3_hold, 15)^bt(asic3_hold, 10)^bt(asic3_hold, 8)^bt(asic3_hold, 5)
                        ^bt(asic3_z, asic3_y)
                        ^(bt(asic3_x, 0) << 4)^(bt(asic3_x, 1) << 6)^(bt(asic3_x, 2) << 10)^(bt(asic3_x, 3) << 12);
                break;
        }
}

static unsigned char pgm_asic3_r()
{
        unsigned char res = 0;
        /* region is supplied by the protection device */
        switch(asic3_reg) {
        case 0x00: res = (asic3_latch[0] & 0xf7) | ((PgmInput[7] << 3) & 0x08); break;
        case 0x01: res = asic3_latch[1]; break;
        case 0x02: res = (asic3_latch[2] & 0x7f) | ((PgmInput[7] << 6) & 0x80); break;
        case 0x03:
                res = (bt(asic3_hold, 15) << 0)
                        | (bt(asic3_hold, 12) << 1)
                        | (bt(asic3_hold, 13) << 2)
                        | (bt(asic3_hold, 10) << 3)
                        | (bt(asic3_hold,  7) << 4)
                        | (bt(asic3_hold,  9) << 5)
                        | (bt(asic3_hold,  2) << 6)
                        | (bt(asic3_hold,  5) << 7);
                break;
        case 0x20: res = 0x49; break;
        case 0x21: res = 0x47; break;
        case 0x22: res = 0x53; break;
        case 0x24: res = 0x41; break;
        case 0x25: res = 0x41; break;
        case 0x26: res = 0x7f; break;
        case 0x27: res = 0x41; break;
        case 0x28: res = 0x41; break;
        case 0x2a: res = 0x3e; break;
        case 0x2b: res = 0x41; break;
        case 0x2c: res = 0x49; break;
        case 0x2d: res = 0xf9; break;
        case 0x2e: res = 0x0a; break;
        case 0x30: res = 0x26; break;
        case 0x31: res = 0x49; break;
        case 0x32: res = 0x49; break;
        case 0x33: res = 0x49; break;
        case 0x34: res = 0x32; break;
        }
        return res;
}

static void pgm_asic3_w(unsigned short data)
{
        {
                if(asic3_reg < 3)
                        asic3_latch[asic3_reg] = data << 1;
                else if(asic3_reg == 0xa0) {
                        asic3_hold = 0;
                } else if(asic3_reg == 0x40) {
                        asic3_h2 = asic3_h1;
                        asic3_h1 = data;
                } else if(asic3_reg == 0x48) {
                        asic3_x = 0;
                        if(!(asic3_h2 & 0x0a)) asic3_x |= 8;
                        if(!(asic3_h2 & 0x90)) asic3_x |= 4;
                        if(!(asic3_h1 & 0x06)) asic3_x |= 2;
                        if(!(asic3_h1 & 0x90)) asic3_x |= 1;
                } else if(asic3_reg >= 0x80 && asic3_reg <= 0x87) {
                        asic3_y = asic3_reg & 7;
                        asic3_z = data;
                        asic3_compute_hold();
                }
        }
}

static void pgm_asic3_reg_w(unsigned short data)
{
        asic3_reg = data & 0xff;
}

static void __fastcall asic3_write_word(unsigned int address, unsigned short data)
{
        if (address == 0xc04000) {
                pgm_asic3_reg_w(data);
                return;
        }

        if (address == 0xc0400e) {
                pgm_asic3_w(data);
                return;
        }
}

static unsigned short __fastcall asic3_read_word(unsigned int address)
{
        if (address == 0xc0400e) {
                return pgm_asic3_r();
        }

        return 0;
}

static void reset_asic3()
{
        asic3_latch[0] = asic3_latch[1] = asic3_latch[2] = 0;
        asic3_hold = asic3_reg = asic3_x = asic3_y, asic3_z = asic3_h1 = asic3_h2 = 0;
}

void install_protection_asic3_orlegend()
{
        pPgmScanCallback = asic3Scan;
        pPgmResetCallback = reset_asic3;

        SekOpen(0);
        SekMapHandler(4,        0xc04000, 0xc0400f, SM_READ | SM_WRITE);
                
        SekSetReadWordHandler(4, asic3_read_word);
        SekSetWriteWordHandler(4, asic3_write_word);
        SekClose();
}


//-----------------------------------------------------------------------------------------------------
// ASIC27 - Knights of Valour

// photo2yk bonus stage
static const unsigned int AETABLE[16]={0x00,0x0a,0x14,  0x01,0x0b,0x15,  0x02,0x0c,0x16};

//Not sure if BATABLE is complete
static const unsigned int BATABLE[0x40]= {
                0x00,0x29,0x2c,0x35,0x3a,0x41,0x4a,0x4e,        //0x00
                0x57,0x5e,0x77,0x79,0x7a,0x7b,0x7c,0x7d,        //0x08
                0x7e,0x7f,0x80,0x81,0x82,0x85,0x86,0x87,        //0x10
                0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x90,        //0x18
                0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,
                0x9e,0xa3,0xd4,0xa9,0xaf,0xb5,0xbb,0xc1 };

static const unsigned int B0TABLE[16]={2,0,1,4,3}; //maps char portraits to tables

static unsigned short ASIC27KEY;
static unsigned short ASIC27REGS[10];
static unsigned short ASICPARAMS[256];
static unsigned short ASIC27RCNT=0;
static unsigned int E0REGS[16];

static unsigned int photoy2k_seqpos;
static unsigned int photoy2k_trf[3], photoy2k_soff;

static unsigned int photoy2k_spritenum()
{
	UINT32 base = photoy2k_seqpos & 0xffc00;
	UINT32 low  = photoy2k_seqpos & 0x003ff;

	switch((photoy2k_seqpos >> 10) & 0xf) {
	case 0x0:
	case 0xa:
		return base | (BITSWAP16(low, 15,14,13,12,11,10, 0,8,3,1,5,9,4,2,6,7) ^ 0x124);
	case 0x1:
	case 0xb:
		return base | (BITSWAP16(low, 15,14,13,12,11,10, 5,1,7,4,0,8,3,6,9,2) ^ 0x088);
	case 0x2:
	case 0x8:
		return base | (BITSWAP16(low, 15,14,13,12,11,10, 3,5,9,7,6,4,1,8,2,0) ^ 0x011);
	case 0x3:
	case 0x9:
		return base | (BITSWAP16(low, 15,14,13,12,11,10, 1,8,3,6,0,4,5,2,9,7) ^ 0x154);
	case 0x4:
	case 0xe:
		return base | (BITSWAP16(low, 15,14,13,12,11,10, 2,1,7,4,5,8,3,6,9,0) ^ 0x0a9);
	case 0x5:
	case 0xf:
		return base | (BITSWAP16(low, 15,14,13,12,11,10, 9,4,6,8,2,1,7,5,3,0) ^ 0x201);
	case 0x6:
	case 0xd:
		return base | (BITSWAP16(low, 15,14,13,12,11,10, 4,6,0,8,9,7,3,5,1,2) ^ 0x008);
	case 0x7:
	case 0xc:
		return base | (BITSWAP16(low, 15,14,13,12,11,10, 8,9,3,2,0,1,6,7,5,4) ^ 0x000);
	}
	return 0;
}


static unsigned short kov_asic27_protram_r(unsigned short offset)
{
        if (offset == 8) return PgmInput[7];
        
        return 0x0000;
}

static unsigned short pgm_kov_asic27_r(unsigned short offset)
{
        unsigned int val=(ASIC27REGS[1]<<16)|(ASIC27REGS[0]);

        bprintf (0, _T("%2.2x\n"), ASIC27REGS[1]&0xff);

        switch(ASIC27REGS[1]&0xff) {

// photoy2k
		case 0x20: // PhotoY2k spritenum conversion 4/4
			val = photoy2k_soff >> 16;
			break;

		case 0x21: // PhotoY2k spritenum conversion 3/4
			if(!ASIC27RCNT) {
				#include "pgmy2ks.h"
				if(photoy2k_trf[0] < 0x3c00)
					photoy2k_soff = pgmy2ks[photoy2k_trf[0]];
				else
					photoy2k_soff = 0;
			}
			val = photoy2k_soff & 0xffff;
			break;

		case 0x22: // PhotoY2k spritenum conversion 2/4
			if(!ASIC27RCNT)	photoy2k_trf[1] = val & 0xffff;
			val = photoy2k_trf[0] | 0x880000;
			break;

		case 0x23: // PhotoY2k spritenum conversion 1/4
			if(!ASIC27RCNT) photoy2k_trf[0] = val & 0xffff;
			val = 0x880000;
			break;

		case 0x30: // PhotoY2k next element
			if(!ASIC27RCNT)	photoy2k_seqpos++;
			val = photoy2k_spritenum();
			break;

		case 0x32: // PhotoY2k start of sequence
			if(!ASIC27RCNT) photoy2k_seqpos = (val & 0xffff) << 4;
			val = photoy2k_spritenum();
			break;

		case 0xae:  //Photo Y2k Bonus stage
			val=AETABLE[ASIC27REGS[0]&0xf];
			break;



                case 0x67:
                        val=0x880000;
                        break;

                case 0x8e:
                        val=0x880000;
                        break;

                case 0x99:
                        val=0x880000;
                        break;
                case 0x9d:      // spr palette
                        val=0xa00000+((ASIC27REGS[0]&0x1f)<<6);
                        break;
                case 0xb0:
                        val=B0TABLE[ASIC27REGS[0]&0xf];
                        break;
                case 0xb4:
                        {
                                int v2=ASIC27REGS[0]&0x0f;
                                int v1=(ASIC27REGS[0]&0x0f00)>>8;
                                if(ASIC27REGS[0]==0x102)
                                        E0REGS[1]=E0REGS[0];
                                else
                                        E0REGS[v1]=E0REGS[v2];
                                val=0x880000;
                        }
                        break;
                case 0xba:
                        val=BATABLE[ASIC27REGS[0]&0x3f];
                        if(ASIC27REGS[0]>0x2f) {

                        }
                        break;
                case 0xc0:
                        val=0x880000;
                        break;
                case 0xc3:      //TXT tile position Uses C0 to select column
                        val=0x904000+(ASICPARAMS[0xc0]+ASICPARAMS[0xc3]*64)*4;
                        break;
                case 0xcb:
                        val=0x880000;
                        break;
                case 0xcc:      //BG
                        {
                                int y=ASICPARAMS[0xcc];
                        if(y&0x400)    //y is signed (probably x too and it also applies to TXT, but I've never seen it used)
                                y=-(0x400-(y&0x3ff));
                        val=0x900000+(((ASICPARAMS[0xcb]+(y)*64)*4)/*&0x1fff*/);
                        }
                        break;
                case 0xd0:      //txt palette
                        val=0xa01000+(ASIC27REGS[0]<<5);
                        break;
                case 0xd6:      //???? check it
                        {
                                int v2=ASIC27REGS[0]&0xf;
                                E0REGS[0]=E0REGS[v2];
                                //E0REGS[v2]=0;
                                val=0x880000;
                        }
                        break;
                case 0xdc:      //bg palette
                        val=0xa00800+(ASIC27REGS[0]<<6);
                        break;
                case 0xe0:      //spr palette
                        val=0xa00000+((ASIC27REGS[0]&0x1f)<<6);
                        break;
                case 0xe5:
                        val=0x880000;
                        break;
                case 0xe7:
                        val=0x880000;
                        break;
                case 0xf0:
                        val=0x00C000;
                        break;
                case 0xf8:
                        val=E0REGS[ASIC27REGS[0]&0xf]&0xffffff;
                        break;
                case 0xfc:      //Adjust damage level to char experience level
                        val=(ASICPARAMS[0xfc]*ASICPARAMS[0xfe])>>6;
                        break;
                case 0xfe:      //todo
                        val=0x880000;
                        break;
                default:
                        val=0x880000;
        }

        if(offset==0) {
                unsigned short d=val&0xffff;
                unsigned short realkey;
                realkey=ASIC27KEY>>8;
                realkey|=ASIC27KEY;
                d^=realkey;
                return d;
        }
        else if(offset==2) {
                unsigned short d=val>>16;
                unsigned short realkey;
                realkey=ASIC27KEY>>8;
                realkey|=ASIC27KEY;
                d^=realkey;
                ASIC27RCNT++;
                if(!(ASIC27RCNT&0x3)) {
                        ASIC27KEY+=0x100;
                        ASIC27KEY&=0xFF00;
                }
                return d;
        }
        return 0xff;
}

static void pgm_kov_asic27_w(unsigned short offset, unsigned short data)
{
        if(offset==0) {
                unsigned short realkey;
                realkey=ASIC27KEY>>8;
                realkey|=ASIC27KEY;
                data^=realkey;
                ASIC27REGS[0]=data;
                return;
        }
        if(offset==2) {
                unsigned short realkey;

                ASIC27KEY=data&0xff00;

                realkey=ASIC27KEY>>8;
                realkey|=ASIC27KEY;
                data^=realkey;
                ASIC27REGS[1]=data;

                ASICPARAMS[ASIC27REGS[1]&0xff]=ASIC27REGS[0];
                if(ASIC27REGS[1]==0xE7) {
                        unsigned int E0R=(ASICPARAMS[0xE7]>>12)&0xf;
                        E0REGS[E0R]&=0xffff;
                        E0REGS[E0R]|=ASIC27REGS[0]<<16;
                }
                if(ASIC27REGS[1]==0xE5) {
                        unsigned int E0R=(ASICPARAMS[0xE7]>>12)&0xf;
                        E0REGS[E0R]&=0xff0000;
                        E0REGS[E0R]|=ASIC27REGS[0];
                }
                ASIC27RCNT=0;
        }
}

static void __fastcall kov_asic27_write_byte(unsigned int address, unsigned char data)
{
        if ((address & 0xfffffc) == 0x500000) {
                pgm_kov_asic27_w(address & 3, data);
        }
}

static void __fastcall kov_asic27_write_word(unsigned int address, unsigned short data)
{
        if ((address & 0xfffffc) == 0x500000) {
                pgm_kov_asic27_w(address & 3, data);
        }
}

static unsigned char __fastcall kov_asic27_read_byte(unsigned int address)
{
        if ((address & 0xff0000) == 0x4f0000) {
                return kov_asic27_protram_r(address & 0xffff);
        }

        if ((address & 0xfffffc) == 0x500000) {
                return pgm_kov_asic27_r(address & 3);
        }

        return 0;
}

static unsigned short __fastcall kov_asic27_read_word(unsigned int address)
{
        if ((address & 0xff0000) == 0x4f0000) {
                return kov_asic27_protram_r(address & 0xffff);
        }

        if ((address & 0xfffffc) == 0x500000) {
                return pgm_kov_asic27_r(address & 3);
        }

        return 0;
}

static void reset_kov_asic27()
{
        ASIC27KEY=0;
        ASIC27RCNT=0;
        memset(ASIC27REGS, 0, 10);
        memset(ASICPARAMS, 0, 256);
        memset(E0REGS, 0, 16);

	// photoy2k
	photoy2k_seqpos = photoy2k_soff = 0;
	memset(photoy2k_trf, 0, 3 * sizeof(int));
}

void install_protection_asic27_kov()
{
        pPgmScanCallback = kov_asic27Scan;
        pPgmResetCallback = reset_kov_asic27;

        SekOpen(0);
        SekMapHandler(4,        0x4f0000, 0x500003, SM_READ | SM_WRITE);
                
        SekSetReadWordHandler(4, kov_asic27_read_word);
        SekSetReadByteHandler(4, kov_asic27_read_byte);
        SekSetWriteWordHandler(4, kov_asic27_write_word);
        SekSetWriteByteHandler(4, kov_asic27_write_byte);
        SekClose();
}


//-----------------------------------------------------------------------------------------------------
// Dragon World 2

#define DW2BITSWAP(s,d,bs,bd)  d=((d&(~(1<<bd)))|(((s>>bs)&1)<<bd))

static unsigned short __fastcall dw2_read_word(unsigned int)
{
        // The value at 0x80EECE is computed in the routine at 0x107c18

        unsigned short d = SekReadWord(0x80EECE);
        unsigned short d2 = 0;

        d=(d>>8)|(d<<8);
        DW2BITSWAP(d,d2,7 ,0);
        DW2BITSWAP(d,d2,4 ,1);
        DW2BITSWAP(d,d2,5 ,2);
        DW2BITSWAP(d,d2,2 ,3);
        DW2BITSWAP(d,d2,15,4);
        DW2BITSWAP(d,d2,1 ,5);
        DW2BITSWAP(d,d2,10,6);
        DW2BITSWAP(d,d2,13,7);

        // ... missing bitswaps here (8-15) there is not enough data to know them
        // the code only checks the lowest 8 bits

        return d2;
}

void install_protection_asic25_asic12_dw2()
{
        SekOpen(0);
        SekMapHandler(4,                0xd80000, 0xd80003, SM_READ);
        SekSetReadWordHandler(4,        dw2_read_word);
        SekClose();
}


//-----------------------------------------------------------------------------------------------------
// Killing Blade

static unsigned short kb_cmd;
static unsigned short kb_reg;
static unsigned short kb_ptr;
static unsigned int   kb_regs[0x100]; //?

static void IGS022_do_dma(unsigned short src, unsigned short dst, unsigned short size, unsigned short mode)
{
        unsigned short param = mode >> 8;
        unsigned short *PROTROM = (unsigned short*)USER1;

        mode &= 0x0f;

        switch (mode)
        {
                case 0:
                case 1:
                case 2:
                case 3:
                {
                        for (int x = 0; x < size; x++)
                        {
                                unsigned short dat2 = swapWord(PROTROM[src + x]);

                                unsigned char extraoffset = param & 0xfe;
                                unsigned char* dectable = (unsigned char *)PROTROM;
                                unsigned short extraxor = ((dectable[((x*2)+0+extraoffset)&0xff]) << 8) | (dectable[((x*2)+1+extraoffset)&0xff] << 0);

                                dat2 = ((dat2 & 0x00ff)<<8) | ((dat2 & 0xff00)>>8);

                                //  mode==0 plain
                                if (mode==3) dat2 ^= extraxor;
                                if (mode==2) dat2 += extraxor;
                                if (mode==1) dat2 -= extraxor;

                                sharedprotram[dst + x] = swapWord(dat2);
                        }

                        if ((mode==3) && (param==0x54) && (src*2==0x2120) && (dst*2==0x2600)) 
                        {
#ifdef LSB_FIRST
                                sharedprotram[0x2600 / 2] = 0x4e75;
#else
                                sharedprotram[0x2600 / 2] = 0x754e;
#endif
                        }
                }
                break;

                case 5: // copy
                {
                        for (int x = 0; x < size; x++) {
                                sharedprotram[dst + x] = PROTROM[src + x];
                        }
                }
                break;

                case 6: // swap bytes and nibbles
                {
                        for (int x = 0; x < size; x++)
                        {
                                unsigned short dat = PROTROM[src + x];

                                dat = ((dat & 0xf000) >> 12) | ((dat & 0x0f00) >> 4) | ((dat & 0x00f0) << 4) | ((dat & 0x000f) << 12);

                                sharedprotram[dst + x] = dat;
                        }
                }
                break;

                case 4: // fixed value xor?
                case 7: // params left in memory? nop?
                default: // ?
                break;
        }
}

static void IGS022_handle_command()
{
        unsigned short cmd = swapWord(sharedprotram[0x200/2]);

        if (cmd == 0x6d) // Store values to asic ram
        {
                unsigned int p1 = (swapWord(sharedprotram[0x298/2]) << 16) | swapWord(sharedprotram[0x29a/2]);
                unsigned int p2 = (swapWord(sharedprotram[0x29c/2]) << 16) | swapWord(sharedprotram[0x29e/2]);

                if ((p2 & 0xffff) == 0x9)       // Set value
                {
                        int reg = (p2 >> 16) & 0xffff;
                        if (reg & 0x200) kb_regs[reg & 0xff] = p1;
                }
                if ((p2 & 0xffff) == 0x6)       // Add value
                {
                        int src1 = (p1 >> 16) & 0x00ff;
                        int src2 = (p1 >>  0) & 0x00ff;
                        int dst  = (p2 >> 16) & 0x00ff;
                        kb_regs[dst] = kb_regs[src2] - kb_regs[src1];
                }
                if ((p2 & 0xffff) == 0x1)       // Add Imm?
                {
                        int reg = (p2 >> 16) & 0x00ff;
                        int imm = (p1 >>  0) & 0xffff;
                        kb_regs[reg] += imm;
                }
                if ((p2 & 0xffff) == 0xa)       // Get value
                {
                        int reg = (p1 >> 16) & 0x00ff;
                        sharedprotram[0x29c/2] = swapWord((kb_regs[reg] >> 16) & 0xffff);
                        sharedprotram[0x29e/2] = swapWord((kb_regs[reg] >>  0) & 0xffff);
                }
        }

        if (cmd == 0x4f) // memcpy with encryption / scrambling
        {
                unsigned short src  = swapWord(sharedprotram[0x290 / 2]) >> 1; // ?
                unsigned short dst  = swapWord(sharedprotram[0x292 / 2]);
                unsigned short size = swapWord(sharedprotram[0x294 / 2]);
                unsigned short mode = swapWord(sharedprotram[0x296 / 2]);

                IGS022_do_dma(src, dst, size, mode);
        }
}

static void killbld_igs025_prot_write(unsigned int offset, unsigned short data)
{
        offset &= 0xf;

        if (offset == 0) {
                kb_cmd = data;
                return;
        }

        switch (kb_cmd)
        {
                case 0:
                        kb_reg = data;
                break;

                case 2:
                        if (data == 1) {
                                IGS022_handle_command();
                                kb_reg++;
                        }
                break;

                case 4:
                        kb_ptr = data;
                break;

                case 0x20:
                        kb_ptr++;
                break;

                default:
                        break;
        }
}

static unsigned short killbld_igs025_prot_read(unsigned int offset)
{
        offset &= 0xf;

        if (offset == 1)
        {
                if (kb_cmd == 1)
                {
                        return (kb_reg & 0x7f);
                }
                else if (kb_cmd == 5)
                {
                        unsigned int protvalue = 0x89911400 | PgmInput[7];
                        return (protvalue >> (8 * (kb_ptr - 1))) & 0xff;
                }
        }

        return 0;
}

static void __fastcall killbld_write_word(unsigned int address, unsigned short data)
{
        killbld_igs025_prot_write(address / 2, data);
}

static unsigned short __fastcall killbld_read_word(unsigned int address)
{
        return killbld_igs025_prot_read(address / 2);
}

static void IGS022Reset()
{
        sharedprotram = (unsigned short*)PGMUSER0;
        USER1 = PGMUSER0 + 0x10000;

        if (strcmp(BurnDrvGetTextA(DRV_NAME), "killbld") == 0) {
                BurnLoadRom(USER1, 11, 1); // load protection data
        } else {
                BurnLoadRom(USER1, 14, 1); // load protection data
        }

        BurnByteswap(USER1, 0x10000);

        unsigned short *PROTROM = (unsigned short*)USER1;

        for (int i = 0; i < 0x4000/2; i++)
                sharedprotram[i] = 0xa55a;

        unsigned short src  = PROTROM[0x100 / 2];
        unsigned short dst  = PROTROM[0x102 / 2];
        unsigned short size = PROTROM[0x104 / 2];
        unsigned short mode = PROTROM[0x106 / 2];
        unsigned short tmp  = PROTROM[0x114 / 2];

        src  = (( src & 0xff00) >> 8) | (( src & 0x00ff) << 8);
        dst  = (( dst & 0xff00) >> 8) | (( dst & 0x00ff) << 8);
        size = ((size & 0xff00) >> 8) | ((size & 0x00ff) << 8);
        tmp  = (( tmp & 0xff00) >> 8) | (( tmp & 0x00ff) << 8);
        mode &= 0xff;

        src >>= 1;

        IGS022_do_dma(src, dst, size, mode);

        sharedprotram[0x2a2/2] = tmp; // crc check?

        kb_cmd = kb_reg = kb_ptr = 0;
        memset (kb_regs, 0, 0x100 * sizeof(int));
}

void install_protection_asic25_asic22_killbld()
{
        pPgmScanCallback = killbldScan;
        pPgmResetCallback = IGS022Reset;

	PGMUSER0 = (unsigned char*)malloc(0x20000);
        sharedprotram = (unsigned short*)PGMUSER0;

        SekOpen(0);
        SekMapMemory(PGMUSER0,  0x300000, 0x303fff, SM_RAM);
        SekMapHandler(4,        0xd40000, 0xd40003, SM_READ | SM_WRITE);
        SekSetReadWordHandler(4, killbld_read_word);
        SekSetWriteWordHandler(4, killbld_write_word);
        SekClose();
}


//-----------------------------------------------------------------------------------------------------
// PStars

unsigned short PSTARSKEY;
static unsigned short PSTARSINT[2];
static unsigned int PSTARS_REGS[16];
static unsigned int PSTARS_VAL;

static unsigned short pstar_e7,pstar_b1,pstar_ce;
static unsigned short pstar_ram[3];

static int Pstar_ba[0x1E]={
        0x02,0x00,0x00,0x01,0x00,0x03,0x00,0x00, //0
        0x02,0x00,0x06,0x00,0x22,0x04,0x00,0x03, //8
        0x00,0x00,0x06,0x00,0x20,0x07,0x00,0x03, //10
        0x00,0x21,0x01,0x00,0x00,0x63
};

static int Pstar_b0[0x10]={
        0x09,0x0A,0x0B,0x00,0x01,0x02,0x03,0x04,
        0x05,0x06,0x07,0x08,0x00,0x00,0x00,0x00
};

static int Pstar_ae[0x10]={
        0x5D,0x86,0x8C ,0x8B,0xE0,0x8B,0x62,0xAF,
        0xB6,0xAF,0x10A,0xAF,0x00,0x00,0x00,0x00
};

static int Pstar_a0[0x10]={
        0x02,0x03,0x04,0x05,0x06,0x01,0x0A,0x0B,
        0x0C,0x0D,0x0E,0x09,0x00,0x00,0x00,0x00,
};

static int Pstar_9d[0x10]={
        0x05,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

static int Pstar_90[0x10]={
        0x0C,0x10,0x0E,0x0C,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static int Pstar_8c[0x23]={
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,
        0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,
        0x02,0x02,0x03,0x03,0x03,0x04,0x04,0x04,
        0x03,0x03,0x03
};

static int Pstar_80[0x1a3]={
        0x03,0x03,0x04,0x04,0x04,0x04,0x05,0x05,
        0x05,0x05,0x06,0x06,0x03,0x03,0x04,0x04,
        0x05,0x05,0x05,0x05,0x06,0x06,0x07,0x07,
        0x03,0x03,0x04,0x04,0x05,0x05,0x05,0x05,
        0x06,0x06,0x07,0x07,0x06,0x06,0x06,0x06,
        0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x07,
        0x06,0x06,0x06,0x06,0x06,0x06,0x07,0x07,
        0x07,0x07,0x08,0x08,0x05,0x05,0x05,0x05,
        0x05,0x05,0x05,0x06,0x06,0x06,0x07,0x07,
        0x06,0x06,0x06,0x07,0x07,0x07,0x08,0x08,
        0x09,0x09,0x09,0x09,0x07,0x07,0x07,0x07,
        0x07,0x08,0x08,0x08,0x08,0x09,0x09,0x09,
        0x06,0x06,0x07,0x07,0x07,0x08,0x08,0x08,
        0x08,0x08,0x09,0x09,0x05,0x05,0x06,0x06,
        0x06,0x07,0x07,0x08,0x08,0x08,0x08,0x09,
        0x07,0x07,0x07,0x07,0x07,0x08,0x08,0x08,
        0x08,0x09,0x09,0x09,0x06,0x06,0x07,0x03,
        0x07,0x06,0x07,0x07,0x08,0x07,0x05,0x04,
        0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x06,
        0x06,0x06,0x06,0x06,0x03,0x04,0x04,0x04,
        0x04,0x05,0x05,0x06,0x06,0x06,0x06,0x07,
        0x04,0x04,0x05,0x05,0x06,0x06,0x06,0x06,
        0x06,0x07,0x07,0x08,0x05,0x05,0x06,0x07,
        0x07,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
        0x05,0x05,0x05,0x07,0x07,0x07,0x07,0x07,
        0x07,0x08,0x08,0x08,0x08,0x08,0x09,0x09,
        0x09,0x09,0x03,0x04,0x04,0x05,0x05,0x05,
        0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,
        0x08,0x09,0x09,0x09,0x03,0x04,0x05,0x05,
        0x04,0x03,0x04,0x04,0x04,0x05,0x05,0x04,
        0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
        0x03,0x03,0x03,0x04,0x04,0x04,0x04,0x04,
        0x04,0x04,0x04,0x04,0x04,0x03,0x03,0x03,
        0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,
        0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00
};

static unsigned short PSTARS_protram_r(unsigned int offset)
{
        offset >>= 1;

        if (offset == 4)                //region
                return PgmInput[7];
        else if (offset >= 0x10)  //timer
        {
                return pstar_ram[offset-0x10]--;
        }
        return 0x0000;
}

static unsigned short PSTARS_r16(unsigned int offset)
{
        offset >>= 1;

        if(offset==0)
        {
                unsigned short d=PSTARS_VAL&0xffff;
                unsigned short realkey;
                realkey=PSTARSKEY>>8;
                realkey|=PSTARSKEY;
                d^=realkey;
                return d;
        }
        else if(offset==1)
        {
                unsigned short d=PSTARS_VAL>>16;
                unsigned short realkey;
                realkey=PSTARSKEY>>8;
                realkey|=PSTARSKEY;
                d^=realkey;
                return d;

        }
        return 0xff;
}

static void PSTARS_w16(unsigned int offset, unsigned short data)
{
        offset >>= 1;

        if(offset==0)
        {
                PSTARSINT[0]=data;
                return;
        }

        if(offset==1)
        {
                unsigned short realkey;
                if((data>>8)==0xff) PSTARSKEY=0xff00;
                realkey=PSTARSKEY>>8;
                realkey|=PSTARSKEY;
                {
                        PSTARSKEY+=0x100;
                        PSTARSKEY&=0xff00;
                        if(PSTARSKEY==0xff00)PSTARSKEY=0x100;
                }
                data^=realkey;
                PSTARSINT[1]=data;
                PSTARSINT[0]^=realkey;

                switch(PSTARSINT[1]&0xff)
                        {
                                case 0x99:
                                {
                                        PSTARSKEY=0x100;
                                        PSTARS_VAL=0x880000;
                                }
                                break;

                                case 0xE0:
                                        {
                                                PSTARS_VAL=0xa00000+(PSTARSINT[0]<<6);
                                        }
                                        break;
                                case 0xDC:
                                        {
                                                PSTARS_VAL=0xa00800+(PSTARSINT[0]<<6);
                                        }
                                        break;
                                case 0xD0:
                                        {
                                                PSTARS_VAL=0xa01000+(PSTARSINT[0]<<5);
                                        }
                                        break;

                                case 0xb1:
                                        {
                                                pstar_b1=PSTARSINT[0];
                                                PSTARS_VAL=0x890000;
                                        }
                                        break;
                                case 0xbf:
                                        {
                                                PSTARS_VAL=pstar_b1*PSTARSINT[0];
                                        }
                                        break;

                                case 0xc1: //TODO:TIMER  0,1,2,FIX TO 0 should be OK?
                                        {
                                                PSTARS_VAL=0;
                                        }
                                        break;
                                case 0xce: //TODO:TIMER  0,1,2
                                        {
                                                pstar_ce=PSTARSINT[0];
                                                PSTARS_VAL=0x890000;
                                        }
                                        break;
                                case 0xcf: //TODO:TIMER  0,1,2
                                        {
                                                pstar_ram[pstar_ce]=PSTARSINT[0];
                                                PSTARS_VAL=0x890000;
                                        }
                                        break;


                                case 0xe7:
                                        {
                                                pstar_e7=(PSTARSINT[0]>>12)&0xf;
                                                PSTARS_REGS[pstar_e7]&=0xffff;
                                                PSTARS_REGS[pstar_e7]|=(PSTARSINT[0]&0xff)<<16;
                                                PSTARS_VAL=0x890000;
                                        }
                                        break;
                                case 0xe5:
                                        {

                                                PSTARS_REGS[pstar_e7]&=0xff0000;
                                                PSTARS_REGS[pstar_e7]|=PSTARSINT[0];
                                                PSTARS_VAL=0x890000;
                                        }
                                        break;
                                case 0xf8: //@73C
                                {
                                PSTARS_VAL=PSTARS_REGS[PSTARSINT[0]&0xf]&0xffffff;
                                }
                                break;


                                case 0xba:
                                {
                                PSTARS_VAL=Pstar_ba[PSTARSINT[0]];
                                }
                                break;
                                case 0xb0:
                                {
                                PSTARS_VAL=Pstar_b0[PSTARSINT[0]];
                                }
                                break;
                                case 0xae:
                                {
                                PSTARS_VAL=Pstar_ae[PSTARSINT[0]];
                                }
                                break;
                                case 0xa0:
                                {
                                PSTARS_VAL=Pstar_a0[PSTARSINT[0]];
                                }
                                break;
                                case 0x9d:
                                {
                                PSTARS_VAL=Pstar_9d[PSTARSINT[0]];
                                }
                                break;
                                case 0x90:
                                {
                                PSTARS_VAL=Pstar_90[PSTARSINT[0]];
                                }
                                break;
                                case 0x8c:
                                {
                                PSTARS_VAL=Pstar_8c[PSTARSINT[0]];
                                }
                                break;
                                case 0x80:
                                {
                                PSTARS_VAL=Pstar_80[PSTARSINT[0]];
                                }
                                break;
                                default:
                                         PSTARS_VAL=0x890000;
                }
        }
}

static void __fastcall pstars_write_byte(unsigned int address, unsigned char data)
{
        if ((address & 0xfffffc) == 0x500000) {
                PSTARS_w16(address & 3, data);
        }
}

void __fastcall pstars_write_word(unsigned int address, unsigned short data)
{
        if ((address & 0xfffffc) == 0x500000) {
                PSTARS_w16(address & 3, data);
        }
}

static unsigned char __fastcall pstars_read_byte(unsigned int address)
{
        if ((address & 0xff0000) == 0x4f0000) {
                return PSTARS_protram_r(address & 0xffff);
        }

        if ((address & 0xfffffc) == 0x500000) {
                return PSTARS_r16(address & 3);
        }

        return 0;
}

static unsigned short __fastcall pstars_read_word(unsigned int address)
{
        if ((address & 0xff0000) == 0x4f0000) {
                return PSTARS_protram_r(address & 0xffff);
        }

        if ((address & 0xfffffc) == 0x500000) {
                return PSTARS_r16(address & 3);
        }

        return 0;
}

static void reset_puzlstar()
{
        PSTARSKEY = 0;
        PSTARS_VAL = 0;
        PSTARSINT[0] = PSTARSINT[1] = 0;
        pstar_e7 = pstar_b1 = pstar_ce = 0;

        memset(PSTARS_REGS, 0, 16);
        memset(pstar_ram,   0,  3);
}

void install_protection_asic27a_puzlstar()
{
        pPgmScanCallback = pstarsScan;
        pPgmResetCallback = reset_puzlstar;

        SekOpen(0);
        SekMapHandler(4,        0x4f0000, 0x500003, SM_READ | SM_WRITE);
                
        SekSetReadWordHandler(4,        pstars_read_word);
        SekSetReadByteHandler(4,        pstars_read_byte);
        SekSetWriteWordHandler(4,       pstars_write_word);
        SekSetWriteByteHandler(4,       pstars_write_byte);
        SekClose();
}


//-----------------------------------------------------------------------------------------------------
// ASIC27A - Kov2, Martmast, etc

static inline void pgm_cpu_sync()
{
        int nCycles = SekTotalCycles() - Arm7TotalCycles();

        if (nCycles > 0) {
                Arm7Run(nCycles);
        }
}

#ifdef ARM7


static unsigned char asic27a_to_arm = 0;
static unsigned char asic27a_to_68k = 0;

static void __fastcall asic27A_write_byte(unsigned int /*address*/, unsigned char /*data*/)
{

}

static void __fastcall asic27A_write_word(unsigned int address, unsigned short data)
{
        if ((address & 0xfffffe) == 0xd10000) {
        //      pgm_cpu_sync();
                asic27a_to_arm = data & 0xff;
                Arm7SetIRQLine(ARM7_FIRQ_LINE, ARM7_HOLD_LINE);
                return;
        }
}

static unsigned char __fastcall asic27A_read_byte(unsigned int address)
{
        if ((address & 0xff0000) == 0xd00000) {
                pgm_cpu_sync();
                return PGMARMShareRAM[(address & 0xffff)^1];
        }

        if ((address & 0xfffffc) == 0xd10000) {
                pgm_cpu_sync();
                return asic27a_to_68k;
        }

        return 0;
}

static unsigned short __fastcall asic27A_read_word(unsigned int address)
{
        if ((address & 0xff0000) == 0xd00000) {
                pgm_cpu_sync();
                return swapWord(*((unsigned short*)(PGMARMShareRAM + (address & 0xfffe))));
        }

        if ((address & 0xfffffc) == 0xd10000) {
                pgm_cpu_sync();
                return asic27a_to_68k;
        }

        return 0;
}

static void asic27A_arm7_write_byte(unsigned int address, unsigned char data)
{
        switch (address)
        {
                case 0x38000000:
                        asic27a_to_68k = data;
                return;
        }
}

static unsigned char asic27A_arm7_read_byte(unsigned int address)
{
        switch (address)
        {
                case 0x38000000:
                        return asic27a_to_arm;
        }

        return 0;
}

#endif
void install_protection_asic27a_martmast()
{
#ifdef ARM7
	nEnableArm7 = 1;
        nPGMArm7Type = 2;
        pPgmScanCallback = asic27AScan;

        SekOpen(0);

        SekMapMemory(PGMARMShareRAM,    0xd00000, 0xd0ffff, SM_FETCH | SM_WRITE);

        SekMapHandler(4,                0xd00000, 0xd10003, SM_READ);
        SekMapHandler(5,                0xd10000, 0xd10003, SM_WRITE);

        SekSetReadWordHandler(4, asic27A_read_word);
        SekSetReadByteHandler(4, asic27A_read_byte);
        SekSetWriteWordHandler(5, asic27A_write_word);
        SekSetWriteByteHandler(5, asic27A_write_byte);
        SekClose();

        Arm7Init(1);
        Arm7Open(0);
        Arm7MapMemory(PGMARMROM,        0x00000000, 0x00003fff, ARM7_ROM);
        Arm7MapMemory(PGMUSER0,         0x08000000, 0x08000000+(nPGMExternalARMLen-1), ARM7_ROM);
        Arm7MapMemory(PGMARMRAM0,       0x10000000, 0x100003ff, ARM7_RAM);
        Arm7MapMemory(PGMARMRAM1,       0x18000000, 0x1800ffff, ARM7_RAM);
        Arm7MapMemory(PGMARMShareRAM,   0x48000000, 0x4800ffff, ARM7_RAM);
        Arm7MapMemory(PGMARMRAM2,       0x50000000, 0x500003ff, ARM7_RAM);
        Arm7SetWriteByteHandler(asic27A_arm7_write_byte);
        Arm7SetReadByteHandler(asic27A_arm7_read_byte);
        Arm7Close();
#endif
}

//----------------------------------------------------------------------------------------------------------
// Kovsh/Photoy2k/Photoy2k2 asic27a emulation... (thanks to XingXing!)


static unsigned short kovsh_highlatch_arm_w = 0;
static unsigned short kovsh_lowlatch_arm_w = 0;
static unsigned short kovsh_highlatch_68k_w = 0;
static unsigned short kovsh_lowlatch_68k_w = 0;
static unsigned int kovsh_counter = 1;

static void __fastcall kovsh_asic27a_write_word(unsigned int address, unsigned short data)
{
        switch (address)
        {
                case 0x500000:
                case 0x600000:
                        kovsh_lowlatch_68k_w = data;
                return;

                case 0x500002:
                case 0x600002:
                        kovsh_highlatch_68k_w = data;
                return;
        }
}

static unsigned short __fastcall kovsh_asic27a_read_word(unsigned int address)
{
        if ((address & 0xffffc0) == 0x4f0000) {
                return swapWord(*((unsigned short*)(PGMARMShareRAM + (address & 0x3e))));
        }

        switch (address)
        {
                case 0x500000:
                case 0x600000:
                        pgm_cpu_sync();
                        return kovsh_lowlatch_arm_w;

                case 0x500002:
                case 0x600002:
                        pgm_cpu_sync();
                        return kovsh_highlatch_arm_w;
        }

        return 0;
}

static void kovsh_asic27a_arm7_write_word(unsigned int address, unsigned short data)
{
        // written... but never read?
        if ((address & 0xffffff80) == 0x50800000) {
                *((unsigned short*)(PGMARMShareRAM + ((address>>1) & 0x3e))) = swapWord(data);
                return;
        }
}

static void kovsh_asic27a_arm7_write_long(unsigned int address, unsigned int data)
{
        switch (address)
        {
                case 0x40000000:
                {
                        kovsh_highlatch_arm_w = data >> 16;
                        kovsh_lowlatch_arm_w = data;

                        kovsh_highlatch_68k_w = 0;
                        kovsh_lowlatch_68k_w = 0;
                }
                return;
        }
}

static unsigned int kovsh_asic27a_arm7_read_long(unsigned int address)
{
        switch (address)
        {
                case 0x40000000:
                        return (kovsh_highlatch_68k_w << 16) | (kovsh_lowlatch_68k_w);

                case 0x4000000c:
                        return kovsh_counter++;
        }

        return 0;
}

void install_protection_asic27a_kovsh()
{
	nEnableArm7 = 1;
        nPGMArm7Type = 1;
        pPgmScanCallback = kovsh_asic27aScan;

        SekOpen(0);
        SekMapMemory(PGMARMShareRAM,    0x4f0000, 0x4f003f, SM_RAM);

        SekMapHandler(4,                0x500000, 0x600005, SM_READ | SM_WRITE);
        SekSetReadWordHandler(4,        kovsh_asic27a_read_word);
        SekSetWriteWordHandler(4,       kovsh_asic27a_write_word);
        SekClose();

        Arm7Init(1);
        Arm7Open(0);
        Arm7MapMemory(PGMARMROM,        0x00000000, 0x00003fff, ARM7_ROM);
        Arm7MapMemory(PGMARMRAM0,       0x10000000, 0x100003ff, ARM7_RAM);
        Arm7MapMemory(PGMARMRAM2,       0x50000000, 0x500003ff, ARM7_RAM);
        Arm7SetWriteWordHandler(kovsh_asic27a_arm7_write_word);
        Arm7SetWriteLongHandler(kovsh_asic27a_arm7_write_long);
        Arm7SetReadLongHandler(kovsh_asic27a_arm7_read_long);
        Arm7Close();
}

//----------------------------------------------------------------------------------------------------------
// olds

static int rego;
static unsigned short olds_bs,olds_cmd3;

static unsigned int olds_prot_addr( unsigned short addr )
{
	unsigned int mode = addr & 0xff;
	unsigned int offset = addr >> 8;
	unsigned int realaddr;

	switch(mode)
	{
		case 0x0:
		case 0x5:
		case 0xa:
			realaddr = 0x402a00 + (offset << 2);
			break;

		case 0x2:
		case 0x8:
			realaddr = 0x402e00 + (offset << 2);
			break;

		case 0x1:
			realaddr = 0x40307e;
			break;

		case 0x3:
			realaddr = 0x403090;
			break;

		case 0x4:
			realaddr = 0x40309a;
			break;

		case 0x6:
			realaddr = 0x4030a4;
			break;

		case 0x7:
			realaddr = 0x403000;
			break;

		case 0x9:
			realaddr = 0x40306e;
			break;

		default:
			realaddr = 0;
	}
	return realaddr;
}

static unsigned int olds_read_reg(unsigned short addr)
{
	unsigned int protaddr = (olds_prot_addr(addr) - 0x400000) / 2;
	return sharedprotram[protaddr] << 16 | sharedprotram[protaddr + 1];
}

static void olds_write_reg(unsigned short addr, unsigned int val)
{
	sharedprotram[(olds_prot_addr(addr) - 0x400000) / 2]     = val >> 16;
	sharedprotram[(olds_prot_addr(addr) - 0x400000) / 2 + 1] = val & 0xffff;
}

unsigned short __fastcall olds_protection_read(unsigned int address)
{
	unsigned short res = 0;

	if (address & 2)
	{
		if (kb_cmd == 1)
			res = kb_reg & 0x7f;
		if (kb_cmd == 2)
			res = olds_bs | 0x80;
		if (kb_cmd == 3)
			res = olds_cmd3;
		else if (kb_cmd == 5)
		{
			unsigned int protvalue = 0x900000 | PgmInput[7]; // region from protection device.
			res = (protvalue >> (8 * (kb_ptr - 1))) & 0xff; // includes region 1 = taiwan , 2 = china, 3 = japan (title = orlegend special), 4 = korea, 5 = hongkong, 6 = world

		}
	}

	return res;
}

void __fastcall olds_protection_write(unsigned int address, unsigned short data)
{
	if ((address & 2) == 0)
		kb_cmd = data;
	else //offset==2
	{
		if (kb_cmd == 0)
			kb_reg = data;
		else if(kb_cmd == 2)	//a bitswap=
		{
			int reg = 0;
			if (data & 0x01) reg |= 0x40;
			if (data & 0x02) reg |= 0x80;
			if (data & 0x04) reg |= 0x20;
			if (data & 0x08) reg |= 0x10;
			olds_bs = reg;
		}
		else if (kb_cmd == 3)
		{
			unsigned short cmd = sharedprotram[0x3026 / 2];

			switch (cmd)
			{
				case 0x11:
				case 0x12:
						break;
				case 0x64:
					{
						unsigned short cmd0 = sharedprotram[0x3082 / 2];
						unsigned short val0 = sharedprotram[0x3050 / 2];	//CMD_FORMAT
						{
							if ((cmd0 & 0xff) == 0x2)
								olds_write_reg(val0, olds_read_reg(val0) + 0x10000);
						}
						break;
					}

				default:
						break;
			}
			olds_cmd3 = ((data >> 4) + 1) & 0x3;
		}
		else if (kb_cmd == 4)
			kb_ptr = data;
		else if(kb_cmd == 0x20)
		  kb_ptr++;
	}
}

static unsigned short __fastcall olds_mainram_read_word(unsigned int address)
{
	if (SekGetPC(-1) >= 0x100000 && address != 0x8178d8) SekWriteWord(0x8178f4, SekReadWord(0x8178D8));

	return *((unsigned short*)(PGM68KRAM + (address & 0x1fffe)));
}

static unsigned char __fastcall olds_mainram_read_byte(unsigned int address)
{
	return PGM68KRAM[(address & 0x1ffff)^1];
}

static void reset_olds()
{
	olds_bs = olds_cmd3 = kb_cmd = ptr = rego = 0;
	memcpy (PGMUSER0, PGMUSER0 + 0x10000, 0x04000);
}

void install_protection_asic25_asic28_olds()
{
        pPgmScanCallback = oldsScan;
        pPgmResetCallback = reset_olds;

	PGMUSER0 = (unsigned char*)malloc(0x30000);
        sharedprotram = (unsigned short*)PGMUSER0;

	if (strcmp(BurnDrvGetTextA(DRV_NAME), "olds100a") == 0) {
		BurnLoadRom(PGMUSER0 + 0x10000,	16, 1);
	} else {
		BurnLoadRom(PGMUSER0 + 0x10000,	20, 1);
		BurnLoadRom(PGMUSER0 + 0x20000,	19, 1);
		BurnByteswap(PGMUSER0 + 0x20000, 0x10000);

		// copy in some 68k code from protection rom
		memcpy (PGMUSER0 + 0x10200, PGMUSER0 + 0x20300, 0x6B4);
	}

	{
		unsigned short *gptr = (unsigned short*)(PGMUSER0 + 0x10000);

		for(int i = 0; i < 0x4000 / 2; i++) {
			if (gptr[i] == (0xffff - i)) gptr[i] = 0x4e75;
		}
	}

        reset_olds();

        SekOpen(0);

        SekMapMemory(PGMUSER0,	0x400000, 0x403fff, SM_RAM);

        SekMapHandler(4,        0xdcb400, 0xdcb403, SM_READ | SM_WRITE);
        SekSetReadWordHandler(4, olds_protection_read);
        SekSetWriteWordHandler(4, olds_protection_write);

	SekMapHandler(5,	0x8178f4, 0x8178f5, SM_READ | SM_FETCH);
	SekSetReadWordHandler(5, olds_mainram_read_word);
	SekSetReadByteHandler(5, olds_mainram_read_byte);

        SekClose();
}

//-------------------------------------------------------------------------------------------
// S.V.G. - Spectral vs Generation / Demon Front / The Gladiator / The Killing Blade EX / Happy 6in1

#ifdef ARM7


static unsigned char svg_ram_sel = 0;
static unsigned char *svg_ram[2];

static void svg_set_ram_bank(int data)
{
        svg_ram_sel = data & 1;
        Arm7MapMemory(svg_ram[svg_ram_sel],     0x38000000, 0x3801ffff, ARM7_RAM);
        SekMapMemory(svg_ram[svg_ram_sel^1],    0x500000, 0x51ffff, SM_FETCH);
}

static void __fastcall svg_write_byte(unsigned int address, unsigned char data)
{
        pgm_cpu_sync();

        if ((address & 0xffe0000) == 0x0500000) {
                svg_ram[svg_ram_sel^1][(address & 0x1ffff)^1] = data;
                return;
        }

        switch (address)
        {
                case 0x5c0000:
                case 0x5c0001:
                        Arm7SetIRQLine(ARM7_FIRQ_LINE, ARM7_HOLD_LINE);
                return;
        }
}

static void __fastcall svg_write_word(unsigned int address, unsigned short data)
{
        pgm_cpu_sync();

        if ((address & 0xffe0000) == 0x0500000) {
                *((unsigned short*)(svg_ram[svg_ram_sel^1] + (address & 0x1fffe))) = data;
                
                return;
        }

        switch (address)
        {
                case 0x5c0000:
                        Arm7SetIRQLine(ARM7_FIRQ_LINE, ARM7_HOLD_LINE);
                return;

                case 0x5c0300:
                        asic27a_to_arm = data;
                return;
        }
}

static unsigned char __fastcall svg_read_byte(unsigned int address)
{
        if ((address & 0xffe0000) == 0x0500000) {
                pgm_cpu_sync();

                int d = svg_ram[svg_ram_sel^1][(address & 0x1ffff)^1];
                return d;
        }

        switch (address)
        {
                case 0x5c0000:
                case 0x5c0001:
                        return 0;
        }

        return 0;
}

static unsigned short __fastcall svg_read_word(unsigned int address)
{
        if ((address & 0xffe0000) == 0x0500000) {
                pgm_cpu_sync();

                return *((unsigned short*)(svg_ram[svg_ram_sel^1] + (address & 0x1fffe)));
        }

        switch (address)
        {
                case 0x5c0000:
                case 0x5c0001:
                        return 0;

                case 0x5c0300:
                        pgm_cpu_sync();
                        return asic27a_to_68k;
        }

        return 0;
}

static void svg_arm7_write_byte(unsigned int address, unsigned char data)
{
        switch (address)
        {
                case 0x40000018:
                        svg_set_ram_bank(data);
                return;

                case 0x48000000:
                        asic27a_to_68k = data;
                return;
        }
}

static void svg_arm7_write_word(unsigned int /*address*/, unsigned short /*data*/)
{

}

static void svg_arm7_write_long(unsigned int address, unsigned int data)
{
        switch (address)
        {
                case 0x40000018:
                        svg_set_ram_bank(data);
                return;

                case 0x48000000:
                        asic27a_to_68k = data;
                return;
        }
}

static unsigned char svg_arm7_read_byte(unsigned int address)
{
        switch (address)
        {
                case 0x48000000:
                case 0x48000001:
                case 0x48000002:
                case 0x48000003:
                        return asic27a_to_arm;
        }

        return 0;
}

static unsigned short svg_arm7_read_word(unsigned int address)
{
        switch (address)
        {
                case 0x48000000:
                case 0x48000002:
                        return asic27a_to_arm;
        }

        return 0;
}

static unsigned int svg_arm7_read_long(unsigned int address)
{
        switch (address)
        {
                case 0x48000000:
                        return asic27a_to_arm;
        }

        return 0;
}

#endif
void install_protection_asic27a_svg()
{
#ifdef ARM7
	nEnableArm7 = 1;
        nPGMArm7Type = 3;

        pPgmScanCallback = svg_asic27aScan;

        svg_ram_sel = 0;
        svg_ram[0] = PGMARMShareRAM;
        svg_ram[1] = PGMARMShareRAM2;

        SekOpen(0);
        SekMapHandler(5,                0x500000, 0x5fffff, SM_RAM);
        SekSetReadWordHandler(5,        svg_read_word);
        SekSetReadByteHandler(5,        svg_read_byte);
        SekSetWriteWordHandler(5,       svg_write_word);
        SekSetWriteByteHandler(5,       svg_write_byte);
        SekClose();

        Arm7Init(1);
        Arm7Open(0);
        Arm7MapMemory(PGMARMROM,        0x00000000, 0x00003fff, ARM7_ROM);
        Arm7MapMemory(PGMUSER0,         0x08000000, 0x08000000 | (nPGMExternalARMLen-1), ARM7_ROM);
        Arm7MapMemory(PGMARMRAM0,       0x10000000, 0x100003ff, ARM7_RAM);
        Arm7MapMemory(PGMARMRAM1,       0x18000000, 0x1803ffff, ARM7_RAM);
        Arm7MapMemory(svg_ram[1],       0x38000000, 0x3801ffff, ARM7_RAM);
        Arm7MapMemory(PGMARMRAM2,       0x50000000, 0x500003ff, ARM7_RAM);
        Arm7SetWriteByteHandler(svg_arm7_write_byte);
        Arm7SetWriteWordHandler(svg_arm7_write_word);
        Arm7SetWriteLongHandler(svg_arm7_write_long);
        Arm7SetReadByteHandler(svg_arm7_read_byte);
        Arm7SetReadWordHandler(svg_arm7_read_word);
        Arm7SetReadLongHandler(svg_arm7_read_long);
        Arm7Close();
#endif
}

//-------------------------------------------------------------------------------------------
// Oriental Legends Special Plus! (Creamy Mami)

static unsigned short m_oldsplus_key;
static unsigned short m_oldsplus_int[2];
static unsigned int m_oldsplus_val;
static unsigned int m_oldsplus_regs[0x100];
static unsigned int m_oldsplus_ram[0x100];

static const int oldsplus_80[0x5]={
        0xbb8,0x1770,0x2328,0x2ee0,0xf4240
};

static const unsigned char oldsplus_fc[0x20]={
        0x00,0x00,0x0a,0x3a,0x4e,0x2e,0x03,0x40,
        0x33,0x43,0x26,0x2c,0x00,0x00,0x00,0x00,
        0x00,0x00,0x44,0x4d,0x0b,0x27,0x3d,0x0f,
        0x37,0x2b,0x02,0x2f,0x15,0x45,0x0e,0x30
};

static const unsigned short oldsplus_a0[0x20]={
        0x000,0x023,0x046,0x069,0x08c,0x0af,0x0d2,0x0f5,
        0x118,0x13b,0x15e,0x181,0x1a4,0x1c7,0x1ea,0x20d,
        0x20d,0x20d,0x20d,0x20d,0x20d,0x20d,0x20d,0x20d,
        0x20d,0x20d,0x20d,0x20d,0x20d,0x20d,0x20d,0x20d,
};

static const unsigned short oldsplus_90[0x7]={
        0x50,0xa0,0xc8,0xf0,0x190,0x1f4,0x258
};

static const unsigned char oldsplus_5e[0x20]={
        0x04,0x04,0x04,0x04,0x04,0x03,0x03,0x03,
        0x02,0x02,0x02,0x01,0x01,0x01,0x01,0x01,
        0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const unsigned char oldsplus_b0[0xe0]={
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,
        0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,
        0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,

        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
        0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,
        0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,

        0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,
        0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,
        0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,
        0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,

        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
        0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,

        0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,
        0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,
        0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,
        0x1c,0x1d,0x1e,0x1f,0x1f,0x1f,0x1f,0x1f
};

static const unsigned char oldsplus_ae[0xe0]={
        0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
        0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
        0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
        0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,

        0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
        0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
        0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
        0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,

        0x1E,0x1F,0x20,0x21,0x22,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,

        0x1F,0x20,0x21,0x22,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,

        0x20,0x21,0x22,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,

        0x21,0x22,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,

        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
        0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23
};

static const unsigned short oldsplus_ba[0x4]={
        0x3138,0x2328,0x1C20,0x1518
};

static const unsigned short oldsplus_9d[0x111]={
        0x0000,0x0064,0x00c8,0x012c,0x0190,0x01f4,0x0258,0x02bc,
        0x02f8,0x0334,0x0370,0x03ac,0x03e8,0x0424,0x0460,0x049c,
        0x04d8,0x0514,0x0550,0x058c,0x05c8,0x0604,0x0640,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x0000,
        0x0064,0x00c8,0x012c,0x0190,0x01f4,0x0258,0x02bc,0x0302,
        0x0348,0x038e,0x03d4,0x041a,0x0460,0x04a6,0x04ec,0x0532,
        0x0578,0x05be,0x0604,0x064a,0x0690,0x06d6,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x0000,0x0064,
        0x00c8,0x012c,0x0190,0x01f4,0x0258,0x02bc,0x0316,0x0370,
        0x03ca,0x0424,0x047e,0x04d8,0x0532,0x058c,0x05e6,0x0640,
        0x069a,0x06f4,0x074e,0x07a8,0x0802,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x0000,0x0064,0x00c8,
        0x012c,0x0190,0x01f4,0x0258,0x02bc,0x032a,0x0398,0x0406,
        0x0474,0x04e2,0x0550,0x05be,0x062c,0x069a,0x0708,0x0776,
        0x07e4,0x0852,0x08c0,0x092e,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x0000,0x0064,0x00c8,0x012c,
        0x0190,0x01f4,0x0258,0x02bc,0x0348,0x03d4,0x0460,0x04ec,
        0x0578,0x0604,0x0690,0x071c,0x07a8,0x0834,0x08c0,0x094c,
        0x09d8,0x0a64,0x0af0,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x0000,0x0064,0x00c8,0x012c,0x0190,
        0x01f4,0x0258,0x02bc,0x0384,0x044c,0x0514,0x05dc,0x06a4,
        0x076c,0x0834,0x08fc,0x09c4,0x0a8c,0x0b54,0x0c1c,0x0ce4,
        0x0dac,0x0e74,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x0000,0x0064,0x00c8,0x012c,0x0190,0x01f4,
        0x0258,0x02bc,0x030c,0x035c,0x03ac,0x03fc,0x044c,0x049c,
        0x04ec,0x053c,0x058c,0x05dc,0x062c,0x067c,0x06cc,0x071c,
        0x076c,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,0x06bc,
        0x06bc
};

static const unsigned short oldsplus_8c[0x20]={
        0x0032,0x0032,0x0064,0x0096,0x0096,0x00fa,0x012c,0x015e,
        0x0032,0x0064,0x0096,0x00c8,0x00c8,0x012c,0x015e,0x0190,
        0x0064,0x0096,0x00c8,0x00fa,0x00fa,0x015e,0x0190,0x01c2,
        0x0096,0x00c8,0x00fa,0x012c,0x012c,0x0190,0x01c2,0x01f4
};

unsigned short __fastcall oldsplus_prot_read(unsigned int address)
{
        if (address == 0x500000)
        {
                UINT16 d = m_oldsplus_val & 0xffff;
                UINT16 realkey = m_oldsplus_key >> 8;
                realkey |= m_oldsplus_key;
                d ^= realkey;
                return d;
        }
        else if (address == 0x500002)
        {
                UINT16 d = m_oldsplus_val >> 16;
                UINT16 realkey = m_oldsplus_key >> 8;
                realkey |= m_oldsplus_key;
                d ^= realkey;
                return d;

        }
        return 0xff;
}

void __fastcall oldsplus_prot_write(unsigned int address, unsigned short data)
{
        if (address == 0x500000)
        {
                m_oldsplus_int[0] = data;
                return;
        }

        if (address == 0x500002)
        {
                unsigned short realkey;
                if ((data >> 8) == 0xff) m_oldsplus_key = 0xff00;
                realkey = m_oldsplus_key >> 8;
                realkey |= m_oldsplus_key;
                {
                        m_oldsplus_key += 0x100;
                        m_oldsplus_key &= 0xff00;
                        if (m_oldsplus_key == 0xff00) m_oldsplus_key = 0x100;
                }
                data ^= realkey;
                m_oldsplus_int[1] = data;
                m_oldsplus_int[0] ^= realkey;

                switch (m_oldsplus_int[1] & 0xff)
                {
                        case 0x88:
                                m_oldsplus_key = 0x100;
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0xd0:
                                m_oldsplus_val = 0xa01000 + (m_oldsplus_int[0] << 5);
                                break;

                        case 0xc0:
                                m_oldsplus_val = 0xa00000 + (m_oldsplus_int[0] << 6);
                                break;

                        case 0xc3:
                                m_oldsplus_val = 0xa00800 + (m_oldsplus_int[0] << 6);
                                break;

                        case 0x36:
                                m_oldsplus_ram[0x36] = m_oldsplus_int[0];
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0x33:
                                m_oldsplus_ram[0x33] = m_oldsplus_int[0];
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0x35:
                                m_oldsplus_ram[0x36] += m_oldsplus_int[0];
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0x37:
                                m_oldsplus_ram[0x33] += m_oldsplus_int[0];
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0x34:
                                m_oldsplus_val = m_oldsplus_ram[0x36];
                                break;

                        case 0x38:
                                m_oldsplus_val = m_oldsplus_ram[0x33];
                                break;

                        case 0x80:
                                m_oldsplus_val = oldsplus_80[m_oldsplus_int[0]];
                                break;

                        case 0xe7:
                                m_oldsplus_ram[0xe7] = m_oldsplus_int[0];
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0xe5:
                                switch (m_oldsplus_ram[0xe7])
                                {
                                        case 0xb000:
                                                m_oldsplus_regs[0xb] = m_oldsplus_int[0];
                                                m_oldsplus_regs[0xc] = 0;
                                                break;

                                        case 0xc000:
                                                m_oldsplus_regs[0xc] = m_oldsplus_int[0];
                                                break;

                                        case 0xd000:
                                                m_oldsplus_regs[0xd] = m_oldsplus_int[0];
                                                break;

                                        case 0xf000:
                                                m_oldsplus_regs[0xf] = m_oldsplus_int[0];
                                                break;
                                }
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0xf8:
                                m_oldsplus_val = m_oldsplus_regs[m_oldsplus_int[0]];
                                break;

                        case 0xfc:
                                m_oldsplus_val = oldsplus_fc[m_oldsplus_int[0]];
                                break;

                        case 0xc5:
                                m_oldsplus_regs[0xd] --;
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0xd6:
                                m_oldsplus_regs[0xb] ++;
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0x3a:
                                m_oldsplus_regs[0xf] = 0;
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0xf0:
                                m_oldsplus_ram[0xf0] = m_oldsplus_int[0];
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0xed:
                                m_oldsplus_val = m_oldsplus_int[0] << 0x6;
                                m_oldsplus_val += m_oldsplus_ram[0xf0];
                                m_oldsplus_val = m_oldsplus_val << 0x2;
                                m_oldsplus_val += 0x900000;
                                break;

                        case 0xe0:
                                m_oldsplus_ram[0xe0] = m_oldsplus_int[0];
                                m_oldsplus_val = 0x990000;
                                break;

                        case 0xdc:
                                m_oldsplus_val = m_oldsplus_int[0] << 0x6;
                                m_oldsplus_val += m_oldsplus_ram[0xe0];
                                m_oldsplus_val = m_oldsplus_val << 2;
                                m_oldsplus_val += 0x904000;
                                break;

                        case 0xcb:
                                m_oldsplus_val =  0xc000;
                                break;

                        case 0xa0:
                                m_oldsplus_val = oldsplus_a0[m_oldsplus_int[0]];
                                break;

                        case 0xba:
                                m_oldsplus_val = oldsplus_ba[m_oldsplus_int[0]];
                                break;

                        case 0x5e:
                                m_oldsplus_val = oldsplus_5e[m_oldsplus_int[0]];
                                break;

                        case 0xb0:
                                m_oldsplus_val = oldsplus_b0[m_oldsplus_int[0]];
                                break;

                        case 0xae:
                                m_oldsplus_val = oldsplus_ae[m_oldsplus_int[0]];
                                break;

                        case 0x9d:
                                m_oldsplus_val = oldsplus_9d[m_oldsplus_int[0]];
                                break;

                        case 0x90:
                                m_oldsplus_val = oldsplus_90[m_oldsplus_int[0]];
                                break;

                        case 0x8c:
                                m_oldsplus_val = oldsplus_8c[m_oldsplus_int[0]];
                                break;

                        default:
                                m_oldsplus_val = 0x990000;
                                break;
                }
        }
}

static void reset_asic27a_oldsplus()
{
        m_oldsplus_key = 0;
        m_oldsplus_val = 0;
        memset (m_oldsplus_int,  0, sizeof(short) * 2);
        memset (m_oldsplus_regs, 0, sizeof(int) * 0x100);
        memset (m_oldsplus_ram,  0, sizeof(int) * 0x100);

        memset (PGMUSER0, 0, 0x400);

        *((unsigned short*)(PGMUSER0 + 0x00008)) = PgmInput[7]; // region
}

void install_protection_asic27a_oldsplus()
{
	PGMUSER0 = (unsigned char*)malloc(0x400);


        pPgmScanCallback = oldsplus_asic27aScan;
        pPgmResetCallback = reset_asic27a_oldsplus;

        SekOpen(0);
        SekMapMemory(PGMUSER0,  0x4f0000, 0x4f003f | 0x3ff, SM_READ); // ram

        SekMapHandler(4,        0x500000, 0x500003, SM_READ | SM_WRITE);
        SekSetReadWordHandler(4, oldsplus_prot_read);
        SekSetWriteWordHandler(4, oldsplus_prot_write);
        SekClose();
}



//-----------------------------------------------------------------------------------------------------
// Save states

int kov_asic27Scan(int nAction, int */*pnMin*/)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = (unsigned char*)ASIC27REGS;
                ba.nLen         = 0x000000a * sizeof(short);
                ba.nAddress     = 0xff0000;
                ba.szName       = "Asic Registers";
                BurnAcb(&ba);

                ba.Data         = (unsigned char*)ASICPARAMS;
                ba.nLen         = 0x0000100 * sizeof(short);
                ba.nAddress     = 0xff1000;
                ba.szName       = "Asic Parameters";
                BurnAcb(&ba);

                ba.Data         = (unsigned char*)E0REGS;
                ba.nLen         = 0x0000010 * sizeof(short);
                ba.nAddress     = 0xff2000;
                ba.szName       = "Asic E0 Registers";
                BurnAcb(&ba);

        }

        if (nAction & ACB_DRIVER_DATA) {
                SCAN_VAR(ASIC27KEY);
                SCAN_VAR(ASIC27RCNT);
		SCAN_VAR(photoy2k_seqpos);
		SCAN_VAR(photoy2k_soff);
		SCAN_VAR(photoy2k_trf[0]);
		SCAN_VAR(photoy2k_trf[1]);
		SCAN_VAR(photoy2k_trf[2]);
        }

        return 0;
}

int asic3Scan(int nAction, int */*pnMin*/)
{
        if (nAction & ACB_DRIVER_DATA) {
                SCAN_VAR(asic3_reg);
                SCAN_VAR(asic3_latch[0]);
                SCAN_VAR(asic3_latch[1]);
                SCAN_VAR(asic3_latch[2]);
                SCAN_VAR(asic3_x);
                SCAN_VAR(asic3_y);
                SCAN_VAR(asic3_z);
                SCAN_VAR(asic3_h1);
                SCAN_VAR(asic3_h2);
                SCAN_VAR(asic3_hold);
        }

        return 0;
}

int killbldScan(int nAction, int */*pnMin*/)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = PGMUSER0 + 0x000000;
                ba.nLen         = 0x0004000;
                ba.nAddress     = 0x300000;
                ba.szName       = "ProtRAM";
                BurnAcb(&ba);

                ba.Data         = (unsigned char*)kb_regs;
                ba.nLen         = 0x00100 * sizeof(int);
                ba.nAddress     = 0xfffffc00;
                ba.szName       = "Protection Registers";
                BurnAcb(&ba);
        }

        if (nAction & ACB_DRIVER_DATA) {
                SCAN_VAR(kb_cmd);
                SCAN_VAR(kb_reg);
                SCAN_VAR(kb_ptr);
        }

        return 0;
}

int pstarsScan(int nAction, int */*pnMin*/)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = (unsigned char*)PSTARSINT;
                ba.nLen         = 0x0000002 * sizeof(short);
                ba.nAddress     = 0xff0000;
                ba.szName       = "Asic Written Values";
                BurnAcb(&ba);

                ba.Data         = (unsigned char*)PSTARS_REGS;
                ba.nLen         = 0x0000010 * sizeof(int);
                ba.nAddress     = 0xff1000;
                ba.szName       = "Asic Register";
                BurnAcb(&ba);

                ba.Data         = (unsigned char*)pstar_ram;
                ba.nLen         = 0x0000003 * sizeof(short);
                ba.nAddress     = 0xff2000;
                ba.szName       = "Asic RAM";
                BurnAcb(&ba);
        }

        if (nAction & ACB_DRIVER_DATA) {
                SCAN_VAR(PSTARSKEY);
                SCAN_VAR(PSTARS_VAL);
                SCAN_VAR(pstar_e7);
                SCAN_VAR(pstar_b1);
                SCAN_VAR(pstar_ce);
        }

        return 0;
}

int oldsScan(int nAction, int */*pnMin*/)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = PGMUSER0 + 0x000000;
                ba.nLen         = 0x0004000;
                ba.nAddress     = 0x400000;
                ba.szName       = "ProtRAM";
                BurnAcb(&ba);
        }

        if (nAction & ACB_DRIVER_DATA) {
                SCAN_VAR(olds_cmd3);
                SCAN_VAR(rego);
                SCAN_VAR(olds_bs);
                SCAN_VAR(ptr);
                SCAN_VAR(kb_cmd);
        }

        return 0;
}

#ifdef ARM7

int asic27AScan(int nAction,int *)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = PGMARMShareRAM;
                ba.nLen         = 0x0010000;
                ba.nAddress     = 0xd00000;
                ba.szName       = "ARM SHARE RAM";
                BurnAcb(&ba);

                ba.Data         = PGMARMRAM0;
                ba.nLen         = 0x0000400;
                ba.nAddress     = 0;
                ba.szName       = "ARM RAM 0";
                BurnAcb(&ba);

                ba.Data         = PGMARMRAM1;
                ba.nLen         = 0x0010000;
                ba.nAddress     = 0;
                ba.szName       = "ARM RAM 1";
                BurnAcb(&ba);

                ba.Data         = PGMARMRAM2;
                ba.nLen         = 0x0000400;
                ba.nAddress     = 0;
                ba.szName       = "ARM RAM 2";
                BurnAcb(&ba);
        }

        if (nAction & ACB_DRIVER_DATA) {
                Arm7Scan(nAction);

                SCAN_VAR(asic27a_to_arm);
                SCAN_VAR(asic27a_to_68k);
        }

        return 0;
}
#endif

int kovsh_asic27aScan(int nAction,int *)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = PGMARMShareRAM;
                ba.nLen         = 0x0000040;
                ba.nAddress     = 0x400000;
                ba.szName       = "ARM SHARE RAM";
                BurnAcb(&ba);

                ba.Data         = PGMARMRAM0;
                ba.nLen         = 0x0000400;
                ba.nAddress     = 0;
                ba.szName       = "ARM RAM 0";
                BurnAcb(&ba);

                ba.Data         = PGMARMRAM2;
                ba.nLen         = 0x0000400;
                ba.nAddress     = 0;
                ba.szName       = "ARM RAM 1";
                BurnAcb(&ba);
        }

        if (nAction & ACB_DRIVER_DATA) {
                Arm7Scan(nAction);

                SCAN_VAR(kovsh_highlatch_arm_w);
                SCAN_VAR(kovsh_lowlatch_arm_w);
                SCAN_VAR(kovsh_highlatch_68k_w);
                SCAN_VAR(kovsh_lowlatch_68k_w);
                SCAN_VAR(kovsh_counter);
        }

        return 0;
}

#ifdef ARM7
int svg_asic27aScan(int nAction,int *)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = PGMARMShareRAM;
                ba.nLen         = 0x0020000;
                ba.nAddress     = 0x400000;
                ba.szName       = "ARM SHARE RAM #0 (address 500000)";
                BurnAcb(&ba);

                ba.Data         = PGMARMShareRAM2;
                ba.nLen         = 0x0020000;
                ba.nAddress     = 0x500000;
                ba.szName       = "ARM SHARE RAM #1";
                BurnAcb(&ba);

                ba.Data         = PGMARMRAM0;
                ba.nLen         = 0x0000400;
                ba.nAddress     = 0;
                ba.szName       = "ARM RAM 0";
                BurnAcb(&ba);

                ba.Data         = PGMARMRAM1;
                ba.nLen         = 0x0040000;
                ba.nAddress     = 0;
                ba.szName       = "ARM RAM 1";
                BurnAcb(&ba);

                ba.Data         = PGMARMRAM2;
                ba.nLen         = 0x0000400;
                ba.nAddress     = 0;
                ba.szName       = "ARM RAM 2";
                BurnAcb(&ba);
        }

        if (nAction & ACB_DRIVER_DATA) {
                Arm7Scan(nAction);
                SCAN_VAR(asic27a_to_arm);
                SCAN_VAR(asic27a_to_68k);

                SCAN_VAR(svg_ram_sel);
                svg_set_ram_bank(svg_ram_sel);
        }

        return 0;
}

#endif

//-------------------------------------------------------------------------------------------
// ddp2 - preliminary (kludgy)

static int ddp2_asic27_0xd10000 = 0;

static void __fastcall Ddp2WriteByte(unsigned int address, unsigned char data)
{
        if ((address & 0xffe000) == 0xd00000) {
                PGMUSER0[(address & 0x1fff)^1] = data;
                *((unsigned short*)(PGMUSER0 + 0x0010)) = 0;
                *((unsigned short*)(PGMUSER0 + 0x0020)) = 1;
                return;
        }

        if ((address & 0xffffffe) == 0xd10000) {
                ddp2_asic27_0xd10000=data;
                return;
        }
}

static void __fastcall Ddp2WriteWord(unsigned int address, unsigned short data)
{
        if ((address & 0xffe000) == 0xd00000) {
                *((unsigned short*)(PGMUSER0 + (address & 0x1ffe))) = data;
                *((unsigned short*)(PGMUSER0 + 0x0010)) = 0;
                *((unsigned short*)(PGMUSER0 + 0x0020)) = 1;
                return;
        }

        if ((address & 0xffffffe) == 0xd10000) {
                ddp2_asic27_0xd10000=data;
                return;
        }
}

static unsigned char __fastcall Ddp2ReadByte(unsigned int address)
{
        if ((address & 0xfffffe) == 0xd10000) {
                ddp2_asic27_0xd10000++;
                ddp2_asic27_0xd10000&=0x7f;
                return ddp2_asic27_0xd10000;
        }

        if ((address & 0xffe000) == 0xd00000) {
                *((unsigned short*)(PGMUSER0 + 0x0002)) = PgmInput[7]; // region
                *((unsigned short*)(PGMUSER0 + 0x1f00)) = 0;
                return PGMUSER0[(address & 0x1fff)^1];
        }

        return 0;
}

static unsigned short __fastcall Ddp2ReadWord(unsigned int address)
{
        if ((address & 0xfffffe) == 0xd10000) {
                ddp2_asic27_0xd10000++;
                ddp2_asic27_0xd10000&=0x7f;
                return ddp2_asic27_0xd10000;
        }

        if ((address & 0xffe000) == 0xd00000) {
                *((unsigned short*)(PGMUSER0 + 0x0002)) = PgmInput[7]; // region
                *((unsigned short*)(PGMUSER0 + 0x1f00)) = 0;
                return *((unsigned short*)(PGMUSER0 + (address & 0x1ffe)));
        }

        return 0;
}

void install_protection_asic27a_ddp2()
{

	PGMUSER0 = (unsigned char*)malloc(0x2000);

        memset (PGMUSER0, 0, 0x2000);

        SekOpen(0);
        SekMapHandler(4,             0xd00000, 0xd1ffff, SM_READ | SM_WRITE);
        SekSetReadWordHandler(4,    Ddp2ReadWord);
        SekSetReadByteHandler(4,    Ddp2ReadByte);
        SekSetWriteWordHandler(4,    Ddp2WriteWord);
        SekSetWriteByteHandler(4,    Ddp2WriteByte);
        SekClose();
}






//-------------------------------------------------------------------------------------------
// ketsui / espgaluda / ddp3

static unsigned short value0;
static unsigned short value1;
static unsigned short valuekey;
static unsigned int   valueresponse;
static unsigned char  ddp3internal_slot = 0;
static unsigned int   ddp3slots[0x100];

void __fastcall ddp3_asic_write(unsigned int offset, unsigned short data)
{
        switch (offset & 0x06)
        {
                case 0:
                        value0 = data;
                return;

                case 2:
                {
                        if ((data >> 8) == 0xff) valuekey = 0xffff;

                        value1 = data ^ valuekey;
                        value0 ^= valuekey;

                        switch (value1 & 0xff)
                        {
                                case 0x40:
                                        valueresponse = 0x880000;
                                        ddp3slots[(value0 >> 10) & 0x1f] = (ddp3slots[(value0 >> 5) & 0x1f] + ddp3slots[(value0 >> 0) & 0x1f]) & 0xffffff;
                                break;

                                case 0x67:
                                        valueresponse = 0x880000;
                                        ddp3internal_slot = (value0 & 0xff00) >> 8;
                                        ddp3slots[ddp3internal_slot] = (value0 & 0x00ff) << 16;
                                break;
                
                                case 0xe5:
                                        valueresponse = 0x880000;
                                        ddp3slots[ddp3internal_slot] |= (value0 & 0xffff);
                                break;
        
                                case 0x8e:
                                        valueresponse = ddp3slots[value0 & 0xff];
                                break;

                                case 0x99: // reset?
                                        valuekey = 0;
                                        valueresponse = 0x880000;
                                break;

                                default:
                                        valueresponse = 0x880000;
                                break;
                        }

                        valuekey = (valuekey + 0x0100) & 0xff00;
                        if (valuekey == 0xff00) valuekey = 0x0100;
                        valuekey |= valuekey >> 8;
                }
                return;

                case 4: return;
        }
}

static unsigned short __fastcall ddp3_asic_read(unsigned int offset)
{
        switch (offset & 0x02)
        {
                case 0: return (valueresponse >>  0) ^ valuekey;
                case 2: return (valueresponse >> 16) ^ valuekey;
        }

        return 0;
}

int ddp3Scan(int nAction, int */*pnMin*/)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = (unsigned char*)ddp3slots;
                ba.nLen         = 0x0000100 * sizeof(int);
                ba.nAddress     = 0xff0000;
                ba.szName       = "ProtRAM";
                BurnAcb(&ba);
        }

        if (nAction & ACB_DRIVER_DATA) {
                SCAN_VAR(value0);
                SCAN_VAR(value1);
                SCAN_VAR(valuekey);
                SCAN_VAR(valueresponse);
                SCAN_VAR(ddp3internal_slot);
        }

        return 0;
}

static void reset_ddp3()
{
        value0 = 0;
        value1 = 0;
        valuekey = 0;
        valueresponse = 0;
        ddp3internal_slot = 0;

        memset (ddp3slots, 0, 0x100 * sizeof(int));
}

void install_protection_asic27a_ketsui()
{
        pPgmResetCallback = reset_ddp3;
        pPgmScanCallback = ddp3Scan;

        SekOpen(0);
        SekMapHandler(4,                0x400000, 0x400005, SM_READ | SM_WRITE);
        SekSetReadWordHandler(4,        ddp3_asic_read);
        SekSetWriteWordHandler(4,       ddp3_asic_write);
        SekClose();
}

void install_protection_asic27a_ddp3()
{
        pPgmResetCallback = reset_ddp3;
        pPgmScanCallback = ddp3Scan;

        SekOpen(0);
        SekMapHandler(4,                0x500000, 0x500005, SM_READ | SM_WRITE);
        SekSetReadWordHandler(4,        ddp3_asic_read);
        SekSetWriteWordHandler(4,       ddp3_asic_write);
        SekClose();
}

int oldsplus_asic27aScan(int nAction, int */*pnMin*/)
{
        struct BurnArea ba;

        if (nAction & ACB_MEMORY_RAM) {
                ba.Data         = (unsigned char*)m_oldsplus_ram;
                ba.nLen         = 0x0000100 * sizeof(int);
                ba.nAddress     = 0xfffc00;
                ba.szName       = "Prot RAM";
                BurnAcb(&ba);

                ba.Data         = (unsigned char*)m_oldsplus_regs;
                ba.nLen         = 0x0000100 * sizeof(int);
                ba.nAddress     = 0xfff800;
                ba.szName       = "Prot REGs";
                BurnAcb(&ba);
        }

        if (nAction & ACB_DRIVER_DATA) {
                SCAN_VAR(m_oldsplus_key);
                SCAN_VAR(m_oldsplus_val);
                SCAN_VAR(m_oldsplus_int[0]);
                SCAN_VAR(m_oldsplus_int[1]);
        }

        return 0;
}
