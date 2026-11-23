// FinalBurn Neo MAME-style artwork (.lay) support
//
// Supported:
//  - layout v2 (<mamelayout>): <element> with <image file/alphafile>, <rect>,
//    <disk>, <led7seg> components, state / statemask, <color>, <bounds>
//    (x/y/width/height, left/top/right/bottom, xc/yc)
//  - <view> with <screen>, <element ref name blend>, <group ref>, <repeat>,
//    <param> (~name~ substitution), <collection>, item <orientation>, <color>
//  - legacy <backdrop>/<overlay>/<bezel>/<cpanel>/<marquee> with MAME's
//    layer ordering, incl. the "Golly! Ghost!" multi-backdrop mode
//  - blend modes none/alpha/add/multiply
//  - outputs: an item's "name" attribute binds its element state to an output
//
// Not supported: <text> and the other LED/reel components,
// clickable inputs, multiple screens (only screen 0 is drawn).

#include "burnint.h"
#include "burn_artwork.h"
#include "spng.h"

// SVG images (src/dep/libs/plutosvg, built as plutosvg_all.o)
#define PLUTOVG_BUILD_STATIC
#define PLUTOSVG_BUILD_STATIC
#include "plutosvg.h"

#include <ctype.h>
#include <vector>
#include <string>
#include <map>
#include <utility>
#include <algorithm>
#include <math.h>

INT32 __cdecl ZipLoadOneFile(char* arcName, const char* fileName, void** Dest, INT32* pnWrote);

#if defined(BUILD_WIN32)
#include "zip.h"		// src/dep/libs/zip: reads the zips inside artwork.zip from memory
#define ART_NESTED_ZIP
#endif

char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, INT32 nOutSize);
#define _TtoA(a)	TCHARToANSI(a, NULL, 0)

#ifndef _tremove
#define _tremove remove
#endif

INT32 bBurnArtworkEnable = 1;
INT32 nBurnArtworkView = 0;
INT32 nBurnArtworkMaxHeight = 2160;
INT32 nBurnArtworkScale = -1;
INT32 nBurnArtworkDefaults = ARTWORK_DEFAULT_OVERLAY | ARTWORK_DEFAULT_BACKDROP | ARTWORK_DEFAULT_BEZEL; //ARTWORK_DEFAULT_ALL;	// parts shown in games without their own settings
INT32 bBurnArtworkLayered = 1;		// let frontends that can composite the artwork on the GPU do it (effects only touch the game)

extern "C" INT32 BurnDrvGetNativeSize(INT32 *pnWidth, INT32 *pnHeight);

// ---------------------------------------------------------------------------
// Outputs

struct burn_output {
	char name[64];
	INT32 value;
	INT32 set;		// written at least once
};

static std::vector<burn_output> outputs;
static INT32 outputs_changed = 0;

INT32 BurnOutputFind(const char *name)
{
	if (name == NULL || name[0] == 0) return -1;

	for (UINT32 i = 0; i < outputs.size(); i++) {
		if (strcmp(outputs[i].name, name) == 0) return i;
	}

	burn_output o;
	memset(&o, 0, sizeof(o));
	snprintf(o.name, sizeof(o.name), "%s", name);
	outputs.push_back(o);

	return outputs.size() - 1;
}

void BurnOutputSetHandle(INT32 handle, INT32 value)
{
	if (handle < 0 || handle >= (INT32)outputs.size()) return;

	burn_output &o = outputs[handle];
	if (o.value != value || o.set == 0) {
		o.value = value;
		o.set = 1;
		outputs_changed = 1;
	}
}

void BurnOutputSet(const char *name, INT32 value)
{
	BurnOutputSetHandle(BurnOutputFind(name), value);
}

void BurnOutputSetIndexed(const char *prefix, INT32 index, INT32 value)
{
	char name[64];
	snprintf(name, sizeof(name), "%s%d", prefix, index);
	BurnOutputSetHandle(BurnOutputFind(name), value);
}

INT32 BurnOutputGet(const char *name)
{
	for (UINT32 i = 0; i < outputs.size(); i++) {
		if (strcmp(outputs[i].name, name) == 0) return outputs[i].value;
	}
	return 0;
}

void BurnOutputReset()
{
	outputs.clear();
	outputs_changed = 1;
}

// ---------------------------------------------------------------------------
// Minimal XML parser

struct XNode {
	std::string name;
	std::vector<std::pair<std::string, std::string> > attr;
	std::vector<XNode*> kids;
	std::string raw;	// <data> only: everything between the start and end tag, as-is

	~XNode() { for (UINT32 i = 0; i < kids.size(); i++) delete kids[i]; }

	const std::string *get(const char *n) const {
		for (UINT32 i = 0; i < attr.size(); i++)
			if (attr[i].first == n) return &attr[i].second;
		return NULL;
	}
};

static const char *xml_p;
static const char *xml_end;

static std::string xml_unescape(const std::string &s)
{
	std::string o;
	for (UINT32 i = 0; i < s.size(); i++) {
		if (s[i] == '&') {
			size_t e = s.find(';', i);
			if (e != std::string::npos) {
				std::string ent = s.substr(i + 1, e - i - 1);
				if (ent == "lt") o += '<';
				else if (ent == "gt") o += '>';
				else if (ent == "amp") o += '&';
				else if (ent == "quot") o += '"';
				else if (ent == "apos") o += '\'';
				else if (ent.size() > 1 && ent[0] == '#') {
					INT32 c = (ent[1] == 'x') ? strtol(ent.c_str() + 2, NULL, 16) : atoi(ent.c_str() + 1);
					o += (c < 0x80) ? (char)c : '?';
				}
				i = e;
				continue;
			}
		}
		o += s[i];
	}
	return o;
}

static void xml_skip_ws() { while (xml_p < xml_end && (*xml_p == ' ' || *xml_p == '\t' || *xml_p == '\r' || *xml_p == '\n')) xml_p++; }

static INT32 xml_starts(const char *s) { size_t l = strlen(s); return (size_t)(xml_end - xml_p) >= l && memcmp(xml_p, s, l) == 0; }

static void xml_skip_to(const char *s)
{
	size_t l = strlen(s);
	while (xml_p < xml_end && !((size_t)(xml_end - xml_p) >= l && memcmp(xml_p, s, l) == 0)) xml_p++;
	xml_p += l;
	if (xml_p > xml_end) xml_p = xml_end;
}

static XNode *xml_parse_node();

static void xml_parse_children(XNode *n)
{
	while (xml_p < xml_end) {
		// text / whitespace
		while (xml_p < xml_end && *xml_p != '<') xml_p++;
		if (xml_p >= xml_end) return;

		if (xml_starts("<!--")) { xml_skip_to("-->"); continue; }
		if (xml_starts("<![CDATA[")) { xml_skip_to("]]>"); continue; }
		if (xml_starts("<?") || xml_starts("<!")) { xml_skip_to(">"); continue; }
		if (xml_starts("</")) { xml_skip_to(">"); return; }

		XNode *k = xml_parse_node();
		if (k == NULL) return;
		n->kids.push_back(k);
	}
}

static XNode *xml_parse_node()
{
	if (xml_p >= xml_end || *xml_p != '<') return NULL;
	xml_p++;

	XNode *n = new XNode;
	while (xml_p < xml_end && !strchr(" \t\r\n/>", *xml_p)) n->name += *xml_p++;

	for (;;) {
		xml_skip_ws();
		if (xml_p >= xml_end) return n;

		if (*xml_p == '/') { xml_skip_to(">"); return n; }	// self-closing
		if (*xml_p == '>') {
			xml_p++;
			const char *inner = xml_p;
			xml_parse_children(n);
			if (n->name == "data") {
				// inline image data: keep the source text (CDATA, escaped text or raw <svg> markup)
				const char *e = xml_p - 1;		// xml_p is just past "</data>": find its "</"
				while (e > inner && !(e[0] == '<' && e + 1 < xml_end && e[1] == '/')) e--;
				n->raw.assign(inner, e - inner);
			}
			return n;
		}

		std::string an, av;
		while (xml_p < xml_end && !strchr(" \t\r\n=/>", *xml_p)) an += *xml_p++;
		xml_skip_ws();
		if (xml_p < xml_end && *xml_p == '=') {
			xml_p++;
			xml_skip_ws();
			char q = *xml_p;
			if (q == '"' || q == '\'') {
				xml_p++;
				while (xml_p < xml_end && *xml_p != q) av += *xml_p++;
				xml_p++;
			} else {
				while (xml_p < xml_end && !strchr(" \t\r\n/>", *xml_p)) av += *xml_p++;
			}
		}
		if (an.size()) n->attr.push_back(std::make_pair(an, xml_unescape(av)));
		else xml_p++; // garbage, skip it
	}
}

static XNode *xml_parse(const char *buf, INT32 len)
{
	xml_p = buf;
	xml_end = buf + len;

	XNode *root = new XNode;
	root->name = "#root";
	xml_parse_children(root);

	return root;
}

// ---------------------------------------------------------------------------
// Bitmaps (premultiplied ARGB)

struct abitmap {
	INT32 w, h;
	std::vector<UINT32> p;
	std::vector<INT32> spans;		// per row: count, then [start, end) pairs of non-transparent pixels
	std::vector<INT32> span_row;	// offset into spans for each row
	abitmap() : w(0), h(0) { }
	void alloc(INT32 nw, INT32 nh) { w = nw; h = nh; p.assign((size_t)nw * nh, 0); spans.clear(); span_row.clear(); }

	void build_spans() {
		spans.clear();
		span_row.assign(h, 0);
		for (INT32 y = 0; y < h; y++) {
			span_row[y] = spans.size();
			spans.push_back(0);
			const UINT32 *r = &p[(size_t)y * w];
			INT32 x = 0, n = 0;
			while (x < w) {
				while (x < w && r[x] == 0) x++;
				if (x >= w) break;
				INT32 st = x;
				while (x < w && r[x] != 0) x++;
				spans.push_back(st);
				spans.push_back(x);
				n++;
			}
			spans[span_row[y]] = n;
		}
	}
};

static inline UINT32 premul(UINT32 a, UINT32 r, UINT32 g, UINT32 b)
{
	return (a << 24) | (((r * a + 127) / 255) << 16) | (((g * a + 127) / 255) << 8) | ((b * a + 127) / 255);
}

static inline void blend_over(UINT32 &d, UINT32 s)
{
	UINT32 sa = s >> 24;
	if (sa == 0xff) { d = s; return; }
	if (sa == 0 && (s & 0xffffff) == 0) return;
	UINT32 inv = 255 - sa;
	UINT32 a = sa + (((d >> 24) * inv + 127) / 255);
	UINT32 r = ((s >> 16) & 0xff) + ((((d >> 16) & 0xff) * inv + 127) / 255);
	UINT32 g = ((s >>  8) & 0xff) + ((((d >>  8) & 0xff) * inv + 127) / 255);
	UINT32 b = ((s >>  0) & 0xff) + ((((d >>  0) & 0xff) * inv + 127) / 255);
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	if (a > 255) a = 255;
	d = (a << 24) | (r << 16) | (g << 8) | b;
}

// separable resampler: box filter when shrinking, linear when enlarging
struct rs_tap { INT32 idx; float w; };

static void rs_weights(INT32 src, INT32 dst, std::vector<std::vector<rs_tap> > &out)
{
	out.assign(dst, std::vector<rs_tap>());
	float scale = (float)src / dst;

	for (INT32 i = 0; i < dst; i++) {
		if (scale > 1.0f) {
			float s0 = i * scale, s1 = (i + 1) * scale, tot = 0;
			for (INT32 k = (INT32)s0; k < (INT32)ceilf(s1) && k < src; k++) {
				float a = (k < s0) ? s0 : (float)k;
				float b = (k + 1 > s1) ? s1 : (float)(k + 1);
				if (b > a) { rs_tap t = { k, b - a }; out[i].push_back(t); tot += b - a; }
			}
			for (UINT32 k = 0; k < out[i].size(); k++) out[i][k].w /= tot;
		} else {
			float c = (i + 0.5f) * scale - 0.5f;
			INT32 k0 = (INT32)floorf(c);
			float f = c - k0;
			INT32 a = k0 < 0 ? 0 : (k0 >= src ? src - 1 : k0);
			INT32 b = k0 + 1 < 0 ? 0 : (k0 + 1 >= src ? src - 1 : k0 + 1);
			rs_tap t0 = { a, 1.0f - f }, t1 = { b, f };
			out[i].push_back(t0);
			out[i].push_back(t1);
		}
	}
}

static void resample(const abitmap &src, INT32 dw, INT32 dh, abitmap &dst)
{
	dst.alloc(dw, dh);
	if (src.w <= 0 || src.h <= 0 || dw <= 0 || dh <= 0) return;

	std::vector<std::vector<rs_tap> > wx, wy;
	rs_weights(src.w, dw, wx);
	rs_weights(src.h, dh, wy);

	std::vector<float> tmp((size_t)dw * src.h * 4);

	for (INT32 y = 0; y < src.h; y++) {
		const UINT32 *s = &src.p[(size_t)y * src.w];
		float *t = &tmp[(size_t)y * dw * 4];
		for (INT32 x = 0; x < dw; x++, t += 4) {
			float a = 0, r = 0, g = 0, b = 0;
			for (UINT32 k = 0; k < wx[x].size(); k++) {
				UINT32 px = s[wx[x][k].idx];
				float w = wx[x][k].w;
				a += (px >> 24) * w; r += ((px >> 16) & 0xff) * w; g += ((px >> 8) & 0xff) * w; b += (px & 0xff) * w;
			}
			t[0] = a; t[1] = r; t[2] = g; t[3] = b;
		}
	}

	for (INT32 y = 0; y < dh; y++) {
		UINT32 *d = &dst.p[(size_t)y * dw];
		for (INT32 x = 0; x < dw; x++) {
			float a = 0, r = 0, g = 0, b = 0;
			for (UINT32 k = 0; k < wy[y].size(); k++) {
				const float *t = &tmp[((size_t)wy[y][k].idx * dw + x) * 4];
				float w = wy[y][k].w;
				a += t[0] * w; r += t[1] * w; g += t[2] * w; b += t[3] * w;
			}
			UINT32 ia = (UINT32)(a + 0.5f), ir = (UINT32)(r + 0.5f), ig = (UINT32)(g + 0.5f), ib = (UINT32)(b + 0.5f);
			if (ia > 255) ia = 255;
			if (ir > ia) ir = ia;
			if (ig > ia) ig = ia;
			if (ib > ia) ib = ia;
			d[x] = (ia << 24) | (ir << 16) | (ig << 8) | ib;
		}
	}
}

// ---------------------------------------------------------------------------
// Artwork file access

static char art_archive[MAX_PATH * 2];	// zip path without extension ("" if none)
static char art_folder[MAX_PATH * 2];	// folder with trailing slash ("" if none)

// artwork.zip, holding every game's artwork: <game>.zip inside it, or a <game>/ folder
static char art_pack[MAX_PATH * 2];		// artwork.zip path without extension, when <game>/ is a folder in it
static char art_pack_dir[MAX_PATH];		// "<game>/"
#ifdef ART_NESTED_ZIP
static struct zip_t *art_inner = NULL;	// <game>.zip from artwork.zip, read from memory
static void *art_inner_buf = NULL;
#endif

static void art_close_archives()
{
#ifdef ART_NESTED_ZIP
	if (art_inner) zip_stream_close(art_inner);
	art_inner = NULL;
	if (art_inner_buf) free(art_inner_buf);
	art_inner_buf = NULL;
#endif
	art_archive[0] = art_folder[0] = art_pack[0] = art_pack_dir[0] = 0;
}

static INT32 file_exists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f) { fclose(f); return 1; }
	return 0;
}

// malloc()'d buffer, NULL if not found
static UINT8 *art_load_file(const char *name, INT32 *len)
{
	*len = 0;

	if (art_archive[0]) {
		void *dest = NULL;
		INT32 l = 0;
		if (ZipLoadOneFile(art_archive, name, &dest, &l) == 0 && dest && l > 0) {
			*len = l;
			return (UINT8*)dest;
		}
		if (dest) free(dest);
	}
#ifdef ART_NESTED_ZIP
	if (art_inner && zip_entry_open(art_inner, name) == 0) {
		void *buf = NULL;
		size_t l = 0;
		ssize_t r = zip_entry_read(art_inner, &buf, &l);
		zip_entry_close(art_inner);
		if (r > 0 && buf) {
			// callers want a 0 after the data (text files)
			UINT8 *out = (UINT8*)realloc(buf, l + 1);
			if (out) {
				out[l] = 0;
				*len = (INT32)l;
				return out;
			}
		}
		if (buf) free(buf);
	}
#endif

	if (art_pack[0]) {
		char path[MAX_PATH * 2];
		snprintf(path, sizeof(path), "%s%s", art_pack_dir, name);
		void *dest = NULL;
		INT32 l = 0;
		if (ZipLoadOneFile(art_pack, path, &dest, &l) == 0 && dest && l > 0) {
			*len = l;
			return (UINT8*)dest;
		}
		if (dest) free(dest);
	}

	if (art_folder[0]) {
		char path[MAX_PATH * 3];
		snprintf(path, sizeof(path), "%s%s", art_folder, name);
		FILE *f = fopen(path, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			long l = ftell(f);
			fseek(f, 0, SEEK_SET);
			UINT8 *buf = (UINT8*)malloc(l + 1);
			if (buf && fread(buf, 1, l, f) == (size_t)l) {
				buf[l] = 0;
				fclose(f);
				*len = l;
				return buf;
			}
			free(buf);
			fclose(f);
		}
	}

	return NULL;
}

static INT32 art_locate(const char *game)
{
	const char *base = _TtoA(szAppArtworkPath);
	char path[MAX_PATH * 3];

	art_close_archives();

	static const char *ext[] = { ".zip", ".7z" };
	for (INT32 e = 0; e < 2; e++) {
		snprintf(path, sizeof(path), "%s%s%s", base, game, ext[e]);
		if (file_exists(path)) {
			snprintf(art_archive, sizeof(art_archive), "%s%s", base, game);
			break;
		}
	}

	snprintf(path, sizeof(path), "%s%s/default.lay", base, game);
	if (file_exists(path)) {
		snprintf(art_folder, sizeof(art_folder), "%s%s/", base, game);
	}

	if (art_archive[0] || art_folder[0]) return 1;

	// artwork.zip (or .7z) with every game in it
	for (INT32 e = 0; e < 2; e++) {
		snprintf(path, sizeof(path), "%sartwork%s", base, ext[e]);
		if (!file_exists(path)) continue;

		char pack[MAX_PATH * 2], name[MAX_PATH];
		snprintf(pack, sizeof(pack), "%sartwork", base);
		void *dest = NULL;
		INT32 l = 0;

#ifdef ART_NESTED_ZIP
		// <game>.zip inside it
		snprintf(name, sizeof(name), "%s.zip", game);
		if (ZipLoadOneFile(pack, name, &dest, &l) == 0 && dest && l > 0) {
			art_inner = zip_stream_open((const char*)dest, l, 0, 'r');
			if (art_inner) {
				art_inner_buf = dest;		// the zip reads from it, keep it until art_close_archives()
				bprintf(PRINT_NORMAL, _T("Artwork: using %S from artwork%S\n"), name, ext[e]);
				return 1;
			}
		}
		if (dest) free(dest);
		dest = NULL;
#endif

		// or a <game>/ folder inside it
		snprintf(name, sizeof(name), "%s/default.lay", game);
		if (ZipLoadOneFile(pack, name, &dest, &l) == 0 && dest && l > 0) {
			free(dest);
			snprintf(art_pack, sizeof(art_pack), "%s", pack);
			snprintf(art_pack_dir, sizeof(art_pack_dir), "%s/", game);
			bprintf(PRINT_NORMAL, _T("Artwork: using %S/ from artwork%S\n"), game, ext[e]);
			return 1;
		}
		if (dest) free(dest);
	}

	return 0;
}

static INT32 png_decode(const UINT8 *data, INT32 len, abitmap &bm)
{
	spng_ctx *ctx = spng_ctx_new(0);
	if (ctx == NULL) return 1;

	spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);
	spng_set_chunk_limits(ctx, 64 * 1024 * 1024, 64 * 1024 * 1024);

	struct spng_ihdr ihdr;
	size_t size = 0;

	if (spng_set_png_buffer(ctx, data, len) || spng_get_ihdr(ctx, &ihdr) || spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &size)) {
		spng_ctx_free(ctx);
		return 1;
	}

	std::vector<UINT8> rgba(size);
	if (spng_decode_image(ctx, &rgba[0], size, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS)) {
		spng_ctx_free(ctx);
		return 1;
	}
	spng_ctx_free(ctx);

	bm.alloc(ihdr.width, ihdr.height);
	for (size_t i = 0; i < (size_t)ihdr.width * ihdr.height; i++) {
		const UINT8 *s = &rgba[i * 4];
		bm.p[i] = premul(s[3], s[0], s[1], s[2]);
	}

	return 0;
}

static INT32 load_png(const std::string &name, abitmap &bm)
{
	INT32 len = 0;
	UINT8 *buf = art_load_file(name.c_str(), &len);
	if (buf == NULL) {
		bprintf(PRINT_ERROR, _T("Artwork: can't find %S\n"), name.c_str());
		return 1;
	}

	INT32 ret = png_decode(buf, len, bm);
	free(buf);

	if (ret) bprintf(PRINT_ERROR, _T("Artwork: can't decode %S (PNG or SVG only)\n"), name.c_str());

	return ret;
}

static INT32 is_png_data(const UINT8 *d, INT32 len)
{
	return len >= 8 && memcmp(d, "\x89PNG\r\n\x1a\n", 8) == 0;
}

static INT32 is_svg_data(const UINT8 *d, INT32 len)
{
	INT32 i = 0;
	if (len >= 3 && d[0] == 0xef && d[1] == 0xbb && d[2] == 0xbf) i = 3;	// UTF-8 BOM
	while (i < len && (d[i] == ' ' || d[i] == '\t' || d[i] == '\r' || d[i] == '\n')) i++;
	return i < len && d[i] == '<';
}

static void svg_free_data(void *closure) { free(closure); }

// takes ownership of buf (malloc'd)
static plutosvg_document_t *svg_load(UINT8 *buf, INT32 len)
{
	plutosvg_document_t *doc = plutosvg_document_load_from_data((const char*)buf, len, -1, -1, svg_free_data, buf);
	if (doc == NULL) return NULL;	// plutosvg has already called svg_free_data()
	if (plutosvg_document_get_width(doc) <= 0 || plutosvg_document_get_height(doc) <= 0) {
		plutosvg_document_destroy(doc);		// nothing to draw (no size / viewBox)
		return NULL;
	}
	return doc;
}

// rasterize at exactly w x h (stretched to fit, like MAME), premultiplied ARGB
static INT32 svg_render(plutosvg_document_t *doc, INT32 w, INT32 h, abitmap &bm)
{
	plutovg_surface_t *surf = plutosvg_document_render_to_surface(doc, NULL, w, h, NULL, NULL, NULL);
	if (surf == NULL) return 1;

	bm.alloc(w, h);
	const UINT8 *p = plutovg_surface_get_data(surf);
	INT32 stride = plutovg_surface_get_stride(surf);
	for (INT32 y = 0; y < h; y++)
		memcpy(&bm.p[y * w], p + y * stride, w * 4);	// both are native-endian premultiplied 0xAARRGGBB

	plutovg_surface_destroy(surf);
	return 0;
}

static INT32 base64_decode(const std::string &in, std::vector<UINT8> &out)
{
	UINT32 acc = 0;
	INT32 bits = 0;
	out.clear();
	for (size_t i = 0; i < in.size(); i++) {
		INT32 c = (UINT8)in[i], v;
		if (c >= 'A' && c <= 'Z') v = c - 'A';
		else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
		else if (c >= '0' && c <= '9') v = c - '0' + 52;
		else if (c == '+' || c == '-') v = 62;
		else if (c == '/' || c == '_') v = 63;
		else if (c == '=') break;
		else continue;	// whitespace
		acc = (acc << 6) | v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out.push_back((acc >> bits) & 0xff);
		}
	}
	return out.empty();
}

// ---------------------------------------------------------------------------
// Layout data

struct lrect {
	float x0, y0, x1, y1;
	lrect() : x0(0), y0(0), x1(1), y1(1) { }
	lrect(float a, float b, float c, float d) : x0(a), y0(b), x1(c), y1(d) { }
	float w() const { return x1 - x0; }
	float h() const { return y1 - y0; }
	void unite(const lrect &o) {
		if (o.x0 < x0) x0 = o.x0;
		if (o.y0 < y0) y0 = o.y0;
		if (o.x1 > x1) x1 = o.x1;
		if (o.y1 > y1) y1 = o.y1;
	}
};

struct lcolor {
	float r, g, b, a;
	lcolor() : r(1), g(1), b(1), a(1) { }
};

enum { COMP_RECT = 0, COMP_DISK, COMP_IMAGE, COMP_LED7SEG };

struct lcomponent {
	INT32 type;
	INT32 state;		// -1 = any
	INT32 statemask;
	lrect bounds;
	lcolor color;
	std::string file, alphafile;
	std::string data;	// inline <data> image (SVG text or base64 PNG)
	INT32 loaded;		// image: 0 = not yet, 1 = ok, -1 = failed
	abitmap img;		// PNG
	plutosvg_document_t *svg;	// SVG: rendered at the size it's drawn at, owned by the element
	std::map<UINT64, abitmap> raster;	// image rasterized / scaled per size (w << 32 | h), so a new state only blends
	lcomponent() : type(COMP_RECT), state(-1), statemask(~0), loaded(0), svg(NULL) { }
};

struct lelement {
	std::string name;
	INT32 defstate;
	std::vector<lcomponent> comps;
	~lelement() {
		for (UINT32 i = 0; i < comps.size(); i++)
			if (comps[i].svg) plutosvg_document_destroy(comps[i].svg);
	}
};

enum { BLEND_DEFAULT = -1, BLEND_NONE = 0, BLEND_ALPHA, BLEND_MULTIPLY, BLEND_ADD };

struct litem {
	INT32 is_screen;
	INT32 screen_index;
	lelement *elem;
	INT32 output;		// output handle, -1 if not bound
	lrect bounds;
	lcolor color;
	INT32 blend;
	INT32 orient;		// bit0 swapxy, bit1 flipx, bit2 flipy (applied in that order)
	UINT32 vis;			// collections (view toggles) the item is in: shown only if all of them are on

	// render state
	INT32 px0, py0, px1, py1;
	INT32 last_state;
	std::map<INT32, abitmap> cache;
	INT32 cache_max;	// states kept before the cache is flushed

	litem() : is_screen(0), screen_index(0), elem(NULL), output(-1), blend(BLEND_DEFAULT), orient(0), vis(0), px0(0), py0(0), px1(0), py1(0), last_state(-0x7fffffff), cache_max(64) { }
};

struct layer_lists {
	std::vector<litem> screens, backdrops, overlays, bezels, cpanels, marquees;
};

struct lview {
	std::string name;
	std::vector<litem> all;		// every item, in drawing order
	std::vector<litem> items;	// the visible ones (see view_apply())
	INT32 has_bounds;
	lrect ebounds;				// explicit <bounds>
	lrect bounds;				// in use: explicit, or the union of the visible items

	// MAME "visibility toggles": named <collection>s (and the legacy layers), max 32
	std::vector<std::string> tog_name;
	UINT32 tog_def;				// on by default (layout's visible="")
	UINT32 tog_on;				// currently on
	lview() : has_bounds(0), tog_def(0), tog_on(0) { }
};

static std::map<std::string, lelement*> elements;
static std::map<std::string, const XNode*> groups;
static std::vector<lview*> views;
static std::vector<INT32> view_list;	// views that show screen 0 (what the frontend gets to pick from)
static INT32 cur_view = 0;				// index into view_list
static lview *parse_view = NULL;	// view being parsed
static UINT32 cur_vis = 0;			// collections enclosing the items being parsed

// toggle bit for a named collection in the view (shared by name), 0 if out of bits
static UINT32 view_toggle(lview *v, const std::string &name, INT32 visible)
{
	for (UINT32 i = 0; i < v->tog_name.size(); i++)
		if (v->tog_name[i] == name) return 1u << i;
	if (v->tog_name.size() >= 32) {
		bprintf(PRINT_ERROR, _T("Artwork: view %S has too many collections, %S is always shown\n"), v->name.c_str(), name.c_str());
		return 0;
	}
	UINT32 bit = 1u << v->tog_name.size();
	v->tog_name.push_back(name);
	if (visible) v->tog_def |= bit;
	return bit;
}
static XNode *layout_root = NULL;

typedef std::map<std::string, std::string> param_map;
static std::vector<param_map> env;

static std::string subst(const std::string &s)
{
	if (s.find('~') == std::string::npos) return s;

	std::string o;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '~') {
			size_t e = s.find('~', i + 1);
			if (e != std::string::npos) {
				std::string key = s.substr(i + 1, e - i - 1);
				INT32 found = 0;
				for (INT32 k = env.size() - 1; k >= 0; k--) {
					param_map::iterator it = env[k].find(key);
					if (it != env[k].end()) { o += it->second; found = 1; break; }
				}
				if (found) { i = e; continue; }
			}
		}
		o += s[i];
	}
	return o;
}

static std::string attr_str(const XNode *n, const char *name, const char *def = "")
{
	const std::string *v = n->get(name);
	return v ? subst(*v) : std::string(def);
}

static INT32 attr_has(const XNode *n, const char *name) { return n->get(name) != NULL; }

static float attr_float(const XNode *n, const char *name, float def)
{
	const std::string *v = n->get(name);
	if (v == NULL) return def;
	return (float)atof(subst(*v).c_str());
}

static INT32 attr_int(const XNode *n, const char *name, INT32 def)
{
	const std::string *v = n->get(name);
	if (v == NULL) return def;
	std::string s = subst(*v);
	const char *c = s.c_str();
	while (*c == ' ') c++;
	if (c[0] == '$') return strtol(c + 1, NULL, 16);
	if (c[0] == '0' && (c[1] == 'x' || c[1] == 'X')) return strtol(c + 2, NULL, 16);
	if (c[0] == '#') return strtol(c + 1, NULL, 10);
	return strtol(c, NULL, 10);
}

static const XNode *child(const XNode *n, const char *name)
{
	for (UINT32 i = 0; i < n->kids.size(); i++)
		if (n->kids[i]->name == name) return n->kids[i];
	return NULL;
}

static lrect parse_bounds(const XNode *n)
{
	const XNode *b = child(n, "bounds");
	if (b == NULL) return lrect(0, 0, 1, 1);

	if (attr_has(b, "left") || attr_has(b, "right") || attr_has(b, "top") || attr_has(b, "bottom")) {
		return lrect(attr_float(b, "left", 0), attr_float(b, "top", 0), attr_float(b, "right", 1), attr_float(b, "bottom", 1));
	}

	float w = attr_float(b, "width", 1), h = attr_float(b, "height", 1);
	if (attr_has(b, "xc") || attr_has(b, "yc")) {
		float xc = attr_float(b, "xc", 0), yc = attr_float(b, "yc", 0);
		return lrect(xc - w / 2, yc - h / 2, xc + w / 2, yc + h / 2);
	}

	float x = attr_float(b, "x", 0), y = attr_float(b, "y", 0);
	return lrect(x, y, x + w, y + h);
}

static lcolor parse_color(const XNode *n)
{
	lcolor c;
	const XNode *k = child(n, "color");
	if (k) {
		c.r = attr_float(k, "red", 1);
		c.g = attr_float(k, "green", 1);
		c.b = attr_float(k, "blue", 1);
		c.a = attr_float(k, "alpha", 1);
	}
	return c;
}

static void parse_param(const XNode *n, INT32 repeat_index)
{
	std::string name = attr_str(n, "name");
	if (name.empty() || env.empty()) return;

	if (attr_has(n, "value")) {
		env.back()[name] = attr_str(n, "value");
		return;
	}

	if (attr_has(n, "start")) {
		std::string st = attr_str(n, "start");
		std::string inc = attr_str(n, "increment", "0");
		INT32 is_float = (st.find('.') != std::string::npos) || (inc.find('.') != std::string::npos);
		char buf[64];
		if (is_float) {
			snprintf(buf, sizeof(buf), "%g", atof(st.c_str()) + atof(inc.c_str()) * repeat_index);
		} else {
			INT32 v = attr_int(n, "start", 0);
			INT32 i = attr_int(n, "increment", 0);
			v += i * repeat_index;
			INT32 ls = attr_int(n, "lshift", 0), rs = attr_int(n, "rshift", 0);
			if (ls) v <<= ls * 1;
			if (rs) v >>= rs * 1;
			snprintf(buf, sizeof(buf), "%d", v);
		}
		env.back()[name] = buf;
	}
}

static void parse_element(const XNode *n)
{
	std::string name = attr_str(n, "name");
	if (name.empty()) return;

	lelement *e = new lelement;
	e->name = name;
	e->defstate = attr_int(n, "defstate", -1);

	for (UINT32 i = 0; i < n->kids.size(); i++) {
		const XNode *k = n->kids[i];
		lcomponent c;

		if (k->name == "rect") c.type = COMP_RECT;
		else if (k->name == "disk") c.type = COMP_DISK;
		else if (k->name == "led7seg") c.type = COMP_LED7SEG;
		else if (k->name == "image") {
			c.type = COMP_IMAGE;
			c.file = attr_str(k, "file");
			c.alphafile = attr_str(k, "alphafile");
			if (c.file.empty()) {
				for (UINT32 j = 0; j < k->kids.size(); j++)
					if (k->kids[j]->name == "data") c.data = k->kids[j]->raw;
				if (c.data.empty()) {
					bprintf(PRINT_ERROR, _T("Artwork: element %S: <image> without a file or data\n"), name.c_str());
					continue;
				}
			}
		}
		else {
			if (k->name != "bounds" && k->name != "color")
				bprintf(PRINT_NORMAL, _T("Artwork: element %S: component <%S> not supported\n"), name.c_str(), k->name.c_str());
			continue;
		}

		c.state = attr_int(k, "state", -1);
		c.statemask = attr_int(k, "statemask", ~0);
		c.bounds = parse_bounds(k);
		c.color = parse_color(k);

		e->comps.push_back(c);
	}

	// normalise component bounds to the union of all components
	if (e->comps.size()) {
		lrect u = e->comps[0].bounds;
		for (UINT32 i = 1; i < e->comps.size(); i++) u.unite(e->comps[i].bounds);
		float w = u.w() > 0 ? u.w() : 1, h = u.h() > 0 ? u.h() : 1;
		for (UINT32 i = 0; i < e->comps.size(); i++) {
			lrect &b = e->comps[i].bounds;
			b = lrect((b.x0 - u.x0) / w, (b.y0 - u.y0) / h, (b.x1 - u.x0) / w, (b.y1 - u.y0) / h);
		}
	}

	if (elements.count(name)) delete elements[name];
	elements[name] = e;
}

static INT32 parse_blend(const XNode *n, INT32 def)
{
	if (!attr_has(n, "blend")) return def;
	std::string b = attr_str(n, "blend");
	if (b == "none") return BLEND_NONE;
	if (b == "alpha") return BLEND_ALPHA;
	if (b == "multiply") return BLEND_MULTIPLY;
	if (b == "add") return BLEND_ADD;
	return def;
}

static INT32 parse_orientation(const XNode *n)
{
	const XNode *o = child(n, "orientation");
	if (o == NULL) return 0;

	INT32 r = 0;
	switch (attr_int(o, "rotate", 0)) {
		case 90:  r = 1 | 2; break;		// swap, flipx
		case 180: r = 2 | 4; break;
		case 270: r = 1 | 4; break;		// swap, flipy
	}
	if (attr_str(o, "swapxy") == "yes") r ^= 1;
	if (attr_str(o, "flipx") == "yes") r ^= 2;
	if (attr_str(o, "flipy") == "yes") r ^= 4;
	return r;
}

static void add_items(const XNode *parent, layer_lists &layers, INT32 depth, INT32 repeat_index = 0);

static void add_group_instance(const XNode *n, layer_lists &layers, INT32 depth)
{
	std::string ref = attr_str(n, "ref");
	std::map<std::string, const XNode*>::iterator it = groups.find(ref);
	if (it == groups.end()) {
		bprintf(PRINT_ERROR, _T("Artwork: unknown group %S\n"), ref.c_str());
		return;
	}

	// collect the group's items in group space
	layer_lists local;
	env.push_back(param_map());
	add_items(it->second, local, depth + 1);
	env.pop_back();

	std::vector<litem> *lists[6] = { &local.screens, &local.backdrops, &local.overlays, &local.bezels, &local.cpanels, &local.marquees };
	std::vector<litem> *dest[6]  = { &layers.screens, &layers.backdrops, &layers.overlays, &layers.bezels, &layers.cpanels, &layers.marquees };

	// group bounds: explicit, or union of contents
	lrect gb;
	INT32 have = 0;
	if (child(it->second, "bounds")) {
		gb = parse_bounds(it->second);
		have = 1;
	} else {
		for (INT32 l = 0; l < 6; l++)
			for (UINT32 i = 0; i < lists[l]->size(); i++) {
				if (!have) { gb = (*lists[l])[i].bounds; have = 1; }
				else gb.unite((*lists[l])[i].bounds);
			}
	}

	INT32 map_it = have && child(n, "bounds") && gb.w() > 0 && gb.h() > 0;
	lrect ib = parse_bounds(n);
	lcolor ic = parse_color(n);

	for (INT32 l = 0; l < 6; l++) {
		for (UINT32 i = 0; i < lists[l]->size(); i++) {
			litem it2 = (*lists[l])[i];
			if (map_it) {
				lrect &b = it2.bounds;
				float sx = ib.w() / gb.w(), sy = ib.h() / gb.h();
				b = lrect(ib.x0 + (b.x0 - gb.x0) * sx, ib.y0 + (b.y0 - gb.y0) * sy, ib.x0 + (b.x1 - gb.x0) * sx, ib.y0 + (b.y1 - gb.y0) * sy);
			}
			it2.color.r *= ic.r; it2.color.g *= ic.g; it2.color.b *= ic.b; it2.color.a *= ic.a;
			dest[l]->push_back(it2);
		}
	}
}

static void add_items(const XNode *parent, layer_lists &layers, INT32 depth, INT32 repeat_index)
{
	if (depth > 32) return;

	for (UINT32 i = 0; i < parent->kids.size(); i++) {
		const XNode *n = parent->kids[i];
		const std::string &nm = n->name;

		if (nm == "param") {
			parse_param(n, repeat_index);
			continue;
		}

		if (nm == "repeat") {
			INT32 count = attr_int(n, "count", 0);
			for (INT32 r = 0; r < count && r < 4096; r++) {
				env.push_back(param_map());
				add_items(n, layers, depth + 1, r);
				env.pop_back();
			}
			continue;
		}

		if (nm == "collection") {
			std::string cname = attr_str(n, "name");
			INT32 visible = (attr_str(n, "visible", "yes") != "no");
			if (cname.empty() || parse_view == NULL) {
				if (!visible) continue;		// unnamed: can't be toggled
			}
			UINT32 saved_vis = cur_vis;
			if (!cname.empty() && parse_view) cur_vis |= view_toggle(parse_view, cname, visible);
			env.push_back(param_map());
			add_items(n, layers, depth + 1);
			env.pop_back();
			cur_vis = saved_vis;
			continue;
		}

		if (nm == "group") {
			add_group_instance(n, layers, depth);
			continue;
		}

		litem it;
		it.bounds = parse_bounds(n);
		it.color = parse_color(n);
		it.orient = parse_orientation(n);
		it.vis = cur_vis;

		std::vector<litem> *list = NULL;

		if (nm == "screen") {
			it.is_screen = 1;
			it.screen_index = attr_int(n, "index", 0);
			it.blend = parse_blend(n, BLEND_DEFAULT);
			list = &layers.screens;
		} else {
			std::string ref;
			if (nm == "element") { ref = attr_str(n, "ref"); list = &layers.screens; it.blend = parse_blend(n, BLEND_ALPHA); }
			else if (nm == "backdrop") { ref = attr_str(n, "element"); list = &layers.backdrops; it.blend = parse_blend(n, BLEND_ALPHA); }
			else if (nm == "overlay")  { ref = attr_str(n, "element"); list = &layers.overlays;  it.blend = parse_blend(n, BLEND_MULTIPLY); }
			else if (nm == "bezel")    { ref = attr_str(n, "element"); list = &layers.bezels;    it.blend = parse_blend(n, BLEND_ALPHA); }
			else if (nm == "cpanel")   { ref = attr_str(n, "element"); list = &layers.cpanels;   it.blend = parse_blend(n, BLEND_ALPHA); }
			else if (nm == "marquee")  { ref = attr_str(n, "element"); list = &layers.marquees;  it.blend = parse_blend(n, BLEND_ALPHA); }
			else continue;	// bounds, color, orientation, etc.

			std::map<std::string, lelement*>::iterator e = elements.find(ref);
			if (e == elements.end()) {
				bprintf(PRINT_ERROR, _T("Artwork: unknown element %S\n"), ref.c_str());
				continue;
			}
			it.elem = e->second;

			std::string out = attr_str(n, "name");
			if (!out.empty()) it.output = BurnOutputFind(out.c_str());
		}

		list->push_back(it);
	}
}

static void parse_layout(const XNode *root)
{
	const XNode *ml = child(root, "mamelayout");
	if (ml == NULL) {
		bprintf(PRINT_ERROR, _T("Artwork: no <mamelayout> in layout file\n"));
		return;
	}

	env.clear();
	env.push_back(param_map());

	for (UINT32 i = 0; i < ml->kids.size(); i++) {
		const XNode *n = ml->kids[i];
		if (n->name == "param") parse_param(n, 0);
		else if (n->name == "element") parse_element(n);
		else if (n->name == "group") { std::string g = attr_str(n, "name"); if (!g.empty()) groups[g] = n; }
	}

	for (UINT32 i = 0; i < ml->kids.size(); i++) {
		const XNode *n = ml->kids[i];
		if (n->name != "view") continue;

		lview *v = new lview;
		v->name = attr_str(n, "name");

		layer_lists layers;
		parse_view = v;
		cur_vis = 0;
		env.push_back(param_map());
		add_items(n, layers, 0);
		env.pop_back();
		parse_view = NULL;

		// legacy layers can be toggled too (MAME names them like this)
		{
			std::vector<litem> *ll[5] = { &layers.backdrops, &layers.overlays, &layers.bezels, &layers.cpanels, &layers.marquees };
			const char *ln[5] = { "Backdrops", "Overlays", "Bezels", "Control Panels", "Marquees" };
			for (INT32 l = 0; l < 5; l++) {
				if (ll[l]->empty()) continue;
				UINT32 bit = view_toggle(v, ln[l], 1);
				for (UINT32 k = 0; k < ll[l]->size(); k++) (*ll[l])[k].vis |= bit;
			}
		}

		// MAME's legacy layer ordering
		if (!layers.overlays.empty() || layers.backdrops.size() <= 1) {
			// screens + overlays (multiply) + backdrop (add) + bezels + cpanels + marquees
			for (UINT32 k = 0; k < layers.backdrops.size(); k++)
				layers.backdrops[k].blend = BLEND_ADD;
			v->all.insert(v->all.end(), layers.screens.begin(), layers.screens.end());
			v->all.insert(v->all.end(), layers.overlays.begin(), layers.overlays.end());
			v->all.insert(v->all.end(), layers.backdrops.begin(), layers.backdrops.end());
		} else {
			// multiple backdrop pieces and no overlays (Golly! Ghost! mode):
			// backdrop (alpha) + screens (add) + bezels + cpanels + marquees
			for (UINT32 k = 0; k < layers.screens.size(); k++)
				if (layers.screens[k].is_screen && layers.screens[k].blend == BLEND_DEFAULT) layers.screens[k].blend = BLEND_ADD;
			v->all.insert(v->all.end(), layers.backdrops.begin(), layers.backdrops.end());
			v->all.insert(v->all.end(), layers.screens.begin(), layers.screens.end());
		}
		v->all.insert(v->all.end(), layers.bezels.begin(), layers.bezels.end());
		v->all.insert(v->all.end(), layers.cpanels.begin(), layers.cpanels.end());
		v->all.insert(v->all.end(), layers.marquees.begin(), layers.marquees.end());

		v->has_bounds = (child(n, "bounds") != NULL);
		if (v->has_bounds) v->ebounds = parse_bounds(n);
		v->tog_on = v->tog_def;

		views.push_back(v);
	}
}

// ---------------------------------------------------------------------------
// Element rendering

static void draw_seg_h(abitmap &d, INT32 minx, INT32 maxx, INT32 midy, INT32 width, UINT32 color)
{
	for (INT32 y = 0; y < width / 2; y++) {
		INT32 ty = (y < width / 8) ? width / 8 : y;
		for (INT32 x = minx + ty; x < maxx - ty; x++) {
			d.p[(midy - y) * d.w + x] = color;
			d.p[(midy + y) * d.w + x] = color;
		}
	}
}

static void draw_seg_v(abitmap &d, INT32 miny, INT32 maxy, INT32 midx, INT32 width, UINT32 color)
{
	for (INT32 x = 0; x < width / 2; x++) {
		INT32 tx = (x < width / 8) ? width / 8 : x;
		for (INT32 y = miny + tx; y < maxy - tx; y++) {
			d.p[y * d.w + midx - x] = color;
			d.p[y * d.w + midx + x] = color;
		}
	}
}

static void draw_seg_dp(abitmap &d, INT32 midx, INT32 midy, INT32 width, UINT32 color)
{
	width /= 2;
	float oor2 = 1.0f / (float)(width * width);
	for (INT32 y = 0; y <= width; y++) {
		float xval = width * sqrtf(1.0f - (float)(y * y) * oor2);
		INT32 left = midx - (INT32)(xval + 0.5f), right = midx + (INT32)(xval + 0.5f);
		for (INT32 x = left; x < right; x++) {
			d.p[(midy - y) * d.w + x] = color;
			d.p[(midy + y) * d.w + x] = color;
		}
	}
}

// MAME's led7seg component
static void render_led7seg(abitmap &out, INT32 state)
{
	const INT32 bmwidth = 250, bmheight = 400, segwidth = 40, skewwidth = 40;
	const UINT32 on = 0xffffffff, off = premul(0x20, 0xff, 0xff, 0xff);

	out.alloc(bmwidth + skewwidth, bmheight);

	draw_seg_h(out, 0 + 2*segwidth/3, bmwidth - 2*segwidth/3, 0 + segwidth/2, segwidth, (state & 0x01) ? on : off);
	draw_seg_v(out, 0 + 2*segwidth/3, bmheight/2 - segwidth/3, bmwidth - segwidth/2, segwidth, (state & 0x02) ? on : off);
	draw_seg_v(out, bmheight/2 + segwidth/3, bmheight - 2*segwidth/3, bmwidth - segwidth/2, segwidth, (state & 0x04) ? on : off);
	draw_seg_h(out, 0 + 2*segwidth/3, bmwidth - 2*segwidth/3, bmheight - segwidth/2, segwidth, (state & 0x08) ? on : off);
	draw_seg_v(out, bmheight/2 + segwidth/3, bmheight - 2*segwidth/3, 0 + segwidth/2, segwidth, (state & 0x10) ? on : off);
	draw_seg_v(out, 0 + 2*segwidth/3, bmheight/2 - segwidth/3, 0 + segwidth/2, segwidth, (state & 0x20) ? on : off);
	draw_seg_h(out, 0 + 2*segwidth/3, bmwidth - 2*segwidth/3, bmheight/2, segwidth, (state & 0x40) ? on : off);

	// skew
	for (INT32 y = 0; y < out.h; y++) {
		UINT32 *row = &out.p[y * out.w];
		INT32 offs = skewwidth * (out.h - y) / out.h;
		for (INT32 x = out.w - skewwidth - 1; x >= 0; x--) row[x + offs] = row[x];
		for (INT32 x = 0; x < offs; x++) row[x] = 0;
	}

	draw_seg_dp(out, bmwidth + segwidth/2, bmheight - segwidth/2, segwidth, (state & 0x80) ? on : off);
}

static void tint(UINT32 &px, const lcolor &c)
{
	UINT32 a = px >> 24, r = (px >> 16) & 0xff, g = (px >> 8) & 0xff, b = px & 0xff;
	float fa = c.a < 0 ? 0 : (c.a > 1 ? 1 : c.a);
	a = (UINT32)(a * fa + 0.5f);
	r = (UINT32)(r * fa * (c.r > 1 ? 1 : c.r) + 0.5f);
	g = (UINT32)(g * fa * (c.g > 1 ? 1 : c.g) + 0.5f);
	b = (UINT32)(b * fa * (c.b > 1 ? 1 : c.b) + 0.5f);
	px = (a << 24) | (r << 16) | (g << 8) | b;
}

static INT32 is_white(const lcolor &c) { return c.r >= 1.0f && c.g >= 1.0f && c.b >= 1.0f && c.a >= 1.0f; }

static void comp_load(lcomponent &c)
{
	if (c.loaded) return;
	c.loaded = -1;

	UINT8 *buf = NULL;
	INT32 len = 0;
	const char *what = c.file.c_str();

	if (c.file.empty()) {
		// inline: CDATA / raw markup / escaped text is SVG, anything else base64 PNG
		what = "inline image";
		std::string d = c.data;
		size_t a = d.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) return;
		d = d.substr(a);
		if (d.compare(0, 9, "<![CDATA[") == 0) {
			size_t e = d.find("]]>");
			d = d.substr(9, (e == std::string::npos ? d.size() : e) - 9);
		} else if (d[0] != '<') {
			std::string u = xml_unescape(d);
			if (u.find('<') != std::string::npos) {
				d = u;		// escaped SVG markup
			} else {
				std::vector<UINT8> png;
				if (base64_decode(d, png) || png_decode(&png[0], png.size(), c.img)) {
					bprintf(PRINT_ERROR, _T("Artwork: can't decode %S\n"), what);
					return;
				}
				c.loaded = 1;
				return;
			}
		}
		len = d.size();
		buf = (UINT8*)malloc(len + 1);
		if (buf == NULL) return;
		memcpy(buf, d.c_str(), len + 1);
	} else {
		buf = art_load_file(c.file.c_str(), &len);
		if (buf == NULL) {
			bprintf(PRINT_ERROR, _T("Artwork: can't find %S\n"), what);
			return;
		}
	}

	if (!is_png_data(buf, len) && is_svg_data(buf, len)) {
		c.svg = svg_load(buf, len);		// takes buf
		if (c.svg == NULL) {
			bprintf(PRINT_ERROR, _T("Artwork: can't decode SVG %S\n"), what);
			return;
		}
		c.loaded = 1;
		return;
	}

	INT32 ret = png_decode(buf, len, c.img);
	free(buf);
	if (ret) {
		bprintf(PRINT_ERROR, _T("Artwork: can't decode %S (PNG or SVG only)\n"), what);
		return;
	}

	if (!c.alphafile.empty()) {
		abitmap a;
		if (load_png(c.alphafile, a) == 0 && a.w == c.img.w && a.h == c.img.h) {
			for (size_t i = 0; i < c.img.p.size(); i++) {
				UINT32 s = c.img.p[i], al = (a.p[i] >> 16) & 0xff; // alpha from red channel
				UINT32 sa = s >> 24;
				// un-premultiply, then premultiply with the new alpha
				UINT32 r = sa ? ((s >> 16) & 0xff) * 255 / sa : 0;
				UINT32 g = sa ? ((s >>  8) & 0xff) * 255 / sa : 0;
				UINT32 b = sa ? ((s >>  0) & 0xff) * 255 / sa : 0;
				c.img.p[i] = premul(al, r, g, b);
			}
		}
	}

	c.loaded = 1;
}

// an image component at its drawn size: SVGs are rasterized (always sharp), PNGs
// scaled. done once per size and kept, rasterizing is far too slow to do mid-game
static abitmap *comp_raster(lcomponent &c, INT32 cw, INT32 ch)
{
	comp_load(c);
	if (c.loaded != 1) return NULL;

	UINT64 key = ((UINT64)cw << 32) | (UINT32)ch;
	std::map<UINT64, abitmap>::iterator r = c.raster.find(key);
	if (r != c.raster.end()) return r->second.p.empty() ? NULL : &r->second;

	abitmap &s = c.raster[key];
	if (c.svg) {
		if (svg_render(c.svg, cw, ch, s)) { s.alloc(0, 0); return NULL; }	// remember the failure
	} else {
		resample(c.img, cw, ch, s);
	}
	return &s;
}

// a component's pixel rectangle inside a w x h element, returns 0 if empty
static INT32 comp_rect(const lcomponent &c, INT32 w, INT32 h, INT32 &x0, INT32 &y0, INT32 &x1, INT32 &y1)
{
	x0 = (INT32)floorf(c.bounds.x0 * w + 0.5f); x1 = (INT32)floorf(c.bounds.x1 * w + 0.5f);
	y0 = (INT32)floorf(c.bounds.y0 * h + 0.5f); y1 = (INT32)floorf(c.bounds.y1 * h + 0.5f);
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > w) x1 = w;
	if (y1 > h) y1 = h;
	return (x1 > x0 && y1 > y0);
}

static void element_render(lelement *e, INT32 state, INT32 w, INT32 h, abitmap &out)
{
	out.alloc(w, h);

	for (UINT32 i = 0; i < e->comps.size(); i++) {
		lcomponent &c = e->comps[i];

		if (c.state != -1 && (state & c.statemask) != c.state) continue;

		INT32 x0, y0, x1, y1;
		if (!comp_rect(c, w, h, x0, y0, x1, y1)) continue;
		INT32 cw = x1 - x0, ch = y1 - y0;

		switch (c.type)
		{
			case COMP_RECT: {
				UINT32 col = 0xffffffff;
				tint(col, c.color);
				for (INT32 y = y0; y < y1; y++)
					for (INT32 x = x0; x < x1; x++) blend_over(out.p[y * w + x], col);
			}
			break;

			case COMP_DISK: {
				UINT32 col = 0xffffffff;
				tint(col, c.color);
				float rx = cw / 2.0f, ry = ch / 2.0f, cx = x0 + rx, cy = y0 + ry;
				for (INT32 y = y0; y < y1; y++) {
					float dy = (y + 0.5f - cy) / ry;
					if (dy * dy > 1.0f) continue;
					float hw = rx * sqrtf(1.0f - dy * dy);
					INT32 l = (INT32)floorf(cx - hw + 0.5f), r = (INT32)floorf(cx + hw + 0.5f);
					if (l < x0) l = x0;
					if (r > x1) r = x1;
					for (INT32 x = l; x < r; x++) blend_over(out.p[y * w + x], col);
				}
			}
			break;

			case COMP_IMAGE: {
				abitmap *sp = comp_raster(c, cw, ch);
				if (sp == NULL) break;
				abitmap &s = *sp;
				INT32 white = is_white(c.color);
				for (INT32 y = 0; y < ch; y++)
					for (INT32 x = 0; x < cw; x++) {
						UINT32 px = s.p[y * cw + x];
						if (!white) tint(px, c.color);
						blend_over(out.p[(y0 + y) * w + x0 + x], px);
					}
			}
			break;

			case COMP_LED7SEG: {
				abitmap seg, s;
				render_led7seg(seg, state);
				resample(seg, cw, ch, s);
				for (INT32 y = 0; y < ch; y++)
					for (INT32 x = 0; x < cw; x++) {
						UINT32 px = s.p[y * cw + x];
						tint(px, c.color);
						blend_over(out.p[(y0 + y) * w + x0 + x], px);
					}
			}
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// Compositor

static INT32 art_active = 0;
static INT32 art_probed = 0;			// layout parsed by BurnArtworkProbe(), not running
static lview *view = NULL;
static INT32 first_screen = -1;		// index of the first screen item in view->items
static INT32 is_vector = 0;
static INT32 orient_v = 0, orient_f = 0;

static INT32 native_w = 0, native_h = 0;	// driver's framebuffer (unrotated)
static INT32 game_w = 0, game_h = 0;		// game screen in display orientation
static INT32 disp_w = 0, disp_h = 0;		// composited output, display orientation

static std::vector<UINT32> bg;			// static part (everything before the first screen)
static std::vector<UINT32> disp;		// composited frame (0x00RRGGBB)
static INT32 bg_dirty = 1;
static INT32 full_dirty = 1;			// recompose everything (not just the screen area)
static INT32 clip_x0, clip_y0, clip_x1, clip_y1;	// area being composed this pass

static UINT8 *native_buf = NULL;
static INT32 native_buf_size = 0;
static UINT8 *saved_draw = NULL;
static INT32 saved_pitch = 0;
static INT32 resized_in_frame = 0;
static INT32 aspect_x = 4, aspect_y = 3;
// layered output (see layers_update())
static INT32 layered = 0;				// set by the frontend (BurnArtworkSetLayered)
static std::vector<UINT32> lay_under, lay_add, lay_mul;	// driver orientation, 0xffRRGGBB
static INT32 lay_w = 0, lay_h = 0;
static INT32 lay_full = 1;				// everything needs (re)building
static INT32 lay_serial = 1;			// bumped when the layers change
static INT32 lay_sx0, lay_sy0, lay_sx1, lay_sy1;	// screen rectangle (driver orientation)
static std::vector<INT32> lay_dirty;	// display rectangles (x0, y0, x1, y1) the last layers_update() changed
// the over-screen items under the first one bound to an output never change: their
// A / B are kept here (display orientation), so a lamp only redoes the items from it up
static std::vector<UINT32> pre_add, pre_mul;
static UINT32 pre_first = 0;			// first item not in pre_add / pre_mul
static INT32 pre_valid = 0;
// over-screen items that changed in the last layers_update() (item, its old state)
static std::vector<std::pair<INT32, INT32> > lay_chg;
// per screen row: runs where A / B aren't "nothing" (compose_area() skips the rest)
static std::vector<INT32> fold_runs, fold_row;
static INT32 fold_serial = 0;
static INT32 lay_ab_serial = 1;			// bumped when A / B change (not for background-only changes)
// for the frontend: lay_serial at which U / A + B last changed, and what changed at lay_serial (layer coordinates)
static INT32 lay_under_serial = 1, lay_over_serial = 1;
static INT32 lay_dx0 = 0, lay_dy0 = 0, lay_dx1 = 0, lay_dy1 = 0;
// A / B for each combination of the output-driven items' states seen (blinking lamps
// go back and forth between a few): switching back is a copy instead of a rebuild
struct lay_memo {
	std::vector<INT32> key;
	std::vector<UINT32> add, mul;
	UINT32 used;
};
static std::vector<lay_memo*> lay_memos;
static UINT32 lay_memo_clock = 0;
static void lay_memo_clear()
{
	for (UINT32 i = 0; i < lay_memos.size(); i++) delete lay_memos[i];
	lay_memos.clear();
}
static void layers_update();
static INT32 told_w = 0, told_h = 0;		// size the frontend last asked for (= its buffer)
static INT32 target_w = 0, target_h = 0;	// frontend output area (auto scale mode)
static INT32 frame_depth = 0;			// drivers often call BurnDrvRedraw() from their Frame()

// pixel format (derived from BurnHighCol)
static INT32 fmt_bpp = 0;
static UINT32 fmt_mask[3];
static INT32 fmt_shift[3];
static UINT32 fmt_max[3];
static INT32 fmt_direct = 0;		// 32bpp 0x00RRGGBB
static std::vector<UINT32> fmt_lut16;

static void fmt_update()
{
	UINT32 m[3] = { BurnHighCol(0xff, 0, 0, 0), BurnHighCol(0, 0xff, 0, 0), BurnHighCol(0, 0, 0xff, 0) };

	if (fmt_bpp == nBurnBpp && m[0] == fmt_mask[0] && m[1] == fmt_mask[1] && m[2] == fmt_mask[2]) return;

	fmt_bpp = nBurnBpp;
	for (INT32 i = 0; i < 3; i++) {
		fmt_mask[i] = m[i];
		fmt_shift[i] = 0;
		while (m[i] && !((m[i] >> fmt_shift[i]) & 1)) fmt_shift[i]++;
		fmt_max[i] = m[i] >> fmt_shift[i];
		if (fmt_max[i] == 0) fmt_max[i] = 1;
	}

	fmt_direct = (nBurnBpp == 4 && m[0] == 0xff0000 && m[1] == 0xff00 && m[2] == 0xff);

	if (nBurnBpp == 2) {
		fmt_lut16.resize(0x10000);
		for (UINT32 p = 0; p < 0x10000; p++) {
			UINT32 c[3];
			for (INT32 i = 0; i < 3; i++) c[i] = ((p & fmt_mask[i]) >> fmt_shift[i]) * 255 / fmt_max[i];
			fmt_lut16[p] = (c[0] << 16) | (c[1] << 8) | c[2];
		}
	}
}

static inline UINT32 fmt_read(const UINT8 *p)
{
	UINT32 v;
	switch (fmt_bpp) {
		case 2: return fmt_lut16[*(const UINT16*)p];
		case 3: v = p[0] | (p[1] << 8) | (p[2] << 16); break;
		default: v = *(const UINT32*)p; if (fmt_direct) return v & 0xffffff; break;
	}
	UINT32 c[3];
	for (INT32 i = 0; i < 3; i++) c[i] = ((v & fmt_mask[i]) >> fmt_shift[i]) * 255 / fmt_max[i];
	return (c[0] << 16) | (c[1] << 8) | c[2];
}

static inline void fmt_write(UINT8 *p, UINT32 rgb)
{
	if (fmt_direct) { *(UINT32*)p = rgb; return; }

	UINT32 c[3] = { (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff };
	UINT32 v = 0;
	for (INT32 i = 0; i < 3; i++) v |= ((c[i] * fmt_max[i] + 127) / 255) << fmt_shift[i];

	switch (fmt_bpp) {
		case 2: *(UINT16*)p = v; break;
		case 3: p[0] = v; p[1] = v >> 8; p[2] = v >> 16; break;
		default: *(UINT32*)p = v; break;
	}
}

static INT32 item_state(const litem &it)
{
	if (it.output >= 0 && it.output < (INT32)outputs.size() && outputs[it.output].set)
		return outputs[it.output].value;
	return it.elem ? it.elem->defstate : 0;
}

static abitmap *item_bitmap(litem &it, INT32 state)
{
	std::map<INT32, abitmap>::iterator c = it.cache.find(state);
	if (c != it.cache.end()) return &c->second;

	if (it.cache.size() > (size_t)it.cache_max) it.cache.clear();

	INT32 w = it.px1 - it.px0, h = it.py1 - it.py0;
	if (w <= 0 || h <= 0) return NULL;

	abitmap &out = it.cache[state];

	INT32 swap = it.orient & 1;
	abitmap tmp;
	element_render(it.elem, state, swap ? h : w, swap ? w : h, tmp);

	out.alloc(w, h);
	for (INT32 y = 0; y < h; y++) {
		for (INT32 x = 0; x < w; x++) {
			// map destination pixel back through the orientation
			INT32 sx = x, sy = y;
			if (it.orient & 2) sx = w - 1 - sx;
			if (it.orient & 4) sy = h - 1 - sy;
			if (swap) { INT32 t = sx; sx = sy; sy = t; }
			UINT32 px = tmp.p[sy * tmp.w + sx];
			if (!is_white(it.color)) tint(px, it.color);
			out.p[y * w + x] = px;
		}
	}

	out.build_spans();

	return &out;
}

static void apply_bitmap(std::vector<UINT32> &dst, const litem &it, const abitmap *bm)
{
	if (bm == NULL) return;

	INT32 sx0 = 0, sy0 = 0, sx1 = bm->w, sy1 = bm->h;
	if (it.px0 + sx0 < clip_x0) sx0 = clip_x0 - it.px0;
	if (it.py0 + sy0 < clip_y0) sy0 = clip_y0 - it.py0;
	if (it.px0 + sx1 > clip_x1) sx1 = clip_x1 - it.px0;
	if (it.py0 + sy1 > clip_y1) sy1 = clip_y1 - it.py0;
	if (sx1 <= sx0 || sy1 <= sy0) return;

	INT32 use_spans = (it.blend == BLEND_ALPHA || it.blend == BLEND_ADD) && !bm->span_row.empty();

	for (INT32 y = sy0; y < sy1; y++) {
		if (use_spans) {
			// only visit the non-transparent runs of this row
			const INT32 *sp = &bm->spans[bm->span_row[y]];
			INT32 count = sp[0];
			for (INT32 k = 0; k < count; k++) {
				INT32 a = sp[1 + k * 2], b = sp[2 + k * 2];
				if (a < sx0) a = sx0;
				if (b > sx1) b = sx1;
				if (a >= b) continue;
				const UINT32 *s = &bm->p[y * bm->w + a];
				UINT32 *d = &dst[(it.py0 + y) * disp_w + it.px0 + a];
				INT32 n = b - a;
				if (it.blend == BLEND_ADD) {
					for (INT32 x = 0; x < n; x++) {
						UINT32 sp2 = s[x], dp = d[x];
						UINT32 r = ((sp2 >> 16) & 0xff) + ((dp >> 16) & 0xff);
						UINT32 g = ((sp2 >>  8) & 0xff) + ((dp >>  8) & 0xff);
						UINT32 bb = ((sp2 >>  0) & 0xff) + ((dp >>  0) & 0xff);
						if (r > 255) r = 255;
						if (g > 255) g = 255;
						if (bb > 255) bb = 255;
						d[x] = (r << 16) | (g << 8) | bb;
					}
				} else {
					for (INT32 x = 0; x < n; x++) {
						UINT32 sp2 = s[x], sa = sp2 >> 24;
						if (sa == 0xff) { d[x] = sp2 & 0xffffff; continue; }
						UINT32 dp = d[x], inv = 255 - sa;
						UINT32 r = ((sp2 >> 16) & 0xff) + (((dp >> 16) & 0xff) * inv + 127) / 255;
						UINT32 g = ((sp2 >>  8) & 0xff) + (((dp >>  8) & 0xff) * inv + 127) / 255;
						UINT32 bb = ((sp2 >>  0) & 0xff) + (((dp >>  0) & 0xff) * inv + 127) / 255;
						if (r > 255) r = 255;
						if (g > 255) g = 255;
						if (bb > 255) bb = 255;
						d[x] = (r << 16) | (g << 8) | bb;
					}
				}
			}
			continue;
		}

		const UINT32 *s = &bm->p[y * bm->w + sx0];
		UINT32 *d = &dst[(it.py0 + y) * disp_w + it.px0 + sx0];
		INT32 n = sx1 - sx0;

		switch (it.blend)
		{
			case BLEND_NONE:
				for (INT32 x = 0; x < n; x++) d[x] = s[x] & 0xffffff;
			break;

			case BLEND_MULTIPLY:
				for (INT32 x = 0; x < n; x++) {
					UINT32 sp = s[x], dp = d[x];
					if ((sp & 0xffffff) == 0xffffff) continue;
					UINT32 r = (((sp >> 16) & 0xff) * ((dp >> 16) & 0xff) + 127) / 255;
					UINT32 g = (((sp >>  8) & 0xff) * ((dp >>  8) & 0xff) + 127) / 255;
					UINT32 b = (((sp >>  0) & 0xff) * ((dp >>  0) & 0xff) + 127) / 255;
					d[x] = (r << 16) | (g << 8) | b;
				}
			break;

			case BLEND_ADD:
				for (INT32 x = 0; x < n; x++) {
					UINT32 sp = s[x];
					if ((sp & 0xffffff) == 0) continue;
					UINT32 dp = d[x];
					UINT32 r = ((sp >> 16) & 0xff) + ((dp >> 16) & 0xff);
					UINT32 g = ((sp >>  8) & 0xff) + ((dp >>  8) & 0xff);
					UINT32 b = ((sp >>  0) & 0xff) + ((dp >>  0) & 0xff);
					if (r > 255) r = 255;
					if (g > 255) g = 255;
					if (b > 255) b = 255;
					d[x] = (r << 16) | (g << 8) | b;
				}
			break;

			default: // alpha (premultiplied)
				for (INT32 x = 0; x < n; x++) {
					UINT32 sp = s[x], sa = sp >> 24;
					if (sa == 0) continue;
					if (sa == 0xff) { d[x] = sp & 0xffffff; continue; }
					UINT32 dp = d[x], inv = 255 - sa;
					UINT32 r = ((sp >> 16) & 0xff) + (((dp >> 16) & 0xff) * inv + 127) / 255;
					UINT32 g = ((sp >>  8) & 0xff) + (((dp >>  8) & 0xff) * inv + 127) / 255;
					UINT32 b = ((sp >>  0) & 0xff) + (((dp >>  0) & 0xff) * inv + 127) / 255;
					if (r > 255) r = 255;
					if (g > 255) g = 255;
					if (b > 255) b = 255;
					d[x] = (r << 16) | (g << 8) | b;
				}
			break;
		}
	}
}

// native framebuffer pixel index of display-oriented game coordinates (u, v),
// split into a u-part and a v-part (every orientation is separable)
static inline INT32 game_index_u(INT32 u)
{
	if (orient_v) return orient_f ? (native_h - 1 - u) * native_w : u * native_w;	// ROT90 / ROT270
	return orient_f ? (native_w - 1 - u) : u;										// ROT180 / ROT0
}

static inline INT32 game_index_v(INT32 v)
{
	if (orient_v) return orient_f ? v : (native_w - 1 - v);
	return (orient_f ? (native_h - 1 - v) : v) * native_w;
}

// per-channel saturating add of two 0x00RRGGBB pixels
static inline UINT32 sat_add24(UINT32 a, UINT32 b)
{
	UINT32 t = (a & 0x7f7f7f) + (b & 0x7f7f7f);				// low 7 bits of each channel, no cross-channel carry
	UINT32 sum = t ^ ((a ^ b) & 0x808080);					// per-channel sum, mod 256
	UINT32 carry = ((a & b) | (t & (a | b))) & 0x808080;	// channels that overflowed
	return (sum | ((carry >> 7) * 0xff)) & 0xffffff;
}

// base: if not NULL, the pixels under the screen come from here instead of dst
// (lets compose_area skip copying the background in first)
static void apply_screen(std::vector<UINT32> &dst, const litem &it, const UINT32 *base = NULL)
{
	if (native_buf == NULL) return;

	INT32 w = it.px1 - it.px0, h = it.py1 - it.py0;
	if (w <= 0 || h <= 0) return;

	INT32 x0 = 0, y0 = 0, x1 = w, y1 = h;
	if (it.px0 + x0 < clip_x0) x0 = clip_x0 - it.px0;
	if (it.py0 + y0 < clip_y0) y0 = clip_y0 - it.py0;
	if (it.px0 + x1 > clip_x1) x1 = clip_x1 - it.px0;
	if (it.py0 + y1 > clip_y1) y1 = clip_y1 - it.py0;
	if (x1 <= x0 || y1 <= y0) return;

	INT32 blend = it.blend;
	if (blend == BLEND_DEFAULT) blend = is_vector ? BLEND_ADD : BLEND_NONE;
	if (blend == BLEND_ALPHA) blend = BLEND_NONE; // the game screen is opaque

	INT32 white = is_white(it.color);
	UINT32 cr = (UINT32)(it.color.r * it.color.a * 256), cg = (UINT32)(it.color.g * it.color.a * 256), cb = (UINT32)(it.color.b * it.color.a * 256);

	std::vector<INT32> col(w);
	for (INT32 x = x0; x < x1; x++) col[x] = game_index_u((INT32)(((INT64)x * game_w * 2 + game_w) / (w * 2)));

	const UINT32 *src32 = (const UINT32*)native_buf;
	const UINT16 *src16 = (const UINT16*)native_buf;
	INT32 prev_row = -1;

	for (INT32 y = y0; y < y1; y++) {
		INT32 row = game_index_v((INT32)(((INT64)y * game_h * 2 + game_h) / (h * 2)));
		INT32 doff = (it.py0 + y) * disp_w + it.px0;
		UINT32 *d = &dst[doff];
		const UINT32 *bsrc = base ? base + doff : d;

		if (fmt_direct && white && (blend == BLEND_NONE || blend == BLEND_ADD)) {
			const UINT32 *srow = src32 + row;
			if (blend == BLEND_NONE) {
				if (y > y0 && row == prev_row) {
					memcpy(d + x0, d - disp_w + x0, (x1 - x0) * sizeof(UINT32));	// same source line as the row above
				} else {
					for (INT32 x = x0; x < x1; x++) d[x] = srow[col[x]] & 0xffffff;
				}
				prev_row = row;
			} else {
				for (INT32 x = x0; x < x1; x++) d[x] = sat_add24(bsrc[x], srow[col[x]] & 0xffffff);
			}
			continue;
		}

		if (base) memcpy(d + x0, bsrc + x0, (x1 - x0) * sizeof(UINT32));

		for (INT32 x = x0; x < x1; x++) {
			INT32 idx = row + col[x];
			UINT32 s;
			if (fmt_direct)         s = src32[idx] & 0xffffff;
			else if (fmt_bpp == 2)  s = fmt_lut16[src16[idx]];
			else                    s = fmt_read(native_buf + idx * fmt_bpp);

			if (!white) {
				UINT32 r = (((s >> 16) & 0xff) * cr) >> 8, g = (((s >> 8) & 0xff) * cg) >> 8, b = ((s & 0xff) * cb) >> 8;
				if (r > 255) r = 255;
				if (g > 255) g = 255;
				if (b > 255) b = 255;
				s = (r << 16) | (g << 8) | b;
			}

			switch (blend)
			{
				case BLEND_ADD: {
					if (s == 0) break;
					UINT32 dp = d[x];
					UINT32 r = ((s >> 16) & 0xff) + ((dp >> 16) & 0xff);
					UINT32 g = ((s >>  8) & 0xff) + ((dp >>  8) & 0xff);
					UINT32 b = ((s >>  0) & 0xff) + ((dp >>  0) & 0xff);
					if (r > 255) r = 255;
					if (g > 255) g = 255;
					if (b > 255) b = 255;
					d[x] = (r << 16) | (g << 8) | b;
				}
				break;

				case BLEND_MULTIPLY: {
					UINT32 dp = d[x];
					UINT32 r = (((s >> 16) & 0xff) * ((dp >> 16) & 0xff) + 127) / 255;
					UINT32 g = (((s >>  8) & 0xff) * ((dp >>  8) & 0xff) + 127) / 255;
					UINT32 b = (((s >>  0) & 0xff) * ((dp >>  0) & 0xff) + 127) / 255;
					d[x] = (r << 16) | (g << 8) | b;
				}
				break;

				default:
					d[x] = s;
				break;
			}
		}
	}
}

// pixels per layout unit for a given scale mode (see nBurnArtworkScale)
static void art_get_scale(INT32 mode, float *psx, float *psy)
{
	const lrect &vb = view->bounds;
	const lrect &sb = view->items[first_screen].bounds;

	float sw = sb.w() > 0 ? sb.w() : 1;
	float sh = sb.h() > 0 ? sb.h() : 1;
	float base = game_w / sw;					// pixels per layout unit at 1x
	float sx, sy;
	INT32 maxh = nBurnArtworkMaxHeight > 16 ? nBurnArtworkMaxHeight : 16;

	if (mode < 0 && target_w > 0 && target_h > 0) {
		// auto: fill the frontend's output area (window / fullscreen), square pixels
		float tw = target_w, th = target_h;
		if (th > maxh) { tw = tw * maxh / th; th = maxh; }	// performance cap
		sx = sy = (tw / vb.w() < th / vb.h()) ? (tw / vb.w()) : (th / vb.h());

		// but never squeeze the game screen below 1x in either direction (dropping
		// columns / lines makes text unreadable): like MAME, render at that minimum
		// and let the frontend scale the whole picture down
		float smin = game_w / sw > game_h / sh ? game_w / sw : game_h / sh;
		if (sx < smin) sx = sy = smin;
	} else if (mode != 0) {
		// game screen at exactly N x its native resolution (1 = 1:1 with the game).
		// x and y are scaled separately so the game image is never resampled; the
		// shape is restored by the aspect ratio reported to the frontend.
		if (mode < 0) mode = 1;					// auto, but the frontend never told us its size
		sx = game_w * mode / sw;
		sy = game_h * mode / sh;
		if (vb.h() * sy > maxh) {				// shrink if the artwork is huge
			float k = maxh / (vb.h() * sy);
			sx *= k; sy *= k;
		}
	} else {
		// 0 = biggest integer upscale that fits nBurnArtworkMaxHeight
		float scale = base;
		for (INT32 k = 2; k <= 4; k++) {
			if (vb.h() * base * k <= maxh) scale = base * k;
		}
		if (vb.h() * scale > maxh) scale = maxh / vb.h();
		sx = sy = scale;
	}

	*psx = sx;
	*psy = sy;
}

static abitmap *item_bitmap(litem &it, INT32 state);

// draw everything up front (at startup and after every resize), so a scene
// change mid-game only blends bitmaps that are already there:
//  - every image component is rasterized at the size it's drawn at
//  - every item's states (its components' state values + defstate) are rendered
// art_layout() only marks the item cache stale; it's drawn once, by the first frame
// that needs it (the frontend sets its target size after BurnArtworkInit(), so
// drawing at every layout would draw everything twice at game start)
static INT32 predraw_pending = 0;

static void art_predraw()
{
	predraw_pending = 0;

	// sizes from a previous layout aren't needed any more
	for (std::map<std::string, lelement*>::iterator e = elements.begin(); e != elements.end(); ++e)
		for (UINT32 i = 0; i < e->second->comps.size(); i++) e->second->comps[i].raster.clear();

	INT32 nstates = 0;
	for (UINT32 i = 0; i < view->items.size(); i++) {
		litem &it = view->items[i];
		if (it.is_screen || it.elem == NULL) continue;

		INT32 w = it.px1 - it.px0, h = it.py1 - it.py0;
		if (w <= 0 || h <= 0) continue;
		if (it.orient & 1) { INT32 t = w; w = h; h = t; }

		lelement *e = it.elem;
		std::vector<INT32> states;
		states.push_back(e->defstate);
		for (UINT32 k = 0; k < e->comps.size(); k++) {
			lcomponent &c = e->comps[k];
			INT32 x0, y0, x1, y1;
			if (c.type == COMP_IMAGE && comp_rect(c, w, h, x0, y0, x1, y1)) comp_raster(c, x1 - x0, y1 - y0);
			if (c.state != -1 && std::find(states.begin(), states.end(), c.state) == states.end()) states.push_back(c.state);
		}

		it.cache_max = states.size() + 64;
		for (UINT32 k = 0; k < states.size(); k++) item_bitmap(it, states[k]);
		nstates += states.size();
	}
	bprintf(PRINT_NORMAL, _T("Artwork: view \"%S\", %dx%d (game screen %dx%d px), %d item states drawn\n"), view->name.c_str(), disp_w, disp_h,
		view->items[first_screen].px1 - view->items[first_screen].px0, view->items[first_screen].py1 - view->items[first_screen].py0, nstates);
}

// work out output size and item pixel rectangles
static void art_layout()
{
	INT32 nw = 0, nh = 0;
	BurnDrvGetNativeSize(&nw, &nh);
	native_w = nw; native_h = nh;
	game_w = orient_v ? nh : nw;
	game_h = orient_v ? nw : nh;

	const lrect &vb = view->bounds;
	float sx, sy;
	art_get_scale(nBurnArtworkScale, &sx, &sy);

	if (nBurnArtworkScale < 0 && target_w > 0) {
		disp_w = (INT32)(vb.w() * sx + 0.5f) & ~1;		// not bigger than the output area (unless the 1x minimum is)
		disp_h = (INT32)(vb.h() * sy + 0.5f) & ~1;
	} else {
		disp_w = ((INT32)(vb.w() * sx + 0.5f) + 1) & ~1;	// even sizes; items keep their exact scale
		disp_h = ((INT32)(vb.h() * sy + 0.5f) + 1) & ~1;
	}
	if (disp_w < 16) disp_w = 16;
	if (disp_h < 16) disp_h = 16;

	// display aspect = the layout's own shape
	aspect_x = (INT32)(vb.w() * 1000.0f / vb.h() + 0.5f);
	aspect_y = 1000;
	for (INT32 a = aspect_x, b = aspect_y; ; ) { if (b == 0) { aspect_x /= a; aspect_y /= a; break; } INT32 t = a % b; a = b; b = t; }

	for (UINT32 i = 0; i < view->items.size(); i++) {
		litem &it = view->items[i];
		it.px0 = (INT32)floorf((it.bounds.x0 - vb.x0) * sx + 0.5f);
		it.py0 = (INT32)floorf((it.bounds.y0 - vb.y0) * sy + 0.5f);
		it.px1 = (INT32)floorf((it.bounds.x1 - vb.x0) * sx + 0.5f);
		it.py1 = (INT32)floorf((it.bounds.y1 - vb.y0) * sy + 0.5f);
		it.cache.clear();
		it.last_state = -0x7fffffff;
	}

	bg.assign((size_t)disp_w * disp_h, 0);
	disp.assign((size_t)disp_w * disp_h, 0);
	bg_dirty = 1;
	full_dirty = 1;
	lay_full = 1;

	predraw_pending = 1;
}

static void art_free()
{
	art_close_archives();

	for (std::map<std::string, lelement*>::iterator it = elements.begin(); it != elements.end(); ++it) delete it->second;
	elements.clear();
	groups.clear();
	for (UINT32 i = 0; i < views.size(); i++) delete views[i];
	views.clear();
	delete layout_root;
	layout_root = NULL;
	env.clear();

	view = NULL;
	first_screen = -1;
	view_list.clear();
	cur_view = 0;

	std::vector<UINT32>().swap(bg);
	std::vector<UINT32>().swap(disp);

	BurnFree(native_buf);
	native_buf = NULL;
	native_buf_size = 0;

	art_active = 0;
	art_probed = 0;
	told_w = told_h = 0;
	lay_under.clear(); lay_add.clear(); lay_mul.clear();
	std::vector<UINT32>().swap(pre_add);
	std::vector<UINT32>().swap(pre_mul);
	pre_valid = 0;
	lay_memo_clear();
	lay_w = lay_h = 0;
	lay_full = 1;
	frame_depth = 0;
	saved_draw = NULL;
}

// ---------------------------------------------------------------------------
// Views and visibility toggles (collections), with per-game settings

// rebuild the visible item list and bounds from the view's toggles
static void view_apply(lview *v)
{
	v->items.clear();
	for (UINT32 i = 0; i < v->all.size(); i++) {
		const litem &it = v->all[i];
		if (it.is_screen || (it.vis & ~v->tog_on) == 0) v->items.push_back(it);	// screens always stay
	}

	if (v->has_bounds) {
		v->bounds = v->ebounds;
	} else if (v->items.size()) {
		v->bounds = v->items[0].bounds;
		for (UINT32 k = 1; k < v->items.size(); k++) v->bounds.unite(v->items[k].bounds);
	}

	if (v == view) {
		first_screen = -1;
		for (UINT32 i = 0; i < v->items.size(); i++) {
			if (v->items[i].is_screen && v->items[i].screen_index == 0) { first_screen = i; break; }
		}
	}
}

// name compare for the default parts: not case sensitive, '_' and ' ' are the
// same (* and ? wildcards work too)
static INT32 tog_char(INT32 c)
{
	if (c == '_') c = ' ';
	return tolower(c);
}

static INT32 wild_match(const char *p, const char *t)
{
	for (;;) {
		if (*p == 0) return (*t == 0);
		if (*p == '*') {
			p++;
			for (;;) {
				if (wild_match(p, t)) return 1;
				if (*t == 0) return 0;
				t++;
			}
		}
		if (*t == 0) return 0;
		if (*p != '?' && tog_char((UINT8)*p) != tog_char((UINT8)*t)) return 0;
		p++;
		t++;
	}
}

static void settings_path(TCHAR *path)
{
	_stprintf(path, _T("%s%s_art.ini"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
}

static void str_trim(char *t)
{
	char *b = t;
	while (*b == ' ' || *b == '\t') b++;
	if (b != t) memmove(t, b, strlen(b) + 1);
	char *e = t + strlen(t);
	while (e > t && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
}

// the common parts, for nBurnArtworkDefaults: a collection is one of these if
// its name is one of the names (not case sensitive, '_' = ' '), so alternatives
// like "Bezel_Alt" or "Marquee_Lit" keep the layout's own default
static const char *def_part_names[5][4] = {
	{ "Marquee",		"Marquees",			NULL,			NULL },	// ARTWORK_DEFAULT_MARQUEE
	{ "Overlay",		"Overlays",			NULL,			NULL },	// ARTWORK_DEFAULT_OVERLAY
	{ "Backdrop",		"Backdrops",		NULL,			NULL },	// ARTWORK_DEFAULT_BACKDROP
	{ "Bezel",			"Bezels",			NULL,			NULL },	// ARTWORK_DEFAULT_BEZEL
	{ "Control Panel",	"Control Panels",	"CPO",			NULL },	// ARTWORK_DEFAULT_CPANEL
};

static void apply_default_parts(lview *v)
{
	for (UINT32 i = 0; i < v->tog_name.size(); i++) {
		for (INT32 c = 0; c < 5; c++) {
			for (INT32 k = 0; k < 4 && def_part_names[c][k]; k++) {
				if (wild_match(def_part_names[c][k], v->tog_name[i].c_str())) {
					if (nBurnArtworkDefaults & (1 << c)) v->tog_on |= (1u << i); else v->tog_on &= ~(1u << i);
				}
			}
		}
	}
}

// layout defaults -> default parts (nBurnArtworkDefaults) -> config/games/<game>_art.ini
static void settings_load()
{
	for (UINT32 k = 0; k < view_list.size(); k++) {
		lview *v = views[view_list[k]];
		v->tog_on = v->tog_def;
		apply_default_parts(v);
	}
	cur_view = (nBurnArtworkView >= 0 && nBurnArtworkView < (INT32)view_list.size()) ? nBurnArtworkView : 0;

#ifdef __LIBRETRO__
	return;		// per-game settings are core options (BurnArtworkSetViewToggles())
#endif

	TCHAR path[MAX_PATH];
	settings_path(path);
	FILE *f = _tfopen(path, _T("rt"));
	if (f == NULL) return;

	char line[512];
	lview *sec = NULL;
	INT32 general = 0;

	while (fgets(line, sizeof(line), f)) {
		str_trim(line);
		if (line[0] == 0 || line[0] == ';' || line[0] == '#' || (line[0] == '/' && line[1] == '/')) continue;

		if (line[0] == '[') {
			char *e = strrchr(line, ']');
			if (e) *e = 0;
			general = (strcmp(line + 1, "artwork") == 0);
			sec = NULL;
			if (strncmp(line + 1, "view:", 5) == 0) {
				for (UINT32 k = 0; k < view_list.size(); k++)
					if (views[view_list[k]]->name == line + 6) sec = views[view_list[k]];
			}
			continue;
		}

		char *eq = strrchr(line, '=');
		if (eq == NULL) continue;
		*eq = 0;
		char *val = eq + 1;
		str_trim(line);
		str_trim(val);

		if (general && strcmp(line, "view") == 0) {
			for (UINT32 k = 0; k < view_list.size(); k++)
				if (views[view_list[k]]->name == val) cur_view = k;
		} else if (sec) {
			for (UINT32 i = 0; i < sec->tog_name.size(); i++) {
				if (sec->tog_name[i] == line) {
					if (atoi(val)) sec->tog_on |= (1u << i); else sec->tog_on &= ~(1u << i);
				}
			}
		}
	}
	fclose(f);
}

// written when something is changed from the frontend
static void settings_save()
{
#ifdef __LIBRETRO__
	return;
#endif

	TCHAR path[MAX_PATH];
	settings_path(path);
	FILE *f = _tfopen(path, _T("wt"));
	if (f == NULL) return;

	fprintf(f, "; FBNeo artwork settings for %s, delete this file to go back to the defaults\n", BurnDrvGetTextA(DRV_NAME));
	fprintf(f, "[artwork]\nview=%s\n", views[view_list[cur_view]]->name.c_str());

	for (UINT32 k = 0; k < view_list.size(); k++) {
		lview *v = views[view_list[k]];
		if (v->tog_name.empty()) continue;
		fprintf(f, "\n[view:%s]\n", v->name.c_str());
		for (UINT32 i = 0; i < v->tog_name.size(); i++)
			fprintf(f, "%s=%d\n", v->tog_name[i].c_str(), (v->tog_on >> i) & 1);
	}
	fclose(f);
}

static INT32 art_try_load(const char *game)
{
	if (game == NULL || game[0] == 0) return 1;
	if (!art_locate(game)) return 1;

	INT32 len = 0;
	UINT8 *lay = art_load_file("default.lay", &len);
	if (lay == NULL) {
		char name[128];
		snprintf(name, sizeof(name), "%s.lay", game);
		lay = art_load_file(name, &len);
	}
	if (lay == NULL) {
		bprintf(PRINT_ERROR, _T("Artwork: no default.lay in artwork for %S\n"), game);
		return 1;
	}

	layout_root = xml_parse((const char*)lay, len);
	free(lay);

	parse_layout(layout_root);

	// views that show screen 0
	view_list.clear();
	for (UINT32 v = 0; v < views.size(); v++) {
		for (UINT32 i = 0; i < views[v]->all.size(); i++) {
			if (views[v]->all[i].is_screen && views[v]->all[i].screen_index == 0) {
				view_list.push_back(v);
				break;
			}
		}
	}

	if (view_list.size()) {
		settings_load();
		view = views[view_list[cur_view]];
		view_apply(view);
	}

	if (view == NULL || first_screen < 0 || view->bounds.w() <= 0 || view->bounds.h() <= 0) {
		bprintf(PRINT_ERROR, _T("Artwork: no usable view in artwork for %S\n"), game);
		art_free();
		return 1;
	}

	bprintf(PRINT_IMPORTANT, _T("Artwork: loaded %S (%d views, %d elements)\n"), game, (INT32)views.size(), (INT32)elements.size());

	return 0;
}

// ---------------------------------------------------------------------------
// Interface

INT32 BurnArtworkActive()
{
	return art_active;
}

static INT32 art_load_game()
{
	if (art_try_load(BurnDrvGetTextA(DRV_NAME))) {
		art_free();
		if (art_try_load(BurnDrvGetTextA(DRV_PARENT))) {
			art_free();
			return 1;
		}
	}
	return 0;
}

void BurnArtworkInit()
{
	// keep what the frontend chose after BurnArtworkProbe()
	std::vector<std::pair<std::string, UINT32> > probed;
	std::string probed_view;
	if (art_probed) {
		for (UINT32 k = 0; k < view_list.size(); k++) probed.push_back(std::make_pair(views[view_list[k]]->name, views[view_list[k]]->tog_on));
		probed_view = views[view_list[cur_view]]->name;
	}

	art_free();

	if (!bBurnArtworkEnable) return;

	if (art_load_game()) return;

	if (probed.size()) {
		for (UINT32 k = 0; k < view_list.size(); k++) {
			lview *v = views[view_list[k]];
			for (UINT32 i = 0; i < probed.size(); i++)
				if (probed[i].first == v->name) v->tog_on = probed[i].second;
			if (v->name == probed_view) cur_view = k;
		}
		view = views[view_list[cur_view]];
		view_apply(view);
	}

	INT32 flags = BurnDrvGetFlags();
	orient_v = (flags & BDF_ORIENTATION_VERTICAL) ? 1 : 0;
	orient_f = (flags & BDF_ORIENTATION_FLIPPED) ? 1 : 0;
	is_vector = (BurnDrvGetGenreFlags() & GBF_VECTOR) ? 1 : 0;

	art_layout();

	art_active = 1;
}

void BurnArtworkExit()
{
	art_free();
}

static void native_buf_alloc()
{
	INT32 need = native_w * native_h * nBurnBpp;
	if (native_buf == NULL || need > native_buf_size) {
		BurnFree(native_buf);
		native_buf = (UINT8*)BurnMalloc(need);
		native_buf_size = need;
		memset(native_buf, 0, need);
	}
}

void BurnArtworkNativeSizeChanged()
{
	if (!art_active) return;

	INT32 nw = 0, nh = 0;
	BurnDrvGetNativeSize(&nw, &nh);
	if (nw == native_w && nh == native_h) return;	// same size again (e.g. set from a driver's scan)

	resized_in_frame = 1;
	art_layout();

	if (saved_draw) {
		// the driver may still draw this frame at its new size
		native_buf_alloc();
		pBurnDraw = native_buf;
		nBurnPitch = native_w * nBurnBpp;
	}
}

INT32 BurnArtworkGetDisplaySize(INT32 *pnWidth, INT32 *pnHeight)
{
	if (!art_active || layered) return 0;	// layered: the frontend's image is the game's own
	*pnWidth = told_w = disp_w;
	*pnHeight = told_h = disp_h;
	return 1;
}

INT32 BurnArtworkGetAspect(INT32 *pnXAspect, INT32 *pnYAspect)
{
	if (!art_active) return 0;
	*pnXAspect = aspect_x;
	*pnYAspect = aspect_y;
	return 1;
}

// size the frontend should base its window size on: the artwork with the game
// screen at 1x (or at nBurnArtworkScale x), independent of the window itself
INT32 BurnArtworkGetBaseSize(INT32 *pnWidth, INT32 *pnHeight)
{
	if (!art_active) return 0;
	float sx, sy;
	art_get_scale(nBurnArtworkScale < 0 ? 1 : nBurnArtworkScale, &sx, &sy);
	if (nBurnArtworkScale < 0) {
		// auto mode renders with square pixels, so keep the layout's shape here too,
		// with the game screen at least 1x in both directions (as MAME does)
		float s = sy > sx ? sy : sx;
		sx = sy = s;
	}
	*pnWidth  = ((INT32)(view->bounds.w() * sx + 0.5f) + 1) & ~1;
	*pnHeight = ((INT32)(view->bounds.h() * sy + 0.5f) + 1) & ~1;
	return 1;
}

// frontend: size (display orientation) of the area the game image is shown in.
// in auto mode (nBurnArtworkScale < 0) the artwork is rendered at this size.
// returns 1 if the artwork's size changed (the frontend should re-init video)
INT32 BurnArtworkSetTargetSize(INT32 nWidth, INT32 nHeight)
{
	if (nWidth < 16 || nHeight < 16) nWidth = nHeight = 0;
	if (nWidth == target_w && nHeight == target_h) return 0;	// the current layout already uses it
	target_w = nWidth;
	target_h = nHeight;

	if (!art_active || nBurnArtworkScale >= 0) return 0;

	INT32 ow = disp_w, oh = disp_h;
	art_layout();
	return (ow != disp_w || oh != disp_h);
}

// Layered output (see layers_update()): a frontend that composites the artwork
// itself calls this before it asks for the image size (BurnDrvGetVisibleSize()
// then gives the game's own size), and BurnArtworkLayered(0) when it goes away.
// returns 1 if artwork is active
INT32 BurnArtworkSetLayered(INT32 bLayered)
{
	if (!bBurnArtworkLayered) bLayered = 0;
	if (layered != (bLayered ? 1 : 0)) {
		layered = bLayered ? 1 : 0;
		lay_full = 1;
		bg_dirty = 1;
		full_dirty = 1;
	}
	return art_active && layered;
}

INT32 BurnArtworkGetLayers(BurnArtworkLayers *pLayers)
{
	if (!art_active || !layered || pLayers == NULL) return 0;

	if (lay_full || lay_under.empty()) layers_update();

	pLayers->nWidth = lay_w;
	pLayers->nHeight = lay_h;
	pLayers->nPitch = lay_w;
	pLayers->pUnder = &lay_under[0];
	pLayers->pAdd = &lay_add[0];
	pLayers->pMul = &lay_mul[0];
	pLayers->nScreenX = lay_sx0;
	pLayers->nScreenY = lay_sy0;
	pLayers->nScreenW = lay_sx1 - lay_sx0;
	pLayers->nScreenH = lay_sy1 - lay_sy0;
	pLayers->nSerial = lay_serial;
	pLayers->nSerialUnder = lay_under_serial;
	pLayers->nSerialOver = lay_over_serial;
	pLayers->nDirtyX = lay_dx0;
	pLayers->nDirtyY = lay_dy0;
	pLayers->nDirtyW = lay_dx1 - lay_dx0;
	pLayers->nDirtyH = lay_dy1 - lay_dy0;
	return 1;
}

// views / toggles for the frontend's menus. the Set functions re-layout the
// artwork (its size can change) and save config/games/<game>_art.ini,
// the frontend should then re-init its video (window size, buffers)

INT32 BurnArtworkGetViewCount()
{
	return (art_active || art_probed) ? (INT32)view_list.size() : 0;
}

const char *BurnArtworkGetViewName(INT32 n)
{
	if (!(art_active || art_probed) || n < 0 || n >= (INT32)view_list.size()) return NULL;
	return views[view_list[n]]->name.c_str();
}

INT32 BurnArtworkGetView()
{
	return (art_active || art_probed) ? cur_view : -1;
}

// toggles of any view (not just the current one)
static lview *view_at(INT32 nView)
{
	if (!(art_active || art_probed) || nView < 0 || nView >= (INT32)view_list.size()) return NULL;
	return views[view_list[nView]];
}

INT32 BurnArtworkGetViewToggleCount(INT32 nView)
{
	lview *v = view_at(nView);
	return v ? (INT32)v->tog_name.size() : 0;
}

const char *BurnArtworkGetViewToggleName(INT32 nView, INT32 n)
{
	lview *v = view_at(nView);
	if (v == NULL || n < 0 || n >= (INT32)v->tog_name.size()) return NULL;
	return v->tog_name[n].c_str();
}

INT32 BurnArtworkGetViewToggle(INT32 nView, INT32 n)
{
	lview *v = view_at(nView);
	if (v == NULL || n < 0 || n >= (INT32)v->tog_name.size()) return 0;
	return (v->tog_on >> n) & 1;
}

// for frontends that keep the per-game settings themselves (libretro core options):
// view and all its toggles at once, so the artwork is only laid out once.
// returns 1 if changed: re-init video
INT32 BurnArtworkSetViewToggles(INT32 nView, UINT32 nToggles)
{
	lview *v = view_at(nView);
	if (v == NULL) return 0;

	if (v->tog_name.size() < 32) nToggles &= (1u << v->tog_name.size()) - 1;
	if (nView == cur_view && nToggles == v->tog_on) return 0;

	cur_view = nView;
	v->tog_on = nToggles;
	view = v;
	view_apply(v);
	if (!art_active) return 0;

	art_layout();
	settings_save();
	return 1;
}

INT32 BurnArtworkSetView(INT32 n)
{
	if (!art_active || n < 0 || n >= (INT32)view_list.size() || n == cur_view) return 0;
	cur_view = n;
	view = views[view_list[n]];
	view_apply(view);
	art_layout();
	settings_save();
	return 1;
}

INT32 BurnArtworkGetToggleCount()
{
	return art_active ? (INT32)view->tog_name.size() : 0;
}

const char *BurnArtworkGetToggleName(INT32 n)
{
	if (!art_active || n < 0 || n >= (INT32)view->tog_name.size()) return NULL;
	return view->tog_name[n].c_str();
}

INT32 BurnArtworkGetToggle(INT32 n)
{
	if (!art_active || n < 0 || n >= (INT32)view->tog_name.size()) return 0;
	return (view->tog_on >> n) & 1;
}

INT32 BurnArtworkSetToggle(INT32 n, INT32 nOn)
{
	if (!art_active || n < 0 || n >= (INT32)view->tog_name.size()) return 0;
	UINT32 on = nOn ? (view->tog_on | (1u << n)) : (view->tog_on & ~(1u << n));
	if (on == view->tog_on) return 0;
	view->tog_on = on;
	view_apply(view);
	art_layout();
	settings_save();
	return 1;
}

// a global setting changed (nBurnArtworkScale, nBurnArtworkMaxHeight, the
// defaults): redo the running game's artwork. returns 1 if active: re-init video
INT32 BurnArtworkRefresh()
{
	if (!art_active) return 0;
	settings_load();		// a game with its own _art.ini keeps those settings
	view = views[view_list[cur_view]];
	view_apply(view);
	art_layout();
	return 1;
}

// parse the selected game's artwork before BurnDrvInit(), so a frontend can
// list its views and toggles and set them (BurnArtworkSetViewToggles()) before
// the game starts. the artwork itself is (re)loaded by BurnDrvInit().
// returns 1 if the game has artwork, whether artwork is enabled or not
INT32 BurnArtworkProbe()
{
	art_free();
	if (art_load_game()) return 0;

	art_probed = 1;
	return 1;
}

// switch artwork on / off for the running game (and the next ones).
// returns 1 if that changed anything: re-init video
INT32 BurnArtworkSetEnable(INT32 bEnable)
{
	bBurnArtworkEnable = bEnable ? 1 : 0;

	if (bBurnArtworkEnable && !art_active) {
		BurnArtworkInit();
		return art_active;
	}
	if (!bBurnArtworkEnable && art_active) {
		art_free();
		return 1;
	}
	return 0;
}

// forget the game's saved settings: back to the layout + global defaults
INT32 BurnArtworkResetSettings()
{
	if (!art_active) return 0;
#ifndef __LIBRETRO__
	TCHAR path[MAX_PATH];
	settings_path(path);
	_tremove(path);
#endif
	settings_load();
	view = views[view_list[cur_view]];
	view_apply(view);
	art_layout();
	return 1;
}

void BurnArtworkFrameBegin()
{
	if (frame_depth++ > 0) return;	// nested (Redraw inside Frame): the outer call handles it

	resized_in_frame = 0;
	saved_draw = NULL;

	if (!art_active || pBurnDraw == NULL) return;

	INT32 nw = 0, nh = 0;
	BurnDrvGetNativeSize(&nw, &nh);
	if (nw != native_w || nh != native_h) art_layout();
	if (predraw_pending) art_predraw();

	if (layered) return;	// the driver draws straight into the frontend's (game sized) buffer

	native_buf_alloc();

	saved_draw = pBurnDraw;
	saved_pitch = nBurnPitch;

	pBurnDraw = native_buf;
	nBurnPitch = native_w * nBurnBpp;
}

// redraw the static background in one rectangle
static void bg_rebuild_area(INT32 x0, INT32 y0, INT32 x1, INT32 y1)
{
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > disp_w) x1 = disp_w;
	if (y1 > disp_h) y1 = disp_h;
	if (x1 <= x0 || y1 <= y0) return;

	clip_x0 = x0; clip_y0 = y0; clip_x1 = x1; clip_y1 = y1;
	for (INT32 y = y0; y < y1; y++)
		memset(&bg[(size_t)y * disp_w + x0], 0, (x1 - x0) * sizeof(UINT32));

	for (INT32 i = 0; i < first_screen; i++) {
		litem &it = view->items[i];
		if (it.is_screen || it.px1 <= x0 || it.px0 >= x1 || it.py1 <= y0 || it.py0 >= y1) continue;
		apply_bitmap(bg, it, item_bitmap(it, item_state(it)));
	}
}

// ---------------------------------------------------------------------------
// Layered output: the frontend draws the game itself (with its effects / shaders
// applied to the game image only) and composites the artwork around it as
//   out = A + B * (U + game)		(per channel, clamped)
// U = everything under the game screen (black inside the screen when the
// screen isn't added to it), A and B = everything on top, folded together:
// every blend MAME layouts use is "dst = a + b * dst" (alpha, add, multiply),
// and a stack of those is one "a + b * dst". On the GPU that's: clear, draw the
// game, U additive, B multiplied (dst * B), A additive.


static INT32 screen_blend(const litem &it)
{
	INT32 blend = it.blend;
	if (blend == BLEND_DEFAULT) blend = is_vector ? BLEND_ADD : BLEND_NONE;
	return (blend == BLEND_ADD) ? BLEND_ADD : BLEND_NONE;
}

// display (layout) pixel -> index into the driver-oriented layers
static inline INT32 lay_index(INT32 dx, INT32 dy)
{
	if (!layered) return dy * disp_w + dx;	// art_compose() keeps them in display orientation

	INT32 x, y;
	if (!orient_v) {
		if (!orient_f) { x = dx; y = dy; }
		else { x = lay_w - 1 - dx; y = lay_h - 1 - dy; }
	} else {
		if (!orient_f) { y = dx; x = lay_w - 1 - dy; }
		else { y = lay_h - 1 - dx; x = dy; }
	}
	return y * lay_w + x;
}

// one over-screen item into a row of A (8.8 fixed) / B (0 - 65536 = 0.0 - 1.0)
static void layers_apply_item(litem &it, INT32 y, INT32 x0, INT32 x1, UINT32 *a, UINT32 *b)
{
	if (it.is_screen || it.elem == NULL || y < it.py0 || y >= it.py1) return;
	INT32 ix0 = it.px0 > x0 ? it.px0 : x0;
	INT32 ix1 = it.px1 < x1 ? it.px1 : x1;
	if (ix1 <= ix0) return;

	abitmap *bm = item_bitmap(it, item_state(it));
	if (bm == NULL || bm->p.empty()) return;
	INT32 ry = y - it.py0;
	const UINT32 *srow = &bm->p[(size_t)ry * bm->w];

	// alpha / add: only the non-transparent runs of the row (a lamp is mostly empty)
	INT32 runs = 1;
	const INT32 *sp = NULL;
	if ((it.blend == BLEND_ALPHA || it.blend == BLEND_ADD) && !bm->span_row.empty()) {
		sp = &bm->spans[bm->span_row[ry]];
		runs = sp[0];
	}

	for (INT32 k = 0; k < runs; k++) {
		INT32 rx0 = ix0, rx1 = ix1;
		if (sp) {
			if (it.px0 + sp[1 + k * 2] > rx0) rx0 = it.px0 + sp[1 + k * 2];
			if (it.px0 + sp[2 + k * 2] < rx1) rx1 = it.px0 + sp[2 + k * 2];
		}

		for (INT32 x = rx0; x < rx1; x++) {
			UINT32 spx = srow[x - it.px0];
			UINT32 *pa = &a[(x - x0) * 3], *pb = &b[(x - x0) * 3];
			UINT32 sc[3] = { (spx >> 16) & 0xff, (spx >> 8) & 0xff, spx & 0xff };
			switch (it.blend) {
				case BLEND_NONE:
					for (INT32 c = 0; c < 3; c++) { pa[c] = sc[c] << 8; pb[c] = 0; }
					break;
				case BLEND_MULTIPLY:
					if ((spx & 0xffffff) == 0xffffff) break;
					for (INT32 c = 0; c < 3; c++) { pa[c] = pa[c] * sc[c] / 255; pb[c] = pb[c] * sc[c] / 255; }
					break;
				case BLEND_ADD:
					for (INT32 c = 0; c < 3; c++) { pa[c] += sc[c] << 8; if (pa[c] > 0xffff) pa[c] = 0xffff; }
					break;
				default: {	// alpha, premultiplied
					UINT32 sa = spx >> 24;
					if (sa == 0) break;
					if (sa == 0xff) {
						for (INT32 c = 0; c < 3; c++) { pa[c] = sc[c] << 8; pb[c] = 0; }
						break;
					}
					UINT32 inv = 255 - sa;
					for (INT32 c = 0; c < 3; c++) { pa[c] = (sc[c] << 8) + pa[c] * inv / 255; pb[c] = pb[c] * inv / 255; if (pa[c] > 0xffff) pa[c] = 0xffff; }
				}
				break;
			}
		}
	}
}

// 8 bit B (as stored) -> 0 - 65536
static UINT32 mul_from8_lut[256];
static INT32 mul_from8_init = 0;

struct lay_ctx {
	INT32 sblend, tint;
	UINT32 tr, tg, tb;
	UINT32 nitems, dyn;
	INT32 use_pre, build_pre;
	std::vector<UINT32> a, b;
};

// U, A and B for display pixels x0 - x1 of row y
static void layers_build_run(lay_ctx &k, INT32 y, INT32 x0, INT32 x1)
{
	INT32 n = x1 - x0;
	if (n <= 0) return;
	if ((INT32)k.a.size() < n * 3) { k.a.resize(n * 3); k.b.resize(n * 3); }
	UINT32 *a = &k.a[0], *b = &k.b[0];		// A: 8.8 fixed, B: 0 - 65536 = 0.0 - 1.0
	const litem &scr = view->items[first_screen];

	UINT32 first = first_screen + 1;
	if (k.use_pre && !k.build_pre) {
		const UINT32 *qa = &pre_add[(size_t)y * disp_w + x0], *qb = &pre_mul[(size_t)y * disp_w + x0];
		for (INT32 x = 0; x < n; x++) {
			UINT32 va = qa[x], vb = qb[x];
			a[x * 3 + 0] = (va >> 8) & 0xff00; a[x * 3 + 1] = va & 0xff00; a[x * 3 + 2] = (va & 0xff) << 8;
			b[x * 3 + 0] = mul_from8_lut[(vb >> 16) & 0xff]; b[x * 3 + 1] = mul_from8_lut[(vb >> 8) & 0xff]; b[x * 3 + 2] = mul_from8_lut[vb & 0xff];
		}
		first = k.dyn;
	} else {
		for (INT32 x = 0; x < n * 3; x++) { a[x] = 0; b[x] = 65536; }
	}

	for (UINT32 i = first; i < k.nitems; i++) {
		if (k.build_pre && i == k.dyn) {
			// store the static part (8 bits, like the layers), and go on from what was stored
			UINT32 *qa = &pre_add[(size_t)y * disp_w + x0], *qb = &pre_mul[(size_t)y * disp_w + x0];
			for (INT32 x = 0; x < n; x++) {
				UINT32 av[3], bv[3];
				for (INT32 c = 0; c < 3; c++) {
					av[c] = (a[x * 3 + c] + 128) >> 8; if (av[c] > 255) av[c] = 255;
					bv[c] = (b[x * 3 + c] * 255 + 32768) >> 16; if (bv[c] > 255) bv[c] = 255;
					a[x * 3 + c] = av[c] << 8;
					b[x * 3 + c] = mul_from8_lut[bv[c]];
				}
				qa[x] = (av[0] << 16) | (av[1] << 8) | av[2];
				qb[x] = (bv[0] << 16) | (bv[1] << 8) | bv[2];
			}
		}
		layers_apply_item(view->items[i], y, x0, x1, a, b);
	}

	// where pixel x lands in the layers: li0 + (x - x0) * lstep
	INT32 li0 = lay_index(x0, y);
	INT32 lstep = (n > 1) ? lay_index(x0 + 1, y) - li0 : 1;

	INT32 in_scr_y = (y >= scr.py0 && y < scr.py1);
	const UINT32 *bgrow = &bg[(size_t)y * disp_w];
	for (INT32 x = x0; x < x1; x++) {
		UINT32 *pa = &a[(x - x0) * 3], *pb = &b[(x - x0) * 3];
		INT32 in_scr = in_scr_y && x >= scr.px0 && x < scr.px1;
		if (in_scr && k.tint) {
			pb[0] = (UINT32)(((UINT64)pb[0] * k.tr) >> 16);
			pb[1] = (UINT32)(((UINT64)pb[1] * k.tg) >> 16);
			pb[2] = (UINT32)(((UINT64)pb[2] * k.tb) >> 16);
		}
		UINT32 av[3], bv[3];
		for (INT32 c = 0; c < 3; c++) {
			av[c] = (pa[c] + 128) >> 8; if (av[c] > 255) av[c] = 255;
			bv[c] = (pb[c] * 255 + 32768) >> 16; if (bv[c] > 255) bv[c] = 255;
		}
		INT32 li = li0 + (x - x0) * lstep;
		lay_add[li] = 0xff000000 | (av[0] << 16) | (av[1] << 8) | av[2];
		lay_mul[li] = 0xff000000 | (bv[0] << 16) | (bv[1] << 8) | bv[2];
		if (layered) lay_under[li] = (in_scr && k.sblend == BLEND_NONE) ? 0xff000000 : (0xff000000 | bgrow[x]);
	}
}

// the display x ranges of row y that a bitmap of item it touches (non-transparent runs),
// appended to out as [start, end) pairs; returns 0 if it can't tell (then: the whole row)
static INT32 item_row_runs(const litem &it, const abitmap *bm, INT32 y, std::vector<INT32> &out)
{
	if (bm == NULL || bm->p.empty() || y < it.py0 || y >= it.py1) return 1;		// nothing there
	if (!(it.blend == BLEND_ALPHA || it.blend == BLEND_ADD) || bm->span_row.empty()) return 0;
	const INT32 *sp = &bm->spans[bm->span_row[y - it.py0]];
	for (INT32 k = 0; k < sp[0]; k++) {
		out.push_back(it.px0 + sp[1 + k * 2]);
		out.push_back(it.px0 + sp[2 + k * 2]);
	}
	return 1;
}

// first over-screen item bound to an output (everything under it, down to the screen, is static)
static UINT32 layers_dyn_first()
{
	UINT32 n = view->items.size(), d = first_screen + 1;
	while (d < n && !(view->items[d].elem && view->items[d].output >= 0)) d++;
	return d;
}

// the display x ranges of row y where an item changed from image bm_old to bm_new
// (sorted, merged [start, end) pairs); returns 0 if it can't tell (then: its whole row)
static INT32 changed_row_runs(const litem &it, const abitmap *bm_old, const abitmap *bm_new, INT32 y, std::vector<INT32> &merged)
{
	static std::vector<INT32> runs;
	runs.clear();
	merged.clear();
	if (!item_row_runs(it, bm_old, y, runs) || !item_row_runs(it, bm_new, y, runs)) return 0;
	if (runs.empty()) return 1;

	std::vector<std::pair<INT32, INT32> > r;
	for (UINT32 q = 0; q + 1 < runs.size(); q += 2) r.push_back(std::make_pair(runs[q], runs[q + 1]));
	std::sort(r.begin(), r.end());
	for (UINT32 q = 0; q < r.size(); q++) {
		if (!merged.empty() && r[q].first <= merged.back()) { if (r[q].second > merged.back()) merged.back() = r[q].second; }
		else { merged.push_back(r[q].first); merged.push_back(r[q].second); }
	}
	return 1;
}

// rebuild U, A and B in a display rectangle. chg (optional): the item whose state
// changed from old_state, only the pixels its old or new image covers are redone
static void layers_build_area(INT32 x0, INT32 y0, INT32 x1, INT32 y1, litem *chg = NULL, INT32 old_state = 0)
{
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > disp_w) x1 = disp_w;
	if (y1 > disp_h) y1 = disp_h;
	if (x1 <= x0 || y1 <= y0) return;

	if (!mul_from8_init) {
		for (UINT32 v = 0; v < 256; v++) mul_from8_lut[v] = (v * 65536 + 127) / 255;
		mul_from8_init = 1;
	}

	const litem &scr = view->items[first_screen];
	lay_ctx k;
	k.sblend = screen_blend(scr);
	// the screen's tint is folded into B for the frontend; art_compose()'s apply_screen() does it itself
	k.tint = layered && !is_white(scr.color) && k.sblend == BLEND_NONE;
	k.tr = (UINT32)(scr.color.r * scr.color.a * 65536); k.tg = (UINT32)(scr.color.g * scr.color.a * 65536); k.tb = (UINT32)(scr.color.b * scr.color.a * 65536);

	// the static items under the first output-driven one: fold them once (whole display), then reuse
	k.nitems = view->items.size();
	k.dyn = layers_dyn_first();
	k.use_pre = (k.dyn > (UINT32)first_screen + 1);
	if (!k.use_pre) {
		pre_valid = 0;
		std::vector<UINT32>().swap(pre_add);
		std::vector<UINT32>().swap(pre_mul);
	} else if (!pre_valid || pre_first != k.dyn || pre_add.size() != (size_t)disp_w * disp_h) {
		pre_add.assign((size_t)disp_w * disp_h, 0);
		pre_mul.assign((size_t)disp_w * disp_h, 0);
		pre_first = k.dyn;
		pre_valid = 0;
		x0 = 0; y0 = 0; x1 = disp_w; y1 = disp_h;	// build it all now
		chg = NULL;
	}
	k.build_pre = k.use_pre && !pre_valid;

	abitmap *bm_old = chg ? item_bitmap(*chg, old_state) : NULL;
	abitmap *bm_new = chg ? item_bitmap(*chg, item_state(*chg)) : NULL;
	std::vector<INT32> merged;

	for (INT32 y = y0; y < y1; y++) {
		if (chg && changed_row_runs(*chg, bm_old, bm_new, y, merged)) {
			for (UINT32 q = 0; q + 1 < merged.size(); q += 2) {
				INT32 a0 = merged[q] > x0 ? merged[q] : x0, a1 = merged[q + 1] < x1 ? merged[q + 1] : x1;
				layers_build_run(k, y, a0, a1);
			}
			continue;
		}
		layers_build_run(k, y, x0, x1);
	}

	if (k.build_pre) pre_valid = 1;
}

// bring the layers up to date (called at the end of each frame, and when asked for)
static void layers_update()
{
	// the frontend gets them in the driver's orientation, art_compose() uses them as displayed
	INT32 ow = (layered && orient_v) ? disp_h : disp_w;
	INT32 oh = (layered && orient_v) ? disp_w : disp_h;
	if (ow != lay_w || oh != lay_h || lay_add.size() != (size_t)ow * oh || (layered && lay_under.size() != (size_t)ow * oh)) {
		lay_w = ow;
		lay_h = oh;
		if (layered) lay_under.assign((size_t)ow * oh, 0xff000000); else std::vector<UINT32>().swap(lay_under);
		lay_add.assign((size_t)ow * oh, 0xff000000);
		lay_mul.assign((size_t)ow * oh, 0xffffffff);
		lay_full = 1;
	}
	if (full_dirty) lay_full = 1;
	lay_dirty.clear();
	lay_chg.clear();

	// background: redo just the areas of items that changed state, or everything if a lot changed at once
	std::vector<INT32> bg_changed;
	INT64 bg_area = 0;
	for (INT32 i = 0; i < first_screen; i++) {
		litem &it = view->items[i];
		if (it.elem && item_state(it) != it.last_state) {
			bg_changed.push_back(i);
			bg_area += (INT64)(it.px1 - it.px0) * (it.py1 - it.py0);
		}
	}
	if (bg_changed.size() > 8 || bg_area * 3 > (INT64)disp_w * disp_h) bg_dirty = 1;

	if (bg_dirty || lay_full) {
		clip_x0 = 0; clip_y0 = 0; clip_x1 = disp_w; clip_y1 = disp_h;
		std::fill(bg.begin(), bg.end(), 0);
		for (INT32 i = 0; i < first_screen; i++) {
			litem &it = view->items[i];
			if (it.is_screen) continue;
			it.last_state = item_state(it);
			apply_bitmap(bg, it, item_bitmap(it, it.last_state));
		}
		bg_dirty = 0;
		lay_full = 1;
		bg_changed.clear();
	}

	std::vector<INT32> changed;
	for (UINT32 i = first_screen + 1; i < view->items.size(); i++) {
		litem &it = view->items[i];
		if (it.elem && item_state(it) != it.last_state) changed.push_back(i);
	}

	if (lay_full) {
		pre_valid = 0;		// a new layout (or the parts changed): fold the static items again
		lay_memo_clear();
		layers_build_area(0, 0, disp_w, disp_h);
		for (UINT32 i = first_screen + 1; i < view->items.size(); i++)
			if (view->items[i].elem) view->items[i].last_state = item_state(view->items[i]);
		lay_full = 0;
		full_dirty = 0;
		lay_serial++;
		lay_ab_serial++;
		lay_under_serial = lay_over_serial = lay_serial;
		lay_dirty.push_back(0); lay_dirty.push_back(0); lay_dirty.push_back(disp_w); lay_dirty.push_back(disp_h);
	} else if (bg_changed.size() || changed.size()) {
		for (UINT32 k = 0; k < bg_changed.size(); k++) {
			litem &c = view->items[bg_changed[k]];
			bg_rebuild_area(c.px0, c.py0, c.px1, c.py1);
			c.last_state = item_state(c);
			if (layered) layers_build_area(c.px0, c.py0, c.px1, c.py1);	// U only (A / B don't change)
			lay_dirty.push_back(c.px0); lay_dirty.push_back(c.py0); lay_dirty.push_back(c.px1); lay_dirty.push_back(c.py1);
		}
		if (changed.size()) {
			// the output-driven items' states before and now
			UINT32 dyn = layers_dyn_first();
			std::vector<INT32> key_old, key_new;
			for (UINT32 i = dyn; i < view->items.size(); i++) {
				litem &it = view->items[i];
				if (!it.elem || it.output < 0) continue;
				key_old.push_back(it.last_state);
				key_new.push_back(item_state(it));
			}

			// a few of them (lamps): keep A / B for each combination
			INT32 use_memo = key_new.size() <= 4;
			size_t bytes = lay_add.size() * sizeof(UINT32) * 2;
			lay_memo *hit = NULL;
			if (use_memo) {
				lay_memo *have_old = NULL;
				for (UINT32 m = 0; m < lay_memos.size(); m++) {
					if (lay_memos[m]->key == key_old) have_old = lay_memos[m];
					if (lay_memos[m]->key == key_new) hit = lay_memos[m];
				}
				if (have_old == NULL) {
					// remember what's there now (up to 128 MB, the least recently used goes)
					while (lay_memos.size() && (lay_memos.size() + 1) * bytes > 128 * 1024 * 1024) {
						UINT32 lru = 0;
						for (UINT32 m = 1; m < lay_memos.size(); m++) if (lay_memos[m]->used < lay_memos[lru]->used) lru = m;
						if (lay_memos[lru] == hit) hit = NULL;
						delete lay_memos[lru];
						lay_memos.erase(lay_memos.begin() + lru);
					}
					if (bytes <= 128 * 1024 * 1024) {
						lay_memo *m = new lay_memo;
						m->key = key_old;
						m->add = lay_add;
						m->mul = lay_mul;
						m->used = ++lay_memo_clock;
						lay_memos.push_back(m);
					}
				} else {
					have_old->used = ++lay_memo_clock;
				}
			}

			if (hit) {
				memcpy(&lay_add[0], &hit->add[0], lay_add.size() * sizeof(UINT32));
				memcpy(&lay_mul[0], &hit->mul[0], lay_mul.size() * sizeof(UINT32));
				hit->used = ++lay_memo_clock;
			}

			for (UINT32 k = 0; k < changed.size(); k++) {
				litem &it = view->items[changed[k]];
				INT32 old_state = it.last_state;
				it.last_state = item_state(it);
				if (!hit) layers_build_area(it.px0, it.py0, it.px1, it.py1, &it, old_state);
				lay_chg.push_back(std::make_pair((INT32)changed[k], old_state));
			}
			lay_ab_serial++;
		}
		lay_serial++;
		if (bg_changed.size()) lay_under_serial = lay_serial;
		if (changed.size()) lay_over_serial = lay_serial;
	}

	// what this update changed, as one rectangle in the layers' coordinates
	if (lay_dirty.size() || lay_chg.size()) {
		INT32 x0 = disp_w, y0 = disp_h, x1 = 0, y1 = 0;
		for (UINT32 k = 0; k + 3 < lay_dirty.size(); k += 4) {
			if (lay_dirty[k] < x0) x0 = lay_dirty[k];
			if (lay_dirty[k + 1] < y0) y0 = lay_dirty[k + 1];
			if (lay_dirty[k + 2] > x1) x1 = lay_dirty[k + 2];
			if (lay_dirty[k + 3] > y1) y1 = lay_dirty[k + 3];
		}
		for (UINT32 k = 0; k < lay_chg.size(); k++) {
			const litem &it = view->items[lay_chg[k].first];
			if (it.px0 < x0) x0 = it.px0;
			if (it.py0 < y0) y0 = it.py0;
			if (it.px1 > x1) x1 = it.px1;
			if (it.py1 > y1) y1 = it.py1;
		}
		if (x0 < 0) x0 = 0;
		if (y0 < 0) y0 = 0;
		if (x1 > disp_w) x1 = disp_w;
		if (y1 > disp_h) y1 = disp_h;
		if (x1 > x0 && y1 > y0) {
			// the corners, through the same mapping as the pixels
			INT32 cx[2] = { x0, x1 - 1 }, cy[2] = { y0, y1 - 1 };
			lay_dx0 = lay_w; lay_dy0 = lay_h; lay_dx1 = 0; lay_dy1 = 0;
			for (INT32 a = 0; a < 2; a++) {
				for (INT32 b = 0; b < 2; b++) {
					INT32 li = lay_index(cx[a], cy[b]);
					INT32 lx = li % lay_w, ly = li / lay_w;
					if (lx < lay_dx0) lay_dx0 = lx;
					if (ly < lay_dy0) lay_dy0 = ly;
					if (lx + 1 > lay_dx1) lay_dx1 = lx + 1;
					if (ly + 1 > lay_dy1) lay_dy1 = ly + 1;
				}
			}
		} else {
			lay_dx0 = lay_dy0 = lay_dx1 = lay_dy1 = 0;
		}
	}

	// the screen rectangle, in driver orientation
	const litem &scr = view->items[first_screen];
	INT32 X0 = scr.px0, Y0 = scr.py0, X1 = scr.px1, Y1 = scr.py1;
	if (!orient_v) {
		if (!orient_f) { lay_sx0 = X0; lay_sx1 = X1; lay_sy0 = Y0; lay_sy1 = Y1; }
		else { lay_sx0 = lay_w - X1; lay_sx1 = lay_w - X0; lay_sy0 = lay_h - Y1; lay_sy1 = lay_h - Y0; }
	} else {
		if (!orient_f) { lay_sy0 = X0; lay_sy1 = X1; lay_sx0 = lay_w - Y1; lay_sx1 = lay_w - Y0; }
		else { lay_sy0 = lay_h - X1; lay_sy1 = lay_h - X0; lay_sx0 = Y0; lay_sx1 = Y1; }
	}
}

// redo one display rectangle: background, game, then everything on top of it as
// out = A + B * (background + game), A / B from layers_build_area()
static void compose_area(INT32 x0, INT32 y0, INT32 x1, INT32 y1)
{
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > disp_w) x1 = disp_w;
	if (y1 > disp_h) y1 = disp_h;
	if (x1 <= x0 || y1 <= y0) return;

	clip_x0 = x0; clip_y0 = y0; clip_x1 = x1; clip_y1 = y1;

	// if the game screen covers the whole area it can read the background
	// directly instead of it being copied in first
	const litem &fs = view->items[first_screen];
	INT32 fused = (fs.is_screen && fs.screen_index == 0 && fs.px0 <= x0 && fs.py0 <= y0 && fs.px1 >= x1 && fs.py1 >= y1);

	if (!fused) {
		for (INT32 y = y0; y < y1; y++)
			memcpy(&disp[y * disp_w + x0], &bg[y * disp_w + x0], (x1 - x0) * sizeof(UINT32));
	}
	if (fs.is_screen && fs.screen_index == 0) apply_screen(disp, fs, fused ? &bg[0] : NULL);

	// on the screen, only where something is on top of it (often nothing, or a small part)
	if (fold_serial != lay_ab_serial) {
		fold_serial = lay_ab_serial;
		fold_runs.clear();
		fold_row.assign(fs.py1 > fs.py0 ? fs.py1 - fs.py0 : 0, 0);
		for (INT32 y = fs.py0; y < fs.py1; y++) {
			fold_row[y - fs.py0] = fold_runs.size();
			fold_runs.push_back(0);
			if (y < 0 || y >= disp_h) continue;
			const UINT32 *la = &lay_add[y * disp_w], *lm = &lay_mul[y * disp_w];
			INT32 x = fs.px0 > 0 ? fs.px0 : 0, xe = fs.px1 < disp_w ? fs.px1 : disp_w, n = 0;
			while (x < xe) {
				while (x < xe && (lm[x] & 0xffffff) == 0xffffff && (la[x] & 0xffffff) == 0) x++;
				if (x >= xe) break;
				INT32 st = x;
				while (x < xe && !((lm[x] & 0xffffff) == 0xffffff && (la[x] & 0xffffff) == 0)) x++;
				fold_runs.push_back(st);
				fold_runs.push_back(x);
				n++;
			}
			fold_runs[fold_row[y - fs.py0]] = n;
		}
	}

	for (INT32 y = y0; y < y1; y++) {
		UINT32 *d = &disp[y * disp_w];
		const UINT32 *la = &lay_add[y * disp_w], *lm = &lay_mul[y * disp_w];

		if (y >= fs.py0 && y < fs.py1 && x0 >= fs.px0 && x1 <= fs.px1) {
			const INT32 *r = &fold_runs[fold_row[y - fs.py0]];
			for (INT32 q = 0; q < r[0]; q++) {
				INT32 a0 = r[1 + q * 2] > x0 ? r[1 + q * 2] : x0, a1 = r[2 + q * 2] < x1 ? r[2 + q * 2] : x1;
				for (INT32 x = a0; x < a1; x++) {
					UINT32 m = lm[x] & 0xffffff, ad = la[x] & 0xffffff;
					if (m == 0) { d[x] = ad; continue; }
					UINT32 p = d[x];
					UINT32 rr = (((p >> 16) & 0xff) * ((m >> 16) & 0xff) + 127) / 255 + ((ad >> 16) & 0xff);
					UINT32 g = (((p >>  8) & 0xff) * ((m >>  8) & 0xff) + 127) / 255 + ((ad >>  8) & 0xff);
					UINT32 b = (((p >>  0) & 0xff) * ((m >>  0) & 0xff) + 127) / 255 + ((ad >>  0) & 0xff);
					if (rr > 255) rr = 255;
					if (g > 255) g = 255;
					if (b > 255) b = 255;
					d[x] = (rr << 16) | (g << 8) | b;
				}
			}
			continue;
		}

		for (INT32 x = x0; x < x1; x++) {
			UINT32 m = lm[x] & 0xffffff, ad = la[x] & 0xffffff;
			if (m == 0xffffff) { if (ad) d[x] = sat_add24(d[x], ad); continue; }	// nothing on top here, or only added light
			if (m == 0) { d[x] = ad; continue; }									// opaque artwork
			UINT32 p = d[x];
			UINT32 r = (((p >> 16) & 0xff) * ((m >> 16) & 0xff) + 127) / 255 + ((ad >> 16) & 0xff);
			UINT32 g = (((p >>  8) & 0xff) * ((m >>  8) & 0xff) + 127) / 255 + ((ad >>  8) & 0xff);
			UINT32 b = (((p >>  0) & 0xff) * ((m >>  0) & 0xff) + 127) / 255 + ((ad >>  0) & 0xff);
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
			d[x] = (r << 16) | (g << 8) | b;
		}
	}
}

// out = A + B * background, for display pixels x0 - x1 of row y that aren't on the
// game screen (art_compose() redoes the screen every frame)
static void compose_outside(INT32 y, INT32 x0, INT32 x1)
{
	if (x0 < 0) x0 = 0;
	if (x1 > disp_w) x1 = disp_w;
	if (y < 0 || y >= disp_h || x1 <= x0) return;

	const litem &fs = view->items[first_screen];
	if (y >= fs.py0 && y < fs.py1 && x0 < fs.px1 && x1 > fs.px0) {
		// split around the screen
		if (x0 < fs.px0) compose_outside(y, x0, fs.px0);
		if (x1 > fs.px1) compose_outside(y, fs.px1, x1);
		return;
	}

	UINT32 *d = &disp[y * disp_w];
	const UINT32 *bgrow = &bg[y * disp_w], *la = &lay_add[y * disp_w], *lm = &lay_mul[y * disp_w];
	for (INT32 x = x0; x < x1; x++) {
		UINT32 m = lm[x] & 0xffffff, ad = la[x] & 0xffffff, p = bgrow[x];
		if (m == 0xffffff) { d[x] = ad ? sat_add24(p, ad) : p; continue; }
		if (m == 0) { d[x] = ad; continue; }
		UINT32 r = (((p >> 16) & 0xff) * ((m >> 16) & 0xff) + 127) / 255 + ((ad >> 16) & 0xff);
		UINT32 g = (((p >>  8) & 0xff) * ((m >>  8) & 0xff) + 127) / 255 + ((ad >>  8) & 0xff);
		UINT32 b = (((p >>  0) & 0xff) * ((m >>  0) & 0xff) + 127) / 255 + ((ad >>  0) & 0xff);
		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;
		d[x] = (r << 16) | (g << 8) | b;
	}
}

static void art_compose()
{
	fmt_update();

	// what changed (background items, or the ones over the screen) since last frame
	layers_update();

	INT32 full = 0;
	for (UINT32 k = 0; k + 3 < lay_dirty.size(); k += 4) {
		if (lay_dirty[k] <= 0 && lay_dirty[k + 1] <= 0 && lay_dirty[k + 2] >= disp_w && lay_dirty[k + 3] >= disp_h) full = 1;
	}

	if (full) {
		compose_area(0, 0, disp_w, disp_h);
	} else {
		// background items that changed
		for (UINT32 k = 0; k + 3 < lay_dirty.size(); k += 4)
			for (INT32 y = lay_dirty[k + 1]; y < lay_dirty[k + 3]; y++) compose_outside(y, lay_dirty[k], lay_dirty[k + 2]);

		// items over the screen that changed: just the pixels their old or new image covers
		std::vector<INT32> merged;
		for (UINT32 k = 0; k < lay_chg.size(); k++) {
			litem &it = view->items[lay_chg[k].first];
			abitmap *bm_old = item_bitmap(it, lay_chg[k].second);
			abitmap *bm_new = item_bitmap(it, it.last_state);
			for (INT32 y = it.py0 > 0 ? it.py0 : 0; y < it.py1 && y < disp_h; y++) {
				if (changed_row_runs(it, bm_old, bm_new, y, merged)) {
					for (UINT32 q = 0; q + 1 < merged.size(); q += 2) compose_outside(y, merged[q], merged[q + 1]);
				} else {
					compose_outside(y, it.px0, it.px1);
				}
			}
		}

		// everything else is static, redo the game screen area only
		const litem &fs = view->items[first_screen];
		compose_area(fs.px0, fs.py0, fs.px1, fs.py1);
	}

	// the size changed (view / part switched on or off) and the frontend hasn't
	// re-initialised its video yet: its buffer is still the old size
	if (disp_w != told_w || disp_h != told_h) return;

	// write out in the driver's (unrotated) orientation, the frontend rotates it
	INT32 ow = orient_v ? disp_h : disp_w;
	INT32 oh = orient_v ? disp_w : disp_h;

	if (!orient_v && fmt_direct) {
		if (!orient_f) {
			for (INT32 y = 0; y < oh; y++)
				memcpy(pBurnDraw + y * nBurnPitch, &disp[y * disp_w], ow * 4);
		} else {
			// ROT180: reversed rows
			for (INT32 y = 0; y < oh; y++) {
				UINT32 *d32 = (UINT32*)(pBurnDraw + y * nBurnPitch);
				const UINT32 *src = &disp[(oh - 1 - y) * disp_w + (ow - 1)];
				for (INT32 x = 0; x < ow; x++) d32[x] = src[-x];
			}
		}
		return;
	}

	if (orient_v && fmt_direct) {
		// ROT90 / ROT270: transpose in tiles to keep it cache friendly
		for (INT32 ty = 0; ty < oh; ty += 32) {
			INT32 ey = (ty + 32 < oh) ? ty + 32 : oh;
			for (INT32 tx = 0; tx < ow; tx += 32) {
				INT32 ex = (tx + 32 < ow) ? tx + 32 : ow;
				for (INT32 y = ty; y < ey; y++) {
					UINT32 *d32 = (UINT32*)(pBurnDraw + y * nBurnPitch);
					INT32 dx = orient_f ? (oh - 1 - y) : y;
					for (INT32 x = tx; x < ex; x++) {
						INT32 dy = orient_f ? x : (ow - 1 - x);
						d32[x] = disp[dy * disp_w + dx];
					}
				}
			}
		}
		return;
	}

	// display index = colpart[x] + rowpart[y]
	std::vector<INT32> colpart(ow);
	for (INT32 x = 0; x < ow; x++) {
		if (orient_v) colpart[x] = (orient_f ? x : (ow - 1 - x)) * disp_w;		// dy
		else          colpart[x] = orient_f ? (ow - 1 - x) : x;					// dx
	}

	for (INT32 y = 0; y < oh; y++) {
		INT32 rowpart;
		if (orient_v) rowpart = orient_f ? (oh - 1 - y) : y;						// dx
		else          rowpart = (orient_f ? (oh - 1 - y) : y) * disp_w;			// dy

		UINT8 *d = pBurnDraw + y * nBurnPitch;
		const UINT32 *src = &disp[rowpart];

		if (fmt_direct) {
			UINT32 *d32 = (UINT32*)d;
			for (INT32 x = 0; x < ow; x++) d32[x] = src[colpart[x]];
		} else {
			for (INT32 x = 0; x < ow; x++, d += fmt_bpp) fmt_write(d, src[colpart[x]]);
		}
	}
}

void BurnArtworkFrameEnd()
{
	if (frame_depth > 0 && --frame_depth > 0) return;	// nested

	if (art_active && layered) {
		layers_update();	// the frontend draws the game and the layers
		return;
	}

	if (!art_active || saved_draw == NULL) return;

	pBurnDraw = saved_draw;
	nBurnPitch = saved_pitch;
	saved_draw = NULL;

	if (resized_in_frame) return;	// the frontend re-initialised video, skip this one

	art_compose();
}

void BurnArtworkScan(INT32 nAction)
{
	// outputs aren't saved: they only mirror driver state, so drivers with outputs
	// rebuild them from their own state when loading (savestate size stays constant)
}
