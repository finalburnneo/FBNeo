#include "tiles_generic.h"
#include "sms.h"
#include "bitswap.h"
#include "z80_intf.h"

UINT32 MastEx; // Extra options
#define MX_GG     (1) // Run as Game Gear
#define MX_PAL    (2) // Run as PAL timing
#define MX_JAPAN  (4) // Return Japan as Region
#define MX_FMCHIP (8) // Emulate FM chip
#define MX_SRAM  (16) // Save/load SRAM in save state


static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static INT32 Hint=0; // Hint counter
static INT32 LineCyc=0,TotalCyc=0;
static INT32 FrameCyc=0; // Total cycles done this frame, (apart from current call)

static INT32 CpuBase=0; // Value to subtract nDozeCycles from in order to get midcycle count

struct Mdraw
{
	UINT16 *Pal; // Palette (0000rrrr ggggbbbb) (0x1000 used)
	UINT8 *Data; // Pixel values
	UINT8 PalChange;
	INT32 Line; // Image line
	UINT8 ThreeD; // 0/1=normal, 2=probably 3D right image, 3=probably 3D left image
};

Mdraw mdraw;

UINT8 *Sram; // battery backup ram
UINT8 *Ram; // normal ram
UINT8 *VRam; // video ram
UINT8 *CRam; // color ram
UINT8 *FmReg; // Current FM values


UINT8 Out3F; // Value written to port 3f
UINT8 ThreeD; // Value written to fffb (3D glasses toggle)
UINT8 FmSel; // Selected FM register
UINT8 FmDetect; // Value written to port f2

static UINT8 *SMSCartROM;
static UINT32 RomLen;

static UINT32 *DrvPalette;

static UINT16 SMSInputs[5];
UINT8 SMSReset;
UINT8 MastInput[2];
UINT8 SMSJoy1[12];
UINT8 SMSDips[3];

INT32 RomPage[3]; // Offsets to Rom banks
UINT8 Bank[4];  // Values written to fffc-f
INT32 MastY;

UINT8 *qc; // Quick color lookup from bits

struct Mastv
{
	UINT16 Low;   // Low byte of video command
	UINT8  Wait;  // 1 if waiting for the high byte at port bf
	UINT8  Stat;  // Status
	UINT8  Reg[0x10]; // Video registers
	UINT8  Mode;  // Video read/write mode
	UINT16 Addr;  // Video read/write addr
	UINT8  CRamLatch; // CRam latch
	UINT8  DataBuf; // data port buffer
};

Mastv v; // video chip
UINT8 Irq; // 1 if an IRQ is latched

UINT8 *MemFetch[0x100], *MemRead[0x100], *MemWrite[0x100];

UINT8 SMSPaletteRecalc;

INT32 MdrawInit()
{
	static UINT8 Src[0x20]=
	{
		0x00,0x01,0x04,0x05, 0x10,0x11,0x14,0x15,
		0x02,0x03,0x06,0x07, 0x12,0x13,0x16,0x17,
		0x08,0x09,0x0c,0x0d, 0x18,0x19,0x1c,0x1d,
		0x0a,0x0b,0x0e,0x0f, 0x1a,0x1b,0x1e,0x1f
	};

	memset(&mdraw,0,sizeof(Mdraw));

	// Make the quick color table AC 000S BD = Color SABCD
	memset(qc,0,sizeof(qc));
	memcpy(qc+0x000,Src+0x00,8); 
	memcpy(qc+0x100,Src+0x08,8);
	memcpy(qc+0x200,Src+0x10,8);
	memcpy(qc+0x300,Src+0x18,8);


	return 0;
}


void MdrawCall()
{
	

	// Draw line at 1 byte per pixel
	UINT16 *pd;
	UINT16	*pe;
	UINT32 *pwp;
	UINT16 *pLine=NULL;
	UINT8 *ps=NULL;
	// Find destination line
	INT32 y=mdraw.Line; 
	if (MastEx&MX_GG)
		y-=24;
	if (y<0) 
		return;
	if (y>=nScreenHeight)
		return;

	pLine=pTransDraw+y*nScreenWidth;

	// Find source pixels
	ps=mdraw.Data+16;
	INT32 Len=256;
	if (MastEx&MX_GG) 
	{ 
		ps+=48; 
		Len=160; 
	}


	pd=pLine;
	pe=pd+Len;
	//pwp=DispWholePal;
	do { 
		*pd=(UINT16) mdraw.Pal[*ps];
		pd++; 
		ps++;
	}
	while (pd<pe);
}


// A change in CRam - update the color in Mdraw
void MdrawCramChange(INT32 a)
{
	static INT32 i = 0;
	INT32 s,c;

	if (MastEx&MX_GG)
	{
		// Game gear color
		a>>=1; a&=0x1f; s=((UINT16 *)CRam)[a];
		c =s&0xfff;      // -> BBBBGGGGRRRR
	}
	else
	{
		// Master System color
		a&=0x1f; 
		s=CRam[a];
		c =(s&0x30)<<6; // -> BB0000000000
		c|=(s&0x0c)<<4; // -> 0000GG000000
		c|=(s&0x03)<<2; // -> 00000000RR00
	}
	mdraw.Pal[a]=(UINT16)c; 
	mdraw.PalChange=1;
}

void MdrawCramChangeAll()
{
	int a=0;
	for (a=0;a<0x40;a++)
		MdrawCramChange(a);
}


INT32 TileLine(UINT8 *pd,UINT32 Line,INT8 nPal,UINT8 *cv,UINT32 x)
{
	UINT8 *pe=pd+8, collision=0;
	do
	{
		UINT32 c;
		c=Line&0x80808080; 
		if (c==0) 
			goto Trans;
		c|=c>>15; // 18180
		c>>=7; c&=0x0303; *pd=qc[nPal+c];
		if (cv) 
		{ 
			if (cv[x/8]&(1<<(x%8))) 
				collision=1; 
			cv[x/8]|=1<<x%8; 
		}
Trans:
		pd++; x++; 
		Line<<=1;
	}
	while (pd<pe);
	return collision;
}

void TileFlip(UINT8 *pd,UINT32 Line,char nPal)
{
	UINT8 *pe; pe=pd+8;
	do
	{
		UINT32 c;
		c=Line&0x01010101; 
		if (c==0) 
			goto Trans;
		c|=c>>15; 
		c&=0x0303; 
		*pd=qc[nPal+c];
Trans:
		pd++; 
		Line>>=1;
	}
	while (pd<pe);
}

char MdrawBackground(UINT16 nPass)
{
	UINT8 *Name; INT32 x,BackY,BackX;
	char NeedHigh=0;
	// Find background line
	BackY=v.Reg[9]+mdraw.Line;
	while (BackY>=224)
		BackY-=224;

	// Find name table
	Name=VRam + ((v.Reg[2]<<10)&0x3800);
	// Find name table line
	Name+=(BackY>>3)<<6;

	// Find scroll X value
	if (mdraw.Line<16 && v.Reg[0]&0x40) 
		BackX=0; // Top two lines static
	else 
		BackX=(-v.Reg[8])&0xff;

	for (x=8-(BackX&7); x<0x108; x+=8)
	{
		UINT8 *Tile; 
		UINT32 Line;
		UINT32 t; 
		INT8 nPal; 
		INT32 ty;
		UINT8 *Dest;

		if (v.Reg[0]&0x80 && x>=0xc8)
		{
			BackY=mdraw.Line;

			// Find name table
			Name=VRam + ((v.Reg[2]<<10)&0x3800);
			// Find name table line
			Name+=(BackY>>3)<<6;
		}

		Dest=mdraw.Data+8+x;
		// Find tile
		t=BackX+x-8; t>>=2; t&=0x3e; t=*((UINT16 *)(Name+t));

		if (nPass==0)
		{
			// Low pass
			// Low background is color zero of the tile (even if high)
			memset(Dest,(t&0x0800)>>7,8);
			if (t&0x1000) 
			{ 
				NeedHigh=1; 
				continue; 
			} // skip and return that we need a high pass
		}
		else
		{
			// High pass
			if ((t&0x1000)==0) 
				continue; // low tile: skip it
		}

		Tile=VRam + ((t<<5)&0x3fe0);
		// Find tile line
		ty=BackY&7;  
		if (t&0x400)
			ty=7-ty;
		Tile+=ty<<2;
		nPal=(char)(t&0x800?4:0);
		Line=*((UINT32 *)Tile);

		if (t&0x200) 
			TileFlip(Dest,Line,nPal);
		else        
			TileLine(Dest,Line,nPal,0,0);
	}
	return NeedHigh;
}

void MdrawSprites()
{
	UINT8 *Sprite,*ps,*Tile,cv[40];
	int i,OnLine=0;
	// Find sprite table
	Sprite=VRam + ((v.Reg[5]<< 7)&0x3f00);
	// Find sprite tiles
	Tile  =VRam + ((v.Reg[6]<<11)&0x2000);
	// Find the end of the sprite list
	for (i=0,ps=Sprite; i<64; i++,ps++)
	{ 
		if (ps[0]==0xd0) 
			break; 
	} // End of sprite list
	i--;
	memset(&cv,0,sizeof(cv));
	// Go through the sprites backwards
	for (ps=Sprite+i; i>=0; i--,ps--)
	{
		int x,y,t;
		UINT32 Line; 
		UINT8 *pa; 
		int Height;

		// Get Y coordinate
		y=ps[0]; 
		if (y>=0xe0) 
			y-=0x100;
		y++;

		if (mdraw.Line<y) 
			continue; // Sprite is below
		// Find sprite height
		Height=8; 
		if (v.Reg[1]&2) 
			Height=16;

		if (mdraw.Line>=y+Height)
			continue; // Sprite is above

		// Sprite is on this line, get other info
		OnLine++;
		pa=Sprite+0x80+(i<<1); x=pa[0];

		if (v.Reg[0]&8) 
			x-=8; // gng
		// Find sprite tile
		t=pa[1]; 

		if (v.Reg[1]&2) 
			t&=~1; // Even tile number (Golvellius)
		t<<=5;

		// Find sprite tile line
		t+=(mdraw.Line-y)<<2;
		Line=*((UINT32 *)(Tile+t));
		// Draw sprite tile line
		if (TileLine(mdraw.Data+16+x,Line,4,cv,16+x))
		{
			v.Stat|=0x20;
		}
	}
	if (OnLine > 8)
		v.Stat|=0x40;
}

// Draw a scanline
void MdrawDo()
{
	int Hide=0; char NeedHigh=0;
	int Border=0;
	mdraw.ThreeD=ThreeD; // Mark whether this line is for the left eye or right

	Border=v.Reg[7]&0x0f; 
	Border|=0x10;

	if (MastEx&MX_GG)
	{
		// Game gear only shows part of the height
		if (mdraw.Line< 0x18)
			Hide=1;
		else if (mdraw.Line>=0xa8)
			Hide=1;
	}

	if (Hide==0) 
	{ 
		if ((v.Reg[1]&0x40)==0)
			Hide=1;
	}

	if (Hide) 
	{ 
		memset(mdraw.Data+16,Border,0x100);
		MdrawCall(); 
		return;
	} // Line is hidden

	NeedHigh=MdrawBackground(0x0000); // low chars
	MdrawSprites();
	if (NeedHigh) 
		MdrawBackground(0x1000); // high chars

	if (v.Reg[0]&0x20) 
		memset(mdraw.Data+16,Border,8); // Hide first column

	MdrawCall();
}


void CalcRomPage(int n)
{
	// Point to the rom page selected for page n
	int b; 
	int PageOff; 
	int Fold=0xff;

	b=Bank[1+n];
TryLower:
	PageOff=(b&Fold)<<14;
	if (PageOff+0x4000>RomLen) // Set if the page exceeds the rom length
	{
		PageOff=0;
		if (Fold)
		{ 
			Fold>>=1;
			goto TryLower; 
		} // (32k games, spellcaster, jungle book)
	}

	RomPage[n]=PageOff; // Store in the Mastz structure
}

UINT8 *GetRomPage(int n)
{
	CalcRomPage(n); // Recalc the rom page
	return SMSCartROM+RomPage[n]; // Get the direct memory pointer
}

// 0400-3fff Page 0
void MastMapPage0()
{
	UINT8 *Page; 
	Page=GetRomPage(0);
	// Map Rom Page
	{
		int i=0;
		for (i=0x04;i<0x40;i++)
		{
			MemFetch[i]=MemRead[i]=Page;
			MemWrite[i]=0;
		}
	}
}

// 4000-7fff Page 1
void MastMapPage1()
{
	UINT8 *Page;
	Page=GetRomPage(1);
	// Map Rom Page
	{
		int i=0;
		Page-=0x4000;
		for (i=0x40;i<0x80;i++)
		{
			MemFetch[i]=MemRead[i]=Page;
			MemWrite[i]=0;
		}
	}
}

// 8000-bfff Page 2
void MastMapPage2()
{
	UINT8 *Page=0; INT32 i=0;
	if (Bank[0]&0x08)
	{
		// Map Battery Ram
		Page=Sram;
		Page+=(Bank[0]&4)<<11; // Page -> 0000 or 2000
		Page-=0x8000;
		for (i=0x80;i<0xc0;i++)
		{
			MemFetch[i]=Page;
			MemRead [i]=Page;
			MemWrite[i]=Page;
		}
	}
	else
	{
		// Map normal Rom Page
		Page=GetRomPage(2);
		Page-=0x8000;
		for (i=0x80;i<0xc0;i++)
		{
			MemFetch[i]=Page;
			MemRead [i]=Page;
			MemWrite[i]=0;
		}
	}
}


int MastMapMemory()
{
	// Map in framework
	int i=0;

	memset(&MemFetch,0,sizeof(MemFetch));
	memset(&MemRead, 0,sizeof(MemRead));
	memset(&MemWrite,0,sizeof(MemWrite));

	// 0000-03ff Fixed Rom view
	for (i=0x00;i<0x04;i++)
	{
		MemFetch[i]=MemRead[i]=SMSCartROM; MemWrite[i]=0; 
	}

	// c000-dfff Ram
	for (i=0xc0;i<0xe0;i++)
	{ 
		MemFetch[i]=MemRead[i]=MemWrite[i]=Ram-0xc000;
	}
	// e000-ffff Ram mirror
	for (i=0xe0;i<0x100;i++)
	{
		MemFetch[i]=MemRead[i]=MemWrite[i]=Ram-0xe000; 
	}

	// For bank writes ff00-ffff callback Doze*
	MemWrite[0xff]=0;
	// Map in pages
	MastMapPage0(); 
	MastMapPage1(); 
	MastMapPage2();
	return 0;
}



// --------------------------  Video chip access  -----------------------------

void VidCtrlWrite(UINT8 d)
{
	int Cmd=0;
	if (v.Wait==0) 
	{ 
		v.Addr&=0xff00; 
		v.Addr|=d; 
		v.Wait=1; 
		return; 
	} // low byte

	// high byte: do video command
	Cmd=d<<8; 
	Cmd|=v.Addr&0xff;
	v.Mode=(UINT8)((Cmd>>14)&3); // 0-2=VRAM read/write 3=CRAM write

	if (v.Mode==2)
	{
		// Video register set
		int i;
		i=(Cmd>>8)&0xf;
		v.Reg[i]=(UINT8)(Cmd&0xff);
	}
	v.Addr=(UINT16)(Cmd&0x3fff);
	if (v.Mode==0)
	{
		v.DataBuf=VRam[(v.Addr++)&0x3fff];
	}

	v.Wait=0;
	Z80SetIrqLine(0x38, 0);
}

UINT8 VidCtrlRead()
{
	UINT8 d=0;
	d=v.Stat;

	v.Wait=0; 
	v.Stat&=0x3f;

	Z80SetIrqLine(0x38, 0);
	return d;
}

// -----------------------------------------------------------------------------

void VidDataWrite(UINT8 d)
{
	v.DataBuf=d;
	if (v.Mode==3)
	{
		// CRam Write
		UINT8 *pc;
		pc=CRam+(v.Addr&((MastEx&MX_GG)?0x3f:0x1f));
		if (MastEx&MX_GG)
		{
			if ((v.Addr&1)==0) { v.CRamLatch = d; }
			else
			{
				*(pc-1) = v.CRamLatch; *pc = d;
				MdrawCramChange(v.Addr);
			}
		}
		else if (pc[0]!=d) { pc[0]=d; MdrawCramChange(v.Addr); }  // CRam byte change
	}
	else
	{
		VRam[v.Addr&0x3fff]=d;
	}
	v.Addr++; // auto increment address
	v.Wait=0;
}

UINT8 VidDataRead()
{
	UINT8 d=0;
	d=v.DataBuf;
	v.DataBuf=VRam[(v.Addr++)&0x3fff];
	v.Wait=0;
	return d;
}

// =============================================================================
UINT8 SysIn(UINT16 a)
{
	UINT8 d=0xff;
	a&=0xff; // 8-bit ports
	if (a==0x00)
	{
		d=0x7f; 
		if ((MastInput[0]&0x80)==0) 
			d|=0x80; // Game gear start button
		if (MastEx&MX_JAPAN) 
			d&=0xbf;

	}
	else if (a==0x05) 
	{ 
		d=0; 
	} // Link-up
	else if (a==0x7e)
	{
		// V-Counter read
		if (MastY>0xda)
			d=(UINT8)(MastY-6);
		else            
			d=(UINT8) MastY;

	}
	else if (a==0x7f)
	{
		// H-Counter read: return about the middle
		d=0x40;

	}
	else if (a==0xbe) 
	{ 
		d=VidDataRead(); 

	}
	else if (a==0xbd || a==0xbf) 
	{
		d=VidCtrlRead(); 

	}
	else if (a==0xdc || a==0xc0)
	{
		// Input
		d=MastInput[0]; 
		d&=0x3f;
		d|=MastInput[1]<<6;
		d=(UINT8)(~d);
	}
	else if (a==0xdd || a==0xc1)
	{
		d=(~MastInput[1]>>2)&0xf;
		d|=0x30;
		// Region detect:
		d|=Out3F&0x80; // bit 7->7
		d|=(Out3F<<1)&0x40; // bit 5->6
		if (MastEx&MX_JAPAN)
			d^=0xc0; //select japanese
	}
	else if (a==0xf2)
	{
		// Fm Detect
		d=0xff;
		if (MastEx&MX_FMCHIP) 
		{
			d=FmDetect; d&=1;
		}
	}


	return d;
}

void SysOut(UINT16 a,UINT8 d)
{
	a&=0xff; // 8-bit ports
	//printf("write 0x%x (PC=0x%x) -> 0x%x\n", a, Doze.pc, d);
	if ( a      ==0x06) 
	{ 
		//	DpsgStereo(d);   
		return;
	} // Psg Stereo
	if ( a      ==0x3f)
	{ 
		Out3F=d;
		return;
	} // Region detect
	if ((a&0xfe)==0x7e) 
	{ 
		//DpsgWrite(d);
		return;
	} // Psg Write
	if ( a      ==0xbe) 
	{ 
		VidDataWrite(d);
		return;
	}
	if ( a      ==0xbd || a == 0xbf) 
	{ 
		VidCtrlWrite(d); 
		return;
	}
	if ( a      ==0xf0)
	{ 
		FmSel=d;
		return;
	}
	if ( a      ==0xf1) 
	{ 
		//	MsndFm(FmSel,d); 
		return;
	}
	if ( a      ==0xf2) 
	{ 
		FmDetect=d;
		return;
	}

	return;
}

// -----------------------------------------------------------------------------

UINT8 SysRead(UINT16 a)
{
	(void)a; 
	return 0;
}

void SysWrite(UINT16 a,UINT8 d)
{
	if (a==0x8000) 
	{ 
		Bank[3]=d; 
		MastMapPage2(); 
		return;
	} // Codemasters mapper

	if ((a&0xc000)==0xc000) 
		Ram[a&0x1fff]=d; // Ram is always written to
	if ((a&0xfffc)==0xfffc)
	{
		// bank select write
		int b;
		b=a&3;
		if (d==Bank[b]) 
			return;
		Bank[b]=d;

		if (b==0)
			MastMapPage2();
		if (b==1) 
			MastMapPage0();
		if (b==2)
			MastMapPage1();
		if (b==3) 
			MastMapPage2();
		return;
	}

	if ((a&0xfffc)==0xfff8)
	{
		int e;
		// Wonderboy 2 writes to this even though it's 2D
		e=ThreeD&1; 
		ThreeD&=2;
		ThreeD|=d&1;
		if (d!=e) 
			ThreeD|=2; // A toggle: looks like it's probably a 3D game
		return;
	}
	return;
}



static UINT8 __fastcall ReadIoHandler(UINT16 a)          
{ 
	return SysIn(a);
}

static void __fastcall WriteIoHandler(UINT16 a, UINT8 v) 
{ 
	SysOut(a,v);
}

static UINT8 __fastcall ReadProgHandler(UINT16 a)
{
	if (MemRead[a >> 8])
	{
		return MemRead[a >> 8][a];
	}
	else
	{
		return SysRead(a);
	}
}

static void __fastcall WriteProgHandler(UINT16 a, UINT8 v)
{
	if (MemWrite[a >> 8])
	{
		MemWrite[a >> 8][a] = v;
	}
	else
	{
		SysWrite(a,v);
	}
}


INT32 SMSGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}

	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j];
	}

	*pszName = szFilename;

	return 0;
}


static INT32 MemIndex(UINT32 cart_size)
{
	UINT8 *Next; Next = AllMem;

	SMSCartROM	= Next; Next += (cart_size <= 0x100000) ? 0x100000 : cart_size;

	DrvPalette	= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam		= Next;
	Sram= Next; Next += 0x4000; // battery backup ram
	Ram= Next; Next += 0x2000;  // normal ram
	VRam= Next; Next += 0x4000; // video ram
	CRam= Next; Next += 0x0040; // color ram
	FmReg= Next; Next += 0x40; // Current FM values
	qc= Next; Next += 0x308;
	mdraw.Pal=(UINT16*)Next; Next += 0x100* sizeof(UINT16); // Palette (0000rrrr ggggbbbb) (0x1000 used)
	mdraw.Data=Next; Next += 0x120;// Pixel values
	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static INT32 SMSDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	// Reset banks
	Bank[0]=0;
	Bank[1]=0; 
	Bank[2]=1; 
	Bank[3]=0;
	MastMapPage0();
	MastMapPage1(); 
	MastMapPage2();
	// Reset vdp
	memset(&v,0,sizeof(v));
	v.Reg[10]=0xff;
	ThreeD=0;

	memset(CRam,0,sizeof(CRam));
	// Update the colors in Mdraw
	 MdrawCramChangeAll();

	ZetOpen(0);
	ZetReset();
	ZetSetHL(0, 0xdff0);
	ZetClose();

	return 0;
}

void SMSLoadRom()
{
	BurnLoadRom(SMSCartROM, 0, 1);
	MastMapMemory();
	ZetOpen(0);
	ZetSetInHandler(ReadIoHandler);
	ZetSetOutHandler(WriteIoHandler);
	ZetSetReadHandler(ReadProgHandler);
	ZetSetWriteHandler(WriteProgHandler);
	ZetClose();
}

INT32 SMSInit()
{
	struct BurnRomInfo ri;
	BurnDrvGetRomInfo(&ri, 0);
	UINT32 length = ri.nLen;
	RomLen = length;
	AllMem = NULL;
	MemIndex(length);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL)
	{
		return 1;
	}
	memset(AllMem, 0, nLen);
	MemIndex(length);

	ZetInit(0);
	ZetOpen(0);
	ZetClose();
	SMSLoadRom();

	GenericTilesInit();

	for (int i = 0; i < sizeof(DrvPalette); i++)
	{
		DrvPalette[i] = i*100;
	}
	SMSDoReset();
	return 0;
}



INT32 SMSExit()
{
	GenericTilesExit();

	BurnFree (AllMem);
	ZetExit();

	return 0;
}

void DrvCalcPalette()
{
	int i;
	// Precalc the whole 4096 color palette
	for (i=0; i<0x1000; i++)
	{
		int r,g,b,c; 
		r=i&15;
		g=(i>>4)&15;
		b=(i>>8)&15;
		// Convert to maximum colors
		r*=0xff; 
		g*=0xff; 
		b*=0xff;
		if (MastEx&MX_GG) 
		{ 
			r/=15; 
			g/=15; 
			b/=15; 
		}
		else 
		{ 
			r/=12; 
			g/=12; 
			b/=12;
		} // Colors e.g. 13,13,13 will overflow, but are unused

		//ColAdjust(r,g,b,Filter);
		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

}

INT32 SMSDraw()
{	
	MdrawCramChangeAll();
	DrvCalcPalette();
	BurnTransferCopy(DrvPalette);
	return 0;
}

static void SMSCompileInputs()
{
	memset (SMSInputs, 0xff, 5 * sizeof(UINT16));

	for (INT32 i = 0; i < 12; i++) {
		SMSInputs[0] ^= (SMSJoy1[i] & 1) << i;
	}
}


int CpuMid() // Returns how many cycles the z80 has done inside the run call
{
	return CpuBase-z80_ICount;
}

// Run the z80 cpu for Cycles more cycles
void CpuRun(int Cycles)
{
	int Done=0;

	z80_ICount+=Cycles; 
	CpuBase=z80_ICount;
	ZetRun(CpuBase);


	Done=CpuMid(); // Find out number of cycles actually done
	CpuBase=z80_ICount; // Reset CpuBase, so CpuMid() will return 0

	FrameCyc+=Done; // Add cycles done to frame total
}

static void RunLine()
{

	if (MastY<=192)
	{
		Hint--;
		if (Hint<0)
		{
			if (v.Reg[0]&0x10) // Do hint
			{ 
				
				ZetSetVector(0xff);
				ZetSetIRQLine(0x38, 1);
			}
			Hint=v.Reg[10];
		}
	}
	else
	{
		Hint=v.Reg[10];
	}

	if (v.Stat&0x80)
	{
		if (v.Reg[1]&0x20) // Do vint
		{ 
			ZetSetVector(0xff);
			ZetSetIRQLine(0x38, 1);
		}
	}

	CpuRun(LineCyc);
}



INT32 SMSFrame()
{
	ZetOpen(0);
	ZetNewFrame();
	ZetSetVector(Irq);
	if (MastEx&MX_PAL) 
		LineCyc=273; // PAL timings (but not really: not enough lines)
	else              
		LineCyc=228; // NTSC timings

	TotalCyc=LineCyc*262; // For sound
	// Start counter and sound
	//MsndDone=0; 
	FrameCyc=0; 
	z80_ICount=0; 
	CpuBase=0;

	// V-Int:
	MastY=192;
	RunLine();
	v.Stat|=0x80; 
	for (MastY=193;MastY<262;MastY++) 
	{ 
		RunLine(); 
	}

	if (MastInput[0]&0x40) // Cause nmi if pause pressed
		ZetNmi();

	// Active scan
	for (MastY=0;MastY<192;MastY++) 
	{
		mdraw.Line=MastY;
		MdrawDo(); 
		RunLine(); 
	}
	// Finish sound
	//MsndTo(MsndLen);

	Irq = ZetGetVector();


	TotalCyc=0; // Don't update sound outside of a frame
	ZetClose();

	if (pBurnDraw) 
		SMSDraw();
	return 0;

}

INT32 SMSScan(INT32 nAction, INT32 *pnMin)
{

	return 0;
}
