/* Ported from MAME */
#include "burnint.h"
#include "midwayic.h"

static int nUpper;
static UINT8 nData[16];
static UINT8 nBuffer;
static UINT8 nIndex;
static UINT8 nStatus;
static UINT8 nBits;
static UINT8 nOrMask;

void MidwaySerialPicInit(int upper)
{
    nUpper = upper;
    for (int i = 0; i < 16; i++)
        nData[i] = 0;
    MidwaySerialPicReset();

    int year = 1994, month = 12, day = 11;
    UINT32 serial_number, temp;
    UINT8 serial_digit[9];

    serial_number = 123456;
    serial_number += upper * 1000000;

    serial_digit[0] = (serial_number / 100000000) % 10;
    serial_digit[1] = (serial_number / 10000000) % 10;
    serial_digit[2] = (serial_number / 1000000) % 10;
    serial_digit[3] = (serial_number / 100000) % 10;
    serial_digit[4] = (serial_number / 10000) % 10;
    serial_digit[5] = (serial_number / 1000) % 10;
    serial_digit[6] = (serial_number / 100) % 10;
    serial_digit[7] = (serial_number / 10) % 10;
    serial_digit[8] = (serial_number / 1) % 10;

    nData[12] = rand() & 0xff;
    nData[13] = rand() & 0xff;

    nData[14] = 0; /* ??? */
    nData[15] = 0; /* ??? */

    temp = 0x174 * (year - 1980) + 0x1f * (month - 1) + day;
    nData[10] = (temp >> 8) & 0xff;
    nData[11] = temp & 0xff;

    temp = serial_digit[4] + serial_digit[7] * 10 + serial_digit[1] * 100;
    temp = (temp + 5 * nData[13]) * 0x1bcd + 0x1f3f0;
    nData[7] = temp & 0xff;
    nData[8] = (temp >> 8) & 0xff;
    nData[9] = (temp >> 16) & 0xff;

    temp = serial_digit[6] + serial_digit[8] * 10 + serial_digit[0] * 100 + serial_digit[2] * 10000;
    temp = (temp + 2 * nData[13] + nData[12]) * 0x107f + 0x71e259;
    nData[3] = temp & 0xff;
    nData[4] = (temp >> 8) & 0xff;
    nData[5] = (temp >> 16) & 0xff;
    nData[6] = (temp >> 24) & 0xff;

    temp = serial_digit[5] * 10 + serial_digit[3] * 100;
    temp = (temp + nData[12]) * 0x245 + 0x3d74;
    nData[0] = temp & 0xff;
    nData[1] = (temp >> 8) & 0xff;
    nData[2] = (temp >> 16) & 0xff;

    /* special hack for RevX */
    nOrMask = 0x80;
    if (upper == 419)
        nOrMask = 0x00;
}

void MidwaySerialPicReset()
{
    nBuffer = 0;
    nIndex = 0;
    nStatus = 0;
    nBits = 0;
    nOrMask = 0;
}


UINT32 MidwaySerialPicStatus()
{
    return nStatus;
}

UINT8 MidwaySerialPicRead()
{
    nStatus = 1;
    return nBuffer;
}


void MidwaySerialPicWrite(UINT8 data)
{
    nStatus = (data >> 4) & 1;
    if (!nStatus) {
        if (data & 0x0f)
            nBuffer = nOrMask | data;
        else
            nBuffer = nData[nIndex++ % sizeof(nData)];
    }
}

