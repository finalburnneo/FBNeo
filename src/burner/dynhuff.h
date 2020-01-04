#ifndef _DYNHUFF_H_
#define _DYNHUFF_H_

#include <stdio.h>

#define MAX_FREQ ((unsigned int)0xFFFFFFFF) // max unsigned int
#define MAX_BUFFER_LEN 32768     // 4 B * 32768  = 128 KB
#define MAX_LIST_LEN 512

//#
//#             Buffer Handling
//#
//##############################################################################

// 1 if decoding is finished, 0 otherwise
extern int end_of_buffer;

// takes a data and compress it to the buffer
void EncodeBuffer(unsigned char data);

// decompresses and returns the next data from buffer
unsigned char DecodeBuffer();

//#
//#             File I/O
//#
//##############################################################################

// takes a decompressed file and compresses the whole thing into another file
int Compress(char *d_file_name, char *c_file_name);

// takes a compressed file and decompresses the whole thing into another file
int Decompress(char *c_file_name, char *d_file_name);

// returns 0 if the compressed file is opened successfully, 1 otherwise
int OpenCompressedFile(char *file_name, char *mode);

// Always returns 0
int EmbedCompressedFile(FILE *pEmb, int nOffset);

// returns 0 if the decompressed file is opened successfully, 1 otherwise
int OpenDecompressedFile(char *file_name, char *mode);

// loads the compressed file into c_buffer
// - use it only if the compressed file was opened successfully
void LoadCompressedFile();

// writes whatever is left in c_buffer to the compressed file,
// and closes the compressed file
// - use this if the compressed file was opened in 'write' mode
void WriteCompressedFile();

// writes whatever is left in d_buffer to the decompressed file,
// and closes the decompressed file
// - use this if the decompressed file was opened in 'write' mode
void WriteDecompressedFile(int bytes_remain);

// closes the compressed file
// - use this if the compressed file was opened in 'read' mode
void CloseCompressedFile();

// closes the compressed file
// - use this if the decompressed file was opened in 'read' mode
void CloseDecompressedFile();

//#
//#             Utility Functions
//#
//##############################################################################

// print the encoded buffer (upto the current buffer item)
void PrintBuffer(); // use this while encoding

// print all nodes while reverse level traversing
void PrintFreqTraverse(); // use this before DestroyDHT() happens

// print the tree in breath order (rotated -90 degrees)
void PrintTree(); // use this before DestroyDHT() happens

// print compression result
void PrintResult(); // use this after finish encoding

//#
//#             Compression Status Freezing
//#
//##############################################################################

// freeze the status of decoding
// returns a malloc()'d buffer in *buffer
// the length of the buffer is returned in *size
// returns 0 always
int FreezeDecode(unsigned char **buffer, int *size);

// unfreeze the status of decoding from the given buffer
// the length of the buffer is passed in size
// returns 0 if successful, 1 otherwise
int UnfreezeDecode(const unsigned char* buffer, int size);

// freeze the status of encoding
// returns a malloc()'d buffer in *buffer
// the length of the buffer is returned in *size
// returns 0 always
int FreezeEncode(unsigned char **buffer, int *size);

// unfreeze the status of encoding from the given buffer
// the length of the buffer is passed in size
// returns 0 if successful, 1 otherwise
int UnfreezeEncode(const unsigned char* buffer, int size);

#endif // end of '#ifndef _DYNHUFF_H_'
