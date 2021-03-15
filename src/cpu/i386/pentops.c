// Pentium+ specific opcodes

static void PENTIUMOP(rdmsr)(void)			// Opcode 0x0f 32
{
	// TODO
	CYCLES(CYCLES_RDMSR);
}

static void PENTIUMOP(wrmsr)(void)			// Opcode 0x0f 30
{
	// TODO
	CYCLES(1);		// TODO: correct cycle count
}

static void PENTIUMOP(rdtsc)(void)			// Opcode 0x0f 31
{
	UINT64 ts = I.tsc + (I.base_cycles - I.cycles);
	REG32(EAX) = (UINT32)(ts);
	REG32(EDX) = (UINT32)(ts >> 32);

	CYCLES(CYCLES_RDTSC);
}

static void I386OP(cyrix_unknown)(void)		// Opcode 0x0f 74
{
	CYCLES(1);
}
