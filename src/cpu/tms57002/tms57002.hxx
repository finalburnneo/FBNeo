#ifdef CDEC1
case 0x01: // abs
  *op = 4 + sfao(st1);
  break;

case 0x02: // neg
  *op = 6 + sfao(st1);
  break;

case 0x03: // add
  *op = 8 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1);
  break;

case 0x04: // add
  *op = 16 + xmode(opcode, 'c', cs) + 2*sfao(st1);
  break;

case 0x05: // add
  *op = 20 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1) + 8*sfmo(st1) + 32*rnd(st1) + 160*movm(st1);
  break;

case 0x06: // add
  *op = 340 + xmode(opcode, 'c', cs) + 2*sfmo(st1) + 8*rnd(st1) + 40*movm(st1);
  break;

case 0x07: // add
  *op = 420 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x09: // sub
  *op = 428 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1);
  break;

case 0x0a: // sub
  *op = 436 + xmode(opcode, 'c', cs) + 2*sfao(st1);
  break;

case 0x0b: // sub
  *op = 440 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1) + 8*sfmo(st1) + 32*rnd(st1) + 160*movm(st1);
  break;

case 0x0c: // sub
  *op = 760 + xmode(opcode, 'c', cs) + 2*sfmo(st1) + 8*rnd(st1) + 40*movm(st1);
  break;

case 0x0d: // sub
  *op = 840 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x11: // lacd
  *op = 848 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1);
  break;

case 0x12: // lacc
  *op = 856 + xmode(opcode, 'c', cs);
  break;

case 0x14: // and
  *op = 858 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1);
  break;

case 0x15: // and
  *op = 866 + xmode(opcode, 'c', cs);
  break;

case 0x16: // and
  *op = 868 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*sfai(st1) + 8*dbp(st1);
  break;

case 0x17: // or
  *op = 884 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1);
  break;

case 0x18: // or
  *op = 892 + xmode(opcode, 'c', cs);
  break;

case 0x19: // or
  *op = 894 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*sfai(st1) + 8*dbp(st1);
  break;

case 0x21: // mpy
  *op = 910 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x22: // mpy
  *op = 918 + xmode(opcode, 'c', cs) + 2*sfao(st1);
  break;

case 0x24: // mac
  *op = 922 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1) + 8*sfma(st1);
  break;

case 0x25: // mac
  *op = 954 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1) + 8*sfma(st1);
  break;

case 0x26: // mac
  *op = 986 + xmode(opcode, 'c', cs) + 2*sfao(st1) + 4*sfma(st1);
  break;

case 0x28: // mpyu
  *op = 1002 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x29: // macu
  *op = 1010 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1) + 8*sfma(st1);
  break;

case 0x2a: // macu
  *op = 1042 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1) + 8*sfma(st1);
  break;

case 0x2e: // macs
  *op = 1074 + xmode(opcode, 'c', cs) + 2*sfao(st1) + 4*sfma(st1);
  break;

case 0x31: // lmhd
  *op = 1090 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x32: // lmld
  *op = 1094 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x33: // lmhc
  *op = 1098 + xmode(opcode, 'c', cs);
  break;

case 0x34: // sfml
  *op = 1100;
  break;

case 0x35: // sfmr
  *op = 1101;
  break;

case 0x38: // wre
  *op = 1102 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x39: // rde
  *op = 1110 + xmode(opcode, 'c', cs);
  break;

#endif

#ifdef CDEC2A
case 0x01: // sacc
  *op = 1112 + xmode(opcode, 'c', cs) + 2*sfao(st1);
  break;

case 0x02: // sacd
  *op = 1116 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1);
  break;

case 0x03: // smhd
  *op = 1124 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfmo(st1) + 16*rnd(st1) + 80*movm(st1);
  break;

case 0x05: // smhc
  *op = 1284 + xmode(opcode, 'c', cs) + 2*sfmo(st1) + 8*rnd(st1) + 40*movm(st1);
  break;

case 0x06: // slmh
  *op = 1364 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfmo(st1) + 16*movm(st1);
  break;

case 0x07: // slml
  *op = 1396 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfmo(st1) + 16*movm(st1);
  break;

case 0x08: // lcaa
  *op = 1428 + sfao(st1);
  break;

case 0x09: // lira
  *op = 1430 + sfao(st1);
  break;

case 0x0e: // ref
  *op = 1432;
  break;

case 0x0f: // srbd
  *op = 1433 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x10: // dis
  *op = 1437 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x11: // dis
  *op = 1441 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x12: // dis
  *op = 1445 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x13: // dis
  *op = 1449 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x20: // domh
  *op = 1453 + sfmo(st1) + 4*rnd(st1) + 20*movm(st1);
  break;

case 0x21: // domh
  *op = 1493 + sfmo(st1) + 4*rnd(st1) + 20*movm(st1);
  break;

case 0x22: // domh
  *op = 1533 + sfmo(st1) + 4*rnd(st1) + 20*movm(st1);
  break;

case 0x23: // domh
  *op = 1573 + sfmo(st1) + 4*rnd(st1) + 20*movm(st1);
  break;

case 0x31: // lpc
  *op = 1613 + xmode(opcode, 'c', cs);
  break;

case 0x3a: // rmov
  break;

case 0x3c: // raom
  break;

case 0x3d: // saom
  break;

case 0x40: // rmom
  break;

case 0x41: // smom
  break;

case 0x44: // ldpk
  break;

case 0x45: // ldpk
  break;

case 0x48: // scrm
  break;

case 0x49: // scrm
  break;

case 0x4a: // scrm
  break;

case 0x4b: // scrm
  break;

case 0x50: // sfao
  break;

case 0x51: // sfao
  break;

case 0x54: // sfai
  break;

case 0x55: // sfai
  break;

case 0x58: // sfma
  break;

case 0x59: // sfma
  break;

case 0x5a: // sfma
  break;

case 0x5b: // sfma
  break;

case 0x60: // sfmo
  break;

case 0x61: // sfmo
  break;

case 0x62: // sfmo
  break;

case 0x63: // sfmo
  break;

case 0x68: // rnd
  break;

case 0x69: // rnd
  break;

case 0x6a: // rnd
  break;

case 0x6b: // rnd
  break;

case 0x6c: // rnd
  break;

case 0x6d: // rnd
  break;

case 0x6e: // rnd
  break;

case 0x6f: // rnd
  break;

#endif

#ifdef CDEC2B
case 0x01: // sacc
  break;

case 0x02: // sacd
  break;

case 0x03: // smhd
  break;

case 0x05: // smhc
  break;

case 0x06: // slmh
  break;

case 0x07: // slml
  break;

case 0x08: // lcaa
  break;

case 0x09: // lira
  break;

case 0x0e: // ref
  break;

case 0x0f: // srbd
  break;

case 0x10: // dis
  break;

case 0x11: // dis
  break;

case 0x12: // dis
  break;

case 0x13: // dis
  break;

case 0x20: // domh
  break;

case 0x21: // domh
  break;

case 0x22: // domh
  break;

case 0x23: // domh
  break;

case 0x31: // lpc
  break;

case 0x3a: // rmov
  *op = 1615;
  break;

case 0x3c: // raom
  *op = 1616;
	/* Undocumented instruction, reset ALU saturation flag */
	st1 &= ~ST1_AOVM;
  break;

case 0x3d: // saom
  *op = 1617;
	/* Undocumented instruction, sets ALU saturation flag */
	st1 |= ST1_AOVM;
  break;

case 0x40: // rmom
  *op = 1618;
	st1 &= ~ST1_MOVM;
  break;

case 0x41: // smom
  *op = 1619;
	st1 |= ST1_MOVM;
  break;

case 0x44: // ldpk
  *op = 1620;
	st1 &= ~ST1_DBP;
  break;

case 0x45: // ldpk
  *op = 1621;
	st1 |= ST1_DBP;
  break;

case 0x48: // scrm
  *op = 1622;
	st1 = (st1 & ~ST1_CRM) | (0 << ST1_CRM_SHIFT);
  break;

case 0x49: // scrm
  *op = 1623;
	st1 = (st1 & ~ST1_CRM) | (1 << ST1_CRM_SHIFT);
  break;

case 0x4a: // scrm
  *op = 1624;
	st1 = (st1 & ~ST1_CRM) | (2 << ST1_CRM_SHIFT);
  break;

case 0x4b: // scrm
  *op = 1625;
	st1 = (st1 & ~ST1_CRM) | (3 << ST1_CRM_SHIFT);
  break;

case 0x50: // sfao
  *op = 1626;
	st1 &= ~ST1_SFAO;
  break;

case 0x51: // sfao
  *op = 1627;
	st1 |= ST1_SFAO;
  break;

case 0x54: // sfai
  *op = 1628;
	st1 &= ~ST1_SFAI;
  break;

case 0x55: // sfai
  *op = 1629;
	st1 |= ST1_SFAI;
  break;

case 0x58: // sfma
  *op = 1630;
	st1 = (st1 & ~ST1_SFMA) | (0 << ST1_SFMA_SHIFT);
  break;

case 0x59: // sfma
  *op = 1631;
	st1 = (st1 & ~ST1_SFMA) | (1 << ST1_SFMA_SHIFT);
  break;

case 0x5a: // sfma
  *op = 1632;
	st1 = (st1 & ~ST1_SFMA) | (2 << ST1_SFMA_SHIFT);
  break;

case 0x5b: // sfma
  *op = 1633;
	st1 = (st1 & ~ST1_SFMA) | (3 << ST1_SFMA_SHIFT);
  break;

case 0x60: // sfmo
  *op = 1634;
	st1 = (st1 & ~ST1_SFMO) | (0 << ST1_SFMO_SHIFT);
  break;

case 0x61: // sfmo
  *op = 1635;
	st1 = (st1 & ~ST1_SFMO) | (1 << ST1_SFMO_SHIFT);
  break;

case 0x62: // sfmo
  *op = 1636;
	st1 = (st1 & ~ST1_SFMO) | (2 << ST1_SFMO_SHIFT);
  break;

case 0x63: // sfmo
  *op = 1637;
	st1 = (st1 & ~ST1_SFMO) | (3 << ST1_SFMO_SHIFT);
  break;

case 0x68: // rnd
  *op = 1638;
	st1 = (st1 & ~ST1_RND) | (0 << ST1_RND_SHIFT);
  break;

case 0x69: // rnd
  *op = 1639;
	st1 = (st1 & ~ST1_RND) | (1 << ST1_RND_SHIFT);
  break;

case 0x6a: // rnd
  *op = 1640;
	st1 = (st1 & ~ST1_RND) | (2 << ST1_RND_SHIFT);
  break;

case 0x6b: // rnd
  *op = 1641;
	st1 = (st1 & ~ST1_RND) | (3 << ST1_RND_SHIFT);
  break;

case 0x6c: // rnd
  *op = 1642;
	st1 = (st1 & ~ST1_RND) | (4 << ST1_RND_SHIFT);
  break;

case 0x6d: // rnd
  *op = 1643;
	st1 = (st1 & ~ST1_RND) | (5 << ST1_RND_SHIFT);
  break;

case 0x6e: // rnd
  *op = 1644;
	st1 = (st1 & ~ST1_RND) | (6 << ST1_RND_SHIFT);
  break;

case 0x6f: // rnd
  *op = 1645;
	st1 = (st1 & ~ST1_RND) | (7 << ST1_RND_SHIFT);
  break;

#endif

#ifdef CDEC3
case 0x08: // idle
  *op = 1646;
  cs->branch = BR_IDLE;
  break;

case 0x10: // rptk
  *op = 1647;
  break;

case 0x18: // lcak
  *op = 1648;
  break;

case 0x20: // lirk
  *op = 1649;
  break;

case 0x40: // lcac
  *op = 1650;
  break;

case 0x48: // b
  *op = 1651;
  cs->branch = BR_UB;
  break;

case 0x50: // bgz
  *op = 1652;
  cs->branch = BR_CB;
  break;

case 0x58: // blz
  *op = 1653;
  cs->branch = BR_CB;
  break;

case 0x60: // bnz
  *op = 1654;
  cs->branch = BR_CB;
  break;

case 0x78: // bv
  *op = 1655;
  cs->branch = BR_CB;
  break;

#endif

#ifdef CINTRPINLINE
case 4: { // abs sfao=0
	if(((int32_t)aacc) < 0) {
	aacc = - aacc;
	if(((int32_t)aacc) < 0)
		st1 |= ST1_AOV;
	}
} break;
case 5: { // abs sfao=1
	aacc = (aacc << 7);
	if(((int32_t)aacc) < 0) {
	aacc = - aacc;
	if(((int32_t)aacc) < 0)
		st1 |= ST1_AOV;
	}
} break;
case 6: { // neg sfao=0
	r = -(int64_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 7: { // neg sfao=1
	r = -(int64_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 8: { // add dmode=0 dbp=0 sfao=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 9: { // add dmode=1 dbp=0 sfao=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 10: { // add dmode=0 dbp=1 sfao=0
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 11: { // add dmode=1 dbp=1 sfao=0
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 12: { // add dmode=0 dbp=0 sfao=1
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 13: { // add dmode=1 dbp=0 sfao=1
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 14: { // add dmode=0 dbp=1 sfao=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 15: { // add dmode=1 dbp=1 sfao=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 16: { // add cmode=0 sfao=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 17: { // add cmode=1 sfao=0
	r = (int64_t)(int32_t)get_cmem(ca) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 18: { // add cmode=0 sfao=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 19: { // add cmode=1 sfao=1
	r = (int64_t)(int32_t)get_cmem(ca) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 20: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 21: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 22: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 23: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 24: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 25: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 26: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 27: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 28: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 29: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 30: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 31: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 32: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 33: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 34: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 35: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 36: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 37: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 38: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 39: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 40: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 41: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 42: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 43: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 44: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 45: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 46: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 47: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 48: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 49: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 50: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 51: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 52: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 53: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 54: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 55: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 56: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 57: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 58: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 59: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 60: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 61: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 62: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 63: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 64: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 65: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 66: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 67: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 68: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 69: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 70: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 71: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 72: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 73: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 74: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 75: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 76: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 77: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 78: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 79: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 80: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 81: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 82: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 83: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 84: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 85: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 86: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 87: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 88: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 89: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 90: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 91: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 92: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 93: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 94: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 95: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 96: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 97: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 98: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 99: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 100: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 101: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 102: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 103: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 104: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 105: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 106: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 107: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 108: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 109: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 110: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 111: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 112: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 113: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 114: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 115: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 116: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 117: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 118: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 119: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 120: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 121: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 122: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 123: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 124: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 125: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 126: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 127: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 128: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 129: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 130: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 131: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 132: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 133: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 134: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 135: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 136: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 137: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 138: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 139: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 140: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 141: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 142: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 143: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 144: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 145: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 146: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 147: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 148: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 149: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 150: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 151: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 152: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 153: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 154: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 155: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 156: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 157: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 158: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 159: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 160: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 161: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 162: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 163: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 164: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 165: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 166: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 167: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 168: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 169: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 170: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 171: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 172: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 173: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 174: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 175: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 176: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 177: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 178: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 179: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 180: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 181: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 182: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 183: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 184: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 185: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 186: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 187: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 188: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 189: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 190: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 191: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 192: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 193: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 194: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 195: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 196: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 197: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 198: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 199: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 200: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 201: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 202: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 203: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 204: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 205: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 206: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 207: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 208: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 209: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 210: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 211: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 212: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 213: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 214: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 215: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 216: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 217: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 218: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 219: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 220: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 221: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 222: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 223: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 224: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 225: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 226: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 227: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 228: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 229: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 230: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 231: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 232: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 233: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 234: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 235: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 236: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 237: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 238: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 239: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 240: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 241: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 242: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 243: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 244: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 245: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 246: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 247: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 248: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 249: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 250: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 251: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 252: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 253: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 254: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 255: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 256: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 257: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 258: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 259: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 260: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 261: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 262: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 263: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 264: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 265: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 266: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 267: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 268: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 269: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 270: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 271: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 272: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 273: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 274: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 275: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 276: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 277: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 278: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 279: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 280: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 281: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 282: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 283: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 284: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 285: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 286: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 287: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 288: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 289: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 290: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 291: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 292: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 293: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 294: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 295: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 296: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 297: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 298: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 299: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 300: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 301: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 302: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 303: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 304: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 305: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 306: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 307: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 308: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 309: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 310: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 311: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 312: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 313: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 314: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 315: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 316: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 317: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 318: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 319: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 320: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 321: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 322: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 323: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 324: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 325: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 326: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 327: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 328: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 329: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 330: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 331: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 332: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 333: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 334: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 335: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 336: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 337: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 338: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 339: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 340: { // add cmode=0 sfmo=0 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 341: { // add cmode=1 sfmo=0 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 342: { // add cmode=0 sfmo=1 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 343: { // add cmode=1 sfmo=1 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 344: { // add cmode=0 sfmo=2 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 345: { // add cmode=1 sfmo=2 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 346: { // add cmode=0 sfmo=3 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 347: { // add cmode=1 sfmo=3 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 348: { // add cmode=0 sfmo=0 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 349: { // add cmode=1 sfmo=0 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 350: { // add cmode=0 sfmo=1 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 351: { // add cmode=1 sfmo=1 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 352: { // add cmode=0 sfmo=2 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 353: { // add cmode=1 sfmo=2 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 354: { // add cmode=0 sfmo=3 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 355: { // add cmode=1 sfmo=3 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 356: { // add cmode=0 sfmo=0 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 357: { // add cmode=1 sfmo=0 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 358: { // add cmode=0 sfmo=1 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 359: { // add cmode=1 sfmo=1 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 360: { // add cmode=0 sfmo=2 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 361: { // add cmode=1 sfmo=2 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 362: { // add cmode=0 sfmo=3 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 363: { // add cmode=1 sfmo=3 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 364: { // add cmode=0 sfmo=0 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 365: { // add cmode=1 sfmo=0 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 366: { // add cmode=0 sfmo=1 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 367: { // add cmode=1 sfmo=1 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 368: { // add cmode=0 sfmo=2 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 369: { // add cmode=1 sfmo=2 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 370: { // add cmode=0 sfmo=3 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 371: { // add cmode=1 sfmo=3 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 372: { // add cmode=0 sfmo=0 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 373: { // add cmode=1 sfmo=0 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 374: { // add cmode=0 sfmo=1 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 375: { // add cmode=1 sfmo=1 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 376: { // add cmode=0 sfmo=2 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 377: { // add cmode=1 sfmo=2 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 378: { // add cmode=0 sfmo=3 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 379: { // add cmode=1 sfmo=3 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 380: { // add cmode=0 sfmo=0 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 381: { // add cmode=1 sfmo=0 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 382: { // add cmode=0 sfmo=1 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 383: { // add cmode=1 sfmo=1 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 384: { // add cmode=0 sfmo=2 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 385: { // add cmode=1 sfmo=2 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 386: { // add cmode=0 sfmo=3 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 387: { // add cmode=1 sfmo=3 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 388: { // add cmode=0 sfmo=0 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 389: { // add cmode=1 sfmo=0 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 390: { // add cmode=0 sfmo=1 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 391: { // add cmode=1 sfmo=1 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 392: { // add cmode=0 sfmo=2 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 393: { // add cmode=1 sfmo=2 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 394: { // add cmode=0 sfmo=3 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 395: { // add cmode=1 sfmo=3 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 396: { // add cmode=0 sfmo=0 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 397: { // add cmode=1 sfmo=0 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 398: { // add cmode=0 sfmo=1 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 399: { // add cmode=1 sfmo=1 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 400: { // add cmode=0 sfmo=2 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 401: { // add cmode=1 sfmo=2 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 402: { // add cmode=0 sfmo=3 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 403: { // add cmode=1 sfmo=3 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 404: { // add cmode=0 sfmo=0 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 405: { // add cmode=1 sfmo=0 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 406: { // add cmode=0 sfmo=1 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 407: { // add cmode=1 sfmo=1 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 408: { // add cmode=0 sfmo=2 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 409: { // add cmode=1 sfmo=2 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 410: { // add cmode=0 sfmo=3 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 411: { // add cmode=1 sfmo=3 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 412: { // add cmode=0 sfmo=0 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 413: { // add cmode=1 sfmo=0 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 414: { // add cmode=0 sfmo=1 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 415: { // add cmode=1 sfmo=1 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 416: { // add cmode=0 sfmo=2 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 417: { // add cmode=1 sfmo=2 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 418: { // add cmode=0 sfmo=3 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 419: { // add cmode=1 sfmo=3 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 420: { // add cmode=0 dmode=0 dbp=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) + (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 421: { // add cmode=1 dmode=0 dbp=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) + (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 422: { // add cmode=0 dmode=1 dbp=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) + (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 423: { // add cmode=1 dmode=1 dbp=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) + (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 424: { // add cmode=0 dmode=0 dbp=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) + (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 425: { // add cmode=1 dmode=0 dbp=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) + (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 426: { // add cmode=0 dmode=1 dbp=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) + (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 427: { // add cmode=1 dmode=1 dbp=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) + (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 428: { // sub dmode=0 dbp=0 sfao=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 429: { // sub dmode=1 dbp=0 sfao=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 430: { // sub dmode=0 dbp=1 sfao=0
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 431: { // sub dmode=1 dbp=1 sfao=0
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 432: { // sub dmode=0 dbp=0 sfao=1
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 433: { // sub dmode=1 dbp=0 sfao=1
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 434: { // sub dmode=0 dbp=1 sfao=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 435: { // sub dmode=1 dbp=1 sfao=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 436: { // sub cmode=0 sfao=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 437: { // sub cmode=1 sfao=0
	r = (int64_t)(int32_t)get_cmem(ca) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 438: { // sub cmode=0 sfao=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 439: { // sub cmode=1 sfao=1
	r = (int64_t)(int32_t)get_cmem(ca) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 440: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 441: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 442: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 443: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 444: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 445: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 446: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 447: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 448: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 449: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 450: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 451: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 452: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 453: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 454: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 455: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 456: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 457: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 458: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 459: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 460: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 461: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 462: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 463: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 464: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 465: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 466: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 467: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 468: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 469: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 470: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 471: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 472: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 473: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 474: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 475: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 476: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 477: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 478: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 479: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 480: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 481: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 482: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 483: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 484: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 485: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 486: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 487: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 488: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 489: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 490: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 491: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 492: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 493: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 494: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 495: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 496: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 497: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 498: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 499: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 500: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 501: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 502: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 503: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 504: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 505: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 506: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 507: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 508: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 509: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 510: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 511: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 512: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 513: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 514: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 515: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 516: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 517: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 518: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 519: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 520: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 521: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 522: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 523: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 524: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 525: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 526: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 527: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 528: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 529: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 530: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 531: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 532: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 533: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 534: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 535: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 536: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 537: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 538: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 539: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 540: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 541: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 542: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 543: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 544: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 545: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 546: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 547: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 548: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 549: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 550: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 551: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 552: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 553: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 554: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 555: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 556: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 557: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 558: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 559: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 560: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 561: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 562: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 563: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 564: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 565: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 566: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 567: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 568: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 569: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 570: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 571: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 572: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 573: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 574: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 575: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 576: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 577: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 578: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 579: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 580: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 581: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 582: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 583: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 584: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 585: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 586: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 587: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 588: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 589: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 590: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 591: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 592: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 593: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 594: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 595: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 596: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 597: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 598: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 599: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 600: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 601: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 602: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 603: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 604: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 605: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 606: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 607: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 608: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 609: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 610: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 611: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 612: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 613: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 614: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 615: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 616: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 617: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 618: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 619: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 620: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 621: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 622: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 623: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 624: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 625: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 626: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 627: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 628: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 629: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 630: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 631: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 632: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 633: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 634: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 635: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 636: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 637: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 638: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 639: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 640: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 641: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 642: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 643: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 644: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 645: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 646: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 647: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 648: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 649: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 650: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 651: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 652: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 653: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 654: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 655: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 656: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 657: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 658: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 659: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 660: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 661: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 662: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 663: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 664: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 665: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 666: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 667: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 668: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 669: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 670: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 671: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 672: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 673: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 674: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 675: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 676: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 677: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 678: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 679: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 680: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 681: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 682: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 683: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 684: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 685: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 686: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 687: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 688: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 689: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 690: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 691: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 692: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 693: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 694: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 695: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 696: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 697: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 698: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 699: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 700: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 701: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 702: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 703: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 704: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 705: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 706: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 707: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 708: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 709: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 710: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 711: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 712: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 713: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 714: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 715: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 716: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 717: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 718: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 719: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 720: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 721: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 722: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 723: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 724: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 725: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 726: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 727: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 728: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 729: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 730: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 731: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 732: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 733: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 734: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 735: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 736: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 737: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 738: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 739: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 740: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 741: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 742: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 743: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 744: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 745: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 746: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 747: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 748: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 749: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 750: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 751: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 752: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 753: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 754: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 755: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 756: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 757: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 758: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 759: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 760: { // sub cmode=0 sfmo=0 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 761: { // sub cmode=1 sfmo=0 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 762: { // sub cmode=0 sfmo=1 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 763: { // sub cmode=1 sfmo=1 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 764: { // sub cmode=0 sfmo=2 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 765: { // sub cmode=1 sfmo=2 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 766: { // sub cmode=0 sfmo=3 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 767: { // sub cmode=1 sfmo=3 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 768: { // sub cmode=0 sfmo=0 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 769: { // sub cmode=1 sfmo=0 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 770: { // sub cmode=0 sfmo=1 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 771: { // sub cmode=1 sfmo=1 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 772: { // sub cmode=0 sfmo=2 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 773: { // sub cmode=1 sfmo=2 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 774: { // sub cmode=0 sfmo=3 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 775: { // sub cmode=1 sfmo=3 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 776: { // sub cmode=0 sfmo=0 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 777: { // sub cmode=1 sfmo=0 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 778: { // sub cmode=0 sfmo=1 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 779: { // sub cmode=1 sfmo=1 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 780: { // sub cmode=0 sfmo=2 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 781: { // sub cmode=1 sfmo=2 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 782: { // sub cmode=0 sfmo=3 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 783: { // sub cmode=1 sfmo=3 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 784: { // sub cmode=0 sfmo=0 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 785: { // sub cmode=1 sfmo=0 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 786: { // sub cmode=0 sfmo=1 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 787: { // sub cmode=1 sfmo=1 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 788: { // sub cmode=0 sfmo=2 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 789: { // sub cmode=1 sfmo=2 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 790: { // sub cmode=0 sfmo=3 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 791: { // sub cmode=1 sfmo=3 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 792: { // sub cmode=0 sfmo=0 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 793: { // sub cmode=1 sfmo=0 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 794: { // sub cmode=0 sfmo=1 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 795: { // sub cmode=1 sfmo=1 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 796: { // sub cmode=0 sfmo=2 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 797: { // sub cmode=1 sfmo=2 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 798: { // sub cmode=0 sfmo=3 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 799: { // sub cmode=1 sfmo=3 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 800: { // sub cmode=0 sfmo=0 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 801: { // sub cmode=1 sfmo=0 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 802: { // sub cmode=0 sfmo=1 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 803: { // sub cmode=1 sfmo=1 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 804: { // sub cmode=0 sfmo=2 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 805: { // sub cmode=1 sfmo=2 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 806: { // sub cmode=0 sfmo=3 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 807: { // sub cmode=1 sfmo=3 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 808: { // sub cmode=0 sfmo=0 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 809: { // sub cmode=1 sfmo=0 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 810: { // sub cmode=0 sfmo=1 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 811: { // sub cmode=1 sfmo=1 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 812: { // sub cmode=0 sfmo=2 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 813: { // sub cmode=1 sfmo=2 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 814: { // sub cmode=0 sfmo=3 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 815: { // sub cmode=1 sfmo=3 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 816: { // sub cmode=0 sfmo=0 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 817: { // sub cmode=1 sfmo=0 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 818: { // sub cmode=0 sfmo=1 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 819: { // sub cmode=1 sfmo=1 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 820: { // sub cmode=0 sfmo=2 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 821: { // sub cmode=1 sfmo=2 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 822: { // sub cmode=0 sfmo=3 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 823: { // sub cmode=1 sfmo=3 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 824: { // sub cmode=0 sfmo=0 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 825: { // sub cmode=1 sfmo=0 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 826: { // sub cmode=0 sfmo=1 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 827: { // sub cmode=1 sfmo=1 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 828: { // sub cmode=0 sfmo=2 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 829: { // sub cmode=1 sfmo=2 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 830: { // sub cmode=0 sfmo=3 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 831: { // sub cmode=1 sfmo=3 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 832: { // sub cmode=0 sfmo=0 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 833: { // sub cmode=1 sfmo=0 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 834: { // sub cmode=0 sfmo=1 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 835: { // sub cmode=1 sfmo=1 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 836: { // sub cmode=0 sfmo=2 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 837: { // sub cmode=1 sfmo=2 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 838: { // sub cmode=0 sfmo=3 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 839: { // sub cmode=1 sfmo=3 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 840: { // sub cmode=0 dmode=0 dbp=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) - (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 841: { // sub cmode=1 dmode=0 dbp=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) - (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 842: { // sub cmode=0 dmode=1 dbp=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) - (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 843: { // sub cmode=1 dmode=1 dbp=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) - (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 844: { // sub cmode=0 dmode=0 dbp=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) - (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 845: { // sub cmode=1 dmode=0 dbp=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) - (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 846: { // sub cmode=0 dmode=1 dbp=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) - (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 847: { // sub cmode=1 dmode=1 dbp=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) - (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} break;
case 848: { // lacd dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = d;
} break;
case 849: { // lacd dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = d;
} break;
case 850: { // lacd dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = d;
} break;
case 851: { // lacd dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = d;
} break;
case 852: { // lacd dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = d;
} break;
case 853: { // lacd dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = d;
} break;
case 854: { // lacd dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = d;
} break;
case 855: { // lacd dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = d;
} break;
case 856: { // lacc cmode=0
	aacc = get_cmem(i->param);
} break;
case 857: { // lacc cmode=1
	aacc = get_cmem(ca);
} break;
case 858: { // and dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc &= d;
} break;
case 859: { // and dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc &= d;
} break;
case 860: { // and dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc &= d;
} break;
case 861: { // and dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc &= d;
} break;
case 862: { // and dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc &= d;
} break;
case 863: { // and dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc &= d;
} break;
case 864: { // and dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc &= d;
} break;
case 865: { // and dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc &= d;
} break;
case 866: { // and cmode=0
	aacc &= get_cmem(i->param);
} break;
case 867: { // and cmode=1
	aacc &= get_cmem(ca);
} break;
case 868: { // and cmode=0 dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) & d;
} break;
case 869: { // and cmode=1 dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) & d;
} break;
case 870: { // and cmode=0 dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) & d;
} break;
case 871: { // and cmode=1 dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) & d;
} break;
case 872: { // and cmode=0 dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
} break;
case 873: { // and cmode=1 dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
} break;
case 874: { // and cmode=0 dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
} break;
case 875: { // and cmode=1 dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
} break;
case 876: { // and cmode=0 dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) & d;
} break;
case 877: { // and cmode=1 dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) & d;
} break;
case 878: { // and cmode=0 dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) & d;
} break;
case 879: { // and cmode=1 dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) & d;
} break;
case 880: { // and cmode=0 dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
} break;
case 881: { // and cmode=1 dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
} break;
case 882: { // and cmode=0 dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
} break;
case 883: { // and cmode=1 dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
} break;
case 884: { // or dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc |= d;
} break;
case 885: { // or dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc |= d;
} break;
case 886: { // or dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc |= d;
} break;
case 887: { // or dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc |= d;
} break;
case 888: { // or dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc |= d;
} break;
case 889: { // or dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc |= d;
} break;
case 890: { // or dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc |= d;
} break;
case 891: { // or dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc |= d;
} break;
case 892: { // or cmode=0
	aacc |= get_cmem(i->param);
} break;
case 893: { // or cmode=1
	aacc |= get_cmem(ca);
} break;
case 894: { // or cmode=0 dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) | d;
} break;
case 895: { // or cmode=1 dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) | d;
} break;
case 896: { // or cmode=0 dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) | d;
} break;
case 897: { // or cmode=1 dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) | d;
} break;
case 898: { // or cmode=0 dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
} break;
case 899: { // or cmode=1 dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
} break;
case 900: { // or cmode=0 dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
} break;
case 901: { // or cmode=1 dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
} break;
case 902: { // or cmode=0 dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) | d;
} break;
case 903: { // or cmode=1 dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) | d;
} break;
case 904: { // or cmode=0 dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) | d;
} break;
case 905: { // or cmode=1 dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) | d;
} break;
case 906: { // or cmode=0 dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
} break;
case 907: { // or cmode=1 dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
} break;
case 908: { // or cmode=0 dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
} break;
case 909: { // or cmode=1 dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
} break;
case 910: { // mpy cmode=0 dmode=0 dbp=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} break;
case 911: { // mpy cmode=1 dmode=0 dbp=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} break;
case 912: { // mpy cmode=0 dmode=1 dbp=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} break;
case 913: { // mpy cmode=1 dmode=1 dbp=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} break;
case 914: { // mpy cmode=0 dmode=0 dbp=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} break;
case 915: { // mpy cmode=1 dmode=0 dbp=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} break;
case 916: { // mpy cmode=0 dmode=1 dbp=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} break;
case 917: { // mpy cmode=1 dmode=1 dbp=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} break;
case 918: { // mpy cmode=0 sfao=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = r >> 15;
} break;
case 919: { // mpy cmode=1 sfao=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = r >> 15;
} break;
case 920: { // mpy cmode=0 sfao=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = r >> 15;
} break;
case 921: { // mpy cmode=1 sfao=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = r >> 15;
} break;
case 922: { // mac cmode=0 dmode=0 dbp=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 923: { // mac cmode=1 dmode=0 dbp=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 924: { // mac cmode=0 dmode=1 dbp=0 sfma=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 925: { // mac cmode=1 dmode=1 dbp=0 sfma=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 926: { // mac cmode=0 dmode=0 dbp=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 927: { // mac cmode=1 dmode=0 dbp=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 928: { // mac cmode=0 dmode=1 dbp=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 929: { // mac cmode=1 dmode=1 dbp=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 930: { // mac cmode=0 dmode=0 dbp=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 931: { // mac cmode=1 dmode=0 dbp=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 932: { // mac cmode=0 dmode=1 dbp=0 sfma=1
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 933: { // mac cmode=1 dmode=1 dbp=0 sfma=1
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 934: { // mac cmode=0 dmode=0 dbp=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 935: { // mac cmode=1 dmode=0 dbp=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 936: { // mac cmode=0 dmode=1 dbp=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 937: { // mac cmode=1 dmode=1 dbp=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 938: { // mac cmode=0 dmode=0 dbp=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 939: { // mac cmode=1 dmode=0 dbp=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 940: { // mac cmode=0 dmode=1 dbp=0 sfma=2
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 941: { // mac cmode=1 dmode=1 dbp=0 sfma=2
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 942: { // mac cmode=0 dmode=0 dbp=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 943: { // mac cmode=1 dmode=0 dbp=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 944: { // mac cmode=0 dmode=1 dbp=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 945: { // mac cmode=1 dmode=1 dbp=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 946: { // mac cmode=0 dmode=0 dbp=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 947: { // mac cmode=1 dmode=0 dbp=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 948: { // mac cmode=0 dmode=1 dbp=0 sfma=3
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 949: { // mac cmode=1 dmode=1 dbp=0 sfma=3
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 950: { // mac cmode=0 dmode=0 dbp=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 951: { // mac cmode=1 dmode=0 dbp=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 952: { // mac cmode=0 dmode=1 dbp=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 953: { // mac cmode=1 dmode=1 dbp=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 954: { // mac dmode=0 dbp=0 sfao=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 955: { // mac dmode=1 dbp=0 sfao=0 sfma=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 956: { // mac dmode=0 dbp=1 sfao=0 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 957: { // mac dmode=1 dbp=1 sfao=0 sfma=0
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 958: { // mac dmode=0 dbp=0 sfao=1 sfma=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 959: { // mac dmode=1 dbp=0 sfao=1 sfma=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 960: { // mac dmode=0 dbp=1 sfao=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 961: { // mac dmode=1 dbp=1 sfao=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} break;
case 962: { // mac dmode=0 dbp=0 sfao=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 963: { // mac dmode=1 dbp=0 sfao=0 sfma=1
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 964: { // mac dmode=0 dbp=1 sfao=0 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 965: { // mac dmode=1 dbp=1 sfao=0 sfma=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 966: { // mac dmode=0 dbp=0 sfao=1 sfma=1
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 967: { // mac dmode=1 dbp=0 sfao=1 sfma=1
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 968: { // mac dmode=0 dbp=1 sfao=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 969: { // mac dmode=1 dbp=1 sfao=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 970: { // mac dmode=0 dbp=0 sfao=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 971: { // mac dmode=1 dbp=0 sfao=0 sfma=2
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 972: { // mac dmode=0 dbp=1 sfao=0 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 973: { // mac dmode=1 dbp=1 sfao=0 sfma=2
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 974: { // mac dmode=0 dbp=0 sfao=1 sfma=2
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 975: { // mac dmode=1 dbp=0 sfao=1 sfma=2
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 976: { // mac dmode=0 dbp=1 sfao=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 977: { // mac dmode=1 dbp=1 sfao=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 978: { // mac dmode=0 dbp=0 sfao=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 979: { // mac dmode=1 dbp=0 sfao=0 sfma=3
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 980: { // mac dmode=0 dbp=1 sfao=0 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 981: { // mac dmode=1 dbp=1 sfao=0 sfma=3
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 982: { // mac dmode=0 dbp=0 sfao=1 sfma=3
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 983: { // mac dmode=1 dbp=0 sfao=1 sfma=3
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 984: { // mac dmode=0 dbp=1 sfao=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 985: { // mac dmode=1 dbp=1 sfao=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 986: { // mac cmode=0 sfao=0 sfma=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = macc + (r >> 15);
} break;
case 987: { // mac cmode=1 sfao=0 sfma=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = macc + (r >> 15);
} break;
case 988: { // mac cmode=0 sfao=1 sfma=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = macc + (r >> 15);
} break;
case 989: { // mac cmode=1 sfao=1 sfma=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = macc + (r >> 15);
} break;
case 990: { // mac cmode=0 sfao=0 sfma=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 2) + (r >> 15);
} break;
case 991: { // mac cmode=1 sfao=0 sfma=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 2) + (r >> 15);
} break;
case 992: { // mac cmode=0 sfao=1 sfma=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 2) + (r >> 15);
} break;
case 993: { // mac cmode=1 sfao=1 sfma=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 2) + (r >> 15);
} break;
case 994: { // mac cmode=0 sfao=0 sfma=2
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 4) + (r >> 15);
} break;
case 995: { // mac cmode=1 sfao=0 sfma=2
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 4) + (r >> 15);
} break;
case 996: { // mac cmode=0 sfao=1 sfma=2
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 4) + (r >> 15);
} break;
case 997: { // mac cmode=1 sfao=1 sfma=2
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 4) + (r >> 15);
} break;
case 998: { // mac cmode=0 sfao=0 sfma=3
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc >> 16) + (r >> 15);
} break;
case 999: { // mac cmode=1 sfao=0 sfma=3
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc >> 16) + (r >> 15);
} break;
case 1000: { // mac cmode=0 sfao=1 sfma=3
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc >> 16) + (r >> 15);
} break;
case 1001: { // mac cmode=1 sfao=1 sfma=3
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc >> 16) + (r >> 15);
} break;
case 1002: { // mpyu cmode=0 dmode=0 dbp=0
	creg = c = get_cmem(i->param);
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} break;
case 1003: { // mpyu cmode=1 dmode=0 dbp=0
	creg = c = get_cmem(ca);
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} break;
case 1004: { // mpyu cmode=0 dmode=1 dbp=0
	creg = c = get_cmem(i->param);
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} break;
case 1005: { // mpyu cmode=1 dmode=1 dbp=0
	creg = c = get_cmem(ca);
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} break;
case 1006: { // mpyu cmode=0 dmode=0 dbp=1
	creg = c = get_cmem(i->param);
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} break;
case 1007: { // mpyu cmode=1 dmode=0 dbp=1
	creg = c = get_cmem(ca);
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} break;
case 1008: { // mpyu cmode=0 dmode=1 dbp=1
	creg = c = get_cmem(i->param);
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} break;
case 1009: { // mpyu cmode=1 dmode=1 dbp=1
	creg = c = get_cmem(ca);
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} break;
case 1010: { // macu cmode=0 dmode=0 dbp=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1011: { // macu cmode=1 dmode=0 dbp=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1012: { // macu cmode=0 dmode=1 dbp=0 sfma=0
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1013: { // macu cmode=1 dmode=1 dbp=0 sfma=0
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1014: { // macu cmode=0 dmode=0 dbp=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1015: { // macu cmode=1 dmode=0 dbp=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1016: { // macu cmode=0 dmode=1 dbp=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1017: { // macu cmode=1 dmode=1 dbp=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1018: { // macu cmode=0 dmode=0 dbp=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1019: { // macu cmode=1 dmode=0 dbp=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1020: { // macu cmode=0 dmode=1 dbp=0 sfma=1
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1021: { // macu cmode=1 dmode=1 dbp=0 sfma=1
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1022: { // macu cmode=0 dmode=0 dbp=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1023: { // macu cmode=1 dmode=0 dbp=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1024: { // macu cmode=0 dmode=1 dbp=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1025: { // macu cmode=1 dmode=1 dbp=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1026: { // macu cmode=0 dmode=0 dbp=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1027: { // macu cmode=1 dmode=0 dbp=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1028: { // macu cmode=0 dmode=1 dbp=0 sfma=2
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1029: { // macu cmode=1 dmode=1 dbp=0 sfma=2
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1030: { // macu cmode=0 dmode=0 dbp=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1031: { // macu cmode=1 dmode=0 dbp=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1032: { // macu cmode=0 dmode=1 dbp=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1033: { // macu cmode=1 dmode=1 dbp=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1034: { // macu cmode=0 dmode=0 dbp=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1035: { // macu cmode=1 dmode=0 dbp=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1036: { // macu cmode=0 dmode=1 dbp=0 sfma=3
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1037: { // macu cmode=1 dmode=1 dbp=0 sfma=3
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1038: { // macu cmode=0 dmode=0 dbp=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1039: { // macu cmode=1 dmode=0 dbp=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1040: { // macu cmode=0 dmode=1 dbp=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1041: { // macu cmode=1 dmode=1 dbp=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1042: { // macu dmode=0 dbp=0 sfao=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1043: { // macu dmode=1 dbp=0 sfao=0 sfma=0
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1044: { // macu dmode=0 dbp=1 sfao=0 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1045: { // macu dmode=1 dbp=1 sfao=0 sfma=0
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1046: { // macu dmode=0 dbp=0 sfao=1 sfma=0
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1047: { // macu dmode=1 dbp=0 sfao=1 sfma=0
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1048: { // macu dmode=0 dbp=1 sfao=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1049: { // macu dmode=1 dbp=1 sfao=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} break;
case 1050: { // macu dmode=0 dbp=0 sfao=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1051: { // macu dmode=1 dbp=0 sfao=0 sfma=1
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1052: { // macu dmode=0 dbp=1 sfao=0 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1053: { // macu dmode=1 dbp=1 sfao=0 sfma=1
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1054: { // macu dmode=0 dbp=0 sfao=1 sfma=1
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1055: { // macu dmode=1 dbp=0 sfao=1 sfma=1
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1056: { // macu dmode=0 dbp=1 sfao=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1057: { // macu dmode=1 dbp=1 sfao=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} break;
case 1058: { // macu dmode=0 dbp=0 sfao=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1059: { // macu dmode=1 dbp=0 sfao=0 sfma=2
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1060: { // macu dmode=0 dbp=1 sfao=0 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1061: { // macu dmode=1 dbp=1 sfao=0 sfma=2
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1062: { // macu dmode=0 dbp=0 sfao=1 sfma=2
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1063: { // macu dmode=1 dbp=0 sfao=1 sfma=2
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1064: { // macu dmode=0 dbp=1 sfao=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1065: { // macu dmode=1 dbp=1 sfao=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} break;
case 1066: { // macu dmode=0 dbp=0 sfao=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1067: { // macu dmode=1 dbp=0 sfao=0 sfma=3
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1068: { // macu dmode=0 dbp=1 sfao=0 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1069: { // macu dmode=1 dbp=1 sfao=0 sfma=3
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1070: { // macu dmode=0 dbp=0 sfao=1 sfma=3
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1071: { // macu dmode=1 dbp=0 sfao=1 sfma=3
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1072: { // macu dmode=0 dbp=1 sfao=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1073: { // macu dmode=1 dbp=1 sfao=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} break;
case 1074: { // macs cmode=0 sfao=0 sfma=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = macc + (r >> 14);
} break;
case 1075: { // macs cmode=1 sfao=0 sfma=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = macc + (r >> 14);
} break;
case 1076: { // macs cmode=0 sfao=1 sfma=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = macc + (r >> 14);
} break;
case 1077: { // macs cmode=1 sfao=1 sfma=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = macc + (r >> 14);
} break;
case 1078: { // macs cmode=0 sfao=0 sfma=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 2) + (r >> 14);
} break;
case 1079: { // macs cmode=1 sfao=0 sfma=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 2) + (r >> 14);
} break;
case 1080: { // macs cmode=0 sfao=1 sfma=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 2) + (r >> 14);
} break;
case 1081: { // macs cmode=1 sfao=1 sfma=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 2) + (r >> 14);
} break;
case 1082: { // macs cmode=0 sfao=0 sfma=2
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 4) + (r >> 14);
} break;
case 1083: { // macs cmode=1 sfao=0 sfma=2
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 4) + (r >> 14);
} break;
case 1084: { // macs cmode=0 sfao=1 sfma=2
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 4) + (r >> 14);
} break;
case 1085: { // macs cmode=1 sfao=1 sfma=2
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 4) + (r >> 14);
} break;
case 1086: { // macs cmode=0 sfao=0 sfma=3
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc >> 16) + (r >> 14);
} break;
case 1087: { // macs cmode=1 sfao=0 sfma=3
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc >> 16) + (r >> 14);
} break;
case 1088: { // macs cmode=0 sfao=1 sfma=3
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc >> 16) + (r >> 14);
} break;
case 1089: { // macs cmode=1 sfao=1 sfma=3
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc >> 16) + (r >> 14);
} break;
case 1090: { // lmhd dmode=0 dbp=0
	macc_write = macc = ((int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8)) << 16;
} break;
case 1091: { // lmhd dmode=1 dbp=0
	macc_write = macc = ((int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8)) << 16;
} break;
case 1092: { // lmhd dmode=0 dbp=1
	macc_write = macc = ((int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8)) << 16;
} break;
case 1093: { // lmhd dmode=1 dbp=1
	macc_write = macc = ((int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8)) << 16;
} break;
case 1094: { // lmld dmode=0 dbp=0
	macc_write = macc = (macc & ~0xffffffULL) | dmem0[(i->param + ba0) & 0xff];
} break;
case 1095: { // lmld dmode=1 dbp=0
	macc_write = macc = (macc & ~0xffffffULL) | dmem0[(id + ba0) & 0xff];
} break;
case 1096: { // lmld dmode=0 dbp=1
	macc_write = macc = (macc & ~0xffffffULL) | dmem1[(i->param + ba1) & 0x1f];
} break;
case 1097: { // lmld dmode=1 dbp=1
	macc_write = macc = (macc & ~0xffffffULL) | dmem1[(id + ba1) & 0x1f];
} break;
case 1098: { // lmhc cmode=0
	macc_write = macc = ((int64_t)(int32_t)get_cmem(i->param)) << 16;
} break;
case 1099: { // lmhc cmode=1
	macc_write = macc = ((int64_t)(int32_t)get_cmem(ca)) << 16;
} break;
case 1100: { // sfml 
	macc = (macc & 0x8000000000000ULL) | ((macc << 1) & 0x7ffffffffffffULL);
} break;
case 1101: { // sfmr 
	macc = (macc & 0x8000000000000ULL) | ((macc >> 1) & 0x7ffffffffffffULL);
} break;
case 1102: { // wre cmode=0 dmode=0 dbp=0
	if(sti & (S_READ|S_WRITE))
	break;
	xwr = dmem0[(i->param + ba0) & 0xff];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
} break;
case 1103: { // wre cmode=1 dmode=0 dbp=0
	if(sti & (S_READ|S_WRITE))
	break;
	xwr = dmem0[(i->param + ba0) & 0xff];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
} break;
case 1104: { // wre cmode=0 dmode=1 dbp=0
	if(sti & (S_READ|S_WRITE))
	break;
	xwr = dmem0[(id + ba0) & 0xff];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
} break;
case 1105: { // wre cmode=1 dmode=1 dbp=0
	if(sti & (S_READ|S_WRITE))
	break;
	xwr = dmem0[(id + ba0) & 0xff];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
} break;
case 1106: { // wre cmode=0 dmode=0 dbp=1
	if(sti & (S_READ|S_WRITE))
	break;
	xwr = dmem1[(i->param + ba1) & 0x1f];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
} break;
case 1107: { // wre cmode=1 dmode=0 dbp=1
	if(sti & (S_READ|S_WRITE))
	break;
	xwr = dmem1[(i->param + ba1) & 0x1f];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
} break;
case 1108: { // wre cmode=0 dmode=1 dbp=1
	if(sti & (S_READ|S_WRITE))
	break;
	xwr = dmem1[(id + ba1) & 0x1f];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
} break;
case 1109: { // wre cmode=1 dmode=1 dbp=1
	if(sti & (S_READ|S_WRITE))
	break;
	xwr = dmem1[(id + ba1) & 0x1f];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
} break;
case 1110: { // rde cmode=0
	if(sti & (S_READ|S_WRITE))
	break;
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_READ;
} break;
case 1111: { // rde cmode=1
	if(sti & (S_READ|S_WRITE))
	break;
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_READ;
} break;
case 1112: { // sacc cmode=0 sfao=0
	cmem[i->param] = aacc;
} break;
case 1113: { // sacc cmode=1 sfao=0
	cmem[ca] = aacc;
} break;
case 1114: { // sacc cmode=0 sfao=1
	cmem[i->param] = (aacc << 7);
} break;
case 1115: { // sacc cmode=1 sfao=1
	cmem[ca] = (aacc << 7);
} break;
case 1116: { // sacd dmode=0 dbp=0 sfao=0
	dmem0[(i->param + ba0) & 0xff] = aacc >> 8;
} break;
case 1117: { // sacd dmode=1 dbp=0 sfao=0
	dmem0[(id + ba0) & 0xff] = aacc >> 8;
} break;
case 1118: { // sacd dmode=0 dbp=1 sfao=0
	dmem1[(i->param + ba1) & 0x1f] = aacc >> 8;
} break;
case 1119: { // sacd dmode=1 dbp=1 sfao=0
	dmem1[(id + ba1) & 0x1f] = aacc >> 8;
} break;
case 1120: { // sacd dmode=0 dbp=0 sfao=1
	dmem0[(i->param + ba0) & 0xff] = (aacc << 7) >> 8;
} break;
case 1121: { // sacd dmode=1 dbp=0 sfao=1
	dmem0[(id + ba0) & 0xff] = (aacc << 7) >> 8;
} break;
case 1122: { // sacd dmode=0 dbp=1 sfao=1
	dmem1[(i->param + ba1) & 0x1f] = (aacc << 7) >> 8;
} break;
case 1123: { // sacd dmode=1 dbp=1 sfao=1
	dmem1[(id + ba1) & 0x1f] = (aacc << 7) >> 8;
} break;
case 1124: { // smhd dmode=0 dbp=0 sfmo=0 rnd=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1125: { // smhd dmode=1 dbp=0 sfmo=0 rnd=0 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1126: { // smhd dmode=0 dbp=1 sfmo=0 rnd=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1127: { // smhd dmode=1 dbp=1 sfmo=0 rnd=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1128: { // smhd dmode=0 dbp=0 sfmo=1 rnd=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1129: { // smhd dmode=1 dbp=0 sfmo=1 rnd=0 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1130: { // smhd dmode=0 dbp=1 sfmo=1 rnd=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1131: { // smhd dmode=1 dbp=1 sfmo=1 rnd=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1132: { // smhd dmode=0 dbp=0 sfmo=2 rnd=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1133: { // smhd dmode=1 dbp=0 sfmo=2 rnd=0 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1134: { // smhd dmode=0 dbp=1 sfmo=2 rnd=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1135: { // smhd dmode=1 dbp=1 sfmo=2 rnd=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1136: { // smhd dmode=0 dbp=0 sfmo=3 rnd=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1137: { // smhd dmode=1 dbp=0 sfmo=3 rnd=0 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1138: { // smhd dmode=0 dbp=1 sfmo=3 rnd=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1139: { // smhd dmode=1 dbp=1 sfmo=3 rnd=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1140: { // smhd dmode=0 dbp=0 sfmo=0 rnd=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1141: { // smhd dmode=1 dbp=0 sfmo=0 rnd=1 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1142: { // smhd dmode=0 dbp=1 sfmo=0 rnd=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1143: { // smhd dmode=1 dbp=1 sfmo=0 rnd=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1144: { // smhd dmode=0 dbp=0 sfmo=1 rnd=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1145: { // smhd dmode=1 dbp=0 sfmo=1 rnd=1 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1146: { // smhd dmode=0 dbp=1 sfmo=1 rnd=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1147: { // smhd dmode=1 dbp=1 sfmo=1 rnd=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1148: { // smhd dmode=0 dbp=0 sfmo=2 rnd=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1149: { // smhd dmode=1 dbp=0 sfmo=2 rnd=1 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1150: { // smhd dmode=0 dbp=1 sfmo=2 rnd=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1151: { // smhd dmode=1 dbp=1 sfmo=2 rnd=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1152: { // smhd dmode=0 dbp=0 sfmo=3 rnd=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1153: { // smhd dmode=1 dbp=0 sfmo=3 rnd=1 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1154: { // smhd dmode=0 dbp=1 sfmo=3 rnd=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1155: { // smhd dmode=1 dbp=1 sfmo=3 rnd=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1156: { // smhd dmode=0 dbp=0 sfmo=0 rnd=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1157: { // smhd dmode=1 dbp=0 sfmo=0 rnd=2 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1158: { // smhd dmode=0 dbp=1 sfmo=0 rnd=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1159: { // smhd dmode=1 dbp=1 sfmo=0 rnd=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1160: { // smhd dmode=0 dbp=0 sfmo=1 rnd=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1161: { // smhd dmode=1 dbp=0 sfmo=1 rnd=2 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1162: { // smhd dmode=0 dbp=1 sfmo=1 rnd=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1163: { // smhd dmode=1 dbp=1 sfmo=1 rnd=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1164: { // smhd dmode=0 dbp=0 sfmo=2 rnd=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1165: { // smhd dmode=1 dbp=0 sfmo=2 rnd=2 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1166: { // smhd dmode=0 dbp=1 sfmo=2 rnd=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1167: { // smhd dmode=1 dbp=1 sfmo=2 rnd=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1168: { // smhd dmode=0 dbp=0 sfmo=3 rnd=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1169: { // smhd dmode=1 dbp=0 sfmo=3 rnd=2 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1170: { // smhd dmode=0 dbp=1 sfmo=3 rnd=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1171: { // smhd dmode=1 dbp=1 sfmo=3 rnd=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1172: { // smhd dmode=0 dbp=0 sfmo=0 rnd=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1173: { // smhd dmode=1 dbp=0 sfmo=0 rnd=3 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1174: { // smhd dmode=0 dbp=1 sfmo=0 rnd=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1175: { // smhd dmode=1 dbp=1 sfmo=0 rnd=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1176: { // smhd dmode=0 dbp=0 sfmo=1 rnd=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1177: { // smhd dmode=1 dbp=0 sfmo=1 rnd=3 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1178: { // smhd dmode=0 dbp=1 sfmo=1 rnd=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1179: { // smhd dmode=1 dbp=1 sfmo=1 rnd=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1180: { // smhd dmode=0 dbp=0 sfmo=2 rnd=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1181: { // smhd dmode=1 dbp=0 sfmo=2 rnd=3 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1182: { // smhd dmode=0 dbp=1 sfmo=2 rnd=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1183: { // smhd dmode=1 dbp=1 sfmo=2 rnd=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1184: { // smhd dmode=0 dbp=0 sfmo=3 rnd=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1185: { // smhd dmode=1 dbp=0 sfmo=3 rnd=3 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1186: { // smhd dmode=0 dbp=1 sfmo=3 rnd=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1187: { // smhd dmode=1 dbp=1 sfmo=3 rnd=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1188: { // smhd dmode=0 dbp=0 sfmo=0 rnd=4 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1189: { // smhd dmode=1 dbp=0 sfmo=0 rnd=4 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1190: { // smhd dmode=0 dbp=1 sfmo=0 rnd=4 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1191: { // smhd dmode=1 dbp=1 sfmo=0 rnd=4 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1192: { // smhd dmode=0 dbp=0 sfmo=1 rnd=4 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1193: { // smhd dmode=1 dbp=0 sfmo=1 rnd=4 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1194: { // smhd dmode=0 dbp=1 sfmo=1 rnd=4 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1195: { // smhd dmode=1 dbp=1 sfmo=1 rnd=4 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1196: { // smhd dmode=0 dbp=0 sfmo=2 rnd=4 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1197: { // smhd dmode=1 dbp=0 sfmo=2 rnd=4 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1198: { // smhd dmode=0 dbp=1 sfmo=2 rnd=4 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1199: { // smhd dmode=1 dbp=1 sfmo=2 rnd=4 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1200: { // smhd dmode=0 dbp=0 sfmo=3 rnd=4 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1201: { // smhd dmode=1 dbp=0 sfmo=3 rnd=4 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1202: { // smhd dmode=0 dbp=1 sfmo=3 rnd=4 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1203: { // smhd dmode=1 dbp=1 sfmo=3 rnd=4 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1204: { // smhd dmode=0 dbp=0 sfmo=0 rnd=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1205: { // smhd dmode=1 dbp=0 sfmo=0 rnd=0 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1206: { // smhd dmode=0 dbp=1 sfmo=0 rnd=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1207: { // smhd dmode=1 dbp=1 sfmo=0 rnd=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1208: { // smhd dmode=0 dbp=0 sfmo=1 rnd=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1209: { // smhd dmode=1 dbp=0 sfmo=1 rnd=0 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1210: { // smhd dmode=0 dbp=1 sfmo=1 rnd=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1211: { // smhd dmode=1 dbp=1 sfmo=1 rnd=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1212: { // smhd dmode=0 dbp=0 sfmo=2 rnd=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1213: { // smhd dmode=1 dbp=0 sfmo=2 rnd=0 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1214: { // smhd dmode=0 dbp=1 sfmo=2 rnd=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1215: { // smhd dmode=1 dbp=1 sfmo=2 rnd=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1216: { // smhd dmode=0 dbp=0 sfmo=3 rnd=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1217: { // smhd dmode=1 dbp=0 sfmo=3 rnd=0 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1218: { // smhd dmode=0 dbp=1 sfmo=3 rnd=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1219: { // smhd dmode=1 dbp=1 sfmo=3 rnd=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1220: { // smhd dmode=0 dbp=0 sfmo=0 rnd=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1221: { // smhd dmode=1 dbp=0 sfmo=0 rnd=1 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1222: { // smhd dmode=0 dbp=1 sfmo=0 rnd=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1223: { // smhd dmode=1 dbp=1 sfmo=0 rnd=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1224: { // smhd dmode=0 dbp=0 sfmo=1 rnd=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1225: { // smhd dmode=1 dbp=0 sfmo=1 rnd=1 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1226: { // smhd dmode=0 dbp=1 sfmo=1 rnd=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1227: { // smhd dmode=1 dbp=1 sfmo=1 rnd=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1228: { // smhd dmode=0 dbp=0 sfmo=2 rnd=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1229: { // smhd dmode=1 dbp=0 sfmo=2 rnd=1 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1230: { // smhd dmode=0 dbp=1 sfmo=2 rnd=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1231: { // smhd dmode=1 dbp=1 sfmo=2 rnd=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1232: { // smhd dmode=0 dbp=0 sfmo=3 rnd=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1233: { // smhd dmode=1 dbp=0 sfmo=3 rnd=1 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1234: { // smhd dmode=0 dbp=1 sfmo=3 rnd=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1235: { // smhd dmode=1 dbp=1 sfmo=3 rnd=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1236: { // smhd dmode=0 dbp=0 sfmo=0 rnd=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1237: { // smhd dmode=1 dbp=0 sfmo=0 rnd=2 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1238: { // smhd dmode=0 dbp=1 sfmo=0 rnd=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1239: { // smhd dmode=1 dbp=1 sfmo=0 rnd=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1240: { // smhd dmode=0 dbp=0 sfmo=1 rnd=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1241: { // smhd dmode=1 dbp=0 sfmo=1 rnd=2 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1242: { // smhd dmode=0 dbp=1 sfmo=1 rnd=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1243: { // smhd dmode=1 dbp=1 sfmo=1 rnd=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1244: { // smhd dmode=0 dbp=0 sfmo=2 rnd=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1245: { // smhd dmode=1 dbp=0 sfmo=2 rnd=2 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1246: { // smhd dmode=0 dbp=1 sfmo=2 rnd=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1247: { // smhd dmode=1 dbp=1 sfmo=2 rnd=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1248: { // smhd dmode=0 dbp=0 sfmo=3 rnd=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1249: { // smhd dmode=1 dbp=0 sfmo=3 rnd=2 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1250: { // smhd dmode=0 dbp=1 sfmo=3 rnd=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1251: { // smhd dmode=1 dbp=1 sfmo=3 rnd=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1252: { // smhd dmode=0 dbp=0 sfmo=0 rnd=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1253: { // smhd dmode=1 dbp=0 sfmo=0 rnd=3 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1254: { // smhd dmode=0 dbp=1 sfmo=0 rnd=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1255: { // smhd dmode=1 dbp=1 sfmo=0 rnd=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1256: { // smhd dmode=0 dbp=0 sfmo=1 rnd=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1257: { // smhd dmode=1 dbp=0 sfmo=1 rnd=3 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1258: { // smhd dmode=0 dbp=1 sfmo=1 rnd=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1259: { // smhd dmode=1 dbp=1 sfmo=1 rnd=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1260: { // smhd dmode=0 dbp=0 sfmo=2 rnd=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1261: { // smhd dmode=1 dbp=0 sfmo=2 rnd=3 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1262: { // smhd dmode=0 dbp=1 sfmo=2 rnd=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1263: { // smhd dmode=1 dbp=1 sfmo=2 rnd=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1264: { // smhd dmode=0 dbp=0 sfmo=3 rnd=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1265: { // smhd dmode=1 dbp=0 sfmo=3 rnd=3 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1266: { // smhd dmode=0 dbp=1 sfmo=3 rnd=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1267: { // smhd dmode=1 dbp=1 sfmo=3 rnd=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1268: { // smhd dmode=0 dbp=0 sfmo=0 rnd=4 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1269: { // smhd dmode=1 dbp=0 sfmo=0 rnd=4 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1270: { // smhd dmode=0 dbp=1 sfmo=0 rnd=4 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1271: { // smhd dmode=1 dbp=1 sfmo=0 rnd=4 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1272: { // smhd dmode=0 dbp=0 sfmo=1 rnd=4 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1273: { // smhd dmode=1 dbp=0 sfmo=1 rnd=4 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1274: { // smhd dmode=0 dbp=1 sfmo=1 rnd=4 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1275: { // smhd dmode=1 dbp=1 sfmo=1 rnd=4 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1276: { // smhd dmode=0 dbp=0 sfmo=2 rnd=4 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1277: { // smhd dmode=1 dbp=0 sfmo=2 rnd=4 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1278: { // smhd dmode=0 dbp=1 sfmo=2 rnd=4 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1279: { // smhd dmode=1 dbp=1 sfmo=2 rnd=4 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1280: { // smhd dmode=0 dbp=0 sfmo=3 rnd=4 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1281: { // smhd dmode=1 dbp=0 sfmo=3 rnd=4 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1282: { // smhd dmode=0 dbp=1 sfmo=3 rnd=4 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1283: { // smhd dmode=1 dbp=1 sfmo=3 rnd=4 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1284: { // smhc cmode=0 sfmo=0 rnd=0 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1285: { // smhc cmode=1 sfmo=0 rnd=0 movm=0
	cmem[ca] = macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1286: { // smhc cmode=0 sfmo=1 rnd=0 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1287: { // smhc cmode=1 sfmo=1 rnd=0 movm=0
	cmem[ca] = macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1288: { // smhc cmode=0 sfmo=2 rnd=0 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1289: { // smhc cmode=1 sfmo=2 rnd=0 movm=0
	cmem[ca] = macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1290: { // smhc cmode=0 sfmo=3 rnd=0 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1291: { // smhc cmode=1 sfmo=3 rnd=0 movm=0
	cmem[ca] = macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1292: { // smhc cmode=0 sfmo=0 rnd=1 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1293: { // smhc cmode=1 sfmo=0 rnd=1 movm=0
	cmem[ca] = macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1294: { // smhc cmode=0 sfmo=1 rnd=1 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1295: { // smhc cmode=1 sfmo=1 rnd=1 movm=0
	cmem[ca] = macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1296: { // smhc cmode=0 sfmo=2 rnd=1 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1297: { // smhc cmode=1 sfmo=2 rnd=1 movm=0
	cmem[ca] = macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1298: { // smhc cmode=0 sfmo=3 rnd=1 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1299: { // smhc cmode=1 sfmo=3 rnd=1 movm=0
	cmem[ca] = macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1300: { // smhc cmode=0 sfmo=0 rnd=2 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1301: { // smhc cmode=1 sfmo=0 rnd=2 movm=0
	cmem[ca] = macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1302: { // smhc cmode=0 sfmo=1 rnd=2 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1303: { // smhc cmode=1 sfmo=1 rnd=2 movm=0
	cmem[ca] = macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1304: { // smhc cmode=0 sfmo=2 rnd=2 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1305: { // smhc cmode=1 sfmo=2 rnd=2 movm=0
	cmem[ca] = macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1306: { // smhc cmode=0 sfmo=3 rnd=2 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1307: { // smhc cmode=1 sfmo=3 rnd=2 movm=0
	cmem[ca] = macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1308: { // smhc cmode=0 sfmo=0 rnd=3 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1309: { // smhc cmode=1 sfmo=0 rnd=3 movm=0
	cmem[ca] = macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1310: { // smhc cmode=0 sfmo=1 rnd=3 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1311: { // smhc cmode=1 sfmo=1 rnd=3 movm=0
	cmem[ca] = macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1312: { // smhc cmode=0 sfmo=2 rnd=3 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1313: { // smhc cmode=1 sfmo=2 rnd=3 movm=0
	cmem[ca] = macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1314: { // smhc cmode=0 sfmo=3 rnd=3 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1315: { // smhc cmode=1 sfmo=3 rnd=3 movm=0
	cmem[ca] = macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1316: { // smhc cmode=0 sfmo=0 rnd=4 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1317: { // smhc cmode=1 sfmo=0 rnd=4 movm=0
	cmem[ca] = macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1318: { // smhc cmode=0 sfmo=1 rnd=4 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1319: { // smhc cmode=1 sfmo=1 rnd=4 movm=0
	cmem[ca] = macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1320: { // smhc cmode=0 sfmo=2 rnd=4 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1321: { // smhc cmode=1 sfmo=2 rnd=4 movm=0
	cmem[ca] = macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1322: { // smhc cmode=0 sfmo=3 rnd=4 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1323: { // smhc cmode=1 sfmo=3 rnd=4 movm=0
	cmem[ca] = macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1324: { // smhc cmode=0 sfmo=0 rnd=0 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1325: { // smhc cmode=1 sfmo=0 rnd=0 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1326: { // smhc cmode=0 sfmo=1 rnd=0 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1327: { // smhc cmode=1 sfmo=1 rnd=0 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1328: { // smhc cmode=0 sfmo=2 rnd=0 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1329: { // smhc cmode=1 sfmo=2 rnd=0 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1330: { // smhc cmode=0 sfmo=3 rnd=0 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1331: { // smhc cmode=1 sfmo=3 rnd=0 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} break;
case 1332: { // smhc cmode=0 sfmo=0 rnd=1 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1333: { // smhc cmode=1 sfmo=0 rnd=1 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1334: { // smhc cmode=0 sfmo=1 rnd=1 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1335: { // smhc cmode=1 sfmo=1 rnd=1 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1336: { // smhc cmode=0 sfmo=2 rnd=1 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1337: { // smhc cmode=1 sfmo=2 rnd=1 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1338: { // smhc cmode=0 sfmo=3 rnd=1 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1339: { // smhc cmode=1 sfmo=3 rnd=1 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} break;
case 1340: { // smhc cmode=0 sfmo=0 rnd=2 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1341: { // smhc cmode=1 sfmo=0 rnd=2 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1342: { // smhc cmode=0 sfmo=1 rnd=2 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1343: { // smhc cmode=1 sfmo=1 rnd=2 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1344: { // smhc cmode=0 sfmo=2 rnd=2 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1345: { // smhc cmode=1 sfmo=2 rnd=2 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1346: { // smhc cmode=0 sfmo=3 rnd=2 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1347: { // smhc cmode=1 sfmo=3 rnd=2 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} break;
case 1348: { // smhc cmode=0 sfmo=0 rnd=3 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1349: { // smhc cmode=1 sfmo=0 rnd=3 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1350: { // smhc cmode=0 sfmo=1 rnd=3 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1351: { // smhc cmode=1 sfmo=1 rnd=3 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1352: { // smhc cmode=0 sfmo=2 rnd=3 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1353: { // smhc cmode=1 sfmo=2 rnd=3 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1354: { // smhc cmode=0 sfmo=3 rnd=3 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1355: { // smhc cmode=1 sfmo=3 rnd=3 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} break;
case 1356: { // smhc cmode=0 sfmo=0 rnd=4 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1357: { // smhc cmode=1 sfmo=0 rnd=4 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1358: { // smhc cmode=0 sfmo=1 rnd=4 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1359: { // smhc cmode=1 sfmo=1 rnd=4 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1360: { // smhc cmode=0 sfmo=2 rnd=4 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1361: { // smhc cmode=1 sfmo=2 rnd=4 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1362: { // smhc cmode=0 sfmo=3 rnd=4 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1363: { // smhc cmode=1 sfmo=3 rnd=4 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} break;
case 1364: { // slmh dmode=0 dbp=0 sfmo=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0() >> 24) & 0xffff00;
} break;
case 1365: { // slmh dmode=1 dbp=0 sfmo=0 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0() >> 24) & 0xffff00;
} break;
case 1366: { // slmh dmode=0 dbp=1 sfmo=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0() >> 24) & 0xffff00;
} break;
case 1367: { // slmh dmode=1 dbp=1 sfmo=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0() >> 24) & 0xffff00;
} break;
case 1368: { // slmh dmode=0 dbp=0 sfmo=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1() >> 24) & 0xffff00;
} break;
case 1369: { // slmh dmode=1 dbp=0 sfmo=1 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1() >> 24) & 0xffff00;
} break;
case 1370: { // slmh dmode=0 dbp=1 sfmo=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1() >> 24) & 0xffff00;
} break;
case 1371: { // slmh dmode=1 dbp=1 sfmo=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1() >> 24) & 0xffff00;
} break;
case 1372: { // slmh dmode=0 dbp=0 sfmo=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2() >> 24) & 0xffff00;
} break;
case 1373: { // slmh dmode=1 dbp=0 sfmo=2 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2() >> 24) & 0xffff00;
} break;
case 1374: { // slmh dmode=0 dbp=1 sfmo=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2() >> 24) & 0xffff00;
} break;
case 1375: { // slmh dmode=1 dbp=1 sfmo=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2() >> 24) & 0xffff00;
} break;
case 1376: { // slmh dmode=0 dbp=0 sfmo=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3() >> 24) & 0xffff00;
} break;
case 1377: { // slmh dmode=1 dbp=0 sfmo=3 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3() >> 24) & 0xffff00;
} break;
case 1378: { // slmh dmode=0 dbp=1 sfmo=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3() >> 24) & 0xffff00;
} break;
case 1379: { // slmh dmode=1 dbp=1 sfmo=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3() >> 24) & 0xffff00;
} break;
case 1380: { // slmh dmode=0 dbp=0 sfmo=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0s() >> 24) & 0xffff00;
} break;
case 1381: { // slmh dmode=1 dbp=0 sfmo=0 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0s() >> 24) & 0xffff00;
} break;
case 1382: { // slmh dmode=0 dbp=1 sfmo=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0s() >> 24) & 0xffff00;
} break;
case 1383: { // slmh dmode=1 dbp=1 sfmo=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0s() >> 24) & 0xffff00;
} break;
case 1384: { // slmh dmode=0 dbp=0 sfmo=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1s() >> 24) & 0xffff00;
} break;
case 1385: { // slmh dmode=1 dbp=0 sfmo=1 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1s() >> 24) & 0xffff00;
} break;
case 1386: { // slmh dmode=0 dbp=1 sfmo=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1s() >> 24) & 0xffff00;
} break;
case 1387: { // slmh dmode=1 dbp=1 sfmo=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1s() >> 24) & 0xffff00;
} break;
case 1388: { // slmh dmode=0 dbp=0 sfmo=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2s() >> 24) & 0xffff00;
} break;
case 1389: { // slmh dmode=1 dbp=0 sfmo=2 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2s() >> 24) & 0xffff00;
} break;
case 1390: { // slmh dmode=0 dbp=1 sfmo=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2s() >> 24) & 0xffff00;
} break;
case 1391: { // slmh dmode=1 dbp=1 sfmo=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2s() >> 24) & 0xffff00;
} break;
case 1392: { // slmh dmode=0 dbp=0 sfmo=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3s() >> 24) & 0xffff00;
} break;
case 1393: { // slmh dmode=1 dbp=0 sfmo=3 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3s() >> 24) & 0xffff00;
} break;
case 1394: { // slmh dmode=0 dbp=1 sfmo=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3s() >> 24) & 0xffff00;
} break;
case 1395: { // slmh dmode=1 dbp=1 sfmo=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3s() >> 24) & 0xffff00;
} break;
case 1396: { // slml dmode=0 dbp=0 sfmo=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0() >> 8) & 0xffffff;
} break;
case 1397: { // slml dmode=1 dbp=0 sfmo=0 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0() >> 8) & 0xffffff;
} break;
case 1398: { // slml dmode=0 dbp=1 sfmo=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0() >> 8) & 0xffffff;
} break;
case 1399: { // slml dmode=1 dbp=1 sfmo=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0() >> 8) & 0xffffff;
} break;
case 1400: { // slml dmode=0 dbp=0 sfmo=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1() >> 8) & 0xffffff;
} break;
case 1401: { // slml dmode=1 dbp=0 sfmo=1 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1() >> 8) & 0xffffff;
} break;
case 1402: { // slml dmode=0 dbp=1 sfmo=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1() >> 8) & 0xffffff;
} break;
case 1403: { // slml dmode=1 dbp=1 sfmo=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1() >> 8) & 0xffffff;
} break;
case 1404: { // slml dmode=0 dbp=0 sfmo=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2() >> 8) & 0xffffff;
} break;
case 1405: { // slml dmode=1 dbp=0 sfmo=2 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2() >> 8) & 0xffffff;
} break;
case 1406: { // slml dmode=0 dbp=1 sfmo=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2() >> 8) & 0xffffff;
} break;
case 1407: { // slml dmode=1 dbp=1 sfmo=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2() >> 8) & 0xffffff;
} break;
case 1408: { // slml dmode=0 dbp=0 sfmo=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3() >> 8) & 0xffffff;
} break;
case 1409: { // slml dmode=1 dbp=0 sfmo=3 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3() >> 8) & 0xffffff;
} break;
case 1410: { // slml dmode=0 dbp=1 sfmo=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3() >> 8) & 0xffffff;
} break;
case 1411: { // slml dmode=1 dbp=1 sfmo=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3() >> 8) & 0xffffff;
} break;
case 1412: { // slml dmode=0 dbp=0 sfmo=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0s() >> 8) & 0xffffff;
} break;
case 1413: { // slml dmode=1 dbp=0 sfmo=0 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0s() >> 8) & 0xffffff;
} break;
case 1414: { // slml dmode=0 dbp=1 sfmo=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0s() >> 8) & 0xffffff;
} break;
case 1415: { // slml dmode=1 dbp=1 sfmo=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0s() >> 8) & 0xffffff;
} break;
case 1416: { // slml dmode=0 dbp=0 sfmo=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1s() >> 8) & 0xffffff;
} break;
case 1417: { // slml dmode=1 dbp=0 sfmo=1 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1s() >> 8) & 0xffffff;
} break;
case 1418: { // slml dmode=0 dbp=1 sfmo=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1s() >> 8) & 0xffffff;
} break;
case 1419: { // slml dmode=1 dbp=1 sfmo=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1s() >> 8) & 0xffffff;
} break;
case 1420: { // slml dmode=0 dbp=0 sfmo=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2s() >> 8) & 0xffffff;
} break;
case 1421: { // slml dmode=1 dbp=0 sfmo=2 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2s() >> 8) & 0xffffff;
} break;
case 1422: { // slml dmode=0 dbp=1 sfmo=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2s() >> 8) & 0xffffff;
} break;
case 1423: { // slml dmode=1 dbp=1 sfmo=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2s() >> 8) & 0xffffff;
} break;
case 1424: { // slml dmode=0 dbp=0 sfmo=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3s() >> 8) & 0xffffff;
} break;
case 1425: { // slml dmode=1 dbp=0 sfmo=3 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3s() >> 8) & 0xffffff;
} break;
case 1426: { // slml dmode=0 dbp=1 sfmo=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3s() >> 8) & 0xffffff;
} break;
case 1427: { // slml dmode=1 dbp=1 sfmo=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3s() >> 8) & 0xffffff;
} break;
case 1428: { // lcaa sfao=0
	ca = aacc >> 24;
} break;
case 1429: { // lcaa sfao=1
	ca = (aacc << 7) >> 24;
} break;
case 1430: { // lira sfao=0
	id = aacc >> 24;
} break;
case 1431: { // lira sfao=1
	id = (aacc << 7) >> 24;
} break;
case 1432: { // ref 
	/* nothing to do */
} break;
case 1433: { // srbd dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = xrd;
} break;
case 1434: { // srbd dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = xrd;
} break;
case 1435: { // srbd dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = xrd;
} break;
case 1436: { // srbd dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = xrd;
} break;
case 1437: { // dis dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = si[0];
} break;
case 1438: { // dis dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = si[0];
} break;
case 1439: { // dis dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = si[0];
} break;
case 1440: { // dis dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = si[0];
} break;
case 1441: { // dis dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = si[1];
} break;
case 1442: { // dis dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = si[1];
} break;
case 1443: { // dis dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = si[1];
} break;
case 1444: { // dis dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = si[1];
} break;
case 1445: { // dis dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = si[2];
} break;
case 1446: { // dis dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = si[2];
} break;
case 1447: { // dis dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = si[2];
} break;
case 1448: { // dis dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = si[2];
} break;
case 1449: { // dis dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = si[3];
} break;
case 1450: { // dis dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = si[3];
} break;
case 1451: { // dis dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = si[3];
} break;
case 1452: { // dis dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = si[3];
} break;
case 1453: { // domh sfmo=0 rnd=0 movm=0
	so[0] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1454: { // domh sfmo=1 rnd=0 movm=0
	so[0] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1455: { // domh sfmo=2 rnd=0 movm=0
	so[0] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1456: { // domh sfmo=3 rnd=0 movm=0
	so[0] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1457: { // domh sfmo=0 rnd=1 movm=0
	so[0] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1458: { // domh sfmo=1 rnd=1 movm=0
	so[0] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1459: { // domh sfmo=2 rnd=1 movm=0
	so[0] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1460: { // domh sfmo=3 rnd=1 movm=0
	so[0] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1461: { // domh sfmo=0 rnd=2 movm=0
	so[0] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1462: { // domh sfmo=1 rnd=2 movm=0
	so[0] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1463: { // domh sfmo=2 rnd=2 movm=0
	so[0] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1464: { // domh sfmo=3 rnd=2 movm=0
	so[0] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1465: { // domh sfmo=0 rnd=3 movm=0
	so[0] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1466: { // domh sfmo=1 rnd=3 movm=0
	so[0] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1467: { // domh sfmo=2 rnd=3 movm=0
	so[0] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1468: { // domh sfmo=3 rnd=3 movm=0
	so[0] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1469: { // domh sfmo=0 rnd=4 movm=0
	so[0] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1470: { // domh sfmo=1 rnd=4 movm=0
	so[0] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1471: { // domh sfmo=2 rnd=4 movm=0
	so[0] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1472: { // domh sfmo=3 rnd=4 movm=0
	so[0] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1473: { // domh sfmo=0 rnd=0 movm=1
	so[0] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1474: { // domh sfmo=1 rnd=0 movm=1
	so[0] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1475: { // domh sfmo=2 rnd=0 movm=1
	so[0] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1476: { // domh sfmo=3 rnd=0 movm=1
	so[0] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1477: { // domh sfmo=0 rnd=1 movm=1
	so[0] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1478: { // domh sfmo=1 rnd=1 movm=1
	so[0] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1479: { // domh sfmo=2 rnd=1 movm=1
	so[0] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1480: { // domh sfmo=3 rnd=1 movm=1
	so[0] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1481: { // domh sfmo=0 rnd=2 movm=1
	so[0] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1482: { // domh sfmo=1 rnd=2 movm=1
	so[0] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1483: { // domh sfmo=2 rnd=2 movm=1
	so[0] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1484: { // domh sfmo=3 rnd=2 movm=1
	so[0] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1485: { // domh sfmo=0 rnd=3 movm=1
	so[0] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1486: { // domh sfmo=1 rnd=3 movm=1
	so[0] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1487: { // domh sfmo=2 rnd=3 movm=1
	so[0] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1488: { // domh sfmo=3 rnd=3 movm=1
	so[0] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1489: { // domh sfmo=0 rnd=4 movm=1
	so[0] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1490: { // domh sfmo=1 rnd=4 movm=1
	so[0] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1491: { // domh sfmo=2 rnd=4 movm=1
	so[0] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1492: { // domh sfmo=3 rnd=4 movm=1
	so[0] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1493: { // domh sfmo=0 rnd=0 movm=0
	so[1] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1494: { // domh sfmo=1 rnd=0 movm=0
	so[1] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1495: { // domh sfmo=2 rnd=0 movm=0
	so[1] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1496: { // domh sfmo=3 rnd=0 movm=0
	so[1] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1497: { // domh sfmo=0 rnd=1 movm=0
	so[1] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1498: { // domh sfmo=1 rnd=1 movm=0
	so[1] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1499: { // domh sfmo=2 rnd=1 movm=0
	so[1] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1500: { // domh sfmo=3 rnd=1 movm=0
	so[1] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1501: { // domh sfmo=0 rnd=2 movm=0
	so[1] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1502: { // domh sfmo=1 rnd=2 movm=0
	so[1] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1503: { // domh sfmo=2 rnd=2 movm=0
	so[1] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1504: { // domh sfmo=3 rnd=2 movm=0
	so[1] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1505: { // domh sfmo=0 rnd=3 movm=0
	so[1] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1506: { // domh sfmo=1 rnd=3 movm=0
	so[1] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1507: { // domh sfmo=2 rnd=3 movm=0
	so[1] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1508: { // domh sfmo=3 rnd=3 movm=0
	so[1] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1509: { // domh sfmo=0 rnd=4 movm=0
	so[1] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1510: { // domh sfmo=1 rnd=4 movm=0
	so[1] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1511: { // domh sfmo=2 rnd=4 movm=0
	so[1] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1512: { // domh sfmo=3 rnd=4 movm=0
	so[1] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1513: { // domh sfmo=0 rnd=0 movm=1
	so[1] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1514: { // domh sfmo=1 rnd=0 movm=1
	so[1] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1515: { // domh sfmo=2 rnd=0 movm=1
	so[1] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1516: { // domh sfmo=3 rnd=0 movm=1
	so[1] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1517: { // domh sfmo=0 rnd=1 movm=1
	so[1] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1518: { // domh sfmo=1 rnd=1 movm=1
	so[1] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1519: { // domh sfmo=2 rnd=1 movm=1
	so[1] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1520: { // domh sfmo=3 rnd=1 movm=1
	so[1] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1521: { // domh sfmo=0 rnd=2 movm=1
	so[1] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1522: { // domh sfmo=1 rnd=2 movm=1
	so[1] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1523: { // domh sfmo=2 rnd=2 movm=1
	so[1] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1524: { // domh sfmo=3 rnd=2 movm=1
	so[1] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1525: { // domh sfmo=0 rnd=3 movm=1
	so[1] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1526: { // domh sfmo=1 rnd=3 movm=1
	so[1] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1527: { // domh sfmo=2 rnd=3 movm=1
	so[1] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1528: { // domh sfmo=3 rnd=3 movm=1
	so[1] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1529: { // domh sfmo=0 rnd=4 movm=1
	so[1] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1530: { // domh sfmo=1 rnd=4 movm=1
	so[1] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1531: { // domh sfmo=2 rnd=4 movm=1
	so[1] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1532: { // domh sfmo=3 rnd=4 movm=1
	so[1] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1533: { // domh sfmo=0 rnd=0 movm=0
	so[2] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1534: { // domh sfmo=1 rnd=0 movm=0
	so[2] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1535: { // domh sfmo=2 rnd=0 movm=0
	so[2] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1536: { // domh sfmo=3 rnd=0 movm=0
	so[2] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1537: { // domh sfmo=0 rnd=1 movm=0
	so[2] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1538: { // domh sfmo=1 rnd=1 movm=0
	so[2] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1539: { // domh sfmo=2 rnd=1 movm=0
	so[2] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1540: { // domh sfmo=3 rnd=1 movm=0
	so[2] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1541: { // domh sfmo=0 rnd=2 movm=0
	so[2] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1542: { // domh sfmo=1 rnd=2 movm=0
	so[2] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1543: { // domh sfmo=2 rnd=2 movm=0
	so[2] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1544: { // domh sfmo=3 rnd=2 movm=0
	so[2] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1545: { // domh sfmo=0 rnd=3 movm=0
	so[2] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1546: { // domh sfmo=1 rnd=3 movm=0
	so[2] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1547: { // domh sfmo=2 rnd=3 movm=0
	so[2] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1548: { // domh sfmo=3 rnd=3 movm=0
	so[2] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1549: { // domh sfmo=0 rnd=4 movm=0
	so[2] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1550: { // domh sfmo=1 rnd=4 movm=0
	so[2] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1551: { // domh sfmo=2 rnd=4 movm=0
	so[2] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1552: { // domh sfmo=3 rnd=4 movm=0
	so[2] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1553: { // domh sfmo=0 rnd=0 movm=1
	so[2] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1554: { // domh sfmo=1 rnd=0 movm=1
	so[2] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1555: { // domh sfmo=2 rnd=0 movm=1
	so[2] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1556: { // domh sfmo=3 rnd=0 movm=1
	so[2] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1557: { // domh sfmo=0 rnd=1 movm=1
	so[2] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1558: { // domh sfmo=1 rnd=1 movm=1
	so[2] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1559: { // domh sfmo=2 rnd=1 movm=1
	so[2] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1560: { // domh sfmo=3 rnd=1 movm=1
	so[2] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1561: { // domh sfmo=0 rnd=2 movm=1
	so[2] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1562: { // domh sfmo=1 rnd=2 movm=1
	so[2] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1563: { // domh sfmo=2 rnd=2 movm=1
	so[2] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1564: { // domh sfmo=3 rnd=2 movm=1
	so[2] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1565: { // domh sfmo=0 rnd=3 movm=1
	so[2] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1566: { // domh sfmo=1 rnd=3 movm=1
	so[2] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1567: { // domh sfmo=2 rnd=3 movm=1
	so[2] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1568: { // domh sfmo=3 rnd=3 movm=1
	so[2] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1569: { // domh sfmo=0 rnd=4 movm=1
	so[2] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1570: { // domh sfmo=1 rnd=4 movm=1
	so[2] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1571: { // domh sfmo=2 rnd=4 movm=1
	so[2] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1572: { // domh sfmo=3 rnd=4 movm=1
	so[2] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1573: { // domh sfmo=0 rnd=0 movm=0
	so[3] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1574: { // domh sfmo=1 rnd=0 movm=0
	so[3] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1575: { // domh sfmo=2 rnd=0 movm=0
	so[3] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1576: { // domh sfmo=3 rnd=0 movm=0
	so[3] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1577: { // domh sfmo=0 rnd=1 movm=0
	so[3] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1578: { // domh sfmo=1 rnd=1 movm=0
	so[3] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1579: { // domh sfmo=2 rnd=1 movm=0
	so[3] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1580: { // domh sfmo=3 rnd=1 movm=0
	so[3] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1581: { // domh sfmo=0 rnd=2 movm=0
	so[3] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1582: { // domh sfmo=1 rnd=2 movm=0
	so[3] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1583: { // domh sfmo=2 rnd=2 movm=0
	so[3] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1584: { // domh sfmo=3 rnd=2 movm=0
	so[3] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1585: { // domh sfmo=0 rnd=3 movm=0
	so[3] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1586: { // domh sfmo=1 rnd=3 movm=0
	so[3] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1587: { // domh sfmo=2 rnd=3 movm=0
	so[3] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1588: { // domh sfmo=3 rnd=3 movm=0
	so[3] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1589: { // domh sfmo=0 rnd=4 movm=0
	so[3] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1590: { // domh sfmo=1 rnd=4 movm=0
	so[3] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1591: { // domh sfmo=2 rnd=4 movm=0
	so[3] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1592: { // domh sfmo=3 rnd=4 movm=0
	so[3] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1593: { // domh sfmo=0 rnd=0 movm=1
	so[3] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1594: { // domh sfmo=1 rnd=0 movm=1
	so[3] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1595: { // domh sfmo=2 rnd=0 movm=1
	so[3] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1596: { // domh sfmo=3 rnd=0 movm=1
	so[3] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} break;
case 1597: { // domh sfmo=0 rnd=1 movm=1
	so[3] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1598: { // domh sfmo=1 rnd=1 movm=1
	so[3] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1599: { // domh sfmo=2 rnd=1 movm=1
	so[3] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1600: { // domh sfmo=3 rnd=1 movm=1
	so[3] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} break;
case 1601: { // domh sfmo=0 rnd=2 movm=1
	so[3] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1602: { // domh sfmo=1 rnd=2 movm=1
	so[3] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1603: { // domh sfmo=2 rnd=2 movm=1
	so[3] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1604: { // domh sfmo=3 rnd=2 movm=1
	so[3] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} break;
case 1605: { // domh sfmo=0 rnd=3 movm=1
	so[3] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1606: { // domh sfmo=1 rnd=3 movm=1
	so[3] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1607: { // domh sfmo=2 rnd=3 movm=1
	so[3] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1608: { // domh sfmo=3 rnd=3 movm=1
	so[3] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} break;
case 1609: { // domh sfmo=0 rnd=4 movm=1
	so[3] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1610: { // domh sfmo=1 rnd=4 movm=1
	so[3] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1611: { // domh sfmo=2 rnd=4 movm=1
	so[3] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1612: { // domh sfmo=3 rnd=4 movm=1
	so[3] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} break;
case 1613: { // lpc cmode=0
	if(sti & S_HOST)
	break;
	c = get_cmem(i->param);
	host[0] = c >> 24;
	host[1] = c >> 16;
	host[2] = c >> 8;
	host[3] = c;
	hidx = 0;
	sti |= S_HOST;
	update_dready();
} break;
case 1614: { // lpc cmode=1
	if(sti & S_HOST)
	break;
	c = get_cmem(ca);
	host[0] = c >> 24;
	host[1] = c >> 16;
	host[2] = c >> 8;
	host[3] = c;
	hidx = 0;
	sti |= S_HOST;
	update_dready();
} break;
case 1615: { // rmov 
	st1 &= ~ST1_MOV;
} break;
case 1616: { // raom 
	/* Undocumented instruction, reset ALU saturation flag */
	st1 &= ~ST1_AOVM;
} break;
case 1617: { // saom 
	/* Undocumented instruction, sets ALU saturation flag */
	st1 |= ST1_AOVM;
} break;
case 1618: { // rmom 
	st1 &= ~ST1_MOVM;
} break;
case 1619: { // smom 
	st1 |= ST1_MOVM;
} break;
case 1620: { // ldpk 
	st1 &= ~ST1_DBP;
} break;
case 1621: { // ldpk 
	st1 |= ST1_DBP;
} break;
case 1622: { // scrm 
	st1 = (st1 & ~ST1_CRM) | (0 << ST1_CRM_SHIFT);
} break;
case 1623: { // scrm 
	st1 = (st1 & ~ST1_CRM) | (1 << ST1_CRM_SHIFT);
} break;
case 1624: { // scrm 
	st1 = (st1 & ~ST1_CRM) | (2 << ST1_CRM_SHIFT);
} break;
case 1625: { // scrm 
	st1 = (st1 & ~ST1_CRM) | (3 << ST1_CRM_SHIFT);
} break;
case 1626: { // sfao 
	st1 &= ~ST1_SFAO;
} break;
case 1627: { // sfao 
	st1 |= ST1_SFAO;
} break;
case 1628: { // sfai 
	st1 &= ~ST1_SFAI;
} break;
case 1629: { // sfai 
	st1 |= ST1_SFAI;
} break;
case 1630: { // sfma 
	st1 = (st1 & ~ST1_SFMA) | (0 << ST1_SFMA_SHIFT);
} break;
case 1631: { // sfma 
	st1 = (st1 & ~ST1_SFMA) | (1 << ST1_SFMA_SHIFT);
} break;
case 1632: { // sfma 
	st1 = (st1 & ~ST1_SFMA) | (2 << ST1_SFMA_SHIFT);
} break;
case 1633: { // sfma 
	st1 = (st1 & ~ST1_SFMA) | (3 << ST1_SFMA_SHIFT);
} break;
case 1634: { // sfmo 
	st1 = (st1 & ~ST1_SFMO) | (0 << ST1_SFMO_SHIFT);
} break;
case 1635: { // sfmo 
	st1 = (st1 & ~ST1_SFMO) | (1 << ST1_SFMO_SHIFT);
} break;
case 1636: { // sfmo 
	st1 = (st1 & ~ST1_SFMO) | (2 << ST1_SFMO_SHIFT);
} break;
case 1637: { // sfmo 
	st1 = (st1 & ~ST1_SFMO) | (3 << ST1_SFMO_SHIFT);
} break;
case 1638: { // rnd 
	st1 = (st1 & ~ST1_RND) | (0 << ST1_RND_SHIFT);
} break;
case 1639: { // rnd 
	st1 = (st1 & ~ST1_RND) | (1 << ST1_RND_SHIFT);
} break;
case 1640: { // rnd 
	st1 = (st1 & ~ST1_RND) | (2 << ST1_RND_SHIFT);
} break;
case 1641: { // rnd 
	st1 = (st1 & ~ST1_RND) | (3 << ST1_RND_SHIFT);
} break;
case 1642: { // rnd 
	st1 = (st1 & ~ST1_RND) | (4 << ST1_RND_SHIFT);
} break;
case 1643: { // rnd 
	st1 = (st1 & ~ST1_RND) | (5 << ST1_RND_SHIFT);
} break;
case 1644: { // rnd 
	st1 = (st1 & ~ST1_RND) | (6 << ST1_RND_SHIFT);
} break;
case 1645: { // rnd 
	st1 = (st1 & ~ST1_RND) | (7 << ST1_RND_SHIFT);
} break;
case 1646: { // idle 
	sti |= S_IDLE;
} break;
case 1647: { // rptk 
	rptc_next = i->param;
} break;
case 1648: { // lcak 
	ca = i->param;
} break;
case 1649: { // lirk 
	id = i->param;
} break;
case 1650: { // lcac 
	if(((int32_t)aacc) >= 0)
	ca = i->param;
} break;
case 1651: { // b 
	pc = i->param;
  sti |= S_BRANCH;
} break;
case 1652: { // bgz 
	if(((int32_t)aacc) > 0) {
	pc = i->param;
  sti |= S_BRANCH;
	}
} break;
case 1653: { // blz 
	if(((int32_t)aacc) < 0) {
	pc = i->param;
  sti |= S_BRANCH;
	}
} break;
case 1654: { // bnz 
	if(aacc) {
	pc = i->param;
  sti |= S_BRANCH;
	}
} break;
case 1655: { // bv 
	if(st1 & ST1_AOV) {
	st1 &= ~ST1_AOV;
	pc = i->param;
  sti |= S_BRANCH;
	}
} break;
#endif

#ifdef CINTRPGOTO
op_4: { // abs sfao=0
	if(((int32_t)aacc) < 0) {
	aacc = - aacc;
	if(((int32_t)aacc) < 0)
		st1 |= ST1_AOV;
	}
} TMS_NEXT_ICD;
op_5: { // abs sfao=1
	aacc = (aacc << 7);
	if(((int32_t)aacc) < 0) {
	aacc = - aacc;
	if(((int32_t)aacc) < 0)
		st1 |= ST1_AOV;
	}
} TMS_NEXT_ICD;
op_6: { // neg sfao=0
	r = -(int64_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_7: { // neg sfao=1
	r = -(int64_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_8: { // add dmode=0 dbp=0 sfao=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_9: { // add dmode=1 dbp=0 sfao=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_10: { // add dmode=0 dbp=1 sfao=0
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_11: { // add dmode=1 dbp=1 sfao=0
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_12: { // add dmode=0 dbp=0 sfao=1
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_13: { // add dmode=1 dbp=0 sfao=1
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_14: { // add dmode=0 dbp=1 sfao=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_15: { // add dmode=1 dbp=1 sfao=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_16: { // add cmode=0 sfao=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_17: { // add cmode=1 sfao=0
	r = (int64_t)(int32_t)get_cmem(ca) + (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_18: { // add cmode=0 sfao=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_19: { // add cmode=1 sfao=1
	r = (int64_t)(int32_t)get_cmem(ca) + (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_20: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_21: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_22: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_23: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_24: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_25: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_26: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_27: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_28: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_29: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_30: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_31: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_32: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_33: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_34: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_35: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_36: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_37: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_38: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_39: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_40: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_41: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_42: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_43: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_44: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_45: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_46: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_47: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_48: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_49: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_50: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_51: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_52: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_53: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_54: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_55: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_56: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_57: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_58: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_59: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_60: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_61: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_62: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_63: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_64: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_65: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_66: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_67: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_68: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_69: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_70: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_71: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_72: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_73: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_74: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_75: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_76: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_77: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_78: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_79: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_80: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_81: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_82: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_83: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_84: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_85: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_86: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_87: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_88: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_89: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_90: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_91: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_92: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_93: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_94: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_95: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_96: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_97: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_98: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_99: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_100: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_101: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_102: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_103: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_104: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_105: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_106: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_107: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_108: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_109: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_110: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_111: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_112: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_113: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_114: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_115: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_116: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_117: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_118: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_119: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_120: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_121: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_122: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_123: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_124: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_125: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_126: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_127: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_128: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_129: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_130: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_131: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_132: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_133: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_134: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_135: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_136: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_137: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_138: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_139: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_140: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_141: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_142: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_143: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_144: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_145: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_146: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_147: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_148: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_149: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_150: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_151: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_152: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_153: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_154: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_155: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_156: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_157: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_158: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_159: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_160: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_161: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_162: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_163: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_164: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_165: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_166: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_167: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_168: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_169: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_170: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_171: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_172: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_173: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_174: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_175: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_176: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_177: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_178: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_179: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_180: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_181: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_182: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_183: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_184: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_185: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_186: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_187: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_188: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_189: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_190: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_191: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_192: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_193: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_194: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_195: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_196: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_197: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_198: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_199: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_200: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_201: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_202: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_203: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_204: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_205: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_206: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_207: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_208: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_209: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_210: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_211: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_212: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_213: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_214: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_215: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_216: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_217: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_218: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_219: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_220: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_221: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_222: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_223: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_224: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_225: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_226: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_227: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_228: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_229: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_230: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_231: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_232: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_233: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_234: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_235: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_236: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_237: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_238: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_239: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_240: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_241: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_242: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_243: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_244: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_245: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_246: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_247: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_248: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_249: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_250: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_251: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_252: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_253: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_254: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_255: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_256: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_257: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_258: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_259: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_260: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_261: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_262: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_263: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_264: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_265: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_266: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_267: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_268: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_269: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_270: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_271: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_272: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_273: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_274: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_275: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_276: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_277: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_278: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_279: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_280: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_281: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_282: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_283: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_284: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_285: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_286: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_287: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_288: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_289: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_290: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_291: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_292: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_293: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_294: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_295: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_296: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_297: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_298: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_299: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_300: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_301: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_302: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_303: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_304: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_305: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_306: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_307: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_308: { // add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_309: { // add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_310: { // add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_311: { // add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_312: { // add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_313: { // add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_314: { // add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_315: { // add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_316: { // add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_317: { // add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_318: { // add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_319: { // add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_320: { // add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_321: { // add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_322: { // add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_323: { // add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_324: { // add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_325: { // add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_326: { // add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_327: { // add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_328: { // add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_329: { // add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_330: { // add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_331: { // add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_332: { // add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_333: { // add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_334: { // add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_335: { // add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_336: { // add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_337: { // add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_338: { // add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_339: { // add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_340: { // add cmode=0 sfmo=0 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_341: { // add cmode=1 sfmo=0 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_342: { // add cmode=0 sfmo=1 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_343: { // add cmode=1 sfmo=1 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_344: { // add cmode=0 sfmo=2 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_345: { // add cmode=1 sfmo=2 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_346: { // add cmode=0 sfmo=3 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_347: { // add cmode=1 sfmo=3 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_348: { // add cmode=0 sfmo=0 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_349: { // add cmode=1 sfmo=0 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_350: { // add cmode=0 sfmo=1 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_351: { // add cmode=1 sfmo=1 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_352: { // add cmode=0 sfmo=2 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_353: { // add cmode=1 sfmo=2 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_354: { // add cmode=0 sfmo=3 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_355: { // add cmode=1 sfmo=3 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_356: { // add cmode=0 sfmo=0 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_357: { // add cmode=1 sfmo=0 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_358: { // add cmode=0 sfmo=1 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_359: { // add cmode=1 sfmo=1 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_360: { // add cmode=0 sfmo=2 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_361: { // add cmode=1 sfmo=2 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_362: { // add cmode=0 sfmo=3 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_363: { // add cmode=1 sfmo=3 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_364: { // add cmode=0 sfmo=0 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_365: { // add cmode=1 sfmo=0 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_366: { // add cmode=0 sfmo=1 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_367: { // add cmode=1 sfmo=1 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_368: { // add cmode=0 sfmo=2 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_369: { // add cmode=1 sfmo=2 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_370: { // add cmode=0 sfmo=3 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_371: { // add cmode=1 sfmo=3 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_372: { // add cmode=0 sfmo=0 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_373: { // add cmode=1 sfmo=0 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_374: { // add cmode=0 sfmo=1 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_375: { // add cmode=1 sfmo=1 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_376: { // add cmode=0 sfmo=2 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_377: { // add cmode=1 sfmo=2 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_378: { // add cmode=0 sfmo=3 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_379: { // add cmode=1 sfmo=3 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_380: { // add cmode=0 sfmo=0 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_381: { // add cmode=1 sfmo=0 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_382: { // add cmode=0 sfmo=1 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_383: { // add cmode=1 sfmo=1 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_384: { // add cmode=0 sfmo=2 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_385: { // add cmode=1 sfmo=2 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_386: { // add cmode=0 sfmo=3 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_387: { // add cmode=1 sfmo=3 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_388: { // add cmode=0 sfmo=0 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_389: { // add cmode=1 sfmo=0 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_390: { // add cmode=0 sfmo=1 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_391: { // add cmode=1 sfmo=1 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_392: { // add cmode=0 sfmo=2 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_393: { // add cmode=1 sfmo=2 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_394: { // add cmode=0 sfmo=3 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_395: { // add cmode=1 sfmo=3 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_396: { // add cmode=0 sfmo=0 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_397: { // add cmode=1 sfmo=0 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_398: { // add cmode=0 sfmo=1 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_399: { // add cmode=1 sfmo=1 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_400: { // add cmode=0 sfmo=2 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_401: { // add cmode=1 sfmo=2 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_402: { // add cmode=0 sfmo=3 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_403: { // add cmode=1 sfmo=3 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_404: { // add cmode=0 sfmo=0 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_405: { // add cmode=1 sfmo=0 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_406: { // add cmode=0 sfmo=1 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_407: { // add cmode=1 sfmo=1 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_408: { // add cmode=0 sfmo=2 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_409: { // add cmode=1 sfmo=2 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_410: { // add cmode=0 sfmo=3 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_411: { // add cmode=1 sfmo=3 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_412: { // add cmode=0 sfmo=0 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_413: { // add cmode=1 sfmo=0 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_414: { // add cmode=0 sfmo=1 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_415: { // add cmode=1 sfmo=1 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_416: { // add cmode=0 sfmo=2 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_417: { // add cmode=1 sfmo=2 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_418: { // add cmode=0 sfmo=3 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_419: { // add cmode=1 sfmo=3 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) + (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_420: { // add cmode=0 dmode=0 dbp=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) + (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_421: { // add cmode=1 dmode=0 dbp=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) + (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_422: { // add cmode=0 dmode=1 dbp=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) + (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_423: { // add cmode=1 dmode=1 dbp=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) + (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_424: { // add cmode=0 dmode=0 dbp=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) + (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_425: { // add cmode=1 dmode=0 dbp=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) + (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_426: { // add cmode=0 dmode=1 dbp=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) + (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_427: { // add cmode=1 dmode=1 dbp=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) + (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_428: { // sub dmode=0 dbp=0 sfao=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_429: { // sub dmode=1 dbp=0 sfao=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_430: { // sub dmode=0 dbp=1 sfao=0
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_431: { // sub dmode=1 dbp=1 sfao=0
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_432: { // sub dmode=0 dbp=0 sfao=1
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_433: { // sub dmode=1 dbp=0 sfao=1
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_434: { // sub dmode=0 dbp=1 sfao=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_435: { // sub dmode=1 dbp=1 sfao=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_436: { // sub cmode=0 sfao=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_437: { // sub cmode=1 sfao=0
	r = (int64_t)(int32_t)get_cmem(ca) - (int64_t)(int32_t)aacc;
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_438: { // sub cmode=0 sfao=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_439: { // sub cmode=1 sfao=1
	r = (int64_t)(int32_t)get_cmem(ca) - (int64_t)(int32_t)(aacc << 7);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_440: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_441: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_442: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_443: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_444: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_445: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_446: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_447: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_448: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_449: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_450: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_451: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_452: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_453: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_454: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_455: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_456: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_457: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_458: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_459: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_460: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_461: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_462: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_463: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_464: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_465: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_466: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_467: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_468: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_469: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_470: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_471: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_472: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_473: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_474: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_475: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_476: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_477: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_478: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_479: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_480: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_481: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_482: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_483: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_484: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_485: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_486: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_487: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_488: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_489: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_490: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_491: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_492: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_493: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_494: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_495: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_496: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_497: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_498: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_499: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_500: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_501: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_502: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_503: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_504: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_505: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_506: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_507: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_508: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_509: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_510: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_511: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_512: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_513: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_514: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_515: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_516: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_517: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_518: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_519: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_520: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_521: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_522: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_523: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_524: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_525: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_526: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_527: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_528: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_529: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_530: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_531: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_532: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_533: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_534: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_535: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_536: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_537: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_538: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_539: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_540: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_541: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_542: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_543: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_544: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_545: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_546: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_547: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_548: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_549: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_550: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_551: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_552: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_553: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_554: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_555: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_556: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_557: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_558: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_559: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_560: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_561: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_562: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_563: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_564: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_565: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_566: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_567: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_568: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_569: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_570: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_571: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_572: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_573: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_574: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_575: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_576: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_577: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_578: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_579: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_580: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_581: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_582: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_583: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_584: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_585: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_586: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_587: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_588: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_589: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_590: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_591: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_592: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_593: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_594: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_595: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_596: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_597: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_598: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_599: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_600: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_601: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_602: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_603: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_604: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_605: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_606: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_607: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_608: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_609: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_610: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_611: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_612: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_613: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_614: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_615: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_616: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_617: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_618: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_619: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_620: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_621: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_622: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_623: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_624: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_625: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_626: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_627: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_628: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_629: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_630: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_631: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_632: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_633: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_634: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_635: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_636: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_637: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_638: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_639: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_640: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_641: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_642: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_643: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_644: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_645: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_646: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_647: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_648: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_649: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_650: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_651: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_652: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_653: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_654: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_655: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_656: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_657: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_658: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_659: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_660: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_661: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_662: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_663: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_664: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_665: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_666: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_667: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_668: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_669: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_670: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_671: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_672: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_673: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_674: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_675: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_676: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_677: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_678: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_679: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_680: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_681: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_682: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_683: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_684: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_685: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_686: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_687: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_688: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_689: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_690: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_691: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_692: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_693: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_694: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_695: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_696: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_697: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_698: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_699: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_700: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_701: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_702: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_703: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_704: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_705: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_706: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_707: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_708: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_709: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_710: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_711: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_712: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_713: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_714: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_715: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_716: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_717: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_718: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_719: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_720: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_721: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_722: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_723: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_724: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_725: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_726: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_727: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_728: { // sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_729: { // sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_730: { // sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_731: { // sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_732: { // sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_733: { // sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_734: { // sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_735: { // sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_736: { // sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_737: { // sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_738: { // sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_739: { // sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_740: { // sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_741: { // sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_742: { // sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_743: { // sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_744: { // sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_745: { // sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_746: { // sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_747: { // sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_748: { // sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_749: { // sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_750: { // sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_751: { // sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_752: { // sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_753: { // sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_754: { // sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_755: { // sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_756: { // sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_757: { // sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_758: { // sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_759: { // sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (int64_t)(int32_t)d - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_760: { // sub cmode=0 sfmo=0 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_761: { // sub cmode=1 sfmo=0 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_762: { // sub cmode=0 sfmo=1 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_763: { // sub cmode=1 sfmo=1 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_764: { // sub cmode=0 sfmo=2 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_765: { // sub cmode=1 sfmo=2 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_766: { // sub cmode=0 sfmo=3 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_767: { // sub cmode=1 sfmo=3 rnd=0 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_768: { // sub cmode=0 sfmo=0 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_769: { // sub cmode=1 sfmo=0 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_770: { // sub cmode=0 sfmo=1 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_771: { // sub cmode=1 sfmo=1 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_772: { // sub cmode=0 sfmo=2 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_773: { // sub cmode=1 sfmo=2 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_774: { // sub cmode=0 sfmo=3 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_775: { // sub cmode=1 sfmo=3 rnd=1 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_776: { // sub cmode=0 sfmo=0 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_777: { // sub cmode=1 sfmo=0 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_778: { // sub cmode=0 sfmo=1 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_779: { // sub cmode=1 sfmo=1 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_780: { // sub cmode=0 sfmo=2 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_781: { // sub cmode=1 sfmo=2 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_782: { // sub cmode=0 sfmo=3 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_783: { // sub cmode=1 sfmo=3 rnd=2 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_784: { // sub cmode=0 sfmo=0 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_785: { // sub cmode=1 sfmo=0 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_786: { // sub cmode=0 sfmo=1 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_787: { // sub cmode=1 sfmo=1 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_788: { // sub cmode=0 sfmo=2 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_789: { // sub cmode=1 sfmo=2 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_790: { // sub cmode=0 sfmo=3 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_791: { // sub cmode=1 sfmo=3 rnd=3 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_792: { // sub cmode=0 sfmo=0 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_793: { // sub cmode=1 sfmo=0 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_794: { // sub cmode=0 sfmo=1 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_795: { // sub cmode=1 sfmo=1 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_796: { // sub cmode=0 sfmo=2 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_797: { // sub cmode=1 sfmo=2 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_798: { // sub cmode=0 sfmo=3 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_799: { // sub cmode=1 sfmo=3 rnd=4 movm=0
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_800: { // sub cmode=0 sfmo=0 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_801: { // sub cmode=1 sfmo=0 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_802: { // sub cmode=0 sfmo=1 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_803: { // sub cmode=1 sfmo=1 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_804: { // sub cmode=0 sfmo=2 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_805: { // sub cmode=1 sfmo=2 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_806: { // sub cmode=0 sfmo=3 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_807: { // sub cmode=1 sfmo=3 rnd=0 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_808: { // sub cmode=0 sfmo=0 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_809: { // sub cmode=1 sfmo=0 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_810: { // sub cmode=0 sfmo=1 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_811: { // sub cmode=1 sfmo=1 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_812: { // sub cmode=0 sfmo=2 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_813: { // sub cmode=1 sfmo=2 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_814: { // sub cmode=0 sfmo=3 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_815: { // sub cmode=1 sfmo=3 rnd=1 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_816: { // sub cmode=0 sfmo=0 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_817: { // sub cmode=1 sfmo=0 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_818: { // sub cmode=0 sfmo=1 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_819: { // sub cmode=1 sfmo=1 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_820: { // sub cmode=0 sfmo=2 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_821: { // sub cmode=1 sfmo=2 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_822: { // sub cmode=0 sfmo=3 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_823: { // sub cmode=1 sfmo=3 rnd=2 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_824: { // sub cmode=0 sfmo=0 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_825: { // sub cmode=1 sfmo=0 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_826: { // sub cmode=0 sfmo=1 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_827: { // sub cmode=1 sfmo=1 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_828: { // sub cmode=0 sfmo=2 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_829: { // sub cmode=1 sfmo=2 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_830: { // sub cmode=0 sfmo=3 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_831: { // sub cmode=1 sfmo=3 rnd=3 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_832: { // sub cmode=0 sfmo=0 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_833: { // sub cmode=1 sfmo=0 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_834: { // sub cmode=0 sfmo=1 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_835: { // sub cmode=1 sfmo=1 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_836: { // sub cmode=0 sfmo=2 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_837: { // sub cmode=1 sfmo=2 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_838: { // sub cmode=0 sfmo=3 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(i->param) - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_839: { // sub cmode=1 sfmo=3 rnd=4 movm=1
	r = (int64_t)(int32_t)get_cmem(ca) - (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_840: { // sub cmode=0 dmode=0 dbp=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) - (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_841: { // sub cmode=1 dmode=0 dbp=0
	r = (int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8) - (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_842: { // sub cmode=0 dmode=1 dbp=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) - (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_843: { // sub cmode=1 dmode=1 dbp=0
	r = (int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8) - (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_844: { // sub cmode=0 dmode=0 dbp=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) - (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_845: { // sub cmode=1 dmode=0 dbp=1
	r = (int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8) - (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_846: { // sub cmode=0 dmode=1 dbp=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) - (int64_t)(int32_t)get_cmem(i->param);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_847: { // sub cmode=1 dmode=1 dbp=1
	r = (int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8) - (int64_t)(int32_t)get_cmem(ca);
  if(r < -2147483648 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = std::max(int64_t(-2147483648), std::min(int64_t(2147483647), r));
  }  aacc = r;
} TMS_NEXT_ICD;
op_848: { // lacd dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = d;
} TMS_NEXT_ICD;
op_849: { // lacd dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = d;
} TMS_NEXT_ICD;
op_850: { // lacd dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = d;
} TMS_NEXT_ICD;
op_851: { // lacd dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = d;
} TMS_NEXT_ICD;
op_852: { // lacd dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = d;
} TMS_NEXT_ICD;
op_853: { // lacd dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = d;
} TMS_NEXT_ICD;
op_854: { // lacd dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = d;
} TMS_NEXT_ICD;
op_855: { // lacd dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = d;
} TMS_NEXT_ICD;
op_856: { // lacc cmode=0
	aacc = get_cmem(i->param);
} TMS_NEXT_ICD;
op_857: { // lacc cmode=1
	aacc = get_cmem(ca);
} TMS_NEXT_ICD;
op_858: { // and dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc &= d;
} TMS_NEXT_ICD;
op_859: { // and dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc &= d;
} TMS_NEXT_ICD;
op_860: { // and dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc &= d;
} TMS_NEXT_ICD;
op_861: { // and dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc &= d;
} TMS_NEXT_ICD;
op_862: { // and dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc &= d;
} TMS_NEXT_ICD;
op_863: { // and dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc &= d;
} TMS_NEXT_ICD;
op_864: { // and dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc &= d;
} TMS_NEXT_ICD;
op_865: { // and dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc &= d;
} TMS_NEXT_ICD;
op_866: { // and cmode=0
	aacc &= get_cmem(i->param);
} TMS_NEXT_ICD;
op_867: { // and cmode=1
	aacc &= get_cmem(ca);
} TMS_NEXT_ICD;
op_868: { // and cmode=0 dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) & d;
} TMS_NEXT_ICD;
op_869: { // and cmode=1 dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) & d;
} TMS_NEXT_ICD;
op_870: { // and cmode=0 dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) & d;
} TMS_NEXT_ICD;
op_871: { // and cmode=1 dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) & d;
} TMS_NEXT_ICD;
op_872: { // and cmode=0 dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
} TMS_NEXT_ICD;
op_873: { // and cmode=1 dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
} TMS_NEXT_ICD;
op_874: { // and cmode=0 dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
} TMS_NEXT_ICD;
op_875: { // and cmode=1 dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
} TMS_NEXT_ICD;
op_876: { // and cmode=0 dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) & d;
} TMS_NEXT_ICD;
op_877: { // and cmode=1 dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) & d;
} TMS_NEXT_ICD;
op_878: { // and cmode=0 dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) & d;
} TMS_NEXT_ICD;
op_879: { // and cmode=1 dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) & d;
} TMS_NEXT_ICD;
op_880: { // and cmode=0 dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
} TMS_NEXT_ICD;
op_881: { // and cmode=1 dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
} TMS_NEXT_ICD;
op_882: { // and cmode=0 dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
} TMS_NEXT_ICD;
op_883: { // and cmode=1 dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
} TMS_NEXT_ICD;
op_884: { // or dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc |= d;
} TMS_NEXT_ICD;
op_885: { // or dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc |= d;
} TMS_NEXT_ICD;
op_886: { // or dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc |= d;
} TMS_NEXT_ICD;
op_887: { // or dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc |= d;
} TMS_NEXT_ICD;
op_888: { // or dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc |= d;
} TMS_NEXT_ICD;
op_889: { // or dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc |= d;
} TMS_NEXT_ICD;
op_890: { // or dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc |= d;
} TMS_NEXT_ICD;
op_891: { // or dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc |= d;
} TMS_NEXT_ICD;
op_892: { // or cmode=0
	aacc |= get_cmem(i->param);
} TMS_NEXT_ICD;
op_893: { // or cmode=1
	aacc |= get_cmem(ca);
} TMS_NEXT_ICD;
op_894: { // or cmode=0 dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) | d;
} TMS_NEXT_ICD;
op_895: { // or cmode=1 dmode=0 sfai=0 dbp=0
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) | d;
} TMS_NEXT_ICD;
op_896: { // or cmode=0 dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) | d;
} TMS_NEXT_ICD;
op_897: { // or cmode=1 dmode=1 sfai=0 dbp=0
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) | d;
} TMS_NEXT_ICD;
op_898: { // or cmode=0 dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
} TMS_NEXT_ICD;
op_899: { // or cmode=1 dmode=0 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
} TMS_NEXT_ICD;
op_900: { // or cmode=0 dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
} TMS_NEXT_ICD;
op_901: { // or cmode=1 dmode=1 sfai=1 dbp=0
	d = ((int32_t)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
} TMS_NEXT_ICD;
op_902: { // or cmode=0 dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) | d;
} TMS_NEXT_ICD;
op_903: { // or cmode=1 dmode=0 sfai=0 dbp=1
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) | d;
} TMS_NEXT_ICD;
op_904: { // or cmode=0 dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) | d;
} TMS_NEXT_ICD;
op_905: { // or cmode=1 dmode=1 sfai=0 dbp=1
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) | d;
} TMS_NEXT_ICD;
op_906: { // or cmode=0 dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
} TMS_NEXT_ICD;
op_907: { // or cmode=1 dmode=0 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
} TMS_NEXT_ICD;
op_908: { // or cmode=0 dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
} TMS_NEXT_ICD;
op_909: { // or cmode=1 dmode=1 sfai=1 dbp=1
	d = ((int32_t)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
} TMS_NEXT_ICD;
op_910: { // mpy cmode=0 dmode=0 dbp=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_911: { // mpy cmode=1 dmode=0 dbp=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_912: { // mpy cmode=0 dmode=1 dbp=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_913: { // mpy cmode=1 dmode=1 dbp=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_914: { // mpy cmode=0 dmode=0 dbp=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_915: { // mpy cmode=1 dmode=0 dbp=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_916: { // mpy cmode=0 dmode=1 dbp=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_917: { // mpy cmode=1 dmode=1 dbp=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_918: { // mpy cmode=0 sfao=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = r >> 15;
} TMS_NEXT_ICD;
op_919: { // mpy cmode=1 sfao=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = r >> 15;
} TMS_NEXT_ICD;
op_920: { // mpy cmode=0 sfao=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = r >> 15;
} TMS_NEXT_ICD;
op_921: { // mpy cmode=1 sfao=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = r >> 15;
} TMS_NEXT_ICD;
op_922: { // mac cmode=0 dmode=0 dbp=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_923: { // mac cmode=1 dmode=0 dbp=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_924: { // mac cmode=0 dmode=1 dbp=0 sfma=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_925: { // mac cmode=1 dmode=1 dbp=0 sfma=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_926: { // mac cmode=0 dmode=0 dbp=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_927: { // mac cmode=1 dmode=0 dbp=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_928: { // mac cmode=0 dmode=1 dbp=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_929: { // mac cmode=1 dmode=1 dbp=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_930: { // mac cmode=0 dmode=0 dbp=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_931: { // mac cmode=1 dmode=0 dbp=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_932: { // mac cmode=0 dmode=1 dbp=0 sfma=1
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_933: { // mac cmode=1 dmode=1 dbp=0 sfma=1
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_934: { // mac cmode=0 dmode=0 dbp=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_935: { // mac cmode=1 dmode=0 dbp=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_936: { // mac cmode=0 dmode=1 dbp=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_937: { // mac cmode=1 dmode=1 dbp=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_938: { // mac cmode=0 dmode=0 dbp=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_939: { // mac cmode=1 dmode=0 dbp=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_940: { // mac cmode=0 dmode=1 dbp=0 sfma=2
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_941: { // mac cmode=1 dmode=1 dbp=0 sfma=2
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_942: { // mac cmode=0 dmode=0 dbp=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_943: { // mac cmode=1 dmode=0 dbp=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_944: { // mac cmode=0 dmode=1 dbp=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_945: { // mac cmode=1 dmode=1 dbp=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_946: { // mac cmode=0 dmode=0 dbp=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_947: { // mac cmode=1 dmode=0 dbp=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_948: { // mac cmode=0 dmode=1 dbp=0 sfma=3
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_949: { // mac cmode=1 dmode=1 dbp=0 sfma=3
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_950: { // mac cmode=0 dmode=0 dbp=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_951: { // mac cmode=1 dmode=0 dbp=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_952: { // mac cmode=0 dmode=1 dbp=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_953: { // mac cmode=1 dmode=1 dbp=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_954: { // mac dmode=0 dbp=0 sfao=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_955: { // mac dmode=1 dbp=0 sfao=0 sfma=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_956: { // mac dmode=0 dbp=1 sfao=0 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_957: { // mac dmode=1 dbp=1 sfao=0 sfma=0
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_958: { // mac dmode=0 dbp=0 sfao=1 sfma=0
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_959: { // mac dmode=1 dbp=0 sfao=1 sfma=0
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_960: { // mac dmode=0 dbp=1 sfao=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_961: { // mac dmode=1 dbp=1 sfao=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_962: { // mac dmode=0 dbp=0 sfao=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_963: { // mac dmode=1 dbp=0 sfao=0 sfma=1
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_964: { // mac dmode=0 dbp=1 sfao=0 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_965: { // mac dmode=1 dbp=1 sfao=0 sfma=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_966: { // mac dmode=0 dbp=0 sfao=1 sfma=1
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_967: { // mac dmode=1 dbp=0 sfao=1 sfma=1
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_968: { // mac dmode=0 dbp=1 sfao=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_969: { // mac dmode=1 dbp=1 sfao=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_970: { // mac dmode=0 dbp=0 sfao=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_971: { // mac dmode=1 dbp=0 sfao=0 sfma=2
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_972: { // mac dmode=0 dbp=1 sfao=0 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_973: { // mac dmode=1 dbp=1 sfao=0 sfma=2
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_974: { // mac dmode=0 dbp=0 sfao=1 sfma=2
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_975: { // mac dmode=1 dbp=0 sfao=1 sfma=2
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_976: { // mac dmode=0 dbp=1 sfao=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_977: { // mac dmode=1 dbp=1 sfao=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_978: { // mac dmode=0 dbp=0 sfao=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_979: { // mac dmode=1 dbp=0 sfao=0 sfma=3
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_980: { // mac dmode=0 dbp=1 sfao=0 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_981: { // mac dmode=1 dbp=1 sfao=0 sfma=3
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_982: { // mac dmode=0 dbp=0 sfao=1 sfma=3
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_983: { // mac dmode=1 dbp=0 sfao=1 sfma=3
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_984: { // mac dmode=0 dbp=1 sfao=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_985: { // mac dmode=1 dbp=1 sfao=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_986: { // mac cmode=0 sfao=0 sfma=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = macc + (r >> 15);
} TMS_NEXT_ICD;
op_987: { // mac cmode=1 sfao=0 sfma=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = macc + (r >> 15);
} TMS_NEXT_ICD;
op_988: { // mac cmode=0 sfao=1 sfma=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = macc + (r >> 15);
} TMS_NEXT_ICD;
op_989: { // mac cmode=1 sfao=1 sfma=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = macc + (r >> 15);
} TMS_NEXT_ICD;
op_990: { // mac cmode=0 sfao=0 sfma=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 2) + (r >> 15);
} TMS_NEXT_ICD;
op_991: { // mac cmode=1 sfao=0 sfma=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 2) + (r >> 15);
} TMS_NEXT_ICD;
op_992: { // mac cmode=0 sfao=1 sfma=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 2) + (r >> 15);
} TMS_NEXT_ICD;
op_993: { // mac cmode=1 sfao=1 sfma=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 2) + (r >> 15);
} TMS_NEXT_ICD;
op_994: { // mac cmode=0 sfao=0 sfma=2
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 4) + (r >> 15);
} TMS_NEXT_ICD;
op_995: { // mac cmode=1 sfao=0 sfma=2
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 4) + (r >> 15);
} TMS_NEXT_ICD;
op_996: { // mac cmode=0 sfao=1 sfma=2
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 4) + (r >> 15);
} TMS_NEXT_ICD;
op_997: { // mac cmode=1 sfao=1 sfma=2
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 4) + (r >> 15);
} TMS_NEXT_ICD;
op_998: { // mac cmode=0 sfao=0 sfma=3
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc >> 16) + (r >> 15);
} TMS_NEXT_ICD;
op_999: { // mac cmode=1 sfao=0 sfma=3
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc >> 16) + (r >> 15);
} TMS_NEXT_ICD;
op_1000: { // mac cmode=0 sfao=1 sfma=3
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc >> 16) + (r >> 15);
} TMS_NEXT_ICD;
op_1001: { // mac cmode=1 sfao=1 sfma=3
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc >> 16) + (r >> 15);
} TMS_NEXT_ICD;
op_1002: { // mpyu cmode=0 dmode=0 dbp=0
	creg = c = get_cmem(i->param);
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_1003: { // mpyu cmode=1 dmode=0 dbp=0
	creg = c = get_cmem(ca);
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_1004: { // mpyu cmode=0 dmode=1 dbp=0
	creg = c = get_cmem(i->param);
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_1005: { // mpyu cmode=1 dmode=1 dbp=0
	creg = c = get_cmem(ca);
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_1006: { // mpyu cmode=0 dmode=0 dbp=1
	creg = c = get_cmem(i->param);
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_1007: { // mpyu cmode=1 dmode=0 dbp=1
	creg = c = get_cmem(ca);
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_1008: { // mpyu cmode=0 dmode=1 dbp=1
	creg = c = get_cmem(i->param);
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_1009: { // mpyu cmode=1 dmode=1 dbp=1
	creg = c = get_cmem(ca);
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = r >> 7;
} TMS_NEXT_ICD;
op_1010: { // macu cmode=0 dmode=0 dbp=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1011: { // macu cmode=1 dmode=0 dbp=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1012: { // macu cmode=0 dmode=1 dbp=0 sfma=0
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1013: { // macu cmode=1 dmode=1 dbp=0 sfma=0
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1014: { // macu cmode=0 dmode=0 dbp=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1015: { // macu cmode=1 dmode=0 dbp=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1016: { // macu cmode=0 dmode=1 dbp=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1017: { // macu cmode=1 dmode=1 dbp=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1018: { // macu cmode=0 dmode=0 dbp=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1019: { // macu cmode=1 dmode=0 dbp=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1020: { // macu cmode=0 dmode=1 dbp=0 sfma=1
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1021: { // macu cmode=1 dmode=1 dbp=0 sfma=1
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1022: { // macu cmode=0 dmode=0 dbp=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1023: { // macu cmode=1 dmode=0 dbp=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1024: { // macu cmode=0 dmode=1 dbp=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1025: { // macu cmode=1 dmode=1 dbp=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1026: { // macu cmode=0 dmode=0 dbp=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1027: { // macu cmode=1 dmode=0 dbp=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1028: { // macu cmode=0 dmode=1 dbp=0 sfma=2
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1029: { // macu cmode=1 dmode=1 dbp=0 sfma=2
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1030: { // macu cmode=0 dmode=0 dbp=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1031: { // macu cmode=1 dmode=0 dbp=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1032: { // macu cmode=0 dmode=1 dbp=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1033: { // macu cmode=1 dmode=1 dbp=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1034: { // macu cmode=0 dmode=0 dbp=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1035: { // macu cmode=1 dmode=0 dbp=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1036: { // macu cmode=0 dmode=1 dbp=0 sfma=3
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1037: { // macu cmode=1 dmode=1 dbp=0 sfma=3
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1038: { // macu cmode=0 dmode=0 dbp=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1039: { // macu cmode=1 dmode=0 dbp=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1040: { // macu cmode=0 dmode=1 dbp=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1041: { // macu cmode=1 dmode=1 dbp=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1042: { // macu dmode=0 dbp=0 sfao=0 sfma=0
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1043: { // macu dmode=1 dbp=0 sfao=0 sfma=0
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1044: { // macu dmode=0 dbp=1 sfao=0 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1045: { // macu dmode=1 dbp=1 sfao=0 sfma=0
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1046: { // macu dmode=0 dbp=0 sfao=1 sfma=0
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1047: { // macu dmode=1 dbp=0 sfao=1 sfma=0
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1048: { // macu dmode=0 dbp=1 sfao=1 sfma=0
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1049: { // macu dmode=1 dbp=1 sfao=1 sfma=0
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = macc + (r >> 7);
} TMS_NEXT_ICD;
op_1050: { // macu dmode=0 dbp=0 sfao=0 sfma=1
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1051: { // macu dmode=1 dbp=0 sfao=0 sfma=1
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1052: { // macu dmode=0 dbp=1 sfao=0 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1053: { // macu dmode=1 dbp=1 sfao=0 sfma=1
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1054: { // macu dmode=0 dbp=0 sfao=1 sfma=1
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1055: { // macu dmode=1 dbp=0 sfao=1 sfma=1
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1056: { // macu dmode=0 dbp=1 sfao=1 sfma=1
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1057: { // macu dmode=1 dbp=1 sfao=1 sfma=1
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 2) + (r >> 7);
} TMS_NEXT_ICD;
op_1058: { // macu dmode=0 dbp=0 sfao=0 sfma=2
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1059: { // macu dmode=1 dbp=0 sfao=0 sfma=2
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1060: { // macu dmode=0 dbp=1 sfao=0 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1061: { // macu dmode=1 dbp=1 sfao=0 sfma=2
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1062: { // macu dmode=0 dbp=0 sfao=1 sfma=2
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1063: { // macu dmode=1 dbp=0 sfao=1 sfma=2
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1064: { // macu dmode=0 dbp=1 sfao=1 sfma=2
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1065: { // macu dmode=1 dbp=1 sfao=1 sfma=2
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc << 4) + (r >> 7);
} TMS_NEXT_ICD;
op_1066: { // macu dmode=0 dbp=0 sfao=0 sfma=3
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1067: { // macu dmode=1 dbp=0 sfao=0 sfma=3
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1068: { // macu dmode=0 dbp=1 sfao=0 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1069: { // macu dmode=1 dbp=1 sfao=0 sfma=3
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = aacc;
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1070: { // macu dmode=0 dbp=0 sfao=1 sfma=3
	d = dmem0[(i->param + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1071: { // macu dmode=1 dbp=0 sfao=1 sfma=3
	d = dmem0[(id + ba0) & 0xff]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1072: { // macu dmode=0 dbp=1 sfao=1 sfma=3
	d = dmem1[(i->param + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1073: { // macu dmode=1 dbp=1 sfao=1 sfma=3
	d = dmem1[(id + ba1) & 0x1f]; // d is 24bits unsigned
	creg = c = (aacc << 7);
	r = (int64_t)(int32_t)c * (int64_t)d;
	macc = (macc >> 16) + (r >> 7);
} TMS_NEXT_ICD;
op_1074: { // macs cmode=0 sfao=0 sfma=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = macc + (r >> 14);
} TMS_NEXT_ICD;
op_1075: { // macs cmode=1 sfao=0 sfma=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = macc + (r >> 14);
} TMS_NEXT_ICD;
op_1076: { // macs cmode=0 sfao=1 sfma=0
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = macc + (r >> 14);
} TMS_NEXT_ICD;
op_1077: { // macs cmode=1 sfao=1 sfma=0
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = macc + (r >> 14);
} TMS_NEXT_ICD;
op_1078: { // macs cmode=0 sfao=0 sfma=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 2) + (r >> 14);
} TMS_NEXT_ICD;
op_1079: { // macs cmode=1 sfao=0 sfma=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 2) + (r >> 14);
} TMS_NEXT_ICD;
op_1080: { // macs cmode=0 sfao=1 sfma=1
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 2) + (r >> 14);
} TMS_NEXT_ICD;
op_1081: { // macs cmode=1 sfao=1 sfma=1
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 2) + (r >> 14);
} TMS_NEXT_ICD;
op_1082: { // macs cmode=0 sfao=0 sfma=2
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 4) + (r >> 14);
} TMS_NEXT_ICD;
op_1083: { // macs cmode=1 sfao=0 sfma=2
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc << 4) + (r >> 14);
} TMS_NEXT_ICD;
op_1084: { // macs cmode=0 sfao=1 sfma=2
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 4) + (r >> 14);
} TMS_NEXT_ICD;
op_1085: { // macs cmode=1 sfao=1 sfma=2
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc << 4) + (r >> 14);
} TMS_NEXT_ICD;
op_1086: { // macs cmode=0 sfao=0 sfma=3
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc >> 16) + (r >> 14);
} TMS_NEXT_ICD;
op_1087: { // macs cmode=1 sfao=0 sfma=3
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)aacc;
	macc = (macc >> 16) + (r >> 14);
} TMS_NEXT_ICD;
op_1088: { // macs cmode=0 sfao=1 sfma=3
	creg = c = get_cmem(i->param);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc >> 16) + (r >> 14);
} TMS_NEXT_ICD;
op_1089: { // macs cmode=1 sfao=1 sfma=3
	creg = c = get_cmem(ca);
	r = (int64_t)(int32_t)c * (int64_t)(int32_t)(aacc << 7);
	macc = (macc >> 16) + (r >> 14);
} TMS_NEXT_ICD;
op_1090: { // lmhd dmode=0 dbp=0
	macc_write = macc = ((int64_t)(int32_t)(dmem0[(i->param + ba0) & 0xff] << 8)) << 16;
} TMS_NEXT_ICD;
op_1091: { // lmhd dmode=1 dbp=0
	macc_write = macc = ((int64_t)(int32_t)(dmem0[(id + ba0) & 0xff] << 8)) << 16;
} TMS_NEXT_ICD;
op_1092: { // lmhd dmode=0 dbp=1
	macc_write = macc = ((int64_t)(int32_t)(dmem1[(i->param + ba1) & 0x1f] << 8)) << 16;
} TMS_NEXT_ICD;
op_1093: { // lmhd dmode=1 dbp=1
	macc_write = macc = ((int64_t)(int32_t)(dmem1[(id + ba1) & 0x1f] << 8)) << 16;
} TMS_NEXT_ICD;
op_1094: { // lmld dmode=0 dbp=0
	macc_write = macc = (macc & ~0xffffffULL) | dmem0[(i->param + ba0) & 0xff];
} TMS_NEXT_ICD;
op_1095: { // lmld dmode=1 dbp=0
	macc_write = macc = (macc & ~0xffffffULL) | dmem0[(id + ba0) & 0xff];
} TMS_NEXT_ICD;
op_1096: { // lmld dmode=0 dbp=1
	macc_write = macc = (macc & ~0xffffffULL) | dmem1[(i->param + ba1) & 0x1f];
} TMS_NEXT_ICD;
op_1097: { // lmld dmode=1 dbp=1
	macc_write = macc = (macc & ~0xffffffULL) | dmem1[(id + ba1) & 0x1f];
} TMS_NEXT_ICD;
op_1098: { // lmhc cmode=0
	macc_write = macc = ((int64_t)(int32_t)get_cmem(i->param)) << 16;
} TMS_NEXT_ICD;
op_1099: { // lmhc cmode=1
	macc_write = macc = ((int64_t)(int32_t)get_cmem(ca)) << 16;
} TMS_NEXT_ICD;
op_1100: { // sfml 
	macc = (macc & 0x8000000000000ULL) | ((macc << 1) & 0x7ffffffffffffULL);
} TMS_NEXT_ICD;
op_1101: { // sfmr 
	macc = (macc & 0x8000000000000ULL) | ((macc >> 1) & 0x7ffffffffffffULL);
} TMS_NEXT_ICD;
op_1102: { // wre cmode=0 dmode=0 dbp=0
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xwr = dmem0[(i->param + ba0) & 0xff];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
} TMS_NEXT_ICD;
op_1103: { // wre cmode=1 dmode=0 dbp=0
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xwr = dmem0[(i->param + ba0) & 0xff];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
} TMS_NEXT_ICD;
op_1104: { // wre cmode=0 dmode=1 dbp=0
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xwr = dmem0[(id + ba0) & 0xff];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
} TMS_NEXT_ICD;
op_1105: { // wre cmode=1 dmode=1 dbp=0
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xwr = dmem0[(id + ba0) & 0xff];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
} TMS_NEXT_ICD;
op_1106: { // wre cmode=0 dmode=0 dbp=1
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xwr = dmem1[(i->param + ba1) & 0x1f];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
} TMS_NEXT_ICD;
op_1107: { // wre cmode=1 dmode=0 dbp=1
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xwr = dmem1[(i->param + ba1) & 0x1f];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
} TMS_NEXT_ICD;
op_1108: { // wre cmode=0 dmode=1 dbp=1
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xwr = dmem1[(id + ba1) & 0x1f];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
} TMS_NEXT_ICD;
op_1109: { // wre cmode=1 dmode=1 dbp=1
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xwr = dmem1[(id + ba1) & 0x1f];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
} TMS_NEXT_ICD;
op_1110: { // rde cmode=0
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_READ;
} TMS_NEXT_ICD;
op_1111: { // rde cmode=1
	if(sti & (S_READ|S_WRITE))
	goto next_icd;
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_READ;
} TMS_NEXT_ICD;
op_1112: { // sacc cmode=0 sfao=0
	cmem[i->param] = aacc;
} TMS_NEXT_ICD;
op_1113: { // sacc cmode=1 sfao=0
	cmem[ca] = aacc;
} TMS_NEXT_ICD;
op_1114: { // sacc cmode=0 sfao=1
	cmem[i->param] = (aacc << 7);
} TMS_NEXT_ICD;
op_1115: { // sacc cmode=1 sfao=1
	cmem[ca] = (aacc << 7);
} TMS_NEXT_ICD;
op_1116: { // sacd dmode=0 dbp=0 sfao=0
	dmem0[(i->param + ba0) & 0xff] = aacc >> 8;
} TMS_NEXT_ICD;
op_1117: { // sacd dmode=1 dbp=0 sfao=0
	dmem0[(id + ba0) & 0xff] = aacc >> 8;
} TMS_NEXT_ICD;
op_1118: { // sacd dmode=0 dbp=1 sfao=0
	dmem1[(i->param + ba1) & 0x1f] = aacc >> 8;
} TMS_NEXT_ICD;
op_1119: { // sacd dmode=1 dbp=1 sfao=0
	dmem1[(id + ba1) & 0x1f] = aacc >> 8;
} TMS_NEXT_ICD;
op_1120: { // sacd dmode=0 dbp=0 sfao=1
	dmem0[(i->param + ba0) & 0xff] = (aacc << 7) >> 8;
} TMS_NEXT_ICD;
op_1121: { // sacd dmode=1 dbp=0 sfao=1
	dmem0[(id + ba0) & 0xff] = (aacc << 7) >> 8;
} TMS_NEXT_ICD;
op_1122: { // sacd dmode=0 dbp=1 sfao=1
	dmem1[(i->param + ba1) & 0x1f] = (aacc << 7) >> 8;
} TMS_NEXT_ICD;
op_1123: { // sacd dmode=1 dbp=1 sfao=1
	dmem1[(id + ba1) & 0x1f] = (aacc << 7) >> 8;
} TMS_NEXT_ICD;
op_1124: { // smhd dmode=0 dbp=0 sfmo=0 rnd=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1125: { // smhd dmode=1 dbp=0 sfmo=0 rnd=0 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1126: { // smhd dmode=0 dbp=1 sfmo=0 rnd=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1127: { // smhd dmode=1 dbp=1 sfmo=0 rnd=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1128: { // smhd dmode=0 dbp=0 sfmo=1 rnd=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1129: { // smhd dmode=1 dbp=0 sfmo=1 rnd=0 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1130: { // smhd dmode=0 dbp=1 sfmo=1 rnd=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1131: { // smhd dmode=1 dbp=1 sfmo=1 rnd=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1132: { // smhd dmode=0 dbp=0 sfmo=2 rnd=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1133: { // smhd dmode=1 dbp=0 sfmo=2 rnd=0 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1134: { // smhd dmode=0 dbp=1 sfmo=2 rnd=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1135: { // smhd dmode=1 dbp=1 sfmo=2 rnd=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1136: { // smhd dmode=0 dbp=0 sfmo=3 rnd=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1137: { // smhd dmode=1 dbp=0 sfmo=3 rnd=0 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1138: { // smhd dmode=0 dbp=1 sfmo=3 rnd=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1139: { // smhd dmode=1 dbp=1 sfmo=3 rnd=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1140: { // smhd dmode=0 dbp=0 sfmo=0 rnd=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1141: { // smhd dmode=1 dbp=0 sfmo=0 rnd=1 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1142: { // smhd dmode=0 dbp=1 sfmo=0 rnd=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1143: { // smhd dmode=1 dbp=1 sfmo=0 rnd=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1144: { // smhd dmode=0 dbp=0 sfmo=1 rnd=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1145: { // smhd dmode=1 dbp=0 sfmo=1 rnd=1 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1146: { // smhd dmode=0 dbp=1 sfmo=1 rnd=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1147: { // smhd dmode=1 dbp=1 sfmo=1 rnd=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1148: { // smhd dmode=0 dbp=0 sfmo=2 rnd=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1149: { // smhd dmode=1 dbp=0 sfmo=2 rnd=1 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1150: { // smhd dmode=0 dbp=1 sfmo=2 rnd=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1151: { // smhd dmode=1 dbp=1 sfmo=2 rnd=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1152: { // smhd dmode=0 dbp=0 sfmo=3 rnd=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1153: { // smhd dmode=1 dbp=0 sfmo=3 rnd=1 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1154: { // smhd dmode=0 dbp=1 sfmo=3 rnd=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1155: { // smhd dmode=1 dbp=1 sfmo=3 rnd=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1156: { // smhd dmode=0 dbp=0 sfmo=0 rnd=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1157: { // smhd dmode=1 dbp=0 sfmo=0 rnd=2 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1158: { // smhd dmode=0 dbp=1 sfmo=0 rnd=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1159: { // smhd dmode=1 dbp=1 sfmo=0 rnd=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1160: { // smhd dmode=0 dbp=0 sfmo=1 rnd=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1161: { // smhd dmode=1 dbp=0 sfmo=1 rnd=2 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1162: { // smhd dmode=0 dbp=1 sfmo=1 rnd=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1163: { // smhd dmode=1 dbp=1 sfmo=1 rnd=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1164: { // smhd dmode=0 dbp=0 sfmo=2 rnd=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1165: { // smhd dmode=1 dbp=0 sfmo=2 rnd=2 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1166: { // smhd dmode=0 dbp=1 sfmo=2 rnd=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1167: { // smhd dmode=1 dbp=1 sfmo=2 rnd=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1168: { // smhd dmode=0 dbp=0 sfmo=3 rnd=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1169: { // smhd dmode=1 dbp=0 sfmo=3 rnd=2 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1170: { // smhd dmode=0 dbp=1 sfmo=3 rnd=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1171: { // smhd dmode=1 dbp=1 sfmo=3 rnd=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1172: { // smhd dmode=0 dbp=0 sfmo=0 rnd=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1173: { // smhd dmode=1 dbp=0 sfmo=0 rnd=3 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1174: { // smhd dmode=0 dbp=1 sfmo=0 rnd=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1175: { // smhd dmode=1 dbp=1 sfmo=0 rnd=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1176: { // smhd dmode=0 dbp=0 sfmo=1 rnd=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1177: { // smhd dmode=1 dbp=0 sfmo=1 rnd=3 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1178: { // smhd dmode=0 dbp=1 sfmo=1 rnd=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1179: { // smhd dmode=1 dbp=1 sfmo=1 rnd=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1180: { // smhd dmode=0 dbp=0 sfmo=2 rnd=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1181: { // smhd dmode=1 dbp=0 sfmo=2 rnd=3 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1182: { // smhd dmode=0 dbp=1 sfmo=2 rnd=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1183: { // smhd dmode=1 dbp=1 sfmo=2 rnd=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1184: { // smhd dmode=0 dbp=0 sfmo=3 rnd=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1185: { // smhd dmode=1 dbp=0 sfmo=3 rnd=3 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1186: { // smhd dmode=0 dbp=1 sfmo=3 rnd=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1187: { // smhd dmode=1 dbp=1 sfmo=3 rnd=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1188: { // smhd dmode=0 dbp=0 sfmo=0 rnd=4 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1189: { // smhd dmode=1 dbp=0 sfmo=0 rnd=4 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1190: { // smhd dmode=0 dbp=1 sfmo=0 rnd=4 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1191: { // smhd dmode=1 dbp=1 sfmo=0 rnd=4 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1192: { // smhd dmode=0 dbp=0 sfmo=1 rnd=4 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1193: { // smhd dmode=1 dbp=0 sfmo=1 rnd=4 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1194: { // smhd dmode=0 dbp=1 sfmo=1 rnd=4 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1195: { // smhd dmode=1 dbp=1 sfmo=1 rnd=4 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1196: { // smhd dmode=0 dbp=0 sfmo=2 rnd=4 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1197: { // smhd dmode=1 dbp=0 sfmo=2 rnd=4 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1198: { // smhd dmode=0 dbp=1 sfmo=2 rnd=4 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1199: { // smhd dmode=1 dbp=1 sfmo=2 rnd=4 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1200: { // smhd dmode=0 dbp=0 sfmo=3 rnd=4 movm=0
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1201: { // smhd dmode=1 dbp=0 sfmo=3 rnd=4 movm=0
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1202: { // smhd dmode=0 dbp=1 sfmo=3 rnd=4 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1203: { // smhd dmode=1 dbp=1 sfmo=3 rnd=4 movm=0
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1204: { // smhd dmode=0 dbp=0 sfmo=0 rnd=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1205: { // smhd dmode=1 dbp=0 sfmo=0 rnd=0 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1206: { // smhd dmode=0 dbp=1 sfmo=0 rnd=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1207: { // smhd dmode=1 dbp=1 sfmo=0 rnd=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1208: { // smhd dmode=0 dbp=0 sfmo=1 rnd=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1209: { // smhd dmode=1 dbp=0 sfmo=1 rnd=0 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1210: { // smhd dmode=0 dbp=1 sfmo=1 rnd=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1211: { // smhd dmode=1 dbp=1 sfmo=1 rnd=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1212: { // smhd dmode=0 dbp=0 sfmo=2 rnd=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1213: { // smhd dmode=1 dbp=0 sfmo=2 rnd=0 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1214: { // smhd dmode=0 dbp=1 sfmo=2 rnd=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1215: { // smhd dmode=1 dbp=1 sfmo=2 rnd=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1216: { // smhd dmode=0 dbp=0 sfmo=3 rnd=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1217: { // smhd dmode=1 dbp=0 sfmo=3 rnd=0 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1218: { // smhd dmode=0 dbp=1 sfmo=3 rnd=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1219: { // smhd dmode=1 dbp=1 sfmo=3 rnd=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1220: { // smhd dmode=0 dbp=0 sfmo=0 rnd=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1221: { // smhd dmode=1 dbp=0 sfmo=0 rnd=1 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1222: { // smhd dmode=0 dbp=1 sfmo=0 rnd=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1223: { // smhd dmode=1 dbp=1 sfmo=0 rnd=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1224: { // smhd dmode=0 dbp=0 sfmo=1 rnd=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1225: { // smhd dmode=1 dbp=0 sfmo=1 rnd=1 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1226: { // smhd dmode=0 dbp=1 sfmo=1 rnd=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1227: { // smhd dmode=1 dbp=1 sfmo=1 rnd=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1228: { // smhd dmode=0 dbp=0 sfmo=2 rnd=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1229: { // smhd dmode=1 dbp=0 sfmo=2 rnd=1 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1230: { // smhd dmode=0 dbp=1 sfmo=2 rnd=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1231: { // smhd dmode=1 dbp=1 sfmo=2 rnd=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1232: { // smhd dmode=0 dbp=0 sfmo=3 rnd=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1233: { // smhd dmode=1 dbp=0 sfmo=3 rnd=1 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1234: { // smhd dmode=0 dbp=1 sfmo=3 rnd=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1235: { // smhd dmode=1 dbp=1 sfmo=3 rnd=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1236: { // smhd dmode=0 dbp=0 sfmo=0 rnd=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1237: { // smhd dmode=1 dbp=0 sfmo=0 rnd=2 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1238: { // smhd dmode=0 dbp=1 sfmo=0 rnd=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1239: { // smhd dmode=1 dbp=1 sfmo=0 rnd=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1240: { // smhd dmode=0 dbp=0 sfmo=1 rnd=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1241: { // smhd dmode=1 dbp=0 sfmo=1 rnd=2 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1242: { // smhd dmode=0 dbp=1 sfmo=1 rnd=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1243: { // smhd dmode=1 dbp=1 sfmo=1 rnd=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1244: { // smhd dmode=0 dbp=0 sfmo=2 rnd=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1245: { // smhd dmode=1 dbp=0 sfmo=2 rnd=2 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1246: { // smhd dmode=0 dbp=1 sfmo=2 rnd=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1247: { // smhd dmode=1 dbp=1 sfmo=2 rnd=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1248: { // smhd dmode=0 dbp=0 sfmo=3 rnd=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1249: { // smhd dmode=1 dbp=0 sfmo=3 rnd=2 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1250: { // smhd dmode=0 dbp=1 sfmo=3 rnd=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1251: { // smhd dmode=1 dbp=1 sfmo=3 rnd=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1252: { // smhd dmode=0 dbp=0 sfmo=0 rnd=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1253: { // smhd dmode=1 dbp=0 sfmo=0 rnd=3 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1254: { // smhd dmode=0 dbp=1 sfmo=0 rnd=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1255: { // smhd dmode=1 dbp=1 sfmo=0 rnd=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1256: { // smhd dmode=0 dbp=0 sfmo=1 rnd=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1257: { // smhd dmode=1 dbp=0 sfmo=1 rnd=3 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1258: { // smhd dmode=0 dbp=1 sfmo=1 rnd=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1259: { // smhd dmode=1 dbp=1 sfmo=1 rnd=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1260: { // smhd dmode=0 dbp=0 sfmo=2 rnd=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1261: { // smhd dmode=1 dbp=0 sfmo=2 rnd=3 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1262: { // smhd dmode=0 dbp=1 sfmo=2 rnd=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1263: { // smhd dmode=1 dbp=1 sfmo=2 rnd=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1264: { // smhd dmode=0 dbp=0 sfmo=3 rnd=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1265: { // smhd dmode=1 dbp=0 sfmo=3 rnd=3 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1266: { // smhd dmode=0 dbp=1 sfmo=3 rnd=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1267: { // smhd dmode=1 dbp=1 sfmo=3 rnd=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1268: { // smhd dmode=0 dbp=0 sfmo=0 rnd=4 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1269: { // smhd dmode=1 dbp=0 sfmo=0 rnd=4 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1270: { // smhd dmode=0 dbp=1 sfmo=0 rnd=4 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1271: { // smhd dmode=1 dbp=1 sfmo=0 rnd=4 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1272: { // smhd dmode=0 dbp=0 sfmo=1 rnd=4 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1273: { // smhd dmode=1 dbp=0 sfmo=1 rnd=4 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1274: { // smhd dmode=0 dbp=1 sfmo=1 rnd=4 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1275: { // smhd dmode=1 dbp=1 sfmo=1 rnd=4 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1276: { // smhd dmode=0 dbp=0 sfmo=2 rnd=4 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1277: { // smhd dmode=1 dbp=0 sfmo=2 rnd=4 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1278: { // smhd dmode=0 dbp=1 sfmo=2 rnd=4 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1279: { // smhd dmode=1 dbp=1 sfmo=2 rnd=4 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1280: { // smhd dmode=0 dbp=0 sfmo=3 rnd=4 movm=1
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1281: { // smhd dmode=1 dbp=0 sfmo=3 rnd=4 movm=1
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1282: { // smhd dmode=0 dbp=1 sfmo=3 rnd=4 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1283: { // smhd dmode=1 dbp=1 sfmo=3 rnd=4 movm=1
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1284: { // smhc cmode=0 sfmo=0 rnd=0 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1285: { // smhc cmode=1 sfmo=0 rnd=0 movm=0
	cmem[ca] = macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1286: { // smhc cmode=0 sfmo=1 rnd=0 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1287: { // smhc cmode=1 sfmo=1 rnd=0 movm=0
	cmem[ca] = macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1288: { // smhc cmode=0 sfmo=2 rnd=0 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1289: { // smhc cmode=1 sfmo=2 rnd=0 movm=0
	cmem[ca] = macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1290: { // smhc cmode=0 sfmo=3 rnd=0 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1291: { // smhc cmode=1 sfmo=3 rnd=0 movm=0
	cmem[ca] = macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1292: { // smhc cmode=0 sfmo=0 rnd=1 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1293: { // smhc cmode=1 sfmo=0 rnd=1 movm=0
	cmem[ca] = macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1294: { // smhc cmode=0 sfmo=1 rnd=1 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1295: { // smhc cmode=1 sfmo=1 rnd=1 movm=0
	cmem[ca] = macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1296: { // smhc cmode=0 sfmo=2 rnd=1 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1297: { // smhc cmode=1 sfmo=2 rnd=1 movm=0
	cmem[ca] = macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1298: { // smhc cmode=0 sfmo=3 rnd=1 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1299: { // smhc cmode=1 sfmo=3 rnd=1 movm=0
	cmem[ca] = macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1300: { // smhc cmode=0 sfmo=0 rnd=2 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1301: { // smhc cmode=1 sfmo=0 rnd=2 movm=0
	cmem[ca] = macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1302: { // smhc cmode=0 sfmo=1 rnd=2 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1303: { // smhc cmode=1 sfmo=1 rnd=2 movm=0
	cmem[ca] = macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1304: { // smhc cmode=0 sfmo=2 rnd=2 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1305: { // smhc cmode=1 sfmo=2 rnd=2 movm=0
	cmem[ca] = macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1306: { // smhc cmode=0 sfmo=3 rnd=2 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1307: { // smhc cmode=1 sfmo=3 rnd=2 movm=0
	cmem[ca] = macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1308: { // smhc cmode=0 sfmo=0 rnd=3 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1309: { // smhc cmode=1 sfmo=0 rnd=3 movm=0
	cmem[ca] = macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1310: { // smhc cmode=0 sfmo=1 rnd=3 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1311: { // smhc cmode=1 sfmo=1 rnd=3 movm=0
	cmem[ca] = macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1312: { // smhc cmode=0 sfmo=2 rnd=3 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1313: { // smhc cmode=1 sfmo=2 rnd=3 movm=0
	cmem[ca] = macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1314: { // smhc cmode=0 sfmo=3 rnd=3 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1315: { // smhc cmode=1 sfmo=3 rnd=3 movm=0
	cmem[ca] = macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1316: { // smhc cmode=0 sfmo=0 rnd=4 movm=0
	cmem[i->param] = macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1317: { // smhc cmode=1 sfmo=0 rnd=4 movm=0
	cmem[ca] = macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1318: { // smhc cmode=0 sfmo=1 rnd=4 movm=0
	cmem[i->param] = macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1319: { // smhc cmode=1 sfmo=1 rnd=4 movm=0
	cmem[ca] = macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1320: { // smhc cmode=0 sfmo=2 rnd=4 movm=0
	cmem[i->param] = macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1321: { // smhc cmode=1 sfmo=2 rnd=4 movm=0
	cmem[ca] = macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1322: { // smhc cmode=0 sfmo=3 rnd=4 movm=0
	cmem[i->param] = macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1323: { // smhc cmode=1 sfmo=3 rnd=4 movm=0
	cmem[ca] = macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1324: { // smhc cmode=0 sfmo=0 rnd=0 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1325: { // smhc cmode=1 sfmo=0 rnd=0 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1326: { // smhc cmode=0 sfmo=1 rnd=0 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1327: { // smhc cmode=1 sfmo=1 rnd=0 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1328: { // smhc cmode=0 sfmo=2 rnd=0 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1329: { // smhc cmode=1 sfmo=2 rnd=0 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1330: { // smhc cmode=0 sfmo=3 rnd=0 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1331: { // smhc cmode=1 sfmo=3 rnd=0 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 16;
} TMS_NEXT_ICD;
op_1332: { // smhc cmode=0 sfmo=0 rnd=1 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1333: { // smhc cmode=1 sfmo=0 rnd=1 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1334: { // smhc cmode=0 sfmo=1 rnd=1 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1335: { // smhc cmode=1 sfmo=1 rnd=1 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1336: { // smhc cmode=0 sfmo=2 rnd=1 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1337: { // smhc cmode=1 sfmo=2 rnd=1 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1338: { // smhc cmode=0 sfmo=3 rnd=1 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1339: { // smhc cmode=1 sfmo=3 rnd=1 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1340: { // smhc cmode=0 sfmo=0 rnd=2 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1341: { // smhc cmode=1 sfmo=0 rnd=2 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1342: { // smhc cmode=0 sfmo=1 rnd=2 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1343: { // smhc cmode=1 sfmo=1 rnd=2 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1344: { // smhc cmode=0 sfmo=2 rnd=2 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1345: { // smhc cmode=1 sfmo=2 rnd=2 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1346: { // smhc cmode=0 sfmo=3 rnd=2 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1347: { // smhc cmode=1 sfmo=3 rnd=2 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1348: { // smhc cmode=0 sfmo=0 rnd=3 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1349: { // smhc cmode=1 sfmo=0 rnd=3 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1350: { // smhc cmode=0 sfmo=1 rnd=3 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1351: { // smhc cmode=1 sfmo=1 rnd=3 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1352: { // smhc cmode=0 sfmo=2 rnd=3 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1353: { // smhc cmode=1 sfmo=2 rnd=3 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1354: { // smhc cmode=0 sfmo=3 rnd=3 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1355: { // smhc cmode=1 sfmo=3 rnd=3 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 16;
} TMS_NEXT_ICD;
op_1356: { // smhc cmode=0 sfmo=0 rnd=4 movm=1
	cmem[i->param] = macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1357: { // smhc cmode=1 sfmo=0 rnd=4 movm=1
	cmem[ca] = macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1358: { // smhc cmode=0 sfmo=1 rnd=4 movm=1
	cmem[i->param] = macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1359: { // smhc cmode=1 sfmo=1 rnd=4 movm=1
	cmem[ca] = macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1360: { // smhc cmode=0 sfmo=2 rnd=4 movm=1
	cmem[i->param] = macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1361: { // smhc cmode=1 sfmo=2 rnd=4 movm=1
	cmem[ca] = macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1362: { // smhc cmode=0 sfmo=3 rnd=4 movm=1
	cmem[i->param] = macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1363: { // smhc cmode=1 sfmo=3 rnd=4 movm=1
	cmem[ca] = macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 16;
} TMS_NEXT_ICD;
op_1364: { // slmh dmode=0 dbp=0 sfmo=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1365: { // slmh dmode=1 dbp=0 sfmo=0 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1366: { // slmh dmode=0 dbp=1 sfmo=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1367: { // slmh dmode=1 dbp=1 sfmo=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1368: { // slmh dmode=0 dbp=0 sfmo=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1369: { // slmh dmode=1 dbp=0 sfmo=1 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1370: { // slmh dmode=0 dbp=1 sfmo=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1371: { // slmh dmode=1 dbp=1 sfmo=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1372: { // slmh dmode=0 dbp=0 sfmo=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1373: { // slmh dmode=1 dbp=0 sfmo=2 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1374: { // slmh dmode=0 dbp=1 sfmo=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1375: { // slmh dmode=1 dbp=1 sfmo=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1376: { // slmh dmode=0 dbp=0 sfmo=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1377: { // slmh dmode=1 dbp=0 sfmo=3 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1378: { // slmh dmode=0 dbp=1 sfmo=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1379: { // slmh dmode=1 dbp=1 sfmo=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1380: { // slmh dmode=0 dbp=0 sfmo=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1381: { // slmh dmode=1 dbp=0 sfmo=0 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1382: { // slmh dmode=0 dbp=1 sfmo=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1383: { // slmh dmode=1 dbp=1 sfmo=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1384: { // slmh dmode=0 dbp=0 sfmo=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1385: { // slmh dmode=1 dbp=0 sfmo=1 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1386: { // slmh dmode=0 dbp=1 sfmo=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1387: { // slmh dmode=1 dbp=1 sfmo=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1388: { // slmh dmode=0 dbp=0 sfmo=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1389: { // slmh dmode=1 dbp=0 sfmo=2 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1390: { // slmh dmode=0 dbp=1 sfmo=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1391: { // slmh dmode=1 dbp=1 sfmo=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1392: { // slmh dmode=0 dbp=0 sfmo=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1393: { // slmh dmode=1 dbp=0 sfmo=3 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1394: { // slmh dmode=0 dbp=1 sfmo=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1395: { // slmh dmode=1 dbp=1 sfmo=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3s() >> 24) & 0xffff00;
} TMS_NEXT_ICD;
op_1396: { // slml dmode=0 dbp=0 sfmo=0 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1397: { // slml dmode=1 dbp=0 sfmo=0 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1398: { // slml dmode=0 dbp=1 sfmo=0 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1399: { // slml dmode=1 dbp=1 sfmo=0 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1400: { // slml dmode=0 dbp=0 sfmo=1 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1401: { // slml dmode=1 dbp=0 sfmo=1 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1402: { // slml dmode=0 dbp=1 sfmo=1 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1403: { // slml dmode=1 dbp=1 sfmo=1 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1404: { // slml dmode=0 dbp=0 sfmo=2 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1405: { // slml dmode=1 dbp=0 sfmo=2 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1406: { // slml dmode=0 dbp=1 sfmo=2 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1407: { // slml dmode=1 dbp=1 sfmo=2 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1408: { // slml dmode=0 dbp=0 sfmo=3 movm=0
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1409: { // slml dmode=1 dbp=0 sfmo=3 movm=0
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1410: { // slml dmode=0 dbp=1 sfmo=3 movm=0
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1411: { // slml dmode=1 dbp=1 sfmo=3 movm=0
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1412: { // slml dmode=0 dbp=0 sfmo=0 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1413: { // slml dmode=1 dbp=0 sfmo=0 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1414: { // slml dmode=0 dbp=1 sfmo=0 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1415: { // slml dmode=1 dbp=1 sfmo=0 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1416: { // slml dmode=0 dbp=0 sfmo=1 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1417: { // slml dmode=1 dbp=0 sfmo=1 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1418: { // slml dmode=0 dbp=1 sfmo=1 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1419: { // slml dmode=1 dbp=1 sfmo=1 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1420: { // slml dmode=0 dbp=0 sfmo=2 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1421: { // slml dmode=1 dbp=0 sfmo=2 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1422: { // slml dmode=0 dbp=1 sfmo=2 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1423: { // slml dmode=1 dbp=1 sfmo=2 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1424: { // slml dmode=0 dbp=0 sfmo=3 movm=1
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1425: { // slml dmode=1 dbp=0 sfmo=3 movm=1
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1426: { // slml dmode=0 dbp=1 sfmo=3 movm=1
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1427: { // slml dmode=1 dbp=1 sfmo=3 movm=1
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3s() >> 8) & 0xffffff;
} TMS_NEXT_ICD;
op_1428: { // lcaa sfao=0
	ca = aacc >> 24;
} TMS_NEXT_ICD;
op_1429: { // lcaa sfao=1
	ca = (aacc << 7) >> 24;
} TMS_NEXT_ICD;
op_1430: { // lira sfao=0
	id = aacc >> 24;
} TMS_NEXT_ICD;
op_1431: { // lira sfao=1
	id = (aacc << 7) >> 24;
} TMS_NEXT_ICD;
op_1432: { // ref 
	/* nothing to do */
} TMS_NEXT_ICD;
op_1433: { // srbd dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = xrd;
} TMS_NEXT_ICD;
op_1434: { // srbd dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = xrd;
} TMS_NEXT_ICD;
op_1435: { // srbd dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = xrd;
} TMS_NEXT_ICD;
op_1436: { // srbd dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = xrd;
} TMS_NEXT_ICD;
op_1437: { // dis dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = si[0];
} TMS_NEXT_ICD;
op_1438: { // dis dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = si[0];
} TMS_NEXT_ICD;
op_1439: { // dis dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = si[0];
} TMS_NEXT_ICD;
op_1440: { // dis dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = si[0];
} TMS_NEXT_ICD;
op_1441: { // dis dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = si[1];
} TMS_NEXT_ICD;
op_1442: { // dis dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = si[1];
} TMS_NEXT_ICD;
op_1443: { // dis dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = si[1];
} TMS_NEXT_ICD;
op_1444: { // dis dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = si[1];
} TMS_NEXT_ICD;
op_1445: { // dis dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = si[2];
} TMS_NEXT_ICD;
op_1446: { // dis dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = si[2];
} TMS_NEXT_ICD;
op_1447: { // dis dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = si[2];
} TMS_NEXT_ICD;
op_1448: { // dis dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = si[2];
} TMS_NEXT_ICD;
op_1449: { // dis dmode=0 dbp=0
	dmem0[(i->param + ba0) & 0xff] = si[3];
} TMS_NEXT_ICD;
op_1450: { // dis dmode=1 dbp=0
	dmem0[(id + ba0) & 0xff] = si[3];
} TMS_NEXT_ICD;
op_1451: { // dis dmode=0 dbp=1
	dmem1[(i->param + ba1) & 0x1f] = si[3];
} TMS_NEXT_ICD;
op_1452: { // dis dmode=1 dbp=1
	dmem1[(id + ba1) & 0x1f] = si[3];
} TMS_NEXT_ICD;
op_1453: { // domh sfmo=0 rnd=0 movm=0
	so[0] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1454: { // domh sfmo=1 rnd=0 movm=0
	so[0] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1455: { // domh sfmo=2 rnd=0 movm=0
	so[0] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1456: { // domh sfmo=3 rnd=0 movm=0
	so[0] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1457: { // domh sfmo=0 rnd=1 movm=0
	so[0] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1458: { // domh sfmo=1 rnd=1 movm=0
	so[0] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1459: { // domh sfmo=2 rnd=1 movm=0
	so[0] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1460: { // domh sfmo=3 rnd=1 movm=0
	so[0] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1461: { // domh sfmo=0 rnd=2 movm=0
	so[0] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1462: { // domh sfmo=1 rnd=2 movm=0
	so[0] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1463: { // domh sfmo=2 rnd=2 movm=0
	so[0] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1464: { // domh sfmo=3 rnd=2 movm=0
	so[0] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1465: { // domh sfmo=0 rnd=3 movm=0
	so[0] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1466: { // domh sfmo=1 rnd=3 movm=0
	so[0] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1467: { // domh sfmo=2 rnd=3 movm=0
	so[0] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1468: { // domh sfmo=3 rnd=3 movm=0
	so[0] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1469: { // domh sfmo=0 rnd=4 movm=0
	so[0] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1470: { // domh sfmo=1 rnd=4 movm=0
	so[0] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1471: { // domh sfmo=2 rnd=4 movm=0
	so[0] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1472: { // domh sfmo=3 rnd=4 movm=0
	so[0] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1473: { // domh sfmo=0 rnd=0 movm=1
	so[0] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1474: { // domh sfmo=1 rnd=0 movm=1
	so[0] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1475: { // domh sfmo=2 rnd=0 movm=1
	so[0] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1476: { // domh sfmo=3 rnd=0 movm=1
	so[0] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1477: { // domh sfmo=0 rnd=1 movm=1
	so[0] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1478: { // domh sfmo=1 rnd=1 movm=1
	so[0] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1479: { // domh sfmo=2 rnd=1 movm=1
	so[0] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1480: { // domh sfmo=3 rnd=1 movm=1
	so[0] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1481: { // domh sfmo=0 rnd=2 movm=1
	so[0] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1482: { // domh sfmo=1 rnd=2 movm=1
	so[0] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1483: { // domh sfmo=2 rnd=2 movm=1
	so[0] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1484: { // domh sfmo=3 rnd=2 movm=1
	so[0] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1485: { // domh sfmo=0 rnd=3 movm=1
	so[0] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1486: { // domh sfmo=1 rnd=3 movm=1
	so[0] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1487: { // domh sfmo=2 rnd=3 movm=1
	so[0] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1488: { // domh sfmo=3 rnd=3 movm=1
	so[0] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1489: { // domh sfmo=0 rnd=4 movm=1
	so[0] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1490: { // domh sfmo=1 rnd=4 movm=1
	so[0] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1491: { // domh sfmo=2 rnd=4 movm=1
	so[0] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1492: { // domh sfmo=3 rnd=4 movm=1
	so[0] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1493: { // domh sfmo=0 rnd=0 movm=0
	so[1] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1494: { // domh sfmo=1 rnd=0 movm=0
	so[1] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1495: { // domh sfmo=2 rnd=0 movm=0
	so[1] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1496: { // domh sfmo=3 rnd=0 movm=0
	so[1] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1497: { // domh sfmo=0 rnd=1 movm=0
	so[1] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1498: { // domh sfmo=1 rnd=1 movm=0
	so[1] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1499: { // domh sfmo=2 rnd=1 movm=0
	so[1] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1500: { // domh sfmo=3 rnd=1 movm=0
	so[1] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1501: { // domh sfmo=0 rnd=2 movm=0
	so[1] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1502: { // domh sfmo=1 rnd=2 movm=0
	so[1] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1503: { // domh sfmo=2 rnd=2 movm=0
	so[1] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1504: { // domh sfmo=3 rnd=2 movm=0
	so[1] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1505: { // domh sfmo=0 rnd=3 movm=0
	so[1] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1506: { // domh sfmo=1 rnd=3 movm=0
	so[1] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1507: { // domh sfmo=2 rnd=3 movm=0
	so[1] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1508: { // domh sfmo=3 rnd=3 movm=0
	so[1] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1509: { // domh sfmo=0 rnd=4 movm=0
	so[1] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1510: { // domh sfmo=1 rnd=4 movm=0
	so[1] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1511: { // domh sfmo=2 rnd=4 movm=0
	so[1] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1512: { // domh sfmo=3 rnd=4 movm=0
	so[1] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1513: { // domh sfmo=0 rnd=0 movm=1
	so[1] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1514: { // domh sfmo=1 rnd=0 movm=1
	so[1] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1515: { // domh sfmo=2 rnd=0 movm=1
	so[1] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1516: { // domh sfmo=3 rnd=0 movm=1
	so[1] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1517: { // domh sfmo=0 rnd=1 movm=1
	so[1] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1518: { // domh sfmo=1 rnd=1 movm=1
	so[1] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1519: { // domh sfmo=2 rnd=1 movm=1
	so[1] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1520: { // domh sfmo=3 rnd=1 movm=1
	so[1] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1521: { // domh sfmo=0 rnd=2 movm=1
	so[1] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1522: { // domh sfmo=1 rnd=2 movm=1
	so[1] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1523: { // domh sfmo=2 rnd=2 movm=1
	so[1] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1524: { // domh sfmo=3 rnd=2 movm=1
	so[1] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1525: { // domh sfmo=0 rnd=3 movm=1
	so[1] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1526: { // domh sfmo=1 rnd=3 movm=1
	so[1] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1527: { // domh sfmo=2 rnd=3 movm=1
	so[1] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1528: { // domh sfmo=3 rnd=3 movm=1
	so[1] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1529: { // domh sfmo=0 rnd=4 movm=1
	so[1] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1530: { // domh sfmo=1 rnd=4 movm=1
	so[1] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1531: { // domh sfmo=2 rnd=4 movm=1
	so[1] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1532: { // domh sfmo=3 rnd=4 movm=1
	so[1] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1533: { // domh sfmo=0 rnd=0 movm=0
	so[2] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1534: { // domh sfmo=1 rnd=0 movm=0
	so[2] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1535: { // domh sfmo=2 rnd=0 movm=0
	so[2] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1536: { // domh sfmo=3 rnd=0 movm=0
	so[2] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1537: { // domh sfmo=0 rnd=1 movm=0
	so[2] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1538: { // domh sfmo=1 rnd=1 movm=0
	so[2] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1539: { // domh sfmo=2 rnd=1 movm=0
	so[2] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1540: { // domh sfmo=3 rnd=1 movm=0
	so[2] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1541: { // domh sfmo=0 rnd=2 movm=0
	so[2] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1542: { // domh sfmo=1 rnd=2 movm=0
	so[2] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1543: { // domh sfmo=2 rnd=2 movm=0
	so[2] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1544: { // domh sfmo=3 rnd=2 movm=0
	so[2] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1545: { // domh sfmo=0 rnd=3 movm=0
	so[2] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1546: { // domh sfmo=1 rnd=3 movm=0
	so[2] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1547: { // domh sfmo=2 rnd=3 movm=0
	so[2] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1548: { // domh sfmo=3 rnd=3 movm=0
	so[2] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1549: { // domh sfmo=0 rnd=4 movm=0
	so[2] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1550: { // domh sfmo=1 rnd=4 movm=0
	so[2] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1551: { // domh sfmo=2 rnd=4 movm=0
	so[2] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1552: { // domh sfmo=3 rnd=4 movm=0
	so[2] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1553: { // domh sfmo=0 rnd=0 movm=1
	so[2] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1554: { // domh sfmo=1 rnd=0 movm=1
	so[2] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1555: { // domh sfmo=2 rnd=0 movm=1
	so[2] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1556: { // domh sfmo=3 rnd=0 movm=1
	so[2] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1557: { // domh sfmo=0 rnd=1 movm=1
	so[2] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1558: { // domh sfmo=1 rnd=1 movm=1
	so[2] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1559: { // domh sfmo=2 rnd=1 movm=1
	so[2] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1560: { // domh sfmo=3 rnd=1 movm=1
	so[2] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1561: { // domh sfmo=0 rnd=2 movm=1
	so[2] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1562: { // domh sfmo=1 rnd=2 movm=1
	so[2] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1563: { // domh sfmo=2 rnd=2 movm=1
	so[2] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1564: { // domh sfmo=3 rnd=2 movm=1
	so[2] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1565: { // domh sfmo=0 rnd=3 movm=1
	so[2] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1566: { // domh sfmo=1 rnd=3 movm=1
	so[2] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1567: { // domh sfmo=2 rnd=3 movm=1
	so[2] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1568: { // domh sfmo=3 rnd=3 movm=1
	so[2] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1569: { // domh sfmo=0 rnd=4 movm=1
	so[2] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1570: { // domh sfmo=1 rnd=4 movm=1
	so[2] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1571: { // domh sfmo=2 rnd=4 movm=1
	so[2] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1572: { // domh sfmo=3 rnd=4 movm=1
	so[2] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1573: { // domh sfmo=0 rnd=0 movm=0
	so[3] = (macc_to_output_0(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1574: { // domh sfmo=1 rnd=0 movm=0
	so[3] = (macc_to_output_1(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1575: { // domh sfmo=2 rnd=0 movm=0
	so[3] = (macc_to_output_2(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1576: { // domh sfmo=3 rnd=0 movm=0
	so[3] = (macc_to_output_3(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1577: { // domh sfmo=0 rnd=1 movm=0
	so[3] = (macc_to_output_0(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1578: { // domh sfmo=1 rnd=1 movm=0
	so[3] = (macc_to_output_1(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1579: { // domh sfmo=2 rnd=1 movm=0
	so[3] = (macc_to_output_2(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1580: { // domh sfmo=3 rnd=1 movm=0
	so[3] = (macc_to_output_3(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1581: { // domh sfmo=0 rnd=2 movm=0
	so[3] = (macc_to_output_0(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1582: { // domh sfmo=1 rnd=2 movm=0
	so[3] = (macc_to_output_1(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1583: { // domh sfmo=2 rnd=2 movm=0
	so[3] = (macc_to_output_2(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1584: { // domh sfmo=3 rnd=2 movm=0
	so[3] = (macc_to_output_3(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1585: { // domh sfmo=0 rnd=3 movm=0
	so[3] = (macc_to_output_0(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1586: { // domh sfmo=1 rnd=3 movm=0
	so[3] = (macc_to_output_1(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1587: { // domh sfmo=2 rnd=3 movm=0
	so[3] = (macc_to_output_2(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1588: { // domh sfmo=3 rnd=3 movm=0
	so[3] = (macc_to_output_3(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1589: { // domh sfmo=0 rnd=4 movm=0
	so[3] = (macc_to_output_0(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1590: { // domh sfmo=1 rnd=4 movm=0
	so[3] = (macc_to_output_1(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1591: { // domh sfmo=2 rnd=4 movm=0
	so[3] = (macc_to_output_2(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1592: { // domh sfmo=3 rnd=4 movm=0
	so[3] = (macc_to_output_3(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1593: { // domh sfmo=0 rnd=0 movm=1
	so[3] = (macc_to_output_0s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1594: { // domh sfmo=1 rnd=0 movm=1
	so[3] = (macc_to_output_1s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1595: { // domh sfmo=2 rnd=0 movm=1
	so[3] = (macc_to_output_2s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1596: { // domh sfmo=3 rnd=0 movm=1
	so[3] = (macc_to_output_3s(0x0000000000000000ULL, 0xffffffffffffffffULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1597: { // domh sfmo=0 rnd=1 movm=1
	so[3] = (macc_to_output_0s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1598: { // domh sfmo=1 rnd=1 movm=1
	so[3] = (macc_to_output_1s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1599: { // domh sfmo=2 rnd=1 movm=1
	so[3] = (macc_to_output_2s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1600: { // domh sfmo=3 rnd=1 movm=1
	so[3] = (macc_to_output_3s(0x0000000000008000ULL, 0xffffffffffff0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1601: { // domh sfmo=0 rnd=2 movm=1
	so[3] = (macc_to_output_0s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1602: { // domh sfmo=1 rnd=2 movm=1
	so[3] = (macc_to_output_1s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1603: { // domh sfmo=2 rnd=2 movm=1
	so[3] = (macc_to_output_2s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1604: { // domh sfmo=3 rnd=2 movm=1
	so[3] = (macc_to_output_3s(0x0000000000800000ULL, 0xffffffffff000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1605: { // domh sfmo=0 rnd=3 movm=1
	so[3] = (macc_to_output_0s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1606: { // domh sfmo=1 rnd=3 movm=1
	so[3] = (macc_to_output_1s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1607: { // domh sfmo=2 rnd=3 movm=1
	so[3] = (macc_to_output_2s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1608: { // domh sfmo=3 rnd=3 movm=1
	so[3] = (macc_to_output_3s(0x0000000000020000ULL, 0xfffffffffffc0000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1609: { // domh sfmo=0 rnd=4 movm=1
	so[3] = (macc_to_output_0s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1610: { // domh sfmo=1 rnd=4 movm=1
	so[3] = (macc_to_output_1s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1611: { // domh sfmo=2 rnd=4 movm=1
	so[3] = (macc_to_output_2s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1612: { // domh sfmo=3 rnd=4 movm=1
	so[3] = (macc_to_output_3s(0x0000000080000000ULL, 0xffffffff00000000ULL) >> 24) & 0xffffff;
} TMS_NEXT_ICD;
op_1613: { // lpc cmode=0
	if(sti & S_HOST)
	goto next_icd;
	c = get_cmem(i->param);
	host[0] = c >> 24;
	host[1] = c >> 16;
	host[2] = c >> 8;
	host[3] = c;
	hidx = 0;
	sti |= S_HOST;
	update_dready();
} TMS_NEXT_ICD;
op_1614: { // lpc cmode=1
	if(sti & S_HOST)
	goto next_icd;
	c = get_cmem(ca);
	host[0] = c >> 24;
	host[1] = c >> 16;
	host[2] = c >> 8;
	host[3] = c;
	hidx = 0;
	sti |= S_HOST;
	update_dready();
} TMS_NEXT_ICD;
op_1615: { // rmov 
	st1 &= ~ST1_MOV;
} TMS_NEXT_ICD;
op_1616: { // raom 
	/* Undocumented instruction, reset ALU saturation flag */
	st1 &= ~ST1_AOVM;
} TMS_NEXT_ICD;
op_1617: { // saom 
	/* Undocumented instruction, sets ALU saturation flag */
	st1 |= ST1_AOVM;
} TMS_NEXT_ICD;
op_1618: { // rmom 
	st1 &= ~ST1_MOVM;
} TMS_NEXT_ICD;
op_1619: { // smom 
	st1 |= ST1_MOVM;
} TMS_NEXT_ICD;
op_1620: { // ldpk 
	st1 &= ~ST1_DBP;
} TMS_NEXT_ICD;
op_1621: { // ldpk 
	st1 |= ST1_DBP;
} TMS_NEXT_ICD;
op_1622: { // scrm 
	st1 = (st1 & ~ST1_CRM) | (0 << ST1_CRM_SHIFT);
} TMS_NEXT_ICD;
op_1623: { // scrm 
	st1 = (st1 & ~ST1_CRM) | (1 << ST1_CRM_SHIFT);
} TMS_NEXT_ICD;
op_1624: { // scrm 
	st1 = (st1 & ~ST1_CRM) | (2 << ST1_CRM_SHIFT);
} TMS_NEXT_ICD;
op_1625: { // scrm 
	st1 = (st1 & ~ST1_CRM) | (3 << ST1_CRM_SHIFT);
} TMS_NEXT_ICD;
op_1626: { // sfao 
	st1 &= ~ST1_SFAO;
} TMS_NEXT_ICD;
op_1627: { // sfao 
	st1 |= ST1_SFAO;
} TMS_NEXT_ICD;
op_1628: { // sfai 
	st1 &= ~ST1_SFAI;
} TMS_NEXT_ICD;
op_1629: { // sfai 
	st1 |= ST1_SFAI;
} TMS_NEXT_ICD;
op_1630: { // sfma 
	st1 = (st1 & ~ST1_SFMA) | (0 << ST1_SFMA_SHIFT);
} TMS_NEXT_ICD;
op_1631: { // sfma 
	st1 = (st1 & ~ST1_SFMA) | (1 << ST1_SFMA_SHIFT);
} TMS_NEXT_ICD;
op_1632: { // sfma 
	st1 = (st1 & ~ST1_SFMA) | (2 << ST1_SFMA_SHIFT);
} TMS_NEXT_ICD;
op_1633: { // sfma 
	st1 = (st1 & ~ST1_SFMA) | (3 << ST1_SFMA_SHIFT);
} TMS_NEXT_ICD;
op_1634: { // sfmo 
	st1 = (st1 & ~ST1_SFMO) | (0 << ST1_SFMO_SHIFT);
} TMS_NEXT_ICD;
op_1635: { // sfmo 
	st1 = (st1 & ~ST1_SFMO) | (1 << ST1_SFMO_SHIFT);
} TMS_NEXT_ICD;
op_1636: { // sfmo 
	st1 = (st1 & ~ST1_SFMO) | (2 << ST1_SFMO_SHIFT);
} TMS_NEXT_ICD;
op_1637: { // sfmo 
	st1 = (st1 & ~ST1_SFMO) | (3 << ST1_SFMO_SHIFT);
} TMS_NEXT_ICD;
op_1638: { // rnd 
	st1 = (st1 & ~ST1_RND) | (0 << ST1_RND_SHIFT);
} TMS_NEXT_ICD;
op_1639: { // rnd 
	st1 = (st1 & ~ST1_RND) | (1 << ST1_RND_SHIFT);
} TMS_NEXT_ICD;
op_1640: { // rnd 
	st1 = (st1 & ~ST1_RND) | (2 << ST1_RND_SHIFT);
} TMS_NEXT_ICD;
op_1641: { // rnd 
	st1 = (st1 & ~ST1_RND) | (3 << ST1_RND_SHIFT);
} TMS_NEXT_ICD;
op_1642: { // rnd 
	st1 = (st1 & ~ST1_RND) | (4 << ST1_RND_SHIFT);
} TMS_NEXT_ICD;
op_1643: { // rnd 
	st1 = (st1 & ~ST1_RND) | (5 << ST1_RND_SHIFT);
} TMS_NEXT_ICD;
op_1644: { // rnd 
	st1 = (st1 & ~ST1_RND) | (6 << ST1_RND_SHIFT);
} TMS_NEXT_ICD;
op_1645: { // rnd 
	st1 = (st1 & ~ST1_RND) | (7 << ST1_RND_SHIFT);
} TMS_NEXT_ICD;
op_1646: { // idle 
	sti |= S_IDLE;
} TMS_NEXT_ICD;
op_1647: { // rptk 
	rptc_next = i->param;
} TMS_NEXT_ICD;
op_1648: { // lcak 
	ca = i->param;
} TMS_NEXT_ICD;
op_1649: { // lirk 
	id = i->param;
} TMS_NEXT_ICD;
op_1650: { // lcac 
	if(((int32_t)aacc) >= 0)
	ca = i->param;
} TMS_NEXT_ICD;
op_1651: { // b 
	pc = i->param;
  sti |= S_BRANCH;
} TMS_NEXT_ICD;
op_1652: { // bgz 
	if(((int32_t)aacc) > 0) {
	pc = i->param;
  sti |= S_BRANCH;
	}
} TMS_NEXT_ICD;
op_1653: { // blz 
	if(((int32_t)aacc) < 0) {
	pc = i->param;
  sti |= S_BRANCH;
	}
} TMS_NEXT_ICD;
op_1654: { // bnz 
	if(aacc) {
	pc = i->param;
  sti |= S_BRANCH;
	}
} TMS_NEXT_ICD;
op_1655: { // bv 
	if(st1 & ST1_AOV) {
	st1 &= ~ST1_AOV;
	pc = i->param;
  sti |= S_BRANCH;
	}
} TMS_NEXT_ICD;
#endif

#ifdef CINTRPTABLE
	&&op_4, &&op_5, &&op_6, &&op_7, &&op_8, &&op_9, &&op_10, &&op_11,
	&&op_12, &&op_13, &&op_14, &&op_15, &&op_16, &&op_17, &&op_18, &&op_19,
	&&op_20, &&op_21, &&op_22, &&op_23, &&op_24, &&op_25, &&op_26, &&op_27,
	&&op_28, &&op_29, &&op_30, &&op_31, &&op_32, &&op_33, &&op_34, &&op_35,
	&&op_36, &&op_37, &&op_38, &&op_39, &&op_40, &&op_41, &&op_42, &&op_43,
	&&op_44, &&op_45, &&op_46, &&op_47, &&op_48, &&op_49, &&op_50, &&op_51,
	&&op_52, &&op_53, &&op_54, &&op_55, &&op_56, &&op_57, &&op_58, &&op_59,
	&&op_60, &&op_61, &&op_62, &&op_63, &&op_64, &&op_65, &&op_66, &&op_67,
	&&op_68, &&op_69, &&op_70, &&op_71, &&op_72, &&op_73, &&op_74, &&op_75,
	&&op_76, &&op_77, &&op_78, &&op_79, &&op_80, &&op_81, &&op_82, &&op_83,
	&&op_84, &&op_85, &&op_86, &&op_87, &&op_88, &&op_89, &&op_90, &&op_91,
	&&op_92, &&op_93, &&op_94, &&op_95, &&op_96, &&op_97, &&op_98, &&op_99,
	&&op_100, &&op_101, &&op_102, &&op_103, &&op_104, &&op_105, &&op_106, &&op_107,
	&&op_108, &&op_109, &&op_110, &&op_111, &&op_112, &&op_113, &&op_114, &&op_115,
	&&op_116, &&op_117, &&op_118, &&op_119, &&op_120, &&op_121, &&op_122, &&op_123,
	&&op_124, &&op_125, &&op_126, &&op_127, &&op_128, &&op_129, &&op_130, &&op_131,
	&&op_132, &&op_133, &&op_134, &&op_135, &&op_136, &&op_137, &&op_138, &&op_139,
	&&op_140, &&op_141, &&op_142, &&op_143, &&op_144, &&op_145, &&op_146, &&op_147,
	&&op_148, &&op_149, &&op_150, &&op_151, &&op_152, &&op_153, &&op_154, &&op_155,
	&&op_156, &&op_157, &&op_158, &&op_159, &&op_160, &&op_161, &&op_162, &&op_163,
	&&op_164, &&op_165, &&op_166, &&op_167, &&op_168, &&op_169, &&op_170, &&op_171,
	&&op_172, &&op_173, &&op_174, &&op_175, &&op_176, &&op_177, &&op_178, &&op_179,
	&&op_180, &&op_181, &&op_182, &&op_183, &&op_184, &&op_185, &&op_186, &&op_187,
	&&op_188, &&op_189, &&op_190, &&op_191, &&op_192, &&op_193, &&op_194, &&op_195,
	&&op_196, &&op_197, &&op_198, &&op_199, &&op_200, &&op_201, &&op_202, &&op_203,
	&&op_204, &&op_205, &&op_206, &&op_207, &&op_208, &&op_209, &&op_210, &&op_211,
	&&op_212, &&op_213, &&op_214, &&op_215, &&op_216, &&op_217, &&op_218, &&op_219,
	&&op_220, &&op_221, &&op_222, &&op_223, &&op_224, &&op_225, &&op_226, &&op_227,
	&&op_228, &&op_229, &&op_230, &&op_231, &&op_232, &&op_233, &&op_234, &&op_235,
	&&op_236, &&op_237, &&op_238, &&op_239, &&op_240, &&op_241, &&op_242, &&op_243,
	&&op_244, &&op_245, &&op_246, &&op_247, &&op_248, &&op_249, &&op_250, &&op_251,
	&&op_252, &&op_253, &&op_254, &&op_255, &&op_256, &&op_257, &&op_258, &&op_259,
	&&op_260, &&op_261, &&op_262, &&op_263, &&op_264, &&op_265, &&op_266, &&op_267,
	&&op_268, &&op_269, &&op_270, &&op_271, &&op_272, &&op_273, &&op_274, &&op_275,
	&&op_276, &&op_277, &&op_278, &&op_279, &&op_280, &&op_281, &&op_282, &&op_283,
	&&op_284, &&op_285, &&op_286, &&op_287, &&op_288, &&op_289, &&op_290, &&op_291,
	&&op_292, &&op_293, &&op_294, &&op_295, &&op_296, &&op_297, &&op_298, &&op_299,
	&&op_300, &&op_301, &&op_302, &&op_303, &&op_304, &&op_305, &&op_306, &&op_307,
	&&op_308, &&op_309, &&op_310, &&op_311, &&op_312, &&op_313, &&op_314, &&op_315,
	&&op_316, &&op_317, &&op_318, &&op_319, &&op_320, &&op_321, &&op_322, &&op_323,
	&&op_324, &&op_325, &&op_326, &&op_327, &&op_328, &&op_329, &&op_330, &&op_331,
	&&op_332, &&op_333, &&op_334, &&op_335, &&op_336, &&op_337, &&op_338, &&op_339,
	&&op_340, &&op_341, &&op_342, &&op_343, &&op_344, &&op_345, &&op_346, &&op_347,
	&&op_348, &&op_349, &&op_350, &&op_351, &&op_352, &&op_353, &&op_354, &&op_355,
	&&op_356, &&op_357, &&op_358, &&op_359, &&op_360, &&op_361, &&op_362, &&op_363,
	&&op_364, &&op_365, &&op_366, &&op_367, &&op_368, &&op_369, &&op_370, &&op_371,
	&&op_372, &&op_373, &&op_374, &&op_375, &&op_376, &&op_377, &&op_378, &&op_379,
	&&op_380, &&op_381, &&op_382, &&op_383, &&op_384, &&op_385, &&op_386, &&op_387,
	&&op_388, &&op_389, &&op_390, &&op_391, &&op_392, &&op_393, &&op_394, &&op_395,
	&&op_396, &&op_397, &&op_398, &&op_399, &&op_400, &&op_401, &&op_402, &&op_403,
	&&op_404, &&op_405, &&op_406, &&op_407, &&op_408, &&op_409, &&op_410, &&op_411,
	&&op_412, &&op_413, &&op_414, &&op_415, &&op_416, &&op_417, &&op_418, &&op_419,
	&&op_420, &&op_421, &&op_422, &&op_423, &&op_424, &&op_425, &&op_426, &&op_427,
	&&op_428, &&op_429, &&op_430, &&op_431, &&op_432, &&op_433, &&op_434, &&op_435,
	&&op_436, &&op_437, &&op_438, &&op_439, &&op_440, &&op_441, &&op_442, &&op_443,
	&&op_444, &&op_445, &&op_446, &&op_447, &&op_448, &&op_449, &&op_450, &&op_451,
	&&op_452, &&op_453, &&op_454, &&op_455, &&op_456, &&op_457, &&op_458, &&op_459,
	&&op_460, &&op_461, &&op_462, &&op_463, &&op_464, &&op_465, &&op_466, &&op_467,
	&&op_468, &&op_469, &&op_470, &&op_471, &&op_472, &&op_473, &&op_474, &&op_475,
	&&op_476, &&op_477, &&op_478, &&op_479, &&op_480, &&op_481, &&op_482, &&op_483,
	&&op_484, &&op_485, &&op_486, &&op_487, &&op_488, &&op_489, &&op_490, &&op_491,
	&&op_492, &&op_493, &&op_494, &&op_495, &&op_496, &&op_497, &&op_498, &&op_499,
	&&op_500, &&op_501, &&op_502, &&op_503, &&op_504, &&op_505, &&op_506, &&op_507,
	&&op_508, &&op_509, &&op_510, &&op_511, &&op_512, &&op_513, &&op_514, &&op_515,
	&&op_516, &&op_517, &&op_518, &&op_519, &&op_520, &&op_521, &&op_522, &&op_523,
	&&op_524, &&op_525, &&op_526, &&op_527, &&op_528, &&op_529, &&op_530, &&op_531,
	&&op_532, &&op_533, &&op_534, &&op_535, &&op_536, &&op_537, &&op_538, &&op_539,
	&&op_540, &&op_541, &&op_542, &&op_543, &&op_544, &&op_545, &&op_546, &&op_547,
	&&op_548, &&op_549, &&op_550, &&op_551, &&op_552, &&op_553, &&op_554, &&op_555,
	&&op_556, &&op_557, &&op_558, &&op_559, &&op_560, &&op_561, &&op_562, &&op_563,
	&&op_564, &&op_565, &&op_566, &&op_567, &&op_568, &&op_569, &&op_570, &&op_571,
	&&op_572, &&op_573, &&op_574, &&op_575, &&op_576, &&op_577, &&op_578, &&op_579,
	&&op_580, &&op_581, &&op_582, &&op_583, &&op_584, &&op_585, &&op_586, &&op_587,
	&&op_588, &&op_589, &&op_590, &&op_591, &&op_592, &&op_593, &&op_594, &&op_595,
	&&op_596, &&op_597, &&op_598, &&op_599, &&op_600, &&op_601, &&op_602, &&op_603,
	&&op_604, &&op_605, &&op_606, &&op_607, &&op_608, &&op_609, &&op_610, &&op_611,
	&&op_612, &&op_613, &&op_614, &&op_615, &&op_616, &&op_617, &&op_618, &&op_619,
	&&op_620, &&op_621, &&op_622, &&op_623, &&op_624, &&op_625, &&op_626, &&op_627,
	&&op_628, &&op_629, &&op_630, &&op_631, &&op_632, &&op_633, &&op_634, &&op_635,
	&&op_636, &&op_637, &&op_638, &&op_639, &&op_640, &&op_641, &&op_642, &&op_643,
	&&op_644, &&op_645, &&op_646, &&op_647, &&op_648, &&op_649, &&op_650, &&op_651,
	&&op_652, &&op_653, &&op_654, &&op_655, &&op_656, &&op_657, &&op_658, &&op_659,
	&&op_660, &&op_661, &&op_662, &&op_663, &&op_664, &&op_665, &&op_666, &&op_667,
	&&op_668, &&op_669, &&op_670, &&op_671, &&op_672, &&op_673, &&op_674, &&op_675,
	&&op_676, &&op_677, &&op_678, &&op_679, &&op_680, &&op_681, &&op_682, &&op_683,
	&&op_684, &&op_685, &&op_686, &&op_687, &&op_688, &&op_689, &&op_690, &&op_691,
	&&op_692, &&op_693, &&op_694, &&op_695, &&op_696, &&op_697, &&op_698, &&op_699,
	&&op_700, &&op_701, &&op_702, &&op_703, &&op_704, &&op_705, &&op_706, &&op_707,
	&&op_708, &&op_709, &&op_710, &&op_711, &&op_712, &&op_713, &&op_714, &&op_715,
	&&op_716, &&op_717, &&op_718, &&op_719, &&op_720, &&op_721, &&op_722, &&op_723,
	&&op_724, &&op_725, &&op_726, &&op_727, &&op_728, &&op_729, &&op_730, &&op_731,
	&&op_732, &&op_733, &&op_734, &&op_735, &&op_736, &&op_737, &&op_738, &&op_739,
	&&op_740, &&op_741, &&op_742, &&op_743, &&op_744, &&op_745, &&op_746, &&op_747,
	&&op_748, &&op_749, &&op_750, &&op_751, &&op_752, &&op_753, &&op_754, &&op_755,
	&&op_756, &&op_757, &&op_758, &&op_759, &&op_760, &&op_761, &&op_762, &&op_763,
	&&op_764, &&op_765, &&op_766, &&op_767, &&op_768, &&op_769, &&op_770, &&op_771,
	&&op_772, &&op_773, &&op_774, &&op_775, &&op_776, &&op_777, &&op_778, &&op_779,
	&&op_780, &&op_781, &&op_782, &&op_783, &&op_784, &&op_785, &&op_786, &&op_787,
	&&op_788, &&op_789, &&op_790, &&op_791, &&op_792, &&op_793, &&op_794, &&op_795,
	&&op_796, &&op_797, &&op_798, &&op_799, &&op_800, &&op_801, &&op_802, &&op_803,
	&&op_804, &&op_805, &&op_806, &&op_807, &&op_808, &&op_809, &&op_810, &&op_811,
	&&op_812, &&op_813, &&op_814, &&op_815, &&op_816, &&op_817, &&op_818, &&op_819,
	&&op_820, &&op_821, &&op_822, &&op_823, &&op_824, &&op_825, &&op_826, &&op_827,
	&&op_828, &&op_829, &&op_830, &&op_831, &&op_832, &&op_833, &&op_834, &&op_835,
	&&op_836, &&op_837, &&op_838, &&op_839, &&op_840, &&op_841, &&op_842, &&op_843,
	&&op_844, &&op_845, &&op_846, &&op_847, &&op_848, &&op_849, &&op_850, &&op_851,
	&&op_852, &&op_853, &&op_854, &&op_855, &&op_856, &&op_857, &&op_858, &&op_859,
	&&op_860, &&op_861, &&op_862, &&op_863, &&op_864, &&op_865, &&op_866, &&op_867,
	&&op_868, &&op_869, &&op_870, &&op_871, &&op_872, &&op_873, &&op_874, &&op_875,
	&&op_876, &&op_877, &&op_878, &&op_879, &&op_880, &&op_881, &&op_882, &&op_883,
	&&op_884, &&op_885, &&op_886, &&op_887, &&op_888, &&op_889, &&op_890, &&op_891,
	&&op_892, &&op_893, &&op_894, &&op_895, &&op_896, &&op_897, &&op_898, &&op_899,
	&&op_900, &&op_901, &&op_902, &&op_903, &&op_904, &&op_905, &&op_906, &&op_907,
	&&op_908, &&op_909, &&op_910, &&op_911, &&op_912, &&op_913, &&op_914, &&op_915,
	&&op_916, &&op_917, &&op_918, &&op_919, &&op_920, &&op_921, &&op_922, &&op_923,
	&&op_924, &&op_925, &&op_926, &&op_927, &&op_928, &&op_929, &&op_930, &&op_931,
	&&op_932, &&op_933, &&op_934, &&op_935, &&op_936, &&op_937, &&op_938, &&op_939,
	&&op_940, &&op_941, &&op_942, &&op_943, &&op_944, &&op_945, &&op_946, &&op_947,
	&&op_948, &&op_949, &&op_950, &&op_951, &&op_952, &&op_953, &&op_954, &&op_955,
	&&op_956, &&op_957, &&op_958, &&op_959, &&op_960, &&op_961, &&op_962, &&op_963,
	&&op_964, &&op_965, &&op_966, &&op_967, &&op_968, &&op_969, &&op_970, &&op_971,
	&&op_972, &&op_973, &&op_974, &&op_975, &&op_976, &&op_977, &&op_978, &&op_979,
	&&op_980, &&op_981, &&op_982, &&op_983, &&op_984, &&op_985, &&op_986, &&op_987,
	&&op_988, &&op_989, &&op_990, &&op_991, &&op_992, &&op_993, &&op_994, &&op_995,
	&&op_996, &&op_997, &&op_998, &&op_999, &&op_1000, &&op_1001, &&op_1002, &&op_1003,
	&&op_1004, &&op_1005, &&op_1006, &&op_1007, &&op_1008, &&op_1009, &&op_1010, &&op_1011,
	&&op_1012, &&op_1013, &&op_1014, &&op_1015, &&op_1016, &&op_1017, &&op_1018, &&op_1019,
	&&op_1020, &&op_1021, &&op_1022, &&op_1023, &&op_1024, &&op_1025, &&op_1026, &&op_1027,
	&&op_1028, &&op_1029, &&op_1030, &&op_1031, &&op_1032, &&op_1033, &&op_1034, &&op_1035,
	&&op_1036, &&op_1037, &&op_1038, &&op_1039, &&op_1040, &&op_1041, &&op_1042, &&op_1043,
	&&op_1044, &&op_1045, &&op_1046, &&op_1047, &&op_1048, &&op_1049, &&op_1050, &&op_1051,
	&&op_1052, &&op_1053, &&op_1054, &&op_1055, &&op_1056, &&op_1057, &&op_1058, &&op_1059,
	&&op_1060, &&op_1061, &&op_1062, &&op_1063, &&op_1064, &&op_1065, &&op_1066, &&op_1067,
	&&op_1068, &&op_1069, &&op_1070, &&op_1071, &&op_1072, &&op_1073, &&op_1074, &&op_1075,
	&&op_1076, &&op_1077, &&op_1078, &&op_1079, &&op_1080, &&op_1081, &&op_1082, &&op_1083,
	&&op_1084, &&op_1085, &&op_1086, &&op_1087, &&op_1088, &&op_1089, &&op_1090, &&op_1091,
	&&op_1092, &&op_1093, &&op_1094, &&op_1095, &&op_1096, &&op_1097, &&op_1098, &&op_1099,
	&&op_1100, &&op_1101, &&op_1102, &&op_1103, &&op_1104, &&op_1105, &&op_1106, &&op_1107,
	&&op_1108, &&op_1109, &&op_1110, &&op_1111, &&op_1112, &&op_1113, &&op_1114, &&op_1115,
	&&op_1116, &&op_1117, &&op_1118, &&op_1119, &&op_1120, &&op_1121, &&op_1122, &&op_1123,
	&&op_1124, &&op_1125, &&op_1126, &&op_1127, &&op_1128, &&op_1129, &&op_1130, &&op_1131,
	&&op_1132, &&op_1133, &&op_1134, &&op_1135, &&op_1136, &&op_1137, &&op_1138, &&op_1139,
	&&op_1140, &&op_1141, &&op_1142, &&op_1143, &&op_1144, &&op_1145, &&op_1146, &&op_1147,
	&&op_1148, &&op_1149, &&op_1150, &&op_1151, &&op_1152, &&op_1153, &&op_1154, &&op_1155,
	&&op_1156, &&op_1157, &&op_1158, &&op_1159, &&op_1160, &&op_1161, &&op_1162, &&op_1163,
	&&op_1164, &&op_1165, &&op_1166, &&op_1167, &&op_1168, &&op_1169, &&op_1170, &&op_1171,
	&&op_1172, &&op_1173, &&op_1174, &&op_1175, &&op_1176, &&op_1177, &&op_1178, &&op_1179,
	&&op_1180, &&op_1181, &&op_1182, &&op_1183, &&op_1184, &&op_1185, &&op_1186, &&op_1187,
	&&op_1188, &&op_1189, &&op_1190, &&op_1191, &&op_1192, &&op_1193, &&op_1194, &&op_1195,
	&&op_1196, &&op_1197, &&op_1198, &&op_1199, &&op_1200, &&op_1201, &&op_1202, &&op_1203,
	&&op_1204, &&op_1205, &&op_1206, &&op_1207, &&op_1208, &&op_1209, &&op_1210, &&op_1211,
	&&op_1212, &&op_1213, &&op_1214, &&op_1215, &&op_1216, &&op_1217, &&op_1218, &&op_1219,
	&&op_1220, &&op_1221, &&op_1222, &&op_1223, &&op_1224, &&op_1225, &&op_1226, &&op_1227,
	&&op_1228, &&op_1229, &&op_1230, &&op_1231, &&op_1232, &&op_1233, &&op_1234, &&op_1235,
	&&op_1236, &&op_1237, &&op_1238, &&op_1239, &&op_1240, &&op_1241, &&op_1242, &&op_1243,
	&&op_1244, &&op_1245, &&op_1246, &&op_1247, &&op_1248, &&op_1249, &&op_1250, &&op_1251,
	&&op_1252, &&op_1253, &&op_1254, &&op_1255, &&op_1256, &&op_1257, &&op_1258, &&op_1259,
	&&op_1260, &&op_1261, &&op_1262, &&op_1263, &&op_1264, &&op_1265, &&op_1266, &&op_1267,
	&&op_1268, &&op_1269, &&op_1270, &&op_1271, &&op_1272, &&op_1273, &&op_1274, &&op_1275,
	&&op_1276, &&op_1277, &&op_1278, &&op_1279, &&op_1280, &&op_1281, &&op_1282, &&op_1283,
	&&op_1284, &&op_1285, &&op_1286, &&op_1287, &&op_1288, &&op_1289, &&op_1290, &&op_1291,
	&&op_1292, &&op_1293, &&op_1294, &&op_1295, &&op_1296, &&op_1297, &&op_1298, &&op_1299,
	&&op_1300, &&op_1301, &&op_1302, &&op_1303, &&op_1304, &&op_1305, &&op_1306, &&op_1307,
	&&op_1308, &&op_1309, &&op_1310, &&op_1311, &&op_1312, &&op_1313, &&op_1314, &&op_1315,
	&&op_1316, &&op_1317, &&op_1318, &&op_1319, &&op_1320, &&op_1321, &&op_1322, &&op_1323,
	&&op_1324, &&op_1325, &&op_1326, &&op_1327, &&op_1328, &&op_1329, &&op_1330, &&op_1331,
	&&op_1332, &&op_1333, &&op_1334, &&op_1335, &&op_1336, &&op_1337, &&op_1338, &&op_1339,
	&&op_1340, &&op_1341, &&op_1342, &&op_1343, &&op_1344, &&op_1345, &&op_1346, &&op_1347,
	&&op_1348, &&op_1349, &&op_1350, &&op_1351, &&op_1352, &&op_1353, &&op_1354, &&op_1355,
	&&op_1356, &&op_1357, &&op_1358, &&op_1359, &&op_1360, &&op_1361, &&op_1362, &&op_1363,
	&&op_1364, &&op_1365, &&op_1366, &&op_1367, &&op_1368, &&op_1369, &&op_1370, &&op_1371,
	&&op_1372, &&op_1373, &&op_1374, &&op_1375, &&op_1376, &&op_1377, &&op_1378, &&op_1379,
	&&op_1380, &&op_1381, &&op_1382, &&op_1383, &&op_1384, &&op_1385, &&op_1386, &&op_1387,
	&&op_1388, &&op_1389, &&op_1390, &&op_1391, &&op_1392, &&op_1393, &&op_1394, &&op_1395,
	&&op_1396, &&op_1397, &&op_1398, &&op_1399, &&op_1400, &&op_1401, &&op_1402, &&op_1403,
	&&op_1404, &&op_1405, &&op_1406, &&op_1407, &&op_1408, &&op_1409, &&op_1410, &&op_1411,
	&&op_1412, &&op_1413, &&op_1414, &&op_1415, &&op_1416, &&op_1417, &&op_1418, &&op_1419,
	&&op_1420, &&op_1421, &&op_1422, &&op_1423, &&op_1424, &&op_1425, &&op_1426, &&op_1427,
	&&op_1428, &&op_1429, &&op_1430, &&op_1431, &&op_1432, &&op_1433, &&op_1434, &&op_1435,
	&&op_1436, &&op_1437, &&op_1438, &&op_1439, &&op_1440, &&op_1441, &&op_1442, &&op_1443,
	&&op_1444, &&op_1445, &&op_1446, &&op_1447, &&op_1448, &&op_1449, &&op_1450, &&op_1451,
	&&op_1452, &&op_1453, &&op_1454, &&op_1455, &&op_1456, &&op_1457, &&op_1458, &&op_1459,
	&&op_1460, &&op_1461, &&op_1462, &&op_1463, &&op_1464, &&op_1465, &&op_1466, &&op_1467,
	&&op_1468, &&op_1469, &&op_1470, &&op_1471, &&op_1472, &&op_1473, &&op_1474, &&op_1475,
	&&op_1476, &&op_1477, &&op_1478, &&op_1479, &&op_1480, &&op_1481, &&op_1482, &&op_1483,
	&&op_1484, &&op_1485, &&op_1486, &&op_1487, &&op_1488, &&op_1489, &&op_1490, &&op_1491,
	&&op_1492, &&op_1493, &&op_1494, &&op_1495, &&op_1496, &&op_1497, &&op_1498, &&op_1499,
	&&op_1500, &&op_1501, &&op_1502, &&op_1503, &&op_1504, &&op_1505, &&op_1506, &&op_1507,
	&&op_1508, &&op_1509, &&op_1510, &&op_1511, &&op_1512, &&op_1513, &&op_1514, &&op_1515,
	&&op_1516, &&op_1517, &&op_1518, &&op_1519, &&op_1520, &&op_1521, &&op_1522, &&op_1523,
	&&op_1524, &&op_1525, &&op_1526, &&op_1527, &&op_1528, &&op_1529, &&op_1530, &&op_1531,
	&&op_1532, &&op_1533, &&op_1534, &&op_1535, &&op_1536, &&op_1537, &&op_1538, &&op_1539,
	&&op_1540, &&op_1541, &&op_1542, &&op_1543, &&op_1544, &&op_1545, &&op_1546, &&op_1547,
	&&op_1548, &&op_1549, &&op_1550, &&op_1551, &&op_1552, &&op_1553, &&op_1554, &&op_1555,
	&&op_1556, &&op_1557, &&op_1558, &&op_1559, &&op_1560, &&op_1561, &&op_1562, &&op_1563,
	&&op_1564, &&op_1565, &&op_1566, &&op_1567, &&op_1568, &&op_1569, &&op_1570, &&op_1571,
	&&op_1572, &&op_1573, &&op_1574, &&op_1575, &&op_1576, &&op_1577, &&op_1578, &&op_1579,
	&&op_1580, &&op_1581, &&op_1582, &&op_1583, &&op_1584, &&op_1585, &&op_1586, &&op_1587,
	&&op_1588, &&op_1589, &&op_1590, &&op_1591, &&op_1592, &&op_1593, &&op_1594, &&op_1595,
	&&op_1596, &&op_1597, &&op_1598, &&op_1599, &&op_1600, &&op_1601, &&op_1602, &&op_1603,
	&&op_1604, &&op_1605, &&op_1606, &&op_1607, &&op_1608, &&op_1609, &&op_1610, &&op_1611,
	&&op_1612, &&op_1613, &&op_1614, &&op_1615, &&op_1616, &&op_1617, &&op_1618, &&op_1619,
	&&op_1620, &&op_1621, &&op_1622, &&op_1623, &&op_1624, &&op_1625, &&op_1626, &&op_1627,
	&&op_1628, &&op_1629, &&op_1630, &&op_1631, &&op_1632, &&op_1633, &&op_1634, &&op_1635,
	&&op_1636, &&op_1637, &&op_1638, &&op_1639, &&op_1640, &&op_1641, &&op_1642, &&op_1643,
	&&op_1644, &&op_1645, &&op_1646, &&op_1647, &&op_1648, &&op_1649, &&op_1650, &&op_1651,
	&&op_1652, &&op_1653, &&op_1654, &&op_1655,
#endif
