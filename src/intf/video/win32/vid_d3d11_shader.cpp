// Shader support for the Direct3D 11 blitter (platform independent parts)
// See vid_d3d11_shader.h

#include "vid_d3d11_shader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <set>
#include <map>

// ----------------------------------------------------------------------------
// String helpers

static std::string Trim(const std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && isspace((unsigned char)s[b])) b++;
	while (e > b && isspace((unsigned char)s[e - 1])) e--;
	return s.substr(b, e - b);
}

static std::string ToLower(const std::string& s)
{
	std::string r = s;
	for (size_t i = 0; i < r.size(); i++) {
		r[i] = (char)tolower((unsigned char)r[i]);
	}
	return r;
}

static bool StartsWith(const std::string& s, const char* p)
{
	return s.compare(0, strlen(p), p) == 0;
}

static bool IsNumber(const std::string& s)
{
	if (s.empty()) return false;
	for (size_t i = 0; i < s.size(); i++) {
		if (!isdigit((unsigned char)s[i])) return false;
	}
	return true;
}

static bool ParseBool(const std::string& s)
{
	std::string l = ToLower(Trim(s));
	return l == "true" || l == "1" || l == "yes" || l == "on";
}

static float ParseFloat(const std::string& s)
{
	// locale independent (the burner may have called setlocale())
	const char* p = s.c_str();
	while (isspace((unsigned char)*p)) p++;

	double sign = 1.0;
	if (*p == '-') { sign = -1.0; p++; } else if (*p == '+') { p++; }

	double v = 0.0;
	while (isdigit((unsigned char)*p)) { v = v * 10.0 + (*p - '0'); p++; }
	if (*p == '.') {
		double f = 0.1;
		p++;
		while (isdigit((unsigned char)*p)) { v += (*p - '0') * f; f *= 0.1; p++; }
	}
	if (*p == 'e' || *p == 'E') {
		p++;
		int esign = 1, e = 0;
		if (*p == '-') { esign = -1; p++; } else if (*p == '+') { p++; }
		while (isdigit((unsigned char)*p)) { e = e * 10 + (*p - '0'); p++; }
		double m = 1.0;
		for (int i = 0; i < e; i++) m *= 10.0;
		v = (esign > 0) ? v * m : v / m;
	}

	return (float)(sign * v);
}

std::string D3D11ShaderDirName(const std::string& szPath)
{
	size_t n = szPath.find_last_of("/\\");
	if (n == std::string::npos) {
		return "";
	}
	return szPath.substr(0, n);
}

std::string D3D11ShaderJoinPath(const std::string& szDir, const std::string& szRelative)
{
	if (szRelative.empty()) {
		return szDir;
	}
	if (szRelative[0] == '/' || szRelative[0] == '\\' || (szRelative.size() > 1 && szRelative[1] == ':')) {
		return szRelative;				// absolute
	}
	if (szDir.empty()) {
		return szRelative;
	}

	// join, and collapse "." / ".." (presets and includes are full of "../../"), so paths stay
	// short and readable in error messages
#ifdef _WIN32
	const char cSep = '\\';
#else
	const char cSep = '/';
#endif
	std::string szFull = szDir + "/" + szRelative;

	std::string szPrefix;
	size_t nStart = 0;
	if (szFull.size() > 1 && szFull[1] == ':') {
		nStart = 2;										// drive letter
	} else if (szFull.size() > 1 && (szFull[0] == '\\' || szFull[0] == '/') && (szFull[1] == '\\' || szFull[1] == '/')) {
		nStart = 2;										// UNC (\\server\share)
	}
	szPrefix = szFull.substr(0, nStart);
	bool bAbsolute = nStart < szFull.size() && (szFull[nStart] == '/' || szFull[nStart] == '\\');

	std::vector<std::string> Parts;
	size_t i = nStart;
	while (i <= szFull.size()) {
		size_t e = szFull.find_first_of("/\\", i);
		if (e == std::string::npos) e = szFull.size();
		std::string s = szFull.substr(i, e - i);
		if (s.empty() || s == ".") {
			// skip
		} else if (s == ".." && !Parts.empty() && Parts.back() != "..") {
			Parts.pop_back();
		} else if (s == ".." && bAbsolute) {
			// can't go above the root
		} else {
			Parts.push_back(s);
		}
		i = e + 1;
	}

	std::string szOut = szPrefix;
	if (bAbsolute) szOut += cSep;
	for (size_t p = 0; p < Parts.size(); p++) {
		if (p) szOut += cSep;
		szOut += Parts[p];
	}
	return szOut;
}

std::string D3D11ShaderExtension(const std::string& szPath)
{
	size_t nDot = szPath.find_last_of('.');
	size_t nSlash = szPath.find_last_of("/\\");
	if (nDot == std::string::npos || (nSlash != std::string::npos && nSlash > nDot)) {
		return "";
	}
	return ToLower(szPath.substr(nDot + 1));
}

// ----------------------------------------------------------------------------
// Presets

static int ParseScaleType(const std::string& s)
{
	std::string l = ToLower(Trim(s));
	if (l == "source")   return D3D11_SCALE_SOURCE;
	if (l == "viewport") return D3D11_SCALE_VIEWPORT;
	if (l == "absolute") return D3D11_SCALE_ABSOLUTE;
	return D3D11_SCALE_UNSET;
}

static int ParseWrap(const std::string& s)
{
	std::string l = ToLower(Trim(s));
	if (l == "clamp_to_edge")   return D3D11_WRAP_EDGE;
	if (l == "repeat")          return D3D11_WRAP_REPEAT;
	if (l == "mirrored_repeat") return D3D11_WRAP_MIRROR;
	return D3D11_WRAP_BORDER;
}

static const std::string* FindValue(const D3D11ShaderPreset& Preset, const std::string& szKey)
{
	for (size_t i = Preset.Values.size(); i > 0; i--) {
		if (Preset.Values[i - 1].first == szKey) {
			return &Preset.Values[i - 1].second;
		}
	}
	return NULL;
}

static std::string Key(const char* szBase, int n)
{
	char szBuf[64];
	sprintf(szBuf, "%s%d", szBase, n);
	return szBuf;
}

bool D3D11ShaderPresetValue(const D3D11ShaderPreset& Preset, const std::string& szName, float* pValue)
{
	const std::string* pValueString = FindValue(Preset, szName);
	if (pValueString == NULL) {
		return false;
	}
	*pValue = ParseFloat(*pValueString);
	return true;
}

bool D3D11ShaderLoadPreset(const std::string& szPath, D3D11ShaderReadFileFn pRead, D3D11ShaderPreset* pPreset, std::string* pError)
{
	*pPreset = D3D11ShaderPreset();

	std::string szExt = D3D11ShaderExtension(szPath);

	if (szExt != "cgp") {
		// A single shader file
		D3D11ShaderPassDesc Pass;
		Pass.szPath = szPath;
		pPreset->Passes.push_back(Pass);
		return true;
	}

	std::string Data;
	if (!pRead(szPath, Data)) {
		if (pError) *pError = "Couldn't read " + szPath;
		return false;
	}

	// key = value lines; '#' starts a comment (outside quotes)
	size_t nPos = 0;
	while (nPos < Data.size()) {
		size_t nEnd = Data.find_first_of("\r\n", nPos);
		if (nEnd == std::string::npos) nEnd = Data.size();
		std::string Line = Data.substr(nPos, nEnd - nPos);
		nPos = nEnd + 1;

		bool bQuote = false;
		for (size_t i = 0; i < Line.size(); i++) {
			if (Line[i] == '"') bQuote = !bQuote;
			if (Line[i] == '#' && !bQuote) {
				Line = Line.substr(0, i);
				break;
			}
		}

		size_t nEq = Line.find('=');
		if (nEq == std::string::npos) continue;

		std::string szKey = Trim(Line.substr(0, nEq));
		std::string szValue = Trim(Line.substr(nEq + 1));
		if (!szValue.empty() && szValue[0] == '"') {
			size_t q = szValue.find('"', 1);
			szValue = szValue.substr(1, (q == std::string::npos) ? std::string::npos : q - 1);
		} else {
			// like RetroArch: an unquoted value ends at the first whitespace (some presets put comments after it)
			size_t w = szValue.find_first_of(" \t");
			if (w != std::string::npos) szValue = szValue.substr(0, w);
		}
		if (!szKey.empty()) {
			pPreset->Values.push_back(std::make_pair(szKey, szValue));
		}
	}

	const std::string* pShaders = FindValue(*pPreset, "shaders");
	int nShaders = pShaders ? atoi(pShaders->c_str()) : 0;
	if (nShaders <= 0) {
		if (pError) *pError = "Preset has no shaders";
		return false;
	}

	std::string szDir = D3D11ShaderDirName(szPath);

	for (int i = 0; i < nShaders; i++) {
		D3D11ShaderPassDesc Pass;
		const std::string* v;

		v = FindValue(*pPreset, Key("shader", i));
		if (v == NULL) {
			if (pError) *pError = "Preset is missing " + Key("shader", i);
			return false;
		}
		Pass.szPath = D3D11ShaderJoinPath(szDir, *v);

		if ((v = FindValue(*pPreset, Key("filter_linear", i))) != NULL) Pass.nFilter = ParseBool(*v) ? 1 : 0;
		if ((v = FindValue(*pPreset, Key("float_framebuffer", i))) != NULL) Pass.bFloatFramebuffer = ParseBool(*v);
		if ((v = FindValue(*pPreset, Key("srgb_framebuffer", i))) != NULL) Pass.bSRGBFramebuffer = ParseBool(*v);
		if ((v = FindValue(*pPreset, Key("frame_count_mod", i))) != NULL) Pass.nFrameCountMod = atoi(v->c_str());
		if ((v = FindValue(*pPreset, Key("wrap_mode", i))) != NULL) Pass.nWrap = ParseWrap(*v);
		if ((v = FindValue(*pPreset, Key("alias", i))) != NULL) Pass.szAlias = *v;

		// scale type: scale_typeN overrides scale_type_xN / scale_type_yN
		if ((v = FindValue(*pPreset, Key("scale_type_x", i))) != NULL) Pass.nScaleTypeX = ParseScaleType(*v);
		if ((v = FindValue(*pPreset, Key("scale_type_y", i))) != NULL) Pass.nScaleTypeY = ParseScaleType(*v);
		if ((v = FindValue(*pPreset, Key("scale_type", i))) != NULL) Pass.nScaleTypeX = Pass.nScaleTypeY = ParseScaleType(*v);

		bool bScaleX = false, bScaleY = false;
		if ((v = FindValue(*pPreset, Key("scale_x", i))) != NULL) { Pass.fScaleX = ParseFloat(*v); bScaleX = true; }
		if ((v = FindValue(*pPreset, Key("scale_y", i))) != NULL) { Pass.fScaleY = ParseFloat(*v); bScaleY = true; }
		if ((v = FindValue(*pPreset, Key("scale", i))) != NULL) { Pass.fScaleX = Pass.fScaleY = ParseFloat(*v); bScaleX = bScaleY = true; }

		// A scale without a type means "source"; a type on only one axis means the other is source 1.0
		if ((bScaleX || bScaleY) && Pass.nScaleTypeX == D3D11_SCALE_UNSET && Pass.nScaleTypeY == D3D11_SCALE_UNSET) {
			Pass.nScaleTypeX = Pass.nScaleTypeY = D3D11_SCALE_SOURCE;
		}
		if (Pass.nScaleTypeX != D3D11_SCALE_UNSET && Pass.nScaleTypeY == D3D11_SCALE_UNSET) {
			Pass.nScaleTypeY = D3D11_SCALE_SOURCE;
			if (!bScaleY) Pass.fScaleY = 1.0f;
		}
		if (Pass.nScaleTypeY != D3D11_SCALE_UNSET && Pass.nScaleTypeX == D3D11_SCALE_UNSET) {
			Pass.nScaleTypeX = D3D11_SCALE_SOURCE;
			if (!bScaleX) Pass.fScaleX = 1.0f;
		}

		pPreset->Passes.push_back(Pass);
	}

	// Lookup textures
	const std::string* pTextures = FindValue(*pPreset, "textures");
	if (pTextures) {
		std::string szList = *pTextures;
		size_t p = 0;
		while (p <= szList.size()) {
			size_t e = szList.find(';', p);
			if (e == std::string::npos) e = szList.size();
			std::string szName = Trim(szList.substr(p, e - p));
			p = e + 1;
			if (szName.empty()) continue;

			D3D11ShaderLUT LUT;
			LUT.szName = szName;
			const std::string* v = FindValue(*pPreset, szName);
			if (v == NULL) {
				if (pError) *pError = "Preset is missing the path of texture " + szName;
				return false;
			}
			LUT.szPath = D3D11ShaderJoinPath(szDir, *v);
			if ((v = FindValue(*pPreset, szName + "_linear")) != NULL) LUT.bLinear = ParseBool(*v);
			if ((v = FindValue(*pPreset, szName + "_wrap_mode")) != NULL) LUT.nWrap = ParseWrap(*v);
			if ((v = FindValue(*pPreset, szName + "_mipmap")) != NULL) LUT.bMipmap = ParseBool(*v);
			pPreset->LUTs.push_back(LUT);
		}
	}

	return true;
}

// ----------------------------------------------------------------------------
// Includes and parameters

static bool InlineIncludes(const std::string& szPath, D3D11ShaderReadFileFn pRead, std::string& Out, std::string* pError, int nDepth)
{
	if (nDepth > 16) {
		if (pError) *pError = "#include nesting too deep in " + szPath;
		return false;
	}

	std::string Data;
	if (!pRead(szPath, Data)) {
		if (pError) *pError = "Couldn't read " + szPath;
		return false;
	}

	std::string szDir = D3D11ShaderDirName(szPath);
	size_t nPos = 0;

	while (nPos < Data.size()) {
		size_t nEnd = Data.find('\n', nPos);
		if (nEnd == std::string::npos) nEnd = Data.size();
		std::string Line = Data.substr(nPos, nEnd - nPos);
		nPos = nEnd + 1;

		std::string t = Trim(Line);
		if (t.size() > 1 && t[0] == '#') {
			std::string d = Trim(t.substr(1));
			if (StartsWith(d, "include")) {
				size_t q1 = d.find_first_of("\"<");
				size_t q2 = (q1 != std::string::npos) ? d.find_first_of("\">", q1 + 1) : std::string::npos;
				if (q1 != std::string::npos && q2 != std::string::npos) {
					std::string szInclude = D3D11ShaderJoinPath(szDir, d.substr(q1 + 1, q2 - q1 - 1));
					std::string Included;
					if (!InlineIncludes(szInclude, pRead, Included, pError, nDepth + 1)) {
						return false;
					}
					Out += Included;
					Out += "\n";
					continue;
				}
			}
		}

		Out += Line;
		Out += "\n";
	}

	return true;
}

bool D3D11ShaderInlineIncludes(const std::string& szPath, D3D11ShaderReadFileFn pRead, std::string& Source, std::string* pError)
{
	Source.clear();
	return InlineIncludes(szPath, pRead, Source, pError, 0);
}

// ----------------------------------------------------------------------------
// D3DPreprocess() workaround (see the header)

namespace {

struct PPIdent {
	size_t nPos, nLen;
	size_t nLineStart;			// start of the line (for directives)
	bool bCall;					// followed by '('
};

static bool PPIdentStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool PPIdentChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

}

void D3D11ShaderFixMacroNames(std::string& Src)
{
	// Collect the identifiers (outside comments, strings and numbers)
	std::vector<PPIdent> Idents;
	size_t i = 0, n = Src.size(), nLineStart = 0;

	while (i < n) {
		char c = Src[i];
		if (c == '\n') { nLineStart = i + 1; i++; continue; }
		if (c == '/' && i + 1 < n && Src[i + 1] == '/') { while (i < n && Src[i] != '\n') i++; continue; }
		if (c == '/' && i + 1 < n && Src[i + 1] == '*') {
			size_t e = Src.find("*/", i + 2);
			e = (e == std::string::npos) ? n : e + 2;
			for (size_t k = i; k < e; k++) if (Src[k] == '\n') nLineStart = k + 1;
			i = e;
			continue;
		}
		if (c == '"') { i++; while (i < n && Src[i] != '"' && Src[i] != '\n') { if (Src[i] == '\\') i++; i++; } i++; continue; }
		if (isdigit((unsigned char)c)) { while (i < n && (PPIdentChar(Src[i]) || Src[i] == '.')) i++; continue; }
		if (PPIdentStart(c)) {
			PPIdent id;
			id.nPos = i;
			while (i < n && PPIdentChar(Src[i])) i++;
			id.nLen = i - id.nPos;
			id.nLineStart = nLineStart;
			size_t k = i;
			while (k < n && (Src[k] == ' ' || Src[k] == '\t' || Src[k] == '\r' || Src[k] == '\n')) k++;
			id.bCall = (k < n && Src[k] == '(');
			Idents.push_back(id);
			continue;
		}
		i++;
	}

	// Function-like macros: "#define NAME(" with no space before the '('
	struct Define { size_t nNameIdent; std::set<std::string> Params; size_t nLineEnd; };
	std::map<std::string, int> FnMacros;					// name -> 1
	std::vector<Define> Defines;							// every #define, for parameter scoping
	for (size_t k = 0; k + 1 < Idents.size(); k++) {
		const PPIdent& d = Idents[k];
		if (Src.compare(d.nPos, d.nLen, "define") != 0) continue;
		// must be "#  define" at the start of a line
		size_t h = d.nLineStart;
		while (h < d.nPos && (Src[h] == ' ' || Src[h] == '\t')) h++;
		if (h >= d.nPos || Src[h] != '#') continue;
		size_t h2 = h + 1;
		while (h2 < d.nPos && (Src[h2] == ' ' || Src[h2] == '\t')) h2++;
		if (h2 != d.nPos) continue;

		const PPIdent& nm = Idents[k + 1];
		Define def;
		def.nNameIdent = k + 1;
		// end of the directive (with line continuations)
		size_t e = nm.nPos;
		while (e < n && Src[e] != '\n') { if (Src[e] == '\\' && e + 1 < n && Src[e + 1] == '\n') e++; e++; }
		def.nLineEnd = e;
		std::string szName = Src.substr(nm.nPos, nm.nLen);
		if (nm.nPos + nm.nLen < n && Src[nm.nPos + nm.nLen] == '(') {
			FnMacros[szName] = 1;
			size_t p = nm.nPos + nm.nLen + 1;
			size_t q = Src.find(')', p);
			if (q != std::string::npos && q < e) {
				std::string szParams = Src.substr(p, q - p);
				size_t a = 0;
				while (a < szParams.size()) {
					while (a < szParams.size() && !PPIdentStart(szParams[a])) a++;
					size_t b = a;
					while (b < szParams.size() && PPIdentChar(szParams[b])) b++;
					if (b > a) def.Params.insert(szParams.substr(a, b - a));
					a = b;
				}
			}
		}
		Defines.push_back(def);
	}

	if (FnMacros.empty()) {
		return;
	}

	// Which ones are also used as plain identifiers?
	std::set<size_t> NameIdents;
	for (size_t d = 0; d < Defines.size(); d++) NameIdents.insert(Defines[d].nNameIdent);

	std::set<std::string> Ambiguous;
	for (size_t k = 0; k < Idents.size(); k++) {
		const PPIdent& id = Idents[k];
		if (id.bCall || NameIdents.count(k)) continue;
		std::string szName = Src.substr(id.nPos, id.nLen);
		if (!FnMacros.count(szName)) continue;

		// "#ifdef X", "#ifndef X", "#undef X", "defined X", "defined(X)" refer to the macro
		bool bDirective = false;
		if (k > 0) {
			const PPIdent& prev = Idents[k - 1];
			std::string szPrev = Src.substr(prev.nPos, prev.nLen);
			if (szPrev == "ifdef" || szPrev == "ifndef" || szPrev == "undef" || szPrev == "defined") bDirective = true;
		}
		if (bDirective) continue;

		// a parameter of a #define with the same name doesn't count
		bool bParam = false;
		for (size_t d = 0; d < Defines.size() && !bParam; d++) {
			size_t nStart = Idents[Defines[d].nNameIdent].nPos;
			if (id.nPos > nStart && id.nPos < Defines[d].nLineEnd && Defines[d].Params.count(szName)) bParam = true;
		}
		if (bParam) continue;

		Ambiguous.insert(szName);
	}

	if (Ambiguous.empty()) {
		return;
	}

	// Rename the macro and its uses as a macro (back to front, so positions stay valid)
	for (size_t k = Idents.size(); k > 0; k--) {
		const PPIdent& id = Idents[k - 1];
		std::string szName = Src.substr(id.nPos, id.nLen);
		if (!Ambiguous.count(szName)) continue;

		bool bRename = NameIdents.count(k - 1) > 0 || id.bCall;
		if (!bRename && k > 1) {
			const PPIdent& prev = Idents[k - 2];
			std::string szPrev = Src.substr(prev.nPos, prev.nLen);
			if (szPrev == "ifdef" || szPrev == "ifndef" || szPrev == "undef" || szPrev == "defined") bRename = true;
		}
		if (!bRename) continue;

		// not a parameter of an enclosing #define
		bool bParam = false;
		for (size_t d = 0; d < Defines.size() && !bParam; d++) {
			size_t nStart = Idents[Defines[d].nNameIdent].nPos;
			if (id.nPos > nStart && id.nPos < Defines[d].nLineEnd && Defines[d].Params.count(szName)) bParam = true;
		}
		if (bParam) continue;

		Src.replace(id.nPos, id.nLen, "fbneo_fn_" + szName);
	}
}

void D3D11ShaderExtractParams(const std::string& Source, std::vector<D3D11ShaderParam>& Params)
{
	size_t nPos = 0;

	while ((nPos = Source.find("#pragma", nPos)) != std::string::npos) {
		size_t nEnd = Source.find('\n', nPos);
		if (nEnd == std::string::npos) nEnd = Source.size();
		std::string Line = Source.substr(nPos + 7, nEnd - nPos - 7);
		nPos = nEnd;

		Line = Trim(Line);
		if (!StartsWith(Line, "parameter")) continue;
		Line = Trim(Line.substr(9));

		// NAME "Description" default min max [step]
		size_t n = 0;
		while (n < Line.size() && (isalnum((unsigned char)Line[n]) || Line[n] == '_')) n++;
		if (n == 0) continue;

		D3D11ShaderParam Param;
		Param.szName = Line.substr(0, n);

		size_t q1 = Line.find('"', n);
		size_t q2 = (q1 != std::string::npos) ? Line.find('"', q1 + 1) : std::string::npos;
		if (q1 == std::string::npos || q2 == std::string::npos) continue;
		Param.szDesc = Line.substr(q1 + 1, q2 - q1 - 1);

		std::string Rest = Line.substr(q2 + 1);
		float v[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
		int nValues = 0;
		size_t p = 0;
		while (nValues < 4) {
			while (p < Rest.size() && isspace((unsigned char)Rest[p])) p++;
			if (p >= Rest.size()) break;
			size_t e = p;
			while (e < Rest.size() && !isspace((unsigned char)Rest[e])) e++;
			v[nValues++] = ParseFloat(Rest.substr(p, e - p));
			p = e;
		}
		if (nValues < 3) continue;

		Param.fDefault = v[0];
		Param.fMin = v[1];
		Param.fMax = v[2];
		Param.fStep = (nValues >= 4 && v[3] > 0.0f) ? v[3] : 0.0f;
		if (Param.fMax < Param.fMin) {
			float t = Param.fMax; Param.fMax = Param.fMin; Param.fMin = t;
		}
		if (Param.fStep <= 0.0f) {
			Param.fStep = (Param.fMax - Param.fMin) / 100.0f;
			if (Param.fStep <= 0.0f) Param.fStep = 0.01f;
		}
		Param.fValue = Param.fDefault;

		bool bDuplicate = false;
		for (size_t i = 0; i < Params.size(); i++) {
			if (Params[i].szName == Param.szName) {
				bDuplicate = true;
				break;
			}
		}
		if (!bDuplicate) {
			Params.push_back(Param);
		}
	}
}

// ----------------------------------------------------------------------------
// Cg -> HLSL translation

namespace {

enum { T_IDENT, T_NUMBER, T_PUNCT, T_STRING, T_PREPROC };

struct Token {
	int nType;
	std::string szText;			// may be changed / emptied by the translator
	std::string szSpace;		// whitespace and comments before the token
	std::string szBefore;		// text inserted before the token
	std::string szAfter;		// text inserted after the token
};

struct Member {
	std::string szType;
	std::string szName;
	std::string szArray;		// "[n]" or ""
	std::string szSemantic;		// upper case, "" if none
	bool bSampler;
	size_t nFirst, nLast;		// token range of the declaration statement (incl. ';')
	size_t nSemanticColon;		// token index of ':' (0 if none)
	size_t nSemantic;			// token index of the semantic
};

struct Struct {
	std::string szName;
	std::vector<Member> Members;
	size_t nStart, nOpen, nClose, nEnd;		// "struct", "{", "}", ";"
};

struct Param {
	bool bUniform, bIn, bOut;
	std::string szType;
	std::string szName;
	std::string szArray;
	std::string szSemantic;
	size_t nFirst, nLast;		// token range (inclusive)
	size_t nSemantic;			// token index of the semantic (0 if none)
	size_t nType;				// token index of the type
};

struct GlobalDecl {
	size_t nFirst, nLast;
	std::vector<std::string> Names;
};

struct Function {
	std::string szName;
	size_t nReturnType;			// token index of the return type
	size_t nName, nOpen, nClose, nBodyOpen, nBodyClose;
	size_t nSemantic;			// token index of the return semantic (0 if none)
	std::vector<Param> Params;
};

class Translator {
public:
	std::vector<Token> Toks;
	std::vector<Struct> Structs;
	std::vector<Function> Functions;
	std::vector<GlobalDecl> Decls;
	std::set<std::string> GlobalNames;
	std::string szError;

	bool Tokenize(const std::string& Src);
	size_t Match(size_t nOpen);
	void Scan();
	bool ParseMembers(Struct& s);
	void ParseParams(Function& f);
	std::string Emit(size_t nFirst, size_t nLast);		// inclusive range, applying rewrites
	std::string EmitPlain(size_t nFirst, size_t nLast);
	const Struct* FindStruct(const std::string& szName) const;
	Function* FindFunction(const char* szName);

	// rewrite state
	std::set<std::string> GlobalTextures;
	std::set<std::string> Constructors;
	std::set<std::string> MemberNames;			// all struct member names
	std::set<std::string> UserFunctions;		// all function names defined by the shader
	std::set<std::string> ParamTextures;		// sampler parameters of helper functions (they get a SamplerState parameter too)
	std::map<std::string, std::set<size_t> > SamplerParamFuncs;	// helper function -> argument positions that are samplers
};

static bool IsIdentStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool IsIdentChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

bool Translator::Tokenize(const std::string& Src)
{
	size_t i = 0, n = Src.size();
	std::string Space;
	bool bLineStart = true;

	while (i < n) {
		char c = Src[i];

		// whitespace
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
			if (c == '\n') bLineStart = true;
			Space += c;
			i++;
			continue;
		}
		// comments
		if (c == '/' && i + 1 < n && Src[i + 1] == '/') {
			size_t e = Src.find('\n', i);
			if (e == std::string::npos) e = n;
			Space += Src.substr(i, e - i);
			i = e;
			continue;
		}
		if (c == '/' && i + 1 < n && Src[i + 1] == '*') {
			size_t e = Src.find("*/", i + 2);
			e = (e == std::string::npos) ? n : e + 2;
			Space += Src.substr(i, e - i);
			i = e;
			continue;
		}

		Token t;
		t.szSpace = Space;
		Space.clear();

		if (c == '#' && bLineStart) {
			// the source is already preprocessed: what's left (#line, #pragma) isn't needed
			size_t e = i;
			while (e < n) {
				if (Src[e] == '\n') {
					if (e > i && Src[e - 1] == '\\') { e++; continue; }
					break;
				}
				e++;
			}
			i = e;
			Space = t.szSpace;
			continue;
		} else if (IsIdentStart(c)) {
			size_t e = i;
			while (e < n && IsIdentChar(Src[e])) e++;
			t.nType = T_IDENT;
			t.szText = Src.substr(i, e - i);
			i = e;
		} else if (isdigit((unsigned char)c) || (c == '.' && i + 1 < n && isdigit((unsigned char)Src[i + 1]))) {
			size_t e = i;
			if (c == '0' && e + 1 < n && (Src[e + 1] == 'x' || Src[e + 1] == 'X')) {
				e += 2;
				while (e < n && isxdigit((unsigned char)Src[e])) e++;
			} else {
				while (e < n && (isdigit((unsigned char)Src[e]) || Src[e] == '.')) e++;
				if (e < n && (Src[e] == 'e' || Src[e] == 'E')) {
					e++;
					if (e < n && (Src[e] == '+' || Src[e] == '-')) e++;
					while (e < n && isdigit((unsigned char)Src[e])) e++;
				}
			}
			while (e < n && (Src[e] == 'f' || Src[e] == 'F' || Src[e] == 'h' || Src[e] == 'H' || Src[e] == 'u' || Src[e] == 'U' || Src[e] == 'l' || Src[e] == 'L')) e++;
			t.nType = T_NUMBER;
			t.szText = Src.substr(i, e - i);
			i = e;
		} else if (c == '"') {
			size_t e = i + 1;
			while (e < n && Src[e] != '"') {
				if (Src[e] == '\\') e++;
				e++;
			}
			e = (e < n) ? e + 1 : n;
			t.nType = T_STRING;
			t.szText = Src.substr(i, e - i);
			i = e;
		} else {
			static const char* Ops[] = { "<<=", ">>=", "++", "--", "&&", "||", "==", "!=", "<=", ">=", "<<", ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "->", "::", NULL };
			t.nType = T_PUNCT;
			t.szText = std::string(1, c);
			for (int o = 0; Ops[o]; o++) {
				size_t l = strlen(Ops[o]);
				if (Src.compare(i, l, Ops[o]) == 0) {
					t.szText = Ops[o];
					break;
				}
			}
			i += t.szText.size();
		}

		bLineStart = false;
		Toks.push_back(t);
	}

	// sentinel carrying the trailing whitespace
	Token t;
	t.nType = T_PUNCT;
	t.szSpace = Space;
	Toks.push_back(t);

	return true;
}

// Index of the bracket matching Toks[nOpen] (or the sentinel)
size_t Translator::Match(size_t nOpen)
{
	const std::string& o = Toks[nOpen].szText;
	const char* c = (o == "(") ? ")" : (o == "{") ? "}" : (o == "[") ? "]" : NULL;
	if (c == NULL) return nOpen;

	int nDepth = 0;
	for (size_t i = nOpen; i < Toks.size() - 1; i++) {
		if (Toks[i].nType != T_PUNCT) continue;
		if (Toks[i].szText == o) nDepth++;
		else if (Toks[i].szText == c) {
			if (--nDepth == 0) return i;
		}
	}
	return Toks.size() - 1;
}

static bool IsSamplerType(const std::string& s)
{
	return s == "sampler2D" || s == "samplerRECT" || s == "sampler1D" || s == "sampler3D" || s == "samplerCUBE" || s == "sampler";
}

static bool IsQualifier(const std::string& s)
{
	return s == "uniform" || s == "const" || s == "static" || s == "in" || s == "out" || s == "inout" || s == "linear" ||
		s == "centroid" || s == "nointerpolation" || s == "noperspective" || s == "row_major" || s == "column_major" || s == "varying";
}

static std::string Upper(const std::string& s)
{
	std::string r = s;
	for (size_t i = 0; i < r.size(); i++) r[i] = (char)toupper((unsigned char)r[i]);
	return r;
}

bool Translator::ParseMembers(Struct& s)
{
	size_t i = s.nOpen + 1;

	while (i < s.nClose) {
		size_t nFirst = i;
		size_t nEnd = i;
		while (nEnd < s.nClose && Toks[nEnd].szText != ";") {
			if (Toks[nEnd].szText == "(" || Toks[nEnd].szText == "{" || Toks[nEnd].szText == "[") nEnd = Match(nEnd);
			nEnd++;
		}
		if (nEnd >= s.nClose) break;

		// qualifiers, type, then declarators
		size_t t = nFirst;
		while (t < nEnd && Toks[t].nType == T_IDENT && IsQualifier(Toks[t].szText)) t++;
		if (t < nEnd && Toks[t].nType == T_IDENT) {
			std::string szType = Toks[t].szText;
			size_t d = t + 1;
			while (d < nEnd) {
				Member m;
				m.szType = szType;
				m.bSampler = IsSamplerType(szType);
				m.nFirst = nFirst;
				m.nLast = nEnd;
				m.nSemanticColon = 0;
				m.nSemantic = 0;
				if (Toks[d].nType != T_IDENT) break;
				m.szName = Toks[d].szText;
				d++;
				if (d < nEnd && Toks[d].szText == "[") {
					size_t c = Match(d);
					m.szArray = EmitPlain(d, c);
					d = c + 1;
				}
				if (d < nEnd && Toks[d].szText == ":") {
					m.nSemanticColon = d;
					if (d + 1 < nEnd) {
						m.nSemantic = d + 1;
						m.szSemantic = Upper(Toks[d + 1].szText);
					}
					d += 2;
				}
				s.Members.push_back(m);
				while (d < nEnd && Toks[d].szText != ",") d++;
				if (d < nEnd) d++;
			}
		}

		i = nEnd + 1;
	}

	return true;
}

void Translator::ParseParams(Function& f)
{
	size_t i = f.nOpen + 1;

	while (i < f.nClose) {
		size_t nEnd = i;
		while (nEnd < f.nClose && Toks[nEnd].szText != ",") {
			if (Toks[nEnd].szText == "(" || Toks[nEnd].szText == "[" || Toks[nEnd].szText == "{") nEnd = Match(nEnd);
			nEnd++;
		}

		Param p;
		p.bUniform = p.bIn = p.bOut = false;
		p.nFirst = i;
		p.nLast = nEnd - 1;
		p.nSemantic = 0;
		p.nType = 0;

		size_t t = i;
		while (t < nEnd && Toks[t].nType == T_IDENT && IsQualifier(Toks[t].szText)) {
			if (Toks[t].szText == "uniform") p.bUniform = true;
			if (Toks[t].szText == "in") p.bIn = true;
			if (Toks[t].szText == "out") p.bOut = true;
			if (Toks[t].szText == "inout") { p.bIn = true; p.bOut = true; }
			t++;
		}
		if (t < nEnd && Toks[t].nType == T_IDENT) {
			p.szType = Toks[t].szText;
			p.nType = t;
			t++;
			if (t < nEnd && Toks[t].nType == T_IDENT) {
				p.szName = Toks[t].szText;
				t++;
				if (t < nEnd && Toks[t].szText == "[") {
					size_t c = Match(t);
					p.szArray = EmitPlain(t, c);
					t = c + 1;
				}
				if (t < nEnd && Toks[t].szText == ":" && t + 1 < nEnd) {
					p.nSemantic = t + 1;
					p.szSemantic = Upper(Toks[t + 1].szText);
				}
			}
		}

		if (nEnd > i) {
			f.Params.push_back(p);
		}
		i = nEnd + 1;
	}
}

void Translator::Scan()
{
	size_t i = 0;
	size_t nLast = Toks.size() - 1;

	while (i < nLast) {
		Token& t = Toks[i];

		if (t.nType == T_PREPROC) {
			i++;
			continue;
		}

		// struct definition
		if (t.nType == T_IDENT && t.szText == "struct" && i + 2 < nLast && Toks[i + 1].nType == T_IDENT && Toks[i + 2].szText == "{") {
			Struct s;
			s.szName = Toks[i + 1].szText;
			s.nStart = i;
			s.nOpen = i + 2;
			s.nClose = Match(s.nOpen);
			size_t e = s.nClose + 1;
			while (e < nLast && Toks[e].szText != ";") e++;
			s.nEnd = e;
			ParseMembers(s);
			Structs.push_back(s);
			i = e + 1;
			continue;
		}

		// top level statement: find its end (';' or a function body)
		size_t nStart = i;
		size_t j = i;
		bool bFunction = false;
		while (j < nLast) {
			const std::string& s = Toks[j].szText;
			if (Toks[j].nType == T_PREPROC) break;
			if (s == ";") break;
			if (s == "(" && j > nStart && Toks[j - 1].nType == T_IDENT) {
				// possible function
				size_t c = Match(j);
				size_t k = c + 1;
				size_t nSemantic = 0;
				if (k < nLast && Toks[k].szText == ":" && k + 1 < nLast && Toks[k + 1].nType == T_IDENT) {
					nSemantic = k + 1;
					k += 2;
				}
				if (k < nLast && Toks[k].szText == "{") {
					Function f;
					f.szName = Toks[j - 1].szText;
					f.nName = j - 1;
					f.nReturnType = nStart;
					for (size_t r = nStart; r < j - 1; r++) {
						if (Toks[r].nType == T_IDENT && !IsQualifier(Toks[r].szText) && Toks[r].szText != "inline") {
							f.nReturnType = r;
							break;
						}
					}
					f.nOpen = j;
					f.nClose = c;
					f.nSemantic = nSemantic;
					f.nBodyOpen = k;
					f.nBodyClose = Match(k);
					ParseParams(f);
					Functions.push_back(f);
					j = f.nBodyClose;
					bFunction = true;
					break;
				}
				j = c;
			} else if (s == "(" || s == "[" || s == "{") {
				j = Match(j);
			}
			j++;
		}

		if (!bFunction && j > nStart && Toks[nStart].nType != T_PREPROC) {
			// global declaration: remember the names it declares
			GlobalDecl d;
			d.nFirst = nStart;
			d.nLast = j;
			int nDepth = 0;
			for (size_t k = nStart; k < j; k++) {
				const std::string& s = Toks[k].szText;
				if (s == "(" || s == "[" || s == "{") nDepth++;
				if (s == ")" || s == "]" || s == "}") nDepth--;
				if (nDepth == 0 && s == "=") {
					// skip the initialiser
					while (k + 1 < j && Toks[k + 1].szText != ",") {
						k++;
						if (Toks[k].szText == "(" || Toks[k].szText == "[" || Toks[k].szText == "{") k = Match(k);
					}
					continue;
				}
				if (nDepth == 0 && Toks[k].nType == T_IDENT && k + 1 <= j) {
					const std::string& n = Toks[k + 1].szText;
					if (n == ";" || n == "=" || n == ":" || n == "[" || n == ",") {
						GlobalNames.insert(Toks[k].szText);
						d.Names.push_back(Toks[k].szText);
					}
				}
			}
			Decls.push_back(d);
		}

		i = j + 1;
	}
}

const Struct* Translator::FindStruct(const std::string& szName) const
{
	for (size_t i = 0; i < Structs.size(); i++) {
		if (Structs[i].szName == szName) return &Structs[i];
	}
	return NULL;
}

Function* Translator::FindFunction(const char* szName)
{
	for (size_t i = 0; i < Functions.size(); i++) {
		if (Functions[i].szName == szName) return &Functions[i];
	}
	return NULL;
}

std::string Translator::EmitPlain(size_t nFirst, size_t nLast)
{
	std::string s;
	for (size_t i = nFirst; i <= nLast && i < Toks.size(); i++) {
		s += Toks[i].szText;
		if (i < nLast) s += " ";
	}
	return s;
}

static bool IsScalarOrMatrixType(const std::string& s)
{
	static const char* Bases[] = { "float", "half", "int", "uint", "bool", "fixed", "double", NULL };
	for (int b = 0; Bases[b]; b++) {
		size_t l = strlen(Bases[b]);
		if (s.compare(0, l, Bases[b]) != 0) continue;
		if (s.size() == l) return true;
		if (s.size() == l + 3 && s[l] >= '1' && s[l] <= '4' && s[l + 1] == 'x' && s[l + 2] >= '1' && s[l + 2] <= '4') return true;
	}
	return false;
}

static bool EndsWith(const std::string& s, const char* e)
{
	size_t l = strlen(e);
	return s.size() >= l && s.compare(s.size() - l, l, e) == 0;
}

// tex2D & co: returns the number of coordinate components (0 = not a texture function)
static int TextureFunctionDim(const std::string& n)
{
	static const char* Suffixes[] = { "", "lod", "bias", "proj", "fetch", "grad", NULL };
	static const struct { const char* szBase; int nDim; } Bases[] = {
		{ "tex1D", 1 }, { "tex2D", 2 }, { "tex3D", 3 }, { "texCUBE", 3 }, { "texRECT", 2 },
		{ "h4tex2D", 2 }, { "f4tex2D", 2 }, { "x4tex2D", 2 }, { "texture2D", 2 }, { "texture2DLod", 2 },
		{ NULL, 0 }
	};
	for (int b = 0; Bases[b].szBase; b++) {
		for (int s = 0; Suffixes[s]; s++) {
			if (n == std::string(Bases[b].szBase) + Suffixes[s]) {
				return Bases[b].nDim;
			}
		}
	}
	return 0;
}

static bool IsVectorType(const std::string& s)
{
	static const char* Bases[] = { "float", "half", "int", "uint", "bool", "fixed", "double", "vec", "ivec", "bvec", NULL };
	for (int b = 0; Bases[b]; b++) {
		size_t l = strlen(Bases[b]);
		if (s.size() == l + 1 && s.compare(0, l, Bases[b]) == 0 && s[l] >= '1' && s[l] <= '4') {
			return true;
		}
	}
	return false;
}

// Emit tokens [nFirst, nLast] with the expression-level rewrites (texture lookups, constructors)
std::string Translator::Emit(size_t nFirst, size_t nLast)
{
	std::string Out;

	for (size_t i = nFirst; i <= nLast && i < Toks.size(); i++) {
		Token& t = Toks[i];

		// GLSL style array constructor: float[](a, b, c) -> { a, b, c }
		if (t.nType == T_IDENT && i + 3 < Toks.size() && Toks[i + 1].szText == "[" && Toks[i + 2].szText == "]" && Toks[i + 3].szText == "(") {
			size_t nClose = Match(i + 3);
			Out += t.szBefore + t.szSpace + "{" + Emit(i + 4, nClose - 1) + "}" + Toks[nClose].szAfter;
			i = nClose;
			continue;
		}

		// GLSL style .st / .stpq swizzles (when not a struct member name)
		if (t.nType == T_IDENT && i > 0 && Toks[i - 1].szText == "." && !t.szText.empty() && t.szText.size() <= 4 &&
			t.szText.find_first_not_of("stpq") == std::string::npos && MemberNames.count(t.szText) == 0) {
			std::string r = t.szText;
			for (size_t k = 0; k < r.size(); k++) {
				r[k] = (r[k] == 's') ? 'x' : (r[k] == 't') ? 'y' : (r[k] == 'p') ? 'z' : 'w';
			}
			Out += t.szBefore + t.szSpace + r + t.szAfter;
			continue;
		}

		if (t.nType == T_IDENT && i + 1 < Toks.size() && Toks[i + 1].szText == "(" && !t.szText.empty()) {
			const std::string& n = t.szText;

			if (n == "atan" && !UserFunctions.count("atan")) {
				// GLSL atan(y, x) is HLSL atan2(y, x)
				size_t nClose = Match(i + 1);
				int nArgs = 1;
				for (size_t k = i + 2; k < nClose; k++) {
					if (Toks[k].szText == "(" || Toks[k].szText == "[" || Toks[k].szText == "{") k = Match(k);
					else if (Toks[k].szText == ",") nArgs++;
				}
				if (nArgs == 2) {
					Out += t.szBefore + t.szSpace + "atan2" + t.szAfter;
					continue;
				}
			}

			// a helper function that takes samplers: pass each texture's sampler state along with it
			if (SamplerParamFuncs.count(n) && (i == 0 || Toks[i - 1].nType != T_IDENT || Toks[i - 1].szText == "return" || Toks[i - 1].szText == "else" || Toks[i - 1].szText == "do")) {
				const std::set<size_t>& Pos = SamplerParamFuncs[n];
				size_t nOpen = i + 1;
				size_t nClose = Match(nOpen);
				std::string r = t.szBefore + t.szSpace + n + t.szAfter + Toks[nOpen].szBefore + Toks[nOpen].szSpace + "(" + Toks[nOpen].szAfter;
				size_t a = nOpen + 1, nArg = 0;
				while (a < nClose) {
					size_t e = a;
					while (e < nClose && Toks[e].szText != ",") {
						if (Toks[e].szText == "(" || Toks[e].szText == "[" || Toks[e].szText == "{") e = Match(e);
						e++;
					}
					std::string szArg = Emit(a, e - 1);
					std::string szName = Trim(szArg);
					r += szArg;
					if (Pos.count(nArg)) {
						bool bKnown = GlobalTextures.count(szName) || ParamTextures.count(szName);
						r += bKnown ? (", fbneo_smp_" + szName) : std::string(", fbneo_smp");
					}
					if (e < nClose) r += Toks[e].szBefore + Toks[e].szSpace + "," + Toks[e].szAfter;
					a = e + 1;
					nArg++;
				}
				r += Toks[nClose].szBefore + Toks[nClose].szSpace + ")" + Toks[nClose].szAfter;
				Out += r;
				i = nClose;
				continue;
			}

			int nTexDim = TextureFunctionDim(n);
			bool bTex = (nTexDim > 0);
			bool bCtor = IsVectorType(n);
			bool bStructCtor = Constructors.count(n) > 0 && (i == 0 || Toks[i - 1].szText != "struct");

			if (bTex || bCtor || bStructCtor) {
				size_t nOpen = i + 1;
				size_t nClose = Match(nOpen);

				// split the arguments
				std::vector<std::string> Args;
				size_t a = nOpen + 1;
				while (a < nClose) {
					size_t e = a;
					while (e < nClose && Toks[e].szText != ",") {
						if (Toks[e].szText == "(" || Toks[e].szText == "[" || Toks[e].szText == "{") e = Match(e);
						e++;
					}
					Args.push_back(Emit(a, e - 1));
					a = e + 1;
				}

				std::string r;

				if (bTex) {
					if (Args.size() < 2) {
						r = n + "(";
						for (size_t k = 0; k < Args.size(); k++) r += (k ? "," : "") + Args[k];
						r += ")";
					} else {
						std::string szTex = Trim(Args[0]);
						std::string szSampler = (GlobalTextures.count(szTex) || ParamTextures.count(szTex)) ? ("fbneo_smp_" + szTex) : std::string("fbneo_smp");
						std::string T = "(" + szTex + ")";
						static const char* Coord[4] = { "", ".x", ".xy", ".xyz" };
						std::string C = Coord[nTexDim];
						std::string V = "(" + Args[1] + ")";

						if (EndsWith(n, "lod")) {
							r = "FBNEO_SAMPLE_LEVEL(" + T + ", " + szSampler + ", " + V + C + ", " + V + ".w)";
						} else if (EndsWith(n, "bias")) {
							r = "FBNEO_SAMPLE_BIAS(" + T + ", " + szSampler + ", " + V + C + ", " + V + ".w)";
						} else if (EndsWith(n, "proj")) {
							char szProj[32];
							sprintf(szProj, "fbneo_proj%d", nTexDim);
							r = "FBNEO_SAMPLE_LEVEL(" + T + ", " + szSampler + ", " + szProj + V + ", 0)";
						} else if (n == "texture2DLod" && Args.size() >= 3) {
							r = "FBNEO_SAMPLE_LEVEL(" + T + ", " + szSampler + ", " + Args[1] + ", " + Args[2] + ")";
						} else if (EndsWith(n, "fetch")) {
							static const char* Load[4] = { "", "int2(V.x, V.w)", "int3(V.xy, V.w)", "int4(V.xyz, V.w)" };
							std::string L = Load[nTexDim];
							size_t q;
							while ((q = L.find('V')) != std::string::npos) L.replace(q, 1, V);
							r = T + ".Load(" + L + ")";
						} else if (Args.size() >= 4) {
							r = T + ".SampleGrad(" + szSampler + ", " + Args[1] + ", " + Args[2] + ", " + Args[3] + ")";
						} else if (Args.size() == 3) {
							r = "FBNEO_SAMPLE_OFFSET(" + T + ", " + szSampler + ", " + Args[1] + ", " + Args[2] + ")";
						} else {
							r = "FBNEO_SAMPLE_LEVEL(" + T + ", " + szSampler + ", " + Args[1] + ", 0)";
						}
					}
				} else if (bCtor && Args.size() == 1) {
					// Cg allows float3(x) to replicate a scalar; HLSL needs a cast
					r = "((" + n + ")(" + Args[0] + "))";
				} else if (bStructCtor) {
					r = "fbneo_make_" + n + "(";
					for (size_t k = 0; k < Args.size(); k++) r += (k ? "," : "") + Args[k];
					r += ")";
				} else {
					r = n + "(";
					for (size_t k = 0; k < Args.size(); k++) r += (k ? "," : "") + Args[k];
					r += ")";
				}

				Out += t.szBefore + t.szSpace + r + Toks[nClose].szAfter;
				i = nClose;
				continue;
			}
		}

		Out += t.szBefore + t.szSpace + t.szText + t.szAfter;
	}

	return Out;
}

} // namespace

static void SetSemantic(Token& t, const std::string& szNew)
{
	t.szText = szNew;
}

struct EntryParam {
	std::string szType, szName, szArray;
	std::string szInSem;			// semantic as written (upper case)
	std::string szSem;				// semantic after translation (upper case, "" for struct parameters)
	bool bIn, bOut, bStruct;
	std::string szText;				// the translated parameter declaration
};

// Canonical form of a varying semantic: "TEXCOORD" == "TEXCOORD0", all the position semantics == "SV_POSITION"
static std::string NormSem(const std::string& s)
{
	std::string u = Upper(Trim(s));
	if (u == "POSITION" || u == "POSITION0" || u == "POS" || u == "VPOS" || u == "WPOS" || u == "SV_POSITION") {
		return "SV_POSITION";
	}
	if (!u.empty() && !isdigit((unsigned char)u[u.size() - 1])) {
		u += "0";
	}
	return u;
}

// Number of components of a scalar / vector type (0 = something else)
static int VecDim(const std::string& t)
{
	static const char* Bases[] = { "float", "half", "fixed", "int", "uint", "bool", "double", NULL };
	for (int b = 0; Bases[b]; b++) {
		size_t l = strlen(Bases[b]);
		if (t.compare(0, l, Bases[b]) != 0) continue;
		if (t.size() == l) return 1;
		if (t.size() == l + 1 && t[l] >= '1' && t[l] <= '4') return t[l] - '0';
	}
	return 0;
}

static std::string Convert(const std::string& szDstType, const std::string& szSrcType, const std::string& szExpr)
{
	int d = VecDim(szDstType), s = VecDim(szSrcType);
	if (d == 0 || s == 0 || d == s) {
		return szExpr;
	}
	if (d < s) {
		return "(" + szDstType + ")(" + szExpr + ")";
	}
	std::string r = szDstType + "(" + szExpr;
	for (int i = s; i < d; i++) r += ", 0";
	return r + ")";
}

static bool IsIntegerType(const std::string& t)
{
	return t.compare(0, 3, "int") == 0 || t.compare(0, 4, "uint") == 0 || t.compare(0, 4, "bool") == 0;
}

struct Varying {
	std::string szType, szArray, szSem;		// szSem normalised
	std::string szSource;					// expression in the vertex shader wrapper
};

static std::string MakeStageWrappers(Translator& x, Function* pVS, Function* pPS, std::vector<EntryParam>* EntryParams,
									 std::map<std::string, std::map<std::string, std::string> >& MemberSem)
{
	std::vector<Varying> V;
	struct Local {
		static int Find(std::vector<Varying>& V, const std::string& szSem) {
			for (size_t i = 0; i < V.size(); i++) if (V[i].szSem == szSem) return (int)i;
			return -1;
		}
		static void Add(std::vector<Varying>& V, const std::string& szType, const std::string& szArray, const std::string& szSem, const std::string& szSource) {
			if (szSem.empty() || Find(V, szSem) >= 0) return;
			Varying v; v.szType = szType; v.szArray = szArray; v.szSem = szSem; v.szSource = szSource;
			V.push_back(v);
		}
	};

	// ---- vertex shader wrapper: collect the outputs
	// (the wrapper's own locals are named fbneo_lN: a parameter may have the same name as its type)
	std::string szVSParams, szVSLocals, szVSArgs;
	for (size_t i = 0; i < EntryParams[0].size(); i++) {
		const EntryParam& p = EntryParams[0][i];
		char szLocal[32];
		sprintf(szLocal, "fbneo_l%d", (int)i);

		if (!p.bOut) {
			szVSParams += std::string(szVSParams.empty() ? "" : ",") + p.szText;
			szVSArgs += std::string(i ? ", " : "") + p.szName;
			continue;
		}

		szVSArgs += std::string(i ? ", " : "") + szLocal;

		if (p.bIn) {
			// inout: input under its original semantic, passed through a local
			szVSParams += std::string(szVSParams.empty() ? "" : ",") + " " + p.szType + " fbneo_in_" + p.szName + p.szArray + (p.szInSem.empty() ? "" : " : " + p.szInSem);
			szVSLocals += "\t" + p.szType + " " + szLocal + p.szArray + " = fbneo_in_" + p.szName + ";\n";
		} else {
			szVSLocals += "\t" + p.szType + " " + szLocal + p.szArray + " = (" + p.szType + ")0;\n";
		}

		if (p.bStruct) {
			const Struct* s = x.FindStruct(p.szType);
			for (size_t m = 0; m < s->Members.size(); m++) {
				const Member& mb = s->Members[m];
				Local::Add(V, mb.szType, mb.szArray, NormSem(MemberSem[s->szName][mb.szName]), std::string(szLocal) + "." + mb.szName);
			}
		} else {
			Local::Add(V, p.szType, p.szArray, NormSem(p.szSem), szLocal);
		}
	}

	std::string szVSRet = x.Toks[pVS->nReturnType].szText;
	std::string szVSCall;
	if (szVSRet != "void") {
		szVSCall = "\t" + szVSRet + " fbneo_ret = fbneo_main_vertex(" + szVSArgs + ");\n";
		const Struct* s = x.FindStruct(szVSRet);
		if (s) {
			for (size_t m = 0; m < s->Members.size(); m++) {
				const Member& mb = s->Members[m];
				Local::Add(V, mb.szType, mb.szArray, NormSem(MemberSem[s->szName][mb.szName]), "fbneo_ret." + mb.szName);
			}
		} else if (pVS->nSemantic) {
			Local::Add(V, szVSRet, "", NormSem(x.Toks[pVS->nSemantic].szText), "fbneo_ret");
		}
	} else {
		szVSCall = "\tfbneo_main_vertex(" + szVSArgs + ");\n";
	}

	// ---- pixel shader wrapper: build the inputs from the shared struct
	std::string szPSParams = " fbneo_v2f fbneo_in", szPSLocals, szPSArgs;
	for (size_t i = 0; i < EntryParams[1].size(); i++) {
		const EntryParam& p = EntryParams[1][i];
		char szLocal[32];
		sprintf(szLocal, "fbneo_l%d", (int)i);

		if (p.bOut) {
			szPSParams += "," + p.szText;
			szPSArgs += std::string(i ? ", " : "") + p.szName;
			continue;
		}

		std::string szSemCheck = NormSem(p.szSem);
		if (!p.bStruct && StartsWith(szSemCheck, "SV_") && szSemCheck != "SV_POSITION") {
			szPSParams += "," + p.szText;						// SV_IsFrontFace etc.
			szPSArgs += std::string(i ? ", " : "") + p.szName;
			continue;
		}

		szPSArgs += std::string(i ? ", " : "") + szLocal;

		if (p.bStruct) {
			const Struct* s = x.FindStruct(p.szType);
			szPSLocals += "\t" + p.szType + " " + szLocal + " = (" + p.szType + ")0;\n";
			for (size_t m = 0; m < s->Members.size(); m++) {
				const Member& mb = s->Members[m];
				std::string szSem = NormSem(MemberSem[s->szName][mb.szName]);
				if (szSem.empty() || szSem == "0") continue;
				int n = Local::Find(V, szSem);
				if (n < 0) {
					Local::Add(V, mb.szType, mb.szArray, szSem, "");
					n = (int)V.size() - 1;
				}
				char szField[32];
				sprintf(szField, "fbneo_in.v%d", n);
				szPSLocals += "\t" + std::string(szLocal) + "." + mb.szName + " = " + (mb.szArray.empty() ? Convert(mb.szType, V[n].szType, szField) : std::string(szField)) + ";\n";
			}
			continue;
		}

		std::string szSem = szSemCheck;
		if (szSem.empty() || szSem == "0") {
			szPSLocals += "\t" + p.szType + " " + szLocal + p.szArray + " = (" + p.szType + ")0;\n";
			continue;
		}
		int n = Local::Find(V, szSem);
		if (n < 0) {
			Local::Add(V, p.szType, p.szArray, szSem, "");
			n = (int)V.size() - 1;
		}
		char szField[32];
		sprintf(szField, "fbneo_in.v%d", n);
		szPSLocals += "\t" + p.szType + " " + szLocal + p.szArray + " = " + (p.szArray.empty() ? Convert(p.szType, V[n].szType, szField) : std::string(szField)) + ";\n";
	}

	// ---- the shared struct
	std::string Out = "\n// Stage linkage generated by FBNeo\nstruct fbneo_v2f {\n";
	bool bHasPosition = false;
	for (size_t i = 0; i < V.size(); i++) {
		std::string szSem = V[i].szSem;
		if (szSem == "SV_POSITION") {
			szSem = "SV_Position";
			bHasPosition = true;
		}
		char szField[32];
		sprintf(szField, "v%d", (int)i);
		Out += "\t" + std::string(IsIntegerType(V[i].szType) ? "nointerpolation " : "") + V[i].szType + " " + szField + V[i].szArray + " : " + szSem + ";\n";
	}
	if (!bHasPosition) {
		Out += "\tfloat4 fbneo_pos : SV_Position;\n";
	}
	Out += "};\n\n";

	// ---- vertex shader wrapper
	Out += "fbneo_v2f main_vertex(" + szVSParams + ")\n{\n" + szVSLocals + szVSCall + "\tfbneo_v2f fbneo_out = (fbneo_v2f)0;\n";
	for (size_t i = 0; i < V.size(); i++) {
		if (V[i].szSource.empty()) continue;
		char szField[32];
		sprintf(szField, "fbneo_out.v%d", (int)i);
		Out += std::string("\t") + szField + " = " + V[i].szSource + ";\n";
	}
	Out += "\treturn fbneo_out;\n}\n\n";

	// ---- pixel shader wrapper
	std::string szPSRet = x.Toks[pPS->nReturnType].szText;
	std::string szPSSem;
	if (pPS->nSemantic) {
		szPSSem = " : " + x.Toks[pPS->nSemantic].szText;
	}
	Out += szPSRet + " main_fragment(" + szPSParams + ")" + szPSSem + "\n{\n" + szPSLocals;
	Out += std::string(szPSRet == "void" ? "\t" : "\treturn ") + "fbneo_main_fragment(" + szPSArgs + ");\n}\n";

	// ---- the original functions become ordinary functions
	x.Toks[pVS->nName].szText = "fbneo_main_vertex";
	x.Toks[pPS->nName].szText = "fbneo_main_fragment";

	// An entry point may end a path with 'discard' and no return value, an ordinary function may not
	if (szPSRet != "void") {
		x.Toks[pPS->nBodyClose].szBefore += "\n\treturn (" + szPSRet + ")0;\n";
	}
	Function* Fn[2] = { pVS, pPS };
	for (int f = 0; f < 2; f++) {
		if (Fn[f]->nSemantic) {
			x.Toks[Fn[f]->nSemantic].szText = "";
			x.Toks[Fn[f]->nSemantic - 1].szText = "";		// the ':'
		}
	}

	return Out;
}

bool D3D11TranslateCg(const std::string& Source, D3D11CgTranslation* pOut, std::string* pError)
{
	Translator x;

	pOut->szHLSL.clear();
	pOut->bHasVertex = false;
	pOut->bHasFragment = false;

	x.Tokenize(Source);
	x.Scan();

	// drop preprocessor leftovers we don't want (#pragma parameter etc.)
	for (size_t i = 0; i < x.Toks.size(); i++) {
		if (x.Toks[i].nType == T_PREPROC && x.Toks[i].szText.find("pragma") != std::string::npos) {
			x.Toks[i].szText = "";
		}
	}

	std::set<std::string> UsedIdents;
	for (size_t i = 0; i < x.Toks.size(); i++) {
		if (x.Toks[i].nType == T_IDENT) UsedIdents.insert(x.Toks[i].szText);
	}
	for (size_t i = 0; i < x.Structs.size(); i++) {
		for (size_t m = 0; m < x.Structs[i].Members.size(); m++) {
			x.MemberNames.insert(x.Structs[i].Members[m].szName);
		}
	}
	for (size_t i = 0; i < x.Functions.size(); i++) {
		x.UserFunctions.insert(x.Functions[i].szName);

		// 'static' isn't allowed on function parameters in HLSL
		for (size_t k = x.Functions[i].nOpen + 1; k < x.Functions[i].nClose; k++) {
			if (x.Toks[k].nType == T_IDENT && x.Toks[k].szText == "static") x.Toks[k].szText = "";
		}

		// Cg accepts functions where not every path returns a value, HLSL doesn't: add an (unreachable) return
		// (the entry points are handled by MakeStageWrappers())
		{
			const Function& f = x.Functions[i];
			const std::string& szRet = x.Toks[f.nReturnType].szText;
			if (szRet != "void" && f.szName != "main_vertex" && f.szName != "main_fragment" && f.nReturnType < f.nName) {
				x.Toks[f.nBodyClose].szBefore += "\n\treturn (" + szRet + ")0;\n";
			}
		}

		// Cg doesn't require a function to write all of its 'out' parameters, HLSL does: zero them first
		{
			const Function& f = x.Functions[i];
			for (size_t pi = 0; pi < f.Params.size(); pi++) {
				const Param& p = f.Params[pi];
				if (!p.bOut || p.bIn || p.szName.empty() || !p.szArray.empty() || IsSamplerType(p.szType)) continue;
				x.Toks[f.nBodyOpen].szAfter += "\n\t" + p.szName + " = (" + p.szType + ")0;";
			}
		}

		// Cg doesn't mind locals being read before they're written, the HLSL compiler may refuse:
		// give uninitialised scalar / vector / matrix / struct locals a zero initialiser
		{
			const Function& f = x.Functions[i];
			for (size_t k = f.nBodyOpen + 1; k + 2 < f.nBodyClose; k++) {
				const std::string& szPrev = x.Toks[k - 1].szText;
				if (szPrev != ";" && szPrev != "{" && szPrev != "}") continue;
				if (x.Toks[k].nType != T_IDENT || x.Toks[k + 1].nType != T_IDENT) continue;
				const std::string& szType = x.Toks[k].szText;
				if (!IsVectorType(szType) && !IsScalarOrMatrixType(szType) && !x.FindStruct(szType)) continue;

				// declarators up to the ';'
				size_t d = k + 1;
				while (d < f.nBodyClose && x.Toks[d].nType == T_IDENT) {
					size_t nName = d;
					d++;
					bool bArray = false, bInit = false;
					if (x.Toks[d].szText == "[") { d = x.Match(d) + 1; bArray = true; }
					if (x.Toks[d].szText == ":") { d += 2; }							// semantic
					if (x.Toks[d].szText == "=") {
						bInit = true;
						while (d < f.nBodyClose && x.Toks[d].szText != "," && x.Toks[d].szText != ";") {
							if (x.Toks[d].szText == "(" || x.Toks[d].szText == "[" || x.Toks[d].szText == "{") d = x.Match(d);
							d++;
						}
					}
					if (!bInit && !bArray && (x.Toks[d].szText == "," || x.Toks[d].szText == ";")) {
						x.Toks[nName].szAfter += " = (" + szType + ")0";
					}
					if (x.Toks[d].szText != ",") break;
					d++;
				}
			}
		}

		// Cg lets 'static const' locals be initialised from parameters; HLSL needs constant
		// initialisers for static locals, but plain 'const' locals work the same here
		for (size_t k = x.Functions[i].nBodyOpen + 1; k + 1 < x.Functions[i].nBodyClose; k++) {
			if (x.Toks[k].nType == T_IDENT && x.Toks[k].szText == "static" && x.Toks[k + 1].szText == "const") x.Toks[k].szText = "";
		}
	}

	// Cg lets shaders write to global variables; in HLSL they have to be static
	for (size_t d = 0; d < x.Decls.size(); d++) {
		GlobalDecl& g = x.Decls[d];
		bool bQualified = false;
		size_t nUniform = 0;
		size_t nType = g.nFirst;
		while (nType <= g.nLast && x.Toks[nType].nType == T_IDENT && IsQualifier(x.Toks[nType].szText)) {
			const std::string& q = x.Toks[nType].szText;
			if (q == "static" || q == "const") bQualified = true;
			if (q == "uniform") nUniform = nType;
			nType++;
		}
		if (bQualified || nType > g.nLast || x.Toks[nType].nType != T_IDENT || x.Toks[nType].szText == "struct" || x.Toks[nType].szText == "typedef") continue;
		if (nUniform && IsSamplerType(x.Toks[nType].szText)) continue;

		bool bStatic = (x.FindStruct(x.Toks[nType].szText) != NULL) && nUniform == 0;

		for (size_t f = 0; f < x.Functions.size() && !bStatic; f++) {
			for (size_t k = x.Functions[f].nBodyOpen + 1; k < x.Functions[f].nBodyClose && !bStatic; k++) {
				const Token& t = x.Toks[k];
				if (t.nType != T_IDENT || x.Toks[k - 1].szText == ".") continue;
				bool bName = false;
				for (size_t n = 0; n < g.Names.size(); n++) if (g.Names[n] == t.szText) bName = true;
				if (!bName) continue;
				size_t a = k + 1;
				while (a < x.Toks.size() && (x.Toks[a].szText == "." || x.Toks[a].szText == "[")) {
					if (x.Toks[a].szText == "[") a = x.Match(a) + 1;
					else a += 2;
				}
				const std::string& op = x.Toks[a].szText;
				if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "++" || op == "--" ||
					(x.Toks[k - 1].szText == "++" || x.Toks[k - 1].szText == "--")) {
					bStatic = true;
				}
			}
		}

		if (bStatic) {
			if (nUniform) {
				x.Toks[nUniform].szText = "";
			}
			x.Toks[g.nFirst].szBefore += "static ";
		}
	}

	Function* pVS = x.FindFunction("main_vertex");
	Function* pPS = x.FindFunction("main_fragment");

	if (pPS == NULL) {
		if (pError) *pError = "No main_fragment() found";
		return false;
	}

	pOut->bHasVertex = (pVS != NULL);
	pOut->bHasFragment = true;

	// -------------------------------------------------------------
	// Work out which structs are varyings and which are uniforms

	std::set<std::string> VaryingStructs;		// VS outputs / PS inputs
	std::set<std::string> VSInputStructs;
	std::set<std::string> PSOutputStructs;

	if (pVS) {
		if (x.FindStruct(x.Toks[pVS->nReturnType].szText)) VaryingStructs.insert(x.Toks[pVS->nReturnType].szText);
		for (size_t i = 0; i < pVS->Params.size(); i++) {
			const Param& p = pVS->Params[i];
			if (p.bOut && x.FindStruct(p.szType)) VaryingStructs.insert(p.szType);
		}
	}
	if (x.FindStruct(x.Toks[pPS->nReturnType].szText)) PSOutputStructs.insert(x.Toks[pPS->nReturnType].szText);

	// struct with a "real" semantic on any member is a varying struct
	std::set<std::string> SemanticStructs;
	for (size_t i = 0; i < x.Structs.size(); i++) {
		for (size_t m = 0; m < x.Structs[i].Members.size(); m++) {
			const std::string& s = x.Structs[i].Members[m].szSemantic;
			if (!s.empty() && !StartsWith(s, "TEXUNIT")) {
				SemanticStructs.insert(x.Structs[i].szName);
			}
		}
	}

	std::map<std::string, std::string> Flatten;		// param name -> struct name
	std::set<std::string> UniformStructs;

	Function* Entries[2] = { pVS, pPS };
	for (int e = 0; e < 2; e++) {
		Function* f = Entries[e];
		if (f == NULL) continue;
		for (size_t i = 0; i < f->Params.size(); i++) {
			const Param& p = f->Params[i];
			const Struct* s = x.FindStruct(p.szType);
			if (s == NULL || p.bOut) continue;
			if (VaryingStructs.count(p.szType)) continue;
			if (!p.bUniform && SemanticStructs.count(p.szType) && !PSOutputStructs.count(p.szType)) {
				if (f == pVS) VSInputStructs.insert(p.szType);
				else VaryingStructs.insert(p.szType);
				continue;
			}
			Flatten[p.szName] = p.szType;
			UniformStructs.insert(p.szType);
		}
	}

	// Pass inputs used without being declared, e.g. "MASK_RESIZE.texture" in a branch that can't be
	// taken (crt-royale's integrated graphics versions): Cg tolerates these, HLSL doesn't, so declare
	// them as if they were uniform parameters (they're bound by name, like any pass input)
	{
		const Struct* pPassStruct = NULL;
		for (std::set<std::string>::iterator it = UniformStructs.begin(); it != UniformStructs.end() && pPassStruct == NULL; ++it) {
			const Struct* s = x.FindStruct(*it);
			bool bTexture = false, bTextureSize = false;
			for (size_t m = 0; m < s->Members.size(); m++) {
				if (s->Members[m].szName == "texture") bTexture = true;
				if (s->Members[m].szName == "texture_size") bTextureSize = true;
			}
			if (bTexture && bTextureSize) pPassStruct = s;
		}

		if (pPassStruct) {
			std::set<std::string> Candidates, DeclaredNames;
			for (size_t fi = 0; fi < x.Functions.size(); fi++) {
				for (size_t i = 0; i < x.Functions[fi].Params.size(); i++) {
					DeclaredNames.insert(x.Functions[fi].Params[i].szName);
				}
			}
			for (size_t i = 0; i + 2 < x.Toks.size(); i++) {
				const Token& t = x.Toks[i];
				if (t.nType != T_IDENT) continue;
				if (i > 0 && x.Toks[i - 1].nType == T_IDENT) {
					DeclaredNames.insert(t.szText);				// "type name"
				}
				if (i > 0 && x.Toks[i - 1].szText == ".") continue;
				if (x.Toks[i + 1].szText != ".") continue;
				const std::string& m = x.Toks[i + 2].szText;
				if (m == "texture" || m == "texture_size" || m == "video_size" || m == "tex_coord") {
					Candidates.insert(t.szText);
				}
			}
			for (std::set<std::string>::iterator it = Candidates.begin(); it != Candidates.end(); ++it) {
				if (Flatten.count(*it) || DeclaredNames.count(*it) || x.GlobalNames.count(*it) || x.FindStruct(*it)) continue;
				Flatten[*it] = pPassStruct->szName;
			}
		}
	}

	// -------------------------------------------------------------
	// New global declarations

	std::string Globals;
	std::set<std::string> Declared = x.GlobalNames;

	struct Local {
		static void Declare(std::string& G, std::set<std::string>& D, std::set<std::string>& Textures, const std::string& szType, const std::string& szName, const std::string& szArray)
		{
			if (D.count(szName)) return;
			D.insert(szName);
			if (IsSamplerType(szType)) {
				G += "Texture2D " + szName + szArray + ";\nSamplerState fbneo_smp_" + szName + ";\n";
				Textures.insert(szName);
			} else {
				G += szType + " " + szName + szArray + ";\n";
			}
		}
	};

	// flattened struct parameters -> NAME_member globals
	std::set<std::string> TexCoordStructs;			// flattened params with a tex_coord member that is used
	for (std::map<std::string, std::string>::iterator it = Flatten.begin(); it != Flatten.end(); ++it) {
		const Struct* s = x.FindStruct(it->second);
		bool bHasVideo = false, bHasTexture = false;
		for (size_t m = 0; m < s->Members.size(); m++) {
			const Member& mb = s->Members[m];
			std::string szName = it->first + "_" + mb.szName;
			if (mb.szName == "video_size") bHasVideo = true;
			if (mb.szName == "texture_size") bHasTexture = true;
			if (mb.szName == "tex_coord") {
				if (!Declared.count(szName)) {
					Declared.insert(szName);
					Globals += "static " + mb.szType + " " + szName + ";\n";
				}
				TexCoordStructs.insert(it->first);
				continue;
			}
			Local::Declare(Globals, Declared, x.GlobalTextures, mb.szType, szName, mb.szArray);
		}
		if (TexCoordStructs.count(it->first)) {
			if (!bHasVideo) Local::Declare(Globals, Declared, x.GlobalTextures, "float2", it->first + "_video_size", "");
			if (!bHasTexture) Local::Declare(Globals, Declared, x.GlobalTextures, "float2", it->first + "_texture_size", "");
		}
	}

	// Helper functions that take one of the flattened structs as a parameter: when only one entry
	// parameter has that type, member accesses in the helper can use the flattened globals too
	std::map<std::string, std::string> TypeToFlat;
	{
		std::map<std::string, int> Count;
		for (std::map<std::string, std::string>::iterator it = Flatten.begin(); it != Flatten.end(); ++it) {
			Count[it->second]++;
			TypeToFlat[it->second] = it->first;
		}
		for (std::map<std::string, int>::iterator it = Count.begin(); it != Count.end(); ++it) {
			if (it->second != 1) TypeToFlat.erase(it->first);
		}
	}
	struct HelperAlias { size_t nFirst, nLast; std::string szParam, szFlat; };
	std::vector<HelperAlias> Aliases;
	for (size_t fi = 0; fi < x.Functions.size(); fi++) {
		Function& f = x.Functions[fi];
		if (&f == pVS || &f == pPS) continue;
		for (size_t i = 0; i < f.Params.size(); i++) {
			if (TypeToFlat.count(f.Params[i].szType) && !f.Params[i].szName.empty()) {
				HelperAlias a;
				a.nFirst = f.nBodyOpen;
				a.nLast = f.nBodyClose;
				a.szParam = f.Params[i].szName;
				a.szFlat = TypeToFlat[f.Params[i].szType];
				Aliases.push_back(a);
			}
		}
	}

	// rewrite NAME.member -> NAME_member, and find whole-struct uses
	std::set<std::string> WholeUses[2];			// per entry point (VS, PS)
	bool bTexCoordUsedInVS = false;
	for (size_t i = 0; i + 2 < x.Toks.size(); i++) {
		Token& t = x.Toks[i];
		if (t.nType != T_IDENT) continue;

		std::string szFlat;
		if (Flatten.count(t.szText)) {
			szFlat = t.szText;
		} else {
			for (size_t a = 0; a < Aliases.size(); a++) {
				if (i > Aliases[a].nFirst && i < Aliases[a].nLast && Aliases[a].szParam == t.szText) {
					szFlat = Aliases[a].szFlat;
					break;
				}
			}
			if (szFlat.empty() || x.Toks[i + 1].szText != ".") continue;
		}

		// skip the parameter declarations themselves
		bool bParam = false;
		for (int e = 0; e < 2 && !bParam; e++) {
			Function* f = Entries[e];
			if (f && i > f->nOpen && i < f->nClose) bParam = true;
		}
		if (bParam) continue;

		if (i > 0 && x.Toks[i - 1].szText == ".") continue;

		if (x.Toks[i + 1].szText == "." && x.Toks[i + 2].nType == T_IDENT) {
			std::string szMember = x.Toks[i + 2].szText;
			if (szMember == "tex_coord" && pVS && i > pVS->nBodyOpen && i < pVS->nBodyClose) {
				bTexCoordUsedInVS = true;
			}
			t.szText = szFlat + "_" + szMember;
			x.Toks[i + 1].szText = "";
			x.Toks[i + 2].szText = "";
			x.Toks[i + 2].szSpace = "";
			x.Toks[i + 1].szSpace = "";
		} else {
			for (int e = 0; e < 2; e++) {
				Function* f = Entries[e];
				if (f && i > f->nBodyOpen && i < f->nBodyClose) WholeUses[e].insert(t.szText);
			}
		}
	}

	// TEXCOORD indices in use by the varyings (so automatic ones don't clash)
	std::set<int> UsedTexCoords;
	std::map<std::string, int> AutoParamTexCoord;		// varying params without a semantic, by name
	{
		for (size_t si = 0; si < x.Structs.size(); si++) {
			if (!VaryingStructs.count(x.Structs[si].szName)) continue;
			for (size_t m = 0; m < x.Structs[si].Members.size(); m++) {
				const std::string& sem = x.Structs[si].Members[m].szSemantic;
				if (StartsWith(sem, "TEXCOORD")) UsedTexCoords.insert(IsNumber(sem.substr(8)) ? atoi(sem.substr(8).c_str()) : 0);
			}
		}
		for (int e = 0; e < 2; e++) {
			Function* f = Entries[e];
			if (f == NULL) continue;
			for (size_t i = 0; i < f->Params.size(); i++) {
				// Cg numbers the vertex shader's outputs independently: ignore the vertex attributes, and the
				// fragment shader's inputs (they name the vertex outputs they read)
				if (f == pVS && !f->Params[i].bOut) continue;
				if (f != pVS && pVS && !f->Params[i].bOut) continue;
				const std::string& sem = f->Params[i].szSemantic;
				if (StartsWith(sem, "TEXCOORD")) UsedTexCoords.insert(IsNumber(sem.substr(8)) ? atoi(sem.substr(8).c_str()) : 0);
			}
		}
	}
	int nNextTexCoord = 0;

	// -------------------------------------------------------------
	// Entry point parameters

	std::string VSBodyStart, PSBodyStart;
	std::vector<EntryParam> EntryParams[2];			// the entry points' remaining (varying / output) parameters

	for (int e = 0; e < 2; e++) {
		Function* f = Entries[e];
		if (f == NULL) continue;
		bool bVS = (f == pVS);

		std::vector<std::string> Kept;

		for (size_t i = 0; i < f->Params.size(); i++) {
			Param& p = f->Params[i];

			if (Flatten.count(p.szName) && !p.bOut) {
				continue;											// now globals
			}
			if (IsSamplerType(p.szType) && !p.bOut) {
				Local::Declare(Globals, Declared, x.GlobalTextures, p.szType, p.szName, p.szArray);
				continue;
			}
			if (p.bUniform && !p.bOut) {
				Local::Declare(Globals, Declared, x.GlobalTextures, p.szType, p.szName, p.szArray);
				continue;
			}

			// Cg assigns semantics to varyings automatically; HLSL needs them spelled out
			if (p.nSemantic == 0 && !p.szName.empty() && !x.FindStruct(p.szType) && ((bVS && p.bOut) || (!bVS && !p.bOut))) {
				if (!AutoParamTexCoord.count(p.szName)) {
					while (UsedTexCoords.count(nNextTexCoord)) nNextTexCoord++;
					UsedTexCoords.insert(nNextTexCoord);
					AutoParamTexCoord[p.szName] = nNextTexCoord;
				}
				char szSem[32];
				sprintf(szSem, " : TEXCOORD%d", AutoParamTexCoord[p.szName]);
				size_t k = p.nFirst;
				while (k < p.nLast && x.Toks[k].szText != p.szName) k++;
				if (k < p.nLast && x.Toks[k + 1].szText == "[") k = x.Match(k + 1);
				x.Toks[k].szAfter += szSem;
				p.szSemantic = szSem + 3;
			}

			// semantics
			if (p.nSemantic) {
				std::string s = p.szSemantic;
				if (bVS && p.bOut && (s == "POSITION" || s == "POSITION0" || s == "POS")) SetSemantic(x.Toks[p.nSemantic], "SV_Position");
				if (!bVS && !p.bOut && (s == "VPOS" || s == "WPOS" || s == "POSITION" || s == "POSITION0")) SetSemantic(x.Toks[p.nSemantic], "SV_Position");
				if (!bVS && p.bOut && StartsWith(s, "COLOR")) SetSemantic(x.Toks[p.nSemantic], "SV_Target" + s.substr(5));
				if (!bVS && p.bOut && s == "DEPTH") SetSemantic(x.Toks[p.nSemantic], "SV_Depth");
			}

			Kept.push_back(x.Emit(p.nFirst, p.nLast));

			EntryParam ep;
			ep.szType = p.szType;
			ep.szName = p.szName;
			ep.szArray = p.szArray;
			ep.szInSem = p.szSemantic;
			ep.szSem = p.nSemantic ? Upper(x.Toks[p.nSemantic].szText) : p.szSemantic;
			ep.bOut = p.bOut;
			ep.bIn = p.bIn || !p.bOut;
			ep.bStruct = (x.FindStruct(p.szType) != NULL);
			ep.szText = Kept.back();
			EntryParams[e].push_back(ep);
		}

		if (bVS && bTexCoordUsedInVS) {
			Kept.push_back(" float2 fbneo_uv01_in : FBNEOUV");
			EntryParam ep;
			ep.szType = "float2";
			ep.szName = "fbneo_uv01_in";
			ep.szInSem = ep.szSem = "FBNEOUV";
			ep.bOut = false;
			ep.bIn = true;
			ep.bStruct = false;
			ep.szText = Kept.back();
			EntryParams[e].push_back(ep);
			for (std::set<std::string>::iterator it = TexCoordStructs.begin(); it != TexCoordStructs.end(); ++it) {
				VSBodyStart += "\n\t" + *it + "_tex_coord = fbneo_uv01_in * " + *it + "_video_size / " + *it + "_texture_size;";
			}
		}

		// static instances of flattened structs that are used as a whole
		for (std::map<std::string, std::string>::iterator it = Flatten.begin(); it != Flatten.end(); ++it) {
			if (!WholeUses[e].count(it->first)) continue;
			const Struct* s = x.FindStruct(it->second);
			if (s->nEnd > f->nBodyOpen) continue;					// struct declared after the function
			std::string& Body = bVS ? VSBodyStart : PSBodyStart;
			for (size_t m = 0; m < s->Members.size(); m++) {
				if (s->Members[m].bSampler) continue;
				Body += "\n\t" + it->first + "." + s->Members[m].szName + " = " + it->first + "_" + s->Members[m].szName + ";";
			}
		}

		// replace the parameter list
		std::string szParams;
		for (size_t i = 0; i < Kept.size(); i++) {
			szParams += (i ? "," : "") + Kept[i];
		}
		for (size_t i = f->nOpen + 1; i < f->nClose; i++) {
			x.Toks[i].szText = "";
			x.Toks[i].szSpace = "";
			x.Toks[i].szBefore = "";
			x.Toks[i].szAfter = "";
		}
		x.Toks[f->nOpen].szAfter = szParams;

		// return semantic
		if (f->nSemantic) {
			std::string s = Upper(x.Toks[f->nSemantic].szText);
			if (bVS && (s == "POSITION" || s == "POSITION0")) SetSemantic(x.Toks[f->nSemantic], "SV_Position");
			if (!bVS && StartsWith(s, "COLOR")) SetSemantic(x.Toks[f->nSemantic], "SV_Target" + s.substr(5));
		}
	}

	if (pVS) x.Toks[pVS->nBodyOpen].szAfter += VSBodyStart;
	x.Toks[pPS->nBodyOpen].szAfter += PSBodyStart;

	// -------------------------------------------------------------
	// Struct definitions

	std::map<std::string, std::map<std::string, std::string> > MemberSem;	// varying struct -> member -> semantic

	for (size_t si = 0; si < x.Structs.size(); si++) {
		Struct& s = x.Structs[si];

		// 'uniform' isn't valid on a struct member in HLSL (Cg ignores it)
		for (size_t k = s.nOpen + 1; k < s.nClose; k++) {
			if (x.Toks[k].nType == T_IDENT && x.Toks[k].szText == "uniform") x.Toks[k].szText = "";
		}

		if (UniformStructs.count(s.szName)) {
			// uniform struct: remove sampler members, 'uniform' qualifiers and TEXUNIT semantics
			int nRemaining = 0;
			for (size_t m = 0; m < s.Members.size(); m++) {
				Member& mb = s.Members[m];
				if (mb.bSampler || mb.szName == "tex_coord") {
					for (size_t k = mb.nFirst; k <= mb.nLast; k++) {
						x.Toks[k].szText = "";
					}
					continue;
				}
				nRemaining++;
				for (size_t k = mb.nFirst; k <= mb.nLast; k++) {
					if (x.Toks[k].szText == "uniform") x.Toks[k].szText = "";
				}
				if (mb.nSemanticColon) {
					x.Toks[mb.nSemanticColon].szText = "";
					x.Toks[mb.nSemantic].szText = "";
				}
			}
			if (nRemaining == 0) {
				x.Toks[s.nOpen].szAfter += " float fbneo_dummy; ";
			}

			// static instance for whole-struct uses
			for (std::map<std::string, std::string>::iterator it = Flatten.begin(); it != Flatten.end(); ++it) {
				if (it->second == s.szName && (WholeUses[0].count(it->first) || WholeUses[1].count(it->first))) {
					x.Toks[s.nEnd].szAfter += "\nstatic " + s.szName + " " + it->first + ";";
				}
			}
			continue;
		}

		bool bVarying = VaryingStructs.count(s.szName) > 0;
		bool bPSOut = PSOutputStructs.count(s.szName) > 0;

		if (bVarying || bPSOut) {
			std::set<int>& Used = UsedTexCoords;
			int& nNext = nNextTexCoord;

			for (size_t m = 0; m < s.Members.size(); m++) {
				Member& mb = s.Members[m];
				for (size_t k = mb.nFirst; k <= mb.nLast; k++) {
					if (x.Toks[k].szText == "uniform") x.Toks[k].szText = "";
				}
				if (mb.nSemantic) {
					std::string sem = mb.szSemantic;
					if (bVarying && (sem == "POSITION" || sem == "POSITION0" || sem == "POS" || sem == "VPOS" || sem == "WPOS")) SetSemantic(x.Toks[mb.nSemantic], "SV_Position");
					if (bPSOut && StartsWith(sem, "COLOR")) SetSemantic(x.Toks[mb.nSemantic], "SV_Target" + sem.substr(5));
					if (bPSOut && sem == "DEPTH") SetSemantic(x.Toks[mb.nSemantic], "SV_Depth");
					if (bVarying) MemberSem[s.szName][mb.szName] = Upper(x.Toks[mb.nSemantic].szText);
				} else if (bVarying) {
					// Cg assigns semantics automatically; HLSL needs them spelled out
					while (Used.count(nNext)) nNext++;
					char szSem[32];
					sprintf(szSem, " : TEXCOORD%d", nNext);
					Used.insert(nNext);
					MemberSem[s.szName][mb.szName] = szSem + 3;
					// insert after the name (and array)
					size_t k = mb.nFirst;
					while (k < mb.nLast && x.Toks[k].szText != mb.szName) k++;
					if (k < mb.nLast && x.Toks[k + 1].szText == "[") k = x.Match(k + 1);
					x.Toks[k].szAfter += szSem;
				}
			}
		}
	}

	// struct constructors: Name(args) -> fbneo_make_Name(args)
	for (size_t si = 0; si < x.Structs.size(); si++) {
		const Struct& s = x.Structs[si];
		bool bUsed = false;
		for (size_t i = s.nEnd + 1; i + 1 < x.Toks.size(); i++) {
			if (x.Toks[i].nType == T_IDENT && x.Toks[i].szText == s.szName && x.Toks[i + 1].szText == "(" && x.Toks[i - 1].szText != "struct") {
				bUsed = true;
				break;
			}
		}
		if (!bUsed || s.Members.empty()) continue;

		x.Constructors.insert(s.szName);

		std::string szMaker = "\n" + s.szName + " fbneo_make_" + s.szName + "(";
		std::string szBody;
		int n = 0;
		for (size_t m = 0; m < s.Members.size(); m++) {
			const Member& mb = s.Members[m];
			if (mb.bSampler) continue;
			char szArg[32];
			sprintf(szArg, "fbneo_a%d", n);
			szMaker += std::string(n ? ", " : "") + mb.szType + " " + szArg + mb.szArray;
			szBody += "\tr." + mb.szName + " = " + szArg + ";\n";
			n++;
		}
		szMaker += ")\n{\n\t" + s.szName + " r;\n" + szBody + "\treturn r;\n}\n";
		x.Toks[s.nEnd].szAfter += szMaker;
	}

	// -------------------------------------------------------------
	// Sampler parameters of helper functions: add a SamplerState parameter after each, so lookups use
	// the right sampler (filter / wrap mode) for whichever texture is passed in

	for (size_t fi = 0; fi < x.Functions.size(); fi++) {
		Function& f = x.Functions[fi];
		if (&f == pVS || &f == pPS) continue;
		for (size_t i = 0; i < f.Params.size(); i++) {
			const Param& p = f.Params[i];
			if (!IsSamplerType(p.szType) || p.bOut || p.szName.empty() || !p.szArray.empty()) continue;
			x.Toks[p.nLast].szAfter += ", SamplerState fbneo_smp_" + p.szName;
			x.ParamTextures.insert(p.szName);
			x.SamplerParamFuncs[f.szName].insert(i);
		}
	}

	// -------------------------------------------------------------
	// Global sampler declarations -> Texture2D + SamplerState

	{
		int nDepth = 0;
		for (size_t i = 0; i + 1 < x.Toks.size(); i++) {
			Token& t = x.Toks[i];
			if (t.nType == T_PUNCT) {
				if (t.szText == "{" || t.szText == "(") nDepth++;
				if (t.szText == "}" || t.szText == ")") nDepth--;
				continue;
			}
			if (t.nType != T_IDENT || !IsSamplerType(t.szText)) continue;

			if (nDepth == 0 && x.Toks[i + 1].nType == T_IDENT) {
				// top level declaration
				std::string szName = x.Toks[i + 1].szText;
				size_t k = i + 2;
				if (k < x.Toks.size() && x.Toks[k].szText == "[") k = x.Match(k) + 1;
				// remove ": TEXUNITn" / ": register(sn)"
				if (k < x.Toks.size() && x.Toks[k].szText == ":") {
					size_t e = k;
					while (e + 1 < x.Toks.size() && x.Toks[e].szText != ";" && x.Toks[e].szText != "=") {
						x.Toks[e].szText = "";
						e++;
					}
				}
				size_t nSemi = i;
				while (nSemi + 1 < x.Toks.size() && x.Toks[nSemi].szText != ";") nSemi++;
				x.Toks[nSemi].szAfter += "\nSamplerState fbneo_smp_" + szName + ";";
				x.GlobalTextures.insert(szName);
			} else if (nDepth > 0 && x.Toks[i + 1].nType == T_IDENT && i + 3 < x.Toks.size() && x.Toks[i + 2].szText == "=") {
				// local alias: "sampler2D a = b;" -> the sampler state goes along with it
				std::string szName = x.Toks[i + 1].szText;
				std::string szSource;
				int nTokens = 0;
				size_t k = i + 3;
				for (; k < x.Toks.size() && x.Toks[k].szText != ";"; k++) {
					if (x.Toks[k].szText.empty()) continue;
					szSource = x.Toks[k].szText;
					nTokens++;
				}
				if (k < x.Toks.size()) {
					bool bKnown = (nTokens == 1) && (x.GlobalTextures.count(szSource) || x.ParamTextures.count(szSource));
					x.Toks[k].szAfter += " SamplerState fbneo_smp_" + szName + " = " + (bKnown ? "fbneo_smp_" + szSource : std::string("fbneo_smp")) + ";";
					x.ParamTextures.insert(szName);
				}
			}

			if (t.szText == "sampler1D") t.szText = "Texture1D";
			else if (t.szText == "sampler3D") t.szText = "Texture3D";
			else if (t.szText == "samplerCUBE") t.szText = "TextureCube";
			else t.szText = "Texture2D";
		}
	}

	// -------------------------------------------------------------
	// Assemble

	std::string Prelude =
		"// Translated from Cg by FBNeo\n"
		"#define fixed float\n#define fixed1 float1\n#define fixed2 float2\n#define fixed3 float3\n#define fixed4 float4\n"
		"#define fixed2x2 float2x2\n#define fixed3x3 float3x3\n#define fixed4x4 float4x4\n#define fixed3x4 float3x4\n#define fixed4x3 float4x3\n"
		"#define texture fbneo_texture\n#define sample fbneo_sample\n#define line fbneo_line\n#define point fbneo_point\n"
		"#define triangle fbneo_triangle\n#define linear fbneo_linear\n#define pass fbneo_pass\n#define string fbneo_string\n"
		"#define precise fbneo_precise\n#define shared fbneo_shared\n#define snorm fbneo_snorm\n#define unorm fbneo_unorm\n"
		"#define technique fbneo_technique\n#define compile fbneo_compile\n#define lineadj fbneo_lineadj\n#define triangleadj fbneo_triangleadj\n"
		"#define matrix fbneo_matrix\n#define vector fbneo_vector\n"
		"#ifdef FBNEO_LEVEL9\n"
		" #define FBNEO_SAMPLE_LEVEL(t, s, uv, l) t.Sample(s, uv)\n"
		" #define FBNEO_SAMPLE_BIAS(t, s, uv, b) t.Sample(s, uv)\n"
		"#else\n"
		" #define FBNEO_SAMPLE_LEVEL(t, s, uv, l) t.SampleLevel(s, uv, l)\n"
		" #define FBNEO_SAMPLE_BIAS(t, s, uv, b) t.SampleBias(s, uv, b)\n"
		"#endif\n"
		"#define FBNEO_SAMPLE_OFFSET(t, s, uv, o) t.SampleLevel(s, uv, 0, o)\n"
		"float fbneo_proj1(float2 v) { return v.x / v.y; }\nfloat fbneo_proj1(float4 v) { return v.x / v.w; }\n"
		"float2 fbneo_proj2(float3 v) { return v.xy / v.z; }\nfloat2 fbneo_proj2(float4 v) { return v.xy / v.w; }\n"
		"float3 fbneo_proj3(float4 v) { return v.xyz / v.w; }\n"
		"SamplerState fbneo_smp;\n";

	// GLSL-isms some of the shaders use
	{
		static const struct { const char* szName; const char* szDefine; } Glsl[] = {
			{ "fract", "#define fract frac\n" },
			{ "double", "#define double float\n" }, { "double2", "#define double2 float2\n" }, { "double3", "#define double3 float3\n" }, { "double4", "#define double4 float4\n" },
			{ "mix", "#define mix lerp\n" },
			{ "mod", "#define mod(x, y) ((x) - (y) * floor((x) / (y)))\n" },
			{ "inversesqrt", "#define inversesqrt rsqrt\n" },
			{ "dFdx", "#define dFdx ddx\n" },
			{ "dFdy", "#define dFdy ddy\n" },
			{ "vec2", "#define vec2 float2\n" }, { "vec3", "#define vec3 float3\n" }, { "vec4", "#define vec4 float4\n" },
			{ "ivec2", "#define ivec2 int2\n" }, { "ivec3", "#define ivec3 int3\n" }, { "ivec4", "#define ivec4 int4\n" },
			{ "bvec2", "#define bvec2 bool2\n" }, { "bvec3", "#define bvec3 bool3\n" }, { "bvec4", "#define bvec4 bool4\n" },
			{ "mat2", "#define mat2 float2x2\n" }, { "mat3", "#define mat3 float3x3\n" }, { "mat4", "#define mat4 float4x4\n" },
			{ "mat2x2", "#define mat2x2 float2x2\n" }, { "mat3x3", "#define mat3x3 float3x3\n" }, { "mat4x4", "#define mat4x4 float4x4\n" },
			{ NULL, NULL }
		};
		for (int g = 0; Glsl[g].szName; g++) {
			if (UsedIdents.count(Glsl[g].szName) && !x.UserFunctions.count(Glsl[g].szName) && !x.GlobalNames.count(Glsl[g].szName) &&
				!x.MemberNames.count(Glsl[g].szName) && !x.FindStruct(Glsl[g].szName)) {
				// also not when it's the name of a local variable / parameter ("float2 fract = ...")
				bool bLocal = false;
				for (size_t i = 1; i < x.Toks.size() && !bLocal; i++) {
					if (x.Toks[i].szText != Glsl[g].szName || x.Toks[i - 1].nType != T_IDENT) continue;
					const std::string& p = x.Toks[i - 1].szText;
					if (p != "return" && p != "else" && p != "case" && p != "do" && !IsQualifier(p) && p != "inline" && p != "typedef") bLocal = true;
				}
				if (!bLocal) {
					Prelude += Glsl[g].szDefine;
				}
			}
		}
	}

	// -------------------------------------------------------------
	// Cg links the vertex shader outputs to the pixel shader inputs by semantic, Direct3D 11 by
	// register. So the entry points become wrappers around the original functions, passing one
	// struct (built from the vertex shader outputs) between the two stages.

	std::string Wrappers;
	if (pVS && pPS) {
		Wrappers = MakeStageWrappers(x, pVS, pPS, EntryParams, MemberSem);
	}

	pOut->szHLSL = Prelude + Globals + x.Emit(0, x.Toks.size() - 1) + Wrappers;

	return true;
}

// ----------------------------------------------------------------------------
// Binding names

// "IN", "ORIG", "PREV3", "PASS2", "PASSPREV1", an alias, ... -> source
static bool ResolvePrefix(const std::string& p, const D3D11ShaderBindContext& Ctx, int* pSource, int* pIndex)
{
	*pIndex = 0;

	if (p.empty() || p == "IN" || p == "in" || p == "input" || p == "prev") {		// HLSL_4 "prev" cbuffer: assume input size
		*pSource = D3D11_SRC_INPUT;
		return true;
	}
	if (p == "ORIG" || p == "orig" || p == "ORIGtex") {
		*pSource = D3D11_SRC_ORIG;
		return true;
	}
	if (p == "PREV") {
		*pSource = D3D11_SRC_PREV;
		*pIndex = 1;
		return true;
	}
	if (StartsWith(p, "PASSPREV") && IsNumber(p.substr(8))) {
		int n = Ctx.nPass - atoi(p.substr(8).c_str());
		if (n < 0) {
			*pSource = D3D11_SRC_ORIG;
		} else {
			*pSource = D3D11_SRC_PASS;
			*pIndex = n;
		}
		return true;
	}
	if (StartsWith(p, "PREV") && IsNumber(p.substr(4))) {
		*pSource = D3D11_SRC_PREV;
		*pIndex = atoi(p.substr(4).c_str()) + 1;
		if (*pIndex > 7) *pIndex = 7;
		return true;
	}
	if (StartsWith(p, "PASS") && IsNumber(p.substr(4))) {
		int n = atoi(p.substr(4).c_str()) - 1;
		if (n < 0) {
			*pSource = D3D11_SRC_ORIG;
		} else {
			*pSource = D3D11_SRC_PASS;
			*pIndex = n;
		}
		return true;
	}
	if (Ctx.pAliases) {
		for (size_t i = 0; i < Ctx.pAliases->size(); i++) {
			if (!(*Ctx.pAliases)[i].empty() && (*Ctx.pAliases)[i] == p) {
				*pSource = D3D11_SRC_PASS;
				*pIndex = (int)i;
				return true;
			}
		}
	}
	if (Ctx.pLUTs) {
		for (size_t i = 0; i < Ctx.pLUTs->size(); i++) {
			if ((*Ctx.pLUTs)[i].szName == p) {
				*pSource = D3D11_SRC_LUT;
				*pIndex = (int)i;
				return true;
			}
		}
	}

	return false;
}

void D3D11ShaderResolveTexture(const std::string& szName, const D3D11ShaderBindContext& Ctx, int* pSource, int* pIndex)
{
	std::string n = szName;

	// "X_texture" (flattened Cg struct member) / "X.texture"
	static const char* Suffixes[] = { "_fbneo_texture", "_texture", ".texture", ".fbneo_texture", NULL };
	for (int s = 0; Suffixes[s]; s++) {
		size_t l = strlen(Suffixes[s]);
		if (n.size() > l && n.compare(n.size() - l, l, Suffixes[s]) == 0) {
			if (ResolvePrefix(n.substr(0, n.size() - l), Ctx, pSource, pIndex)) {
				return;
			}
		}
	}

	if (ResolvePrefix(n, Ctx, pSource, pIndex) && !n.empty() && n != "IN" && n != "in" && n != "input" && n != "prev") {
		return;
	}

	// anything else is the pass input (decal, s_p, s0, texture, Source, ...)
	*pSource = D3D11_SRC_INPUT;
	*pIndex = 0;
}

int D3D11ShaderResolveSampler(const std::string& szName, const D3D11ShaderBindContext& Ctx)
{
	std::string l = ToLower(szName);

	if (l.find("linear") != std::string::npos && !StartsWith(l, "fbneo_smp")) return 1;
	if (l.find("point") != std::string::npos && !StartsWith(l, "fbneo_smp")) return 0;

	if (StartsWith(szName, "fbneo_smp_")) {
		int nSource, nIndex;
		D3D11ShaderResolveTexture(szName.substr(10), Ctx, &nSource, &nIndex);
		if (nSource == D3D11_SRC_LUT && Ctx.pLUTs && nIndex < (int)Ctx.pLUTs->size()) {
			return (*Ctx.pLUTs)[nIndex].bLinear ? 1 : 0;
		}
	}

	// a LUT name used directly as a sampler (D3D9 style shaders)
	if (Ctx.pLUTs) {
		for (size_t i = 0; i < Ctx.pLUTs->size(); i++) {
			if ((*Ctx.pLUTs)[i].szName == szName) return (*Ctx.pLUTs)[i].bLinear ? 1 : 0;
		}
	}

	return -1;
}

void D3D11ShaderResolveUniform(const std::string& szName, const D3D11ShaderBindContext& Ctx, int* pUniform, int* pSource, int* pIndex)
{
	*pUniform = D3D11_UNI_NONE;
	*pSource = D3D11_SRC_INPUT;
	*pIndex = 0;

	// parameters first (exact name)
	if (Ctx.pParams) {
		for (size_t i = 0; i < Ctx.pParams->size(); i++) {
			if ((*Ctx.pParams)[i].szName == szName) {
				*pUniform = D3D11_UNI_PARAM;
				*pIndex = (int)i;
				return;
			}
		}
	}

	if (szName == "video_time") {
		*pUniform = D3D11_UNI_VIDEO_TIME;
		return;
	}

	static const struct { const char* szMember; int nUniform; } Members[] = {
		{ "video_size", D3D11_UNI_VIDEO_SIZE },
		{ "texture_size", D3D11_UNI_TEXTURE_SIZE },
		{ "output_size", D3D11_UNI_OUTPUT_SIZE },
		{ "frame_count", D3D11_UNI_FRAME_COUNT },
		{ "frame_direction", D3D11_UNI_FRAME_DIRECTION },
		{ "frame_rotation", D3D11_UNI_FRAME_ROTATION },
		{ NULL, 0 }
	};

	for (int m = 0; Members[m].szMember; m++) {
		size_t l = strlen(Members[m].szMember);
		if (szName.size() < l || szName.compare(szName.size() - l, l, Members[m].szMember) != 0) continue;

		std::string szPrefix = szName.substr(0, szName.size() - l);
		if (!szPrefix.empty()) {
			char c = szPrefix[szPrefix.size() - 1];
			if (c != '_' && c != '.') continue;
			szPrefix = szPrefix.substr(0, szPrefix.size() - 1);
		}

		int nSource, nIndex;
		if (ResolvePrefix(szPrefix, Ctx, &nSource, &nIndex)) {
			*pUniform = Members[m].nUniform;
			*pSource = nSource;
			*pIndex = nIndex;
			return;
		}
	}
}
