#pragma once

struct MemoryBuffer {
	void Init(const unsigned char *_data, int _len) {
		len = _len > 2048 ? 2048 : _len;
		memcpy(data, (const char *)_data, len);
	}
	char data[2048];
	int len;
};

bool LoadMemoryBufferFromFile(MemoryBuffer &buffer, const char *file);
