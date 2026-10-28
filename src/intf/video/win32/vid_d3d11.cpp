// Direct3D 11 blitter
//
// Features:
//  - Direct3D 11 device (feature level 9_1 .. 11_0, WARP fallback), flip-model swap chain where available
//  - SoftFX prescale filters, point / linear filtering, rotation & flipping, motion blur
//  - Shaders, selected with a file dialog (+ a list of recently used ones):
//     - libretro / RetroArch style Cg presets (.cgp) and shaders (.cg), e.g. Hyllian's common-shaders:
//       multi-pass, scale types, float / sRGB framebuffers, ORIG / PASSn / PASSPREVn / PREVn / alias
//       inputs, lookup textures (PNG / TGA) and user changeable "#pragma parameter" options
//     - HLSL shaders following the same conventions, and DX9 style .fx effects (single pass)
//    See vid_d3d11_shader.cpp for the preset parser and the Cg -> HLSL translator.
//  - The full OSD of the DirectDraw / Direct3D7 blitters (status icons, short / tiny / joystick
//    messages, chat and netplay edit box), via the OSD backend in vid_directx_support.cpp
//
// The image is presented through a child window that covers the game image (windowed mode), so the
// menu bar and bezel of the main window are left alone, just like the other blitters.
// D3D11, DXGI and D3DCompiler are loaded at runtime; no import libraries are needed.

// The C++ standard headers must come before burner.h: the DirectX SDK's sal.h (pulled in by
// burner.h) defines macros such as __in, __out and __null, which break GCC's libstdc++ headers
#include <string>
#include <vector>
#include "vid_d3d11_shader.h"

#include "burner.h"
#include "vid_softfx.h"

#if defined (_MSC_VER)
 #define FBNEO_HAVE_D3D11
#elif defined (__has_include)
 #if __has_include(<d3d11.h>) && __has_include(<d3dcompiler.h>) && __has_include(<d3d11shader.h>)
  #define FBNEO_HAVE_D3D11
 #endif
#endif

#if defined (FBNEO_HAVE_D3D11)

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <math.h>

#include "spng.h"

#ifndef D3D_COMPILE_STANDARD_FILE_INCLUDE
 #define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)
#endif

#define D3D11_RELEASE(x) { if ((x)) { (x)->Release(); } (x) = NULL; }

// Interface IDs (defined here so no extra import / uuid libraries are needed)
static const GUID FBNEO_IID_IDXGIDevice		= { 0x54ec77fa, 0x1377, 0x44e6, { 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c } };
static const GUID FBNEO_IID_IDXGIDevice1	= { 0x77db970f, 0x6276, 0x48ba, { 0xba, 0x28, 0x07, 0x01, 0x43, 0xb4, 0x39, 0x2c } };
static const GUID FBNEO_IID_IDXGIFactory	= { 0x7b7166ec, 0x21c7, 0x44ae, { 0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69 } };
static const GUID FBNEO_IID_IDXGIFactory5	= { 0x7632e1f5, 0xee65, 0x4dca, { 0x87, 0xfd, 0x84, 0xcd, 0x75, 0xf8, 0x83, 0x8d } };
static const GUID FBNEO_IID_ID3D11Texture2D	= { 0x6f15aaf2, 0xd208, 0x4e89, { 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c } };
// ID3D11ShaderReflection changed IID between d3dcompiler versions
static const GUID FBNEO_IID_ID3D11ShaderReflection_47 = { 0x8d536ca1, 0x0cca, 0x4956, { 0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84 } };
static const GUID FBNEO_IID_ID3D11ShaderReflection_43 = { 0x0a233719, 0x3960, 0x4578, { 0x9d, 0x7c, 0x20, 0x3b, 0x8b, 0x1d, 0x9c, 0xc1 } };

// DXGI values that older headers may not have
#define FBNEO_DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL	((DXGI_SWAP_EFFECT)3)
#define FBNEO_DXGI_SWAP_EFFECT_FLIP_DISCARD		((DXGI_SWAP_EFFECT)4)
#define FBNEO_DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING	0x800
#define FBNEO_DXGI_PRESENT_ALLOW_TEARING		0x200
#define FBNEO_DXGI_FEATURE_PRESENT_ALLOW_TEARING	0

// ----------------------------------------------------------------------------
// Runtime-loaded DLLs

typedef HRESULT (WINAPI* pfnD3D11CreateDevice)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
typedef HRESULT (WINAPI* pfnD3DCompile)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
typedef HRESULT (WINAPI* pfnD3DReflect)(LPCVOID, SIZE_T, REFIID, void**);
typedef HRESULT (WINAPI* pfnD3DPreprocess)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, ID3DBlob**, ID3DBlob**);

static HMODULE hD3D11Dll = NULL;
static HMODULE hD3DCompilerDll = NULL;
static int nD3DCompilerVersion = 0;
static pfnD3D11CreateDevice _D3D11CreateDevice = NULL;
static pfnD3DCompile _D3DCompile = NULL;
static pfnD3DReflect _D3DReflect = NULL;
static pfnD3DPreprocess _D3DPreprocess = NULL;

static void d3d11UnloadLibraries()
{
	if (hD3DCompilerDll) {
		FreeLibrary(hD3DCompilerDll);
		hD3DCompilerDll = NULL;
	}
	if (hD3D11Dll) {
		FreeLibrary(hD3D11Dll);
		hD3D11Dll = NULL;
	}
	_D3D11CreateDevice = NULL;
	_D3DCompile = NULL;
	_D3DReflect = NULL;
	_D3DPreprocess = NULL;
	nD3DCompilerVersion = 0;
}

static int d3d11LoadLibraries()
{
	d3d11UnloadLibraries();

	hD3D11Dll = LoadLibrary(_T("d3d11.dll"));
	if (hD3D11Dll == NULL) {
		return 1;
	}
	_D3D11CreateDevice = (pfnD3D11CreateDevice)GetProcAddress(hD3D11Dll, "D3D11CreateDevice");
	if (_D3D11CreateDevice == NULL) {
		return 1;
	}

	static const struct { const TCHAR* szName; int nVersion; } CompilerDlls[] = {
		{ _T("d3dcompiler_47.dll"), 47 },
		{ _T("d3dcompiler_46.dll"), 46 },
		{ _T("d3dcompiler_43.dll"), 43 },
	};

	for (int i = 0; i < (int)(sizeof(CompilerDlls) / sizeof(CompilerDlls[0])); i++) {
		hD3DCompilerDll = LoadLibrary(CompilerDlls[i].szName);
		if (hD3DCompilerDll) {
			_D3DCompile = (pfnD3DCompile)GetProcAddress(hD3DCompilerDll, "D3DCompile");
			_D3DReflect = (pfnD3DReflect)GetProcAddress(hD3DCompilerDll, "D3DReflect");
			if (_D3DCompile && _D3DReflect) {
				_D3DPreprocess = (pfnD3DPreprocess)GetProcAddress(hD3DCompilerDll, "D3DPreprocess");
				nD3DCompilerVersion = CompilerDlls[i].nVersion;
				break;
			}
			FreeLibrary(hD3DCompilerDll);
			hD3DCompilerDll = NULL;
			_D3DCompile = NULL;
			_D3DReflect = NULL;
		}
	}

	if (_D3DCompile == NULL) {
		return 2;
	}

	return 0;
}

static HRESULT d3d11Reflect(ID3DBlob* pCode, ID3D11ShaderReflection** ppRefl)
{
	*ppRefl = NULL;
	if (_D3DReflect == NULL || pCode == NULL) {
		return E_FAIL;
	}
	const GUID* pIID = (nD3DCompilerVersion >= 46) ? &FBNEO_IID_ID3D11ShaderReflection_47 : &FBNEO_IID_ID3D11ShaderReflection_43;
	return _D3DReflect(pCode->GetBufferPointer(), pCode->GetBufferSize(), *pIID, (void**)ppRefl);
}

// ----------------------------------------------------------------------------
// Device state

struct d3d11Vertex {
	float x, y, z, w;		// clip space position
	float u, v;				// texture coordinates
	float r, g, b, a;		// colour (alpha used for motion blur)
	float u1, v1;			// 0..1 across the image (TEXCOORD1: lookup texture coordinates for shaders)
};

static ID3D11Device* pDevice = NULL;
static ID3D11DeviceContext* pContext = NULL;
static IDXGISwapChain* pSwapChain = NULL;
static ID3D11RenderTargetView* pRenderTarget = NULL;
static D3D_FEATURE_LEVEL nFeatureLevel = D3D_FEATURE_LEVEL_9_1;
static bool bWarpDevice = false;
static DXGI_SWAP_EFFECT nSwapEffect = DXGI_SWAP_EFFECT_DISCARD;
static UINT nSwapChainFlags = 0;
static UINT nSwapChainWidth = 0, nSwapChainHeight = 0;
static UINT nDeviceGeneration = 0;					// bumped every time a device is created (OSD textures follow it)
static bool bDeviceLost = false;

static ID3D11VertexShader* pBasicVS = NULL;
static ID3D11PixelShader* pBasicPS = NULL;
static ID3D11PixelShader* pOSDPS = NULL;
static ID3D11InputLayout* pBasicLayout = NULL;
static ID3D11Buffer* pGameVB = NULL;
static ID3D11Buffer* pOSDVB = NULL;
static ID3D11SamplerState* pSamplerPoint = NULL;
static ID3D11SamplerState* pSamplerLinear = NULL;
static ID3D11RasterizerState* pRasterState = NULL;
static ID3D11BlendState* pBlendAlpha = NULL;

// The emulated image; a ring of textures when previous frames are needed (motion blur / shader PREV inputs)
static ID3D11Texture2D* pGameTexture[8] = { NULL, };
static ID3D11ShaderResourceView* pGameSRV[8] = { NULL, };
static int nGameTextures = 0;
static int nCurrentTexture = 0;
static int nFramesUploaded = 0;
static bool bTextureValid = false;

static d3d11Vertex GameQuad[4];

// View (child) window that the swap chain presents into
static HWND hD3D11View = NULL;
static const TCHAR szD3D11ViewClass[] = _T("FBNeoD3D11View");
static bool bViewClassRegistered = false;
static bool bInPaint = false;
static bool bD3D11Ready = false;				// set once initialisation has completed

// Fullscreen display mode handling
static bool bChangedDisplayMode = false;
static TCHAR szFullscreenDevice[CCHDEVICENAME] = _T("");

// Game image
static int nRotateGame = 0;
static int nGameWidth = 0, nGameHeight = 0;
static int nGameImageWidth = 0, nGameImageHeight = 0;
static int nTextureWidth = 0, nTextureHeight = 0;
static int nImageWidth = 0, nImageHeight = 0;		// size of the image on screen (in game orientation)
static int nPreScaleZoom = 1;
static int nPreScaleEffect = 0;
static unsigned char* pConvBuffer = NULL;			// for SoftFX filters that only output 16-bit
static int nConvPitch = 0;

static RECT Dest = { 0, 0, 0, 0 };					// game image, client coordinates of hVidWnd
static RECT Render = { 0, 0, 0, 0 };				// game image, render target coordinates

// ----------------------------------------------------------------------------
// Built-in shaders

static const char szBuiltinShaders[] =
	"Texture2D tex0 : register(t0);\n"
	"SamplerState samp0 : register(s0);\n"
	"struct VSIn  { float4 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };\n"
	"struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float4 col : COLOR0; };\n"
	"VSOut vs_main(VSIn i) { VSOut o; o.pos = i.pos; o.uv = i.uv; o.col = i.col; return o; }\n"
	"float4 ps_main(VSOut i) : SV_Target { float4 c = tex0.Sample(samp0, i.uv); return float4(c.rgb, i.col.a); }\n"
	// OSD surfaces use colour key 0x000001 (B = 1, G = 0, R = 0)
	"float4 ps_osd(VSOut i) : SV_Target {\n"
	"	float4 c = tex0.Sample(samp0, i.uv);\n"
	"	float3 d = abs(c.rgb - float3(0.0, 0.0, 1.0 / 255.0));\n"
	"	if (d.r < 0.5 / 255.0 && d.g < 0.5 / 255.0 && d.b < 0.5 / 255.0) discard;\n"
	"	return float4(c.rgb, 1.0);\n"
	"}\n";

static const D3D11_INPUT_ELEMENT_DESC BasicLayout[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

#define BASIC_LAYOUT_SIZE (sizeof(BasicLayout) / sizeof(BasicLayout[0]))

static const char* d3d11ShaderTarget(bool bPixelShader)
{
	if (nFeatureLevel >= D3D_FEATURE_LEVEL_10_0) {
		return bPixelShader ? "ps_4_0" : "vs_4_0";
	}
	if (nFeatureLevel >= D3D_FEATURE_LEVEL_9_3) {
		return bPixelShader ? "ps_4_0_level_9_3" : "vs_4_0_level_9_3";
	}
	return bPixelShader ? "ps_4_0_level_9_1" : "vs_4_0_level_9_1";
}

// Log text of any length: FBNeo's dprintf() formats into a 1024 character buffer, so the text goes
// out a line at a time, each line clamped, and always through "%hs" (compiler output may contain '%')
static void d3d11LogText(const char* pText, size_t nLen, int nMaxLines)
{
	size_t nPos = 0;
	int nLines = 0;

	while (nPos < nLen && pText[nPos]) {
		size_t nEnd = nPos;
		while (nEnd < nLen && pText[nEnd] && pText[nEnd] != '\n') nEnd++;

		if (nLines >= nMaxLines) {
			dprintf(_T("    (more lines not shown)\n"));
			return;
		}

		char szLine[800];
		size_t nLineLen = nEnd - nPos;
		if (nLineLen > sizeof(szLine) - 1) nLineLen = sizeof(szLine) - 1;
		memcpy(szLine, pText + nPos, nLineLen);
		szLine[nLineLen] = 0;
		if (nLineLen && szLine[nLineLen - 1] == '\r') szLine[nLineLen - 1] = 0;
		dprintf(_T("%hs\n"), szLine);

		nLines++;
		nPos = nEnd + 1;
	}
}

static void d3d11PrintCompilerErrors(ID3DBlob* pErrors)
{
	if (pErrors && pErrors->GetBufferSize()) {
		d3d11LogText((const char*)pErrors->GetBufferPointer(), pErrors->GetBufferSize(), 40);
	}
}

static HRESULT d3d11Compile(const char* pSrc, size_t nLen, const char* pSourceName, const char* pEntry, bool bPixelShader, bool bCompat, ID3DBlob** ppCode)
{
	ID3DBlob* pErrors = NULL;
	D3D_SHADER_MACRO Macros[] = { { "FBNEO_D3D11", "1" }, { NULL, NULL } };
	UINT nFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
	if (bCompat) {
		nFlags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
	}

	*ppCode = NULL;

	HRESULT hr = _D3DCompile(pSrc, nLen, pSourceName, Macros, pSourceName ? D3D_COMPILE_STANDARD_FILE_INCLUDE : NULL, pEntry, d3d11ShaderTarget(bPixelShader), nFlags, 0, ppCode, &pErrors);

	if (FAILED(hr)) {
		dprintf(_T("  * Error: Couldn't compile shader \"%hs\" (%hs, %hs).\n"), pSourceName ? pSourceName : "built-in", pEntry, d3d11ShaderTarget(bPixelShader));
		d3d11PrintCompilerErrors(pErrors);
		D3D11_RELEASE(*ppCode);
	}

	D3D11_RELEASE(pErrors);

	return hr;
}

// ----------------------------------------------------------------------------
// Helpers

static int GetTextureSize(int nSize)
{
	int nTextureSize = 128;

	while (nTextureSize < nSize) {
		nTextureSize <<= 1;
	}

	return nTextureSize;
}

static ID3D11SamplerState* d3d11CreateSampler(bool bLinear, D3D11_TEXTURE_ADDRESS_MODE nAddress)
{
	D3D11_SAMPLER_DESC sd;
	ID3D11SamplerState* pSampler = NULL;

	memset(&sd, 0, sizeof(sd));
	sd.Filter = bLinear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = nAddress;
	sd.AddressV = nAddress;
	sd.AddressW = nAddress;
	sd.MaxAnisotropy = 1;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MinLOD = 0.0f;
	sd.MaxLOD = D3D11_FLOAT32_MAX;

	if (FAILED(pDevice->CreateSamplerState(&sd, &pSampler))) {
		return NULL;
	}

	return pSampler;
}

static ID3D11Buffer* d3d11CreateDynamicBuffer(UINT nSize, UINT nBind)
{
	D3D11_BUFFER_DESC bd;
	ID3D11Buffer* pBuffer = NULL;

	memset(&bd, 0, sizeof(bd));
	bd.ByteWidth = nSize;
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.BindFlags = nBind;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(pDevice->CreateBuffer(&bd, NULL, &pBuffer))) {
		return NULL;
	}

	return pBuffer;
}

static void d3d11UpdateBuffer(ID3D11Buffer* pBuffer, const void* pData, UINT nSize)
{
	D3D11_MAPPED_SUBRESOURCE ms;

	if (SUCCEEDED(pContext->Map(pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
		memcpy(ms.pData, pData, nSize);
		pContext->Unmap(pBuffer, 0);
	}
}

static void d3d11SetVertex(d3d11Vertex* pVertex, float x, float y, float u, float v, float a)
{
	pVertex->x = x; pVertex->y = y; pVertex->z = 0.0f; pVertex->w = 1.0f;
	pVertex->u = u; pVertex->v = v;
	pVertex->r = 1.0f; pVertex->g = 1.0f; pVertex->b = 1.0f; pVertex->a = a;
	pVertex->u1 = 0.0f; pVertex->v1 = 0.0f;
}

// ----------------------------------------------------------------------------
// HardFX shader loader

// Overwrite pSrc[nStart, nEnd) with spaces, keeping line breaks (so compiler line numbers stay valid)
static void BlankSource(char* pSrc, size_t nStart, size_t nEnd)
{
	for (size_t i = nStart; i < nEnd; i++) {
		if (pSrc[i] != '\n' && pSrc[i] != '\r') {
			pSrc[i] = ' ';
		}
	}
}

static bool IsIdentChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// Find whole-word sToken in pSrc starting at nPos
static const char* FindToken(const char* pSrc, const char* pStart, const char* sToken)
{
	size_t nLen = strlen(sToken);
	const char* p = pStart;

	while ((p = strstr(p, sToken)) != NULL) {
		bool bStartOK = (p == pSrc) || !IsIdentChar(p[-1]);
		bool bEndOK = !IsIdentChar(p[nLen]);
		if (bStartOK && bEndOK) {
			return p;
		}
		p += nLen;
	}

	return NULL;
}

// Given pOpen pointing at '{', return pointer to the matching '}' (or NULL)
static const char* FindMatchingBrace(const char* pOpen)
{
	int nDepth = 0;

	for (const char* p = pOpen; *p; p++) {
		if (*p == '{') {
			nDepth++;
		} else if (*p == '}') {
			if (--nDepth == 0) {
				return p;
			}
		}
	}

	return NULL;
}

static void SkipSpace(const char** pp)
{
	while (**pp == ' ' || **pp == '\t' || **pp == '\r' || **pp == '\n') {
		(*pp)++;
	}
}

static bool ReadIdent(const char** pp, char* pOut, int nOutSize)
{
	int n = 0;

	SkipSpace(pp);
	while (IsIdentChar(**pp)) {
		if (n < nOutSize - 1) {
			pOut[n++] = **pp;
		}
		(*pp)++;
	}
	pOut[n] = 0;

	return n > 0;
}

// Find the entry points named by "compile vs_x_x name(" / "compile ps_x_x name(" in a technique block
static void ParseTechniqueEntryPoints(const char* pSrc, char* szVS, char* szPS, int nSize)
{
	const char* p = pSrc;

	szVS[0] = 0;
	szPS[0] = 0;

	while ((p = FindToken(pSrc, p, "compile")) != NULL) {
		char szProfile[32], szEntry[64];
		const char* q = p + 7;

		if (ReadIdent(&q, szProfile, sizeof(szProfile)) && ReadIdent(&q, szEntry, sizeof(szEntry))) {
			SkipSpace(&q);
			if (*q == '(') {
				if ((szProfile[0] == 'p' || szProfile[0] == 'P') && szPS[0] == 0) {
					strncpy(szPS, szEntry, nSize - 1);
					szPS[nSize - 1] = 0;
				}
				if ((szProfile[0] == 'v' || szProfile[0] == 'V') && szVS[0] == 0) {
					strncpy(szVS, szEntry, nSize - 1);
					szVS[nSize - 1] = 0;
				}
			}
		}
		p += 7;
	}
}

// Look for "name ... = sampler_state { ... }" and read the filter / address mode from it
static void ParseSamplerState(const char* pSrc, const char* szName, int* pnLinear, D3D11_TEXTURE_ADDRESS_MODE* pnAddress)
{
	const char* p = pSrc;

	while ((p = FindToken(pSrc, p, szName)) != NULL) {
		const char* q = p + strlen(szName);

		// allow a register / semantic between the name and the '='
		while (*q && *q != '=' && *q != ';' && *q != '{' && *q != '(' && *q != ',') {
			q++;
		}
		if (*q == '=') {
			q++;
			SkipSpace(&q);
			if (strncmp(q, "sampler_state", 13) == 0) {
				const char* pOpen = strchr(q, '{');
				const char* pClose = pOpen ? FindMatchingBrace(pOpen) : NULL;
				if (pOpen && pClose) {
					char szBlock[1024];
					size_t nLen = pClose - pOpen;
					if (nLen >= sizeof(szBlock)) {
						nLen = sizeof(szBlock) - 1;
					}
					for (size_t i = 0; i < nLen; i++) {
						char c = pOpen[i];
						szBlock[i] = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
					}
					szBlock[nLen] = 0;

					const char* f = strstr(szBlock, "MAGFILTER");
					if (f == NULL) f = strstr(szBlock, "MINFILTER");
					if (f == NULL) f = strstr(szBlock, "FILTER");
					if (f) {
						const char* e = strchr(f, ';');
						const char* l = strstr(f, "LINEAR");
						const char* t = strstr(f, "POINT");
						if (l && (e == NULL || l < e)) *pnLinear = 1;
						if (t && (e == NULL || t < e)) *pnLinear = 0;
					}

					const char* a = strstr(szBlock, "ADDRESSU");
					if (a) {
						const char* e = strchr(a, ';');
						const char* w = strstr(a, "WRAP");
						const char* b = strstr(a, "BORDER");
						const char* m = strstr(a, "MIRROR");
						if (w && (e == NULL || w < e)) *pnAddress = D3D11_TEXTURE_ADDRESS_WRAP;
						if (b && (e == NULL || b < e)) *pnAddress = D3D11_TEXTURE_ADDRESS_BORDER;
						if (m && (e == NULL || m < e)) *pnAddress = D3D11_TEXTURE_ADDRESS_MIRROR;
					}
				}
				return;
			}
		}
		p += strlen(szName);
	}
}

// Remove D3D9 effect-framework syntax that a non-effect compile doesn't need:
// technique blocks and "= sampler_state { ... }" initialisers
static void StripEffectSyntax(char* pSrc)
{
	static const char* Techniques[] = { "technique", "technique10", "technique11" };

	for (int t = 0; t < 3; t++) {
		const char* p = pSrc;
		while ((p = FindToken(pSrc, p, Techniques[t])) != NULL) {
			const char* pOpen = strchr(p, '{');
			const char* pClose = pOpen ? FindMatchingBrace(pOpen) : NULL;
			if (pClose == NULL) {
				break;
			}
			BlankSource(pSrc, p - pSrc, pClose - pSrc + 1);
			p = pClose + 1;
		}
	}

	const char* p = pSrc;
	while ((p = FindToken(pSrc, p, "sampler_state")) != NULL) {
		// walk back to the '='
		const char* pEq = p - 1;
		while (pEq > pSrc && (*pEq == ' ' || *pEq == '\t' || *pEq == '\r' || *pEq == '\n')) {
			pEq--;
		}
		const char* pOpen = strchr(p, '{');
		const char* pClose = pOpen ? FindMatchingBrace(pOpen) : NULL;
		if (*pEq != '=' || pClose == NULL) {
			p += 13;
			continue;
		}
		BlankSource(pSrc, pEq - pSrc, pClose - pSrc + 1);
		p = pClose + 1;
	}
}

static void AppendString(char* pDest, size_t nSize, const char* pFormat, ...)
{
	size_t nLen = strlen(pDest);
	if (nLen >= nSize - 1) {
		return;
	}
	va_list args;
	va_start(args, pFormat);
	_vsnprintf(pDest + nLen, nSize - nLen - 1, pFormat, args);
	va_end(args);
	pDest[nSize - 1] = 0;
}

// Write a vertex shader whose outputs exactly match the input signature of the pixel shader
static int GenerateVertexShader(ID3DBlob* pPSCode, char* pOut, size_t nOutSize)
{
	static const char* Swizzle[5] = { "", ".x", ".xy", ".xyz", "" };
	static const char* TypeNames[3] = { "float", "uint", "int" };

	ID3D11ShaderReflection* pRefl = NULL;
	D3D11_SHADER_DESC sd;
	D3D11_SIGNATURE_PARAMETER_DESC Params[32];
	int nParams = 0;
	bool bHasPosition = false;

	if (FAILED(d3d11Reflect(pPSCode, &pRefl))) {
		return 1;
	}

	memset(&sd, 0, sizeof(sd));
	pRefl->GetDesc(&sd);

	for (UINT i = 0; i < sd.InputParameters && nParams < 32; i++) {
		D3D11_SIGNATURE_PARAMETER_DESC pd;
		memset(&pd, 0, sizeof(pd));
		if (FAILED(pRefl->GetInputParameterDesc(i, &pd))) {
			continue;
		}
		if (pd.SystemValueType != D3D_NAME_UNDEFINED && pd.SystemValueType != D3D_NAME_POSITION) {
			continue;					// generated by the rasterizer, not the vertex shader
		}
		Params[nParams++] = pd;
	}

	// sort by register (then by first component used)
	for (int i = 0; i < nParams; i++) {
		for (int j = i + 1; j < nParams; j++) {
			int nFirstI = 0, nFirstJ = 0;
			while (nFirstI < 4 && !(Params[i].Mask & (1 << nFirstI))) nFirstI++;
			while (nFirstJ < 4 && !(Params[j].Mask & (1 << nFirstJ))) nFirstJ++;
			if (Params[j].Register < Params[i].Register || (Params[j].Register == Params[i].Register && nFirstJ < nFirstI)) {
				D3D11_SIGNATURE_PARAMETER_DESC t = Params[i]; Params[i] = Params[j]; Params[j] = t;
			}
		}
	}

	pOut[0] = 0;
	AppendString(pOut, nOutSize, "struct VSIn { float4 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; float2 uv1 : TEXCOORD1; };\n");
	AppendString(pOut, nOutSize, "struct VSOut {\n");

	for (int i = 0; i < nParams; i++) {
		int nComponents = 0;
		for (int c = 0; c < 4; c++) {
			if (Params[i].Mask & (1 << c)) {
				nComponents = c + 1;
			}
		}
		if (nComponents == 0) {
			nComponents = 4;
		}

		int nType = 0;
		if (Params[i].ComponentType == D3D_REGISTER_COMPONENT_UINT32) nType = 1;
		if (Params[i].ComponentType == D3D_REGISTER_COMPONENT_SINT32) nType = 2;

		if (Params[i].SystemValueType == D3D_NAME_POSITION) {
			bHasPosition = true;
			AppendString(pOut, nOutSize, "\tfloat4 o%d : SV_Position;\n", i);
		} else {
			AppendString(pOut, nOutSize, "\t%s%s%d o%d : %s%d;\n", nType ? "nointerpolation " : "", TypeNames[nType], nComponents, i, Params[i].SemanticName, Params[i].SemanticIndex);
		}
	}
	if (!bHasPosition) {
		AppendString(pOut, nOutSize, "\tfloat4 fbneo_pos : SV_Position;\n");
	}
	AppendString(pOut, nOutSize, "};\n");

	AppendString(pOut, nOutSize, "VSOut main(VSIn i) {\n\tVSOut o;\n");
	for (int i = 0; i < nParams; i++) {
		int nComponents = 0;
		for (int c = 0; c < 4; c++) {
			if (Params[i].Mask & (1 << c)) {
				nComponents = c + 1;
			}
		}
		if (nComponents == 0) {
			nComponents = 4;
		}

		int nType = 0;
		if (Params[i].ComponentType == D3D_REGISTER_COMPONENT_UINT32) nType = 1;
		if (Params[i].ComponentType == D3D_REGISTER_COMPONENT_SINT32) nType = 2;

		if (Params[i].SystemValueType == D3D_NAME_POSITION) {
			AppendString(pOut, nOutSize, "\to.o%d = i.pos;\n", i);
		} else if (nType) {
			AppendString(pOut, nOutSize, "\to.o%d = (%s%d)0;\n", i, TypeNames[nType], nComponents);
		} else if (_strnicmp(Params[i].SemanticName, "TEXCOORD", 8) == 0) {
			// TEXCOORD1 is 0..1 across the image (lookup textures), the others are the input's coordinates
			AppendString(pOut, nOutSize, "\to.o%d = float4(%s, 0.0, 1.0)%s;\n", i, (Params[i].SemanticIndex == 1) ? "i.uv1" : "i.uv", Swizzle[nComponents]);
		} else if (_strnicmp(Params[i].SemanticName, "COLOR", 5) == 0) {
			AppendString(pOut, nOutSize, "\to.o%d = i.col%s;\n", i, Swizzle[nComponents]);
		} else {
			AppendString(pOut, nOutSize, "\to.o%d = (float%d)0;\n", i, nComponents);
		}
	}
	if (!bHasPosition) {
		AppendString(pOut, nOutSize, "\to.fbneo_pos = i.pos;\n");
	}
	AppendString(pOut, nOutSize, "\treturn o;\n}\n");

	pRefl->Release();

	return 0;
}

// Input layout for a vertex shader supplied by the HardFX file
static HRESULT CreateLayoutForVertexShader(ID3DBlob* pVSCode, ID3D11InputLayout** ppLayout)
{
	ID3D11ShaderReflection* pRefl = NULL;
	D3D11_SHADER_DESC sd;
	D3D11_INPUT_ELEMENT_DESC Elements[16];
	D3D11_SIGNATURE_PARAMETER_DESC Params[16];
	int nElements = 0;

	*ppLayout = NULL;

	if (FAILED(d3d11Reflect(pVSCode, &pRefl))) {
		return E_FAIL;
	}

	memset(&sd, 0, sizeof(sd));
	pRefl->GetDesc(&sd);

	for (UINT i = 0; i < sd.InputParameters && nElements < 16; i++) {
		D3D11_SIGNATURE_PARAMETER_DESC* pd = &Params[nElements];
		memset(pd, 0, sizeof(*pd));
		if (FAILED(pRefl->GetInputParameterDesc(i, pd)) || pd->SystemValueType == D3D_NAME_VERTEX_ID || pd->SystemValueType == D3D_NAME_INSTANCE_ID ||
			_strnicmp(pd->SemanticName, "SV_VertexID", 11) == 0 || _strnicmp(pd->SemanticName, "SV_InstanceID", 13) == 0) {
			continue;					// these don't come from the vertex buffer
		}

		D3D11_INPUT_ELEMENT_DESC* pe = &Elements[nElements];
		memset(pe, 0, sizeof(*pe));
		pe->SemanticName = pd->SemanticName;
		pe->SemanticIndex = pd->SemanticIndex;
		pe->InputSlot = 0;
		pe->InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

		if (_strnicmp(pd->SemanticName, "POSITION", 8) == 0 || _strnicmp(pd->SemanticName, "SV_POSITION", 11) == 0 || _stricmp(pd->SemanticName, "POS") == 0) {
			pe->Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			pe->AlignedByteOffset = 0;
		} else if (_strnicmp(pd->SemanticName, "COLOR", 5) == 0) {
			pe->Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			pe->AlignedByteOffset = 24;
		} else if (_stricmp(pd->SemanticName, "FBNEOUV") == 0 || (_stricmp(pd->SemanticName, "TEXCOORD") == 0 && pd->SemanticIndex == 1)) {
			pe->Format = DXGI_FORMAT_R32G32_FLOAT;			// 0..1 across the image (lookup textures)
			pe->AlignedByteOffset = 40;
		} else {
			pe->Format = DXGI_FORMAT_R32G32_FLOAT;			// TEXCOORDn and anything else
			pe->AlignedByteOffset = 16;
		}
		nElements++;
	}

	// (the semantic names belong to the reflection object, so it's released after creating the layout)
	HRESULT hr;
	if (nElements == 0) {
		hr = pDevice->CreateInputLayout(BasicLayout, BASIC_LAYOUT_SIZE, pVSCode->GetBufferPointer(), pVSCode->GetBufferSize(), ppLayout);
	} else {
		hr = pDevice->CreateInputLayout(Elements, nElements, pVSCode->GetBufferPointer(), pVSCode->GetBufferSize(), ppLayout);
	}

	pRefl->Release();

	return hr;
}

// ----------------------------------------------------------------------------
// Shader presets: libretro style Cg (.cgp / .cg, e.g. Hyllian's common-shaders), HLSL and DX9 .fx
//
// Each pass is compiled in the first of these modes that works:
//  - "HLSL_4": the shader's own Direct3D 10/11 path (Hyllian's compat_includes), compiled as is
//  - "Cg":     preprocessed, then translated to HLSL by D3D11TranslateCg()
//  - "FX":     a DX9 effect file, compiled in backwards compatibility mode

#define D3D11_MAX_HISTORY		8			// original frames kept for PREV .. PREV6

enum { D3D11_VAR_FLOAT = 0, D3D11_VAR_INT, D3D11_VAR_UINT, D3D11_VAR_BOOL };

struct d3d11UniformVar {
	int nUniform;
	int nSource;
	int nIndex;
	UINT nOffset;
	UINT nSize;
	int nType;								// D3D11_VAR_*: the values are converted from float when written
};

struct d3d11CBuffer {
	ID3D11Buffer* pBuffer;
	UINT nSlot;
	UINT nSize;
	std::vector<unsigned char> Data;
	std::vector<d3d11UniformVar> Vars;
};

struct d3d11TexBinding {
	UINT nSlot;
	int nSource;
	int nIndex;
};

struct d3d11SamplerBinding {
	UINT nSlot;
	int nFilter;							// 1 linear, 0 point, -1 pass setting
	int nWrap;
};

struct d3d11StageBindings {
	std::vector<d3d11CBuffer> CBuffers;
	std::vector<d3d11TexBinding> Textures;
	std::vector<d3d11SamplerBinding> Samplers;
};

struct d3d11ShaderPass {
	D3D11ShaderPassDesc Desc;
	ID3D11VertexShader* pVS;
	ID3D11PixelShader* pPS;
	ID3D11InputLayout* pLayout;
	d3d11StageBindings VSBind;
	d3d11StageBindings PSBind;
	ID3D11Texture2D* pTexture;				// render target (unless the pass renders to the screen)
	ID3D11RenderTargetView* pRTV;
	ID3D11ShaderResourceView* pSRV;
	int nWidth, nHeight;					// size of the image rendered into the target
	int nTexWidth, nTexHeight;				// size of the render target texture (a power of 2, like RetroArch)
	bool bToScreen;
	const char* szMode;
};

struct d3d11LUT {
	ID3D11Texture2D* pTexture;
	ID3D11ShaderResourceView* pSRV;
	int nWidth, nHeight;
};

struct d3d11Shader {
	std::string szPath;						// UTF-8
	D3D11ShaderPreset Preset;
	std::vector<d3d11ShaderPass> Passes;
	std::vector<d3d11LUT> LUTs;
	std::vector<D3D11ShaderParam> Params;
	std::vector<std::string> Aliases;
	int nHistory;							// number of previous frames used (PREV = 1)
};

static d3d11Shader* pShader = NULL;
static int d3d11UploadImage();
static int d3d11Paint(int bValidate);
static ID3D11Buffer* pPassVB = NULL;
static ID3D11SamplerState* pSamplerCache[2][4] = { { NULL, }, };	// [linear][wrap]

// Paths are kept as UTF-8 in the shader code
static std::wstring d3d11Widen(const std::string& s)
{
	if (s.empty()) return std::wstring();
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
	std::wstring w(n, 0);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
	return w;
}

static std::string d3d11NarrowW(const wchar_t* w)
{
	if (w == NULL || *w == 0) return std::string();
	int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
	std::string s(n > 0 ? n - 1 : 0, 0);
	if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, NULL, NULL);
	return s;
}

static std::string d3d11Narrow(const TCHAR* t)
{
#if defined (UNICODE) || defined (_UNICODE)
	return d3d11NarrowW(t);
#else
	if (t == NULL || *t == 0) return std::string();
	int n = MultiByteToWideChar(CP_ACP, 0, t, -1, NULL, 0);
	std::wstring w(n, 0);
	MultiByteToWideChar(CP_ACP, 0, t, -1, &w[0], n);
	return d3d11NarrowW(w.c_str());
#endif
}

static FILE* d3d11OpenFile(const std::string& szPath, const wchar_t* szMode)
{
	return _wfopen(d3d11Widen(szPath).c_str(), szMode);
}

static bool d3d11ReadFile(const std::string& szPath, std::string& Data)
{
	FILE* fp = d3d11OpenFile(szPath, L"rb");
	if (fp == NULL) {
		return false;
	}

	Data.clear();
	char Buf[16384];
	size_t n;
	while ((n = fread(Buf, 1, sizeof(Buf), fp)) > 0) {
		Data.append(Buf, n);
	}
	fclose(fp);

	return true;
}

static std::string d3d11BaseName(const std::string& szPath)
{
	size_t n = szPath.find_last_of("/\\");
	return (n == std::string::npos) ? szPath : szPath.substr(n + 1);
}

// ----------------------------------------------------------------------------
// Lookup textures (PNG / TGA)

static unsigned char* d3d11LoadTGA(FILE* fp, int* pWidth, int* pHeight)
{
	unsigned char h[18];
	if (fread(h, 1, 18, fp) != 18) return NULL;

	int nType = h[2], w = h[12] | (h[13] << 8), hh = h[14] | (h[15] << 8), bpp = h[16];
	bool bTopDown = (h[17] & 0x20) != 0;
	if ((nType != 2 && nType != 10) || (bpp != 24 && bpp != 32) || w <= 0 || hh <= 0) return NULL;
	fseek(fp, h[0], SEEK_CUR);

	int nBytes = bpp / 8;
	unsigned char* pOut = (unsigned char*)malloc(w * hh * 4);
	if (pOut == NULL) return NULL;

	int nPixels = w * hh, i = 0;
	unsigned char px[4] = { 0, 0, 0, 255 };
	while (i < nPixels) {
		int nCount = 1;
		bool bRaw = true;
		if (nType == 10) {
			int c = fgetc(fp);
			if (c == EOF) break;
			nCount = (c & 0x7F) + 1;
			bRaw = (c & 0x80) == 0;
			if (!bRaw && fread(px, 1, nBytes, fp) != (size_t)nBytes) break;
		}
		for (int k = 0; k < nCount && i < nPixels; k++, i++) {
			if (bRaw && fread(px, 1, nBytes, fp) != (size_t)nBytes) { i = nPixels; break; }
			int x = i % w, y = i / w;
			if (!bTopDown) y = hh - 1 - y;
			unsigned char* d = pOut + (y * w + x) * 4;
			d[0] = px[2]; d[1] = px[1]; d[2] = px[0]; d[3] = (nBytes == 4) ? px[3] : 255;
		}
	}

	*pWidth = w;
	*pHeight = hh;
	return pOut;
}

static unsigned char* d3d11LoadPNG(FILE* fp, int* pWidth, int* pHeight)
{
	spng_ctx* ctx = spng_ctx_new(0);
	if (ctx == NULL) return NULL;

	unsigned char* pOut = NULL;
	struct spng_ihdr ihdr;
	size_t nSize = 0;

	spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);
	spng_set_png_file(ctx, fp);

	if (spng_get_ihdr(ctx, &ihdr) == 0 && spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &nSize) == 0) {
		pOut = (unsigned char*)malloc(nSize);
		if (pOut && spng_decode_image(ctx, pOut, nSize, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS) != 0) {
			free(pOut);
			pOut = NULL;
		}
		*pWidth = ihdr.width;
		*pHeight = ihdr.height;
	}

	spng_ctx_free(ctx);
	return pOut;
}

static int d3d11LoadLUT(const D3D11ShaderLUT& Desc, d3d11LUT* pLUT, std::string& szErrors)
{
	memset(pLUT, 0, sizeof(*pLUT));

	FILE* fp = d3d11OpenFile(Desc.szPath, L"rb");
	if (fp == NULL) {
		szErrors += "Couldn't open texture " + Desc.szPath + "\n";
		return 1;
	}

	int w = 0, h = 0;
	std::string szExt = D3D11ShaderExtension(Desc.szPath);
	unsigned char* pPixels = (szExt == "tga") ? d3d11LoadTGA(fp, &w, &h) : d3d11LoadPNG(fp, &w, &h);
	fclose(fp);

	if (pPixels == NULL) {
		szErrors += "Couldn't decode texture " + Desc.szPath + " (only PNG and TGA are supported)\n";
		return 1;
	}

	D3D11_TEXTURE2D_DESC td;
	memset(&td, 0, sizeof(td));
	td.Width = w;
	td.Height = h;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_IMMUTABLE;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = E_FAIL;

	if (Desc.bMipmap) {
		// "<name>_mipmap = true": a full mip chain, generated by the GPU
		td.MipLevels = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		hr = pDevice->CreateTexture2D(&td, NULL, &pLUT->pTexture);
		if (SUCCEEDED(hr)) {
			hr = pDevice->CreateShaderResourceView(pLUT->pTexture, NULL, &pLUT->pSRV);
		}
		if (SUCCEEDED(hr)) {
			pContext->UpdateSubresource(pLUT->pTexture, 0, NULL, pPixels, w * 4, 0);
			pContext->GenerateMips(pLUT->pSRV);
		} else {
			D3D11_RELEASE(pLUT->pSRV);
			D3D11_RELEASE(pLUT->pTexture);
			td.MipLevels = 1;						// fall back to a single level
			td.Usage = D3D11_USAGE_IMMUTABLE;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			td.MiscFlags = 0;
		}
	}

	if (pLUT->pTexture == NULL) {
		D3D11_SUBRESOURCE_DATA sd;
		sd.pSysMem = pPixels;
		sd.SysMemPitch = w * 4;
		sd.SysMemSlicePitch = 0;

		hr = pDevice->CreateTexture2D(&td, &sd, &pLUT->pTexture);
		if (SUCCEEDED(hr)) {
			hr = pDevice->CreateShaderResourceView(pLUT->pTexture, NULL, &pLUT->pSRV);
		}
	}

	free(pPixels);

	if (FAILED(hr)) {
		D3D11_RELEASE(pLUT->pSRV);
		D3D11_RELEASE(pLUT->pTexture);
		szErrors += "Couldn't create texture " + Desc.szPath + "\n";
		return 1;
	}

	pLUT->nWidth = w;
	pLUT->nHeight = h;

	return 0;
}

// ----------------------------------------------------------------------------
// Compiling

static HRESULT d3d11CompileEx(const std::string& Src, const std::string& szName, const char* pEntry, bool bPixelShader, const D3D_SHADER_MACRO* pMacros, UINT nFlags, ID3DBlob** ppCode, std::string* pErrors)
{
	ID3DBlob* pErrorBlob = NULL;

	*ppCode = NULL;

	HRESULT hr = _D3DCompile(Src.c_str(), Src.size(), szName.c_str(), pMacros, NULL, pEntry, d3d11ShaderTarget(bPixelShader), nFlags | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, ppCode, &pErrorBlob);

	if (FAILED(hr)) {
		if (pErrors) {
			*pErrors += std::string(pEntry) + ": ";
			if (pErrorBlob && pErrorBlob->GetBufferSize()) {
				*pErrors += std::string((const char*)pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize() - 1);
			} else {
				*pErrors += "compilation failed";
			}
			*pErrors += "\n";
		}
		D3D11_RELEASE(*ppCode);
	}

	D3D11_RELEASE(pErrorBlob);

	return hr;
}

static bool d3d11Preprocess(const std::string& Src, const std::string& szName, const D3D_SHADER_MACRO* pMacros, std::string& Out, std::string* pErrors)
{
	if (_D3DPreprocess == NULL) {
		if (pErrors) *pErrors += "D3DPreprocess() is not available\n";
		return false;
	}

	ID3DBlob* pText = NULL;
	ID3DBlob* pErrorBlob = NULL;

	HRESULT hr = _D3DPreprocess(Src.c_str(), Src.size(), szName.c_str(), pMacros, NULL, &pText, &pErrorBlob);

	if (SUCCEEDED(hr) && pText) {
		Out.assign((const char*)pText->GetBufferPointer(), pText->GetBufferSize());
		while (!Out.empty() && Out[Out.size() - 1] == 0) Out.erase(Out.size() - 1);
	} else if (pErrors && pErrorBlob && pErrorBlob->GetBufferSize()) {
		*pErrors += std::string((const char*)pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize() - 1) + "\n";
	}

	D3D11_RELEASE(pText);
	D3D11_RELEASE(pErrorBlob);

	return SUCCEEDED(hr);
}

static int d3d11WrapIndex(D3D11_TEXTURE_ADDRESS_MODE nMode)
{
	switch (nMode) {
		case D3D11_TEXTURE_ADDRESS_WRAP:   return D3D11_WRAP_REPEAT;
		case D3D11_TEXTURE_ADDRESS_MIRROR: return D3D11_WRAP_MIRROR;
		case D3D11_TEXTURE_ADDRESS_BORDER: return D3D11_WRAP_BORDER;
		default:                           return D3D11_WRAP_EDGE;
	}
}

// Work out what goes in every constant buffer / texture / sampler slot of a compiled shader stage
static int d3d11ReflectBindings(ID3DBlob* pCode, int nPass, d3d11StageBindings* pBind, const char* pFXSource)
{
	ID3D11ShaderReflection* pRefl = NULL;
	D3D11_SHADER_DESC sd;

	if (FAILED(d3d11Reflect(pCode, &pRefl))) {
		return 1;
	}

	D3D11ShaderBindContext Ctx;
	Ctx.nPass = nPass;
	Ctx.pAliases = &pShader->Aliases;
	Ctx.pLUTs = &pShader->Preset.LUTs;
	Ctx.pParams = &pShader->Params;

	memset(&sd, 0, sizeof(sd));
	pRefl->GetDesc(&sd);

	// Constant buffers
	for (UINT i = 0; i < sd.ConstantBuffers; i++) {
		ID3D11ShaderReflectionConstantBuffer* pCB = pRefl->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC bd;
		D3D11_SHADER_INPUT_BIND_DESC bind;

		memset(&bd, 0, sizeof(bd));
		if (FAILED(pCB->GetDesc(&bd)) || bd.Type != D3D_CT_CBUFFER || bd.Size == 0) {
			continue;
		}
		memset(&bind, 0, sizeof(bind));
		if (FAILED(pRefl->GetResourceBindingDescByName(bd.Name, &bind))) {
			continue;
		}

		d3d11CBuffer CB;
		CB.nSlot = bind.BindPoint;
		CB.nSize = (bd.Size + 15) & ~15;
		CB.Data.assign(CB.nSize, 0);
		CB.pBuffer = d3d11CreateDynamicBuffer(CB.nSize, D3D11_BIND_CONSTANT_BUFFER);
		if (CB.pBuffer == NULL) {
			pRefl->Release();
			return 1;
		}

		for (UINT v = 0; v < bd.Variables; v++) {
			ID3D11ShaderReflectionVariable* pVar = pCB->GetVariableByIndex(v);
			D3D11_SHADER_VARIABLE_DESC vd;
			D3D11_SHADER_TYPE_DESC td;

			memset(&vd, 0, sizeof(vd));
			memset(&td, 0, sizeof(td));
			if (FAILED(pVar->GetDesc(&vd)) || vd.StartOffset + vd.Size > CB.nSize) {
				continue;
			}
			pVar->GetType()->GetDesc(&td);

			if (vd.DefaultValue) {
				memcpy(&CB.Data[vd.StartOffset], vd.DefaultValue, vd.Size);
			} else if ((td.Class == D3D_SVC_MATRIX_ROWS || td.Class == D3D_SVC_MATRIX_COLUMNS) && td.Rows == 4 && td.Columns == 4 && vd.Size >= 64) {
				// modelViewProj & co: we draw in clip space
				float* m = (float*)&CB.Data[vd.StartOffset];
				memset(m, 0, 64);
				m[0] = m[5] = m[10] = m[15] = 1.0f;
				continue;
			}

			d3d11UniformVar Var;
			D3D11ShaderResolveUniform(vd.Name, Ctx, &Var.nUniform, &Var.nSource, &Var.nIndex);
			if (Var.nUniform == D3D11_UNI_NONE) {
				continue;
			}
			switch (td.Type) {
				case D3D_SVT_INT:  Var.nType = D3D11_VAR_INT;   break;
				case D3D_SVT_UINT: Var.nType = D3D11_VAR_UINT;  break;
				case D3D_SVT_BOOL: Var.nType = D3D11_VAR_BOOL;  break;
				default:           Var.nType = D3D11_VAR_FLOAT; break;
			}
			Var.nOffset = vd.StartOffset;
			Var.nSize = (vd.Size > 16) ? 16 : vd.Size;
			CB.Vars.push_back(Var);
		}

		pBind->CBuffers.push_back(CB);
	}

	// Textures and samplers
	for (UINT i = 0; i < sd.BoundResources; i++) {
		D3D11_SHADER_INPUT_BIND_DESC bind;

		memset(&bind, 0, sizeof(bind));
		if (FAILED(pRefl->GetResourceBindingDesc(i, &bind))) {
			continue;
		}

		UINT nCount = bind.BindCount ? bind.BindCount : 1;

		if (bind.Type == D3D_SIT_TEXTURE) {
			d3d11TexBinding Tex;
			D3D11ShaderResolveTexture(bind.Name, Ctx, &Tex.nSource, &Tex.nIndex);
			for (UINT n = 0; n < nCount; n++) {
				Tex.nSlot = bind.BindPoint + n;
				pBind->Textures.push_back(Tex);
			}
		} else if (bind.Type == D3D_SIT_SAMPLER) {
			d3d11SamplerBinding Smp;
			Smp.nFilter = D3D11ShaderResolveSampler(bind.Name, Ctx);
			Smp.nWrap = pShader->Passes[nPass].Desc.nWrap;

			// a sampler for a lookup texture uses that texture's wrap mode
			std::string szName = bind.Name;
			if (szName.compare(0, 10, "fbneo_smp_") == 0) szName = szName.substr(10);
			for (size_t l = 0; l < pShader->Preset.LUTs.size(); l++) {
				if (pShader->Preset.LUTs[l].szName == szName) Smp.nWrap = pShader->Preset.LUTs[l].nWrap;
			}

			// DX9 .fx sampler_state blocks
			if (pFXSource) {
				int nLinear = -1;
				D3D11_TEXTURE_ADDRESS_MODE nAddress = D3D11_TEXTURE_ADDRESS_CLAMP;
				ParseSamplerState(pFXSource, bind.Name, &nLinear, &nAddress);
				if (nLinear >= 0) Smp.nFilter = nLinear;
				Smp.nWrap = d3d11WrapIndex(nAddress);
			}

			for (UINT n = 0; n < nCount; n++) {
				Smp.nSlot = bind.BindPoint + n;
				pBind->Samplers.push_back(Smp);
			}
		}
	}

	pRefl->Release();

	return 0;
}

static void d3d11ReleaseBindings(d3d11StageBindings& Bind)
{
	for (size_t i = 0; i < Bind.CBuffers.size(); i++) {
		D3D11_RELEASE(Bind.CBuffers[i].pBuffer);
	}
	Bind.CBuffers.clear();
	Bind.Textures.clear();
	Bind.Samplers.clear();
}

static void d3d11ReleasePassTarget(d3d11ShaderPass& Pass)
{
	D3D11_RELEASE(Pass.pSRV);
	D3D11_RELEASE(Pass.pRTV);
	D3D11_RELEASE(Pass.pTexture);
	Pass.nWidth = Pass.nHeight = 0;
	Pass.nTexWidth = Pass.nTexHeight = 0;
}

static void d3d11ReleaseShader()
{
	if (pShader == NULL) {
		return;
	}

	if (pContext) {
		ID3D11ShaderResourceView* NullSRV[16] = { NULL, };
		pContext->PSSetShaderResources(0, 16, NullSRV);
		pContext->VSSetShaderResources(0, 16, NullSRV);
	}

	for (size_t i = 0; i < pShader->Passes.size(); i++) {
		d3d11ShaderPass& Pass = pShader->Passes[i];
		D3D11_RELEASE(Pass.pVS);
		D3D11_RELEASE(Pass.pPS);
		D3D11_RELEASE(Pass.pLayout);
		d3d11ReleaseBindings(Pass.VSBind);
		d3d11ReleaseBindings(Pass.PSBind);
		d3d11ReleasePassTarget(Pass);
	}
	for (size_t i = 0; i < pShader->LUTs.size(); i++) {
		D3D11_RELEASE(pShader->LUTs[i].pSRV);
		D3D11_RELEASE(pShader->LUTs[i].pTexture);
	}

	delete pShader;
	pShader = NULL;
}

// The legacy DX9 .fx route (single pass, backwards compatibility mode)
static int d3d11CompileFX(const std::string& Source, const std::string& szName, ID3DBlob** ppVS, ID3DBlob** ppPS, std::string& szErrors)
{
	char szVSEntry[64], szPSEntry[64];
	ParseTechniqueEntryPoints(Source.c_str(), szVSEntry, szPSEntry, sizeof(szVSEntry));

	std::string Src = Source;
	StripEffectSyntax(&Src[0]);

	D3D_SHADER_MACRO Macros[] = { { "FBNEO_D3D11", "1" }, { NULL, NULL } };

	if (szPSEntry[0]) {
		d3d11CompileEx(Src, szName, szPSEntry, true, Macros, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, ppPS, &szErrors);
	} else {
		static const char* Fallback[] = { "main_fragment", "ps_main", "main_ps", "PS", "PixelShaderFunction", "main" };
		for (int i = 0; i < (int)(sizeof(Fallback) / sizeof(Fallback[0])) && *ppPS == NULL; i++) {
			if (FindToken(Src.c_str(), Src.c_str(), Fallback[i])) {
				d3d11CompileEx(Src, szName, Fallback[i], true, Macros, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, ppPS, &szErrors);
			}
		}
	}

	if (*ppPS == NULL) {
		return 1;
	}

	if (szVSEntry[0]) {
		d3d11CompileEx(Src, szName, szVSEntry, false, Macros, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, ppVS, &szErrors);
	} else if (szPSEntry[0] == 0) {
		static const char* Fallback[] = { "main_vertex", "vs_main", "main_vs", "VS", "VertexShaderFunction" };
		for (int i = 0; i < (int)(sizeof(Fallback) / sizeof(Fallback[0])) && *ppVS == NULL; i++) {
			if (FindToken(Src.c_str(), Src.c_str(), Fallback[i])) {
				d3d11CompileEx(Src, szName, Fallback[i], false, Macros, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, ppVS, &szErrors);
			}
		}
	}

	return 0;
}

// Direct3D 11 links the vertex shader outputs to the pixel shader inputs by register: check that every
// pixel shader input is a vertex shader output with the same semantic, in the same register
static bool d3d11SignaturesMatch(ID3DBlob* pVSCode, ID3DBlob* pPSCode)
{
	ID3D11ShaderReflection* pVS = NULL;
	ID3D11ShaderReflection* pPS = NULL;
	bool bMatch = true;

	if (FAILED(d3d11Reflect(pVSCode, &pVS)) || FAILED(d3d11Reflect(pPSCode, &pPS))) {
		D3D11_RELEASE(pVS);
		D3D11_RELEASE(pPS);
		return true;								// can't tell
	}

	D3D11_SHADER_DESC vd, pd;
	memset(&vd, 0, sizeof(vd));
	memset(&pd, 0, sizeof(pd));
	pVS->GetDesc(&vd);
	pPS->GetDesc(&pd);

	for (UINT i = 0; i < pd.InputParameters && bMatch; i++) {
		D3D11_SIGNATURE_PARAMETER_DESC in;
		memset(&in, 0, sizeof(in));
		if (FAILED(pPS->GetInputParameterDesc(i, &in))) continue;
		if (in.SystemValueType != D3D_NAME_UNDEFINED && in.SystemValueType != D3D_NAME_POSITION) continue;	// generated by the rasterizer

		bool bFound = false;
		for (UINT o = 0; o < vd.OutputParameters && !bFound; o++) {
			D3D11_SIGNATURE_PARAMETER_DESC out;
			memset(&out, 0, sizeof(out));
			if (FAILED(pVS->GetOutputParameterDesc(o, &out))) continue;
			if (_stricmp(out.SemanticName, in.SemanticName) == 0 && out.SemanticIndex == in.SemanticIndex && out.Register == in.Register) {
				bFound = true;
			}
		}
		if (!bFound) {
			bMatch = false;
		}
	}

	pVS->Release();
	pPS->Release();

	return bMatch;
}

static int d3d11CompilePass(int nPass, std::string& szErrors)
{
	d3d11ShaderPass& Pass = pShader->Passes[nPass];
	const std::string& szPath = Pass.Desc.szPath;
	std::string Source, szError;

	if (!D3D11ShaderInlineIncludes(szPath, d3d11ReadFile, Source, &szError)) {
		szErrors += szError + "\n";
		return 1;
	}

	D3D11ShaderFixMacroNames(Source);						// D3DPreprocess() workaround
	D3D11ShaderExtractParams(Source, pShader->Params);

	ID3DBlob* pVSCode = NULL;
	ID3DBlob* pPSCode = NULL;
	std::string szExt = D3D11ShaderExtension(szPath);
	std::string szTried;
	bool bFX = false;
	const char* szLevel9 = (nFeatureLevel < D3D_FEATURE_LEVEL_10_0) ? "1" : NULL;

	if (szExt != "fx") {
		bool bHasVS = Source.find("main_vertex") != std::string::npos;

		// 1. the shader's own HLSL 4 path
		if (Source.find("HLSL_4") != std::string::npos) {
			D3D_SHADER_MACRO Macros[] = { { "HLSL_4", "1" }, { "PARAMETER_UNIFORM", "1" }, { "FBNEO_D3D11", "1" }, { NULL, NULL } };
			std::string szErr;
			// backwards compatibility mode: some of these still use the D3D9 POSITION / COLOR semantics
			if (SUCCEEDED(d3d11CompileEx(Source, szPath, "main_fragment", true, Macros, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, &pPSCode, &szErr)) &&
				(!bHasVS || SUCCEEDED(d3d11CompileEx(Source, szPath, "main_vertex", false, Macros, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, &pVSCode, &szErr)))) {
				Pass.szMode = "HLSL_4";
				if (pVSCode && !d3d11SignaturesMatch(pVSCode, pPSCode)) {
					// the shader's own D3D11 code doesn't link up: use its Cg code instead
					D3D11_RELEASE(pPSCode);
					D3D11_RELEASE(pVSCode);
					szTried += "[HLSL_4] the vertex and pixel shader signatures don't match\n";
				}
			} else {
				D3D11_RELEASE(pPSCode);
				D3D11_RELEASE(pVSCode);
				szTried += "[HLSL_4] " + szErr;
			}
		}

		// 2. translate the Cg code
		if (pPSCode == NULL && Source.find("main_fragment") != std::string::npos) {
			D3D_SHADER_MACRO PPMacros[] = { { "PARAMETER_UNIFORM", "1" }, { "FBNEO_D3D11", "1" }, { NULL, NULL } };
			std::string Preprocessed, szErr;

			if (d3d11Preprocess(Source, szPath, PPMacros, Preprocessed, &szErr)) {
				D3D11CgTranslation Translation;
				if (D3D11TranslateCg(Preprocessed, &Translation, &szErr)) {
					D3D_SHADER_MACRO Macros[] = { { "FBNEO_D3D11", "1" }, { szLevel9 ? "FBNEO_LEVEL9" : NULL, szLevel9 }, { NULL, NULL } };
					if (SUCCEEDED(d3d11CompileEx(Translation.szHLSL, szPath, "main_fragment", true, Macros, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, &pPSCode, &szErr)) &&
						(!Translation.bHasVertex || SUCCEEDED(d3d11CompileEx(Translation.szHLSL, szPath, "main_vertex", false, Macros, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, &pVSCode, &szErr)))) {
						Pass.szMode = "Cg";
					} else {
						D3D11_RELEASE(pPSCode);
						D3D11_RELEASE(pVSCode);

						// Keep the translated source for reference: config/shaders/<name>.translated.hlsl
						CreateDirectoryW(L"config", NULL);
						CreateDirectoryW(L"config/shaders", NULL);
						std::wstring szDump = L"config/shaders/" + d3d11Widen(d3d11BaseName(szPath)) + L".translated.hlsl";
						FILE* fp = _wfopen(szDump.c_str(), L"wb");
						if (fp) {
							fwrite(Translation.szHLSL.data(), 1, Translation.szHLSL.size(), fp);
							fclose(fp);
							szErr += "(translated source saved as " + d3d11NarrowW(szDump.c_str()) + ")\n";
						}
					}
				}
			}
			if (pPSCode == NULL) {
				szTried += "[Cg] " + szErr;
			}
		}
	}

	// 3. a DX9 effect / plain shader (not for Cg files, which need the conventions above)
	if (pPSCode == NULL && szExt != "cg") {
		std::string szErr;
		if (d3d11CompileFX(Source, szPath, &pVSCode, &pPSCode, szErr) == 0) {
			Pass.szMode = "FX";
			bFX = true;
			if (pVSCode && !d3d11SignaturesMatch(pVSCode, pPSCode)) {
				D3D11_RELEASE(pVSCode);					// use a generated vertex shader that matches the pixel shader
			}
		} else {
			D3D11_RELEASE(pPSCode);
			D3D11_RELEASE(pVSCode);
			if (szExt == "fx" || szTried.empty()) szTried += "[FX] " + szErr;
		}
	}

	if (pPSCode == NULL) {
		szErrors += d3d11BaseName(szPath) + ":\n" + szTried;
		return 1;
	}

	int nRet = 1;

	do {
		if (FAILED(pDevice->CreatePixelShader(pPSCode->GetBufferPointer(), pPSCode->GetBufferSize(), NULL, &Pass.pPS))) {
			szErrors += d3d11BaseName(szPath) + ": CreatePixelShader failed\n";
			break;
		}
		if (d3d11ReflectBindings(pPSCode, nPass, &Pass.PSBind, bFX ? Source.c_str() : NULL)) {
			szErrors += d3d11BaseName(szPath) + ": couldn't reflect the pixel shader\n";
			break;
		}

		if (pVSCode) {
			HRESULT hr = pDevice->CreateVertexShader(pVSCode->GetBufferPointer(), pVSCode->GetBufferSize(), NULL, &Pass.pVS);
			if (FAILED(hr)) {
				char szHR[64];
				sprintf(szHR, " (0x%08X)", (unsigned int)hr);
				szErrors += d3d11BaseName(szPath) + ": couldn't create the vertex shader" + szHR + "\n";
				break;
			}
			hr = CreateLayoutForVertexShader(pVSCode, &Pass.pLayout);
			if (FAILED(hr)) {
				char szHR[64];
				sprintf(szHR, " (0x%08X)", (unsigned int)hr);
				szErrors += d3d11BaseName(szPath) + ": couldn't create the vertex input layout" + szHR + "\n";
				break;
			}
			if (d3d11ReflectBindings(pVSCode, nPass, &Pass.VSBind, bFX ? Source.c_str() : NULL)) {
				szErrors += d3d11BaseName(szPath) + ": couldn't reflect the vertex shader\n";
				break;
			}
		} else {
			// no vertex shader in the file: generate one that feeds the pixel shader
			char szVS[4096];
			ID3DBlob* pGenerated = NULL;
			if (GenerateVertexShader(pPSCode, szVS, sizeof(szVS)) ||
				FAILED(d3d11CompileEx(szVS, "generated", "main", false, NULL, 0, &pGenerated, &szErrors))) {
				D3D11_RELEASE(pGenerated);
				break;
			}
			HRESULT hr = pDevice->CreateVertexShader(pGenerated->GetBufferPointer(), pGenerated->GetBufferSize(), NULL, &Pass.pVS);
			if (SUCCEEDED(hr)) {
				hr = pDevice->CreateInputLayout(BasicLayout, sizeof(BasicLayout) / sizeof(BasicLayout[0]), pGenerated->GetBufferPointer(), pGenerated->GetBufferSize(), &Pass.pLayout);
			}
			pGenerated->Release();
			if (FAILED(hr)) {
				break;
			}
		}

		nRet = 0;
	} while (0);

	D3D11_RELEASE(pPSCode);
	D3D11_RELEASE(pVSCode);

	return nRet;
}

// "config/shaders/<name>.ini" holds the user's parameter values for a shader
static std::wstring d3d11ParamFileName(const std::string& szShaderPath)
{
	return L"config/shaders/" + d3d11Widen(d3d11BaseName(szShaderPath)) + L".ini";
}

static void d3d11LoadParamValues()
{
	for (size_t i = 0; i < pShader->Params.size(); i++) {
		D3D11ShaderParam& p = pShader->Params[i];
		float v;
		if (D3D11ShaderPresetValue(pShader->Preset, p.szName, &v)) {
			p.fDefault = v;					// the preset's value is the default for this preset
		}
		p.fValue = p.fDefault;
	}

	FILE* fp = _wfopen(d3d11ParamFileName(pShader->szPath).c_str(), L"rt");
	if (fp == NULL) {
		return;
	}

	char szLine[512];
	while (fgets(szLine, sizeof(szLine), fp)) {
		char* pEq = strchr(szLine, '=');
		if (pEq == NULL) continue;
		*pEq = 0;
		std::string szName = szLine, szValue = pEq + 1;
		while (!szName.empty() && isspace((unsigned char)szName[szName.size() - 1])) szName.erase(szName.size() - 1);
		while (!szName.empty() && isspace((unsigned char)szName[0])) szName.erase(0, 1);

		for (size_t i = 0; i < pShader->Params.size(); i++) {
			if (pShader->Params[i].szName == szName) {
				D3D11ShaderPreset Tmp;
				Tmp.Values.push_back(std::make_pair(szName, szValue));
				float v;
				if (D3D11ShaderPresetValue(Tmp, szName, &v)) {			// locale independent parsing
					D3D11ShaderParam& p = pShader->Params[i];
					p.fValue = (v < p.fMin) ? p.fMin : (v > p.fMax) ? p.fMax : v;
				}
			}
		}
	}
	fclose(fp);
}

static int d3d11SaveParamValues()
{
	if (pShader == NULL || pShader->Params.empty()) {
		return 1;
	}

	CreateDirectoryW(L"config", NULL);
	CreateDirectoryW(L"config/shaders", NULL);

	FILE* fp = _wfopen(d3d11ParamFileName(pShader->szPath).c_str(), L"wt");
	if (fp == NULL) {
		return 1;
	}

	fprintf(fp, "# Parameters for %s\n", pShader->szPath.c_str());
	for (size_t i = 0; i < pShader->Params.size(); i++) {
		const D3D11ShaderParam& p = pShader->Params[i];
		// locale independent output
		char szValue[64];
		_snprintf(szValue, sizeof(szValue), "%d.%06d", (int)p.fValue, (int)(fabs(p.fValue - (int)p.fValue) * 1000000.0 + 0.5));
		if (p.fValue < 0.0f && (int)p.fValue == 0) {
			fprintf(fp, "%s = -%s\n", p.szName.c_str(), szValue);
		} else {
			fprintf(fp, "%s = %s\n", p.szName.c_str(), szValue);
		}
	}
	fclose(fp);

	return 0;
}

// FBAPopupAddText() formats into a 1024 character buffer, so long text is added in pieces
static void d3d11PopupText(const std::string& szText)
{
	size_t nPos = 0;
	while (nPos < szText.size()) {
		size_t nLen = szText.size() - nPos;
		if (nLen > 700) {
			nLen = 700;
			while (nLen > 1 && (szText[nPos + nLen] & 0xC0) == 0x80) nLen--;	// don't split a UTF-8 sequence
		}
		std::string szPiece = szText.substr(nPos, nLen);
#if defined (UNICODE) || defined (_UNICODE)
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("%s"), d3d11Widen(szPiece).c_str());
#else
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("%s"), szPiece.c_str());
#endif
		nPos += nLen;
	}
}

static void d3d11ShaderError(const std::string& szPath, const std::string& szErrors)
{
	dprintf(_T("  * Error: Couldn't load shader:\n"));
	d3d11LogText(szPath.c_str(), szPath.size(), 1);
	d3d11LogText(szErrors.c_str(), szErrors.size(), 200);

	// For the popup: only the error lines (not the warnings), with "C:\long\path\file.cg(12,3)" shortened to "file.cg(12,3)"
	std::string szShort;
	int nLines = 0;
	size_t nPos = 0;
	while (nPos < szErrors.size() && nLines < 12) {
		size_t nEnd = szErrors.find('\n', nPos);
		if (nEnd == std::string::npos) nEnd = szErrors.size();
		std::string szLine = szErrors.substr(nPos, nEnd - nPos);
		nPos = nEnd + 1;

		if (szLine.find("warning") != std::string::npos || szLine.find(": W") != std::string::npos) {
			continue;
		}
		size_t nParen = szLine.find('(');
		size_t nSlash = szLine.find_last_of("/\\", nParen);
		if (nParen != std::string::npos && nSlash != std::string::npos) {
			szLine = szLine.substr(nSlash + 1);
		}
		if (szLine.size() > 200) {
			szLine = szLine.substr(0, 200) + "...";
		}
		if (!szLine.empty()) {
			szShort += szLine + "\n";
			nLines++;
		}
	}
	if (nPos < szErrors.size()) {
		szShort += "...\n(the full messages are in the debug log)\n";
	}

	d3d11PopupText("Couldn't load shader \"" + d3d11BaseName(szPath) + "\".\n\n" + szShort);
	FBAPopupDisplay(PUF_TYPE_ERROR);
}

static int d3d11CreateTextures();

// Load the preset in szVidD3D11Shader
static int d3d11LoadShader()
{
	d3d11ReleaseShader();

	if (szVidD3D11Shader[0] == 0) {
		return 0;
	}

	std::string szPath = d3d11Narrow(szVidD3D11Shader);
	std::string szErrors;

	pShader = new d3d11Shader;
	pShader->szPath = szPath;
	pShader->nHistory = 0;

	if (!D3D11ShaderLoadPreset(szPath, d3d11ReadFile, &pShader->Preset, &szErrors)) {
		d3d11ReleaseShader();
		d3d11ShaderError(szPath, szErrors);
		return 1;
	}

	size_t nPasses = pShader->Preset.Passes.size();
	pShader->Passes.resize(nPasses);
	for (size_t i = 0; i < nPasses; i++) {
		d3d11ShaderPass& Pass = pShader->Passes[i];
		Pass.Desc = pShader->Preset.Passes[i];
		Pass.pVS = NULL;
		Pass.pPS = NULL;
		Pass.pLayout = NULL;
		Pass.pTexture = NULL;
		Pass.pRTV = NULL;
		Pass.pSRV = NULL;
		Pass.nWidth = Pass.nHeight = 0;
		Pass.nTexWidth = Pass.nTexHeight = 0;
		Pass.bToScreen = (i == nPasses - 1) && Pass.Desc.nScaleTypeX == D3D11_SCALE_UNSET && Pass.Desc.nScaleTypeY == D3D11_SCALE_UNSET;
		Pass.szMode = "";
		pShader->Aliases.push_back(Pass.Desc.szAlias);
	}

	bool bFailed = false;

	for (size_t i = 0; i < pShader->Preset.LUTs.size() && !bFailed; i++) {
		d3d11LUT LUT;
		if (d3d11LoadLUT(pShader->Preset.LUTs[i], &LUT, szErrors)) {
			bFailed = true;
		}
		pShader->LUTs.push_back(LUT);
	}

	// parameters are collected while compiling, but the bindings need the full list, so collect them first
	for (size_t i = 0; i < nPasses && !bFailed; i++) {
		std::string Source, szError;
		if (D3D11ShaderInlineIncludes(pShader->Passes[i].Desc.szPath, d3d11ReadFile, Source, &szError)) {
			D3D11ShaderExtractParams(Source, pShader->Params);
		}
	}

	for (size_t i = 0; i < nPasses && !bFailed; i++) {
		if (d3d11CompilePass((int)i, szErrors)) {
			bFailed = true;
		}
	}

	if (bFailed) {
		d3d11ReleaseShader();
		d3d11ShaderError(szPath, szErrors);
		return 1;
	}

	// How many previous frames are needed?
	for (size_t i = 0; i < nPasses; i++) {
		d3d11StageBindings* Binds[2] = { &pShader->Passes[i].VSBind, &pShader->Passes[i].PSBind };
		for (int b = 0; b < 2; b++) {
			for (size_t t = 0; t < Binds[b]->Textures.size(); t++) {
				if (Binds[b]->Textures[t].nSource == D3D11_SRC_PREV && Binds[b]->Textures[t].nIndex > pShader->nHistory) {
					pShader->nHistory = Binds[b]->Textures[t].nIndex;
				}
			}
		}
	}

	d3d11LoadParamValues();

	for (size_t i = 0; i < nPasses; i++) {
		bprintf(0, _T("  * Shader pass %i: %hs (%hs)\n"), (int)i, d3d11BaseName(pShader->Passes[i].Desc.szPath).c_str(), pShader->Passes[i].szMode);
	}

	// make sure there are enough frame textures for the history
	if (nGameTextures < pShader->nHistory + 1) {
		d3d11CreateTextures();
		if (pVidImage) {
			d3d11UploadImage();
		}
	}

	return 0;
}

// Fill in the shader constants for a pass
static void d3d11ShaderSource(int nSource, int nIndex, int nPass, ID3D11ShaderResourceView** ppSRV, float* pVideoSize, float* pTextureSize);

static void d3d11UpdatePassConstants(d3d11StageBindings& Bind, int nPass, float fOutWidth, float fOutHeight)
{
	d3d11ShaderPass& Pass = pShader->Passes[nPass];
	UINT32 nFrame = nCurrentFrame;
	if (Pass.Desc.nFrameCountMod > 0) {
		nFrame %= (UINT32)Pass.Desc.nFrameCountMod;
	}

	for (size_t c = 0; c < Bind.CBuffers.size(); c++) {
		d3d11CBuffer& CB = Bind.CBuffers[c];

		for (size_t v = 0; v < CB.Vars.size(); v++) {
			const d3d11UniformVar& Var = CB.Vars[v];
			float f[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			switch (Var.nUniform) {
				case D3D11_UNI_VIDEO_SIZE:
				case D3D11_UNI_TEXTURE_SIZE: {
					ID3D11ShaderResourceView* pSRV;
					float Video[2], Texture[2];
					d3d11ShaderSource(Var.nSource, Var.nIndex, nPass, &pSRV, Video, Texture);
					f[0] = (Var.nUniform == D3D11_UNI_VIDEO_SIZE) ? Video[0] : Texture[0];
					f[1] = (Var.nUniform == D3D11_UNI_VIDEO_SIZE) ? Video[1] : Texture[1];
					break;
				}
				case D3D11_UNI_OUTPUT_SIZE:
					f[0] = fOutWidth;
					f[1] = fOutHeight;
					break;
				case D3D11_UNI_FRAME_COUNT:
					f[0] = (float)nFrame;
					break;
				case D3D11_UNI_FRAME_DIRECTION:
					f[0] = 1.0f;
					break;
				case D3D11_UNI_FRAME_ROTATION:
					f[0] = 0.0f;
					break;
				case D3D11_UNI_VIDEO_TIME:
					f[0] = (float)nFrame;
					f[1] = (float)nFrame / 60.0f;
					break;
				case D3D11_UNI_PARAM:
					if (Var.nIndex < (int)pShader->Params.size()) {
						f[0] = pShader->Params[Var.nIndex].fValue;
					}
					break;
			}

			if (Var.nType != D3D11_VAR_FLOAT) {
				// int / uint / bool variables (e.g. "uniform int Ywidth;") need the value, not the bits of a float
				UINT32 n[4];
				for (int k = 0; k < 4; k++) {
					switch (Var.nType) {
						case D3D11_VAR_INT:  n[k] = (UINT32)(INT32)floorf(f[k] + 0.5f); break;
						case D3D11_VAR_UINT: n[k] = (f[k] > 0.0f) ? (UINT32)(f[k] + 0.5f) : 0; break;
						default:             n[k] = (f[k] != 0.0f) ? 1 : 0; break;
					}
				}
				memcpy(&CB.Data[Var.nOffset], n, Var.nSize);
			} else {
				memcpy(&CB.Data[Var.nOffset], f, Var.nSize);
			}
		}

		d3d11UpdateBuffer(CB.pBuffer, &CB.Data[0], CB.nSize);
	}
}

static void d3d11BindPassResources(d3d11StageBindings& Bind, int nPass, bool bPixelShader)
{
	for (size_t c = 0; c < Bind.CBuffers.size(); c++) {
		if (bPixelShader) pContext->PSSetConstantBuffers(Bind.CBuffers[c].nSlot, 1, &Bind.CBuffers[c].pBuffer);
		else              pContext->VSSetConstantBuffers(Bind.CBuffers[c].nSlot, 1, &Bind.CBuffers[c].pBuffer);
	}

	for (size_t t = 0; t < Bind.Textures.size(); t++) {
		ID3D11ShaderResourceView* pSRV = NULL;
		float Video[2], Texture[2];
		d3d11ShaderSource(Bind.Textures[t].nSource, Bind.Textures[t].nIndex, nPass, &pSRV, Video, Texture);
		if (bPixelShader) pContext->PSSetShaderResources(Bind.Textures[t].nSlot, 1, &pSRV);
		else              pContext->VSSetShaderResources(Bind.Textures[t].nSlot, 1, &pSRV);
	}

	for (size_t s = 0; s < Bind.Samplers.size(); s++) {
		int nFilter = Bind.Samplers[s].nFilter;
		if (nFilter < 0) {
			nFilter = pShader->Passes[nPass].Desc.nFilter;
			if (nFilter < 0) nFilter = bVidDX9Bilinear ? 1 : 0;
		}
		int nWrap = Bind.Samplers[s].nWrap & 3;
		ID3D11SamplerState* pSampler = pSamplerCache[nFilter ? 1 : 0][nWrap];
		if (bPixelShader) pContext->PSSetSamplers(Bind.Samplers[s].nSlot, 1, &pSampler);
		else              pContext->VSSetSamplers(Bind.Samplers[s].nSlot, 1, &pSampler);
	}
}

static int d3d11EnsurePassTarget(d3d11ShaderPass& Pass, int nWidth, int nHeight, int nTexWidth, int nTexHeight)
{
	if (nTexWidth < nWidth) nTexWidth = nWidth;
	if (nTexHeight < nHeight) nTexHeight = nHeight;

	if (Pass.pTexture && Pass.nTexWidth == nTexWidth && Pass.nTexHeight == nTexHeight) {
		Pass.nWidth = nWidth;
		Pass.nHeight = nHeight;
		return 0;
	}

	d3d11ReleasePassTarget(Pass);

	D3D11_TEXTURE2D_DESC td;
	memset(&td, 0, sizeof(td));
	td.Width = nTexWidth;
	td.Height = nTexHeight;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	if (Pass.Desc.bFloatFramebuffer) {
		td.Format = (nFeatureLevel >= D3D_FEATURE_LEVEL_10_0) ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT;
	} else if (Pass.Desc.bSRGBFramebuffer) {
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	}
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(pDevice->CreateTexture2D(&td, NULL, &Pass.pTexture))) {
		// fall back to a plain 8-bit target
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		if (FAILED(pDevice->CreateTexture2D(&td, NULL, &Pass.pTexture))) {
			return 1;
		}
	}
	if (FAILED(pDevice->CreateRenderTargetView(Pass.pTexture, NULL, &Pass.pRTV)) ||
		FAILED(pDevice->CreateShaderResourceView(Pass.pTexture, NULL, &Pass.pSRV))) {
		d3d11ReleasePassTarget(Pass);
		return 1;
	}

	float Black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	pContext->ClearRenderTargetView(Pass.pRTV, Black);

	Pass.nWidth = nWidth;
	Pass.nHeight = nHeight;
	Pass.nTexWidth = nTexWidth;
	Pass.nTexHeight = nTexHeight;

	return 0;
}

// Where a shader input comes from, and its sizes
static void d3d11ShaderSource(int nSource, int nIndex, int nPass, ID3D11ShaderResourceView** ppSRV, float* pVideoSize, float* pTextureSize)
{
	if (nSource == D3D11_SRC_INPUT) {
		if (nPass <= 0) {
			nSource = D3D11_SRC_ORIG;
		} else {
			nSource = D3D11_SRC_PASS;
			nIndex = nPass - 1;
		}
	}

	if (nSource == D3D11_SRC_PASS) {
		if (nIndex < 0 || nIndex >= (int)pShader->Passes.size() || pShader->Passes[nIndex].pSRV == NULL || nIndex == nPass) {
			nSource = D3D11_SRC_ORIG;		// a pass that hasn't been rendered (yet)
		} else {
			const d3d11ShaderPass& p = pShader->Passes[nIndex];
			*ppSRV = p.pSRV;
			pVideoSize[0] = (float)p.nWidth;
			pVideoSize[1] = (float)p.nHeight;
			pTextureSize[0] = (float)p.nTexWidth;
			pTextureSize[1] = (float)p.nTexHeight;
			return;
		}
	}

	if (nSource == D3D11_SRC_LUT && nIndex >= 0 && nIndex < (int)pShader->LUTs.size()) {
		const d3d11LUT& l = pShader->LUTs[nIndex];
		*ppSRV = l.pSRV;
		pVideoSize[0] = pTextureSize[0] = (float)l.nWidth;
		pVideoSize[1] = pTextureSize[1] = (float)l.nHeight;
		return;
	}

	// the original frame (or a previous one)
	int nTexture = nCurrentTexture;
	if (nSource == D3D11_SRC_PREV && nGameTextures > 1) {
		int nBack = nIndex;
		if (nBack > nFramesUploaded - 1) nBack = nFramesUploaded - 1;
		if (nBack > nGameTextures - 1) nBack = nGameTextures - 1;
		if (nBack < 0) nBack = 0;
		nTexture = (nCurrentTexture - nBack + nGameTextures) % nGameTextures;
	}

	*ppSRV = pGameSRV[nTexture];
	pVideoSize[0] = (float)(nGameImageWidth * (nPreScaleEffect ? nPreScaleZoom : 1));
	pVideoSize[1] = (float)(nGameImageHeight * (nPreScaleEffect ? nPreScaleZoom : 1));
	pTextureSize[0] = (float)nTextureWidth;
	pTextureSize[1] = (float)nTextureHeight;
}

static int d3d11PassSize(int nType, float fScale, int nInput, int nViewport)
{
	float f;
	switch (nType) {
		case D3D11_SCALE_VIEWPORT: f = nViewport * fScale; break;
		case D3D11_SCALE_ABSOLUTE: f = fScale; break;
		case D3D11_SCALE_SOURCE:   f = nInput * fScale; break;
		default:                   f = (float)nInput; break;
	}

	int n = (int)(f + 0.5f);
	int nMax = (nFeatureLevel >= D3D_FEATURE_LEVEL_11_0) ? 16384 : (nFeatureLevel >= D3D_FEATURE_LEVEL_10_0) ? 8192 : 4096;
	return (n < 1) ? 1 : (n > nMax) ? nMax : n;
}

// Size of a pass's render target texture: like RetroArch, the next power of 2 of the (maximum)
// image size, so the image of a pass and the original frame use the same texture coordinates
// (many Cg shaders sample ORIG / PASSPREVn with the texture coordinates of their input)
static int d3d11PassTextureSize(int nType, float fScale, int nInputTexture, int nSize)
{
	int n = nSize;
	if (nType == D3D11_SCALE_SOURCE || nType == D3D11_SCALE_UNSET) {
		n = (int)(nInputTexture * fScale + 0.5f);
		if (n < nSize) n = nSize;
	}

	int nMax = (nFeatureLevel >= D3D_FEATURE_LEVEL_11_0) ? 16384 : (nFeatureLevel >= D3D_FEATURE_LEVEL_10_0) ? 8192 : (nFeatureLevel >= D3D_FEATURE_LEVEL_9_3) ? 4096 : 2048;
	int nPow2 = 1;
	while (nPow2 < n && nPow2 < nMax) {
		nPow2 <<= 1;
	}
	if (nPow2 < nSize) {
		nPow2 = nSize;						// too large for a power of 2 texture - use the exact size
	}
	return nPow2;
}

// Run the shader chain; the last pass (or a final stretch) draws into the image area of the back buffer
static void d3d11DrawShader()
{
	UINT nStride = sizeof(d3d11Vertex), nOffset = 0;
	ID3D11ShaderResourceView* NullSRV[16] = { NULL, };

	float InVideo[2], InTexture[2];
	ID3D11ShaderResourceView* pDummy;
	d3d11ShaderSource(D3D11_SRC_ORIG, 0, 0, &pDummy, InVideo, InTexture);

	int nInWidth = (int)InVideo[0], nInHeight = (int)InVideo[1];
	int nInTexWidth = (int)InTexture[0], nInTexHeight = (int)InTexture[1];
	size_t nPasses = pShader->Passes.size();

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetVertexBuffers(0, 1, &pPassVB, &nStride, &nOffset);
	pContext->OMSetBlendState(NULL, NULL, 0xFFFFFFFF);

	for (size_t i = 0; i < nPasses; i++) {
		d3d11ShaderPass& Pass = pShader->Passes[i];
		int nPass = (int)i;

		int nOutWidth, nOutHeight, nOutTexWidth = 0, nOutTexHeight = 0;
		if (Pass.bToScreen) {
			nOutWidth = nImageWidth;
			nOutHeight = nImageHeight;
		} else {
			nOutWidth = d3d11PassSize(Pass.Desc.nScaleTypeX, Pass.Desc.fScaleX, nInWidth, nImageWidth);
			nOutHeight = d3d11PassSize(Pass.Desc.nScaleTypeY, Pass.Desc.fScaleY, nInHeight, nImageHeight);
			nOutTexWidth = d3d11PassTextureSize(Pass.Desc.nScaleTypeX, Pass.Desc.fScaleX, nInTexWidth, nOutWidth);
			nOutTexHeight = d3d11PassTextureSize(Pass.Desc.nScaleTypeY, Pass.Desc.fScaleY, nInTexHeight, nOutHeight);
		}

		// the input's texture coordinates
		ID3D11ShaderResourceView* pInput;
		d3d11ShaderSource(D3D11_SRC_INPUT, 0, nPass, &pInput, InVideo, InTexture);
		float u1 = InVideo[0] / InTexture[0], v1 = InVideo[1] / InTexture[1];

		d3d11Vertex Quad[4];
		if (Pass.bToScreen) {
			for (int k = 0; k < 4; k++) {
				Quad[k] = GameQuad[k];			// rotated to the screen
			}
		} else {
			d3d11SetVertex(&Quad[0], -1.0f,  1.0f, 0.0f, 0.0f, 1.0f);
			d3d11SetVertex(&Quad[1],  1.0f,  1.0f, 0.0f, 0.0f, 1.0f);
			d3d11SetVertex(&Quad[2], -1.0f, -1.0f, 0.0f, 0.0f, 1.0f);
			d3d11SetVertex(&Quad[3],  1.0f, -1.0f, 0.0f, 0.0f, 1.0f);
		}
		Quad[0].u = 0.0f; Quad[0].v = 0.0f; Quad[0].u1 = 0.0f; Quad[0].v1 = 0.0f;
		Quad[1].u = u1;   Quad[1].v = 0.0f; Quad[1].u1 = 1.0f; Quad[1].v1 = 0.0f;
		Quad[2].u = 0.0f; Quad[2].v = v1;   Quad[2].u1 = 0.0f; Quad[2].v1 = 1.0f;
		Quad[3].u = u1;   Quad[3].v = v1;   Quad[3].u1 = 1.0f; Quad[3].v1 = 1.0f;
		d3d11UpdateBuffer(pPassVB, Quad, sizeof(Quad));

		// render target
		pContext->PSSetShaderResources(0, 16, NullSRV);
		pContext->VSSetShaderResources(0, 16, NullSRV);

		D3D11_VIEWPORT vp;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		if (Pass.bToScreen) {
			pContext->OMSetRenderTargets(1, &pRenderTarget, NULL);
			vp.TopLeftX = (float)Render.left;
			vp.TopLeftY = (float)Render.top;
			vp.Width = (float)(Render.right - Render.left);
			vp.Height = (float)(Render.bottom - Render.top);
		} else {
			if (d3d11EnsurePassTarget(Pass, nOutWidth, nOutHeight, nOutTexWidth, nOutTexHeight)) {
				return;
			}
			pContext->OMSetRenderTargets(1, &Pass.pRTV, NULL);
			vp.TopLeftX = 0.0f;
			vp.TopLeftY = 0.0f;
			vp.Width = (float)nOutWidth;
			vp.Height = (float)nOutHeight;
		}
		pContext->RSSetViewports(1, &vp);

		d3d11UpdatePassConstants(Pass.VSBind, nPass, (float)nOutWidth, (float)nOutHeight);
		d3d11UpdatePassConstants(Pass.PSBind, nPass, (float)nOutWidth, (float)nOutHeight);

		pContext->IASetInputLayout(Pass.pLayout);
		pContext->VSSetShader(Pass.pVS, NULL, 0);
		pContext->PSSetShader(Pass.pPS, NULL, 0);
		d3d11BindPassResources(Pass.VSBind, nPass, false);
		d3d11BindPassResources(Pass.PSBind, nPass, true);

		pContext->Draw(4, 0);

		nInWidth = nOutWidth;
		nInHeight = nOutHeight;
		nInTexWidth = Pass.bToScreen ? nOutWidth : Pass.nTexWidth;
		nInTexHeight = Pass.bToScreen ? nOutHeight : Pass.nTexHeight;
	}

	pContext->PSSetShaderResources(0, 16, NullSRV);
	pContext->VSSetShaderResources(0, 16, NullSRV);

	// stretch the output of the last pass onto the screen
	d3d11ShaderPass& Last = pShader->Passes[nPasses - 1];
	if (!Last.bToScreen && Last.pSRV) {
		d3d11Vertex Quad[4];
		for (int k = 0; k < 4; k++) {
			Quad[k] = GameQuad[k];
		}
		float u1 = (float)Last.nWidth / Last.nTexWidth, v1 = (float)Last.nHeight / Last.nTexHeight;
		Quad[0].u = 0.0f; Quad[0].v = 0.0f;
		Quad[1].u = u1;   Quad[1].v = 0.0f;
		Quad[2].u = 0.0f; Quad[2].v = v1;
		Quad[3].u = u1;   Quad[3].v = v1;
		d3d11UpdateBuffer(pPassVB, Quad, sizeof(Quad));

		D3D11_VIEWPORT vp;
		vp.TopLeftX = (float)Render.left;
		vp.TopLeftY = (float)Render.top;
		vp.Width = (float)(Render.right - Render.left);
		vp.Height = (float)(Render.bottom - Render.top);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		pContext->OMSetRenderTargets(1, &pRenderTarget, NULL);
		pContext->RSSetViewports(1, &vp);
		pContext->IASetInputLayout(pBasicLayout);
		pContext->VSSetShader(pBasicVS, NULL, 0);
		pContext->PSSetShader(pBasicPS, NULL, 0);
		pContext->PSSetShaderResources(0, 1, &Last.pSRV);
		pContext->PSSetSamplers(0, 1, bVidDX9Bilinear ? &pSamplerLinear : &pSamplerPoint);
		pContext->Draw(4, 0);

		pContext->PSSetShaderResources(0, 16, NullSRV);
	}

	pContext->OMSetRenderTargets(1, &pRenderTarget, NULL);
}

// ----------------------------------------------------------------------------
// OSD backend: GDI draws into a DIB section, which is uploaded to a texture when it's blitted

struct d3d11OSDSurface {
	int nWidth, nHeight;
	HDC hDC;
	HBITMAP hBitmap, hOldBitmap;
	UINT32* pBits;
	ID3D11Texture2D* pTexture;
	ID3D11ShaderResourceView* pSRV;
	UINT nGeneration;
	bool bDirty;
};

static void* d3d11OSDCreateSurface(int nWidth, int nHeight)
{
	if (nWidth <= 0 || nHeight <= 0) {
		return NULL;
	}

	d3d11OSDSurface* pSurf = (d3d11OSDSurface*)calloc(1, sizeof(d3d11OSDSurface));
	if (pSurf == NULL) {
		return NULL;
	}

	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = nWidth;
	bmi.bmiHeader.biHeight = -nHeight;					// top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	pSurf->nWidth = nWidth;
	pSurf->nHeight = nHeight;
	pSurf->hDC = CreateCompatibleDC(NULL);
	pSurf->hBitmap = CreateDIBSection(pSurf->hDC, &bmi, DIB_RGB_COLORS, (void**)&pSurf->pBits, NULL, 0);

	if (pSurf->hDC == NULL || pSurf->hBitmap == NULL || pSurf->pBits == NULL) {
		if (pSurf->hBitmap) DeleteObject(pSurf->hBitmap);
		if (pSurf->hDC) DeleteDC(pSurf->hDC);
		free(pSurf);
		return NULL;
	}

	pSurf->hOldBitmap = (HBITMAP)SelectObject(pSurf->hDC, pSurf->hBitmap);
	pSurf->bDirty = true;

	return pSurf;
}

static void d3d11OSDReleaseSurface(void* p)
{
	d3d11OSDSurface* pSurf = (d3d11OSDSurface*)p;
	if (pSurf == NULL) {
		return;
	}

	D3D11_RELEASE(pSurf->pSRV);
	D3D11_RELEASE(pSurf->pTexture);

	SelectObject(pSurf->hDC, pSurf->hOldBitmap);
	DeleteObject(pSurf->hBitmap);
	DeleteDC(pSurf->hDC);
	free(pSurf);
}

static HDC d3d11OSDGetDC(void* p)
{
	return ((d3d11OSDSurface*)p)->hDC;
}

static void d3d11OSDReleaseDC(void* p, HDC)
{
	GdiFlush();
	((d3d11OSDSurface*)p)->bDirty = true;
}

static void d3d11OSDFill(void* p, unsigned int nColour, RECT* pRect)
{
	d3d11OSDSurface* pSurf = (d3d11OSDSurface*)p;
	RECT r = { 0, 0, pSurf->nWidth, pSurf->nHeight };

	if (pRect) {
		r.left = (pRect->left > 0) ? pRect->left : 0;
		r.top = (pRect->top > 0) ? pRect->top : 0;
		r.right = (pRect->right < pSurf->nWidth) ? pRect->right : pSurf->nWidth;
		r.bottom = (pRect->bottom < pSurf->nHeight) ? pRect->bottom : pSurf->nHeight;
	}

	GdiFlush();

	for (int y = r.top; y < r.bottom; y++) {
		UINT32* pLine = pSurf->pBits + y * pSurf->nWidth;
		for (int x = r.left; x < r.right; x++) {
			pLine[x] = nColour;
		}
	}

	pSurf->bDirty = true;
}

static void d3d11OSDBlit(RECT* pDest, void* p, RECT* pSrc)
{
	d3d11OSDSurface* pSurf = (d3d11OSDSurface*)p;

	if (pSurf == NULL || pDevice == NULL || pRenderTarget == NULL || pOSDVB == NULL) {
		return;
	}
	if (pDest->right <= pDest->left || pDest->bottom <= pDest->top || pSrc->right <= pSrc->left || pSrc->bottom <= pSrc->top) {
		return;
	}

	// (Re)create the texture if needed (first use or a new device)
	if (pSurf->pTexture == NULL || pSurf->nGeneration != nDeviceGeneration) {
		D3D11_TEXTURE2D_DESC td;

		D3D11_RELEASE(pSurf->pSRV);
		D3D11_RELEASE(pSurf->pTexture);

		memset(&td, 0, sizeof(td));
		td.Width = pSurf->nWidth;
		td.Height = pSurf->nHeight;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		if (FAILED(pDevice->CreateTexture2D(&td, NULL, &pSurf->pTexture))) {
			return;
		}
		if (FAILED(pDevice->CreateShaderResourceView(pSurf->pTexture, NULL, &pSurf->pSRV))) {
			D3D11_RELEASE(pSurf->pTexture);
			return;
		}

		pSurf->nGeneration = nDeviceGeneration;
		pSurf->bDirty = true;
	}

	if (pSurf->bDirty) {
		GdiFlush();
		pContext->UpdateSubresource(pSurf->pTexture, 0, NULL, pSurf->pBits, pSurf->nWidth * 4, 0);
		pSurf->bDirty = false;
	}

	// Quad in clip space
	float fW = (float)nSwapChainWidth, fH = (float)nSwapChainHeight;
	float x0 = (float)pDest->left / fW * 2.0f - 1.0f, x1 = (float)pDest->right / fW * 2.0f - 1.0f;
	float y0 = 1.0f - (float)pDest->top / fH * 2.0f, y1 = 1.0f - (float)pDest->bottom / fH * 2.0f;
	float u0 = (float)pSrc->left / pSurf->nWidth, u1 = (float)pSrc->right / pSurf->nWidth;
	float v0 = (float)pSrc->top / pSurf->nHeight, v1 = (float)pSrc->bottom / pSurf->nHeight;

	d3d11Vertex Quad[4];
	d3d11SetVertex(&Quad[0], x0, y0, u0, v0, 1.0f);
	d3d11SetVertex(&Quad[1], x1, y0, u1, v0, 1.0f);
	d3d11SetVertex(&Quad[2], x0, y1, u0, v1, 1.0f);
	d3d11SetVertex(&Quad[3], x1, y1, u1, v1, 1.0f);
	d3d11UpdateBuffer(pOSDVB, Quad, sizeof(Quad));

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, fW, fH, 0.0f, 1.0f };
	UINT nStride = sizeof(d3d11Vertex), nOffset = 0;

	pContext->RSSetViewports(1, &vp);
	pContext->IASetInputLayout(pBasicLayout);
	pContext->IASetVertexBuffers(0, 1, &pOSDVB, &nStride, &nOffset);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->VSSetShader(pBasicVS, NULL, 0);
	pContext->PSSetShader(pOSDPS, NULL, 0);
	pContext->PSSetShaderResources(0, 1, &pSurf->pSRV);
	pContext->PSSetSamplers(0, 1, &pSamplerPoint);
	pContext->OMSetBlendState(NULL, NULL, 0xFFFFFFFF);
	pContext->Draw(4, 0);
}

static VidSOSDBackend D3D11OSDBackend = {
	d3d11OSDCreateSurface,
	d3d11OSDReleaseSurface,
	d3d11OSDGetDC,
	d3d11OSDReleaseDC,
	d3d11OSDFill,
	d3d11OSDBlit
};

// ----------------------------------------------------------------------------
// View window

static int d3d11Paint(int bValidate);

static LRESULT CALLBACK D3D11ViewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_NCHITTEST:
			return HTTRANSPARENT;						// let the main window handle the mouse
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			ValidateRect(hWnd, NULL);
			if (bD3D11Ready && !bInPaint) {
				d3d11Paint(0);
			}
			return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static int d3d11CreateView()
{
	if (!bViewClassRegistered) {
		WNDCLASSEX wc;
		memset(&wc, 0, sizeof(wc));
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = D3D11ViewProc;
		wc.hInstance = hAppInst;
		wc.hCursor = NULL;
		wc.hbrBackground = NULL;
		wc.lpszClassName = szD3D11ViewClass;
		if (RegisterClassEx(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
			return 1;
		}
		bViewClassRegistered = true;
	}

	RECT rect;
	GetClientRect(hVidWnd, &rect);

	hD3D11View = CreateWindowEx(WS_EX_NOPARENTNOTIFY, szD3D11ViewClass, _T(""), WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS,
		0, 0, (rect.right > 0) ? rect.right : 1, (rect.bottom > 0) ? rect.bottom : 1, hVidWnd, NULL, hAppInst, NULL);

	if (hD3D11View == NULL) {
		return 1;
	}

	SetWindowPos(hD3D11View, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	return 0;
}

static void d3d11DestroyView()
{
	if (hD3D11View) {
		DestroyWindow(hD3D11View);
		hD3D11View = NULL;
	}
}

// ----------------------------------------------------------------------------
// Fullscreen

static BOOL CALLBACK d3d11MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam)
{
	MONITORINFOEX mi;
	memset(&mi, 0, sizeof(mi));
	mi.cbSize = sizeof(mi);

	if (GetMonitorInfo(hMonitor, &mi)) {
		const wchar_t* pName = (const wchar_t*)(((void**)lParam)[0]);
#if defined (UNICODE) || defined (_UNICODE)
		if (_wcsicmp(mi.szDevice, pName) == 0) {
#else
		char szName[64];
		WideCharToMultiByte(CP_ACP, 0, pName, -1, szName, sizeof(szName), NULL, NULL);
		if (_stricmp(mi.szDevice, szName) == 0) {
#endif
			((HMONITOR*)lParam)[1] = hMonitor;
			return FALSE;
		}
	}

	return TRUE;
}

static HMONITOR d3d11GetMonitor()
{
	const wchar_t* pName = NULL;

	if ((nRotateGame & 1) && VerScreen[0]) {
		pName = VerScreen;
	} else if (HorScreen[0]) {
		pName = HorScreen;
	}

	if (pName) {
		void* Data[2] = { (void*)pName, NULL };
		EnumDisplayMonitors(NULL, NULL, d3d11MonitorEnumProc, (LPARAM)Data);
		if (Data[1]) {
			return (HMONITOR)Data[1];
		}
	}

	return MonitorFromWindow(hVidWnd, MONITOR_DEFAULTTOPRIMARY);
}

static void d3d11RestoreDisplayMode()
{
	if (bChangedDisplayMode) {
		ChangeDisplaySettingsEx(szFullscreenDevice[0] ? szFullscreenDevice : NULL, NULL, NULL, 0, NULL);
		bChangedDisplayMode = false;
	}
}

static int d3d11EnterFullscreen()
{
	HMONITOR hMonitor = d3d11GetMonitor();
	MONITORINFOEX mi;

	memset(&mi, 0, sizeof(mi));
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfo(hMonitor, &mi)) {
		return 1;
	}

	_tcsncpy(szFullscreenDevice, mi.szDevice, CCHDEVICENAME - 1);
	szFullscreenDevice[CCHDEVICENAME - 1] = 0;

	// Unless borderless windowed fullscreen is selected, switch to the requested display mode
	if (!bVidDX9WinFullscreen) {
		int nWidth = nVidWidth, nHeight = nVidHeight;

		if (bVidArcaderes) {
			if (!VidSGetArcaderes(&nWidth, &nHeight)) {
				nWidth = nVidWidth;
				nHeight = nVidHeight;
			}
		}

		if (nWidth > 0 && nHeight > 0 && (nWidth != mi.rcMonitor.right - mi.rcMonitor.left || nHeight != mi.rcMonitor.bottom - mi.rcMonitor.top)) {
			DEVMODE dm;
			memset(&dm, 0, sizeof(dm));
			dm.dmSize = sizeof(dm);
			dm.dmPelsWidth = nWidth;
			dm.dmPelsHeight = nHeight;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
			if (nVidRefresh) {
				dm.dmDisplayFrequency = nVidRefresh;
				dm.dmFields |= DM_DISPLAYFREQUENCY;
			}

			if (ChangeDisplaySettingsEx(szFullscreenDevice, &dm, NULL, CDS_FULLSCREEN, NULL) == DISP_CHANGE_SUCCESSFUL) {
				bChangedDisplayMode = true;
			} else {
#ifdef PRINT_DEBUG_INFO
				dprintf(_T("  * Warning: Couldn't set display mode %i x %i, using the desktop resolution.\n"), nWidth, nHeight);
#endif
			}

			// The monitor rectangle may have changed
			GetMonitorInfo(hMonitor, &mi);
		}
	}

	nVidScrnWidth = mi.rcMonitor.right - mi.rcMonitor.left;
	nVidScrnHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

	SetWindowPos(hVidWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, nVidScrnWidth, nVidScrnHeight, SWP_SHOWWINDOW | SWP_FRAMECHANGED);

	return 0;
}

// ----------------------------------------------------------------------------
// Device / swap chain

static int d3d11CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer = NULL;

	D3D11_RELEASE(pRenderTarget);

	if (FAILED(pSwapChain->GetBuffer(0, FBNEO_IID_ID3D11Texture2D, (void**)&pBackBuffer))) {
		return 1;
	}

	HRESULT hr = pDevice->CreateRenderTargetView(pBackBuffer, NULL, &pRenderTarget);
	pBackBuffer->Release();

	return FAILED(hr) ? 1 : 0;
}

static int d3d11ResizeSwapChain(UINT nWidth, UINT nHeight)
{
	if (nWidth == 0 || nHeight == 0) {
		return 1;
	}
	if (nWidth == nSwapChainWidth && nHeight == nSwapChainHeight && pRenderTarget) {
		return 0;
	}

	pContext->OMSetRenderTargets(0, NULL, NULL);
	D3D11_RELEASE(pRenderTarget);

	if (FAILED(pSwapChain->ResizeBuffers(0, nWidth, nHeight, DXGI_FORMAT_UNKNOWN, nSwapChainFlags))) {
		return 1;
	}

	nSwapChainWidth = nWidth;
	nSwapChainHeight = nHeight;

	return d3d11CreateRenderTarget();
}

// IDXGIFactory5::CheckFeatureSupport() is called through the vtable, so no DXGI 1.5 headers are needed
static bool d3d11TearingSupported(IDXGIFactory* pFactory)
{
	typedef HRESULT (STDMETHODCALLTYPE* pfnCheckFeatureSupport)(void*, UINT, void*, UINT);
	void* pFactory5 = NULL;
	BOOL bAllowTearing = FALSE;

	if (FAILED(pFactory->QueryInterface(FBNEO_IID_IDXGIFactory5, &pFactory5))) {
		return false;
	}

	pfnCheckFeatureSupport pCheckFeatureSupport = (pfnCheckFeatureSupport)((*(void***)pFactory5)[28]);
	if (FAILED(pCheckFeatureSupport(pFactory5, FBNEO_DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bAllowTearing, sizeof(bAllowTearing)))) {
		bAllowTearing = FALSE;
	}

	((IUnknown*)pFactory5)->Release();

	return bAllowTearing != FALSE;
}

static int d3d11CreateSwapChain()
{
	IDXGIDevice* pDXGIDevice = NULL;
	IDXGIAdapter* pAdapter = NULL;
	IDXGIFactory* pFactory = NULL;
	RECT rect;

	if (FAILED(pDevice->QueryInterface(FBNEO_IID_IDXGIDevice, (void**)&pDXGIDevice))) {
		return 1;
	}
	if (FAILED(pDXGIDevice->GetAdapter(&pAdapter))) {
		pDXGIDevice->Release();
		return 1;
	}
	if (FAILED(pAdapter->GetParent(FBNEO_IID_IDXGIFactory, (void**)&pFactory))) {
		pAdapter->Release();
		pDXGIDevice->Release();
		return 1;
	}

	GetClientRect(hD3D11View, &rect);
	nSwapChainWidth = (rect.right > 0) ? rect.right : 1;
	nSwapChainHeight = (rect.bottom > 0) ? rect.bottom : 1;

	bool bTearing = bVidVRR && d3d11TearingSupported(pFactory);
	nSwapChainFlags = 0;

	// Prefer the flip model (Win10: FLIP_DISCARD, Win8: FLIP_SEQUENTIAL), fall back to the blt model (Win7)
	static const DXGI_SWAP_EFFECT SwapEffects[] = { FBNEO_DXGI_SWAP_EFFECT_FLIP_DISCARD, FBNEO_DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_DISCARD };

	for (int i = 0; i < 3 && pSwapChain == NULL; i++) {
		DXGI_SWAP_CHAIN_DESC scd;
		bool bFlip = (SwapEffects[i] != DXGI_SWAP_EFFECT_DISCARD);

		memset(&scd, 0, sizeof(scd));
		scd.BufferDesc.Width = nSwapChainWidth;
		scd.BufferDesc.Height = nSwapChainHeight;
		scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		scd.SampleDesc.Count = 1;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.BufferCount = bFlip ? ((nVidFullscreen && bVidTripleBuffer) ? 3 : 2) : 1;
		scd.OutputWindow = hD3D11View;
		scd.Windowed = TRUE;
		scd.SwapEffect = SwapEffects[i];
		scd.Flags = (bFlip && bTearing) ? FBNEO_DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		if (SUCCEEDED(pFactory->CreateSwapChain(pDevice, &scd, &pSwapChain))) {
			nSwapEffect = SwapEffects[i];
			nSwapChainFlags = scd.Flags;
		} else {
			pSwapChain = NULL;
		}
	}

	if (pSwapChain) {
		pFactory->MakeWindowAssociation(hD3D11View, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
	}

	// Keep latency low
	IDXGIDevice1* pDXGIDevice1 = NULL;
	if (SUCCEEDED(pDevice->QueryInterface(FBNEO_IID_IDXGIDevice1, (void**)&pDXGIDevice1))) {
		pDXGIDevice1->SetMaximumFrameLatency(1);
		pDXGIDevice1->Release();
	}

	pFactory->Release();
	pAdapter->Release();
	pDXGIDevice->Release();

	if (pSwapChain == NULL) {
		return 1;
	}

	return d3d11CreateRenderTarget();
}

static int d3d11CreateDevice()
{
	static const D3D_FEATURE_LEVEL FeatureLevels[] = {
		D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
	};
	static const D3D_DRIVER_TYPE DriverTypes[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP };

	for (int d = 0; d < 2 && pDevice == NULL; d++) {
		UINT nFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined (FBNEO_DEBUG) && 0
		nFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		HRESULT hr = _D3D11CreateDevice(NULL, DriverTypes[d], NULL, nFlags, FeatureLevels, 6, D3D11_SDK_VERSION, &pDevice, &nFeatureLevel, &pContext);
		if (FAILED(hr)) {
			hr = _D3D11CreateDevice(NULL, DriverTypes[d], NULL, 0, FeatureLevels, 6, D3D11_SDK_VERSION, &pDevice, &nFeatureLevel, &pContext);
		}
		if (FAILED(hr)) {
			pDevice = NULL;
			pContext = NULL;
		} else {
			bWarpDevice = (DriverTypes[d] == D3D_DRIVER_TYPE_WARP);
		}
	}

	if (pDevice == NULL) {
		return 1;
	}

	nDeviceGeneration++;
	bDeviceLost = false;

	return 0;
}

static int d3d11CreateResources()
{
	ID3DBlob* pCode = NULL;

	// Built-in shaders
	if (FAILED(d3d11Compile(szBuiltinShaders, strlen(szBuiltinShaders), NULL, "vs_main", false, false, &pCode))) {
		return 1;
	}
	if (FAILED(pDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &pBasicVS)) ||
		FAILED(pDevice->CreateInputLayout(BasicLayout, BASIC_LAYOUT_SIZE, pCode->GetBufferPointer(), pCode->GetBufferSize(), &pBasicLayout))) {
		pCode->Release();
		return 1;
	}
	pCode->Release();

	if (FAILED(d3d11Compile(szBuiltinShaders, strlen(szBuiltinShaders), NULL, "ps_main", true, false, &pCode))) {
		return 1;
	}
	HRESULT hr = pDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &pBasicPS);
	pCode->Release();
	if (FAILED(hr)) {
		return 1;
	}

	if (FAILED(d3d11Compile(szBuiltinShaders, strlen(szBuiltinShaders), NULL, "ps_osd", true, false, &pCode))) {
		return 1;
	}
	hr = pDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), NULL, &pOSDPS);
	pCode->Release();
	if (FAILED(hr)) {
		return 1;
	}

	// Vertex buffers
	pGameVB = d3d11CreateDynamicBuffer(sizeof(d3d11Vertex) * 4, D3D11_BIND_VERTEX_BUFFER);
	pOSDVB = d3d11CreateDynamicBuffer(sizeof(d3d11Vertex) * 4, D3D11_BIND_VERTEX_BUFFER);
	pPassVB = d3d11CreateDynamicBuffer(sizeof(d3d11Vertex) * 4, D3D11_BIND_VERTEX_BUFFER);
	if (pGameVB == NULL || pOSDVB == NULL || pPassVB == NULL) {
		return 1;
	}

	// Samplers for shaders: [point, linear] x [border, clamp, wrap, mirror]
	{
		static const D3D11_TEXTURE_ADDRESS_MODE Modes[4] = { D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR };
		for (int f = 0; f < 2; f++) {
			for (int w = 0; w < 4; w++) {
				pSamplerCache[f][w] = d3d11CreateSampler(f != 0, Modes[w]);
				if (pSamplerCache[f][w] == NULL) {
					return 1;
				}
			}
		}
	}

	// States
	pSamplerPoint = d3d11CreateSampler(false, D3D11_TEXTURE_ADDRESS_CLAMP);
	pSamplerLinear = d3d11CreateSampler(true, D3D11_TEXTURE_ADDRESS_CLAMP);
	if (pSamplerPoint == NULL || pSamplerLinear == NULL) {
		return 1;
	}

	D3D11_RASTERIZER_DESC rd;
	memset(&rd, 0, sizeof(rd));
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	if (FAILED(pDevice->CreateRasterizerState(&rd, &pRasterState))) {
		return 1;
	}

	D3D11_BLEND_DESC bd;
	memset(&bd, 0, sizeof(bd));
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if (FAILED(pDevice->CreateBlendState(&bd, &pBlendAlpha))) {
		return 1;
	}

	return 0;
}

static void d3d11ReleaseTextures()
{
	for (int i = 0; i < 8; i++) {
		D3D11_RELEASE(pGameSRV[i]);
		D3D11_RELEASE(pGameTexture[i]);
	}
	nGameTextures = 0;
	nFramesUploaded = 0;
	bTextureValid = false;
}

static int d3d11CreateTextures()
{
	D3D11_TEXTURE2D_DESC td;

	d3d11ReleaseTextures();

	memset(&td, 0, sizeof(td));
	td.Width = nTextureWidth;
	td.Height = nTextureHeight;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DYNAMIC;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	int nCount = bVidMotionBlur ? 2 : 1;
	if (pShader && pShader->nHistory + 1 > nCount) {
		nCount = pShader->nHistory + 1;
	}
	if (nCount > D3D11_MAX_HISTORY) {
		nCount = D3D11_MAX_HISTORY;
	}

	for (int i = 0; i < nCount; i++) {
		if (FAILED(pDevice->CreateTexture2D(&td, NULL, &pGameTexture[i]))) {
			return 1;
		}
		if (FAILED(pDevice->CreateShaderResourceView(pGameTexture[i], NULL, &pGameSRV[i]))) {
			return 1;
		}
	}

	nGameTextures = nCount;
	nCurrentTexture = 0;

	return 0;
}

static void d3d11ReleaseDevice()
{
	if (pContext) {
		pContext->ClearState();
		pContext->Flush();
	}

	d3d11ReleaseShader();
	d3d11ReleaseTextures();

	for (int f = 0; f < 2; f++) {
		for (int w = 0; w < 4; w++) {
			D3D11_RELEASE(pSamplerCache[f][w]);
		}
	}
	D3D11_RELEASE(pPassVB);

	D3D11_RELEASE(pBlendAlpha);
	D3D11_RELEASE(pRasterState);
	D3D11_RELEASE(pSamplerLinear);
	D3D11_RELEASE(pSamplerPoint);
	D3D11_RELEASE(pOSDVB);
	D3D11_RELEASE(pGameVB);
	D3D11_RELEASE(pBasicLayout);
	D3D11_RELEASE(pOSDPS);
	D3D11_RELEASE(pBasicPS);
	D3D11_RELEASE(pBasicVS);

	D3D11_RELEASE(pRenderTarget);
	D3D11_RELEASE(pSwapChain);
	D3D11_RELEASE(pContext);
	D3D11_RELEASE(pDevice);

	nSwapChainWidth = nSwapChainHeight = 0;
}

// ----------------------------------------------------------------------------
// Blitter interface

static int d3d11Exit()
{
	bD3D11Ready = false;

	VidSExitOSD();
	VidSSetOSDBackend(NULL);

	VidSoftFXExit();

	d3d11ReleaseDevice();
	d3d11DestroyView();

	if (pConvBuffer) {
		free(pConvBuffer);
		pConvBuffer = NULL;
	}

	VidSFreeVidImage();

	d3d11RestoreDisplayMode();

	d3d11UnloadLibraries();

	nRotateGame = 0;

	return 0;
}

static int d3d11ImageInit()
{
	if (nRotateGame & 1) {
		nVidImageWidth = nGameHeight;
		nVidImageHeight = nGameWidth;
	} else {
		nVidImageWidth = nGameWidth;
		nVidImageHeight = nGameHeight;
	}

	nGameImageWidth = nVidImageWidth;
	nGameImageHeight = nVidImageHeight;

	// The texture is always 32-bit; SoftFX filters that can't output 32-bit run in 16-bit and get converted
	nVidImageDepth = 32;
	if (bDrvOkay && nPreScaleEffect && VidSoftFXCheckDepth(nPreScaleEffect, 32) != 32) {
		nVidImageDepth = 16;
	}

	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;

	SetBurnHighCol(nVidImageDepth);

	if (VidSAllocVidImage()) {
		return 1;
	}

	int nZoom = nPreScaleEffect ? nPreScaleZoom : 1;

	nTextureWidth = GetTextureSize(nGameImageWidth * nZoom);
	nTextureHeight = GetTextureSize(nGameImageHeight * nZoom);

	// 2xSaI etc. needs an extra line below the image
	if (nPreScaleEffect >= FILTER_SUPEREAGLE && nPreScaleEffect <= FILTER_SUPER_2XSAI && nGameImageHeight * nZoom == nTextureHeight) {
		nTextureHeight <<= 1;
	}

	UINT nMaxSize = (nFeatureLevel >= D3D_FEATURE_LEVEL_11_0) ? 16384 : (nFeatureLevel >= D3D_FEATURE_LEVEL_10_0) ? 8192 : (nFeatureLevel >= D3D_FEATURE_LEVEL_9_3) ? 4096 : 2048;
	if ((UINT)nTextureWidth > nMaxSize || (UINT)nTextureHeight > nMaxSize) {
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Required texture size not supported by 3D hardware."));
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return 1;
	}

	if (pConvBuffer) {
		free(pConvBuffer);
		pConvBuffer = NULL;
	}
	if (nVidImageDepth == 16) {
		nConvPitch = nTextureWidth * 2;
		pConvBuffer = (unsigned char*)calloc(nConvPitch, nTextureHeight + 2);
		if (pConvBuffer == NULL) {
			return 1;
		}
	}

	if (d3d11CreateTextures()) {
		return 1;
	}

#ifdef PRINT_DEBUG_INFO
	dprintf(_T("  * Allocated a %i x %i (%i-bit) image, %i x %i texture.\n"), nVidImageWidth, nVidImageHeight, nVidImageDepth, nTextureWidth, nTextureHeight);
#endif

	return 0;
}

static int d3d11Init()
{
	if (hScrnWnd == NULL) {
		return 1;
	}

	hVidWnd = hScrnWnd;

#ifdef PRINT_DEBUG_INFO
	dprintf(_T("*** Initialising Direct3D 11 blitter.\n"));
#endif

	int nRet = d3d11LoadLibraries();
	if (nRet) {
		if (nRet == 2) {
			FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Direct3D 11 blitter: couldn't load the HLSL shader compiler (d3dcompiler_47.dll)."));
		} else {
			FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Direct3D 11 blitter: Direct3D 11 is not available on this system."));
		}
		FBAPopupDisplay(PUF_TYPE_ERROR);
		d3d11Exit();
		return 1;
	}

	// Orientation
	nGameWidth = nVidImageWidth;
	nGameHeight = nVidImageHeight;
	nRotateGame = 0;

	if (bDrvOkay) {
		BurnDrvGetVisibleSize(&nGameWidth, &nGameHeight);

		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
			if (nVidRotationAdjust & 1) {
				int n = nGameWidth;
				nGameWidth = nGameHeight;
				nGameHeight = n;
				nRotateGame |= (nVidRotationAdjust & 2);
			} else {
				nRotateGame |= 1;
			}
		}

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
			nRotateGame ^= 2;
		}
	}

	// Screen
	if (nVidFullscreen) {
		if (d3d11EnterFullscreen()) {
			d3d11Exit();
			return 1;
		}
	} else {
		MONITORINFO mi;
		memset(&mi, 0, sizeof(mi));
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(MonitorFromWindow(hVidWnd, MONITOR_DEFAULTTOPRIMARY), &mi);
		nVidScrnWidth = mi.rcMonitor.right - mi.rcMonitor.left;
		nVidScrnHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}
	nVidScrnDepth = 32;

	if (d3d11CreateView()) {
#ifdef PRINT_DEBUG_INFO
		dprintf(_T("  * Error: Couldn't create the view window.\n"));
#endif
		d3d11Exit();
		return 1;
	}

	if (d3d11CreateDevice()) {
#ifdef PRINT_DEBUG_INFO
		dprintf(_T("  * Error: Couldn't create a Direct3D 11 device.\n"));
#endif
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Direct3D 11 blitter: couldn't create a Direct3D 11 device."));
		FBAPopupDisplay(PUF_TYPE_ERROR);
		d3d11Exit();
		return 1;
	}

	if (d3d11CreateSwapChain() || d3d11CreateResources()) {
#ifdef PRINT_DEBUG_INFO
		dprintf(_T("  * Error: Couldn't initialise Direct3D 11 resources.\n"));
#endif
		d3d11Exit();
		return 1;
	}

	// SoftFX prescale
	nPreScaleEffect = 0;
	nPreScaleZoom = 1;
	if (bDrvOkay) {
		nPreScaleEffect = nVidBlitterOpt[nVidSelect] & 0xFF;
		nPreScaleZoom = VidSoftFXGetZoom(nPreScaleEffect);
		if (nPreScaleZoom < 1) {
			nPreScaleZoom = 1;
		}
	}

	if (d3d11ImageInit()) {
		d3d11Exit();
		return 1;
	}

	if (nPreScaleEffect) {
		if (VidSoftFXInit(nPreScaleEffect, 0)) {
#ifdef PRINT_DEBUG_INFO
			dprintf(_T("  * Error: Couldn't initialise SoftFX.\n"));
#endif
			d3d11Exit();
			return 1;
		}
	}

	// Shader
	if (d3d11LoadShader()) {
		szVidD3D11Shader[0] = 0;				// don't complain about it every time
	}

	// OSD
	VidSSetOSDBackend(&D3D11OSDBackend);

	int nFlags = 0;
	if (nVidScrnWidth < 1280) {
		nFlags++;
		if (nVidScrnWidth < 1024) {
			nFlags++;
			if (nVidScrnWidth < 640) {
				nFlags++;
			}
		}
	}
	VidSInitOSD(nFlags);

	nImageWidth = 0;
	nImageHeight = 0;
	memset(&Dest, 0, sizeof(Dest));
	memset(GameQuad, 0, sizeof(GameQuad));		// force the vertex buffer to be filled

	bD3D11Ready = true;

#ifdef PRINT_DEBUG_INFO
	{
		dprintf(_T("  * Initialisation complete: feature level %x%s, %s swap chain.\n"), (int)nFeatureLevel, bWarpDevice ? _T(" (WARP)") : _T(""), (nSwapEffect == DXGI_SWAP_EFFECT_DISCARD) ? _T("blt-model") : _T("flip-model"));
		dprintf(_T("    Emulation running in %i-bit mode.\n"), nVidImageDepth);
		if (nVidFullscreen) {
			dprintf(_T("    Running in fullscreen mode (%i x %i).\n"), nVidScrnWidth, nVidScrnHeight);
		} else {
			dprintf(_T("    Running in windowed mode.\n"));
		}
	}
#endif

	return 0;
}

static int d3d11Scale(RECT* pRect, int nWidth, int nHeight)
{
	if (nVidBlitterOpt[nVidSelect] & 0x0100) {
		return VidSoftFXScale(pRect, nWidth, nHeight);
	}

	return VidSScaleImage(pRect, nWidth, nHeight, bVidScanRotate);
}

// Copy (and convert/filter) the emulator image into the current game texture
static int d3d11UploadImage()
{
	if (pVidImage == NULL || pGameTexture[0] == NULL) {
		return 1;
	}

	// advance the ring of frames
	if (nGameTextures > 1) {
		nCurrentTexture = (nCurrentTexture + 1) % nGameTextures;
	}
	if (nFramesUploaded < 1000) {
		nFramesUploaded++;
	}

	ID3D11Texture2D* pTexture = pGameTexture[nCurrentTexture];
	D3D11_MAPPED_SUBRESOURCE ms;

	if (FAILED(pContext->Map(pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
		return 1;
	}

	unsigned char* pd = (unsigned char*)ms.pData;
	int nPitch = ms.RowPitch;
	int nWidth = nGameImageWidth, nHeight = nGameImageHeight;

	if (nPreScaleEffect) {
		nWidth *= nPreScaleZoom;
		nHeight *= nPreScaleZoom;

		if (nVidImageDepth == 32) {
			VidFilterApplyEffect(pd, nPitch);
		} else {
			VidFilterApplyEffect(pConvBuffer, nConvPitch);
		}
	}

	if (nVidImageDepth == 16) {
		// RGB565 -> XRGB8888
		unsigned char* ps = nPreScaleEffect ? pConvBuffer : (pVidImage + nVidImageLeft * nVidImageBPP);
		int nSrcPitch = nPreScaleEffect ? nConvPitch : nVidImagePitch;

		for (int y = 0; y < nHeight; y++) {
			UINT16* s = (UINT16*)(ps + y * nSrcPitch);
			UINT32* d = (UINT32*)(pd + y * nPitch);
			for (int x = 0; x < nWidth; x++) {
				UINT32 c = s[x];
				UINT32 r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
				d[x] = 0xFF000000 | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
			}
		}
	} else if (nPreScaleEffect == 0) {
		unsigned char* ps = pVidImage + nVidImageLeft * nVidImageBPP;
		int nLineSize = nWidth * 4;

		for (int y = 0; y < nHeight; y++) {
			memcpy(pd + y * nPitch, ps + y * nVidImagePitch, nLineSize);
		}
	}

	// Repeat the last column / line into the texture padding, so linear filtering at the edges is clean
	if (nWidth < nTextureWidth) {
		for (int y = 0; y < nHeight; y++) {
			UINT32* d = (UINT32*)(pd + y * nPitch);
			d[nWidth] = d[nWidth - 1];
		}
	}
	if (nHeight < nTextureHeight) {
		memcpy(pd + nHeight * nPitch, pd + (nHeight - 1) * nPitch, ((nWidth < nTextureWidth) ? nWidth + 1 : nWidth) * 4);
	}

	pContext->Unmap(pTexture, 0);

	bTextureValid = true;

	return 0;
}

// Work out where the image goes, and keep the view window & swap chain in step with it
static int d3d11UpdateGeometry()
{
	RECT rect;

	GetClientRect(hVidWnd, &rect);

	if (nVidFullscreen == 0) {
		rect.top += nMenuHeight;
	}

	if (rect.right <= rect.left || rect.bottom <= rect.top) {
		return 1;									// minimised
	}

	RECT View = rect;								// the area the view window covers

	if (bVidArcaderes && nVidFullscreen && !bChangedDisplayMode && bVidDX9WinFullscreen) {
		// arcade resolution in borderless mode: centre the image at 1:1
		Dest.left = (rect.right + rect.left) / 2 - nGameWidth / 2;
		Dest.right = Dest.left + nGameWidth;
		Dest.top = (rect.top + rect.bottom) / 2 - nGameHeight / 2;
		Dest.bottom = Dest.top + nGameHeight;
	} else {
		Dest = rect;
		d3d11Scale(&Dest, nGameWidth, nGameHeight);
	}

	if (Dest.right <= Dest.left || Dest.bottom <= Dest.top) {
		return 1;
	}

	if (nVidFullscreen) {
		Render = Dest;
	} else {
		// In a window, the view window only covers the game image (leaves the menu / bezel alone)
		View = Dest;
		Render.left = 0;
		Render.top = 0;
		Render.right = Dest.right - Dest.left;
		Render.bottom = Dest.bottom - Dest.top;
	}

	// Move the view window if needed
	{
		RECT Current;
		GetWindowRect(hD3D11View, &Current);
		MapWindowPoints(HWND_DESKTOP, hVidWnd, (LPPOINT)&Current, 2);

		if (memcmp(&Current, &View, sizeof(RECT)) != 0) {
			SetWindowPos(hD3D11View, HWND_BOTTOM, View.left, View.top, View.right - View.left, View.bottom - View.top, SWP_NOACTIVATE | SWP_NOREDRAW);
		}
	}

	if (d3d11ResizeSwapChain(View.right - View.left, View.bottom - View.top)) {
		return 1;
	}

	// The quad for the game image (vertex order: 0 = TL, 1 = TR, 2 = BL, 3 = BR of the game image)
	int nNewImageWidth = (nRotateGame & 1) ? (Dest.bottom - Dest.top) : (Dest.right - Dest.left);
	int nNewImageHeight = (nRotateGame & 1) ? (Dest.right - Dest.left) : (Dest.bottom - Dest.top);

	nImageWidth = nNewImageWidth;
	nImageHeight = nNewImageHeight;

	{
		int nWidth = nGameImageWidth, nHeight = nGameImageHeight;
		if (nPreScaleEffect) {
			nWidth *= nPreScaleZoom;
			nHeight *= nPreScaleZoom;
		}

		float u1 = (float)nWidth / nTextureWidth;
		float v1 = (float)nHeight / nTextureHeight;

		// screen positions (left/right = -1/1, top/bottom = 1/-1) for each corner of the game image
		float x[4], y[4];
		switch (nRotateGame & 3) {
			case 0:		// TL TR BL BR
				x[0] = -1; y[0] =  1;  x[1] =  1; y[1] =  1;  x[2] = -1; y[2] = -1;  x[3] =  1; y[3] = -1;
				break;
			case 1:		// rotated 90 degrees anti-clockwise
				x[0] = -1; y[0] = -1;  x[1] = -1; y[1] =  1;  x[2] =  1; y[2] = -1;  x[3] =  1; y[3] =  1;
				break;
			case 2:		// flipped (180 degrees)
				x[0] =  1; y[0] = -1;  x[1] = -1; y[1] = -1;  x[2] =  1; y[2] =  1;  x[3] = -1; y[3] =  1;
				break;
			default:	// rotated 90 degrees clockwise
				x[0] =  1; y[0] =  1;  x[1] =  1; y[1] = -1;  x[2] = -1; y[2] =  1;  x[3] = -1; y[3] = -1;
				break;
		}

		d3d11Vertex Quad[4];
		d3d11SetVertex(&Quad[0], x[0], y[0], 0.0f, 0.0f, 1.0f);
		d3d11SetVertex(&Quad[1], x[1], y[1], u1,   0.0f, 1.0f);
		d3d11SetVertex(&Quad[2], x[2], y[2], 0.0f, v1,   1.0f);
		d3d11SetVertex(&Quad[3], x[3], y[3], u1,   v1,   1.0f);

		if (memcmp(Quad, GameQuad, sizeof(Quad)) != 0) {
			memcpy(GameQuad, Quad, sizeof(Quad));
			d3d11UpdateBuffer(pGameVB, GameQuad, sizeof(GameQuad));
		}
	}

	return 0;
}

static void d3d11DrawGame(ID3D11ShaderResourceView* pSRV, float fAlpha)
{
	UINT nStride = sizeof(d3d11Vertex), nOffset = 0;

	if (fAlpha < 1.0f) {
		// motion blur: draw with a blended copy of the quad
		d3d11Vertex Quad[4];
		memcpy(Quad, GameQuad, sizeof(Quad));
		for (int i = 0; i < 4; i++) {
			Quad[i].a = fAlpha;
		}
		d3d11UpdateBuffer(pOSDVB, Quad, sizeof(Quad));
		pContext->IASetVertexBuffers(0, 1, &pOSDVB, &nStride, &nOffset);
		pContext->OMSetBlendState(pBlendAlpha, NULL, 0xFFFFFFFF);
	} else {
		pContext->IASetVertexBuffers(0, 1, &pGameVB, &nStride, &nOffset);
		pContext->OMSetBlendState(NULL, NULL, 0xFFFFFFFF);
	}

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pContext->IASetInputLayout(pBasicLayout);
	pContext->VSSetShader(pBasicVS, NULL, 0);
	pContext->PSSetShader(pBasicPS, NULL, 0);
	pContext->PSSetShaderResources(0, 1, &pSRV);
	pContext->PSSetSamplers(0, 1, bVidDX9Bilinear ? &pSamplerLinear : &pSamplerPoint);

	pContext->Draw(4, 0);
}

static int d3d11Render()
{
	if (d3d11UpdateGeometry()) {
		return 1;
	}

	float Black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	pContext->OMSetRenderTargets(1, &pRenderTarget, NULL);
	pContext->ClearRenderTargetView(pRenderTarget, Black);
	pContext->RSSetState(pRasterState);

	if (bTextureValid) {
		D3D11_VIEWPORT vp;
		vp.TopLeftX = (float)Render.left;
		vp.TopLeftY = (float)Render.top;
		vp.Width = (float)(Render.right - Render.left);
		vp.Height = (float)(Render.bottom - Render.top);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		pContext->RSSetViewports(1, &vp);

		if (pShader) {
			d3d11DrawShader();
		} else if (bVidMotionBlur && nGameTextures > 1 && nFramesUploaded > 1) {
			d3d11DrawGame(pGameSRV[(nCurrentTexture + nGameTextures - 1) % nGameTextures], 1.0f);
			d3d11DrawGame(pGameSRV[nCurrentTexture], 0.5f);
		} else {
			d3d11DrawGame(pGameSRV[nCurrentTexture], 1.0f);
		}

		// unbind the game texture from every slot a shader may have used
		ID3D11ShaderResourceView* NullSRV[16] = { NULL, };
		pContext->PSSetShaderResources(0, 16, NullSRV);
		pContext->VSSetShaderResources(0, 16, NullSRV);
	}

	// OSD (status, messages, chat, netplay edit box)
	VidSDisplayOSD(NULL, &Render, 0);

	return 0;
}

static void d3d11HandleDeviceLost(HRESULT hr)
{
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_HUNG) {
		if (!bDeviceLost) {
			bDeviceLost = true;
#ifdef PRINT_DEBUG_INFO
			dprintf(_T("  * Direct3D 11 device lost (0x%08X), re-initialising.\n"), (unsigned int)hr);
#endif
			PostMessage(NULL, WM_APP + 0, 0, 0);	// re-initialise video (same as POST_INITIALISE_MESSAGE)
		}
	}
}

// Run one frame and render the screen
static int d3d11Frame(bool bRedraw)	// bRedraw = 0
{
	if (!bD3D11Ready || pVidImage == NULL || pDevice == NULL) {
		return 1;
	}

	if (bDeviceLost) {
		return 1;
	}

	VidFrameCallback(bRedraw);						// Run emulation for 1 frame / render image

	if (pVidImage == NULL) {						// If a mode change was requested by game, pVidImage has been invalidated - time to leave.
		return 0;
	}

	d3d11UploadImage();

	return 0;
}

// Draw the image (+ OSD) and present it
static int d3d11Paint(int bValidate)
{
	if (!bD3D11Ready || pDevice == NULL || pSwapChain == NULL || bDeviceLost || bInPaint) {
		return 1;
	}

	bInPaint = true;

	// Nothing uploaded yet (e.g. a preview image painted straight after init): upload it now
	if (!bTextureValid && pVidImage) {
		d3d11UploadImage();
	}

	if (d3d11Render()) {
		bInPaint = false;
		return 1;
	}

	UINT nSyncInterval = (bVidVSync && !EmulatorAppIsFast()) ? 1 : 0;
	// VRR: present immediately, the display follows our own frame pacing
	UINT nPresentFlags = (nSyncInterval == 0 && (nSwapChainFlags & FBNEO_DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)) ? FBNEO_DXGI_PRESENT_ALLOW_TEARING : 0;
	HRESULT hr = pSwapChain->Present(nSyncInterval, nPresentFlags);

	bInPaint = false;

	if (FAILED(hr)) {
		d3d11HandleDeviceLost(hr);
		return 1;
	}

	if (bValidate & 1) {
		ValidateRect(hVidWnd, &Dest);
	}

	return 0;
}

static int d3d11GetSettings(InterfaceInfo* pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");
	const TCHAR* pszLevel = _T("9_1");

	switch (nFeatureLevel) {
		case D3D_FEATURE_LEVEL_11_0: pszLevel = _T("11_0"); break;
		case D3D_FEATURE_LEVEL_10_1: pszLevel = _T("10_1"); break;
		case D3D_FEATURE_LEVEL_10_0: pszLevel = _T("10_0"); break;
		case D3D_FEATURE_LEVEL_9_3:  pszLevel = _T("9_3");  break;
		case D3D_FEATURE_LEVEL_9_2:  pszLevel = _T("9_2");  break;
		default: break;
	}
	if (nFeatureLevel > D3D_FEATURE_LEVEL_11_0) {
		pszLevel = _T("11_0+");
	}

	_sntprintf(szString, MAX_PATH, _T("Using a Direct3D 11 %sdevice, feature level %s"), bWarpDevice ? _T("software (WARP) ") : _T(""), pszLevel);
	IntInfoAddStringModule(pInfo, szString);

	if (nSwapEffect == DXGI_SWAP_EFFECT_DISCARD) {
		IntInfoAddStringModule(pInfo, _T("Using a blt-model swap chain"));
	} else {
		IntInfoAddStringModule(pInfo, _T("Using a flip-model swap chain"));
	}

	if (nSwapChainFlags & FBNEO_DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) {
		IntInfoAddStringModule(pInfo, _T("Variable refresh rate (tearing) presents enabled"));
	} else if (bVidVRR) {
		IntInfoAddStringModule(pInfo, _T("Variable refresh rate requested, but not supported"));
	}

	if (nVidFullscreen) {
		if (bChangedDisplayMode) {
			_sntprintf(szString, MAX_PATH, _T("Fullscreen (%i x %i, display mode changed)"), nVidScrnWidth, nVidScrnHeight);
		} else {
			_sntprintf(szString, MAX_PATH, _T("Borderless fullscreen (%i x %i)"), nVidScrnWidth, nVidScrnHeight);
		}
		IntInfoAddStringModule(pInfo, szString);
	}

	if (bDrvOkay && nPreScaleEffect) {
		_sntprintf(szString, MAX_PATH, _T("Prescaling using %s (%ix zoom)"), VidSoftFXGetEffect(nPreScaleEffect), nPreScaleZoom);
		IntInfoAddStringModule(pInfo, szString);
	}

	if (pShader) {
		_sntprintf(szString, MAX_PATH, _T("Applying shader %s (%i pass%s, %i parameter%s)"), d3d11Widen(d3d11BaseName(pShader->szPath)).c_str(),
			(int)pShader->Passes.size(), (pShader->Passes.size() == 1) ? _T("") : _T("es"), (int)pShader->Params.size(), (pShader->Params.size() == 1) ? _T("") : _T("s"));
		IntInfoAddStringModule(pInfo, szString);
	}

	if (bVidDX9Bilinear) {
		IntInfoAddStringModule(pInfo, _T("Applying linear filter"));
	} else {
		IntInfoAddStringModule(pInfo, _T("Applying point filter"));
	}

	if (bVidMotionBlur) {
		IntInfoAddStringModule(pInfo, _T("Applying motion blur effect"));
	}

	return 0;
}

// ----------------------------------------------------------------------------
// Shader selection / parameters (used by the menus and the parameter dialog)

TCHAR szVidD3D11Shader[MAX_PATH] = _T("");
TCHAR szVidD3D11ShaderMRU[D3D11_SHADER_MRU_SIZE][MAX_PATH];

static void d3d11ShaderAddMRU(const TCHAR* szPath)
{
	if (szPath == NULL || szPath[0] == 0) {
		return;
	}

	int nFound = D3D11_SHADER_MRU_SIZE - 1;
	for (int i = 0; i < D3D11_SHADER_MRU_SIZE; i++) {
		if (_tcsicmp(szVidD3D11ShaderMRU[i], szPath) == 0) {
			nFound = i;
			break;
		}
	}
	for (int i = nFound; i > 0; i--) {
		_tcscpy(szVidD3D11ShaderMRU[i], szVidD3D11ShaderMRU[i - 1]);
	}
	_tcsncpy(szVidD3D11ShaderMRU[0], szPath, MAX_PATH - 1);
	szVidD3D11ShaderMRU[0][MAX_PATH - 1] = 0;
}

static void d3d11ShaderRedraw()
{
	if (bD3D11Ready && !bInPaint) {
		d3d11Paint(0);
	}
}

int VidD3D11SetShader(const TCHAR* szPath)
{
	if (szPath == NULL) {
		szPath = _T("");
	}

	_tcsncpy(szVidD3D11Shader, szPath, MAX_PATH - 1);
	szVidD3D11Shader[MAX_PATH - 1] = 0;
	d3d11ShaderAddMRU(szVidD3D11Shader);

	int nRet = 0;
	if (bD3D11Ready) {
		nRet = d3d11LoadShader();
		if (nRet) {
			szVidD3D11Shader[0] = 0;				// don't try again next time
		}
		d3d11ShaderRedraw();
	}

	return nRet;
}

int VidD3D11ShaderParamCount()
{
	if (!bD3D11Ready || pShader == NULL) {
		return -1;
	}
	return (int)pShader->Params.size();
}

int VidD3D11ShaderGetParam(int n, TCHAR* szDesc, int nDescSize, float* pValue, float* pMin, float* pMax, float* pStep, float* pDefault)
{
	if (pShader == NULL || n < 0 || n >= (int)pShader->Params.size()) {
		return 1;
	}

	const D3D11ShaderParam& p = pShader->Params[n];

	if (szDesc && nDescSize > 0) {
#if defined (UNICODE) || defined (_UNICODE)
		std::wstring w = d3d11Widen(p.szDesc.empty() ? p.szName : p.szDesc);
		_tcsncpy(szDesc, w.c_str(), nDescSize - 1);
#else
		strncpy(szDesc, (p.szDesc.empty() ? p.szName : p.szDesc).c_str(), nDescSize - 1);
#endif
		szDesc[nDescSize - 1] = 0;
	}
	if (pValue) *pValue = p.fValue;
	if (pMin) *pMin = p.fMin;
	if (pMax) *pMax = p.fMax;
	if (pStep) *pStep = p.fStep;
	if (pDefault) *pDefault = p.fDefault;

	return 0;
}

void VidD3D11ShaderSetParam(int n, float fValue)
{
	if (pShader == NULL || n < 0 || n >= (int)pShader->Params.size()) {
		return;
	}

	D3D11ShaderParam& p = pShader->Params[n];
	p.fValue = (fValue < p.fMin) ? p.fMin : (fValue > p.fMax) ? p.fMax : fValue;

	d3d11ShaderRedraw();
}

int VidD3D11ShaderSaveParams()
{
	return d3d11SaveParamValues();
}

int VidD3D11ShaderName(TCHAR* szName, int nSize)
{
	if (szName == NULL || nSize <= 0) {
		return 1;
	}
	szName[0] = 0;
	if (pShader == NULL) {
		return 1;
	}
#if defined (UNICODE) || defined (_UNICODE)
	std::wstring w = d3d11Widen(d3d11BaseName(pShader->szPath));
	_tcsncpy(szName, w.c_str(), nSize - 1);
#else
	strncpy(szName, d3d11BaseName(pShader->szPath).c_str(), nSize - 1);
#endif
	szName[nSize - 1] = 0;
	return 0;
}

// The Video Output plugin:
struct VidOut VidOutD3D11 = { d3d11Init, d3d11Exit, d3d11Frame, d3d11Paint, d3d11Scale, d3d11GetSettings, _T("Direct3D 11 video output") };

#else

// No Direct3D 11 headers: provide a stub that fails gracefully
static int d3d11StubInit()
{
	FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("This build doesn't include the Direct3D 11 blitter."));
	FBAPopupDisplay(PUF_TYPE_ERROR);
	return 1;
}
static int d3d11StubExit() { return 0; }
static int d3d11StubFrame(bool) { return 1; }
static int d3d11StubPaint(int) { return 1; }
static int d3d11StubScale(RECT*, int, int) { return 0; }
static int d3d11StubGetSettings(InterfaceInfo*) { return 0; }

struct VidOut VidOutD3D11 = { d3d11StubInit, d3d11StubExit, d3d11StubFrame, d3d11StubPaint, d3d11StubScale, d3d11StubGetSettings, _T("Direct3D 11 video output") };

TCHAR szVidD3D11Shader[MAX_PATH] = _T("");
TCHAR szVidD3D11ShaderMRU[D3D11_SHADER_MRU_SIZE][MAX_PATH];

int VidD3D11SetShader(const TCHAR* szPath) { _tcsncpy(szVidD3D11Shader, szPath ? szPath : _T(""), MAX_PATH - 1); return 1; }
int VidD3D11ShaderParamCount() { return -1; }
int VidD3D11ShaderGetParam(int, TCHAR*, int, float*, float*, float*, float*, float*) { return 1; }
void VidD3D11ShaderSetParam(int, float) { }
int VidD3D11ShaderSaveParams() { return 1; }
int VidD3D11ShaderName(TCHAR* szName, int nSize) { if (szName && nSize > 0) szName[0] = 0; return 1; }

#endif
