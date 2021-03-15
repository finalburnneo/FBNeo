// Intel 486+ specific opcodes

static void I486OP(cpuid)(void)				// Opcode 0x0F A2
{
	switch (REG32(EAX))
	{
		case 0:
		{
			REG32(EAX) = I.cpuid_max_input_value_eax;
			REG32(EBX) = I.cpuid_id0;
			REG32(ECX) = I.cpuid_id2;
			REG32(EDX) = I.cpuid_id1;
			CYCLES(CYCLES_CPUID);
			break;
		}

		case 1:
		{
			REG32(EAX) = I.cpu_version;
			REG32(EDX) = I.feature_flags;
			CYCLES(CYCLES_CPUID_EAX1);
			break;
		}
	}
}

static void I486OP(invd)(void)				// Opcode 0x0f 08
{
	// Nothing to do ?
	CYCLES(CYCLES_INVD);
}

static void I486OP(wbinvd)(void)			// Opcode 0x0f 09
{
	// Nothing to do ?
}
