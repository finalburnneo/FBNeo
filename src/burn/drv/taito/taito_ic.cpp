#include "burnint.h"
#include "taito_ic.h"

INT32 TaitoIC_PC080SNInUse = 0;
INT32 TaitoIC_PC090OJInUse = 0;
INT32 TaitoIC_TC0100SCNInUse = 0;
INT32 TaitoIC_TC0110PCRInUse = 0;
INT32 TaitoIC_TC0140SYTInUse = 0;
INT32 TaitoIC_TC0150RODInUse = 0;
INT32 TaitoIC_TC0180VCUInUse = 0;
INT32 TaitoIC_TC0220IOCInUse = 0;
INT32 TaitoIC_TC0280GRDInUse = 0;
INT32 TaitoIC_TC0360PRIInUse = 0;
INT32 TaitoIC_TC0430GRWInUse = 0;
INT32 TaitoIC_TC0480SCPInUse = 0;
INT32 TaitoIC_TC0510NIOInUse = 0;
INT32 TaitoIC_TC0640FIOInUse = 0;

INT32 TaitoWatchdog;

void TaitoICReset()
{
	if (TaitoIC_PC080SNInUse) PC080SNReset();
	if (TaitoIC_PC090OJInUse) PC090OJReset();
	if (TaitoIC_TC0100SCNInUse) TC0100SCNReset();
	if (TaitoIC_TC0110PCRInUse) TC0110PCRReset();
	if (TaitoIC_TC0140SYTInUse) TC0140SYTReset();
	if (TaitoIC_TC0150RODInUse) TC0150RODReset();
	if (TaitoIC_TC0180VCUInUse) TC0180VCUReset();
	if (TaitoIC_TC0220IOCInUse) TC0220IOCReset();
	if (TaitoIC_TC0280GRDInUse) TC0280GRDReset();
	if (TaitoIC_TC0360PRIInUse) TC0360PRIReset();
	if (TaitoIC_TC0430GRWInUse) TC0430GRWReset();
	if (TaitoIC_TC0480SCPInUse) TC0480SCPReset();
	if (TaitoIC_TC0510NIOInUse) TC0510NIOReset();
	if (TaitoIC_TC0640FIOInUse) TC0640FIOReset();

	if (cchip_active) cchip_reset();

	TaitoWatchdog = 0;
}

void TaitoICExit()
{
	if (TaitoIC_PC080SNInUse) PC080SNExit();
	if (TaitoIC_PC090OJInUse) PC090OJExit();
	if (TaitoIC_TC0100SCNInUse) TC0100SCNExit();
	if (TaitoIC_TC0110PCRInUse) TC0110PCRExit();
	if (TaitoIC_TC0140SYTInUse) TC0140SYTExit();
	if (TaitoIC_TC0150RODInUse) TC0150RODExit();
	if (TaitoIC_TC0180VCUInUse) TC0180VCUExit();
	if (TaitoIC_TC0220IOCInUse) TC0220IOCExit();
	if (TaitoIC_TC0280GRDInUse) TC0280GRDExit();
	if (TaitoIC_TC0360PRIInUse) TC0360PRIExit();
	if (TaitoIC_TC0430GRWInUse) TC0430GRWExit();
	if (TaitoIC_TC0480SCPInUse) TC0480SCPExit();
	if (TaitoIC_TC0510NIOInUse) TC0510NIOExit();
	if (TaitoIC_TC0640FIOInUse) TC0640FIOExit();

	if (cchip_active) cchip_exit();
	
	TaitoIC_PC080SNInUse = 0;
	TaitoIC_PC090OJInUse = 0;
	TaitoIC_TC0100SCNInUse = 0;
	TaitoIC_TC0110PCRInUse = 0;
	TaitoIC_TC0140SYTInUse = 0;
	TaitoIC_TC0150RODInUse = 0;
	TaitoIC_TC0180VCUInUse = 0;
	TaitoIC_TC0220IOCInUse = 0;
	TaitoIC_TC0280GRDInUse = 0;
	TaitoIC_TC0360PRIInUse = 0;
	TaitoIC_TC0430GRWInUse = 0;
	TaitoIC_TC0480SCPInUse = 0;
	TaitoIC_TC0510NIOInUse = 0;
	TaitoIC_TC0640FIOInUse = 0;

	TaitoWatchdog = 0;
}

void TaitoICScan(INT32 nAction)
{
	if (TaitoIC_PC080SNInUse) PC080SNScan(nAction);
	if (TaitoIC_PC090OJInUse) PC090OJScan(nAction);
	if (TaitoIC_TC0100SCNInUse) TC0100SCNScan(nAction);
	if (TaitoIC_TC0110PCRInUse) TC0110PCRScan(nAction);
	if (TaitoIC_TC0140SYTInUse) TC0140SYTScan(nAction);
	if (TaitoIC_TC0150RODInUse) TC0150RODScan(nAction);
	if (TaitoIC_TC0180VCUInUse) TC0180VCUScan(nAction);
	if (TaitoIC_TC0220IOCInUse) TC0220IOCScan(nAction);
	if (TaitoIC_TC0280GRDInUse) TC0280GRDScan(nAction);
	if (TaitoIC_TC0360PRIInUse) TC0360PRIScan(nAction);
	if (TaitoIC_TC0430GRWInUse) TC0430GRWScan(nAction);
	if (TaitoIC_TC0480SCPInUse) TC0480SCPScan(nAction);
	if (TaitoIC_TC0510NIOInUse) TC0510NIOScan(nAction);
	if (TaitoIC_TC0640FIOInUse) TC0640FIOScan(nAction);

	if (cchip_active) cchip_scan(nAction);

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(TaitoWatchdog);
	}
}
