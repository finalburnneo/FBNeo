// Shader support for the Direct3D 11 blitter (platform independent parts)
//
//  - shader files (.cg, .hlsl, .fx)
//  - #include inlining and "#pragma parameter" (user changeable options) extraction
//  - a Cg -> HLSL (Shader Model 4) source translator, so Cg shaders (such as Hyllian's
//    common-shaders collection) can be compiled with D3DCompile
//  - name resolution for the uniforms / textures a shader pass declares

#ifndef VID_D3D11_SHADER_H
#define VID_D3D11_SHADER_H

// Note: include this before burner.h - the DirectX SDK's sal.h (included by burner.h) defines
// macros such as __in / __out / __null that break GCC's libstdc++ headers.
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Presets

enum {
	D3D11_SCALE_UNSET = 0,
	D3D11_SCALE_SOURCE,
	D3D11_SCALE_VIEWPORT,
	D3D11_SCALE_ABSOLUTE
};

enum {
	D3D11_WRAP_BORDER = 0,			// clamp_to_border (default, as per the Cg shader spec)
	D3D11_WRAP_EDGE,				// clamp_to_edge
	D3D11_WRAP_REPEAT,				// repeat
	D3D11_WRAP_MIRROR				// mirrored_repeat
};

struct D3D11ShaderPassDesc {
	std::string szPath;				// shader source
	int nFilter;					// -1 = not specified, 0 = point, 1 = linear
	bool bFloatFramebuffer;
	bool bSRGBFramebuffer;
	int nFrameCountMod;				// 0 = none
	int nScaleTypeX, nScaleTypeY;
	float fScaleX, fScaleY;
	int nWrap;
	std::string szAlias;

	D3D11ShaderPassDesc() : nFilter(-1), bFloatFramebuffer(false), bSRGBFramebuffer(false), nFrameCountMod(0),
		nScaleTypeX(D3D11_SCALE_UNSET), nScaleTypeY(D3D11_SCALE_UNSET), fScaleX(1.0f), fScaleY(1.0f), nWrap(D3D11_WRAP_BORDER) { }
};

struct D3D11ShaderLUT {
	std::string szName;
	std::string szPath;
	bool bLinear;
	bool bMipmap;
	int nWrap;

	D3D11ShaderLUT() : bLinear(true), bMipmap(false), nWrap(D3D11_WRAP_BORDER) { }
};

struct D3D11ShaderPreset {
	std::vector<D3D11ShaderPassDesc> Passes;
	std::vector<D3D11ShaderLUT> LUTs;
	std::vector<std::pair<std::string, std::string> > Values;		// every key = value pair (used for parameter values)
};

struct D3D11ShaderParam {
	std::string szName;
	std::string szDesc;
	float fDefault, fMin, fMax, fStep;
	float fValue;
};

// Reads a whole file; paths are UTF-8 (the caller converts them for the OS)
typedef bool (*D3D11ShaderReadFileFn)(const std::string& szPath, std::string& Data);

std::string D3D11ShaderDirName(const std::string& szPath);
std::string D3D11ShaderJoinPath(const std::string& szDir, const std::string& szRelative);
std::string D3D11ShaderExtension(const std::string& szPath);		// lower case, without the '.'

// Load a preset (.cgp), or wrap a single shader file in a one-pass preset
bool D3D11ShaderLoadPreset(const std::string& szPath, D3D11ShaderReadFileFn pRead, D3D11ShaderPreset* pPreset, std::string* pError);

// Inline #include "..." directives (recursively, relative to the including file)
bool D3D11ShaderInlineIncludes(const std::string& szPath, D3D11ShaderReadFileFn pRead, std::string& Source, std::string* pError);

// Work around a bug in D3DPreprocess(): a function-like macro whose name is also used as a plain
// identifier (e.g. "#define X(i) ..." and a variable X) is renamed, together with its calls
void D3D11ShaderFixMacroNames(std::string& Source);

// Add the "#pragma parameter" declarations found in Source to Params (duplicates are ignored)
void D3D11ShaderExtractParams(const std::string& Source, std::vector<D3D11ShaderParam>& Params);

// Look up a value for a parameter in the preset ("NAME = value")
bool D3D11ShaderPresetValue(const D3D11ShaderPreset& Preset, const std::string& szName, float* pValue);

// ----------------------------------------------------------------------------
// Cg -> HLSL translation
//
// Input is *preprocessed* Cg source (macros expanded, includes inlined).
// The output compiles as Shader Model 4 HLSL with entry points main_vertex / main_fragment.

struct D3D11CgTranslation {
	std::string szHLSL;
	bool bHasVertex;
	bool bHasFragment;
};

bool D3D11TranslateCg(const std::string& Source, D3D11CgTranslation* pOut, std::string* pError);

// ----------------------------------------------------------------------------
// Binding names used by a compiled pass

enum {
	D3D11_SRC_NONE = 0,
	D3D11_SRC_INPUT,				// the input of the current pass (output of the previous pass)
	D3D11_SRC_ORIG,					// the original frame
	D3D11_SRC_PREV,					// a previous original frame, index 1..7 (PREV = 1, PREV1 = 2, ...)
	D3D11_SRC_PASS,					// the output of pass index (0 based)
	D3D11_SRC_LUT					// lookup texture index
};

enum {
	D3D11_UNI_NONE = 0,
	D3D11_UNI_VIDEO_SIZE,
	D3D11_UNI_TEXTURE_SIZE,
	D3D11_UNI_OUTPUT_SIZE,
	D3D11_UNI_FRAME_COUNT,
	D3D11_UNI_FRAME_DIRECTION,
	D3D11_UNI_FRAME_ROTATION,
	D3D11_UNI_VIDEO_TIME,			// FBNeo DX9 HardFX: float2(frame, frame / 60)
	D3D11_UNI_PARAM					// a #pragma parameter (index into the parameter list)
};

struct D3D11ShaderBindContext {
	int nPass;													// the pass being bound
	const std::vector<std::string>* pAliases;					// alias of each pass ("" if none)
	const std::vector<D3D11ShaderLUT>* pLUTs;
	const std::vector<D3D11ShaderParam>* pParams;
};

// Texture resource name -> source
void D3D11ShaderResolveTexture(const std::string& szName, const D3D11ShaderBindContext& Ctx, int* pSource, int* pIndex);

// Sampler name -> filter (1 = linear, 0 = point, -1 = use the pass filter)
int D3D11ShaderResolveSampler(const std::string& szName, const D3D11ShaderBindContext& Ctx);

// Constant buffer variable name -> what to put there
void D3D11ShaderResolveUniform(const std::string& szName, const D3D11ShaderBindContext& Ctx, int* pUniform, int* pSource, int* pIndex);

#endif
