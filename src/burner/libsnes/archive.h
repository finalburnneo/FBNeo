#ifndef ARCHIVE_H__
#define ARCHIVE_H__

#include <stdint.h>
#include <stddef.h>

struct ArcEntry { char* szName; unsigned int nLen; unsigned int nCrc; };

enum ARCTYPE { ARC_NONE = -1, ARC_ZIP = 0, ARC_7Z, ARC_NUM };

int archiveCheck(char* name, int zipOnly = 0);
int archiveOpen(const char* archive);
int archiveClose();
int archiveGetList(ArcEntry** list, int* count = NULL);
int archiveLoadFile(uint8_t* dest, int len, int entry, int* wrote = NULL);

#endif

