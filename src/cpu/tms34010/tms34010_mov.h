/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef TMS34010_MOV_H
#define TMS34010_MOV_H

#include "tms34010.h"
#include "tms34010_memacc.h"
#include "tms34010_defs.h"

namespace tms { namespace ops {

// MOVB RS, *RD
void movb_rs_ird(cpu_state *cpu, word opcode)
{
    wrfield_8(_rd, (dword)(byte)_rs);

    CONSUME_CYCLES(1);
}

// MOVB *RS, RD
void movb_irs_rd(cpu_state *cpu, word opcode)
{
    _rd = rdfield_8_sx(_rs);
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}

// MOVB *RS, *RD
void movb_irs_ird(cpu_state *cpu, word opcode)
{
    wrfield_8(_rd, (dword)(byte)rdfield_8(_rs));

    CONSUME_CYCLES(3);
}

// MOVB RS, *RD(OFFS)
void movb_rs_irdo(cpu_state *cpu, word opcode)
{
    wrfield_8(_rd + wsign_ext(mem_read(_pc)), (dword)(byte)_rs);
    _pc += 16;

    CONSUME_CYCLES(3);
}

// MOVB *RS(OFFS), RD
void movb_irso_rd(cpu_state *cpu, word opcode)
{
    _rd = rdfield_8_sx(_rs + wsign_ext(mem_read(_pc)));
    _pc += 16;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(5);
}

// MOVB *RS(OFFS), *RD(OFFS)
void movb_irso_irdo(cpu_state *cpu, word opcode)
{
    dword data = rdfield_8(_rs + wsign_ext(mem_read(_pc)));
    wrfield_8(_rd + wsign_ext(mem_read(_pc + 16)), data);
    _pc += 32;

    CONSUME_CYCLES(5);
}

// MOVB RS, @DADDR
void movb_rs_daddr(cpu_state *cpu, word opcode)
{
    dword daddr = mem_read_d(_pc);
    _pc += 32;
    wrfield_8(daddr, (dword)(byte)_rd);

    CONSUME_CYCLES(1);
}

// MOVB @SADDR, RD
void movb_addr_rd(cpu_state *cpu, word opcode)
{
    _rd = rdfield_8_sx(mem_read_d(_pc));
    _pc += 32;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(5);
}

// MOVB @SADDR, @DADDR
void movb_saddr_daddr(cpu_state *cpu, word opcode)
{
    dword src = mem_read_d(_pc);
    _pc += 32;
    dword dst = mem_read_d(_pc);
    _pc += 32;
    wrfield_8(dst, (dword)(byte)rdfield_8(src));

    CONSUME_CYCLES(7);
}

// MOVE RS, RD
void move_rs_rd(cpu_state *cpu, word opcode)
{
    _rd = _rs;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(1);
}

// MOVE RS, RD  [A->B]
void move_rs_rd_a(cpu_state *cpu, word opcode)
{
    cpu->b[RD_n].value = cpu->a[RS_n].value;
    _st &= ~ST_V;
    update_zn(cpu->b[RD_n].value);

    CONSUME_CYCLES(1);
}

// MOVE RS, RD [B->A]
void move_rs_rd_b(cpu_state *cpu, word opcode)
{
    cpu->a[RD_n].value = cpu->b[RS_n].value;
    _st &= ~ST_V;
    update_zn(cpu->a[RD_n].value);

    CONSUME_CYCLES(1);
}

// MOVE RS, *RD, 0
void move_rs_ird_0(cpu_state *cpu, word opcode)
{
    wrfield0(_rd, _rs);

    CONSUME_CYCLES(1);
}

// MOVE RS, *RD, 1
void move_rs_ird_1(cpu_state *cpu, word opcode)
{
    wrfield1(_rd, _rs);

    CONSUME_CYCLES(1);
}

// MOVE RS, -*RD, 0
void move_rs_mird_0(cpu_state *cpu, word opcode)
{
    _rd -= FW0;
    wrfield0(_rd, _rs);

    CONSUME_CYCLES(2);
}

// MOVE RS, -*RD, 1
void move_rs_mird_1(cpu_state *cpu, word opcode)
{
    _rd -= FW1;
    wrfield1(_rd, _rs);

    CONSUME_CYCLES(2);
}

// MOVE RS, *RD+, 0
void move_rs_irdp_0(cpu_state *cpu, word opcode)
{
    wrfield0(_rd, _rs);
    _rd += FW0;

    CONSUME_CYCLES(1);
}

// MOVE RS, *RD+, 1
void move_rs_irdp_1(cpu_state *cpu, word opcode)
{
    wrfield1(_rd, _rs);
    _rd += FW1;

    CONSUME_CYCLES(1);
}

// MOVE *RS, RD, 0
void move_irs_rd_0(cpu_state *cpu, word opcode)
{
    _rd = rdfield0(_rs);
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}

// MOVE *RS, RD, 1
void move_irs_rd_1(cpu_state *cpu, word opcode)
{
    _rd = rdfield1(_rs);
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}


// MOVE -*RS, RD, 0
void move_mirs_rd_0(cpu_state *cpu, word opcode)
{
    _rs -= FW0;
    _rd = rdfield0(_rs);
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(4);
}

// MOVE -*RS, RD, 1
void move_mirs_rd_1(cpu_state *cpu, word opcode)
{
    _rs -= FW1;
    _rd = rdfield1(_rs);
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(4);
}

// MOVE *RS+, RD, 0
void move_irsp_rd_0(cpu_state *cpu, word opcode)
{
    dword data = rdfield0(_rs);
    _rs += FW0;
    _rd = data;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}


// MOVE *RS+, RD, 1
void move_irsp_rd_1(cpu_state *cpu, word opcode)
{
    dword data = rdfield1(_rs);
    _rs += FW1;
    _rd = data;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}

// MOVE *RS, *RD, 0
void move_irs_ird_0(cpu_state *cpu, word opcode)
{
    wrfield0(_rd, rdfield0(_rs));

    CONSUME_CYCLES(3);
}

// MOVE *RS, *RD, 1
void move_irs_ird_1(cpu_state *cpu, word opcode)
{
    wrfield1(_rd, rdfield1(_rs));

    CONSUME_CYCLES(3);
}

// MOVE -*RS, -*RD, 0
void move_mirs_mird_0(cpu_state *cpu, word opcode)
{
    _rs -= FW0;
    sdword data = rdfield0(_rs);
    _rd -= FW0;
    wrfield0(_rd, data);

    CONSUME_CYCLES(4);
}


// MOVE -*RS, -*RD, 1
void move_mirs_mird_1(cpu_state *cpu, word opcode)
{
    _rs -= FW1;
    sdword data = rdfield1(_rs);
    _rd -= FW1;
    wrfield1(_rd, data);

    CONSUME_CYCLES(4);
}

// MOVE *RS+, *RD+, 0
void move_irsp_irdp_0(cpu_state *cpu, word opcode)
{
    dword value = rdfield0(_rs);
    _rs += FW0;
    wrfield0(_rd, value);
    _rd += FW0;

    CONSUME_CYCLES(4);
}

// MOVE *RS+, *RD+, 1
void move_irsp_irdp_1(cpu_state *cpu, word opcode)
{
    dword value = rdfield1(_rs);
    _rs += FW1;
    wrfield1(_rd, value);
    _rd += FW1;

    CONSUME_CYCLES(4);
}

// MOVE RS, *RD(OFFS), 0
void move_rs_irdo_0(cpu_state *cpu, word opcode)
{
    wrfield0(_rd + wsign_ext(mem_read(_pc)), _rs);
    _pc += 16;

    CONSUME_CYCLES(3);
}

// MOVE RS, *RD(OFFS), 1
void move_rs_irdo_1(cpu_state *cpu, word opcode)
{
    wrfield1(_rd + wsign_ext(mem_read(_pc)), _rs);
    _pc += 16;

    CONSUME_CYCLES(3);
}

// MOVE *RS(OFFS), RD, 0
void move_irso_rd_0(cpu_state *cpu, word opcode)
{
    _rd = rdfield0(_rs + wsign_ext(mem_read(_pc)));
    _pc += 16;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(5);
}

// MOVE *RS(OFFS), RD, 1
void move_irso_rd_1(cpu_state *cpu, word opcode)
{
    _rd = rdfield1(_rs + wsign_ext(mem_read(_pc)));
    _pc += 16;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(5);
}

// MOVE *RS(OFFS), *RD+, 0
void move_irso_irdp_0(cpu_state *cpu, word opcode)
{
    dword data = rdfield0(_rs + wsign_ext(mem_read(_pc)));
    _pc += 16;
    wrfield0(_rd, data);
    _rd += FW0;

    CONSUME_CYCLES(5);
}

// MOVE *RS(OFFS), *RD+, 1
void move_irso_irdp_1(cpu_state *cpu, word opcode)
{
    dword data = rdfield1(_rs + wsign_ext(mem_read(_pc)));
    _pc += 16;
    wrfield1(_rd, data);
    _rd += FW1;

    CONSUME_CYCLES(5);
}

// MOVE *RS(OFFS), *RD(OFFS), 0
void move_irso_irdo_0(cpu_state *cpu, word opcode)
{
    dword data = rdfield0(_rs + wsign_ext(mem_read(_pc)));
    wrfield0(_rd + wsign_ext(mem_read(_pc + 16)), data);
    _pc += 32;

    CONSUME_CYCLES(5);
}

// MOVE *RS(OFFS), *RD(OFFS), 1
void move_irso_irdo_1(cpu_state *cpu, word opcode)
{
    dword data = rdfield1(_rs + wsign_ext(mem_read(_pc)));
    wrfield1(_rd + wsign_ext(mem_read(_pc + 16)), data);
    _pc += 32;

    CONSUME_CYCLES(5);
}

// MOVE RS, @DADDR, 0
void move_rs_daddr_0(cpu_state *cpu, word opcode)
{
    dword addr = mem_read_d(_pc);
    _pc += 32;
    wrfield0(addr, _rd);

    CONSUME_CYCLES(3);
}

// MOVE RS, @DADDR, 1
void move_rs_daddr_1(cpu_state *cpu, word opcode)
{
    dword addr = mem_read_d(_pc);
    _pc += 32;
    wrfield1(addr, _rd);

    CONSUME_CYCLES(3);
}

// MOVE @SADDR, RD, 0
void move_saddr_rd_0(cpu_state *cpu, word opcode)
{
    dword addr = mem_read_d(_pc);
    _pc += 32;
    _rd = rdfield0(addr);
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(5);
}

// MOVE @SADDR, RD, 0
void move_saddr_rd_1(cpu_state *cpu, word opcode)
{
    dword addr = mem_read_d(_pc);
    _pc += 32;
    _rd = rdfield1(addr);
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(5);
}

// MOVE @SADDR, *RS+, 0
void move_addr_irsp_0(cpu_state *cpu, word opcode)
{
    dword addr = mem_read_d(_pc);
    _pc += 32;
    wrfield0(addr, _rd);
    _rd += FW0;

    CONSUME_CYCLES(5);
}

// MOVE @SADDR, *RS+, 0
void move_addr_irsp_1(cpu_state *cpu, word opcode)
{
    dword addr = mem_read_d(_pc);
    _pc += 32;
    wrfield1(addr, _rd);
    _rd += FW1;

    CONSUME_CYCLES(5);
}

// MOVE @SADDR, @DADDR, 0
void move_saddr_daddr_0(cpu_state *cpu, word opcode)
{
    dword src = mem_read_d(_pc);
    _pc += 32;
    dword dst = mem_read_d(_pc);
    _pc += 32;
    wrfield0(dst, rdfield0(src));

    CONSUME_CYCLES(7);
}

// MOVE @SADDR, @DADDR, 0
void move_saddr_daddr_1(cpu_state *cpu, word opcode)
{
    dword src = mem_read_d(_pc);
    _pc += 32;
    dword dst = mem_read_d(_pc);
    _pc += 32;
    wrfield1(dst, rdfield1(src));

    CONSUME_CYCLES(7);
}

// MOVEI IW, RD
void movi_iw_rd(cpu_state *cpu, word opcode)
{
    _rd = wsign_ext(mem_read(_pc));
    _pc += 16;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(2);
}


// MOVEI IL, RD
void movi_il_rd(cpu_state *cpu, word opcode)
{
    _rd = mem_read_d(_pc);
    _pc += 32;
    _st &= ~ST_V;
    update_zn(_rd);

    CONSUME_CYCLES(3);
}

// MOVX RS, RD
void movx_rs_rd(cpu_state *cpu, word opcode)
{
    _rdx = _rsx;
    CONSUME_CYCLES(1);
}

// MOVY RS, RD
void movy_rs_rd(cpu_state *cpu, word opcode)
{
    _rdy = _rsy;
    CONSUME_CYCLES(1);
}

// MOVK K, RD
void movk_k_rd(cpu_state *cpu, word opcode)
{
    _rd = fw_lut[K];
    CONSUME_CYCLES(1);
}


} // ops
} // tms

#endif // TMS34010_MOV_H
