#define abs32(value) (value & 0x7FFFFFFF)
#define abs16(value) (value & 0x7FFF)

const  int	Ymask = 0x00FF0000;
const  int	Umask = 0x0000FF00;
const  int	Vmask = 0x000000FF;
const  int	trY   = 0x00300000;
const  int	trU   = 0x00000700;
const  int	trV   = 0x00000006;

void Interp1(unsigned char * pc, unsigned int c1, unsigned int c2);
void Interp2(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3);
void Interp3(unsigned char * pc, unsigned int c1, unsigned int c2);
void Interp4(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3);
void Interp5(unsigned char * pc, unsigned int c1, unsigned int c2);
void Interp1_16(unsigned char * pc, unsigned short c1, unsigned short c2);
void Interp2_16(unsigned char * pc, unsigned short c1, unsigned short c2, unsigned short c3);
void Interp3_16(unsigned char * pc, unsigned short c1, unsigned short c2);
void Interp4_16(unsigned char * pc, unsigned short c1, unsigned short c2, unsigned short c3);
void Interp5_16(unsigned char * pc, unsigned short c1, unsigned short c2);
bool Diff(unsigned int c1, unsigned int c2);
unsigned int RGBtoYUV(unsigned int c);
