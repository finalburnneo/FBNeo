#include "burner.h"
#include "vid_detector.h"
#include "detector_buffers.h"

struct DetectorBuffer {
	char name[128];
	const unsigned char *buffer;
	int size;
};

#define ITEM(name,buffer,size) { name,buffer,size },
static DetectorBuffer detector_buffers[] = {
	#include "detector_loaders.h"
	{ "", 0, 0 },
};
#undef ITEM

bool LoadMemoryBufferDetector(MemoryBuffer &buffer, const char *file, bool force_disk)
{
	if (force_disk) {
#ifdef PRINT_DEBUG_INFO
		dprintf(_T("Load DetectorMemoryBuffer from file: %s\n"), ANSIToTCHAR(file,0,0));
#endif
		return LoadMemoryBufferFromFile(buffer, file);
	}
	else {
#ifdef PRINT_DEBUG_INFO
		dprintf(_T("Load DetectorMemoryBuffer from internal buffer: %s\n"), ANSIToTCHAR(file,0,0));
#endif
		for (int i = 0; detector_buffers[i].size > 0; i++) {
			if (!_stricmp(detector_buffers[i].name, file)) {
				buffer.Init(detector_buffers[i].buffer, detector_buffers[i].size);
				break;
			}
		}
#ifdef PRINT_DEBUG_INFO
		if (buffer.len == 0) {
			dprintf(_T("Can't load DetectorMemoryBuffer: %s\n"), ANSIToTCHAR(file,0,0));
		}
#endif
		return buffer.len > 0;
	}
	return false;
}
