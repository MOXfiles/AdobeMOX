
#include "AEConfig.h"

#ifndef AE_OS_WIN
	#include "AE_General.r"
#endif

resource 'PiPL' (16000) {
	{	/* array properties: 7 elements */
		/* [1] */
		Kind {
			AEGP
		},
		/* [2] */
		Name {
			"NOX"
		},
		/* [3] */
		Category {
			"General Plugin"
		},
		/* [4] */
		Version {
			65536
		},
		/* [5] */
#ifdef AE_OS_WIN
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"GPMain_IO"},
	#else
		CodeWin32X86 {"GPMain_IO"},
	#endif
#else	
	#ifdef AE_OS_MAC
		CodeMachOPowerPC {"GPMain_IO"},
		CodeMacIntel32 {"GPMain_IO"},
		CodeMacIntel64 {"GPMain_IO"},
	#endif
#endif
	}
};

