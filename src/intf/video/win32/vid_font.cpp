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

#include "burner.h"
#include "vid_font.h"
#include "vid_dynrender.h"
#include "vid_effect.h"
#include <d3dx9.h>

using namespace std;

bool LoadD3DTextureFromFile(IDirect3DDevice9* device, const char* filename, IDirect3DTexture9*& texture, int& width, int& height);

// Implement private helper classes for loading the bitmap font files

class CFontLoader
{
public:
	CFontLoader(FILE *f, CFont *font, const char *fontFile);

	virtual int Load() = 0; // Must be implemented by derived class

protected:
	void LoadPage(int id, const char *pageFile, const char *fontFile);
	void SetFontInfo(int outlineThickness);
	void SetCommonInfo(int fontHeight, int base, int scaleW, int scaleH, int pages, bool isPacked);
	void AddChar(int id, int x, int y, int w, int h, int xoffset, int yoffset, int xadvance, int page, int chnl);
	void AddKerningPair(int first, int second, int amount);

	FILE *f;
	CFont *font;
	const char *fontFile;

	int outlineThickness;
};

class CFontLoaderTextFormat : public CFontLoader
{
public:
	CFontLoaderTextFormat(FILE *f, CFont *font, const char *fontFile);

	int Load();

	int SkipWhiteSpace(std::string &str, int start);
	int FindEndOfToken(std::string &str, int start);

	void InterpretInfo(std::string &str, int start);
	void InterpretCommon(std::string &str, int start);
	void InterpretChar(std::string &str, int start);
	void InterpretSpacing(std::string &str, int start);
	void InterpretKerning(std::string &str, int start);
	void InterpretPage(std::string &str, int start, const char *fontFile);
};

class CFontLoaderBinaryFormat : public CFontLoader
{
public:
	CFontLoaderBinaryFormat(FILE *f, CFont *font, const char *fontFile);

	int Load();

	void ReadInfoBlock(int size);
	void ReadCommonBlock(int size);
	void ReadPagesBlock(int size);
	void ReadCharsBlock(int size);
	void ReadKerningPairsBlock(int size);
};

//=============================================================================
// CFont
//
// This is the CFont class that is used to write text with bitmap fonts.
//=============================================================================

CFont::CFont()
{
	fontHeight = 0;
	base = 0;
	scaleX = 1;
	scaleY = 1;
	scaleW = 0;
	scaleH = 0;
	effect = 0;
	render = 0;
	hasOutline = false;
}

CFont::~CFont()
{
	End();
}

int CFont::Init(const char *fontFile, const char *shaderfile, CDynRender *render)
{
	this->render = render;

	// Load the font
	FILE *f = fopen(fontFile, "rb");
	if (f)
	{
		// Determine format by reading the first bytes of the file
		char str[4] = {0};
		fread(str, 3, 1, f);
		fseek(f, 0, SEEK_SET);

		CFontLoader *loader = 0;
		if( strcmp(str, "BMF") == 0 )
			loader = new CFontLoaderBinaryFormat(f, this, fontFile);
		else
			loader = new CFontLoaderTextFormat(f, this, fontFile);

		int r = loader->Load();
		delete loader;

		fclose(f);
		
		/*
		effect = new VidEffect(render->GetDevice());
		effect->Load(shaderfile);
		*/

		return r;
	}
	return 0;
}

void CFont::End()
{
	/*
	if (effect) {
		delete effect;
		effect = 0;
	}
	*/

	std::map<int, SCharDescr*>::iterator it = chars.begin();
	while( it != chars.end() )
	{
		delete it->second;
		it++;
	}
	chars.clear();

	for( UINT n = 0; n < pages.size(); n++ ) {
		if( pages[n] )
			RELEASE(pages[n]);
	}
	pages.clear();
}

// Internal
SCharDescr *CFont::GetChar(int id)
{
	std::map<int, SCharDescr*>::iterator it = chars.find(id);
	if( it == chars.end() ) return 0;

	return it->second;
}

// Internal
float CFont::AdjustForKerningPairs(int first, int second)
{	
	SCharDescr *ch = GetChar(first);
	if( ch == 0 ) return 0;
	for( UINT n = 0; n < ch->kerningPairs.size(); n += 2 )
	{
		if( ch->kerningPairs[n] == second )
			return ch->kerningPairs[n+1] * scaleX;
	}

	return 0;
}

float CFont::GetTextWidth(const wchar_t *text, int count, float spacing)
{
	if( count <= 0 )
		count = GetTextLength(text);

	float x = 0;

	for( int n = 0; n < count; )
	{
		int charId = GetTextChar(text,n,&n);

		SCharDescr *ch = GetChar(charId);
		if( ch == 0 ) ch = &defChar;

		x += (ch->xAdv) * scaleX;
		if( n < count )
		{
			if (charId != ' ')
				x += spacing;
			x += AdjustForKerningPairs(charId, GetTextChar(text,n));
		}
	}

	return x;
}

float CFont::GetHeight()
{
	return scaleY * float(fontHeight);
}

float CFont::GetBottomOffset()
{
	return scaleY * (base - fontHeight);
}

float CFont::GetTopOffset()
{
	return scaleY * (base - 0);
}

// Internal
// Returns the number of bytes in the string until the null char
int CFont::GetTextLength(const wchar_t *text)
{
	return (int)wcslen(text);
}

// Internal
int CFont::GetTextChar(const wchar_t *text, int pos, int *nextPos)
{
	int ch = text[pos];
	unsigned int len = 1;

	if( nextPos ) *nextPos = pos + len;
	return ch;
}

// Internal
int CFont::FindTextChar(const wchar_t *text, int start, int length, int ch)
{
	int pos = start;
	int nextPos;
	int currChar = -1;
    while( pos < length )
	{
		currChar = GetTextChar(text, pos, &nextPos);
		if( currChar == ch )
			return pos;
		pos = nextPos;
	} 

	return -1;
}

void CFont::InternalWrite(float x, float y, float z, DWORD color, const wchar_t *text, int count, float spacing)
{
	for( int n = 0; n < count; )
	{
		int charId = GetTextChar(text, n, &n);
		SCharDescr *ch = GetChar(charId);
		if( ch == 0 ) ch = &defChar;

		// Map the center of the texel to the corners
		// in order to get pixel perfect mapping
		float u = (float(ch->srcX)+0.5f) / scaleW;
		float v = (float(ch->srcY)+0.5f) / scaleH;
		float u2 = u + float(ch->srcW) / scaleW;
		float v2 = v + float(ch->srcH) / scaleH;

		float a = scaleX * float(ch->xAdv);
		float w = scaleX * float(ch->srcW);
		float h = scaleY * float(ch->srcH);
		float ox = scaleX * float(ch->xOff);
		float oy = scaleY * float(ch->yOff);

		render->SetTexture(pages[ch->page], effect);
		render->VtxColor(color);
		render->VtxTexCoord(u, v);
		render->VtxPos(x+ox, y+oy, z);
		render->VtxColor(color);
		render->VtxTexCoord(u2, v);
		render->VtxPos(x+w+ox, y+oy, z);
		render->VtxColor(color);
		render->VtxTexCoord(u2, v2);
		render->VtxPos(x+w+ox, y+h+oy, z);
		render->VtxColor(color);
		render->VtxTexCoord(u, v2);
		render->VtxPos(x+ox, y+h+oy, z);

		x += a;
		if( charId != ' ' )
			x += spacing;

		if( n < count )
			x += AdjustForKerningPairs(charId, GetTextChar(text,n));
	}
}

void CFont::Write(float x, float y, float z, DWORD color, const wchar_t *text, int count, unsigned int mode, float spacing)
{
	if( count <= 0 )
		count = GetTextLength(text);

	if( mode == FONT_ALIGN_CENTER )
	{
		float w = GetTextWidth(text, count, spacing);
		x -= w/2;
	}
	else if( mode == FONT_ALIGN_RIGHT )
	{
		float w = GetTextWidth(text, count, spacing);
		x -= w;
	}

	InternalWrite(x, y, z, color, text, count, spacing);
}

void CFont::WriteML(float x, float y, float z, DWORD color, const wchar_t *text, int count, unsigned int mode, float spacing)
{
	if( count <= 0 )
		count = GetTextLength(text);

	// Get first line
	int pos = 0;
	int len = FindTextChar(text, pos, count, '\n'); 
	if( len == -1 ) len = count;
	while( pos < count )
	{
		float cx = x;
		if( mode == FONT_ALIGN_CENTER )
		{
			float w = GetTextWidth(&text[pos], len, spacing);
			cx -= w/2;
		}
		else if( mode == FONT_ALIGN_RIGHT )
		{
			float w = GetTextWidth(&text[pos], len, spacing);
			cx -= w;
		}

		InternalWrite(cx, y, z, color, &text[pos], len, spacing);

		y -= scaleY * float(fontHeight);

		// Get next line
		pos += len;
		int ch = GetTextChar(text, pos, &pos);
		if( ch == '\n' )
		{
			len = FindTextChar(text, pos, count, '\n');
			if( len == -1 )
				len = count - pos;
			else 
				len = len - pos;
		}
	}
}

void CFont::WriteBox(float x, float y, float z, DWORD color, float width, const wchar_t *text, int count, unsigned int mode)
{
	if( count <= 0 )
		count = GetTextLength(text);

	float currWidth = 0, wordWidth;
	int lineS = 0, lineE = 0, wordS = 0, wordE = 0;
	int wordCount = 0;

	const wchar_t *s = L" ";
	float spaceWidth = GetTextWidth(s, 1);
	bool softBreak = false;
	
	for(; lineS < count;)
	{
		// Determine the extent of the line
		for(;;)
		{
			// Determine the number of characters in the word
			while( wordE < count && 
				GetTextChar(text,wordE) != ' ' &&
				GetTextChar(text,wordE) != '\n' )
				// Advance the cursor to the next character
				GetTextChar(text,wordE,&wordE);

			// Determine the width of the word
			if( wordE > wordS )
			{
				wordCount++;
				wordWidth = GetTextWidth(&text[wordS], wordE-wordS);
			}
			else
				wordWidth = 0;

			// Does the word fit on the line? The first word is always accepted.
			if( wordCount == 1 || currWidth + (wordCount > 1 ? spaceWidth : 0) + wordWidth <= width )
			{
				// Increase the line extent to the end of the word
				lineE = wordE;
				currWidth += (wordCount > 1 ? spaceWidth : 0) + wordWidth;

				// Did we reach the end of the line?
				if( wordE == count || GetTextChar(text,wordE) == '\n' )
				{
					softBreak = false;

					// Skip the newline character
					if( wordE < count )
						// Advance the cursor to the next character
						GetTextChar(text,wordE,&wordE);

					break;
				}				

				// Skip the trailing space
				if( wordE < count && GetTextChar(text,wordE) == ' ' )
					// Advance the cursor to the next character
					GetTextChar(text,wordE,&wordE);

				// Move to next word
				wordS = wordE;
			}
			else
			{
				softBreak = true;

				// Skip the trailing space
				if( wordE < count && GetTextChar(text,wordE) == ' ' )
					// Advance the cursor to the next character
					GetTextChar(text,wordE,&wordE);

				break;
			}
		}

		// Write the line
		if( mode == FONT_ALIGN_JUSTIFY )
		{
			float spacing = 0;
			if( softBreak )
			{
				if( wordCount > 2 )
					spacing = (width - currWidth) / (wordCount-2);
				else
					spacing = (width - currWidth);
			}
			
            InternalWrite(x, y, z, color, &text[lineS], lineE - lineS, spacing);
		}
		else
		{
			float cx = x;
			if( mode == FONT_ALIGN_RIGHT )
				cx = x + width - currWidth;
			else if( mode == FONT_ALIGN_CENTER )
				cx = x + 0.5f*(width - currWidth);

			InternalWrite(cx, y, z, color, &text[lineS], lineE - lineS);
		}

		if( softBreak )
		{
			// Skip the trailing space
			if( lineE < count && GetTextChar(text,lineE) == ' ' )
				// Advance the cursor to the next character
				GetTextChar(text,lineE,&lineE);

			// We've already counted the first word on the next line
			currWidth = wordWidth;
			wordCount = 1;
		}
		else
		{
			// Skip the line break
			if( lineE < count && GetTextChar(text,lineE) == '\n' )
				// Advance the cursor to the next character
				GetTextChar(text,lineE,&lineE);

			currWidth = 0;
			wordCount = 0;
		}
		
		// Move to next line
		lineS = lineE;
		wordS = wordE;
		y -= scaleY * float(fontHeight);
	}
}

//=============================================================================
// CFontLoader
//
// This is the base class for all loader classes. This is the only class
// that has access to and knows how to set the CFont members.
//=============================================================================

CFontLoader::CFontLoader(FILE *f, CFont *font, const char *fontFile)
{
	this->f = f;
	this->font = font;
	this->fontFile = fontFile;

	outlineThickness = 0;
}

void CFontLoader::LoadPage(int id, const char *pageFile, const char *fontFile)
{
  string str;
	// Load the texture from the same directory as the font descriptor file

	// Find the directory
	str = fontFile;
	for( size_t n = 0; (n = str.find('/', n)) != string::npos; ) str.replace(n, 1, "\\");
	size_t i = str.rfind('\\');
	if( i != string::npos )
		str = str.substr(0, i+1);
	else
		str = "";

	// Load the font textures
	str += pageFile;
	int w, h;
	LoadD3DTextureFromFile(font->render->GetDevice(), str.c_str(), font->pages[id], w, h);
}

void CFontLoader::SetFontInfo(int outlineThickness)
{
	this->outlineThickness = outlineThickness;
}

void CFontLoader::SetCommonInfo(int fontHeight, int base, int scaleW, int scaleH, int pages, bool isPacked)
{
	font->fontHeight = fontHeight;
	font->base = base;
	font->scaleW = scaleW;
	font->scaleH = scaleH;
	font->pages.resize(pages);
	for( int n = 0; n < pages; n++ )
		font->pages[n] = 0;

	if( isPacked && outlineThickness )
		font->hasOutline = true;
}

void CFontLoader::AddChar(int id, int x, int y, int w, int h, int xoffset, int yoffset, int xadvance, int page, int chnl)
{
	// Convert to a 4 element vector
	// TODO: Does this depend on hardware? It probably does
	if     ( chnl == 1 ) chnl = 0x00010000;  // Blue channel
	else if( chnl == 2 ) chnl = 0x00000100;  // Green channel
	else if( chnl == 4 ) chnl = 0x00000001;  // Red channel
	else if( chnl == 8 ) chnl = 0x01000000;  // Alpha channel
	else chnl = 0;

	if( id >= 0 )
	{
		SCharDescr *ch = new SCharDescr;
		ch->srcX = x;
		ch->srcY = y;
		ch->srcW = w;
		ch->srcH = h;
		ch->xOff = xoffset;
		ch->yOff = yoffset;
		ch->xAdv = xadvance;
		ch->page = page;
		ch->chnl = chnl;

		font->chars.insert(std::map<int, SCharDescr*>::value_type(id, ch));
	}

	if( id == -1 )
	{
		font->defChar.srcX = x;
		font->defChar.srcY = y;
		font->defChar.srcW = w;
		font->defChar.srcH = h;
		font->defChar.xOff = xoffset;
		font->defChar.yOff = yoffset;
		font->defChar.xAdv = xadvance;
		font->defChar.page = page;
		font->defChar.chnl = chnl;
	}
}

void CFontLoader::AddKerningPair(int first, int second, int amount)
{
	if( first >= 0 && first < 256 && font->chars[first] )
	{
		font->chars[first]->kerningPairs.push_back(second);
		font->chars[first]->kerningPairs.push_back(amount);
	}
}

//=============================================================================
// CFontLoaderTextFormat
//
// This class implements the logic for loading a BMFont file in text format
//=============================================================================

CFontLoaderTextFormat::CFontLoaderTextFormat(FILE *f, CFont *font, const char *fontFile) : CFontLoader(f, font, fontFile)
{
}

int CFontLoaderTextFormat::Load()
{
	string line;

	while( !feof(f) )
	{
		// Read until line feed (or EOF)
		line = "";
		line.reserve(256);
		while( !feof(f) )
		{
			char ch;
			if( fread(&ch, 1, 1, f) )
			{
				if( ch != '\n' ) 
					line += ch; 
				else
					break;
			}
		}

		// Skip white spaces
		int pos = SkipWhiteSpace(line, 0);

		// Read token
		int pos2 = FindEndOfToken(line, pos);
		string token = line.substr(pos, pos2-pos);

		// Interpret line
		if( token == "info" )
			InterpretInfo(line, pos2);
		else if( token == "common" )
			InterpretCommon(line, pos2);
		else if( token == "char" )
			InterpretChar(line, pos2);
		else if( token == "kerning" )
			InterpretKerning(line, pos2);
		else if( token == "page" )
			InterpretPage(line, pos2, fontFile);
	}

	fclose(f);

	// Success
	return 0;
}

int CFontLoaderTextFormat::SkipWhiteSpace(string &str, int start)
{
	UINT n = start;
	while( n < str.size() )
	{
		char ch = str[n];
		if( ch != ' ' && 
			ch != '\t' && 
			ch != '\r' && 
			ch != '\n' )
			break;

		++n;
	}

	return n;
}

int CFontLoaderTextFormat::FindEndOfToken(string &str, int start)
{
	UINT n = start;
	if( str[n] == '"' )
	{
		n++;
		while( n < str.size() )
		{
			char ch = str[n];
			if( ch == '"' )
			{
				// Include the last quote char in the token
				++n;
				break;
			}
			++n;
		}
	}
	else
	{
		while( n < str.size() )
		{
			char ch = str[n];
			if( ch == ' ' ||
				ch == '\t' ||
				ch == '\r' ||
				ch == '\n' ||
				ch == '=' )
				break;

			++n;
		}
	}

	return n;
}

void CFontLoaderTextFormat::InterpretKerning(string &str, int start)
{
	// Read the attributes
	int first = 0;
	int second = 0;
	int amount = 0;

	int pos, pos2 = start;
	while( true )
	{
		pos = SkipWhiteSpace(str, pos2);
		pos2 = FindEndOfToken(str, pos);

		string token = str.substr(pos, pos2-pos);

		pos = SkipWhiteSpace(str, pos2);
		if( pos == (int)str.size() || str[pos] != '=' ) break;

		pos = SkipWhiteSpace(str, pos+1);
		pos2 = FindEndOfToken(str, pos);

		string value = str.substr(pos, pos2-pos);

		if( token == "first" )
			first = strtol(value.c_str(), 0, 10);
		else if( token == "second" )
			second = strtol(value.c_str(), 0, 10);
		else if( token == "amount" )
			amount = strtol(value.c_str(), 0, 10);

		if( pos == (int)str.size() ) break;
	}

	// Store the attributes
	AddKerningPair(first, second, amount);
}

void CFontLoaderTextFormat::InterpretChar(string &str, int start)
{
	// Read all attributes
	int id = 0;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	int xoffset = 0;
	int yoffset = 0;
	int xadvance = 0;
	int page = 0;
	int chnl = 0;

	int pos, pos2 = start;
	while( true )
	{
		pos = SkipWhiteSpace(str, pos2);
		pos2 = FindEndOfToken(str, pos);

		string token = str.substr(pos, pos2-pos);

		pos = SkipWhiteSpace(str, pos2);
		if( pos == (int)str.size() || str[pos] != '=' ) break;

		pos = SkipWhiteSpace(str, pos+1);
		pos2 = FindEndOfToken(str, pos);

		string value = str.substr(pos, pos2-pos);

		if( token == "id" )
			id = strtol(value.c_str(), 0, 10);
		else if( token == "x" )
			x = strtol(value.c_str(), 0, 10);
		else if( token == "y" )
			y = strtol(value.c_str(), 0, 10);
		else if( token == "width" )
			width = strtol(value.c_str(), 0, 10);
		else if( token == "height" )
			height = strtol(value.c_str(), 0, 10);
		else if( token == "xoffset" )
			xoffset = strtol(value.c_str(), 0, 10);
		else if( token == "yoffset" )
			yoffset = strtol(value.c_str(), 0, 10);
		else if( token == "xadvance" )
			xadvance = strtol(value.c_str(), 0, 10);
		else if( token == "page" )
			page = strtol(value.c_str(), 0, 10);
		else if( token == "chnl" )
			chnl = strtol(value.c_str(), 0, 10);

		if( pos == (int)str.size() ) break;
	}

	// Store the attributes
	AddChar(id, x, y, width, height, xoffset, yoffset, xadvance, page, chnl);
}

void CFontLoaderTextFormat::InterpretCommon(string &str, int start)
{
	int fontHeight;
	int base;
	int scaleW;
	int scaleH;
	int pages;
	int packed;

	// Read all attributes
	int pos, pos2 = start;
	while( true )
	{
		pos = SkipWhiteSpace(str, pos2);
		pos2 = FindEndOfToken(str, pos);

		string token = str.substr(pos, pos2-pos);

		pos = SkipWhiteSpace(str, pos2);
		if( pos == (int)str.size() || str[pos] != '=' ) break;

		pos = SkipWhiteSpace(str, pos+1);
		pos2 = FindEndOfToken(str, pos);

		string value = str.substr(pos, pos2-pos);

		if( token == "lineHeight" )
			fontHeight = (short)strtol(value.c_str(), 0, 10);
		else if( token == "base" )
			base = (short)strtol(value.c_str(), 0, 10);
		else if( token == "scaleW" )
			scaleW = (short)strtol(value.c_str(), 0, 10);
		else if( token == "scaleH" )
			scaleH = (short)strtol(value.c_str(), 0, 10);
		else if( token == "pages" )
			pages = strtol(value.c_str(), 0, 10);
		else if( token == "packed" )
			packed = strtol(value.c_str(), 0, 10);

		if( pos == (int)str.size() ) break;
	}

	SetCommonInfo(fontHeight, base, scaleW, scaleH, pages, packed ? true : false);
}

void CFontLoaderTextFormat::InterpretInfo(string &str, int start)
{
	int outlineThickness;

	// Read all attributes
	int pos, pos2 = start;
	while( true )
	{
		pos = SkipWhiteSpace(str, pos2);
		pos2 = FindEndOfToken(str, pos);

		string token = str.substr(pos, pos2-pos);

		pos = SkipWhiteSpace(str, pos2);
		if( pos == (int)str.size() || str[pos] != '=' ) break;

		pos = SkipWhiteSpace(str, pos+1);
		pos2 = FindEndOfToken(str, pos);

		string value = str.substr(pos, pos2-pos);

		if( token == "outline" )
			outlineThickness = (short)strtol(value.c_str(), 0, 10);

		if( pos == (int)str.size() ) break;
	}

	SetFontInfo(outlineThickness);
}

void CFontLoaderTextFormat::InterpretPage(string &str, int start, const char *fontFile)
{
	int id = 0;
	string file;

	// Read all attributes
	int pos, pos2 = start;
	while( true )
	{
		pos = SkipWhiteSpace(str, pos2);
		pos2 = FindEndOfToken(str, pos);

		string token = str.substr(pos, pos2-pos);

		pos = SkipWhiteSpace(str, pos2);
		if( pos == (int)str.size() || str[pos] != '=' ) break;

		pos = SkipWhiteSpace(str, pos+1);
		pos2 = FindEndOfToken(str, pos);

		string value = str.substr(pos, pos2-pos);

		if( token == "id" )
			id = strtol(value.c_str(), 0, 10);
		else if( token == "file" )
			file = value.substr(1, value.length()-2);

		if( pos == (int)str.size() ) break;
	}

	LoadPage(id, file.c_str(), fontFile);
}

//=============================================================================
// CFontLoaderBinaryFormat
//
// This class implements the logic for loading a BMFont file in binary format
//=============================================================================

CFontLoaderBinaryFormat::CFontLoaderBinaryFormat(FILE *f, CFont *font, const char *fontFile) : CFontLoader(f, font, fontFile)
{
}

int CFontLoaderBinaryFormat::Load()
{
	// Read and validate the tag. It should be 66, 77, 70, 2, 
	// or 'BMF' and 2 where the number is the file version.
	char magicString[4];
	fread(magicString, 4, 1, f);
	if( strncmp(magicString, "BMF\003", 4) != 0 )
	{
		//LOG(("Unrecognized format for '%s'", fontFile));
		fclose(f);
		return -1;
	}

	// Read each block
	char blockType;
	int blockSize;
	while( fread(&blockType, 1, 1, f) )
	{
		// Read the blockSize
		fread(&blockSize, 4, 1, f);

		switch( blockType )
		{
		case 1: // info
			ReadInfoBlock(blockSize);
			break;
		case 2: // common
			ReadCommonBlock(blockSize);
			break;
		case 3: // pages
			ReadPagesBlock(blockSize);
			break;
		case 4: // chars
			ReadCharsBlock(blockSize);
			break;
		case 5: // kerning pairs
			ReadKerningPairsBlock(blockSize);
			break;
		default:
			//LOG(("Unexpected block type (%d)", blockType));
			fclose(f);
			return -1;
		}
	}

	fclose(f);

	// Success
	return 0;
}

void CFontLoaderBinaryFormat::ReadInfoBlock(int size)
{
#pragma pack(push)
#pragma pack(1)
struct infoBlock
{
  WORD fontSize;
  BYTE reserved:4;
  BYTE bold    :1;
  BYTE italic  :1;
  BYTE unicode :1;
  BYTE smooth  :1;
  BYTE charSet;
  WORD stretchH;
  BYTE aa;
  BYTE paddingUp;
  BYTE paddingRight;
  BYTE paddingDown;
  BYTE paddingLeft;
  BYTE spacingHoriz;
  BYTE spacingVert;
  BYTE outline;         // Added with version 2
  char fontName[1];
};
#pragma pack(pop)

	char *buffer = new char[size];
	fread(buffer, size, 1, f);

	// We're only interested in the outline thickness
	infoBlock *blk = (infoBlock*)buffer;
	SetFontInfo(blk->outline);

	delete[] buffer;
}

void CFontLoaderBinaryFormat::ReadCommonBlock(int size)
{
#pragma pack(push)
#pragma pack(1)
struct commonBlock
{
	WORD lineHeight;
	WORD base;
	WORD scaleW;
	WORD scaleH;
	WORD pages;
	BYTE packed  :1;
	BYTE reserved:7;
	BYTE alphaChnl;
	BYTE redChnl;
	BYTE greenChnl;
	BYTE blueChnl;
}; 
#pragma pack(pop)

	char *buffer = new char[size];
	fread(buffer, size, 1, f);

	commonBlock *blk = (commonBlock*)buffer;

	SetCommonInfo(blk->lineHeight, blk->base, blk->scaleW, blk->scaleH, blk->pages, blk->packed ? true : false);

	delete[] buffer;
}

void CFontLoaderBinaryFormat::ReadPagesBlock(int size)
{
#pragma pack(push)
#pragma pack(1)
struct pagesBlock
{
    char pageNames[1];
};
#pragma pack(pop)

	char *buffer = new char[size];
	fread(buffer, size, 1, f);

	pagesBlock *blk = (pagesBlock*)buffer;

	for( int id = 0, pos = 0; pos < size; id++ )
	{
		LoadPage(id, &blk->pageNames[pos], fontFile);
		pos += 1 + (int)strlen(&blk->pageNames[pos]);
	}

	delete[] buffer;
}

void CFontLoaderBinaryFormat::ReadCharsBlock(int size)
{
#pragma pack(push)
#pragma pack(1)
struct charsBlock
{
	struct charInfo
	{
		DWORD id;
		WORD  x;
		WORD  y;
		WORD  width;
		WORD  height;
		short xoffset;
		short yoffset;
		short xadvance;
		BYTE  page;
		BYTE  chnl;
	} chars[1];
};
#pragma pack(pop)

	char *buffer = new char[size];
	fread(buffer, size, 1, f);

	charsBlock *blk = (charsBlock*)buffer;

	for (int n = 0; int(n * sizeof(charsBlock::charInfo)) < size; n++)
	{
		AddChar(blk->chars[n].id,
						blk->chars[n].x,
						blk->chars[n].y,
						blk->chars[n].width,
						blk->chars[n].height,
						blk->chars[n].xoffset,
						blk->chars[n].yoffset,
						blk->chars[n].xadvance,
						blk->chars[n].page,
						blk->chars[n].chnl);
	}

	delete[] buffer;
}

void CFontLoaderBinaryFormat::ReadKerningPairsBlock(int size)
{
#pragma pack(push)
#pragma pack(1)
struct kerningPairsBlock
{
    struct kerningPair
    {
        DWORD first;
        DWORD second;
        short amount;
    } kerningPairs[1];
};
#pragma pack(pop)

	char *buffer = new char[size];
	fread(buffer, size, 1, f);

	kerningPairsBlock *blk = (kerningPairsBlock*)buffer;

	for( int n = 0; int(n*sizeof(kerningPairsBlock::kerningPair)) < size; n++ )
	{
		AddKerningPair(blk->kerningPairs[n].first,
					 blk->kerningPairs[n].second,
					 blk->kerningPairs[n].amount);
	}

	delete[] buffer;
}

// 2008-05-11 Storing the characters in a map instead of an array
// 2008-05-11 Loading the new binary format for BMFont v1.10
