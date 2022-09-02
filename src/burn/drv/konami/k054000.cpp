// license:BSD-3-Clause
// copyright-holders:David Haywood, Angelo Salese, furrtek
/**************************************************************************************************

    Konami K054000 hitbox/math custom chip

    Sort of a protection device, used for collision detection.
    It is passed a few parameters, and returns a boolean telling if collision
    happened. It has no access to gfx data, it only does arithmetical operations
    on the parameters.

    Thunder Cross II POST checks of this chip.
    It literally tests the chip in an unit test fashion:
      1. zeroing all ports;
      2. test that status returns 0;
      3. ping ACX reg 0 with 0xff;
      4. test status = 1;
      5. ping BCX reg 0 with 0xff;
      6. test status = 0;
      7. ping ACX reg 1 with 0xff;
      8. test status = 1;
      9. rinse and repeat until all registers are exausted.

    The fun part is that game doesn't even access the chip at all during gameplay
    (or at least not until stage 6, where game disallows continues) while the specific
    "delta" registers are instead challenged by Vendetta OTG attacks (cfr. MT06393, MT07839).

**************************************************************************************************/

#include "burnint.h"

static UINT8 m_raw_Acx[4], m_raw_Acy[4], m_raw_Bcx[4], m_raw_Bcy[4];
static int m_Acx = 0, m_Acy = 0, m_Bcx = 0, m_Bcy = 0;
static int m_Aax = 0, m_Aay = 0, m_Bax = 0, m_Bay = 0;

void K054000Reset()
{
	memset(m_raw_Acx, 0, sizeof(m_raw_Acx));
	memset(m_raw_Acy, 0, sizeof(m_raw_Acy));
	memset(m_raw_Bcx, 0, sizeof(m_raw_Bcx));
	memset(m_raw_Bcy, 0, sizeof(m_raw_Bcy));
	m_Aax = 1;
	m_Aay = 1;
	m_Bax = 1;
	m_Bay = 1;
}

static int convert_raw_to_result_delta(UINT8 *buf)
{
	int res = (buf[0] << 16) | (buf[1] << 8) | buf[2];

	// Last value in the buffer is used as OTG correction in Vendetta
	if (buf[3] & 0x80)
		res -= (0x100 - buf[3]);
	else
		res += buf[3];

	return res;
}

static int convert_raw_to_result(UINT8 *buf)
{
	return (buf[0] << 16) | (buf[1] << 8) | buf[2];
}

static UINT8 axis_check(UINT32 m_Ac, UINT32 m_Bc, UINT32 m_Aa, UINT32 m_Ba)
{
	UINT8 res = 0;
	INT32 sub = m_Ac - m_Bc;

	// MSB check
	if ((sub > 511) || (sub <= -1024))
		res |= 1;

	// LSB check
	if ((abs(sub) & 0x1ff) > ((m_Aa + m_Ba) & 0x1ff))
		res |= 1;

	return res;
}

static UINT8 status_r()
{
	UINT8 res;

	res = axis_check(m_Acx, m_Bcx, m_Aax, m_Bax);
	res |= axis_check(m_Acy, m_Bcy, m_Aay, m_Bay);

	return res;
}

void K054000Write(INT32 offset, INT32 data)
{
	offset &= 0x1f;

	switch (offset) {
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
			m_raw_Acx[offset - 0x01] = data;
			m_Acx = convert_raw_to_result_delta(m_raw_Acx);
			break;

		case 0x09:
		case 0x0a:
		case 0x0b:
		case 0x0c:
			m_raw_Acy[offset - 0x09] = data;
			m_Acy = convert_raw_to_result_delta(m_raw_Acy);
			break;

		case 0x11:
		case 0x12:
		case 0x13:
			m_raw_Bcy[offset - 0x11] = data;
			m_Bcy = convert_raw_to_result(m_raw_Bcy);
			break;

		case 0x15:
		case 0x16:
		case 0x17:
			m_raw_Bcx[offset - 0x15] = data;
			m_Bcx = convert_raw_to_result(m_raw_Bcx);
			break;

		case 0x06:
			m_Aax = data;
			break;
		case 0x07:
			m_Aay = data;
			break;
		case 0x0e:
			m_Bax = data;
			break;
		case 0x0f:
			m_Bay = data;
			break;
	}
}

UINT8 K054000Read(INT32 offset)
{
	offset &= 0x1f;

	if (offset == 0x18) return status_r();

	return 0;
}

void K054000Scan(INT32 nAction)
{
	if (nAction & ACB_MEMORY_RAM) {
		SCAN_VAR(m_raw_Acx);
		SCAN_VAR(m_raw_Acy);
		SCAN_VAR(m_raw_Bcx);
		SCAN_VAR(m_raw_Bcy);
		SCAN_VAR(m_Aax);
		SCAN_VAR(m_Aay);
		SCAN_VAR(m_Bax);
		SCAN_VAR(m_Bay);
	}
}
