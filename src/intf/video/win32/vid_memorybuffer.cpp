#include "burner.h"
#include "vid_memorybuffer.h"

bool LoadMemoryBufferFromFile(MemoryBuffer &buffer, const char *file)
{
	FILE *f = fopen(file, "rb");
	if (f) {
		const int max_len = 2048;
		unsigned char buf[max_len];
		int len = fread(buf, 1, max_len, f);
		fclose(f);
		buffer.Init(buf, len);
		return true;
	}
#ifdef PRINT_DEBUG_INFO
	dprintf(_T("LoadMemoryBufferFromFile: can't open file %s\n"), ANSIToTCHAR(file,0,0));
#endif

	return false;
}
