// Burn - hard disk image module
//
// Gives drivers sector access to the hard disks listed by their GetHDDInfo/GetHDDName functions.
// Images can be MAME CHDs (hard disk type, compressed or not) or raw dumps, found in
// <szAppHDDPath><romset>/ then <szAppHDDPath><parent>/.
// The image files are never written: sectors written by the game are kept in memory and saved in
// <szAppEEPROMPath><romset>.diff (<romset>_<n>.diff for disk n > 0), reloaded on the next run.

#ifndef BURN_HDD_H
#define BURN_HDD_H

struct BurnHDD;

// Opens disk nIndex of the running driver. Returns NULL on failure.
BurnHDD* BurnHDDOpen(INT32 nIndex);

// Saves the diff file (if anything was written) and frees everything.
void BurnHDDClose(BurnHDD* pDisk);

// Writes the diff file now, returns 0 on success (also done by BurnHDDClose).
INT32 BurnHDDFlush(BurnHDD* pDisk);

UINT32 BurnHDDGetSectorSize(BurnHDD* pDisk);
UINT32 BurnHDDGetSectorCount(BurnHDD* pDisk);

// CHS geometry stored in a CHD (GDDD metadata). Returns 1 when the image has none (raw dumps).
INT32 BurnHDDGetGeometry(BurnHDD* pDisk, INT32* pnCylinders, INT32* pnHeads, INT32* pnSectors);

// Sector access, returns 0 on success. Out of range sectors read as zeroes and fail.
INT32 BurnHDDRead(BurnHDD* pDisk, UINT32 nLba, UINT32 nCount, void* pDest);
INT32 BurnHDDWrite(BurnHDD* pDisk, UINT32 nLba, UINT32 nCount, const void* pSrc);

// Forgets every write (back to the pristine image) and deletes the diff file.
void BurnHDDDiscardWrites(BurnHDD* pDisk);

#endif
