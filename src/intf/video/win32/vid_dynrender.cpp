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

#include "vid_dynrender.h"
#include "vid_effect.h"
#include <d3dx9.h>

struct SVertex
{
	float x,y,z,w;
	DWORD color;
	float u,v;
};

static const D3DVERTEXELEMENT9 elements[] =
{
	{0,  0, D3DDECLTYPE_FLOAT4  , D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT   , 0},
	{0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR       , 0},
	{0, 20, D3DDECLTYPE_FLOAT2  , D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD    , 0},
	D3DDECL_END()
};


CDynRender::~CDynRender()
{
	End();
}

void CDynRender::Init(IDirect3DDevice9Ex *pD3DDevice)
{
	device = pD3DDevice;
	dynamicVertexBuffer = 0;
	dynamicVertices = 0;
	color = 0xFFFFFFFF;
	declaration = 0;
	texture = 0;

	InitDeviceObjects();
}

void CDynRender::End()
{
	ReleaseDeviceObjects();
}

int CDynRender::InitDeviceObjects()
{
	// Create dynamic vertex buffer

	// Note that since this buffer is placed in the default pool it must be released and recreated when resetting the device
	maxDynamicVertices = 1000;
	HRESULT hr = device->CreateVertexBuffer(maxDynamicVertices*sizeof(SVertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &dynamicVertexBuffer, NULL);
	if( FAILED(hr) )
	{
		//LOG(("Failed to create vertex buffer (%X)", hr));
		return -1;
	}

	hr = device->CreateVertexDeclaration(elements, &declaration);
	if( FAILED(hr) )
	{
		//LOG(("Failed to create vertex declaration (%X)", hr));
		return -1;
	}

	vertexBase = 0;

	// Allocate memory for a temporary storage for dynamic vertices
	vertexLimit = 300;
	tempVertexBuffer.resize(vertexLimit*sizeof(SVertex));

	return 0;
}

void CDynRender::ReleaseDeviceObjects()
{
	if( device )
		device = 0;

	if( declaration )
	{
		declaration->Release();
		declaration = 0;
	}

	if( dynamicVertexBuffer )
	{
		dynamicVertexBuffer->Release();
		dynamicVertexBuffer = 0;
	}
}

void CDynRender::SetTexture(IDirect3DTexture9 *tex, VidEffect *eff)
{
	if (texture != tex || effect != eff)
	{
		EndRender();
	
		texture = tex;
		effect = eff;
		
		BeginRender();
	}
}

int CDynRender::BeginRender()
{
	vertexCount     = 0;
	subVertexCount  = 0;
	dynamicVertices = &tempVertexBuffer[0];

	device->SetTexture(0, texture);

	if (texture && effect && effect->IsValid()) {
		D3DSURFACE_DESC desc;
		texture->GetLevelDesc(0, &desc);
		effect->Begin();
	}

	return 0;
}

int CDynRender::EndRender(bool reuse)
{	
	if( vertexCount == 0 ) return 0;

	if( device == 0 ) return -1;

	// Lock the vertex buffer to update it
	if( vertexBase + vertexCount > maxDynamicVertices )
		vertexBase = 0;
	HRESULT hr = dynamicVertexBuffer->Lock(vertexBase * sizeof(SVertex), vertexCount*sizeof(SVertex), (void**)&dynamicVertices, vertexBase ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD);
	if( FAILED(hr) ) 
	{
		//LOG(("Failed to lock vertex buffer (%X)", hr));
		return -1;
	}

	// Copy data
	memcpy(dynamicVertices, &tempVertexBuffer[0], vertexCount*sizeof(SVertex));

	// Unlock
	hr = dynamicVertexBuffer->Unlock();
	dynamicVertices = 0;

	// Set streams
	hr = device->SetStreamSource(0, dynamicVertexBuffer, 0, sizeof(SVertex));

	// Set the vertex declaration that we're using
	hr = device->SetVertexDeclaration(declaration);

	// Draw the vertices
	device->DrawPrimitive(D3DPT_TRIANGLELIST, vertexBase, vertexCount/3);
	vertexBase += vertexCount;

	if (effect && effect->IsValid()) {
		effect->End();
	}

	// reset texture
	if (!reuse) {
		texture = 0;
		effect = 0;
	}

	return 0;
}

void CDynRender::Flush()
{
	EndRender(true);
	BeginRender();
}

void CDynRender::VtxColor(DWORD argb)
{
	color = argb;
}

void CDynRender::VtxTexCoord(float u, float v)
{
	texCoord.x = u;
	texCoord.y = v;
}

void CDynRender::VtxPos(float x, float y, float z)
{
	if( !dynamicVertices ) return;

	// If too many vertices are used, render the current ones and then start over
	if( vertexCount < vertexLimit )
	{
		// Increase the number of vertices used
		vertexCount++;

		((SVertex*)dynamicVertices)->x = x - 0.5f;
		((SVertex*)dynamicVertices)->y = y - 0.5f;
		((SVertex*)dynamicVertices)->z = z;
		((SVertex*)dynamicVertices)->w = 1;
		((SVertex*)dynamicVertices)->color = color;
		((SVertex*)dynamicVertices)->u = texCoord.x;
		((SVertex*)dynamicVertices)->v = texCoord.y;

		dynamicVertices += sizeof(SVertex);

		//if( renderType == RENDER_QUAD_LIST )
		{
			subVertexCount = (++subVertexCount) % 4;
			if( subVertexCount == 0 )
			{
				// Duplicate vertex 0 in the quad
				memcpy(dynamicVertices, dynamicVertices - sizeof(SVertex)*4, sizeof(SVertex));
				dynamicVertices += sizeof(SVertex);

				// Duplicate vertex 2 in the quad
				memcpy(dynamicVertices, dynamicVertices - sizeof(SVertex)*3, sizeof(SVertex));
				dynamicVertices += sizeof(SVertex);

				vertexCount += 2;
			}
		}
	}
	else
	{
		Flush();
		VtxPos(x,y,z);
	}
}
