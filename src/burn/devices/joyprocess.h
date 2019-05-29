// ---[ ProcessJoystick() Flags (grep ProcessJoystick in drv/pre90s for examples)
#define INPUT_4WAY              0x02  // convert 8-way inputs to 4-way
#define INPUT_CLEAROPPOSITES    0x04  // disallow up+down or left+right
#define INPUT_MAKEACTIVELOW     0x08  // input is active-high, make active-low after processing
#define INPUT_ISACTIVELOW       0x10  // input is active-low to begin with

void ProcessJoystick(UINT8 *input, INT8 playernum, INT8 up_bit, INT8 down_bit, INT8 left_bit, INT8 right_bit, UINT8 flags);
void CompileInput(UINT8 **input, void *output, INT32 num, INT32 bits, UINT32 *init);

// ---[ ProcessAnalog() flags
// by default, it centers at the midpoint between scalemin and scalemax.
// use INPUT_LINEAR to change to linear-mode.
#define INPUT_DEADZONE          0x01
#define INPUT_LINEAR            0x02

// for analog pedals, allows a mapped digital button to work
#define INPUT_MIGHTBEDIGITAL    0x04

UINT8 ProcessAnalog(INT16 anaval, INT32 reversed, INT32 flags, UINT8 scalemin, UINT8 scalemax);
UINT8 ProcessAnalog(INT16 anaval, INT32 reversed, INT32 flags, UINT8 scalemin, UINT8 scalemax, UINT8 centerval);

INT16 AnalogDeadZone(INT16 anaval);

UINT32 scalerange(UINT32 x, UINT32 in_min, UINT32 in_max, UINT32 out_min, UINT32 out_max);
