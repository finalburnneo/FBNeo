extern void MDYM2612Init(void);
extern void MDYM2612Exit(void);
extern void MDYM2612Config(unsigned char dac_bits);
extern void MDYM2612Reset(void);
extern void MDYM2612Update(INT16 **buffer, int length);
extern void MDYM2612Write(unsigned int a, unsigned int v);
extern unsigned int MDYM2612Read(void);
extern int MDYM2612LoadContext();
extern int MDYM2612SaveContext();

void BurnMD2612UpdateRequest();
