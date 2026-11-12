// hq2x filter
// Copyright (C) 2003 MaxSt ( maxst@hiend3d.com ), GPL v2 or later - see hqx.h
//
// Pattern table translated from hq2x32.asm.
// Output pixels are numbered left-to-right, top-to-bottom (o[0] .. o[3]).

#include "hqx.h"

static void hq2x_pattern(int pattern, const hq_u32* w, const hq_u32* y, hq_u32 c1, hq_u32 c2, hq_u32 c3, hq_u32 c4, hq_u32 c5, hq_u32 c6, hq_u32 c7, hq_u32 c8, hq_u32 c9, hq_u32* o)
{
	switch (pattern) {
		case 0: case 1: case 4: case 32: case 128: case 5: case 132: case 160: case 33: case 129: case 36: case 133: case 164: case 161: case 37: case 165:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 2: case 34: case 130: case 162:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 16: case 17: case 48: case 49:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 64: case 65: case 68: case 69:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 8: case 12: case 136: case 140:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 3: case 35: case 131: case 163:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 6: case 38: case 134: case 166:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 20: case 21: case 52: case 53:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 144: case 145: case 176: case 177:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 192: case 193: case 196: case 197:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 96: case 97: case 100: case 101:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 40: case 44: case 168: case 172:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 9: case 13: case 137: case 141:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 18: case 50:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 80: case 81:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 72: case 76:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 10: case 138:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 66:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 24:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 7: case 39: case 135:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 148: case 149: case 180:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 224: case 228: case 225:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 41: case 169: case 45:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 22: case 54:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 208: case 209:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 104: case 108:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 11: case 139:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 19: case 51:
			if (Diff(2, 6)) { o[0] = Mix2(c5,3,c4,1,2); o[1] = Mix2(c5,3,c3,1,2); } else { o[0] = Mix3(c5,5,c2,2,c4,1,3); o[1] = Mix3(c5,2,c2,3,c6,3,3); }
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 146: case 178:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); o[3] = Mix2(c5,3,c8,1,2); } else { o[1] = Mix3(c5,2,c2,3,c6,3,3); o[3] = Mix3(c5,5,c6,2,c8,1,3); }
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			break;
		case 84: case 85:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			if (Diff(6, 8)) { o[1] = Mix2(c5,3,c2,1,2); o[3] = Mix2(c5,3,c9,1,2); } else { o[1] = Mix3(c5,5,c6,2,c2,1,3); o[3] = Mix3(c5,2,c6,3,c8,3,3); }
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			break;
		case 112: case 113:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			if (Diff(6, 8)) { o[2] = Mix2(c5,3,c4,1,2); o[3] = Mix2(c5,3,c9,1,2); } else { o[2] = Mix3(c5,5,c8,2,c4,1,3); o[3] = Mix3(c5,2,c6,3,c8,3,3); }
			break;
		case 200: case 204:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); o[3] = Mix2(c5,3,c6,1,2); } else { o[2] = Mix3(c5,2,c8,3,c4,3,3); o[3] = Mix3(c5,5,c8,2,c6,1,3); }
			break;
		case 73: case 77:
			if (Diff(8, 4)) { o[0] = Mix2(c5,3,c2,1,2); o[2] = Mix2(c5,3,c7,1,2); } else { o[0] = Mix3(c5,5,c4,2,c2,1,3); o[2] = Mix3(c5,2,c8,3,c4,3,3); }
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 42: case 170:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); o[2] = Mix2(c5,3,c8,1,2); } else { o[0] = Mix3(c5,2,c4,3,c2,3,3); o[2] = Mix3(c5,5,c4,2,c8,1,3); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 14: case 142:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); o[1] = Mix2(c5,3,c6,1,2); } else { o[0] = Mix3(c5,2,c4,3,c2,3,3); o[1] = Mix3(c5,5,c2,2,c6,1,3); }
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 67:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 70:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 28:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 152:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 194:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 98:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 56:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 25:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 26: case 31:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 82: case 214:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 88: case 248:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 74: case 107:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 27:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix2(c5,3,c3,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 86:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 216:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix2(c5,3,c7,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 106:
			o[0] = Mix2(c5,3,c1,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 30:
			o[0] = Mix2(c5,3,c1,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 210:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix2(c5,3,c3,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 120:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 75:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix2(c5,3,c7,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 29:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 198:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 184:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 99:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 57:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 71:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 156:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 226:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 60:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 195:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 102:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 153:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 58:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 83:
			o[0] = Mix2(c5,3,c4,1,2);
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 92:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 202:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 78:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			o[1] = Mix2(c5,3,c6,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 154:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 114:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix2(c5,3,c4,1,2);
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 89:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 90:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 55: case 23:
			if (Diff(2, 6)) { o[0] = Mix2(c5,3,c4,1,2); o[1] = c5; } else { o[0] = Mix3(c5,5,c2,2,c4,1,3); o[1] = Mix3(c5,2,c2,3,c6,3,3); }
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 182: case 150:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; o[3] = Mix2(c5,3,c8,1,2); } else { o[1] = Mix3(c5,2,c2,3,c6,3,3); o[3] = Mix3(c5,5,c6,2,c8,1,3); }
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			break;
		case 213: case 212:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			if (Diff(6, 8)) { o[1] = Mix2(c5,3,c2,1,2); o[3] = c5; } else { o[1] = Mix3(c5,5,c6,2,c2,1,3); o[3] = Mix3(c5,2,c6,3,c8,3,3); }
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			break;
		case 241: case 240:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			if (Diff(6, 8)) { o[2] = Mix2(c5,3,c4,1,2); o[3] = c5; } else { o[2] = Mix3(c5,5,c8,2,c4,1,3); o[3] = Mix3(c5,2,c6,3,c8,3,3); }
			break;
		case 236: case 232:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; o[3] = Mix2(c5,3,c6,1,2); } else { o[2] = Mix3(c5,2,c8,3,c4,3,3); o[3] = Mix3(c5,5,c8,2,c6,1,3); }
			break;
		case 109: case 105:
			if (Diff(8, 4)) { o[0] = Mix2(c5,3,c2,1,2); o[2] = c5; } else { o[0] = Mix3(c5,5,c4,2,c2,1,3); o[2] = Mix3(c5,2,c8,3,c4,3,3); }
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 171: case 43:
			if (Diff(4, 2)) { o[0] = c5; o[2] = Mix2(c5,3,c8,1,2); } else { o[0] = Mix3(c5,2,c4,3,c2,3,3); o[2] = Mix3(c5,5,c4,2,c8,1,3); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 143: case 15:
			if (Diff(4, 2)) { o[0] = c5; o[1] = Mix2(c5,3,c6,1,2); } else { o[0] = Mix3(c5,2,c4,3,c2,3,3); o[1] = Mix3(c5,5,c2,2,c6,1,3); }
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 124:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 203:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix2(c5,3,c7,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 62:
			o[0] = Mix2(c5,3,c1,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 211:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix2(c5,3,c3,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 118:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 217:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix2(c5,3,c7,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 110:
			o[0] = Mix2(c5,3,c1,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 155:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix2(c5,3,c3,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 188:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 185:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 61:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 157:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 103:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 227:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 230:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 199:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 220:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 158:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 234:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 242:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix2(c5,3,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 59:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 121:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 87:
			o[0] = Mix2(c5,3,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 79:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix2(c5,3,c6,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 122:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 94:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 218:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 91:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 229:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 167:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 173:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 181:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 186:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 115:
			o[0] = Mix2(c5,3,c4,1,2);
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix2(c5,3,c4,1,2);
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 93:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 206:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			o[1] = Mix2(c5,3,c6,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 205: case 201:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = Mix2(c5,3,c7,1,2); } else { o[2] = Mix3(c5,6,c8,1,c4,1,3); }
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 174: case 46:
			if (Diff(4, 2)) { o[0] = Mix2(c5,3,c1,1,2); } else { o[0] = Mix3(c5,6,c4,1,c2,1,3); }
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 179: case 147:
			o[0] = Mix2(c5,3,c4,1,2);
			if (Diff(2, 6)) { o[1] = Mix2(c5,3,c3,1,2); } else { o[1] = Mix3(c5,6,c2,1,c6,1,3); }
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 117: case 116:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			if (Diff(6, 8)) { o[3] = Mix2(c5,3,c9,1,2); } else { o[3] = Mix3(c5,6,c6,1,c8,1,3); }
			break;
		case 189:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 231:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 126:
			o[0] = Mix2(c5,3,c1,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 219:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix2(c5,3,c3,1,2);
			o[2] = Mix2(c5,3,c7,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 125:
			if (Diff(8, 4)) { o[0] = Mix2(c5,3,c2,1,2); o[2] = c5; } else { o[0] = Mix3(c5,5,c4,2,c2,1,3); o[2] = Mix3(c5,2,c8,3,c4,3,3); }
			o[1] = Mix2(c5,3,c2,1,2);
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 221:
			o[0] = Mix2(c5,3,c2,1,2);
			if (Diff(6, 8)) { o[1] = Mix2(c5,3,c2,1,2); o[3] = c5; } else { o[1] = Mix3(c5,5,c6,2,c2,1,3); o[3] = Mix3(c5,2,c6,3,c8,3,3); }
			o[2] = Mix2(c5,3,c7,1,2);
			break;
		case 207:
			if (Diff(4, 2)) { o[0] = c5; o[1] = Mix2(c5,3,c6,1,2); } else { o[0] = Mix3(c5,2,c4,3,c2,3,3); o[1] = Mix3(c5,5,c2,2,c6,1,3); }
			o[2] = Mix2(c5,3,c7,1,2);
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 238:
			o[0] = Mix2(c5,3,c1,1,2);
			o[1] = Mix2(c5,3,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; o[3] = Mix2(c5,3,c6,1,2); } else { o[2] = Mix3(c5,2,c8,3,c4,3,3); o[3] = Mix3(c5,5,c8,2,c6,1,3); }
			break;
		case 190:
			o[0] = Mix2(c5,3,c1,1,2);
			if (Diff(2, 6)) { o[1] = c5; o[3] = Mix2(c5,3,c8,1,2); } else { o[1] = Mix3(c5,2,c2,3,c6,3,3); o[3] = Mix3(c5,5,c6,2,c8,1,3); }
			o[2] = Mix2(c5,3,c8,1,2);
			break;
		case 187:
			if (Diff(4, 2)) { o[0] = c5; o[2] = Mix2(c5,3,c8,1,2); } else { o[0] = Mix3(c5,2,c4,3,c2,3,3); o[2] = Mix3(c5,5,c4,2,c8,1,3); }
			o[1] = Mix2(c5,3,c3,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 243:
			o[0] = Mix2(c5,3,c4,1,2);
			o[1] = Mix2(c5,3,c3,1,2);
			if (Diff(6, 8)) { o[2] = Mix2(c5,3,c4,1,2); o[3] = c5; } else { o[2] = Mix3(c5,5,c8,2,c4,1,3); o[3] = Mix3(c5,2,c6,3,c8,3,3); }
			break;
		case 119:
			if (Diff(2, 6)) { o[0] = Mix2(c5,3,c4,1,2); o[1] = c5; } else { o[0] = Mix3(c5,5,c2,2,c4,1,3); o[1] = Mix3(c5,2,c2,3,c6,3,3); }
			o[2] = Mix2(c5,3,c4,1,2);
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 237: case 233:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c2,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,14,c8,1,c4,1,4); }
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 175: case 47:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,14,c4,1,c2,1,4); }
			o[1] = Mix2(c5,3,c6,1,2);
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c6,1,c8,1,2);
			break;
		case 183: case 151:
			o[0] = Mix2(c5,3,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,14,c2,1,c6,1,4); }
			o[2] = Mix3(c5,2,c8,1,c4,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 245: case 244:
			o[0] = Mix3(c5,2,c4,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			o[2] = Mix2(c5,3,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,14,c6,1,c8,1,4); }
			break;
		case 250:
			o[0] = Mix2(c5,3,c1,1,2);
			o[1] = Mix2(c5,3,c3,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 123:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix2(c5,3,c3,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 95:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix2(c5,3,c7,1,2);
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 222:
			o[0] = Mix2(c5,3,c1,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix2(c5,3,c7,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 252:
			o[0] = Mix3(c5,2,c1,1,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,14,c6,1,c8,1,4); }
			break;
		case 249:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix3(c5,2,c3,1,c2,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,14,c8,1,c4,1,4); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 235:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix3(c5,2,c3,1,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,14,c8,1,c4,1,4); }
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 111:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,14,c4,1,c2,1,4); }
			o[1] = Mix2(c5,3,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix3(c5,2,c9,1,c6,1,2);
			break;
		case 63:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,14,c4,1,c2,1,4); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix3(c5,2,c9,1,c8,1,2);
			break;
		case 159:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,14,c2,1,c6,1,4); }
			o[2] = Mix3(c5,2,c7,1,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 215:
			o[0] = Mix2(c5,3,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,14,c2,1,c6,1,4); }
			o[2] = Mix3(c5,2,c7,1,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 246:
			o[0] = Mix3(c5,2,c1,1,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			o[2] = Mix2(c5,3,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,14,c6,1,c8,1,4); }
			break;
		case 254:
			o[0] = Mix2(c5,3,c1,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,14,c6,1,c8,1,4); }
			break;
		case 253:
			o[0] = Mix2(c5,3,c2,1,2);
			o[1] = Mix2(c5,3,c2,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,14,c8,1,c4,1,4); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,14,c6,1,c8,1,4); }
			break;
		case 251:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			o[1] = Mix2(c5,3,c3,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,14,c8,1,c4,1,4); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 239:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,14,c4,1,c2,1,4); }
			o[1] = Mix2(c5,3,c6,1,2);
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,14,c8,1,c4,1,4); }
			o[3] = Mix2(c5,3,c6,1,2);
			break;
		case 127:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,14,c4,1,c2,1,4); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,2,c2,1,c6,1,2); }
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,2,c8,1,c4,1,2); }
			o[3] = Mix2(c5,3,c9,1,2);
			break;
		case 191:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,14,c4,1,c2,1,4); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,14,c2,1,c6,1,4); }
			o[2] = Mix2(c5,3,c8,1,2);
			o[3] = Mix2(c5,3,c8,1,2);
			break;
		case 223:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,2,c4,1,c2,1,2); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,14,c2,1,c6,1,4); }
			o[2] = Mix2(c5,3,c7,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,2,c6,1,c8,1,2); }
			break;
		case 247:
			o[0] = Mix2(c5,3,c4,1,2);
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,14,c2,1,c6,1,4); }
			o[2] = Mix2(c5,3,c4,1,2);
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,14,c6,1,c8,1,4); }
			break;
		case 255:
			if (Diff(4, 2)) { o[0] = c5; } else { o[0] = Mix3(c5,14,c4,1,c2,1,4); }
			if (Diff(2, 6)) { o[1] = c5; } else { o[1] = Mix3(c5,14,c2,1,c6,1,4); }
			if (Diff(8, 4)) { o[2] = c5; } else { o[2] = Mix3(c5,14,c8,1,c4,1,4); }
			if (Diff(6, 8)) { o[3] = c5; } else { o[3] = Mix3(c5,14,c6,1,c8,1,4); }
			break;
	}
}

void hq2x(unsigned char* src, unsigned int srcPitch, unsigned char* dst, unsigned int dstPitch, int width, int height)
{
	HqRunDepth<2>(hq2x_pattern, src, srcPitch, dst, dstPitch, width, height);
}
