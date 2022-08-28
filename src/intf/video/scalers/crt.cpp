/*
By Kannagichan (kannagichan@gmail.com)
*/

#include "crt.h"
static unsigned char line_buf[0x8000];

static inline void CRT_initline(unsigned char *dstPtr,int l,int n,int bytepixel)
{
	for(int j = 0;j < n;j+= bytepixel)
	{
		dstPtr[l+j+0] = (line_buf[j+0])&0xF8;
		dstPtr[l+j+1] = (line_buf[j+1])&0xF8;
		dstPtr[l+j+2] = (line_buf[j+2])&0xF8;
		j+= bytepixel;

		dstPtr[l+j+0] = (line_buf[j+0])|7;
		dstPtr[l+j+1] = (line_buf[j+1])|7;
		dstPtr[l+j+2] = (line_buf[j+2])|7;
	}

}

static inline void CRT_drawline(unsigned char *dstPtr,int l,int n,int bytepixel,float *fading)
{

	for(int j = 0;j < n;j+= bytepixel)
	{
		dstPtr[l+j+0] = (line_buf[j+0])*fading[0];
		dstPtr[l+j+1] = (line_buf[j+1])*fading[0];
		dstPtr[l+j+2] = (line_buf[j+2])*fading[0];
		j+= bytepixel;

		dstPtr[l+j+0] = (line_buf[j+0])*fading[1];
		dstPtr[l+j+1] = (line_buf[j+1])*fading[1];
		dstPtr[l+j+2] = (line_buf[j+2])*fading[1];
	}

}


void CRTx22(unsigned char *srcPtr,unsigned char *dstPtr,int width, int height,int srcpitch,int pitch)
{
	const int bytepixel = 4;
	int i;

	int R = 0,G = 0,B = 0,RS,GS,BS;
	int l = 0;
	int x,y;

	for(y = 0;y < height;y++)
	{
		for(x = 0;x < width;x++)
		{

			i = (x*bytepixel) + (y*srcpitch);
			R = srcPtr[i+0];
			G = srcPtr[i+1];
			B = srcPtr[i+2];

			if(x != width-1)
			{
				i+=bytepixel;
				RS = ( ((int)srcPtr[i+0]+R)/2 );
				GS = ( ((int)srcPtr[i+1]+G)/2 );
				BS = ( ((int)srcPtr[i+2]+B)/2 );
			}
			else
			{
				RS = R;
				GS = G;
				BS = B;
			}

			int tmp = x*bytepixel*2;
			line_buf[tmp+0] = R;
			line_buf[tmp+1] = G;
			line_buf[tmp+2] = B;
			tmp += bytepixel;
			line_buf[tmp+0] = RS;
			line_buf[tmp+1] = GS;
			line_buf[tmp+2] = BS;

		}

		int n = bytepixel*width*2;

		CRT_initline(dstPtr,l,n,bytepixel);
		l += pitch;

		float fading[2];

		fading[0] = 1.0/1.25;
		fading[1] = 1.0/1.125;

		CRT_drawline(dstPtr,l,n,bytepixel,fading);

		l += pitch;


	}

}

void CRTx32(unsigned char *srcPtr,unsigned char *dstPtr,int width, int height,int srcpitch,int pitch)
{
	const int bytepixel = 4;
	int i;
	int R = 0,G = 0,B = 0,RS[3],GS[3],BS[3];
	int l = 0,j;
	int x,y;

	for(y = 0;y < height;y++)
	{
		for(x = 0;x < width;x++)
		{
			i = (x*bytepixel) + (y*srcpitch);
			R = srcPtr[i+0];
			G = srcPtr[i+1];
			B = srcPtr[i+2];

			if(x != width-1)
			{
				i+=bytepixel;
				RS[0] = ( ((float)srcPtr[i+0]*0.3)+((float)R*0.7) );
				GS[0] = ( ((float)srcPtr[i+1]*0.3)+((float)G*0.7) );
				BS[0] = ( ((float)srcPtr[i+2]*0.3)+((float)B*0.7) );


				RS[2] = ( ((float)srcPtr[i+0]*0.7)+((float)R*0.4) );
				GS[2] = ( ((float)srcPtr[i+1]*0.7)+((float)G*0.4) );
				BS[2] = ( ((float)srcPtr[i+2]*0.7)+((float)B*0.4) );


			}
			else
			{
				for(j = 0;j < 2;j++)
				{
					RS[j] = R;
					GS[j] = G;
					BS[j] = B;
				}
			}

			int tmp = x*bytepixel*4;
			line_buf[tmp+0] = R;
			line_buf[tmp+1] = G;
			line_buf[tmp+2] = B;
			line_buf[tmp+3] = 0xFF;

			for(j = 0;j < 2;j++)
			{
				line_buf[tmp+4] = RS[j];
				line_buf[tmp+5] = GS[j];
				line_buf[tmp+6] = BS[j];
				line_buf[tmp+7] = 0xFF;

				tmp += bytepixel;
			}
		}

		int n = bytepixel*width*2;

		CRT_initline(dstPtr,l,n,bytepixel);
		l += pitch;

		float fading[2];

		fading[0] = 1.0/1.25;
		fading[1] = 1.0/1.125;

		CRT_drawline(dstPtr,l,n,bytepixel,fading);

		l += pitch;
	}

}

void CRTx33(unsigned char *srcPtr,unsigned char *dstPtr,int width, int height,int srcpitch,int pitch)
{
	const int bytepixel = 4;
	int i;
	int R = 0,G = 0,B = 0,RS[3],GS[3],BS[3];
	int l = 0,j;
	int x,y;

	for(y = 0;y < height;y++)
	{
		for(x = 0;x < width;x++)
		{
			i = (x*bytepixel) + (y*srcpitch);
			R = srcPtr[i+0];
			G = srcPtr[i+1];
			B = srcPtr[i+2];

			if(x != width-1)
			{
				i+=bytepixel;
				RS[0] = ( ((float)srcPtr[i+0]*0.3)+((float)R*0.7) );
				GS[0] = ( ((float)srcPtr[i+1]*0.3)+((float)G*0.7) );
				BS[0] = ( ((float)srcPtr[i+2]*0.3)+((float)B*0.7) );


				RS[2] = ( ((float)srcPtr[i+0]*0.7)+((float)R*0.4) );
				GS[2] = ( ((float)srcPtr[i+1]*0.7)+((float)G*0.4) );
				BS[2] = ( ((float)srcPtr[i+2]*0.7)+((float)B*0.4) );


			}
			else
			{
				for(j = 0;j < 2;j++)
				{
					RS[j] = R;
					GS[j] = G;
					BS[j] = B;
				}
			}

			int tmp = x*bytepixel*4;
			line_buf[tmp+0] = R;
			line_buf[tmp+1] = G;
			line_buf[tmp+2] = B;
			line_buf[tmp+3] = 0xFF;

			for(j = 0;j < 2;j++)
			{
				line_buf[tmp+4] = RS[j];
				line_buf[tmp+5] = GS[j];
				line_buf[tmp+6] = BS[j];
				line_buf[tmp+7] = 0xFF;

				tmp += bytepixel;
			}
		}

		int n = bytepixel*width*4;
		CRT_initline(dstPtr,l,n,bytepixel);
		l += pitch;

		float fading[2];
		for(i = 0;i < 2;i++)
		{
			if(i == 0)
			{
				fading[0] = 1.0/1.25;
				fading[1] = 1.0/1.125;
			}
			else
			{
				fading[0] = 1.0/1.75;
				fading[1] = 1.0/1.5;
			}

			CRT_drawline(dstPtr,l,n,bytepixel,fading);

			l += pitch;
		}
	}

}


void CRTx43(unsigned char *srcPtr,unsigned char *dstPtr,int width, int height,int srcpitch,int pitch)
{
	const int bytepixel = 4;
	int i;
	int R = 0,G = 0,B = 0,RS[3],GS[3],BS[3];
	int l = 0,j;
	int x,y;

	for(y = 0;y < height;y++)
	{
		for(x = 0;x < width;x++)
		{
			i = (x*bytepixel) + (y*srcpitch);
			R = srcPtr[i+0];
			G = srcPtr[i+1];
			B = srcPtr[i+2];

			if(x != width-1)
			{
				i+=bytepixel;
				RS[0] = ( ((float)srcPtr[i+0]*0.3)+((float)R*0.7) );
				GS[0] = ( ((float)srcPtr[i+1]*0.3)+((float)G*0.7) );
				BS[0] = ( ((float)srcPtr[i+2]*0.3)+((float)B*0.7) );

				RS[1] = ((int)srcPtr[i+0]+R)/2;
				GS[1] = ((int)srcPtr[i+1]+G)/2;
				BS[1] = ((int)srcPtr[i+2]+B)/2;

				RS[2] = ( ((float)srcPtr[i+0]*0.7)+((float)R*0.3) );
				GS[2] = ( ((float)srcPtr[i+1]*0.7)+((float)G*0.3) );
				BS[2] = ( ((float)srcPtr[i+2]*0.7)+((float)B*0.3) );


			}
			else
			{
				for(j = 0;j < 3;j++)
				{
					RS[j] = R;
					GS[j] = G;
					BS[j] = B;
				}
			}

			int tmp = x*bytepixel*4;
			line_buf[tmp+0] = R;
			line_buf[tmp+1] = G;
			line_buf[tmp+2] = B;
			line_buf[tmp+3] = 0xFF;

			for(j = 0;j < 3;j++)
			{
				line_buf[tmp+4] = RS[j];
				line_buf[tmp+5] = GS[j];
				line_buf[tmp+6] = BS[j];
				line_buf[tmp+7] = 0xFF;

				tmp += bytepixel;
			}
		}

		int n = bytepixel*width*4;
		CRT_initline(dstPtr,l,n,bytepixel);
		l += pitch;

		float fading[2];
		for(i = 0;i < 2;i++)
		{
			if(i == 0)
			{
				fading[0] = 1.0/1.25;
				fading[1] = 1.0/1.125;
			}
			else
			{
				fading[0] = 1.0/1.75;
				fading[1] = 1.0/1.5;
			}

			CRT_drawline(dstPtr,l,n,bytepixel,fading);

			l += pitch;
		}
	}

}

void CRTx44(unsigned char *srcPtr,unsigned char *dstPtr,int width, int height,int srcpitch,int pitch)
{
	const int bytepixel = 4;
	int i;

	int R = 0,G = 0,B = 0,RS[3],GS[3],BS[3];
	int l = 0,j;
	int x,y;

	for(y = 0;y < height;y++)
	{
		for(x = 0;x < width;x++)
		{
			i = (x*bytepixel) + (y*srcpitch);
			R = srcPtr[i+0];
			G = srcPtr[i+1];
			B = srcPtr[i+2];

			if(x != width-1)
			{
				i+=bytepixel;
				RS[0] = ( ((float)srcPtr[i+0]*0.3)+((float)R*0.7) );
				GS[0] = ( ((float)srcPtr[i+1]*0.3)+((float)G*0.7) );
				BS[0] = ( ((float)srcPtr[i+2]*0.3)+((float)B*0.7) );

				RS[1] = ((int)srcPtr[i+0]+R)/2;
				GS[1] = ((int)srcPtr[i+1]+G)/2;
				BS[1] = ((int)srcPtr[i+2]+B)/2;

				RS[2] = ( ((float)srcPtr[i+0]*0.7)+((float)R*0.3) );
				GS[2] = ( ((float)srcPtr[i+1]*0.7)+((float)G*0.3) );
				BS[2] = ( ((float)srcPtr[i+2]*0.7)+((float)B*0.3) );


			}
			else
			{
				for(j = 0;j < 3;j++)
				{
					RS[j] = R;
					GS[j] = G;
					BS[j] = B;
				}
			}

			int tmp = x*bytepixel*4;
			line_buf[tmp+0] = R;
			line_buf[tmp+1] = G;
			line_buf[tmp+2] = B;
			line_buf[tmp+3] = 0xFF;

			for(j = 0;j < 3;j++)
			{
				line_buf[tmp+4] = RS[j];
				line_buf[tmp+5] = GS[j];
				line_buf[tmp+6] = BS[j];
				line_buf[tmp+7] = 0xFF;
				tmp += bytepixel;
			}

		}

		int n = bytepixel*width*4;
		CRT_initline(dstPtr,l,n,bytepixel);
		l += pitch;

		float fading[2];
		for(i = 0;i < 3;i++)
		{
			if(i == 0)
			{
				fading[0] = 1.0/1.25;
				fading[1] = 1.0/1.125;
			}

			if(i == 1)
			{
				fading[0] = 1.0/1.75;
				fading[1] = 1.0/1.5;
			}

			if(i == 2)
			{
				fading[0] = 1.0/2;
				fading[1] = 1.0/1.8;
			}

			CRT_drawline(dstPtr,l,n,bytepixel,fading);

			l += pitch;
		}

	}

}

void CRTx54(unsigned char *srcPtr,unsigned char *dstPtr,int width, int height,int srcpitch,int pitch)
{
	const int bytepixel = 4;
	int i;

	int R = 0,G = 0,B = 0,RS[4],GS[4],BS[4];
	int l = 0,j;
	int x,y;

	for(y = 0;y < height;y++)
	{
		for(x = 0;x < width;x++)
		{
			i = (x*bytepixel) + (y*srcpitch);
			R = srcPtr[i+0];
			G = srcPtr[i+1];
			B = srcPtr[i+2];

			if(x != width-1)
			{
				i+=bytepixel;
				RS[0] = ( ((float)srcPtr[i+0]*0.2)+((float)R*0.8) );
				GS[0] = ( ((float)srcPtr[i+1]*0.2)+((float)G*0.8) );
				BS[0] = ( ((float)srcPtr[i+2]*0.2)+((float)B*0.8) );

				RS[1] = ( ((float)srcPtr[i+0]*0.4)+((float)R*0.6) );
				GS[1] = ( ((float)srcPtr[i+1]*0.4)+((float)G*0.6) );
				BS[1] = ( ((float)srcPtr[i+2]*0.4)+((float)B*0.6) );

				RS[2] = ( ((float)srcPtr[i+0]*0.6)+((float)R*0.4) );
				GS[2] = ( ((float)srcPtr[i+1]*0.6)+((float)G*0.4) );
				BS[2] = ( ((float)srcPtr[i+2]*0.6)+((float)B*0.4) );

				RS[3] = ( ((float)srcPtr[i+0]*0.8)+((float)R*0.2) );
				GS[3] = ( ((float)srcPtr[i+1]*0.8)+((float)G*0.2) );
				BS[3] = ( ((float)srcPtr[i+2]*0.8)+((float)B*0.2) );


			}
			else
			{
				for(j = 0;j < 4;j++)
				{
					RS[j] = R;
					GS[j] = G;
					BS[j] = B;
				}
			}

			int tmp = x*bytepixel*5;
			line_buf[tmp+0] = R;
			line_buf[tmp+1] = G;
			line_buf[tmp+2] = B;
			line_buf[tmp+3] = 0xFF;

			for(j = 0;j < 4;j++)
			{
				line_buf[tmp+4] = RS[j];
				line_buf[tmp+5] = GS[j];
				line_buf[tmp+6] = BS[j];
				line_buf[tmp+7] = 0xFF;
				tmp += bytepixel;
			}

		}

		int n = bytepixel*width*5;
		CRT_initline(dstPtr,l,n,bytepixel);
		l += pitch;

		float fading[2];

		for(i = 0;i < 3;i++)
		{
			if(i == 0)
			{
				fading[0] = 1.0/1.25;
				fading[1] = 1.0/1.125;
			}

			if(i == 1)
			{
				fading[0] = 1.0/1.75;
				fading[1] = 1.0/1.5;
			}

			if(i == 2)
			{
				fading[0] = 1.0/2;
				fading[1] = 1.0/1.8;
			}

			CRT_drawline(dstPtr,l,n,bytepixel,fading);

			l += pitch;
		}

	}

}
