/*
   // modifications

   AngelCode Tool Box Library
   Copyright (c) 2007 Andreas J�nsson

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

   Andreas J�nsson
   andreas@angelcode.com
*/

#ifndef ACGFX_DYNRENDER_H
#define ACGFX_DYNRENDER_H

#include <d3d9.h>
#include <vector>

class VidEffect;

class CDynRender
{
public:
	CDynRender() {}
	~CDynRender();

	void Init(IDirect3DDevice9Ex* device);
	void End();

	IDirect3DDevice9Ex *GetDevice() { return device; }

	int  InitDeviceObjects();
	void ReleaseDeviceObjects();

	void SetTexture(IDirect3DTexture9* tex, VidEffect *eff = 0);
	int  BeginRender();
	int  EndRender(bool reuse = false);
	void Flush();

	void VtxColor(DWORD argb);
	void VtxTexCoord(float x, float y);
	void VtxPos(float x, float y, float z);

protected:
	IDirect3DDevice9Ex *device;
	UINT maxDynamicVertices;
	IDirect3DVertexBuffer9 *dynamicVertexBuffer;
	UINT vertexBase;
	UINT vertexLimit;
	std::vector<unsigned char> tempVertexBuffer;
	unsigned char *dynamicVertices;
	UINT vertexCount;
	UINT subVertexCount;
	IDirect3DVertexDeclaration9 *declaration;
	IDirect3DTexture9* texture;
	VidEffect* effect;

	DWORD color;
	D3DVECTOR texCoord;
	D3DVECTOR normal;
};

#endif
