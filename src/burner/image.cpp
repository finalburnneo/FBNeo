#include "burner.h"
#include "spng.h"

#define PNG_SIG_LEN (8)
const UINT8 PNG_SIG[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

int bPngImageOrientation = 0;

void img_free(IMAGE* img)
{
	free(img->rowptr);
	img->rowptr = NULL;
	if (img->flags & IMG_FREE) {
		if (img->bmpbits) {
			free(img->bmpbits);
			img->bmpbits = NULL;
		}
	}
}

INT32 img_alloc(IMAGE* img)
{
	img->flags		  = 0;

	img->rowbytes	  = ((DWORD)img->width * 24 + 31) / 32 * 4;
	img->imgbytes	  = img->rowbytes * img->height;
	img->rowptr		  = (BYTE**)malloc((size_t)img->height*4 * sizeof(BYTE*));

	if (img->bmpbits == NULL) {
		img->flags   |= IMG_FREE;
		img->bmpbits  = (BYTE*)malloc((size_t)img->imgbytes);
	}

	if (img->rowptr == NULL || img->bmpbits == NULL) {
		img_free(img);
		return 1;
	}

	for (UINT32 y = 0; y < img->height; y++) {
		img->rowptr[img->height - y - 1] = img->bmpbits + y * img->rowbytes;
	}

	return 0;
}

// Interpolate using a Catmull-Rom spline
static inline double interpolate(const double f, const double* c)
{
	return 0.5 * ((				2.0 * c[1]					  ) +
				  (		-c[0] +					   c[2]		  ) *  f +
				  (2.0 * c[0] - 5.0 * c[1] + 4.0 * c[2] - c[3]) * (f * f) +
				  (		-c[0] + 3.0 * c[1] - 3.0 * c[2] + c[3]) * (f * f * f));
}

static inline double interpolatePixelH(const double f, const INT32 x, const UINT8* row, const INT32 width)
{
	double c[4];

	c[0] = x >= 1		   ? row[(x - 1) * 3] : row[0        ];
	c[1] =					 row[ x      * 3];
	c[2] = x < (width - 1) ? row[(x + 1) * 3] : row[width - 1];
	c[3] = x < (width - 2) ? row[(x + 2) * 3] : row[width - 1];

	return interpolate(f, c);
}

static void interpolateRowH(const IMAGE* img, const INT32 y, double* row, const INT32 width)
{
	for (INT32 x = 0, x2; x < width; x++) {

		double f = (double)x * img->width / width;
		x2 = (INT32)f;	f -= x2;

		row[x * 3 + 0] = interpolatePixelH(f, x2, img->rowptr[y] + 0, img->width);
		row[x * 3 + 1] = interpolatePixelH(f, x2, img->rowptr[y] + 1, img->width);
		row[x * 3 + 2] = interpolatePixelH(f, x2, img->rowptr[y] + 2, img->width);
	}
}

static inline void interpolateRowV(const double f, const INT32 y, double** row, const IMAGE* img)
{
	double c[5];

	for (UINT32 x = 0; x < img->width * 3; x++) {

		c[0] = row[0][x];
		c[1] = row[1][x];
		c[2] = row[2][x];
		c[3] = row[3][x];

		c[4] = interpolate(f, c);

		if (c[4] < 0.0) c[4] = 0.0; else if (c[4] > 255.0) c[4] = 255.0;

		img->rowptr[y][x] = (UINT8)c[4];
	}
}

// Resize the image to the required size using area averaging
static INT32 img_process(IMAGE* img, UINT32 width, UINT32 height, INT32 /*preset*/, bool /*swapRB*/)
{
/*	static struct { double gamma; double sharpness; INT32 min; INT32 max; } presetdata[] = {
		{ 1.000, 0.000, 0x000000, 0xFFFFFF },			//  0 no effects
		{ 1.000, 1.000, 0x000000, 0xFFFFFF },			//  1 normal sharpening
		{ 1.000, 1.250, 0x000000, 0xFFFFFF },			//  2 preview 1
		{ 1.250, 1.750, 0x000000, 0xFFFFFF },			//  3 preview 2
		{ 1.000, 0.750, 0x000000, 0xFFFFFF },			//  4 preview 3
		{ 1.125, 1.500, 0x000000, 0xFFFFFF },			//  5 preview 4
		{ 1.000, 1.000, 0x000000, 0xFFFFFF },			//  6 marquee dim   1
		{ 1.250, 1.000, 0x202020, 0xFFFFFF },			//  7 marquee light 1
		{ 1.250, 1.000, 0x000000, 0xEFEFEF },			//  8 marquee dim   2
		{ 0.750, 1.750, 0x202020, 0xFFFFFF },			//  9 marquee light 2
	};*/

	IMAGE sized_img;

	double ratio = (double)(height * width) / (img->height * img->width);

/*	{
		double LUT[256];

		INT32 rdest = 0, gdest = 1, bdest = 2;
		if (swapRB) {
			rdest = 2, gdest = 1, bdest = 0;
		}

		// Compute a look-up table for gamma correction

		double min = (presetdata[preset].min & 255);
		double rng = ((presetdata[preset].max & 255) - (presetdata[preset].min & 255));

		for (INT32 i = 0; i < 256; i++) {

			LUT[i] = min + rng * pow((i / 255.0), presetdata[preset].gamma);
		}

		// Apply gamma
		for (UINT32 y = 0; y < img->height; y++) {
			for (UINT32 x = 0; x < img->width; x++) {
				UINT8 r = (UINT8)LUT[img->rowptr[y][x * 3 + 0]];
				UINT8 g = (UINT8)LUT[img->rowptr[y][x * 3 + 1]];
				UINT8 b = (UINT8)LUT[img->rowptr[y][x * 3 + 2]];

				img->rowptr[y][x * 3 + rdest] = r;
				img->rowptr[y][x * 3 + gdest] = g;
				img->rowptr[y][x * 3 + bdest] = b;
			}
		}
	}*/

	if (img->height == height && img->width == width) {

		// We don't need to resize the image

		return 0;
	}

	memset(&sized_img, 0, sizeof(IMAGE));
	sized_img.width = width;
	sized_img.height = height;
	img_alloc(&sized_img);

	if (ratio >= 1.0) {

		double* row[4];

		// Enlarge the image using bi-cubic filtering

		for (INT32 i = 0; i < 4; i++) {
			row[i] = (double*)malloc(width * (3+1) * sizeof(double)); // added +1 to prevent crash with corrupt png
		}

		interpolateRowH(img, 0, row[0], width);
		interpolateRowH(img, 0, row[1], width);
		interpolateRowH(img, 1, row[2], width);
		interpolateRowH(img, 2, row[3], width);

		for (UINT32 y = 0, ylast = 0, y2 = 0; y < height; ylast = y2, y++) {
			double f = (double)y * img->height / height;

			y2 = (UINT32)f; f -= y2;

			if (y2 > ylast) {
				double* r = row[0];
				row[0] = row[1];
				row[1] = row[2];
				row[2] = row[3];
				row[3] = r;

				interpolateRowH(img, (y2 + 2) < img->height ? y2 + 2 : img->height - 1, row[3], width);
			}

			interpolateRowV(f, y, row, &sized_img);
		}

		for (INT32 i = 0; i < 4; i++) {
			if (row[i]) {
				free(row[i]);
				row[i] = NULL;
			}
		}

		img_free(img);

		memcpy(img, &sized_img, sizeof(IMAGE));

		return 0;
	}

	// Shrink the image using area averaging and  apply gamma

	for (UINT32 y = 0; y < sized_img.height; y++) {
		for (UINT32 x = 0; x < sized_img.width; x++) {

			double r0, b0, g0, r1, g1, b1, xf, yf;
			UINT32 x1, y1;

			r0 = g0 = b0 = 0.0;

			y1 = img->height * y / sized_img.height;
			yf = (double)img->height * y / sized_img.height - y1;

			x1 = img->width * x / sized_img.width;
			xf = (double)img->width * x / sized_img.width - x1;
			r1 = (double)img->rowptr[y1][x1 * 3 + 0] * (1.0 - xf);
			g1 = (double)img->rowptr[y1][x1 * 3 + 1] * (1.0 - xf);
			b1 = (double)img->rowptr[y1][x1 * 3 + 2] * (1.0 - xf);

			for (x1 = x1 + 1; x1 < img->width * (x + 1) / sized_img.width; x1++) {
				r1 += (double)img->rowptr[y1][x1 * 3 + 0];
				g1 += (double)img->rowptr[y1][x1 * 3 + 1];
				b1 += (double)img->rowptr[y1][x1 * 3 + 2];
			}

			if (x1 < img->width) {
				xf = (double)img->width * (x + 1) / sized_img.width - x1;
				r1 += (double)img->rowptr[y1][x1 * 3 + 0] * xf;
				g1 += (double)img->rowptr[y1][x1 * 3 + 1] * xf;
				b1 += (double)img->rowptr[y1][x1 * 3 + 2] * xf;
			}

			r0 += r1 * (1.0 - yf);
			g0 += g1 * (1.0 - yf);
			b0 += b1 * (1.0 - yf);

			for (y1 = y1 + 1; y1 < img->height * (y + 1) / sized_img.height; y1++) {
				x1 = img->width * x / sized_img.width;
				xf = (double)img->width * x / sized_img.width - x1;

				r1 = (double)img->rowptr[y1][x1 * 3 + 0] * (1.0 - xf);
				g1 = (double)img->rowptr[y1][x1 * 3 + 1] * (1.0 - xf);
				b1 = (double)img->rowptr[y1][x1 * 3 + 2] * (1.0 - xf);

				for (x1 = x1 + 1; x1 < img->width * (x + 1) / sized_img.width; x1++) {
					r1 += (double)img->rowptr[y1][x1 * 3 + 0];
					g1 += (double)img->rowptr[y1][x1 * 3 + 1];
					b1 += (double)img->rowptr[y1][x1 * 3 + 2];
				}

				if (x1 < img->width) {
					xf = (double)img->width * (x + 1) / sized_img.width - x1;
					r1 += (double)img->rowptr[y1][x1 * 3 + 0] * xf;
					g1 += (double)img->rowptr[y1][x1 * 3 + 1] * xf;
					b1 += (double)img->rowptr[y1][x1 * 3 + 2] * xf;
				}

				r0 += r1;
				g0 += g1;
				b0 += b1;
			}

			if (y1 < img->height) {
				yf = (double)img->height * (y + 1) / sized_img.height - y1;

				x1 = img->width * x / sized_img.width;
				xf = (double)img->width * x / sized_img.width - x1;
				r1 = (double)img->rowptr[y1][x1 * 3 + 0] * (1.0 - xf);
				g1 = (double)img->rowptr[y1][x1 * 3 + 1] * (1.0 - xf);
				b1 = (double)img->rowptr[y1][x1 * 3 + 2] * (1.0 - xf);

				for (x1 = x1 + 1; x1 < img->width * (x + 1) / sized_img.width; x1++) {
					r1 += (double)img->rowptr[y1][x1 * 3 + 0];
					g1 += (double)img->rowptr[y1][x1 * 3 + 1];
					b1 += (double)img->rowptr[y1][x1 * 3 + 2];
				}

				if (x1 < img->width) {
					xf = (double)img->width * (x + 1) / sized_img.width - x1;
					r1 += (double)img->rowptr[y1][x1 * 3 + 0] * xf;
					g1 += (double)img->rowptr[y1][x1 * 3 + 1] * xf;
					b1 += (double)img->rowptr[y1][x1 * 3 + 2] * xf;
				}

				r0 += r1 * yf;
				g0 += g1 * yf;
				b0 += b1 * yf;
			}

			sized_img.rowptr[y][x * 3 + 0] = (UINT8)(r0 * ratio);// + 0.5;
			sized_img.rowptr[y][x * 3 + 1] = (UINT8)(g0 * ratio);// + 0.5;
			sized_img.rowptr[y][x * 3 + 2] = (UINT8)(b0 * ratio);// + 0.5;
		}
	}

	img_free(img);

//	if (!presetdata[preset].sharpness || ratio >= 1.0) {
		memcpy(img, &sized_img, sizeof(IMAGE));

		return 0;
//	}

	// Sharpen the image using an unsharp mask

/*	IMAGE sharp_img;

	memset(&sharp_img, 0, sizeof(IMAGE));
	sharp_img.width = width;
	sharp_img.height = height;
	img_alloc(&sharp_img);

	// Create a convolution matrix for a gaussian blur

	double matrix[9][9];

	// Control the radius of the blur based on sharpness and image reduction
	double b = presetdata[preset].sharpness / (8.0 * ratio);

//	dprintf(_T("    %3ix%3i -> %3ix%3i %0.4lf %0.4lf\n"), img->width, img->height, sized_img.width, sized_img.height, ratio, b);
	if (b > 1.5) { b = 1.5; }
	b = pow(b, 2.0);

	for (INT32 x = -4; x < 5; x++) {
		for (INT32 y = -4; y < 5; y++) {

			double c = sqrt(double(x * x + y * y));

			matrix[y + 4][x + 4] = 1.0 / exp(-c * -c / b);
		}
	}

	for (INT32 y = 0; y < sized_img.height; y++) {
		for (INT32 x = 0; x < sized_img.width; x++) {

			double r, g, b, m;

			r = g = b = m = 0.0;

			// Convolve the image
			for (INT32 y1 = -4; y1 < 5; y1++) {
				if (y + y1 > 0 && y + y1 < sized_img.height) {
					for (INT32 x1 = -4; x1 < 5; x1++) {
						if (x + x1 > 0 && x + x1 < sized_img.width) {
							r += matrix[y1 + 4][x1 + 4] * sized_img.rowptr[y + y1][(x + x1) * 3 + 0];
							g += matrix[y1 + 4][x1 + 4] * sized_img.rowptr[y + y1][(x + x1) * 3 + 1];
							b += matrix[y1 + 4][x1 + 4] * sized_img.rowptr[y + y1][(x + x1) * 3 + 2];
							m += abs(matrix[y1 + 4][x1 + 4]);
						}
					}
				}
			}

			// Normalise the image
			r /= m;
			g /= m;
			b /= m;

			// create the mask by subtracting the blurred image from the original
			r = presetdata[preset].sharpness * ((double)sized_img.rowptr[y][x * 3 + 0] - r);
			g = presetdata[preset].sharpness * ((double)sized_img.rowptr[y][x * 3 + 1] - g);
			b = presetdata[preset].sharpness * ((double)sized_img.rowptr[y][x * 3 + 2] - b);

			// Implement a treshold control, rolloff beneath the treshold based on image reduction

			double treshold = 32.0 / presetdata[preset].sharpness;

			if (abs(r) < treshold && abs(g) < treshold && abs(b) < treshold) {
				 r /= 1.0 + ((treshold - abs(r)) * ratio);
				 g /= 1.0 + ((treshold - abs(g)) * ratio);
				 b /= 1.0 + ((treshold - abs(b)) * ratio);
			}

			// Add the mask back to the original
			r = (double)sized_img.rowptr[y][x * 3 + 0] + r;
			g = (double)sized_img.rowptr[y][x * 3 + 1] + g;
			b = (double)sized_img.rowptr[y][x * 3 + 2] + b;

			// Clamp RGB values
			if (r < 0) { r = 0; } if (r > 255) { r = 255; }
			if (g < 0) { g = 0; } if (g > 255) { g = 255; }
			if (b < 0) { b = 0; } if (b > 255) { b = 255; }

			// Store image
			sharp_img.rowptr[y][x * 3 + 0] = r;
			sharp_img.rowptr[y][x * 3 + 1] = g;
			sharp_img.rowptr[y][x * 3 + 2] = b;
		}
	}*/

	img_free(&sized_img);

//	memcpy(img, &sharp_img, sizeof(IMAGE));

	return 0;
}

int png_sig_check(UINT8 *sig)
{
	return memcmp(sig, PNG_SIG, PNG_SIG_LEN);
}

bool PNGIsImage(FILE* fp)
{
	if (fp) {
		UINT8 pngsig[PNG_SIG_LEN];

		fseek(fp, 0, SEEK_SET);
		fread(pngsig, 1, PNG_SIG_LEN, fp);
		fseek(fp, 0, SEEK_SET);

		if (png_sig_check(pngsig) == 0) {
			return true;
		}
	}

	return false;
}

const char *color_type_str(int color_type)
{
    switch(color_type)
    {
        case SPNG_COLOR_TYPE_GRAYSCALE: return "grayscale";
        case SPNG_COLOR_TYPE_TRUECOLOR: return "truecolor";
        case SPNG_COLOR_TYPE_INDEXED: return "indexed color";
        case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA: return "grayscale with alpha";
        case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA: return "truecolor with alpha";
        default: return "(invalid)";
    }
}

INT32 PNGLoad(IMAGE* img, FILE* fp, INT32 nPreset)
{
	IMAGE temp_img;
	INT32 width = 0, height = 0;
	int ret;

	if (fp) {
		// check signature
		UINT8 pngsig[PNG_SIG_LEN];
		fread(pngsig, 1, PNG_SIG_LEN, fp);
		if (png_sig_check(pngsig)) {
			return 1;
		}
		rewind(fp);

		spng_ctx *ctx = NULL;
		struct spng_ihdr ihdr;

		ctx = spng_ctx_new(0);
		spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);
		size_t limit = 1024 * 1024 * 64;
		spng_set_chunk_limits(ctx, limit, limit);
		spng_set_png_file(ctx, fp);
		ret = spng_get_ihdr(ctx, &ihdr);

		if (ret) {
			spng_ctx_free(ctx);
			return 1;
		}

#if 0
		// debuggy stuff
		const char *color_name = color_type_str(ihdr.color_type);

		bprintf(0, _T("width: %u\n"
			   "height: %u\n"
			   "bit depth: %u\n"
			   "color type: %u - %S\n"),
			   ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.color_type, color_name);

		bprintf(0, _T("compression method: %u\n"
			   "filter method: %u\n"
			   "interlace method: %u\n"),
			   ihdr.compression_method, ihdr.filter_method, ihdr.interlace_method);

		struct spng_plte plte = { 0 };
		ret = spng_get_plte(ctx, &plte);
		if (!ret) bprintf(0, _T("palette entries: %u\n"), plte.n_entries);
#endif

		size_t image_size = 0, image_width_bytes = 0;
		int fmt = SPNG_FMT_RGB8;

		ret = spng_decoded_image_size(ctx, fmt, &image_size);
		ret = spng_decode_image(ctx, NULL, 0, fmt, SPNG_DECODE_PROGRESSIVE | SPNG_DECODE_TRNS );

		image_width_bytes = image_size / ihdr.height; // it's the width * 3 (1 for each RGB)

		memset(&temp_img, 0, sizeof(IMAGE));
		temp_img.width  = ihdr.width;
		temp_img.height = ihdr.height;

		if (img_alloc(&temp_img)) {
			spng_ctx_free(ctx);
			return 1;
		}

		struct spng_row_info row_info = {0};

		// deal with transparency that decodes to non-black
		// testcases: dacholer, dowild, ...
		int trans_to_black = 0;
		struct spng_plte plte;
		struct spng_trns trns;

		trans_to_black =
			(ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) &&
			(spng_get_plte(ctx, &plte) == 0) &&
			(spng_get_trns(ctx, &trns) == 0);

		do {
			if ((ret = spng_get_row_info(ctx, &row_info)) != 0) break;

			ret = spng_decode_row(ctx, temp_img.rowptr[row_info.row_num], image_width_bytes);

			for (int i = 0; i < temp_img.width * 3; i += 3) {
				int r = temp_img.rowptr[row_info.row_num][i + 0];
				int g = temp_img.rowptr[row_info.row_num][i + 1];
				int b = temp_img.rowptr[row_info.row_num][i + 2];

				if (trans_to_black) {
					for(int j = 0; j < trns.n_type3_entries; j++)
					{
						if(trns.type3_alpha[j] < 20 &&
						   plte.entries[trns.type3_alpha[j]].red == r &&
						   plte.entries[trns.type3_alpha[j]].green == g &&
						   plte.entries[trns.type3_alpha[j]].blue == b)
						{
							r = r * trns.type3_alpha[j] / 20;
							g = g * trns.type3_alpha[j] / 20;
							b = b * trns.type3_alpha[j] / 20;
						}
					}
				}

				temp_img.rowptr[row_info.row_num][i + 0] = b;
				temp_img.rowptr[row_info.row_num][i + 1] = g;
				temp_img.rowptr[row_info.row_num][i + 2] = r;
			}
		} while (ret == 0);

		spng_ctx_free(ctx);
	} else {

#ifdef BUILD_WIN32
		// Find resource
		HRSRC hrsrc = FindResource(NULL, MAKEINTRESOURCE(BMP_SPLASH), RT_BITMAP);
		HGLOBAL hglobal = LoadResource(NULL, (HRSRC)hrsrc);
		BYTE* pResourceData = (BYTE*)LockResource(hglobal);

		BITMAPINFOHEADER* pbmih = (BITMAPINFOHEADER*)LockResource(hglobal);

		// Allocate a new image
		memset(&temp_img, 0, sizeof(IMAGE));
		temp_img.width   = pbmih->biWidth;
		temp_img.height  = pbmih->biHeight;
		temp_img.bmpbits = pResourceData + pbmih->biSize;
		img_alloc(&temp_img);

#else
		return 1;
#endif

	}

	if (img_process(&temp_img, img->width ? img->width : temp_img.width, img->height ? img->height : temp_img.height, nPreset, false)) {
		img_free(&temp_img);
		return 1;
	}

	bPngImageOrientation = 0;
	if (height && width && height > width) bPngImageOrientation = 1;

	memcpy(img, &temp_img, sizeof(IMAGE));

	return 0;
}

INT32 PNGGetInfo(IMAGE* img, FILE *fp)
{
	IMAGE temp_img = { 0, 0, 0, 0, NULL, NULL, 0 };

	if (fp) {
		// check signature
		UINT8 pngsig[PNG_SIG_LEN];
		fread(pngsig, 1, PNG_SIG_LEN, fp);
		if (png_sig_check(pngsig)) {
			return 1;
		}
		rewind(fp);

		spng_ctx *ctx = NULL;
		struct spng_ihdr ihdr;

		ctx = spng_ctx_new(0);
		spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);
		size_t limit = 1024 * 1024 * 64;
		spng_set_chunk_limits(ctx, limit, limit);
		spng_set_png_file(ctx, fp);
		int ret = spng_get_ihdr(ctx, &ihdr);

		if (ret) {
			// can't decode header info's - bad png
			return 1;
		}

		memset(&temp_img, 0, sizeof(IMAGE));
		temp_img.width = ihdr.width;
		temp_img.height = ihdr.height;

		spng_ctx_free(ctx);
	}

	memcpy(img, &temp_img, sizeof(IMAGE));
	img_free(&temp_img);

	return 0;
}
