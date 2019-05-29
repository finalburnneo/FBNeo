/*Snem 0.1 by Tom Walker
65816 emulation*/

#include <stdio.h>
#include "snes.h"

CPU_65816 snes_cpu;
void (*opcodes[256][5])();

void updatecpumode()
{
	if (snes_cpu.e)
	{
		snes_cpu.cpumode = 4;
		REG_XH() = REG_YH() = 0;
	}
	else
	{
		snes_cpu.cpumode = 0;
		if (!CHECK_MEMORYACC())
		{
			snes_cpu.cpumode |= 1;
		}
		if (!CHECK_INDEX())
		{
			snes_cpu.cpumode |= 2;
		}
		else
		{
			REG_XH() = REG_YH() = 0;
		}
	}
}

/*Addressing modes*/
static inline UINT32 absolute()
{
	UINT32 temp = readmemw(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc += 2;
	return temp | snes_cpu.dbr;
}

static inline UINT32 absolutex()
{
	UINT32 temp = (readmemw(snes_cpu.pbr | snes_cpu.pc)) + REG_XW() + snes_cpu.dbr;
	snes_cpu.pc += 2;
	return temp;
}

static inline UINT32 absolutey()
{
	UINT32 temp = (readmemw(snes_cpu.pbr | snes_cpu.pc)) + REG_YW() + snes_cpu.dbr;
	snes_cpu.pc += 2;
	return temp;
}

static inline UINT32 absolutelong()
{
	UINT32 temp = readmemw(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc += 2;
	temp |= (snes_readmem(snes_cpu.pbr | snes_cpu.pc) << 16);
	snes_cpu.pc++;
	return temp;
}

static inline UINT32 absolutelongx()
{
	UINT32 temp = (readmemw(snes_cpu.pbr | snes_cpu.pc)) + REG_XW();
	snes_cpu.pc += 2;
	temp += (snes_readmem(snes_cpu.pbr | snes_cpu.pc) << 16);
	snes_cpu.pc++;
	return temp;
}

static inline UINT32 zeropage() /*It's actually direct page, but I'm used to calling it zero page*/
{
	UINT32 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc++;
	temp += snes_cpu.dp;
	if (snes_cpu.dp & 0xFF)
	{
		snes_cpu.cycles -= 6;
		clockspc(6);
	}
	return temp & 0xFFFF;
}

static inline UINT32 zeropagex()
{
	UINT32 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc) + REG_XW();
	snes_cpu.pc++;
	if (snes_cpu.e)
	{
		temp &= 0xFF;
	}
	temp += snes_cpu.dp;
	if (snes_cpu.dp & 0xFF)
	{
		snes_cpu.cycles -= 6;
		clockspc(6);
	}
	return temp & 0xFFFF;
}

static inline UINT32 zeropagey()
{
	UINT32 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc) + REG_YW();
	snes_cpu.pc++;
	if (snes_cpu.e)
	{
		temp &= 0xFF;
	}
	temp += snes_cpu.dp;
	if (snes_cpu.dp & 0xFF)
	{
		snes_cpu.cycles -= 6;
		clockspc(6);
	}
	return temp & 0xFFFF;
}

static inline UINT32 stack()
{
	UINT32 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc++;
	temp += REG_SW();
	return temp & 0xFFFF;
}

static inline UINT32 indirect()
{
	UINT32 temp = (snes_readmem(snes_cpu.pbr | snes_cpu.pc) + snes_cpu.dp) & 0xFFFF;
	snes_cpu.pc++;
	return (readmemw(temp)) + snes_cpu.dbr;
}

static inline UINT32 indirectx()
{
	UINT32 temp = (snes_readmem(snes_cpu.pbr | snes_cpu.pc) + snes_cpu.dp + REG_XW()) & 0xFFFF;
	snes_cpu.pc++;
	return (readmemw(temp)) + snes_cpu.dbr;
}
static inline UINT32 jindirectx() /*JSR (,x) uses p.pbr instead of p.dbr, and 2 byte address insted of 1 + p.dp*/
{
	UINT32 temp = (snes_readmem(snes_cpu.pbr | snes_cpu.pc) + (snes_readmem((snes_cpu.pbr | snes_cpu.pc) + 1) << 8) + REG_XW()) + snes_cpu.pbr;
	snes_cpu.pc += 2;
	return temp;
}

static inline UINT32 indirecty()
{
	UINT32 temp = (snes_readmem(snes_cpu.pbr | snes_cpu.pc) + snes_cpu.dp) & 0xFFFF;
	snes_cpu.pc++;
	return (readmemw(temp)) + REG_YW() + snes_cpu.dbr;
}
static inline UINT32 sindirecty()
{
	UINT32 temp = (snes_readmem(snes_cpu.pbr | snes_cpu.pc) + REG_SW()) & 0xFFFF;
	snes_cpu.pc++;
	return (readmemw(temp)) + REG_YW() + snes_cpu.dbr;
}

static inline UINT32 indirectl()
{
	UINT32 temp = (snes_readmem(snes_cpu.pbr | snes_cpu.pc) + snes_cpu.dp) & 0xFFFF;
	snes_cpu.pc++;
	UINT32 address = readmemw(temp) | (snes_readmem(temp + 2) << 16);
	return address;
}

static inline UINT32 indirectly()
{
	UINT32 temp = (snes_readmem(snes_cpu.pbr | snes_cpu.pc) + snes_cpu.dp) & 0xFFFF;
	snes_cpu.pc++;
	UINT32 address = (readmemw(temp) | (snes_readmem(temp + 2) << 16)) + REG_YW();
	return address;
}

/*Flag setting*/
#define setzn8(v)	if(!(v)) SET_ZERO(); else CLEAR_ZERO(); \
					if ((v) & 0x80) SET_NEGATIVE(); else CLEAR_NEGATIVE();

#define setzn16(v) if(!(v)) SET_ZERO(); else CLEAR_ZERO(); \
					if ((v) & 0x8000) SET_NEGATIVE(); else CLEAR_NEGATIVE();

#define ADC8() tempw=REG_AL()+temp+CHECK_CARRY();                          \
	if (tempw>=0x100) SET_CARRY(); else CLEAR_CARRY(); \
	if (~(REG_AL() ^ temp) & (temp ^ (UINT8)tempw) & 0x80) SET_OVERFLOW(); else CLEAR_OVERFLOW();       \
	REG_AL()=tempw&0xFF;                                       \
	setzn8(REG_AL());                                          \



#define ADC16() templ=REG_AW()+tempw+CHECK_CARRY();                           \
	if (!((REG_AW()^tempw)&0x8000)&&((REG_AW()^templ)&0x8000)) SET_OVERFLOW(); else CLEAR_OVERFLOW();     \
	REG_AW()=templ&0xFFFF;                                       \
	setzn16(REG_AW());                                           \
	if (templ&0x10000) SET_CARRY(); else CLEAR_CARRY();

#define ADCBCD8() INT8 carry = CHECK_CARRY();   \
	tempw=(REG_AL()&0x0F)+(temp&0xF)+carry;     \
	if (tempw> 0x09)                            \
	{                                                       \
		tempw+=0x06;                                       \
	}                                                       \
	carry = tempw > 0x0f; \
	tempw=((REG_AL()&0xF0)+(temp&0xF0)+(tempw &0x0f) + (carry*0x10));                      \
	if ((REG_AL()&0x80) == (temp &0x80) &&  (REG_AL() & 0x80) != (tempw & 0x80)) SET_OVERFLOW(); else CLEAR_OVERFLOW();       \
	if (tempw>0x9F)                                         \
	{                                                       \
		tempw+=0x60;                                    \
	}    \
	if (tempw>0xFF) SET_CARRY(); else CLEAR_CARRY();                    \
	REG_AL() = tempw & 0xFF;                                       \
	setzn8(REG_AL());                                          \
	snes_cpu.cycles-=6; clockspc(6);

#define ADCBCD16()                                                      \
	templ=(REG_AW()&0xF)+(tempw&0xF)+CHECK_CARRY();                  \
	if (templ>9)                                            \
{                                                       \
	templ+=6;                                       \
}                                                       \
	templ+=((REG_AW()&0xF0)+(tempw&0xF0));                       \
	if (templ>0x9F)                                         \
{                                                       \
	templ+=0x60;                                    \
}                                                       \
	templ+=((REG_AW()&0xF00)+(tempw&0xF00));                     \
	if (templ>0x9FF)                                        \
{                                                       \
	templ+=0x600;                                   \
}                                                       \
	templ+=((REG_AW()&0xF000)+(tempw&0xF000));                   \
	if (templ>0x9FFF)                                       \
{                                                       \
	templ+=0x6000;                                  \
}                                                       \
	if (!((REG_AW()^tempw)&0x8000)&&((REG_AW()^templ)&0x8000)) SET_OVERFLOW(); else CLEAR_OVERFLOW();      \
	REG_AW()=templ&0xFFFF;                                       \
	setzn16(REG_AW());                                           \
	if (templ>0xFFFF) SET_CARRY(); else CLEAR_CARRY();                                  \
	snes_cpu.cycles-=6; clockspc(6);

#define SBC8()  tempw=REG_AL()-temp-((CHECK_CARRY())?0:1);                          \
	if (((REG_AL()^temp)&0x80)&&((REG_AL()^tempw)&0x80)) SET_OVERFLOW(); else CLEAR_OVERFLOW();        \
	REG_AL()=tempw&0xFF;                                       \
	setzn8(REG_AL());                                          \
	if (tempw<=0xFF) SET_CARRY(); else CLEAR_CARRY();
#define SBC16() templ=REG_AW()-tempw-((CHECK_CARRY())?0:1);                           \
	if (((REG_AW()^tempw)&(REG_AW()^templ))&0x8000) SET_OVERFLOW(); else CLEAR_OVERFLOW();                 \
	REG_AW()=templ&0xFFFF;                                       \
	setzn16(REG_AW());                                           \
	if (templ<=0xFFFF) SET_CARRY(); else CLEAR_CARRY();

#define SBCBCD8()                                                       \
	tempw=(REG_AL()&0xF)-(temp&0xF)-(CHECK_CARRY()?0:1);                 \
	if (tempw>9)                                            \
{                                                       \
	tempw-=6;                                       \
}                                                       \
	tempw+=((REG_AL()&0xF0)-(temp&0xF0));                      \
	if (tempw>0x9F)                                         \
{                                                       \
	tempw-=0x60;                                    \
}                                                       \
	if (((REG_AL()^temp)&0x80)&&((REG_AL()^tempw)&0x80)) SET_OVERFLOW(); else CLEAR_OVERFLOW();        \
	REG_AL()=tempw&0xFF;                                       \
	setzn8(REG_AL());                                          \
	if (tempw<=0xFF) SET_CARRY(); else CLEAR_CARRY();                                        \
	snes_cpu.cycles-=6; clockspc(6);

#define SBCBCD16()                                                      \
	templ=(REG_AW()&0xF)-(tempw&0xF)-(CHECK_CARRY()?0:1);                  \
	if (templ>9)                                            \
{                                                       \
	templ-=6;                                       \
}                                                       \
	templ+=((REG_AW()&0xF0)-(tempw&0xF0));                       \
	if (templ>0x9F)                                         \
{                                                       \
	templ-=0x60;                                    \
}                                                       \
	templ+=((REG_AW()&0xF00)-(tempw&0xF00));                     \
	if (templ>0x9FF)                                        \
{                                                       \
	templ-=0x600;                                   \
}                                                       \
	templ+=((REG_AW()&0xF000)-(tempw&0xF000));                   \
	if (templ>0x9FFF)                                       \
{                                                       \
	templ-=0x6000;                                  \
}                                                       \
	if (((REG_AW()^tempw)&0x8000)&&((REG_AW()^templ)&0x8000)) SET_OVERFLOW(); else CLEAR_OVERFLOW();        \
	REG_AW()=templ&0xFFFF;                                       \
	setzn16(REG_AW());                                           \
	if (templ<=0xFFFF) SET_CARRY(); else CLEAR_CARRY();                                    \
	snes_cpu.cycles-=6; clockspc(6);

/*Instructions*/
static inline void inca8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AL()++;
	setzn8(REG_AL());
}
static inline void inca16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AW()++;
	setzn16(REG_AW());
}
static inline void inx8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XL()++;
	setzn8(REG_XL());
}
static inline void inx16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XW()++;
	setzn16(REG_XW());
}
static inline void iny8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_YL()++;
	setzn8(REG_YL());
}
static inline void iny16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_YW()++;
	setzn16(REG_YW());
}

static inline void deca8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AL()--;
	setzn8(REG_AL());
}
static inline void deca16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AW()--;
	setzn16(REG_AW());
}
static inline void dex8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XL()--;
	setzn8(REG_XL());
}
static inline void dex16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XW()--;
	setzn16(REG_XW());
}
static inline void dey8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_YL()--;
	setzn8(REG_YL());
}
static inline void dey16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_YW()--;
	setzn16(REG_YW());
}

/*INC group*/
static inline void incZp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp++;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void incZp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp++;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void incZpx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp++;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void incZpx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp++;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void incAbs8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp++;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void incAbs16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp++;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void incAbsx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp++;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void incAbsx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolutex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp++;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

/*DEC group*/
static inline void decZp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp--;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void decZp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp--;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void decZpx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp--;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void decZpx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp--;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void decAbs8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp--;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void decAbs16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp--;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void decAbsx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp--;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void decAbsx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolutex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	temp--;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

/*Flag group*/
static inline void clc()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	CLEAR_CARRY();
}
static inline void cld()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	CLEAR_DECIMAL();
}
static inline void cli()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	CLEAR_IRQ();
}
static inline void clv()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	CLEAR_OVERFLOW();
}

static inline void sec()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	SET_CARRY();
}
static inline void sed()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	SET_DECIMAL();
}
static inline void sei()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	SET_IRQ();
}

static inline void xce()
{
	INT16 temp = CHECK_CARRY();

	if (snes_cpu.e)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	snes_cpu.e = temp;
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	updatecpumode();
}

static inline void sep()
{
	UINT8 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (temp & 1) SET_CARRY();
	if (temp & 2) SET_ZERO();
	if (temp & 4) SET_IRQ();
	if (temp & 8) SET_DECIMAL();
	if (temp & 0x40) SET_OVERFLOW();
	if (temp & 0x80) SET_NEGATIVE();
	if (!snes_cpu.e)
	{
		if (temp & 0x10) SET_INDEX();
		if (temp & 0x20) SET_MEMORYACC();
		updatecpumode();
	}
}

static inline void rep()
{
	UINT8 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (temp & 1) CLEAR_CARRY();
	if (temp & 2) CLEAR_ZERO();
	if (temp & 4) CLEAR_IRQ();
	if (temp & 8) CLEAR_DECIMAL();
	if (temp & 0x40) CLEAR_OVERFLOW();
	if (temp & 0x80) CLEAR_NEGATIVE();
	if (!snes_cpu.e)
	{
		if (temp & 0x10) CLEAR_INDEX();
		if (temp & 0x20) CLEAR_MEMORYACC();
		updatecpumode();
	}
}

/*Transfer group*/
static inline void tax8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XL() = REG_AL();
	setzn8(REG_XL());
}
static inline void tay8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_YL() = REG_AL();
	setzn8(REG_YL());
}
static inline void txa8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AL() = REG_XL();
	setzn8(REG_AL());
}
static inline void tya8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AL() = REG_YL();
	setzn8(REG_AL());
}
static inline void tsx8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XL() = REG_SL();
	setzn8(REG_XL());
}
static inline void txs8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SL() = REG_XL();
	//        setzn8(s.b.l);
}
static inline void txy8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_YL() = REG_XL();
	setzn8(REG_YL());
}
static inline void tyx8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XL() = REG_YL();
	setzn8(REG_XL());
}

static inline void tax16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XW() = REG_AW();
	setzn16(REG_XW());
}
static inline void tay16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_YW() = REG_AW();
	setzn16(REG_YW());
}
static inline void txa16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AW() = REG_XW();
	setzn16(REG_AW());
}
static inline void tya16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AW() = REG_YW();
	setzn16(REG_AW());
}
static inline void tsx16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XW() = REG_SW();
	setzn16(REG_XW());
}
static inline void txs16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW() = REG_XW();
	//        setzn16(s.w);
}
static inline void txy16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_YW() = REG_XW();
	setzn16(REG_YW());
}
static inline void tyx16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_XW() = REG_YW();
	setzn16(REG_XW());
}

/*LDX group*/
static inline void ldxImm8()
{
	REG_XL() = snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc++;
	setzn8(REG_XL());
}

static inline void ldxZp8()
{
	snes_cpu.tempAddr = zeropage();
	REG_XL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_XL());
}

static inline void ldxZpy8()
{
	snes_cpu.tempAddr = zeropagey();
	REG_XL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_XL());
}

static inline void ldxAbs8()
{
	snes_cpu.tempAddr = absolute();
	REG_XL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_XL());
}
static inline void ldxAbsy8()
{
	snes_cpu.tempAddr = absolutey();
	REG_XL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_XL());
}

static inline void ldxImm16()
{
	REG_XW() = readmemw(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc += 2;
	setzn16(REG_XW());
}
static inline void ldxZp16()
{
	snes_cpu.tempAddr = zeropage();
	REG_XW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_XW());
}
static inline void ldxZpy16()
{
	snes_cpu.tempAddr = zeropagey();
	REG_XW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_XW());
}
static inline void ldxAbs16()
{
	snes_cpu.tempAddr = absolute();
	REG_XW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_XW());
}
static inline void ldxAbsy16()
{
	snes_cpu.tempAddr = absolutey();
	REG_XW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_XW());
}

/*LDY group*/
static inline void ldyImm8()
{
	REG_YL() = snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc++;
	setzn8(REG_YL());
}
static inline void ldyZp8()
{
	snes_cpu.tempAddr = zeropage();
	REG_YL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_YL());
}
static inline void ldyZpx8()
{
	snes_cpu.tempAddr = zeropagex();
	REG_YL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_YL());
}
static inline void ldyAbs8()
{
	snes_cpu.tempAddr = absolute();
	REG_YL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_YL());
}
static inline void ldyAbsx8()
{
	snes_cpu.tempAddr = absolutex();
	REG_YL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_YL());
}

static inline void ldyImm16()
{
	REG_YW() = readmemw(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc += 2;
	setzn16(REG_YW());
}
static inline void ldyZp16()
{
	snes_cpu.tempAddr = zeropage();
	REG_YW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_YW());
}
static inline void ldyZpx16()
{
	snes_cpu.tempAddr = zeropagex();
	REG_YW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_YW());
}
static inline void ldyAbs16()
{
	snes_cpu.tempAddr = absolute();
	REG_YW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_YW());
}
static inline void ldyAbsx16()
{
	snes_cpu.tempAddr = absolutex();
	REG_YW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_YW());
}

/*LDA group*/
static inline void ldaImm8()
{
	REG_AL() = snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc++;
	setzn8(REG_AL());
}
static inline void ldaZp8()
{
	snes_cpu.tempAddr = zeropage();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaZpx8()
{
	snes_cpu.tempAddr = zeropagex();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaSp8()
{
	snes_cpu.tempAddr = stack();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaSIndirecty8()
{
	snes_cpu.tempAddr = sindirecty();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaAbs8()
{
	snes_cpu.tempAddr = absolute();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaAbsx8()
{
	snes_cpu.tempAddr = absolutex();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaAbsy8()
{
	snes_cpu.tempAddr = absolutey();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaLong8()
{
	snes_cpu.tempAddr = absolutelong();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaLongx8()
{
	snes_cpu.tempAddr = absolutelongx();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaIndirect8()
{
	snes_cpu.tempAddr = indirect();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaIndirectx8()
{
	snes_cpu.tempAddr = indirectx();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaIndirecty8()
{
	snes_cpu.tempAddr = indirecty();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaIndirectLong8()
{
	snes_cpu.tempAddr = indirectl();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void ldaIndirectLongy8()
{
	snes_cpu.tempAddr = indirectly();
	REG_AL() = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}

static inline void ldaImm16()
{
	REG_AW() = readmemw(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc += 2;
	setzn16(REG_AW());
}
static inline void ldaZp16()
{
	snes_cpu.tempAddr = zeropage();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaZpx16()
{
	snes_cpu.tempAddr = zeropagex();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaSp16()
{
	snes_cpu.tempAddr = stack();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaSIndirecty16()
{
	snes_cpu.tempAddr = sindirecty();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaAbs16()
{
	snes_cpu.tempAddr = absolute();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaAbsx16()
{
	snes_cpu.tempAddr = absolutex();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaAbsy16()
{
	snes_cpu.tempAddr = absolutey();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaLong16()
{
	snes_cpu.tempAddr = absolutelong();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaLongx16()
{
	snes_cpu.tempAddr = absolutelongx();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaIndirect16()
{
	snes_cpu.tempAddr = indirect();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaIndirectx16()
{
	snes_cpu.tempAddr = indirectx();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaIndirecty16()
{
	snes_cpu.tempAddr = indirecty();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaIndirectLong16()
{
	snes_cpu.tempAddr = indirectl();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void ldaIndirectLongy16()
{
	snes_cpu.tempAddr = indirectly();
	REG_AW() = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

/*STA group*/
static inline void staZp8()
{
	snes_cpu.tempAddr = zeropage();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staZpx8()
{
	snes_cpu.tempAddr = zeropagex();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staAbs8()
{
	snes_cpu.tempAddr = absolute();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staAbsx8()
{
	snes_cpu.tempAddr = absolutex();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staAbsy8()
{
	snes_cpu.tempAddr = absolutey();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staLong8()
{
	snes_cpu.tempAddr = absolutelong();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staLongx8()
{
	snes_cpu.tempAddr = absolutelongx();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staIndirect8()
{
	snes_cpu.tempAddr = indirect();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staIndirectx8()
{
	snes_cpu.tempAddr = indirectx();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staIndirecty8()
{
	snes_cpu.tempAddr = indirecty();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staIndirectLong8()
{
	snes_cpu.tempAddr = indirectl();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staIndirectLongy8()
{
	snes_cpu.tempAddr = indirectly();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staSp8()
{
	snes_cpu.tempAddr = stack();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}
static inline void staSIndirecty8()
{
	snes_cpu.tempAddr = sindirecty();
	snes_writemem(snes_cpu.tempAddr, REG_AL());
}

static inline void staZp16()
{
	snes_cpu.tempAddr = zeropage();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staZpx16()
{
	snes_cpu.tempAddr = zeropagex();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staAbs16()
{
	snes_cpu.tempAddr = absolute();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staAbsx16()
{
	snes_cpu.tempAddr = absolutex();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staAbsy16()
{
	snes_cpu.tempAddr = absolutey();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staLong16()
{
	snes_cpu.tempAddr = absolutelong();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staLongx16()
{
	snes_cpu.tempAddr = absolutelongx();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staIndirect16()
{
	snes_cpu.tempAddr = indirect();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staIndirectx16()
{
	snes_cpu.tempAddr = indirectx();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staIndirecty16()
{
	snes_cpu.tempAddr = indirecty();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staIndirectLong16()
{
	snes_cpu.tempAddr = indirectl();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staIndirectLongy16()
{
	snes_cpu.tempAddr = indirectly();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staSp16()
{
	snes_cpu.tempAddr = stack();
	writememw(snes_cpu.tempAddr, REG_AW());
}
static inline void staSIndirecty16()
{
	snes_cpu.tempAddr = sindirecty();
	writememw(snes_cpu.tempAddr, REG_AW());
}

/*STX group*/
static inline void stxZp8()
{
	snes_cpu.tempAddr = zeropage();
	snes_writemem(snes_cpu.tempAddr, REG_XL());
}
static inline void stxZpy8()
{
	snes_cpu.tempAddr = zeropagey();
	snes_writemem(snes_cpu.tempAddr, REG_XL());
}
static inline void stxAbs8()
{
	snes_cpu.tempAddr = absolute();
	snes_writemem(snes_cpu.tempAddr, REG_XL());
}

static inline void stxZp16()
{
	snes_cpu.tempAddr = zeropage();
	writememw(snes_cpu.tempAddr, REG_XW());
}
static inline void stxZpy16()
{
	snes_cpu.tempAddr = zeropagey();
	writememw(snes_cpu.tempAddr, REG_XW());
}
static inline void stxAbs16()
{
	snes_cpu.tempAddr = absolute();
	writememw(snes_cpu.tempAddr, REG_XW());
}

/*STY group*/
static inline void styZp8()
{
	snes_cpu.tempAddr = zeropage();
	snes_writemem(snes_cpu.tempAddr, REG_YL());
}
static inline void styZpx8()
{
	snes_cpu.tempAddr = zeropagex();
	snes_writemem(snes_cpu.tempAddr, REG_YL());
}
static inline void styAbs8()
{
	snes_cpu.tempAddr = absolute();
	snes_writemem(snes_cpu.tempAddr, REG_YL());
}

static inline void styZp16()
{
	snes_cpu.tempAddr = zeropage();
	writememw(snes_cpu.tempAddr, REG_YW());
}
static inline void styZpx16()
{
	snes_cpu.tempAddr = zeropagex();
	writememw(snes_cpu.tempAddr, REG_YW());
}
static inline void styAbs16()
{
	snes_cpu.tempAddr = absolute();
	writememw(snes_cpu.tempAddr, REG_YW());
}

/*STZ group*/
static inline void stzZp8()
{
	snes_cpu.tempAddr = zeropage();
	snes_writemem(snes_cpu.tempAddr, 0);
}
static inline void stzZpx8()
{
	snes_cpu.tempAddr = zeropagex();
	snes_writemem(snes_cpu.tempAddr, 0);
}
static inline void stzAbs8()
{
	snes_cpu.tempAddr = absolute();
	snes_writemem(snes_cpu.tempAddr, 0);
}
static inline void stzAbsx8()
{
	snes_cpu.tempAddr = absolutex();
	snes_writemem(snes_cpu.tempAddr, 0);
}

static inline void stzZp16()
{
	snes_cpu.tempAddr = zeropage();
	writememw(snes_cpu.tempAddr, 0);
}
static inline void stzZpx16()
{
	snes_cpu.tempAddr = zeropagex();
	writememw(snes_cpu.tempAddr, 0);
}
static inline void stzAbs16()
{
	snes_cpu.tempAddr = absolute();
	writememw(snes_cpu.tempAddr, 0);
}
static inline void stzAbsx16()
{
	snes_cpu.tempAddr = absolutex();
	writememw(snes_cpu.tempAddr, 0);
}

/*ADC group*/
static inline void adcImm8()
{
	UINT16 tempw;
	UINT8 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc++;
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcZp8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcZpx8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcSp8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = stack();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcAbs8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcAbsx8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcAbsy8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolutey();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcLong8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolutelong();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcLongx8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolutelongx();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcIndirect8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirect();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcIndirectx8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirectx();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcIndirecty8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirecty();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcsIndirecty8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = sindirecty();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcIndirectLong8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirectl();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL())
	{
		ADCBCD8();
	}
	else
	{
		ADC8();
	}
}
static inline void adcIndirectLongy8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirectly();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD8(); }
	else { ADC8(); }
}

static inline void adcImm16()
{
	UINT32 templ;
	UINT16 tempw;
	tempw = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcZp16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = zeropage();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcZpx16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = zeropagex();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcSp16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = stack();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcAbs16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolute();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcAbsx16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolutex();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcAbsy16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolutey();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcLong16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolutelong();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcLongx16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolutelongx();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcIndirect16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirect();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcIndirectx16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirectx();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcIndirecty16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirecty();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcsIndirecty16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = sindirecty();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcIndirectLong16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirectl();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}
static inline void adcIndirectLongy16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirectly();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { ADCBCD16(); }
	else { ADC16(); }
}

/*SBC group*/
static inline void sbcImm8()
{
	UINT16 tempw;
	UINT8 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcZp8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcZpx8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcSp8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = stack();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcAbs8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcAbsx8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcAbsy8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolutey();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcLong8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolutelong();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcLongx8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = absolutelongx();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcIndirect8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirect();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcIndirectx8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirectx();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcIndirecty8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirecty();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcIndirectLong8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirectl();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}
static inline void sbcIndirectLongy8()
{
	UINT16 tempw;
	UINT8 temp;
	snes_cpu.tempAddr = indirectly();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD8(); }
	else { SBC8(); }
}

static inline void sbcImm16()
{
	UINT32 templ;
	UINT16 tempw;
	tempw = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcZp16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = zeropage();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcZpx16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = zeropagex();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcSp16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = stack();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcAbs16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolute();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcAbsx16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolutex();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcAbsy16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolutey();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcLong16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolutelong();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcLongx16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = absolutelongx();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcIndirect16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirect();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcIndirectx16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirectx();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcIndirecty16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirecty();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcIndirectLong16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirectl();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}
static inline void sbcIndirectLongy16()
{
	UINT32 templ;
	UINT16 tempw;
	snes_cpu.tempAddr = indirectly();
	tempw = readmemw(snes_cpu.tempAddr);
	if (CHECK_DECIMAL()) { SBCBCD16(); }
	else { SBC16(); }
}

/*EOR group*/
static inline void eorImm8()
{
	REG_AL() ^= snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	setzn8(REG_AL());
}
static inline void eorZp8()
{
	snes_cpu.tempAddr = zeropage();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorZpx8()
{
	snes_cpu.tempAddr = zeropagex();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorSp8()
{
	snes_cpu.tempAddr = stack();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorAbs8()
{
	snes_cpu.tempAddr = absolute();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorAbsx8()
{
	snes_cpu.tempAddr = absolutex();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorAbsy8()
{
	snes_cpu.tempAddr = absolutey();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorLong8()
{
	snes_cpu.tempAddr = absolutelong();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorLongx8()
{
	snes_cpu.tempAddr = absolutelongx();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorIndirect8()
{
	snes_cpu.tempAddr = indirect();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void eorIndirectx8()
{
	snes_cpu.tempAddr = indirectx();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}

static inline void eorIndirecty8()
{
	snes_cpu.tempAddr = indirecty();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}

static inline void eorIndirectLong8()
{
	snes_cpu.tempAddr = indirectl();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}

static inline void eorIndirectLongy8()
{
	snes_cpu.tempAddr = indirectly();
	REG_AL() ^= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}

static inline void eorImm16()
{
	REG_AW() ^= readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	setzn16(REG_AW());
}

static inline void eorZp16()
{
	snes_cpu.tempAddr = zeropage();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorZpx16()
{
	snes_cpu.tempAddr = zeropagex();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorSp16()
{
	snes_cpu.tempAddr = stack();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorAbs16()
{
	snes_cpu.tempAddr = absolute();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorAbsx16()
{
	snes_cpu.tempAddr = absolutex();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorAbsy16()
{
	snes_cpu.tempAddr = absolutey();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorLong16()
{
	snes_cpu.tempAddr = absolutelong();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorLongx16()
{
	snes_cpu.tempAddr = absolutelongx();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorIndirect16()
{
	snes_cpu.tempAddr = indirect();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void eorIndirectx16()
{
	snes_cpu.tempAddr = indirectx();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorIndirecty16()
{
	snes_cpu.tempAddr = indirecty();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorIndirectLong16()
{
	snes_cpu.tempAddr = indirectl();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

static inline void eorIndirectLongy16()
{
	snes_cpu.tempAddr = indirectly();
	REG_AW() ^= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

/*AND group*/
static inline void andImm8()
{
	REG_AL() &= snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	setzn8(REG_AL());
}
static inline void andZp8()
{
	snes_cpu.tempAddr = zeropage();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andZpx8()
{
	snes_cpu.tempAddr = zeropagex();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}

static inline void andSp8()
{
	snes_cpu.tempAddr = stack();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andAbs8()
{
	snes_cpu.tempAddr = absolute();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andAbsx8()
{
	snes_cpu.tempAddr = absolutex();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andAbsy8()
{
	snes_cpu.tempAddr = absolutey();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andLong8()
{
	snes_cpu.tempAddr = absolutelong();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andLongx8()
{
	snes_cpu.tempAddr = absolutelongx();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andIndirect8()
{
	snes_cpu.tempAddr = indirect();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andIndirectx8()
{
	snes_cpu.tempAddr = indirectx();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andIndirecty8()
{
	snes_cpu.tempAddr = indirecty();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andIndirectLong8()
{
	snes_cpu.tempAddr = indirectl();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void andIndirectLongy8()
{
	snes_cpu.tempAddr = indirectly();
	REG_AL() &= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}

static inline void andImm16()
{
	REG_AW() &= readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	setzn16(REG_AW());
}
static inline void andZp16()
{
	snes_cpu.tempAddr = zeropage();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andZpx16()
{
	snes_cpu.tempAddr = zeropagex();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andSp16()
{
	snes_cpu.tempAddr = stack();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andAbs16()
{
	snes_cpu.tempAddr = absolute();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andAbsx16()
{
	snes_cpu.tempAddr = absolutex();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andAbsy16()
{
	snes_cpu.tempAddr = absolutey();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andLong16()
{
	snes_cpu.tempAddr = absolutelong();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andLongx16()
{
	snes_cpu.tempAddr = absolutelongx();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andIndirect16()
{
	snes_cpu.tempAddr = indirect();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andIndirectx16()
{
	snes_cpu.tempAddr = indirectx();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andIndirecty16()
{
	snes_cpu.tempAddr = indirecty();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andIndirectLong16()
{
	snes_cpu.tempAddr = indirectl();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void andIndirectLongy16()
{
	snes_cpu.tempAddr = indirectly();
	REG_AW() &= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

/*ORA group*/
static inline void oraImm8()
{
	REG_AL() |= snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	setzn8(REG_AL());
}
static inline void oraZp8()
{
	snes_cpu.tempAddr = zeropage();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraZpx8()
{
	snes_cpu.tempAddr = zeropagex();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraSp8()
{
	snes_cpu.tempAddr = stack();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraAbs8()
{
	snes_cpu.tempAddr = absolute();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraAbsx8()
{
	snes_cpu.tempAddr = absolutex();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraAbsy8()
{
	snes_cpu.tempAddr = absolutey();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraLong8()
{
	snes_cpu.tempAddr = absolutelong();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraLongx8()
{
	snes_cpu.tempAddr = absolutelongx();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraIndirect8()
{
	snes_cpu.tempAddr = indirect();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraIndirectx8()
{
	snes_cpu.tempAddr = indirectx();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraIndirecty8()
{
	snes_cpu.tempAddr = indirecty();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraIndirectLong8()
{
	snes_cpu.tempAddr = indirectl();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}
static inline void oraIndirectLongy8()
{
	snes_cpu.tempAddr = indirectly();
	REG_AL() |= snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL());
}

static inline void oraImm16()
{
	REG_AW() |= readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	setzn16(REG_AW());
}
static inline void oraZp16()
{
	snes_cpu.tempAddr = zeropage();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraZpx16()
{
	snes_cpu.tempAddr = zeropagex();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraSp16()
{
	snes_cpu.tempAddr = stack();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraAbs16()
{
	snes_cpu.tempAddr = absolute();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraAbsx16()
{
	snes_cpu.tempAddr = absolutex();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraAbsy16()
{
	snes_cpu.tempAddr = absolutey();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraLong16()
{
	snes_cpu.tempAddr = absolutelong();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraLongx16()
{
	snes_cpu.tempAddr = absolutelongx();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraIndirect16()
{
	snes_cpu.tempAddr = indirect();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraIndirectx16()
{
	snes_cpu.tempAddr = indirectx();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraIndirecty16()
{
	snes_cpu.tempAddr = indirecty();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraIndirectLong16()
{
	snes_cpu.tempAddr = indirectl();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}
static inline void oraIndirectLongy16()
{
	snes_cpu.tempAddr = indirectly();
	REG_AW() |= readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW());
}

/*BIT group*/
static inline void bitImm8()
{
	UINT8 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;

	if (!(temp & REG_AL()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
}
static inline void bitImm16()
{
	UINT16 temp = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;

	if (!(temp & REG_AW()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
}

static inline void bitZp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);

	if (!(temp & REG_AL()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	if (temp & 0x40)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x80)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
}
static inline void bitZp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);

	if (!(temp & REG_AW()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	if (temp & 0x4000)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x8000)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
}

static inline void bitZpx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);

	if (!(temp & REG_AL()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}

	if (temp & 0x40)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x80)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
}
static inline void bitZpx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = readmemw(snes_cpu.tempAddr);
	if (!(temp & REG_AW()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	if (temp & 0x4000)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x8000)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
}

static inline void bitAbs8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);

	if (!(temp & REG_AL()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}

	if (temp & 0x40)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x80)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
}
static inline void bitAbs16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);

	if (!(temp & REG_AW()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}

	if (temp & 0x4000)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x8000)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
}

static inline void bitAbsx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	if (!(temp & REG_AL()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	if (temp & 0x40)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x80)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
}
static inline void bitAbsx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolutex();
	temp = readmemw(snes_cpu.tempAddr);

	if (!(temp & REG_AW()))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	if (temp & 0x4000)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x8000)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
}

/*CMP group*/
static inline void cmpImm8()
{
	UINT8 temp;
	temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	setzn8(REG_AL() - temp);
	if ((REG_AL() >= temp))
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpZp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);

	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpZpx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpSp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = stack();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpAbs8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpAbsx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpAbsy8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolutey();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpLong8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolutelong();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpLongx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolutelongx();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirect8()
{
	UINT8 temp;
	snes_cpu.tempAddr = indirect();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirectx8()
{
	UINT8 temp;
	snes_cpu.tempAddr = indirectx();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirecty8()
{
	UINT8 temp;
	snes_cpu.tempAddr = indirecty();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirectLong8()
{
	UINT8 temp;
	snes_cpu.tempAddr = indirectl();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirectLongy8()
{
	UINT8 temp;
	snes_cpu.tempAddr = indirectly();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_AL() - temp);
	if (REG_AL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}

static inline void cmpImm16()
{
	UINT16 temp;
	temp = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpZp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpSp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = stack();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpZpx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropagex();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpAbs16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpAbsx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolutex();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpAbsy16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolutey();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpLong16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolutelong();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpLongx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolutelongx();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirect16()
{
	UINT16 temp;
	snes_cpu.tempAddr = indirect();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirectx16()
{
	UINT16 temp;
	snes_cpu.tempAddr = indirectx();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirecty16()
{
	UINT16 temp;
	snes_cpu.tempAddr = indirecty();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirectLong16()
{
	UINT16 temp;
	snes_cpu.tempAddr = indirectl();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}
static inline void cmpIndirectLongy16()
{
	UINT16 temp;
	snes_cpu.tempAddr = indirectly();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_AW() - temp);

	if (REG_AW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}

/*Stack Group*/
static inline void phb()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), snes_cpu.dbr >> 16);
	REG_SW()--;
}
static inline void phbe()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), snes_cpu.dbr >> 16);
	REG_SL()--;
}

static inline void phk()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), snes_cpu.pbr >> 16);
	REG_SW()--;
}
static inline void phke()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), snes_cpu.pbr >> 16);
	REG_SL()--;
}

static inline void pea()
{
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	snes_writemem(REG_SW(), snes_cpu.tempAddr >> 8);
	REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.tempAddr & 0xFF);
	REG_SW()--;
}

static inline void pei()
{
	snes_cpu.tempAddr = indirect();
	snes_writemem(REG_SW(), snes_cpu.tempAddr >> 8);
	REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.tempAddr & 0xFF);
	REG_SW()--;
}

static inline void per()
{
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	snes_cpu.tempAddr += snes_cpu.pc;
	snes_writemem(REG_SW(), snes_cpu.tempAddr >> 8);
	REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.tempAddr & 0xFF);
	REG_SW()--;
}

static inline void phd()
{
	snes_writemem(REG_SW(), snes_cpu.dp >> 8);
	REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.dp & 0xFF);
	REG_SW()--;
}
static inline void pld()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW()++;
	snes_cpu.cycles -= 6;
	clockspc(6);
	snes_cpu.dp = snes_readmem(REG_SW());
	REG_SW()++;
	snes_cpu.dp |= (snes_readmem(REG_SW()) << 8);
}

static inline void pha8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), REG_AL());
	REG_SW()--;
}
static inline void pha16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), REG_AH());
	REG_SW()--;
	snes_writemem(REG_SW(), REG_AL());
	REG_SW()--;
}

static inline void phx8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), REG_XL());
	REG_SW()--;
}
static inline void phx16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), REG_XH());
	REG_SW()--;
	snes_writemem(REG_SW(), REG_XL());
	REG_SW()--;
}

static inline void phy8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), REG_YL());
	REG_SW()--;
}
static inline void phy16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), REG_YH());
	REG_SW()--;
	snes_writemem(REG_SW(), REG_YL());
	REG_SW()--;
}

static inline void pla8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW()++;
	snes_cpu.cycles -= 6;
	clockspc(6);
	REG_AL() = snes_readmem(REG_SW());
	setzn8(REG_AL());
}
static inline void pla16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW()++; snes_cpu.cycles -= 6;
	clockspc(6);
	REG_AL() = snes_readmem(REG_SW());
	REG_SW()++;
	REG_AH() = snes_readmem(REG_SW());
	setzn16(REG_AW());
}

static inline void plx8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW()++;
	snes_cpu.cycles -= 6;
	clockspc(6);
	REG_XL() = snes_readmem(REG_SW());
	setzn8(REG_XL());
}
static inline void plx16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW()++; snes_cpu.cycles -= 6;
	clockspc(6);
	REG_XL() = snes_readmem(REG_SW());
	REG_SW()++;
	REG_XH() = snes_readmem(REG_SW());
	setzn16(REG_XW());
}

static inline void ply8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW()++;
	snes_cpu.cycles -= 6;
	clockspc(6);
	REG_YL() = snes_readmem(REG_SW());
	setzn8(REG_YL());
}
static inline void ply16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW()++;
	snes_cpu.cycles -= 6;
	clockspc(6);
	REG_YL() = snes_readmem(REG_SW());
	REG_SW()++; REG_YH() = snes_readmem(REG_SW());
	setzn16(REG_YW());
}

static inline void plb()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW()++;
	snes_cpu.cycles -= 6;
	clockspc(6);
	snes_cpu.dbr = snes_readmem(REG_SW()) << 16;
}
static inline void plbe()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SL()++;
	snes_cpu.cycles -= 6;
	clockspc(6);
	snes_cpu.dbr = snes_readmem(REG_SW()) << 16;
}

static inline void plp()
{
	UINT8 temp = snes_readmem(REG_SW() + 1); REG_SW()++;
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	if (temp & 2)
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}

	if (temp & 4)
	{
		SET_IRQ();
	}
	else
	{
		CLEAR_IRQ();
	}

	if (temp & 8)
	{
		SET_DECIMAL();
	}
	else
	{
		CLEAR_DECIMAL();
	}

	if (temp & 0x10)
	{
		SET_INDEX();
	}
	else
	{
		CLEAR_INDEX();
	}
	if (temp & 0x20)
	{
		SET_MEMORYACC();
	}
	else
	{
		CLEAR_MEMORYACC();
	}
	if (temp & 0x40)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}

	if (temp & 0x80)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}

	snes_cpu.cycles -= 12;
	clockspc(12);
	updatecpumode();
}
static inline void plpe()
{
	UINT8 temp;
	REG_SL()++; temp = snes_readmem(REG_SW());
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	if (temp & 2)
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	if (temp & 4)
	{
		SET_IRQ();
	}
	else
	{
		CLEAR_IRQ();
	}
	if (temp & 8)
	{
		SET_DECIMAL();
	}
	else
	{
		CLEAR_DECIMAL();
	}
	if (temp & 0x40)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x80)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}
	snes_cpu.cycles -= 12;
	clockspc(12);
}

static inline void php()
{
	UINT8 temp = (CHECK_CARRY()) ? 1 : 0;
	if (CHECK_ZERO()) temp |= 2;
	if (CHECK_IRQ()) temp |= 4;
	if (CHECK_DECIMAL()) temp |= 8;
	if (CHECK_OVERFLOW()) temp |= 0x40;
	if (CHECK_NEGATIVE()) temp |= 0x80;
	if (CHECK_INDEX()) temp |= 0x10;
	if (CHECK_MEMORYACC()) temp |= 0x20;
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), temp); REG_SW()--;
}
static inline void phpe()
{
	UINT8 temp = (CHECK_CARRY()) ? 1 : 0;
	if (CHECK_ZERO()) temp |= 2;
	if (CHECK_IRQ()) temp |= 4;
	if (CHECK_DECIMAL()) temp |= 8;
	if (CHECK_OVERFLOW()) temp |= 0x40;
	if (CHECK_NEGATIVE()) temp |= 0x80;
	temp |= 0x30;
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), temp);
	REG_SL()--;
}

/*CPX group*/
static inline void cpxImm8()
{
	UINT8 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	setzn8(REG_XL() - temp);

	if (REG_XL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}

static inline void cpxImm16()
{
	UINT16 temp = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	setzn16(REG_XW() - temp);

	if (REG_XW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}

static inline void cpxZp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_XL() - temp);

	if (REG_XL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}
static inline void cpxZp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_XW() - temp);

	if (REG_XW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}

static inline void cpxAbs8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_XL() - temp);
	if (REG_XL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}
static inline void cpxAbs16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_XW() - temp);

	if (REG_XW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}

/*CPY group*/
static inline void cpyImm8()
{
	UINT8 temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	setzn8(REG_YL() - temp);

	if (REG_YL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}
static inline void cpyImm16()
{
	UINT16 temp = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	setzn16(REG_YW() - temp);

	if (REG_YW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

}

static inline void cpyZp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_YL() - temp);

	if (REG_YL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}
static inline void cpyZp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_YW() - temp);
	if (REG_YW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}

static inline void cpyAbs8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	setzn8(REG_YL() - temp);
	if (REG_YL() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}

static inline void cpyAbs16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	setzn16(REG_YW() - temp);

	if (REG_YW() >= temp)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
}

/*Branch group*/
static inline void bcc()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (!CHECK_CARRY())
	{
		snes_cpu.pc += temp;
		snes_cpu.cycles -= 6;
		clockspc(6);
	}
}

static inline void bcs()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (CHECK_CARRY())
	{
		snes_cpu.pc += temp;
		snes_cpu.cycles -= 6;
		clockspc(6);
	}
}

static inline void beq()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (CHECK_ZERO())
	{
		snes_cpu.pc += temp;
		snes_cpu.cycles -= 6;
		clockspc(6);
	}
}

static inline void bne()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;

	if (!CHECK_ZERO())
	{
		snes_cpu.pc += temp;
		snes_cpu.cycles -= 6; clockspc(6);
	}

}

static inline void bpl()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (!CHECK_NEGATIVE())
	{
		snes_cpu.pc += temp;
		snes_cpu.cycles -= 6; clockspc(6);
	}
}

static inline void bmi()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (CHECK_NEGATIVE())
	{
		snes_cpu.pc += temp;
		snes_cpu.cycles -= 6; clockspc(6);
	}
}

static inline void bvc()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (!CHECK_OVERFLOW())
	{
		snes_cpu.pc += temp;
		snes_cpu.cycles -= 6; clockspc(6);
	}
}

static inline void bvs()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	if (CHECK_OVERFLOW())
	{
		snes_cpu.pc += temp;
		snes_cpu.cycles -= 6; clockspc(6);
	}
}

static inline void bra()
{
	INT8 temp = (INT8)snes_readmem(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc++;
	snes_cpu.pc += temp;
	snes_cpu.cycles -= 6; clockspc(6);
}

static inline void brl()
{
	UINT16 temp = readmemw(snes_cpu.pbr | snes_cpu.pc); snes_cpu.pc += 2;
	snes_cpu.pc += temp;
	snes_cpu.cycles -= 6; clockspc(6);
}

/*Jump group*/
static inline void jmp()
{
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc = snes_cpu.tempAddr & 0xFFFF;
}

static inline void jmplong()
{
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc) | (snes_readmem((snes_cpu.pbr | snes_cpu.pc) + 2) << 16);
	snes_cpu.pc = snes_cpu.tempAddr & 0xFFFF;
	snes_cpu.pbr = snes_cpu.tempAddr & 0xFF0000;
}

static inline void jmpind()
{
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc = readmemw(snes_cpu.tempAddr);
}

static inline void jmpindx()
{
	snes_cpu.tempAddr = (readmemw(snes_cpu.pbr | snes_cpu.pc)) + REG_XW() + snes_cpu.pbr;
	snes_cpu.pc = readmemw(snes_cpu.tempAddr);
}

static inline void jmlind()
{
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.pc = readmemw(snes_cpu.tempAddr);
	snes_cpu.pbr = snes_readmem(snes_cpu.tempAddr + 2) << 16;
}

static inline void jsr()
{
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc);  snes_cpu.pc++;
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), snes_cpu.pc >> 8);   REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.pc & 0xFF); REG_SW()--;
	snes_cpu.pc = snes_cpu.tempAddr & 0xFFFF;
}

static inline void jsre()
{
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc);  snes_cpu.pc++;
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), snes_cpu.pc >> 8);   REG_SL()--;
	snes_writemem(REG_SW(), snes_cpu.pc & 0xFF); REG_SL()--;
	snes_cpu.pc = snes_cpu.tempAddr & 0xFFFF;
}

static inline void jsrIndx()
{
	snes_cpu.tempAddr = jindirectx(); snes_cpu.pc--;
	snes_writemem(REG_SW(), snes_cpu.pc >> 8);   REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.pc & 0xFF); REG_SW()--;
	snes_cpu.pc = readmemw(snes_cpu.tempAddr);
}

static inline void jsrIndxe()
{
	snes_cpu.tempAddr = jindirectx(); snes_cpu.pc--;
	snes_writemem(REG_SW(), snes_cpu.pc >> 8);   REG_SL()--;
	snes_writemem(REG_SW(), snes_cpu.pc & 0xFF); REG_SL()--;
	snes_cpu.pc = readmemw(snes_cpu.tempAddr);
}

static inline void jsl()
{
	UINT8 temp;
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc);  snes_cpu.pc += 2;
	temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), snes_cpu.pbr >> 16); REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.pc >> 8);   REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.pc & 0xFF); REG_SW()--;
	snes_cpu.pc = snes_cpu.tempAddr & 0xFFFF;
	snes_cpu.pbr = temp << 16;
}

static inline void jsle()
{
	UINT8 temp;
	snes_cpu.tempAddr = readmemw(snes_cpu.pbr | snes_cpu.pc);  snes_cpu.pc += 2;
	temp = snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_writemem(REG_SW(), snes_cpu.pbr >> 16); REG_SL()--;
	snes_writemem(REG_SW(), snes_cpu.pc >> 8);   REG_SL()--;
	snes_writemem(REG_SW(), snes_cpu.pc & 0xFF); REG_SL()--;
	snes_cpu.pc = snes_cpu.tempAddr & 0xFFFF;
	snes_cpu.pbr = temp << 16;
}

static inline void rtl()
{
	snes_cpu.cycles -= 18; clockspc(18);
	snes_cpu.pc = readmemw(REG_SW() + 1); REG_SW() += 2;
	snes_cpu.pbr = snes_readmem(REG_SW() + 1) << 16; REG_SW()++;
	snes_cpu.pc++;
}

static inline void rtle()
{
	snes_cpu.cycles -= 18; clockspc(18);
	REG_SL()++; snes_cpu.pc = snes_readmem(REG_SW());
	REG_SL()++; snes_cpu.pc |= (snes_readmem(REG_SW()) << 8);
	REG_SL()++; snes_cpu.pbr = snes_readmem(REG_SW()) << 16;
}

static inline void rts()
{
	snes_cpu.cycles -= 18; clockspc(18);
	snes_cpu.pc = readmemw(REG_SW() + 1); REG_SW() += 2;
	snes_cpu.pc++;
}

static inline void rtse()
{
	snes_cpu.cycles -= 18; clockspc(18);
	REG_SL()++;
	snes_cpu.pc = snes_readmem(REG_SW());
	REG_SL()++;
	snes_cpu.pc |= (snes_readmem(REG_SW()) << 8);
}

static inline void rti()
{
	UINT8 temp;
	snes_cpu.cycles -= 6;
	REG_SW()++;
	clockspc(6);
	temp = snes_readmem(REG_SW());
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	if (temp & 2)
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	if (temp & 4)
	{
		SET_IRQ();
	}
	else
	{
		CLEAR_IRQ();
	}
	if (temp & 8)
	{
		SET_DECIMAL();
	}
	else
	{
		CLEAR_DECIMAL();
	}
	if (temp & 0x10)
	{
		SET_INDEX();
	}
	else
	{
		CLEAR_INDEX();
	}

	if (temp & 0x20)
	{
		SET_MEMORYACC();
	}
	else
	{
		CLEAR_MEMORYACC();
	}
	if (temp & 0x40)
	{
		SET_OVERFLOW();
	}
	else
	{
		CLEAR_OVERFLOW();
	}
	if (temp & 0x80)
	{
		SET_NEGATIVE();
	}
	else
	{
		CLEAR_NEGATIVE();
	}

	REG_SW()++;
	snes_cpu.pc = snes_readmem(REG_SW());
	REG_SW()++;
	snes_cpu.pc |= (snes_readmem(REG_SW()) << 8);
	REG_SW()++;
	snes_cpu.pbr = snes_readmem(REG_SW()) << 16;
	updatecpumode();
}

/*Shift group*/
static inline void asla8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	if (REG_AL() & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	REG_AL() <<= 1;
	setzn8(REG_AL());
}
static inline void asla16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);

	if (REG_AW() & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	REG_AW() <<= 1;
	setzn16(REG_AW());
}

static inline void aslZp8()
{
	UINT8 temp;

	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);

	if (temp & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	temp <<= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void aslZp16()
{
	UINT16 temp;

	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);

	if (temp & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void aslZpx8()
{
	UINT8 temp;

	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void aslZpx16()
{
	UINT16 temp;

	snes_cpu.tempAddr = zeropagex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void aslAbs8()
{
	UINT8 temp;

	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void aslAbs16()
{
	UINT16 temp;

	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void aslAbsx8()
{
	UINT8 temp;

	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void aslAbsx16()
{
	UINT16 temp;

	snes_cpu.tempAddr = absolutex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void lsra8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	if (REG_AL() & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	REG_AL() >>= 1;
	setzn8(REG_AL());
}
static inline void lsra16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);

	if (REG_AW() & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	REG_AW() >>= 1;
	setzn16(REG_AW());
}

static inline void lsrZp8()
{
	UINT8 temp;

	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);

	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void lsrZp16()
{
	UINT16 temp;

	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void lsrZpx8()
{
	UINT8 temp;

	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void lsrZpx16()
{
	UINT16 temp;

	snes_cpu.tempAddr = zeropagex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void lsrAbs8()
{
	UINT8 temp;

	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void lsrAbs16()
{
	UINT16 temp;

	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void lsrAbsx8()
{
	UINT8 temp;

	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void lsrAbsx16()
{
	UINT16 temp;

	snes_cpu.tempAddr = absolutex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void rola8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.tempAddr = CHECK_CARRY();
	if (REG_AL() & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	REG_AL() <<= 1;
	if (snes_cpu.tempAddr) REG_AL() |= 1;
	setzn8(REG_AL());
}

static inline void rola16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.tempAddr = CHECK_CARRY();

	if (REG_AW() & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	REG_AW() <<= 1;
	if (snes_cpu.tempAddr) REG_AW() |= 1;
	setzn16(REG_AW());
}

static inline void rolZp8()
{
	UINT8 temp;
	INT16 tempc;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();

	if (temp & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	if (tempc) temp |= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}

static inline void rolZp16()
{
	UINT16 temp;
	INT16 tempc;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();

	if (temp & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	temp <<= 1;
	if (tempc) temp |= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void rolZpx8()
{
	UINT8 temp;
	INT16 tempc;
	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();

	if (temp & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	temp <<= 1;
	if (tempc) temp |= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void rolZpx16()
{
	UINT16 temp;
	INT16 tempc;
	snes_cpu.tempAddr = zeropagex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();

	if (temp & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	temp <<= 1;
	if (tempc) temp |= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void rolAbs8()
{
	UINT8 temp;
	INT16 tempc;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();

	if (temp & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	temp <<= 1;
	if (tempc) temp |= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void rolAbs16()
{
	UINT16 temp;
	INT16 tempc;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	if (tempc) temp |= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void rolAbsx8()
{
	UINT8 temp;
	INT16 tempc;
	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();

	if (temp & 0x80)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	if (tempc) temp |= 1;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void rolAbsx16()
{
	UINT16 temp;
	INT16 tempc;
	snes_cpu.tempAddr = absolutex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 0x8000)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp <<= 1;
	if (tempc) temp |= 1;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void rora8()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.tempAddr = CHECK_CARRY();
	if (REG_AL() & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	REG_AL() >>= 1;
	if (snes_cpu.tempAddr) REG_AL() |= 0x80;
	setzn8(REG_AL());
}
static inline void rora16()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.tempAddr = CHECK_CARRY();
	if (REG_AW() & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}

	REG_AW() >>= 1;
	if (snes_cpu.tempAddr) REG_AW() |= 0x8000;
	setzn16(REG_AW());
}

static inline void rorZp8()
{
	UINT8 temp;
	INT16 tempc;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	if (tempc) temp |= 0x80;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void rorZp16()
{
	UINT16 temp;
	INT16 tempc;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	if (tempc) temp |= 0x8000;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void rorZpx8()
{
	UINT8 temp;
	INT16 tempc;
	snes_cpu.tempAddr = zeropagex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	if (tempc) temp |= 0x80;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void rorZpx16()
{
	UINT16 temp;
	INT16 tempc;
	snes_cpu.tempAddr = zeropagex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	if (tempc) temp |= 0x8000;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void rorAbs8()
{
	UINT8 temp;
	INT16 tempc;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	if (tempc) temp |= 0x80;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void rorAbs16()
{
	UINT16 temp;
	INT16 tempc;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	if (tempc) temp |= 0x8000;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void rorAbsx8()
{
	UINT8 temp;
	INT16 tempc;
	snes_cpu.tempAddr = absolutex();
	temp = snes_readmem(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6; clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	if (tempc) temp |= 0x80;
	setzn8(temp);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void rorAbsx16()
{
	UINT16 temp;
	INT16 tempc;
	snes_cpu.tempAddr = absolutex();
	temp = readmemw(snes_cpu.tempAddr);
	snes_cpu.cycles -= 6;
	clockspc(6);
	tempc = CHECK_CARRY();
	if (temp & 1)
	{
		SET_CARRY();
	}
	else
	{
		CLEAR_CARRY();
	}
	temp >>= 1;
	if (tempc)
		temp |= 0x8000;
	setzn16(temp);
	writememw2(snes_cpu.tempAddr, temp);
}

/*Misc group*/
static inline void xba()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AW() = (REG_AW() >> 8) | (REG_AW() << 8);
	setzn8(REG_AL());
}
static inline void nop()
{
	snes_cpu.cycles -= 6; clockspc(6);
}

static inline void tcd()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.dp = REG_AW();
	setzn16(snes_cpu.dp);
}

static inline void tdc()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AW() = snes_cpu.dp;
	setzn16(REG_AW());
}

static inline void tcs()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_SW() = REG_AW();
}

static inline void tsc()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	REG_AW() = REG_SW();
	setzn16(REG_AW());
}

static inline void trbZp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);

	if (!(REG_AL() & temp))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	temp &= ~REG_AL();
	snes_cpu.cycles -= 6; clockspc(6);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void trbZp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);

	if (!(REG_AW() & temp))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	temp &= ~REG_AW();
	snes_cpu.cycles -= 6; clockspc(6);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void trbAbs8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);

	if (!(REG_AL() & temp))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	temp &= ~REG_AL();
	snes_cpu.cycles -= 6; clockspc(6);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void trbAbs16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);
	if (!(REG_AW() & temp))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	temp &= ~REG_AW();
	snes_cpu.cycles -= 6; clockspc(6);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void tsbZp8()
{
	UINT8 temp;
	snes_cpu.tempAddr = zeropage();
	temp = snes_readmem(snes_cpu.tempAddr);

	if (!(REG_AL() & temp))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}

	temp |= REG_AL();
	snes_cpu.cycles -= 6; clockspc(6);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void tsbZp16()
{
	UINT16 temp;
	snes_cpu.tempAddr = zeropage();
	temp = readmemw(snes_cpu.tempAddr);

	if (!(REG_AW() & temp))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	temp |= REG_AW();
	snes_cpu.cycles -= 6; clockspc(6);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void tsbAbs8()
{
	UINT8 temp;
	snes_cpu.tempAddr = absolute();
	temp = snes_readmem(snes_cpu.tempAddr);

	if (!(REG_AL() & temp))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	temp |= REG_AL();
	snes_cpu.cycles -= 6; clockspc(6);
	snes_writemem(snes_cpu.tempAddr, temp);
}
static inline void tsbAbs16()
{
	UINT16 temp;
	snes_cpu.tempAddr = absolute();
	temp = readmemw(snes_cpu.tempAddr);

	if (!(REG_AW() & temp))
	{
		SET_ZERO();
	}
	else
	{
		CLEAR_ZERO();
	}
	temp |= REG_AW();
	snes_cpu.cycles -= 6; clockspc(6);
	writememw2(snes_cpu.tempAddr, temp);
}

static inline void wai()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.inwai = 1;
	snes_cpu.pc--;

}

static inline void mvp()
{
	UINT8 temp;
	snes_cpu.dbr = (snes_readmem(snes_cpu.pbr | snes_cpu.pc)) << 16; snes_cpu.pc++;
	snes_cpu.tempAddr = (snes_readmem(snes_cpu.pbr | snes_cpu.pc)) << 16; snes_cpu.pc++;
	temp = snes_readmem(snes_cpu.tempAddr | REG_XW());
	snes_writemem(snes_cpu.dbr | REG_YW(), temp);
	REG_XW()--;
	REG_YW()--;
	REG_AW()--;
	if (REG_AW() != 0xFFFF) snes_cpu.pc -= 3;
	snes_cpu.cycles -= 12; clockspc(12);
}

static inline void mvn()
{
	UINT8 temp;
	snes_cpu.dbr = (snes_readmem(snes_cpu.pbr | snes_cpu.pc)) << 16; snes_cpu.pc++;
	snes_cpu.tempAddr = (snes_readmem(snes_cpu.pbr | snes_cpu.pc)) << 16; snes_cpu.pc++;
	temp = snes_readmem(snes_cpu.tempAddr | REG_XW());
	snes_writemem(snes_cpu.dbr | REG_YW(), temp);
	REG_XW()++;
	REG_YW()++;
	REG_AW()--;
	if (REG_AW() != 0xFFFF) snes_cpu.pc -= 3;
	snes_cpu.cycles -= 12; clockspc(12);
}

static inline void brk()
{
	UINT8 temp = 0;
	snes_writemem(REG_SW(), snes_cpu.pbr >> 16);
	REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.pc >> 8);
	REG_SW()--;
	snes_writemem(REG_SW(), snes_cpu.pc & 0xFF);
	REG_SW()--;

	if (CHECK_CARRY()) temp |= 1;
	if (CHECK_ZERO()) temp |= 2;
	if (CHECK_IRQ()) temp |= 4;
	if (CHECK_DECIMAL()) temp |= 8;
	if (CHECK_INDEX()) temp |= 0x10;
	if (CHECK_MEMORYACC()) temp |= 0x20;
	if (CHECK_OVERFLOW()) temp |= 0x40;
	if (CHECK_NEGATIVE()) temp |= 0x80;
	snes_writemem(REG_SW(), temp);
	REG_SW()--;
	snes_cpu.pc = readmemw(0xFFE6);
	snes_cpu.pbr = 0;
	SET_IRQ();
	CLEAR_DECIMAL();
}

/*Functions*/
void reset65816()
{
	snes_cpu.pbr = snes_cpu.dbr = 0;
	REG_SW() = 0x1FF;
	snes_cpu.cpumode = 4;
	snes_cpu.e = 1;
	SET_IRQ();
	snes_cpu.pc = readmemw(0xFFFC);
	REG_AW() = REG_XW() = REG_YW() = 0;
	SET_INDEX();
	SET_MEMORYACC();
}

static inline void badopcode()
{
	//snemlog(L"Bad opcode %02X\n",opcode);
	snes_cpu.pc--;
}

void makeopcodetable()
{
	INT16 c, d;
	for (c = 0; c < 256; c++)
	{
		for (d = 0; d < 5; d++)
		{
			opcodes[c][d] = badopcode;
		}
	}
	/*LDA group*/
	opcodes[0xA9][0] = opcodes[0xA9][2] = opcodes[0xA9][4] = ldaImm8;
	opcodes[0xA9][1] = opcodes[0xA9][3] = ldaImm16;
	opcodes[0xA5][0] = opcodes[0xA5][2] = opcodes[0xA5][4] = ldaZp8;
	opcodes[0xA5][1] = opcodes[0xA5][3] = ldaZp16;
	opcodes[0xB5][0] = opcodes[0xB5][2] = opcodes[0xB5][4] = ldaZpx8;
	opcodes[0xB5][1] = opcodes[0xB5][3] = ldaZpx16;
	opcodes[0xA3][0] = opcodes[0xA3][2] = opcodes[0xA3][4] = ldaSp8;
	opcodes[0xA3][1] = opcodes[0xA3][3] = ldaSp16;
	opcodes[0xB3][0] = opcodes[0xB3][2] = opcodes[0xB3][4] = ldaSIndirecty8;
	opcodes[0xB3][1] = opcodes[0xB3][3] = ldaSIndirecty16;
	opcodes[0xAD][0] = opcodes[0xAD][2] = opcodes[0xAD][4] = ldaAbs8;
	opcodes[0xAD][1] = opcodes[0xAD][3] = ldaAbs16;
	opcodes[0xBD][0] = opcodes[0xBD][2] = opcodes[0xBD][4] = ldaAbsx8;
	opcodes[0xBD][1] = opcodes[0xBD][3] = ldaAbsx16;
	opcodes[0xB9][0] = opcodes[0xB9][2] = opcodes[0xB9][4] = ldaAbsy8;
	opcodes[0xB9][1] = opcodes[0xB9][3] = ldaAbsy16;
	opcodes[0xAF][0] = opcodes[0xAF][2] = opcodes[0xAF][4] = ldaLong8;
	opcodes[0xAF][1] = opcodes[0xAF][3] = ldaLong16;
	opcodes[0xBF][0] = opcodes[0xBF][2] = opcodes[0xBF][4] = ldaLongx8;
	opcodes[0xBF][1] = opcodes[0xBF][3] = ldaLongx16;
	opcodes[0xB2][0] = opcodes[0xB2][2] = opcodes[0xB2][4] = ldaIndirect8;
	opcodes[0xB2][1] = opcodes[0xB2][3] = ldaIndirect16;
	opcodes[0xA1][0] = opcodes[0xA1][2] = opcodes[0xA1][4] = ldaIndirectx8;
	opcodes[0xA1][1] = opcodes[0xA1][3] = ldaIndirectx16;
	opcodes[0xB1][0] = opcodes[0xB1][2] = opcodes[0xB1][4] = ldaIndirecty8;
	opcodes[0xB1][1] = opcodes[0xB1][3] = ldaIndirecty16;
	opcodes[0xA7][0] = opcodes[0xA7][2] = opcodes[0xA7][4] = ldaIndirectLong8;
	opcodes[0xA7][1] = opcodes[0xA7][3] = ldaIndirectLong16;
	opcodes[0xB7][0] = opcodes[0xB7][2] = opcodes[0xB7][4] = ldaIndirectLongy8;
	opcodes[0xB7][1] = opcodes[0xB7][3] = ldaIndirectLongy16;
	/*LDX group*/
	opcodes[0xA2][0] = opcodes[0xA2][1] = opcodes[0xA2][4] = ldxImm8;
	opcodes[0xA2][2] = opcodes[0xA2][3] = ldxImm16;
	opcodes[0xA6][0] = opcodes[0xA6][1] = opcodes[0xA6][4] = ldxZp8;
	opcodes[0xA6][2] = opcodes[0xA6][3] = ldxZp16;
	opcodes[0xB6][0] = opcodes[0xB6][1] = opcodes[0xB6][4] = ldxZpy8;
	opcodes[0xB6][2] = opcodes[0xB6][3] = ldxZpy16;
	opcodes[0xAE][0] = opcodes[0xAE][1] = opcodes[0xAE][4] = ldxAbs8;
	opcodes[0xAE][2] = opcodes[0xAE][3] = ldxAbs16;
	opcodes[0xBE][0] = opcodes[0xBE][1] = opcodes[0xBE][4] = ldxAbsy8;
	opcodes[0xBE][2] = opcodes[0xBE][3] = ldxAbsy16;
	/*LDY group*/
	opcodes[0xA0][0] = opcodes[0xA0][1] = opcodes[0xA0][4] = ldyImm8;
	opcodes[0xA0][2] = opcodes[0xA0][3] = ldyImm16;
	opcodes[0xA4][0] = opcodes[0xA4][1] = opcodes[0xA4][4] = ldyZp8;
	opcodes[0xA4][2] = opcodes[0xA4][3] = ldyZp16;
	opcodes[0xB4][0] = opcodes[0xB4][1] = opcodes[0xB4][4] = ldyZpx8;
	opcodes[0xB4][2] = opcodes[0xB4][3] = ldyZpx16;
	opcodes[0xAC][0] = opcodes[0xAC][1] = opcodes[0xAC][4] = ldyAbs8;
	opcodes[0xAC][2] = opcodes[0xAC][3] = ldyAbs16;
	opcodes[0xBC][0] = opcodes[0xBC][1] = opcodes[0xBC][4] = ldyAbsx8;
	opcodes[0xBC][2] = opcodes[0xBC][3] = ldyAbsx16;

	/*STA group*/
	opcodes[0x85][0] = opcodes[0x85][2] = opcodes[0x85][4] = staZp8;
	opcodes[0x85][1] = opcodes[0x85][3] = staZp16;
	opcodes[0x95][0] = opcodes[0x95][2] = opcodes[0x95][4] = staZpx8;
	opcodes[0x95][1] = opcodes[0x95][3] = staZpx16;
	opcodes[0x8D][0] = opcodes[0x8D][2] = opcodes[0x8D][4] = staAbs8;
	opcodes[0x8D][1] = opcodes[0x8D][3] = staAbs16;
	opcodes[0x9D][0] = opcodes[0x9D][2] = opcodes[0x9D][4] = staAbsx8;
	opcodes[0x9D][1] = opcodes[0x9D][3] = staAbsx16;
	opcodes[0x99][0] = opcodes[0x99][2] = opcodes[0x99][4] = staAbsy8;
	opcodes[0x99][1] = opcodes[0x99][3] = staAbsy16;
	opcodes[0x8F][0] = opcodes[0x8F][2] = opcodes[0x8F][4] = staLong8;
	opcodes[0x8F][1] = opcodes[0x8F][3] = staLong16;
	opcodes[0x9F][0] = opcodes[0x9F][2] = opcodes[0x9F][4] = staLongx8;
	opcodes[0x9F][1] = opcodes[0x9F][3] = staLongx16;
	opcodes[0x92][0] = opcodes[0x92][2] = opcodes[0x92][4] = staIndirect8;
	opcodes[0x92][1] = opcodes[0x92][3] = staIndirect16;
	opcodes[0x81][0] = opcodes[0x81][2] = opcodes[0x81][4] = staIndirectx8;
	opcodes[0x81][1] = opcodes[0x81][3] = staIndirectx16;
	opcodes[0x91][0] = opcodes[0x91][2] = opcodes[0x91][4] = staIndirecty8;
	opcodes[0x91][1] = opcodes[0x91][3] = staIndirecty16;
	opcodes[0x87][0] = opcodes[0x87][2] = opcodes[0x87][4] = staIndirectLong8;
	opcodes[0x87][1] = opcodes[0x87][3] = staIndirectLong16;
	opcodes[0x97][0] = opcodes[0x97][2] = opcodes[0x97][4] = staIndirectLongy8;
	opcodes[0x97][1] = opcodes[0x97][3] = staIndirectLongy16;
	opcodes[0x83][0] = opcodes[0x83][2] = opcodes[0x83][4] = staSp8;
	opcodes[0x83][1] = opcodes[0x83][3] = staSp16;
	opcodes[0x93][0] = opcodes[0x93][2] = opcodes[0x93][4] = staSIndirecty8;
	opcodes[0x93][1] = opcodes[0x93][3] = staSIndirecty16;
	/*STX group*/
	opcodes[0x86][0] = opcodes[0x86][1] = opcodes[0x86][4] = stxZp8;
	opcodes[0x86][2] = opcodes[0x86][3] = stxZp16;
	opcodes[0x96][0] = opcodes[0x96][1] = opcodes[0x96][4] = stxZpy8;
	opcodes[0x96][2] = opcodes[0x96][3] = stxZpy16;
	opcodes[0x8E][0] = opcodes[0x8E][1] = opcodes[0x8E][4] = stxAbs8;
	opcodes[0x8E][2] = opcodes[0x8E][3] = stxAbs16;
	/*STY group*/
	opcodes[0x84][0] = opcodes[0x84][1] = opcodes[0x84][4] = styZp8;
	opcodes[0x84][2] = opcodes[0x84][3] = styZp16;
	opcodes[0x94][0] = opcodes[0x94][1] = opcodes[0x94][4] = styZpx8;
	opcodes[0x94][2] = opcodes[0x94][3] = styZpx16;
	opcodes[0x8C][0] = opcodes[0x8C][1] = opcodes[0x8C][4] = styAbs8;
	opcodes[0x8C][2] = opcodes[0x8C][3] = styAbs16;
	/*STZ group*/
	opcodes[0x64][0] = opcodes[0x64][2] = opcodes[0x64][4] = stzZp8;
	opcodes[0x64][1] = opcodes[0x64][3] = stzZp16;
	opcodes[0x74][0] = opcodes[0x74][2] = opcodes[0x74][4] = stzZpx8;
	opcodes[0x74][1] = opcodes[0x74][3] = stzZpx16;
	opcodes[0x9C][0] = opcodes[0x9C][2] = opcodes[0x9C][4] = stzAbs8;
	opcodes[0x9C][1] = opcodes[0x9C][3] = stzAbs16;
	opcodes[0x9E][0] = opcodes[0x9E][2] = opcodes[0x9E][4] = stzAbsx8;
	opcodes[0x9E][1] = opcodes[0x9E][3] = stzAbsx16;

	opcodes[0x3A][0] = opcodes[0x3A][2] = opcodes[0x3A][4] = deca8;
	opcodes[0x3A][1] = opcodes[0x3A][3] = deca16;
	opcodes[0xCA][0] = opcodes[0xCA][1] = opcodes[0xCA][4] = dex8;
	opcodes[0xCA][2] = opcodes[0xCA][3] = dex16;
	opcodes[0x88][0] = opcodes[0x88][1] = opcodes[0x88][4] = dey8;
	opcodes[0x88][2] = opcodes[0x88][3] = dey16;
	opcodes[0x1A][0] = opcodes[0x1A][2] = opcodes[0x1A][4] = inca8;
	opcodes[0x1A][1] = opcodes[0x1A][3] = inca16;
	opcodes[0xE8][0] = opcodes[0xE8][1] = opcodes[0xE8][4] = inx8;
	opcodes[0xE8][2] = opcodes[0xE8][3] = inx16;
	opcodes[0xC8][0] = opcodes[0xC8][1] = opcodes[0xC8][4] = iny8;
	opcodes[0xC8][2] = opcodes[0xC8][3] = iny16;

	/*INC group*/
	opcodes[0xE6][0] = opcodes[0xE6][2] = opcodes[0xE6][4] = incZp8;
	opcodes[0xE6][1] = opcodes[0xE6][3] = incZp16;
	opcodes[0xF6][0] = opcodes[0xF6][2] = opcodes[0xF6][4] = incZpx8;
	opcodes[0xF6][1] = opcodes[0xF6][3] = incZpx16;
	opcodes[0xEE][0] = opcodes[0xEE][2] = opcodes[0xEE][4] = incAbs8;
	opcodes[0xEE][1] = opcodes[0xEE][3] = incAbs16;
	opcodes[0xFE][0] = opcodes[0xFE][2] = opcodes[0xFE][4] = incAbsx8;
	opcodes[0xFE][1] = opcodes[0xFE][3] = incAbsx16;

	/*DEC group*/
	opcodes[0xC6][0] = opcodes[0xC6][2] = opcodes[0xC6][4] = decZp8;
	opcodes[0xC6][1] = opcodes[0xC6][3] = decZp16;
	opcodes[0xD6][0] = opcodes[0xD6][2] = opcodes[0xD6][4] = decZpx8;
	opcodes[0xD6][1] = opcodes[0xD6][3] = decZpx16;
	opcodes[0xCE][0] = opcodes[0xCE][2] = opcodes[0xCE][4] = decAbs8;
	opcodes[0xCE][1] = opcodes[0xCE][3] = decAbs16;
	opcodes[0xDE][0] = opcodes[0xDE][2] = opcodes[0xDE][4] = decAbsx8;
	opcodes[0xDE][1] = opcodes[0xDE][3] = decAbsx16;

	/*AND group*/
	opcodes[0x29][0] = opcodes[0x29][2] = opcodes[0x29][4] = andImm8;
	opcodes[0x29][1] = opcodes[0x29][3] = andImm16;
	opcodes[0x25][0] = opcodes[0x25][2] = opcodes[0x25][4] = andZp8;
	opcodes[0x25][1] = opcodes[0x25][3] = andZp16;
	opcodes[0x35][0] = opcodes[0x35][2] = opcodes[0x35][4] = andZpx8;
	opcodes[0x35][1] = opcodes[0x35][3] = andZpx16;
	opcodes[0x23][0] = opcodes[0x23][2] = opcodes[0x23][4] = andSp8;
	opcodes[0x23][1] = opcodes[0x23][3] = andSp16;
	opcodes[0x2D][0] = opcodes[0x2D][2] = opcodes[0x2D][4] = andAbs8;
	opcodes[0x2D][1] = opcodes[0x2D][3] = andAbs16;
	opcodes[0x3D][0] = opcodes[0x3D][2] = opcodes[0x3D][4] = andAbsx8;
	opcodes[0x3D][1] = opcodes[0x3D][3] = andAbsx16;
	opcodes[0x39][0] = opcodes[0x39][2] = opcodes[0x39][4] = andAbsy8;
	opcodes[0x39][1] = opcodes[0x39][3] = andAbsy16;
	opcodes[0x2F][0] = opcodes[0x2F][2] = opcodes[0x2F][4] = andLong8;
	opcodes[0x2F][1] = opcodes[0x2F][3] = andLong16;
	opcodes[0x3F][0] = opcodes[0x3F][2] = opcodes[0x3F][4] = andLongx8;
	opcodes[0x3F][1] = opcodes[0x3F][3] = andLongx16;
	opcodes[0x32][0] = opcodes[0x32][2] = opcodes[0x32][4] = andIndirect8;
	opcodes[0x32][1] = opcodes[0x32][3] = andIndirect16;
	opcodes[0x21][0] = opcodes[0x21][2] = opcodes[0x21][4] = andIndirectx8;
	opcodes[0x21][1] = opcodes[0x21][3] = andIndirectx16;
	opcodes[0x31][0] = opcodes[0x31][2] = opcodes[0x31][4] = andIndirecty8;
	opcodes[0x31][1] = opcodes[0x31][3] = andIndirecty16;
	opcodes[0x27][0] = opcodes[0x27][2] = opcodes[0x27][4] = andIndirectLong8;
	opcodes[0x27][1] = opcodes[0x27][3] = andIndirectLong16;
	opcodes[0x37][0] = opcodes[0x37][2] = opcodes[0x37][4] = andIndirectLongy8;
	opcodes[0x37][1] = opcodes[0x37][3] = andIndirectLongy16;

	/*EOR group*/
	opcodes[0x49][0] = opcodes[0x49][2] = opcodes[0x49][4] = eorImm8;
	opcodes[0x49][1] = opcodes[0x49][3] = eorImm16;
	opcodes[0x45][0] = opcodes[0x45][2] = opcodes[0x45][4] = eorZp8;
	opcodes[0x45][1] = opcodes[0x45][3] = eorZp16;
	opcodes[0x55][0] = opcodes[0x55][2] = opcodes[0x55][4] = eorZpx8;
	opcodes[0x55][1] = opcodes[0x55][3] = eorZpx16;
	opcodes[0x43][0] = opcodes[0x43][2] = opcodes[0x43][4] = eorSp8;
	opcodes[0x43][1] = opcodes[0x43][3] = eorSp16;
	opcodes[0x4D][0] = opcodes[0x4D][2] = opcodes[0x4D][4] = eorAbs8;
	opcodes[0x4D][1] = opcodes[0x4D][3] = eorAbs16;
	opcodes[0x5D][0] = opcodes[0x5D][2] = opcodes[0x5D][4] = eorAbsx8;
	opcodes[0x5D][1] = opcodes[0x5D][3] = eorAbsx16;
	opcodes[0x59][0] = opcodes[0x59][2] = opcodes[0x59][4] = eorAbsy8;
	opcodes[0x59][1] = opcodes[0x59][3] = eorAbsy16;
	opcodes[0x4F][0] = opcodes[0x4F][2] = opcodes[0x4F][4] = eorLong8;
	opcodes[0x4F][1] = opcodes[0x4F][3] = eorLong16;
	opcodes[0x5F][0] = opcodes[0x5F][2] = opcodes[0x5F][4] = eorLongx8;
	opcodes[0x5F][1] = opcodes[0x5F][3] = eorLongx16;
	opcodes[0x52][0] = opcodes[0x52][2] = opcodes[0x52][4] = eorIndirect8;
	opcodes[0x52][1] = opcodes[0x52][3] = eorIndirect16;
	opcodes[0x41][0] = opcodes[0x41][2] = opcodes[0x41][4] = eorIndirectx8;
	opcodes[0x41][1] = opcodes[0x41][3] = eorIndirectx16;
	opcodes[0x51][0] = opcodes[0x51][2] = opcodes[0x51][4] = eorIndirecty8;
	opcodes[0x51][1] = opcodes[0x51][3] = eorIndirecty16;
	opcodes[0x47][0] = opcodes[0x47][2] = opcodes[0x47][4] = eorIndirectLong8;
	opcodes[0x47][1] = opcodes[0x47][3] = eorIndirectLong16;
	opcodes[0x57][0] = opcodes[0x57][2] = opcodes[0x57][4] = eorIndirectLongy8;
	opcodes[0x57][1] = opcodes[0x57][3] = eorIndirectLongy16;

	/*ORA group*/
	opcodes[0x09][0] = opcodes[0x09][2] = opcodes[0x09][4] = oraImm8;
	opcodes[0x09][1] = opcodes[0x09][3] = oraImm16;
	opcodes[0x05][0] = opcodes[0x05][2] = opcodes[0x05][4] = oraZp8;
	opcodes[0x05][1] = opcodes[0x05][3] = oraZp16;
	opcodes[0x15][0] = opcodes[0x15][2] = opcodes[0x15][4] = oraZpx8;
	opcodes[0x15][1] = opcodes[0x15][3] = oraZpx16;
	opcodes[0x03][0] = opcodes[0x03][2] = opcodes[0x03][4] = oraSp8;
	opcodes[0x03][1] = opcodes[0x03][3] = oraSp16;
	opcodes[0x0D][0] = opcodes[0x0D][2] = opcodes[0x0D][4] = oraAbs8;
	opcodes[0x0D][1] = opcodes[0x0D][3] = oraAbs16;
	opcodes[0x1D][0] = opcodes[0x1D][2] = opcodes[0x1D][4] = oraAbsx8;
	opcodes[0x1D][1] = opcodes[0x1D][3] = oraAbsx16;
	opcodes[0x19][0] = opcodes[0x19][2] = opcodes[0x19][4] = oraAbsy8;
	opcodes[0x19][1] = opcodes[0x19][3] = oraAbsy16;
	opcodes[0x0F][0] = opcodes[0x0F][2] = opcodes[0x0F][4] = oraLong8;
	opcodes[0x0F][1] = opcodes[0x0F][3] = oraLong16;
	opcodes[0x1F][0] = opcodes[0x1F][2] = opcodes[0x1F][4] = oraLongx8;
	opcodes[0x1F][1] = opcodes[0x1F][3] = oraLongx16;
	opcodes[0x12][0] = opcodes[0x12][2] = opcodes[0x12][4] = oraIndirect8;
	opcodes[0x12][1] = opcodes[0x12][3] = oraIndirect16;
	opcodes[0x01][0] = opcodes[0x01][2] = opcodes[0x01][4] = oraIndirectx8;
	opcodes[0x01][1] = opcodes[0x01][3] = oraIndirectx16;
	opcodes[0x11][0] = opcodes[0x11][2] = opcodes[0x11][4] = oraIndirecty8;
	opcodes[0x11][1] = opcodes[0x11][3] = oraIndirecty16;
	opcodes[0x07][0] = opcodes[0x07][2] = opcodes[0x07][4] = oraIndirectLong8;
	opcodes[0x07][1] = opcodes[0x07][3] = oraIndirectLong16;
	opcodes[0x17][0] = opcodes[0x17][2] = opcodes[0x17][4] = oraIndirectLongy8;
	opcodes[0x17][1] = opcodes[0x17][3] = oraIndirectLongy16;

	/*ADC group*/
	opcodes[0x69][0] = opcodes[0x69][2] = opcodes[0x69][4] = adcImm8;
	opcodes[0x69][1] = opcodes[0x69][3] = adcImm16;
	opcodes[0x65][0] = opcodes[0x65][2] = opcodes[0x65][4] = adcZp8;
	opcodes[0x65][1] = opcodes[0x65][3] = adcZp16;
	opcodes[0x75][0] = opcodes[0x75][2] = opcodes[0x75][4] = adcZpx8;
	opcodes[0x75][1] = opcodes[0x75][3] = adcZpx16;
	opcodes[0x63][0] = opcodes[0x63][2] = opcodes[0x63][4] = adcSp8;
	opcodes[0x63][1] = opcodes[0x63][3] = adcSp16;
	opcodes[0x6D][0] = opcodes[0x6D][2] = opcodes[0x6D][4] = adcAbs8;
	opcodes[0x6D][1] = opcodes[0x6D][3] = adcAbs16;
	opcodes[0x7D][0] = opcodes[0x7D][2] = opcodes[0x7D][4] = adcAbsx8;
	opcodes[0x7D][1] = opcodes[0x7D][3] = adcAbsx16;
	opcodes[0x79][0] = opcodes[0x79][2] = opcodes[0x79][4] = adcAbsy8;
	opcodes[0x79][1] = opcodes[0x79][3] = adcAbsy16;
	opcodes[0x6F][0] = opcodes[0x6F][2] = opcodes[0x6F][4] = adcLong8;
	opcodes[0x6F][1] = opcodes[0x6F][3] = adcLong16;
	opcodes[0x7F][0] = opcodes[0x7F][2] = opcodes[0x7F][4] = adcLongx8;
	opcodes[0x7F][1] = opcodes[0x7F][3] = adcLongx16;
	opcodes[0x72][0] = opcodes[0x72][2] = opcodes[0x72][4] = adcIndirect8;
	opcodes[0x72][1] = opcodes[0x72][3] = adcIndirect16;
	opcodes[0x61][0] = opcodes[0x61][2] = opcodes[0x61][4] = adcIndirectx8;
	opcodes[0x61][1] = opcodes[0x61][3] = adcIndirectx16;
	opcodes[0x71][0] = opcodes[0x71][2] = opcodes[0x71][4] = adcIndirecty8;
	opcodes[0x71][1] = opcodes[0x71][3] = adcIndirecty16;
	opcodes[0x73][0] = opcodes[0x73][2] = opcodes[0x73][4] = adcsIndirecty8;
	opcodes[0x73][1] = opcodes[0x73][3] = adcsIndirecty16;
	opcodes[0x67][0] = opcodes[0x67][2] = opcodes[0x67][4] = adcIndirectLong8;
	opcodes[0x67][1] = opcodes[0x67][3] = adcIndirectLong16;
	opcodes[0x77][0] = opcodes[0x77][2] = opcodes[0x77][4] = adcIndirectLongy8;
	opcodes[0x77][1] = opcodes[0x77][3] = adcIndirectLongy16;

	/*SBC group*/
	opcodes[0xE9][0] = opcodes[0xE9][2] = opcodes[0xE9][4] = sbcImm8;
	opcodes[0xE9][1] = opcodes[0xE9][3] = sbcImm16;
	opcodes[0xE5][0] = opcodes[0xE5][2] = opcodes[0xE5][4] = sbcZp8;
	opcodes[0xE5][1] = opcodes[0xE5][3] = sbcZp16;
	opcodes[0xE3][0] = opcodes[0xE3][2] = opcodes[0xE3][4] = sbcSp8;
	opcodes[0xE3][1] = opcodes[0xE3][3] = sbcSp16;
	opcodes[0xF5][0] = opcodes[0xF5][2] = opcodes[0xF5][4] = sbcZpx8;
	opcodes[0xF5][1] = opcodes[0xF5][3] = sbcZpx16;
	opcodes[0xED][0] = opcodes[0xED][2] = opcodes[0xED][4] = sbcAbs8;
	opcodes[0xED][1] = opcodes[0xED][3] = sbcAbs16;
	opcodes[0xFD][0] = opcodes[0xFD][2] = opcodes[0xFD][4] = sbcAbsx8;
	opcodes[0xFD][1] = opcodes[0xFD][3] = sbcAbsx16;
	opcodes[0xF9][0] = opcodes[0xF9][2] = opcodes[0xF9][4] = sbcAbsy8;
	opcodes[0xF9][1] = opcodes[0xF9][3] = sbcAbsy16;
	opcodes[0xEF][0] = opcodes[0xEF][2] = opcodes[0xEF][4] = sbcLong8;
	opcodes[0xEF][1] = opcodes[0xEF][3] = sbcLong16;
	opcodes[0xFF][0] = opcodes[0xFF][2] = opcodes[0xFF][4] = sbcLongx8;
	opcodes[0xFF][1] = opcodes[0xFF][3] = sbcLongx16;
	opcodes[0xF2][0] = opcodes[0xF2][2] = opcodes[0xF2][4] = sbcIndirect8;
	opcodes[0xF2][1] = opcodes[0xF2][3] = sbcIndirect16;
	opcodes[0xE1][0] = opcodes[0xE1][2] = opcodes[0xE1][4] = sbcIndirectx8;
	opcodes[0xE1][1] = opcodes[0xE1][3] = sbcIndirectx16;
	opcodes[0xF1][0] = opcodes[0xF1][2] = opcodes[0xF1][4] = sbcIndirecty8;
	opcodes[0xF1][1] = opcodes[0xF1][3] = sbcIndirecty16;
	opcodes[0xE7][0] = opcodes[0xE7][2] = opcodes[0xE7][4] = sbcIndirectLong8;
	opcodes[0xE7][1] = opcodes[0xE7][3] = sbcIndirectLong16;
	opcodes[0xF7][0] = opcodes[0xF7][2] = opcodes[0xF7][4] = sbcIndirectLongy8;
	opcodes[0xF7][1] = opcodes[0xF7][3] = sbcIndirectLongy16;

	/*Transfer group*/
	opcodes[0xAA][0] = opcodes[0xAA][1] = opcodes[0xAA][4] = tax8;
	opcodes[0xAA][2] = opcodes[0xAA][3] = tax16;
	opcodes[0xA8][0] = opcodes[0xA8][1] = opcodes[0xA8][4] = tay8;
	opcodes[0xA8][2] = opcodes[0xA8][3] = tay16;
	opcodes[0x8A][0] = opcodes[0x8A][2] = opcodes[0x8A][4] = txa8;
	opcodes[0x8A][1] = opcodes[0x8A][3] = txa16;
	opcodes[0x98][0] = opcodes[0x98][2] = opcodes[0x98][4] = tya8;
	opcodes[0x98][1] = opcodes[0x98][3] = tya16;
	opcodes[0x9B][0] = opcodes[0x9B][1] = opcodes[0x9B][4] = txy8;
	opcodes[0x9B][2] = opcodes[0x9B][3] = txy16;
	opcodes[0xBB][0] = opcodes[0xBB][1] = opcodes[0xBB][4] = tyx8;
	opcodes[0xBB][2] = opcodes[0xBB][3] = tyx16;
	opcodes[0xBA][0] = opcodes[0xBA][1] = opcodes[0xBA][4] = tsx8;
	opcodes[0xBA][2] = opcodes[0xBA][3] = tsx16;
	opcodes[0x9A][0] = opcodes[0x9A][1] = opcodes[0x9A][4] = txs8;
	opcodes[0x9A][2] = opcodes[0x9A][3] = txs16;

	/*Flag Group*/
	opcodes[0x18][0] = opcodes[0x18][1] = opcodes[0x18][2] =
		opcodes[0x18][3] = opcodes[0x18][4] = clc;
	opcodes[0xD8][0] = opcodes[0xD8][1] = opcodes[0xD8][2] =
		opcodes[0xD8][3] = opcodes[0xD8][4] = cld;
	opcodes[0x58][0] = opcodes[0x58][1] = opcodes[0x58][2] =
		opcodes[0x58][3] = opcodes[0x58][4] = cli;
	opcodes[0xB8][0] = opcodes[0xB8][1] = opcodes[0xB8][2] =
		opcodes[0xB8][3] = opcodes[0xB8][4] = clv;
	opcodes[0x38][0] = opcodes[0x38][1] = opcodes[0x38][2] =
		opcodes[0x38][3] = opcodes[0x38][4] = sec;
	opcodes[0xF8][0] = opcodes[0xF8][1] = opcodes[0xF8][2] =
		opcodes[0xF8][3] = opcodes[0xF8][4] = sed;
	opcodes[0x78][0] = opcodes[0x78][1] = opcodes[0x78][2] =
		opcodes[0x78][3] = opcodes[0x78][4] = sei;
	opcodes[0xFB][0] = opcodes[0xFB][1] = opcodes[0xFB][2] =
		opcodes[0xFB][3] = opcodes[0xFB][4] = xce;
	opcodes[0xE2][0] = opcodes[0xE2][1] = opcodes[0xE2][2] =
		opcodes[0xE2][3] = opcodes[0xE2][4] = sep;
	opcodes[0xC2][0] = opcodes[0xC2][1] = opcodes[0xC2][2] =
		opcodes[0xC2][3] = opcodes[0xC2][4] = rep;

	/*Stack group*/
	opcodes[0x8B][0] = opcodes[0x8B][1] = opcodes[0x8B][2] =
		opcodes[0x8B][3] = phb;
	opcodes[0x8B][4] = phbe;
	opcodes[0x4B][0] = opcodes[0x4B][1] = opcodes[0x4B][2] =
		opcodes[0x4B][3] = phk;
	opcodes[0x4B][4] = phke;
	opcodes[0xAB][0] = opcodes[0xAB][1] = opcodes[0xAB][2] =
		opcodes[0xAB][3] = plb;
	opcodes[0xAB][4] = plbe;
	opcodes[0x08][0] = opcodes[0x08][1] = opcodes[0x08][2] =
		opcodes[0x08][3] = php;
	opcodes[0x08][4] = php;
	opcodes[0x28][0] = opcodes[0x28][1] = opcodes[0x28][2] =
		opcodes[0x28][3] = plp;
	opcodes[0x28][4] = plp;
	opcodes[0x48][0] = opcodes[0x48][2] = opcodes[0x48][4] = pha8;
	opcodes[0x48][1] = opcodes[0x48][3] = pha16;
	opcodes[0xDA][0] = opcodes[0xDA][1] = opcodes[0xDA][4] = phx8;
	opcodes[0xDA][2] = opcodes[0xDA][3] = phx16;
	opcodes[0x5A][0] = opcodes[0x5A][1] = opcodes[0x5A][4] = phy8;
	opcodes[0x5A][2] = opcodes[0x5A][3] = phy16;
	opcodes[0x68][0] = opcodes[0x68][2] = opcodes[0x68][4] = pla8;
	opcodes[0x68][1] = opcodes[0x68][3] = pla16;
	opcodes[0xFA][0] = opcodes[0xFA][1] = opcodes[0xFA][4] = plx8;
	opcodes[0xFA][2] = opcodes[0xFA][3] = plx16;
	opcodes[0x7A][0] = opcodes[0x7A][1] = opcodes[0x7A][4] = ply8;
	opcodes[0x7A][2] = opcodes[0x7A][3] = ply16;
	opcodes[0xD4][0] = opcodes[0xD4][1] = opcodes[0xD4][2] =
		opcodes[0xD4][3] = opcodes[0xD4][4] = pei;
	opcodes[0xF4][0] = opcodes[0xF4][1] = opcodes[0xF4][2] =
		opcodes[0xF4][3] = opcodes[0xF4][4] = pea;
	opcodes[0x62][0] = opcodes[0x62][1] = opcodes[0x62][2] =
		opcodes[0x62][3] = opcodes[0x62][4] = per;
	opcodes[0x0B][0] = opcodes[0x0B][1] = opcodes[0x0B][2] =
		opcodes[0x0B][3] = opcodes[0x0B][4] = phd;
	opcodes[0x2B][0] = opcodes[0x2B][1] = opcodes[0x2B][2] =
		opcodes[0x2B][3] = opcodes[0x2B][4] = pld;

	/*CMP group*/
	opcodes[0xC9][0] = opcodes[0xC9][2] = opcodes[0xC9][4] = cmpImm8;
	opcodes[0xC9][1] = opcodes[0xC9][3] = cmpImm16;
	opcodes[0xC5][0] = opcodes[0xC5][2] = opcodes[0xC5][4] = cmpZp8;
	opcodes[0xC5][1] = opcodes[0xC5][3] = cmpZp16;
	opcodes[0xC3][0] = opcodes[0xC3][2] = opcodes[0xC3][4] = cmpSp8;
	opcodes[0xC3][1] = opcodes[0xC3][3] = cmpSp16;
	opcodes[0xD5][0] = opcodes[0xD5][2] = opcodes[0xD5][4] = cmpZpx8;
	opcodes[0xD5][1] = opcodes[0xD5][3] = cmpZpx16;
	opcodes[0xCD][0] = opcodes[0xCD][2] = opcodes[0xCD][4] = cmpAbs8;
	opcodes[0xCD][1] = opcodes[0xCD][3] = cmpAbs16;
	opcodes[0xDD][0] = opcodes[0xDD][2] = opcodes[0xDD][4] = cmpAbsx8;
	opcodes[0xDD][1] = opcodes[0xDD][3] = cmpAbsx16;
	opcodes[0xD9][0] = opcodes[0xD9][2] = opcodes[0xD9][4] = cmpAbsy8;
	opcodes[0xD9][1] = opcodes[0xD9][3] = cmpAbsy16;
	opcodes[0xCF][0] = opcodes[0xCF][2] = opcodes[0xCF][4] = cmpLong8;
	opcodes[0xCF][1] = opcodes[0xCF][3] = cmpLong16;
	opcodes[0xDF][0] = opcodes[0xDF][2] = opcodes[0xDF][4] = cmpLongx8;
	opcodes[0xDF][1] = opcodes[0xDF][3] = cmpLongx16;
	opcodes[0xD2][0] = opcodes[0xD2][2] = opcodes[0xD2][4] = cmpIndirect8;
	opcodes[0xD2][1] = opcodes[0xD2][3] = cmpIndirect16;
	opcodes[0xC1][0] = opcodes[0xC1][2] = opcodes[0xC1][4] = cmpIndirectx8;
	opcodes[0xC1][1] = opcodes[0xC1][3] = cmpIndirectx16;
	opcodes[0xD1][0] = opcodes[0xD1][2] = opcodes[0xD1][4] = cmpIndirecty8;
	opcodes[0xD1][1] = opcodes[0xD1][3] = cmpIndirecty16;
	opcodes[0xC7][0] = opcodes[0xC7][2] = opcodes[0xC7][4] = cmpIndirectLong8;
	opcodes[0xC7][1] = opcodes[0xC7][3] = cmpIndirectLong16;
	opcodes[0xD7][0] = opcodes[0xD7][2] = opcodes[0xD7][4] = cmpIndirectLongy8;
	opcodes[0xD7][1] = opcodes[0xD7][3] = cmpIndirectLongy16;

	/*CPX group*/
	opcodes[0xE0][0] = opcodes[0xE0][1] = opcodes[0xE0][4] = cpxImm8;
	opcodes[0xE0][2] = opcodes[0xE0][3] = cpxImm16;
	opcodes[0xE4][0] = opcodes[0xE4][1] = opcodes[0xE4][4] = cpxZp8;
	opcodes[0xE4][2] = opcodes[0xE4][3] = cpxZp16;
	opcodes[0xEC][0] = opcodes[0xEC][1] = opcodes[0xEC][4] = cpxAbs8;
	opcodes[0xEC][2] = opcodes[0xEC][3] = cpxAbs16;

	/*CPY group*/
	opcodes[0xC0][0] = opcodes[0xC0][1] = opcodes[0xC0][4] = cpyImm8;
	opcodes[0xC0][2] = opcodes[0xC0][3] = cpyImm16;
	opcodes[0xC4][0] = opcodes[0xC4][1] = opcodes[0xC4][4] = cpyZp8;
	opcodes[0xC4][2] = opcodes[0xC4][3] = cpyZp16;
	opcodes[0xCC][0] = opcodes[0xCC][1] = opcodes[0xCC][4] = cpyAbs8;
	opcodes[0xCC][2] = opcodes[0xCC][3] = cpyAbs16;

	/*Branch group*/
	opcodes[0x90][0] = opcodes[0x90][1] = opcodes[0x90][2] =
		opcodes[0x90][3] = opcodes[0x90][4] = bcc;
	opcodes[0xB0][0] = opcodes[0xB0][1] = opcodes[0xB0][2] =
		opcodes[0xB0][3] = opcodes[0xB0][4] = bcs;
	opcodes[0xF0][0] = opcodes[0xF0][1] = opcodes[0xF0][2] =
		opcodes[0xF0][3] = opcodes[0xF0][4] = beq;
	opcodes[0xD0][0] = opcodes[0xD0][1] = opcodes[0xD0][2] =
		opcodes[0xD0][3] = opcodes[0xD0][4] = bne;
	opcodes[0x80][0] = opcodes[0x80][1] = opcodes[0x80][2] =
		opcodes[0x80][3] = opcodes[0x80][4] = bra;
	opcodes[0x82][0] = opcodes[0x82][1] = opcodes[0x82][2] =
		opcodes[0x82][3] = opcodes[0x82][4] = brl;
	opcodes[0x10][0] = opcodes[0x10][1] = opcodes[0x10][2] =
		opcodes[0x10][3] = opcodes[0x10][4] = bpl;
	opcodes[0x30][0] = opcodes[0x30][1] = opcodes[0x30][2] =
		opcodes[0x30][3] = opcodes[0x30][4] = bmi;
	opcodes[0x50][0] = opcodes[0x50][1] = opcodes[0x50][2] =
		opcodes[0x50][3] = opcodes[0x50][4] = bvc;
	opcodes[0x70][0] = opcodes[0x70][1] = opcodes[0x70][2] =
		opcodes[0x70][3] = opcodes[0x70][4] = bvs;

	/*Jump group*/
	opcodes[0x4C][0] = opcodes[0x4C][1] = opcodes[0x4C][2] =
		opcodes[0x4C][3] = opcodes[0x4C][4] = jmp;
	opcodes[0x5C][0] = opcodes[0x5C][1] = opcodes[0x5C][2] =
		opcodes[0x5C][3] = opcodes[0x5C][4] = jmplong;
	opcodes[0x6C][0] = opcodes[0x6C][1] = opcodes[0x6C][2] =
		opcodes[0x6C][3] = opcodes[0x6C][4] = jmpind;
	opcodes[0x7C][0] = opcodes[0x7C][1] = opcodes[0x7C][2] =
		opcodes[0x7C][3] = opcodes[0x7C][4] = jmpindx;
	opcodes[0xDC][0] = opcodes[0xDC][1] = opcodes[0xDC][2] =
		opcodes[0xDC][3] = opcodes[0xDC][4] = jmlind;
	opcodes[0x20][0] = opcodes[0x20][1] = opcodes[0x20][2] =
		opcodes[0x20][3] = jsr;
	opcodes[0x20][4] = jsre;
	opcodes[0xFC][0] = opcodes[0xFC][1] = opcodes[0xFC][2] =
		opcodes[0xFC][3] = jsrIndx;
	opcodes[0xFC][4] = jsrIndxe;
	opcodes[0x60][0] = opcodes[0x60][1] = opcodes[0x60][2] =
		opcodes[0x60][3] = rts;
	opcodes[0x60][4] = rtse;
	opcodes[0x6B][0] = opcodes[0x6B][1] = opcodes[0x6B][2] =
		opcodes[0x6B][3] = rtl;
	opcodes[0x6B][4] = rtle;
	opcodes[0x40][0] = opcodes[0x40][1] = opcodes[0x40][2] =
		opcodes[0x40][3] = rti;
	opcodes[0x22][0] = opcodes[0x22][1] = opcodes[0x22][2] =
		opcodes[0x22][3] = jsl;
	opcodes[0x22][4] = jsle;

	/*Shift group*/
	opcodes[0x0A][0] = opcodes[0x0A][2] = opcodes[0x0A][4] = asla8;
	opcodes[0x0A][1] = opcodes[0x0A][3] = asla16;
	opcodes[0x06][0] = opcodes[0x06][2] = opcodes[0x06][4] = aslZp8;
	opcodes[0x06][1] = opcodes[0x06][3] = aslZp16;
	opcodes[0x16][0] = opcodes[0x16][2] = opcodes[0x16][4] = aslZpx8;
	opcodes[0x16][1] = opcodes[0x16][3] = aslZpx16;
	opcodes[0x0E][0] = opcodes[0x0E][2] = opcodes[0x0E][4] = aslAbs8;
	opcodes[0x0E][1] = opcodes[0x0E][3] = aslAbs16;
	opcodes[0x1E][0] = opcodes[0x1E][2] = opcodes[0x1E][4] = aslAbsx8;
	opcodes[0x1E][1] = opcodes[0x1E][3] = aslAbsx16;

	opcodes[0x4A][0] = opcodes[0x4A][2] = opcodes[0x4A][4] = lsra8;
	opcodes[0x4A][1] = opcodes[0x4A][3] = lsra16;
	opcodes[0x46][0] = opcodes[0x46][2] = opcodes[0x46][4] = lsrZp8;
	opcodes[0x46][1] = opcodes[0x46][3] = lsrZp16;
	opcodes[0x56][0] = opcodes[0x56][2] = opcodes[0x56][4] = lsrZpx8;
	opcodes[0x56][1] = opcodes[0x56][3] = lsrZpx16;
	opcodes[0x4E][0] = opcodes[0x4E][2] = opcodes[0x4E][4] = lsrAbs8;
	opcodes[0x4E][1] = opcodes[0x4E][3] = lsrAbs16;
	opcodes[0x5E][0] = opcodes[0x5E][2] = opcodes[0x5E][4] = lsrAbsx8;
	opcodes[0x5E][1] = opcodes[0x5E][3] = lsrAbsx16;

	opcodes[0x2A][0] = opcodes[0x2A][2] = opcodes[0x2A][4] = rola8;
	opcodes[0x2A][1] = opcodes[0x2A][3] = rola16;
	opcodes[0x26][0] = opcodes[0x26][2] = opcodes[0x26][4] = rolZp8;
	opcodes[0x26][1] = opcodes[0x26][3] = rolZp16;
	opcodes[0x36][0] = opcodes[0x36][2] = opcodes[0x36][4] = rolZpx8;
	opcodes[0x36][1] = opcodes[0x36][3] = rolZpx16;
	opcodes[0x2E][0] = opcodes[0x2E][2] = opcodes[0x2E][4] = rolAbs8;
	opcodes[0x2E][1] = opcodes[0x2E][3] = rolAbs16;
	opcodes[0x3E][0] = opcodes[0x3E][2] = opcodes[0x3E][4] = rolAbsx8;
	opcodes[0x3E][1] = opcodes[0x3E][3] = rolAbsx16;

	opcodes[0x6A][0] = opcodes[0x6A][2] = opcodes[0x6A][4] = rora8;
	opcodes[0x6A][1] = opcodes[0x6A][3] = rora16;
	opcodes[0x66][0] = opcodes[0x66][2] = opcodes[0x66][4] = rorZp8;
	opcodes[0x66][1] = opcodes[0x66][3] = rorZp16;
	opcodes[0x76][0] = opcodes[0x76][2] = opcodes[0x76][4] = rorZpx8;
	opcodes[0x76][1] = opcodes[0x76][3] = rorZpx16;
	opcodes[0x6E][0] = opcodes[0x6E][2] = opcodes[0x6E][4] = rorAbs8;
	opcodes[0x6E][1] = opcodes[0x6E][3] = rorAbs16;
	opcodes[0x7E][0] = opcodes[0x7E][2] = opcodes[0x7E][4] = rorAbsx8;
	opcodes[0x7E][1] = opcodes[0x7E][3] = rorAbsx16;

	/*BIT group*/
	opcodes[0x89][0] = opcodes[0x89][2] = opcodes[0x89][4] = bitImm8;
	opcodes[0x89][1] = opcodes[0x89][3] = bitImm16;
	opcodes[0x24][0] = opcodes[0x24][2] = opcodes[0x24][4] = bitZp8;
	opcodes[0x24][1] = opcodes[0x24][3] = bitZp16;
	opcodes[0x34][0] = opcodes[0x34][2] = opcodes[0x34][4] = bitZpx8;
	opcodes[0x34][1] = opcodes[0x34][3] = bitZpx16;
	opcodes[0x2C][0] = opcodes[0x2C][2] = opcodes[0x2C][4] = bitAbs8;
	opcodes[0x2C][1] = opcodes[0x2C][3] = bitAbs16;
	opcodes[0x3C][0] = opcodes[0x3C][2] = opcodes[0x3C][4] = bitAbsx8;
	opcodes[0x3C][1] = opcodes[0x3C][3] = bitAbsx16;

	/*Misc group*/
	opcodes[0x00][0] = opcodes[0x00][1] = opcodes[0x00][2] =
		opcodes[0x00][3] = opcodes[0x00][4] = brk;
	opcodes[0xEB][0] = opcodes[0xEB][1] = opcodes[0xEB][2] =
		opcodes[0xEB][3] = opcodes[0xEB][4] = xba;
	opcodes[0xEA][0] = opcodes[0xEA][1] = opcodes[0xEA][2] =
		opcodes[0xEA][3] = opcodes[0xEA][4] = nop;
	opcodes[0x5B][0] = opcodes[0x5B][1] = opcodes[0x5B][2] =
		opcodes[0x5B][3] = opcodes[0x5B][4] = tcd;
	opcodes[0x7B][0] = opcodes[0x7B][1] = opcodes[0x7B][2] =
		opcodes[0x7B][3] = opcodes[0x7B][4] = tdc;
	opcodes[0x1B][0] = opcodes[0x1B][1] = opcodes[0x1B][2] =
		opcodes[0x1B][3] = opcodes[0x1B][4] = tcs;
	opcodes[0x3B][0] = opcodes[0x3B][1] = opcodes[0x3B][2] =
		opcodes[0x3B][3] = opcodes[0x3B][4] = tsc;
	opcodes[0xCB][0] = opcodes[0xCB][1] = opcodes[0xCB][2] =
		opcodes[0xCB][3] = opcodes[0xCB][4] = wai;
	opcodes[0x44][0] = opcodes[0x44][1] = opcodes[0x44][2] =
		opcodes[0x44][3] = opcodes[0x44][4] = mvp;
	opcodes[0x54][0] = opcodes[0x54][1] = opcodes[0x54][2] =
		opcodes[0x54][3] = opcodes[0x54][4] = mvn;
	opcodes[0x04][0] = opcodes[0x04][2] = opcodes[0x04][4] = tsbZp8;
	opcodes[0x04][1] = opcodes[0x04][3] = tsbZp16;
	opcodes[0x0C][0] = opcodes[0x0C][2] = opcodes[0x0C][4] = tsbAbs8;
	opcodes[0x0C][1] = opcodes[0x0C][3] = tsbAbs16;
	opcodes[0x14][0] = opcodes[0x14][2] = opcodes[0x14][4] = trbZp8;
	opcodes[0x14][1] = opcodes[0x14][3] = trbZp16;
	opcodes[0x1C][0] = opcodes[0x1C][2] = opcodes[0x1C][4] = trbAbs8;
	opcodes[0x1C][1] = opcodes[0x1C][3] = trbAbs16;
}



void nmi65816()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.cycles -= 6;
	clockspc(6);
	if (snes_cpu.inwai)
	{
		snes_cpu.pc++;
	}
	snes_cpu.inwai = 0;
	if (!snes_cpu.e)
	{
		snes_writemem(REG_SW(), snes_cpu.pbr >> 16);
		REG_SW()--;
		snes_writemem(REG_SW(), snes_cpu.pc >> 8);
		REG_SW()--;
		snes_writemem(REG_SW(), snes_cpu.pc & 0xFF);
		REG_SW()--;

		snes_writemem(REG_SW(), REG_PL());
		REG_SW()--;
		snes_cpu.pc = readmemw(0xFFEA);
		snes_cpu.pbr = 0;
		SET_IRQ();
		CLEAR_DECIMAL();
	}
}

void irq65816()
{
	snes_readmem(snes_cpu.pbr | snes_cpu.pc);
	snes_cpu.cycles -= 6; clockspc(6);
	if (snes_cpu.inwai && CHECK_IRQ())
	{
		snes_cpu.pc++;
		snes_cpu.inwai = 0;
		return;
	}
	if (snes_cpu.inwai) snes_cpu.pc++;
	snes_cpu.inwai = 0;
	if (!snes_cpu.e)
	{
		snes_writemem(REG_SW(), snes_cpu.pbr >> 16);
		REG_SW()--;
		snes_writemem(REG_SW(), snes_cpu.pc >> 8);
		REG_SW()--;
		snes_writemem(REG_SW(), snes_cpu.pc & 0xFF);
		REG_SW()--;

		snes_writemem(REG_SW(), REG_PL());
		REG_SW()--;
		snes_cpu.pc = readmemw(0xFFEE);
		snes_cpu.pbr = 0;
		SET_IRQ();
		CLEAR_DECIMAL();
	}
}