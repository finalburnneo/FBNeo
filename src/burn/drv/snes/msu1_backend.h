// =============================================================================
//  FBNeo SNES  -  MSU-1 media backend  -  public interface
// =============================================================================

#ifndef MSU1_BACKEND_H
#define MSU1_BACKEND_H

#include "burnint.h"

void  snes_msu1_backend_setGame(const char* shortName, const char* parentName);
INT32 snes_msu1_backend_detect(const char* shortName);
void  snes_msu1_backend_install();
void  snes_msu1_backend_free();

#endif
