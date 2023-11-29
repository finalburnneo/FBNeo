#include "burner.h"

HBITMAP ImageToBitmap(HWND hwnd, IMAGE* img)
{
	BITMAPINFO bi;

	if (hwnd == nullptr || img == nullptr)
	{
		return nullptr;
	}

	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = img->width;
	bi.bmiHeader.biHeight = img->height;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 24;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = img->imgbytes;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	HDC hDC = GetDC(hwnd);
	BYTE* pbits = nullptr;
	HBITMAP hBitmap = CreateDIBSection(hDC, &bi, DIB_RGB_COLORS, (void**)&pbits, nullptr, 0);
	if (pbits)
	{
		memcpy(pbits, img->bmpbits, img->imgbytes);
	}
	ReleaseDC(hwnd, hDC);
	img_free(img);

	return hBitmap;
}

HBITMAP PNGLoadBitmap(HWND hWnd, FILE* fp, int nWidth, int nHeight, int nPreset)
{
	IMAGE img = {nWidth, nHeight, 0, 0, nullptr, nullptr, 0};

	if (PNGLoad(&img, fp, nPreset))
	{
		return nullptr;
	}

	return ImageToBitmap(hWnd, &img);
}

HBITMAP LoadBitmap(HWND hWnd, FILE* fp, int nWidth, int nHeight, int nPreset)
{
	if (hWnd == nullptr || fp == nullptr)
	{
		return nullptr;
	}

	if (PNGIsImage(fp)) return PNGLoadBitmap(hWnd, fp, nWidth, nHeight, nPreset);

	return nullptr;
}
