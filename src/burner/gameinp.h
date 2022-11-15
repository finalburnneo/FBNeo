
struct giConstant {
	UINT8 nConst;				// The constant value
};

struct giSwitch {
	UINT16 nCode;				// The input code (for digital)
};

struct giJoyAxis {
	UINT8 nJoy;					// The joystick number
	UINT8 nAxis;	   			// The joystick axis number
};

struct giMouseAxis {
	UINT8 nMouse;				// The mouse number
	UINT8 nAxis;				// The axis number
	UINT16 nOffset;				// Used for absolute axes
};

struct giSliderAxis {
	UINT16 nSlider[2];			// Keys to use for slider
};

struct giSlider {
	union {
		struct giJoyAxis JoyAxis;
		struct giSliderAxis SliderAxis;
	};
	INT16 nSliderSpeed;					// speed with which keys move the slider
	INT16 nSliderCenter;				// Speed the slider should center itself (high value = slow)
	INT32 nSliderValue;					// Current position of the slider
};

struct giInput {
	union {								// Destination for the Input Value
		UINT8* pVal;
		UINT16* pShortVal;
	};
	UINT16 nVal;				// The Input Value

	union {
		struct giConstant Constant;
		struct giSwitch Switch;
		struct giJoyAxis JoyAxis;
		struct giMouseAxis MouseAxis;
		struct giSlider Slider;
	};
};

struct giForce {
	UINT8 nInput;				// The input to apply force feedback effects to
	UINT8 nEffect;				// The effect to use
};

struct giMacro {
	UINT8 nMode;				// 0 = Unused, 1 = used

	UINT8* pVal[4];				// Destination for the Input Value
	UINT8 nVal[4];				// The Input Value
	UINT8 nInput[4];			// Which inputs are mapped

	struct giSwitch Switch;

	char szName[33];			// Maximum name length 16 chars
	UINT8 nSysMacro;			// mappable system macro (1) or Auto-Fire (15)
};

#define GIT_CONSTANT		(0x01)
#define GIT_SWITCH			(0x02)

#define GIT_GROUP_SLIDER	(0x08)
#define GIT_KEYSLIDER		(0x08)
#define GIT_JOYSLIDER		(0x09)

#define GIT_GROUP_MOUSE		(0x10)
#define GIT_MOUSEAXIS		(0x10)

#define GIT_GROUP_JOYSTICK	(0x20)
#define GIT_JOYAXIS_FULL	(0x20)
#define GIT_JOYAXIS_NEG		(0x21)
#define GIT_JOYAXIS_POS		(0x22)

#define GIT_FORCE			(0x40)

#define GIT_GROUP_MACRO		(0x80)
#define GIT_MACRO_AUTO		(0x80)
#define GIT_MACRO_CUSTOM	(0x81)

// nIdent - easily find identity of a "GameImp (pgi->)"

// -( global - used without player # (GK_P#) prefix
#define GK_RESET			(0x00000001)
#define GK_SERVICE_MODE		(0x00000002)
#define GK_TILT             (0x00000003)
//                             x.......  player # prefix
#define GK_P1				(0x10000000)
#define GK_P2				(0x20000000)
#define GK_P3				(0x30000000)
#define GK_P4				(0x40000000)
#define GK_P5				(0x50000000)
#define GK_P6				(0x60000000)
#define GK_P7				(0x70000000)
#define GK_P8				(0x80000000)

// -( note: every key below will have a player # prefix
//                             ..x.....  player action
#define GK_COIN				(0x00100000)
#define GK_START			(0x00200000)
//                             ......x.  player Directionals (UDLR)
#define GK_UP				(0x00000010)
#define GK_DOWN				(0x00000020)
#define GK_LEFT				(0x00000030)
#define GK_RIGHT			(0x00000040)
//							   ....xx..  player buttons
#define GK_BUTTON1			(0x00000100)
#define GK_BUTTON2			(0x00000200)
#define GK_BUTTON3			(0x00000300)
#define GK_BUTTON4			(0x00000400)
#define GK_BUTTON5			(0x00000500)
#define GK_BUTTON6			(0x00000600)
#define GK_BUTTON7			(0x00000700)
#define GK_BUTTON8			(0x00000800)
#define GK_BUTTON9			(0x00000900)
#define GK_BUTTON10			(0x00000a00)
#define GK_BUTTON11 		(0x00000b00)
#define GK_BUTTON12 		(0x00000c00)
#define GK_BUTTON13 		(0x00000d00)
#define GK_BUTTON14 		(0x00000e00)
#define GK_BUTTON15 		(0x00000f00)
#define GK_BUTTON16 		(0x00001000)

// -( Macros for encoding & decoding nIdent
#define gi_Global2nIdent(g)		((g & 0xf) << 0)
#define gi_nIdent2Global(nI)	((nI & 0xf) >> 0)

#define gi_Player2nIdent(p)		((p & 0xf) << 28)
#define gi_nIdent2Player(nI)	((nI & 0xf) >> 28)

#define gi_Action2nIdent(a)		((a & 0xf) << 20)
#define gi_nIdent2Action(nI)	((nI & 0xf) >> 20)

#define gi_Direction2nIdent(d)	((d & 0xf) << 4)
#define gi_nIdent2Direction(nI)	((nI & 0xf) >> 4)

#define gi_Button2nIdent(b)		((b & 0x1f) << 8)
#define gi_nIdent2Button(nI)	((nI & 0x1f) >> 8)

struct GameInp {
	UINT8 nInput;				// PC side: see above
	UINT8 nType;				// game side: see burn.h

	UINT32 nIdent;				// Button identity

	union {
		struct giInput Input;
		struct giForce Force;
		struct giMacro Macro;
	};
};

