#include "switchres_wrapper.h"
#include "burner.h"


#ifdef __linux__
#define LIBSWR "libswitchres.so"
#elif _WIN32
#define LIBSWR "libswitchres.dll"
#endif

srAPI* SRobj;

unsigned char switchres_init() {
	const char* err_msg;

	printf("About to open %s.\n", LIBSWR);

	// Load the lib
	LIBTYPE dlp = OPENLIB(LIBSWR);

	// Loading failed, inform and exit
	if (!dlp) {
		printf("Loading %s failed.\n", LIBSWR);
		printf("Error: %s\n", LIBERROR());
		return 0;
	}

	printf("Loading %s succeded.\n", LIBSWR);

	// Load the init()
	LIBERROR();
	SRobj =  (srAPI*)LIBFUNC(dlp, "srlib");
	if ((err_msg = LIBERROR()) != NULL) {
		printf("Failed to load srAPI: %s\n", err_msg);
		return 0;
	}

	// Testing the function
	printf("Init a new switchres_manager object:\n");
	SRobj->init();

	return 1;
}

unsigned char switchres_switch_resolution(int& w, int& h, double& rr, unsigned char& interlace) {
		// Call mode + get result values
	unsigned char ret;
	sr_mode srm;

	printf("Orignial resolution expected: %dx%d@%f-%d\n", w, h, rr, interlace);
	ret = SRobj->sr_add_mode(w, h, rr, interlace, &srm);
	if(!ret)
	{
		printf("ERROR: couldn't add the required mode. Exiting!\n");
		return 0;
	}

	printf("Got resolution: %dx%d%c@%f\n", srm.width, srm.height, srm.interlace, srm.refresh);
	ret = SRobj->sr_switch_to_mode(srm.width, srm.height, srm.refresh, srm.interlace, &srm);
	if(!ret)
	{
		printf("ERROR: couldn't switch to the required mode. Exiting!\n");
		return 0;
	}

	// Set the values
	w = srm.width;
	h = srm.height;
	rr = srm.refresh;
	interlace = srm.interlace;
	return 1;
}

void switchres_exit() {
	SRobj->deinit();
}

