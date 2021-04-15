// K007452 multiplier/divider, silicon-RE, code & (c) by furrtek (Sean Gonsalves) April 2021

#include "burnint.h"

static UINT32 math_reg[6];
static UINT16 multiply_result;
static UINT16 divide_quotient;
static UINT16 divide_remainder;

void K007452Reset()
{
	memset(math_reg, 0, sizeof(math_reg));

	multiply_result = 0;
	divide_quotient = 0;
	divide_remainder = 0;
}

void K007452Init()
{
	K007452Reset();
}

void K007452Exit()
{
	K007452Reset();
}

void K007452Scan(INT32 nAction)
{
	if (nAction & ACB_VOLATILE) {
		SCAN_VAR(math_reg);
		SCAN_VAR(multiply_result);
		SCAN_VAR(divide_quotient);
		SCAN_VAR(divide_remainder);
	}
}

UINT8 K007452Read(UINT16 address)
{
	switch (address & 7)
	{
		case 0:	return multiply_result & 0xff;
		case 1:	return (multiply_result >> 8) & 0xff;
		case 2:	return divide_remainder & 0xff;
		case 3:	return (divide_remainder >> 8) & 0xff;
		case 4:	return divide_quotient & 0xff;
		case 5:	return (divide_quotient >> 8) & 0xff;
	}

	return 0;
}

void K007452Write(UINT16 address, UINT8 data)
{
	address &= 7;

	if (address < 6) math_reg[address] = data;

	if (address == 1)
	{
		// Starts multiplication process
		multiply_result = math_reg[0] * math_reg[1];
	}
	else if (address == 5)
	{
		// Starts division process
		UINT16 dividend = (math_reg[4]<<8) + math_reg[5];
		UINT16 divisor = (math_reg[2]<<8) + math_reg[3];
		if (!divisor) {
			divide_quotient = 0xffff;
			divide_remainder = 0x0000;
		} else {
			divide_quotient = dividend / divisor;
			divide_remainder = dividend % divisor;
		}
	}
}
