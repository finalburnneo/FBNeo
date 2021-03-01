/*
   // modifications

   AngelCode Tool Box Library
   Copyright (c) 2007-2008 Andreas Jonsson
  
   This software is provided 'as-is', without any express or implied 
   warranty. In no event will the authors be held liable for any 
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any 
   purpose, including commercial applications, and to alter it and 
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you 
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product 
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and 
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source 
      distribution.
  
   Andreas Jonsson
   andreas@angelcode.com
*/

#ifndef FONT_H
#define FONT_H

#include <d3d9.h>

struct IDirect3DTexture9;

class VidEffect;
class CDynRender;
class CFontLoader;

struct SCharDescr
{
	SCharDescr() : srcX(0), srcY(0), srcW(0), srcH(0), xOff(0), yOff(0), xAdv(0), page(0) {}

	short srcX;
	short srcY;
	short srcW;
	short srcH;
	short xOff;
	short yOff;
	short xAdv;
	short page;
	unsigned int chnl;

	std::vector<int> kerningPairs;
};

enum EFontTextEncoding
{
	NONE,
	UTF8,
	UTF16
};

class CFont
{
public:
	CFont();
	~CFont();

	int Init(const char *fontFile, const char *shaderfile, CDynRender *dynRender);
	void End();
	bool IsOk() { return fontHeight > 0; }

	float GetTextWidth(const wchar_t *text, int count, float spacing = 0);
	void Write(float x, float y, float z, DWORD color, const wchar_t *text, int count, unsigned int mode, float spacing = 0);
	void WriteML(float x, float y, float z, DWORD color, const wchar_t *text, int count, unsigned int mode, float spacing = 0);
	void WriteBox(float x, float y, float z, DWORD color, float width, const wchar_t *text, int count, unsigned mode);

	float GetHeight();
	float GetScaleW() const { return scaleW; }
	float GetScaleH() const { return scaleH; }

	float GetBottomOffset();
	float GetTopOffset();

	void SetScale(float x, float y) { scaleX = x; scaleY = y; }

protected:
	friend class CFontLoader;

	void InternalWrite(float x, float y, float z, DWORD color, const wchar_t *text, int count, float spacing = 0);

	float AdjustForKerningPairs(int first, int second);
	SCharDescr *GetChar(int id);

	int GetTextLength(const wchar_t *text);
	int GetTextChar(const wchar_t *text, int pos, int *nextPos = 0);
	int FindTextChar(const wchar_t *text, int start, int length, int ch);

	short fontHeight; // total height of the font
	short base;       // y of base line
	float scaleX;
	float scaleY;
	short scaleW;
	short scaleH;
	SCharDescr defChar;
	bool hasOutline;

	CDynRender *render;
	VidEffect *effect;

	std::map<int, SCharDescr*> chars;
	std::vector<IDirect3DTexture9*> pages;
};

const unsigned int FONT_ALIGN_LEFT    = 0;
const unsigned int FONT_ALIGN_CENTER  = 1;
const unsigned int FONT_ALIGN_RIGHT   = 2;
const unsigned int FONT_ALIGN_JUSTIFY = 3;

// 2008-05-11 Storing the characters in a map instead of an array
// 2008-05-17 Added support for writing text with UTF8 and UTF16 encoding

#endif
