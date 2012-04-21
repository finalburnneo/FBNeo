#ifndef _XBR_H
#define _XBR_H

void xbr2x(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres, int flavour);
			
void xbr3x(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres, int flavour);

void xbr4x(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres, int flavour);
			
#endif
