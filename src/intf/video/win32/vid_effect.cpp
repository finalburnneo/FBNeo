#include "burner.h"
#include "vid_effect.h"
#include <d3d9.h>
#include <d3dx9.h>
#include "directx9_core.h"

#define RELEASE(x) { if ((x)) (x)->Release(); (x) = NULL; }

// ----------------------------------------------------------------------------
// --
// ----------------------------------------------------------------------------
VidEffect::VidEffect(IDirect3DDevice9 *_d3dDevice) : mD3DDevice(_d3dDevice), mEffect(NULL)
{
	mEffect = NULL;
	_D3DXCreateBuffer(0x10000, &mErrorBuffer);
}

// ----------------------------------------------------------------------------
// --
// ----------------------------------------------------------------------------
VidEffect::~VidEffect()
{
	RELEASE(mErrorBuffer);
	RELEASE(mEffect);
}

// ----------------------------------------------------------------------------
// --
// ----------------------------------------------------------------------------
int VidEffect::Load(const char *_filename)
{
	RELEASE(mErrorBuffer);
	RELEASE(mEffect);

	wchar_t filename[64];
	mbstowcs(filename, _filename, strlen(_filename) + 1);

	if (FAILED(_D3DXCreateEffectFromFile(mD3DDevice, filename, NULL, NULL, D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY, NULL, &mEffect, &mErrorBuffer)))
	{
		if (mErrorBuffer) {
			dprintf(_T("  * Error: Couldn't compile effect.\n"));
			dprintf(_T("\n%hs\n\n"), mErrorBuffer->GetBufferPointer());
		}
		return 1;
	}

	return 0;
}

// ----------------------------------------------------------------------------
// --
// ----------------------------------------------------------------------------
void VidEffect::Begin()
{
	mEffect->Begin(NULL, NULL);
	mEffect->BeginPass(0);
}

// ----------------------------------------------------------------------------
// --
// ----------------------------------------------------------------------------
void VidEffect::End()
{
	mEffect->EndPass();
	mEffect->End();
}

// ----------------------------------------------------------------------------
// --
// ----------------------------------------------------------------------------
void VidEffect::SetParamFloat2(const char *_name, float _x, float _y)
{
	float array[] = {_x, _y};
	mEffect->SetFloatArray(_name, array, 2);
}

// ----------------------------------------------------------------------------
// --
// ----------------------------------------------------------------------------
void VidEffect::SetParamFloat4(const char *_name, float _x, float _y, float _z, float _w)
{
	float array[] = {_x, _y, _z, _w};
	mEffect->SetFloatArray(_name, array, 4);
}
