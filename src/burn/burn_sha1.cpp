#include "burnint.h"
#include <stdint.h>

#define SHA1_BLOCK_SIZE 64
#define SHA1_HASH_SIZE 20

struct sha1_state {
    UINT32 state[5];
    UINT64 bitcount;
    UINT8 buffer[SHA1_BLOCK_SIZE];
};

#define ROTLEFT(a, b) (((a) << (b)) | ((a) >> (32 - (b))))
#define F0(b, c, d) ((b & c) | (~b & d))
#define F1(b, c, d) (b ^ c ^ d)
#define F2(b, c, d) ((b & c) | (b & d) | (c & d))
#define F3(b, c, d) (b ^ c ^ d)

void SHA1_Transform(sha1_state *ctx, const UINT8 *data) {
    UINT32 a, b, c, d, e, i, t;
    UINT32 m[80];

    for (i = 0; i < 16; i++) {
        m[i] = (data[i * 4] << 24) | (data[i * 4 + 1] << 16) |
               (data[i * 4 + 2] << 8) | (data[i * 4 + 3]);
    }
    for (i = 16; i < 80; i++) {
        m[i] = ROTLEFT(m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16], 1);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 80; i++) {
        if (i < 20) {
            t = ROTLEFT(a, 5) + F0(b, c, d) + e + m[i] + 0x5A827999;
        } else if (i < 40) {
            t = ROTLEFT(a, 5) + F1(b, c, d) + e + m[i] + 0x6ED9EBA1;
        } else if (i < 60) {
            t = ROTLEFT(a, 5) + F2(b, c, d) + e + m[i] + 0x8F1BBCDC;
        } else {
            t = ROTLEFT(a, 5) + F3(b, c, d) + e + m[i] + 0xCA62C1D6;
        }
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void SHA1_Init(sha1_state *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->bitcount = 0;
    memset(ctx->buffer, 0, SHA1_BLOCK_SIZE);
}

void SHA1_Update(sha1_state *ctx, const UINT8 *data, INT32 len) {
    INT32 i;

    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->bitcount / 8 % SHA1_BLOCK_SIZE] = data[i];
        ctx->bitcount += 8;
        if ((ctx->bitcount / 8 % SHA1_BLOCK_SIZE) == 0) {
            SHA1_Transform(ctx, ctx->buffer);
        }
    }
}

void SHA1_Final(UINT8 *hash, sha1_state *ctx) {
    INT32 i = ctx->bitcount / 8 % SHA1_BLOCK_SIZE;

    ctx->buffer[i++] = 0x80;
    if (i > SHA1_BLOCK_SIZE - 8) {
        while (i < SHA1_BLOCK_SIZE) {
            ctx->buffer[i++] = 0;
        }
        SHA1_Transform(ctx, ctx->buffer);
        i = 0;
    }
    while (i < SHA1_BLOCK_SIZE - 8) {
        ctx->buffer[i++] = 0;
    }

    for (int j = 0; j < 8; j++) {
        ctx->buffer[SHA1_BLOCK_SIZE - 1 - j] = ctx->bitcount >> (j * 8);
    }
    SHA1_Transform(ctx, ctx->buffer);

    for (i = 0; i < 5; i++) {
        hash[i * 4 + 0] = (ctx->state[i] >> 24) & 0xff;
        hash[i * 4 + 1] = (ctx->state[i] >> 16) & 0xff;
        hash[i * 4 + 2] = (ctx->state[i] >> 8) & 0xff;
        hash[i * 4 + 3] = (ctx->state[i]) & 0xff;
    }
}

int BurnComputeSHA1(const UINT8 *buffer, int buffer_size, char *hash_str) {
    sha1_state ctx;
	UINT8 hash[SHA1_HASH_SIZE];

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, buffer, buffer_size);
    SHA1_Final(hash, &ctx);

	for (int i = 0; i < SHA1_HASH_SIZE; i++) {
		char temp[128];
		sprintf(temp, "%02x", hash[i]);
		hash_str[i*2 + 0] = temp[0];
		hash_str[i*2 + 1] = temp[1];
	}

	hash_str[SHA1_HASH_SIZE*2] = 0;

	return 0;
}

int BurnComputeSHA1(const char *filename, char *hash_str) {
    sha1_state ctx;
	UINT8 hash[SHA1_HASH_SIZE];
	const int filebuf_size = 1024 * 1024;
	UINT8 *buffer;

    FILE *file = fopen(filename, "rb");
    if (!file) {
        return 1;
    }

	buffer = (UINT8*)malloc(filebuf_size);
	if (!buffer) return 2;

	SHA1_Init(&ctx);
    INT32 bytes_read;
    while ((bytes_read = fread(buffer, 1, filebuf_size, file)) > 0) {
        SHA1_Update(&ctx, buffer, bytes_read);
    }
    fclose(file);

	free(buffer);

    SHA1_Final(hash, &ctx);

	for (int i = 0; i < SHA1_HASH_SIZE; i++) {
		char temp[128];
		sprintf(temp, "%02x", hash[i]);
		hash_str[i*2 + 0] = temp[0];
		hash_str[i*2 + 1] = temp[1];
    }

	hash_str[SHA1_HASH_SIZE*2] = 0;

	return 0;
}
