//
//  Dynamic Huffman Encoder/Decoder
//
//  implemented by Gangta
//
//-----------------------------------------------------------
//
//  Version History
//
//  version 0.1  - first release
//  version 0.1a - fixed a bug where the bit remainder in
//                 the EncodeBuffer() function didn't reset
//                 to 32 if encoding is restarted before
//                 program exit
//  version 0.1b - fixed right shift
//               - fixed some utility functions
//
//  version 0.1c - fixed PrintResult()
//               - fixed a fatal bug in CorrectDH_T) 15% slower
//               - optimized compression/decompression 95% faster
//               - overall 80% faster :)
//
//  version 0.2  - optimized even more, now O(lg 2) :)
//
//-----------------------------------------------------------
//
//  Memory usage:
//
//     128 KB for both encoding/decoding
//
//     I tested buffer size 16K, 32K, 64K, 128K, etc
//     The performance started to drop after 512K,
//     so I chose 128K.
//
//  Comments:
//
//     I tried to optimize the speed as best I can in my
//     knowledge.
//
//     It has buffer overflow protection for encoding and
//     decoding.
//
//     It has a frequency reset function which prevents
//     frequency overflow.
//
//     It can compress text files as well as any type of
//     binary files.  The compression is not as good as
//     zlib, or bzlibb2, but it supports "on the fly"
//     encoding/decoding, and I believe it is faster than
//     bzlibb2.
//
//
///////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynhuff.h"

#ifndef UINT32
#define   UINT32 unsigned int
#endif

//#
//#             Data sets for Dynamic Huffman Encoding/Decoding
//#
//##############################################################################

// data sets for dynamic huffman
struct DHTNode
{
	unsigned int freq;
	unsigned char value;
	struct DHTNode* left;
	struct DHTNode* right;
	struct DHTNode* parent;
	int l_index; // where this node is in the list
};

static struct DHTNode DHTroot = {0, 0, nullptr, nullptr, nullptr, 0};
static struct DHTNode* null_node_ptr = &DHTroot; // where to add a new data
static struct DHTNode* look_up_table[257] = {nullptr,}; // check if it's a new data
static struct DHTNode* node_list[MAX_LIST_LEN] = {nullptr,}; // reverse level traverse
static int list_idx = MAX_LIST_LEN - 1;

// data sets for compressed buffer and compressed file I/O
static FILE* cFile = nullptr;
static UINT32* c_buffer = nullptr;
static int c_buffer_idx = 0;
static int c_buffer_size = 0;
int end_of_buffer = 0;
static int code_length = 0;

// data sets for decompressed buffer and decompressed file I/O
static FILE* dFile = nullptr;
static UINT32 d_buffer[MAX_BUFFER_LEN] = {0,};
static int d_buffer_idx = 0;

// file embedding
static int bEmbed = 0;
static int embed_offset = 0;

// flags used to determine when to stop decoding
static unsigned int code_count = 0;
static int code_reset_count = 0;

// for saving compression result
static unsigned int nCompressedFileSize; // flag
static unsigned int nCodeCount; // flag
static int nCodeResetCount; // flag
static int nBufferIdx;
static int nBufferResetCount; // flag

// for decompressing
static struct DecodeStatus
{
	UINT32 code_slice; // slice of a buffer element
	int bit_counter; // if 32, get a new slice
	unsigned int nCode; // how many codes has been decoded so far
	int nCodeReset; // how many times the data counter has been reset
} dcs;

// function prototypes
static void AllocBufferC(int size);
static void GrowBufferC();
static void ResetBufferD();
static void ReloadBufferD();
static void SaveResult();
#ifdef _DEBUG
static void CheckList();
#endif

//#
//#             Functions for Dynamic Huffman Encoding/Decoding
//#
//##############################################################################

// initialize a node in the dynamic huffman tree
static void SetNode(struct DHTNode* node_ptr, unsigned int freq, unsigned char value, struct DHTNode* left,
                    struct DHTNode* right, struct DHTNode* parent)
{
	node_ptr->freq = freq; // count # of occurances
	node_ptr->value = value; // corresponding key in the mInputSet
	node_ptr->left = left; // left subtree
	node_ptr->right = right; // right subtree
	node_ptr->parent = parent; // parent node
}

// the list is a reverse level traversal of the dynamic huffman tree
static void BuildNodeList(struct DHTNode* node_ptr)
{
	if (list_idx == MAX_LIST_LEN - 1) { node_list[MAX_LIST_LEN - 1] = nullptr; }
	node_list[--list_idx] = node_ptr->parent;
	node_ptr->parent->l_index = list_idx;
	node_list[--list_idx] = node_ptr;
	node_ptr->l_index = list_idx;
}

// swap the two nodes in the dynamic huffman tree
static void SwapNodes(struct DHTNode* node1, struct DHTNode* node2)
{
	unsigned int freq_diff;
	struct DHTNode temp_node;
	struct DHTNode* node_ptr = node1; // node1 always has higher freq

	// backup node1 before swapping
	memcpy(&temp_node, node1, sizeof(struct DHTNode));

	// swap each node's parent's left or right depending on
	// whether each noce is a left child or a right child
	if (node2->parent->left == node2)
		node2->parent->left = node1;
	else
		node2->parent->right = node1;
	if (temp_node.parent->left == node1)
		node1->parent->left = node2;
	else
		node1->parent->right = node2;

	// swap each node's parent
	node1->parent = node2->parent;
	node2->parent = temp_node.parent;

	// swap each node's l_index
	node1->l_index = node2->l_index;
	node2->l_index = temp_node.l_index;

	// correct frequencies
	freq_diff = node1->freq - node2->freq;
	node_ptr = node1;
	while (node_ptr->parent)
	{
		node_ptr = node_ptr->parent;
		node_ptr->freq += freq_diff;
	}
	node_ptr = node2;
	while (node_ptr->parent)
	{
		node_ptr = node_ptr->parent;
		node_ptr->freq -= freq_diff;
	}
}

// correct the dynamic huffman tree if necessary
static void CorrectDHT(int curr_idx)
{
	struct DHTNode* temp;
	int swap_idx;

	// search for nodes that are not in order
	while (node_list[curr_idx + 1])
	{
		// if found something out of order
		if (node_list[curr_idx]->freq > node_list[curr_idx + 1]->freq)
		{
			// search for the node to swap with, then swap them
			swap_idx = curr_idx + 1;
			while (node_list[swap_idx + 1])
			{
				// found node to swap it with, so swap them,
				if (node_list[swap_idx]->freq < node_list[swap_idx + 1]->freq)
				{
					// extra check for null node exception
					if (node_list[list_idx]->parent == node_list[curr_idx])
					{
						SwapNodes(node_list[list_idx], node_list[swap_idx]);
						temp = node_list[list_idx];
						node_list[list_idx] = node_list[swap_idx];
						node_list[swap_idx] = temp;
					}
					else
					{
						SwapNodes(node_list[curr_idx], node_list[swap_idx]);
						temp = node_list[curr_idx];
						node_list[curr_idx] = node_list[swap_idx];
						node_list[swap_idx] = temp;
					}

					if (node_list[swap_idx]->parent)
						CorrectDHT(node_list[swap_idx]->parent->l_index);

					return;
				}
				++swap_idx;
			}
		}
		if (!(node_list[curr_idx]->parent))
			return;
		curr_idx = node_list[curr_idx]->parent->l_index;
	}
}

// prevent node frequency from overflowing
static void ResetFrequency()
{
	int idx = list_idx; // start with the bottom left node

	node_list[idx++]->freq = 1;
	node_list[idx++]->freq = 1;

	while (node_list[idx])
	{
		// if it's a leaf, set it to the minimum possible frequency
		if (!node_list[idx]->right)
			node_list[idx]->freq = node_list[idx - 1]->freq;
			// if it's a joint node, set the frequency to left + right
		else if (node_list[idx]->left)
			node_list[idx]->freq = node_list[idx]->left->freq + node_list[idx]->right->freq;
		++idx;
	}
}

// build the dynamic huffman tree
static void BuildDHT(unsigned char data)
{
	struct DHTNode* node_ptr = look_up_table[data];
	struct DHTNode* update_node_ptr = nullptr;

	// if tree is empty, initialize the tree
	if (!DHTroot.freq)
	{
		null_node_ptr->right = static_cast<struct DHTNode*>(malloc(sizeof(struct DHTNode)));
		SetNode(&DHTroot, 1, 0, nullptr, DHTroot.right, nullptr);
		SetNode(DHTroot.right, 1, data, nullptr, nullptr, &DHTroot);

		look_up_table[data] = DHTroot.right;
		BuildNodeList(look_up_table[data]);
	}
	// if the data already exists in the tree,
	// just increment the node counter
	else if (look_up_table[data])
	{
		// reset frequencies before they overflow
		if (DHTroot.freq + 1 > MAX_FREQ) ResetFrequency();

		// increment node frequency
		node_ptr->freq += 1;

		// reduce level of reverse level traversing (optimization)
		if (node_ptr->parent != null_node_ptr && node_ptr->freq > node_list[(node_ptr->l_index) + 1]->freq)
			update_node_ptr = node_ptr;

		// increment frequencies for all its ancestors
		node_ptr = node_ptr->parent;
		while (node_ptr)
		{
			node_ptr->freq += 1;

			// reduce level of reverse level traversing (optimization)
			if (!update_node_ptr && node_ptr->parent && node_ptr->freq > node_list[(node_ptr->l_index) + 1]->freq)
				update_node_ptr = node_ptr;

			node_ptr = node_ptr->parent;
		}

		// correct the dynamic huffman tree if neccessary
		if (update_node_ptr) CorrectDHT(update_node_ptr->l_index);
	}
	// if it's a new data, insert it into a new node
	else
	{
		// reset frequencies before they overflow
		if (DHTroot.freq + 1 > MAX_FREQ) ResetFrequency();

		null_node_ptr->left = static_cast<struct DHTNode*>(malloc(sizeof(struct DHTNode)));
		SetNode(null_node_ptr->left, 1, 0, nullptr, nullptr, null_node_ptr);
		null_node_ptr = null_node_ptr->left;

		null_node_ptr->right = static_cast<struct DHTNode*>(malloc(sizeof(struct DHTNode)));
		SetNode(null_node_ptr->right, 1, data, nullptr, nullptr, null_node_ptr);

		look_up_table[data] = null_node_ptr->right;
		BuildNodeList(look_up_table[data]);

		// increment frequencies of all its ancestors
		node_ptr = null_node_ptr->parent;
		while (node_ptr)
		{
			node_ptr->freq += 1;

			// reduce level of reverse level traversing (optimization)
			if (!update_node_ptr && node_ptr->parent && node_ptr->freq > node_list[(node_ptr->l_index) + 1]->freq)
				update_node_ptr = node_ptr;

			node_ptr = node_ptr->parent;
		}

		// correct the dynamic huffman tree if necessary
		if (update_node_ptr) CorrectDHT(list_idx);
	}
}

static void RemoveLookup(struct DHTNode* node_ptr)
{
	// to aid in cleanup / avoid calling free on a pointer twice in "reset lookup table"
	for (int i = 0; i < 257; i++)
	{
		if (look_up_table[i] == node_ptr)
			look_up_table[i] = nullptr;
	}
}

// clean-ups
static void DestroyDHT()
{
	int i;

	// free all nodes in the dynamic huffman tree
	// and reset the node list
	list_idx = 0;
	while (list_idx < MAX_LIST_LEN - 3)
	{
		if (node_list[list_idx])
		{
			RemoveLookup(node_list[list_idx]);
			free(node_list[list_idx]);
			node_list[list_idx] = nullptr;
		}
		++list_idx;
	}
	node_list[MAX_LIST_LEN - 2] = nullptr;
	node_list[MAX_LIST_LEN - 1] = nullptr;
	list_idx = MAX_LIST_LEN - 1;

	// reset lookup table
	for (i = 0; i < 257; i++)
	{
		if (look_up_table[i]) free(look_up_table[i]);
		look_up_table[i] = nullptr;
	}

	// reset the root node and null node pointer
	SetNode(&DHTroot, 0, 0, nullptr, nullptr, nullptr);
	null_node_ptr = &DHTroot;
	end_of_buffer = 0;
}

// return the reverse path for the data existing in the tree
static UINT32 ReverseDataPath(struct DHTNode* node_ptr)
{
	UINT32 path_reverse = 0;
	code_length = 0;

	while (node_ptr->parent)
	{
		path_reverse <<= 1;

		if (node_ptr->parent->left == node_ptr)
			path_reverse &= 0xFFFFFFFE; // fill the bit with 0
		else
			path_reverse |= 1; // fill the bit with 1

		node_ptr = node_ptr->parent;
		++code_length;
	}

	return path_reverse;
}

// generates and return a reverse code
static UINT32 GenerateReverseCode(unsigned char data)
{
	UINT32 path_reverse = 0;
	UINT32 code_reverse = 0;
	int i;

	// this data exists in the tree, so code is just
	// return the reverse path of the data
	if (look_up_table[data]) return ReverseDataPath(look_up_table[data]);

	// reverse the code
	for (i = 0; i < 8; i++)
	{
		code_reverse <<= 1; // shift path_reverse 1 bit to the left
		code_reverse |= data & 1; // get the first bit
		data >>= 1; // shift data 1 bit to the right
	}

	// tree is empty, the reverse code is
	// just the original data reversed.
	if (!DHTroot.freq)
	{
		code_length = 8;
		return code_reverse; // return the reversed code
	}

	// it's a new data. get the reverse path for the null
	// node and append it after the reverse code
	path_reverse = ReverseDataPath(node_list[list_idx]->parent);
	++code_length; // for the null node path (0)
	code_reverse <<= code_length;
	code_length += 8;
	return code_reverse | path_reverse;
}

//#
//#             Functions for Buffer Handling
//#
//##############################################################################

// takes a decompressed file and compresses the whole thing into another file
int Compress(char* d_file_name, char* c_file_name)
{
	int i, nRet;
	UINT32 code_slice;
	unsigned char data;
	int byte_count;
	int bytes_remain;

	nRet = OpenDecompressedFile(d_file_name, "rb");
	if (nRet) return nRet;

	nRet = OpenCompressedFile(c_file_name, "wb");
	if (nRet) return nRet;

	// get file size (# of bytes)
	fseek(dFile, 0,SEEK_END);
	byte_count = ftell(dFile);
	fseek(dFile, 0,SEEK_SET);

	// if buffer is smaller than the file, load max allowed bytes
	// into the buffer, otherwise, load everything into the buffer
	if (byte_count * 4 > MAX_BUFFER_LEN)
		fread(d_buffer, 4,MAX_BUFFER_LEN, dFile);
	else
		fread(d_buffer, 1, byte_count, dFile);

	d_buffer_idx = 0;
	code_slice = d_buffer[0];
	bytes_remain = byte_count % 4;
	byte_count /= 4;

	for (i = 0; i < byte_count; i++)
	{
		data = static_cast<unsigned char>(code_slice >> 24);
		EncodeBuffer(data);
		data = static_cast<unsigned char>((code_slice >> 16) & 0xFF);
		EncodeBuffer(data);
		data = static_cast<unsigned char>((code_slice >> 8) & 0xFF);
		EncodeBuffer(data);
		data = static_cast<unsigned char>(code_slice & 0xFF);
		EncodeBuffer(data);

		// prevent buffer overflow
		if (++d_buffer_idx == MAX_BUFFER_LEN) { ReloadBufferD(); }
		code_slice = d_buffer[d_buffer_idx];
	}
	if (bytes_remain)
	{
		code_slice <<= 32 - (bytes_remain * 8);
		for (i = 0; i < bytes_remain; i++)
		{
			data = static_cast<unsigned char>(code_slice >> 24);
			EncodeBuffer(data);
			code_slice <<= 8;
		}
	}
	CloseDecompressedFile();
	WriteCompressedFile();
	return nRet;
}

// takes a compressed file and decompresses the whole thing into another file
int Decompress(char* c_file_name, char* d_file_name)
{
	int remainder = 32;
	UINT32 code_slice = 0;
	int bytes_remain = 0;
	int nRet;

	nRet = OpenCompressedFile(c_file_name, "rb");
	if (nRet) return nRet;

	nRet = OpenDecompressedFile(d_file_name, "wb");
	if (nRet) return nRet;

	LoadCompressedFile();

	d_buffer[0] = 0;
	d_buffer_idx = 0;

	while (!end_of_buffer)
	{
		code_slice = static_cast<unsigned int>(DecodeBuffer());
		remainder -= 8;
		code_slice <<= remainder;
		d_buffer[d_buffer_idx] |= code_slice;

		if (!remainder)
		{
			if (++d_buffer_idx == MAX_BUFFER_LEN) ResetBufferD();
			d_buffer[d_buffer_idx] = 0;
			remainder = 32;
		}
	}
	bytes_remain = (32 - remainder) / 8;

	CloseCompressedFile();
	WriteDecompressedFile(bytes_remain);
	return nRet;
}

// take a data and encode it into c_buffer
void EncodeBuffer(unsigned char data)
{
	UINT32 code = 0; // code that is ready to put into the buffer
	UINT32 code_temp = 0; // used to break the code into two parts
	UINT32 code_reverse;
	int i;

	// at the beginning, reset everything
	// also fill 0s for the headers (coude_count, code_reset_count, buffer_reset_count)
	if (!DHTroot.freq)
	{
		dcs.bit_counter = 32;
		code_count = 0;
		code_reset_count = 0;
		c_buffer_idx = 0;

		// leave space for the 4 flags
		fwrite(&code_count, sizeof(int), 1, cFile);
		fwrite(&code_count, sizeof(int), 1, cFile);
		fwrite(&code_count, sizeof(int), 1, cFile);
		fwrite(&code_count, sizeof(int), 1, cFile);

		// init code buffer
		AllocBufferC(MAX_BUFFER_LEN);
		c_buffer[0] = 0;
	}

	// generate reverse code for the data
	code_reverse = GenerateReverseCode(data);

	// put the data in the tree
	BuildDHT(data);

	// check for overflow and reset code counter
	if (code_count + 1 > MAX_FREQ)
	{
		code_count = DHTroot.freq;
		++code_reset_count;
	}
	else ++code_count;

	// convert the reversed code to original code
	for (i = 0; i < code_length; i++)
	{
		code <<= 1;
		code |= code_reverse & 1;
		code_reverse >>= 1;
	}

	// calculate how many bits would remain in current buffer element after putting the code
	dcs.bit_counter -= code_length;

	// if enough bits left to put the code, just put the code
	if (dcs.bit_counter > 0)
	{
		code <<= dcs.bit_counter;
		c_buffer[c_buffer_idx] |= code;
	}
	// if it will fit in perfectly, put the code, and increment buffer index
	else if (dcs.bit_counter == 0)
	{
		c_buffer[c_buffer_idx] |= code;
		dcs.bit_counter = 32;

		// prevent buffer overflow
		if (++c_buffer_idx == c_buffer_size) GrowBufferC();

		// get next buffer element ready
		c_buffer[c_buffer_idx] = 0;
	}
	// if not enough bits left to put the code
	else
	{
		// retrieve part of the code
		for (i = 0; i > dcs.bit_counter; i--)
		{
			code_temp <<= 1;
			code_temp |= code & 1;
			code >>= 1;
		}
		// put part of the code in the remaining bits of the current buffer element
		c_buffer[c_buffer_idx] |= code;

		// retrieve rest of the code
		for (i = 0; i > dcs.bit_counter; i--)
		{
			code <<= 1;
			code |= code_temp & 1;
			code_temp >>= 1;
		}
		dcs.bit_counter += 32;
		code <<= dcs.bit_counter;
		// prevent buffer overflow
		if (++c_buffer_idx == c_buffer_size) GrowBufferC();

		// put the rest of the code in the next buffer element
		c_buffer[c_buffer_idx] = code;
	}
}

// decode and return the next data from c_buffer
unsigned char DecodeBuffer()
{
	struct DHTNode* node_ptr = &DHTroot; // start with the root node
	unsigned char data = 0; // decoded data
	int isData = 0;
	int i;

	// if it's the first code, reset everything and decode the first data
	// otherwise, get the next data
	if (!DHTroot.freq)
	{
		c_buffer_idx = 0;
		end_of_buffer = 0;

		dcs.code_slice = c_buffer[c_buffer_idx];
		data = static_cast<unsigned char>(dcs.code_slice >> 24);
		dcs.code_slice <<= 8;

		dcs.nCodeReset = 0;
		dcs.nCode = 0;
		dcs.bit_counter = 8;
	}
	else
	{
		do
		{
			isData = 0;

			// if it's the end of the code slice, get the next slice
			if (dcs.bit_counter == 32)
			{
				dcs.code_slice = c_buffer[++c_buffer_idx];
				dcs.bit_counter = 0;
			}

			// if next bit is 1, go to right. if it is 0, goto left
			if (dcs.code_slice & 0x80000000)
				node_ptr = node_ptr->right;
			else
				node_ptr = node_ptr->left;
			dcs.code_slice <<= 1;
			++dcs.bit_counter;

			// if it is a data node, just copy the data from that node
			if (node_ptr && !node_ptr->right)
			{
				data = node_ptr->value;
				isData = 1;
			}

			// if the it's a new data, get the next 8 bits
			else if (node_ptr == nullptr)
			{
				for (i = 0; i < 8; i++)
				{
					// if it's the end of the code slice, get the next slice.
					if (dcs.bit_counter == 32)
					{
						dcs.code_slice = c_buffer[++c_buffer_idx];
						dcs.bit_counter = 0;
					}
					data <<= 1;
					data |= dcs.code_slice >> 31;
					dcs.code_slice <<= 1;
					++dcs.bit_counter;
				}
				isData = 1;
			} // end of 'else if(node_ptr==NULL)'
		}
		while (!isData);
	} // end of 'if(DHTroot.freq)'

	BuildDHT(data);

	// prevent code count overflow
	if (dcs.nCode + 1 > MAX_FREQ)
	{
		dcs.nCode = DHTroot.freq;
		++dcs.nCodeReset;
	}
	else ++dcs.nCode;

	// if everything has been decoded, set end_of_buffer flag
	if (dcs.nCodeReset == code_reset_count && dcs.nCode == code_count)
	{
		end_of_buffer = 1;
	}

	return data; // return the decoded data
}

static void AllocBufferC(int size)
{
	c_buffer_size = size;
	c_buffer = static_cast<unsigned int*>(realloc(c_buffer, c_buffer_size * 4));
}

// prevent c_buffer overflow while encoding
static void GrowBufferC()
{
	// flush to disk periodically
	fwrite(c_buffer + c_buffer_size - MAX_BUFFER_LEN, 4,MAX_BUFFER_LEN, cFile);
	AllocBufferC(c_buffer_size + MAX_BUFFER_LEN);
}

// prevent d_buffer foverflow while encoding
static void ReloadBufferD()
{
	fread(d_buffer, 4,MAX_BUFFER_LEN, dFile);
	d_buffer_idx = 0;
}

// prevent d_buffer overflow while decoding
static void ResetBufferD()
{
	fwrite(d_buffer, 4,MAX_BUFFER_LEN, dFile);
	d_buffer_idx = 0;
}

//#
//#             Functions for File I/O
//#
//##############################################################################

// return 0 if the compressed file is opened sucessfully, 1 otherwise
int OpenCompressedFile(char* file_name, char* mode)
{
	if (cFile) fclose(cFile);
	cFile = nullptr;

	cFile = fopen(file_name, mode);
	if (cFile) return 0;

	return 1;
}

// Always returns 0
int EmbedCompressedFile(FILE* pEmb, int nOffset)
{
	if (cFile) fclose(cFile);
	cFile = nullptr;

	cFile = pEmb;
	bEmbed = 1;
	if (nOffset >= 0)
	{
		embed_offset = nOffset;
		fseek(cFile, nOffset,SEEK_SET);
	}
	else
	{
		if (nOffset == -2)
		{
			fseek(cFile, 0,SEEK_END);
		}
		embed_offset = ftell(cFile);
	}
	return 0;
}

// return 0 if the decompressed file is opened sucessfully, 1 otherwise
int OpenDecompressedFile(char* file_name, char* mode)
{
	dFile = nullptr;
	dFile = fopen(file_name, mode);
	if (dFile) return 0;
	return 1;
}

// load the compressed file into the c_buffer
// - call this function only if the compressed file is opened successfully
void LoadCompressedFile()
{
	int byte_count;
	int buffer_reset_count;

	// load the flags
	fread(&byte_count, sizeof(int), 1, cFile);
	fread(&code_count, sizeof(int), 1, cFile);
	fread(&code_reset_count, sizeof(int), 1, cFile);
	fread(&buffer_reset_count, sizeof(int), 1, cFile);

	// load everything into the buffer
	AllocBufferC((buffer_reset_count + 1) * MAX_BUFFER_LEN);
	fread(c_buffer, 1, byte_count - 16, cFile); // note:  byte_count includes the size of the headers
}

// this method is used when unfreezing the compression status
// it flushes the contents of the compressed file to disk
static void DoWriteCompressedFile()
{
	// save file size, code count, etc
	SaveResult();

	// handle file embedding
	fseek(cFile, embed_offset,SEEK_SET);

	fwrite(&nCompressedFileSize, sizeof(int), 1, cFile);
	fwrite(&nCodeCount, sizeof(int), 1, cFile);
	fwrite(&nCodeResetCount, sizeof(int), 1, cFile);
	fwrite(&nBufferResetCount, sizeof(int), 1, cFile);

	// flush entire contents of compress buffer
	fwrite(c_buffer, 4, c_buffer_idx + 1, cFile);
}

// write whatever is left in the c_buffer to the
// compressed file, and close the compressed file
// - use this if the compressed file was opened in 'write' mode
void WriteCompressedFile()
{
	if (cFile)
	{
		DoWriteCompressedFile();
		CloseCompressedFile();
	}
}

// write whatever is left in the d_buffer to the
// decompressed file, and close the decompressed file
// - use this if the decompressed file was opened in 'write' mode
void WriteDecompressedFile(int bytes_remain)
{
	UINT32 code_slice;
	int i;

	if (dFile)
	{
		// write the buffer to the file
		if (d_buffer_idx) fwrite(d_buffer, 4, d_buffer_idx, dFile);
		if (bytes_remain)
		{
			code_slice = d_buffer[d_buffer_idx];
			for (i = 0; i < (4 - bytes_remain); i++)
			{
				code_slice >>= 8;
			}
			fwrite(&code_slice, 1, bytes_remain, dFile);
		}

		fclose(dFile);
		dFile = nullptr;
	}
}

// close the compressed file
// - use this if the compressed file was opened in 'read' mode
void CloseCompressedFile()
{
	if (cFile)
	{
		// handle file embedding
		if (!bEmbed)
		{
			fclose(cFile);
		}

		cFile = nullptr;
		embed_offset = 0;
		bEmbed = 0;

#ifdef _DEBUG
		// PrintTree();
		CheckList();
#endif

		DestroyDHT();

		free(c_buffer);
		c_buffer = nullptr;
		c_buffer_size = 0;
	}
}

// close the decompressed file
// - use this if the decompressed file was opened in 'read' mode
void CloseDecompressedFile()
{
	if (dFile)
	{
		fclose(dFile);
		dFile = nullptr;
	}
}

//#
//#             Utility Functions for Debugging Purpose
//#
//##############################################################################

// print the node frequency while reverse level traversing
void PrintFreqTraverse()
{
	int curr_idx = list_idx;

	printf("Frequency Traverse -");
	while (node_list[curr_idx])
	{
		printf(" %d", node_list[curr_idx]->freq);
		if (node_list[curr_idx]->value) printf(":%c", node_list[curr_idx]->value);
		++curr_idx;
	}
}

// print the tree view (rotated -90 degrees)
void PrintTreeBreath(struct DHTNode* node_ptr, int spaces)
{
	int i;
	int num_digits;
	int tab_len;
	if (node_ptr)
	{
		num_digits = node_ptr->freq;
		tab_len = spaces;
		PrintTreeBreath(node_ptr->right, spaces + 7); // right tree first

		num_digits = num_digits / 10;
		while (num_digits)
		{
			--tab_len;
			num_digits = num_digits / 10;
		}
		for (i = 0; i < tab_len; i++) printf(" ");
		printf("%d", node_ptr->freq);
		if (node_ptr->value) printf(":%c", node_ptr->value);
		printf("\n");

		PrintTreeBreath(node_ptr->left, spaces + 7); // then left tree
	}
}

// print the header, then print the tree in breath order
void PrintTree()
{
	printf("View of the Tree (rotated -90 degrees)\n");
	PrintTreeBreath(&DHTroot, 0);
}

// check the reverse traverse list and print any frequencies that are out of order
#ifdef _DEBUG
static void CheckList()
{
	int curr_idx = list_idx;
	int count_errors = 0;
	while(curr_idx < MAX_LIST_LEN-3)
	{
		if(node_list[curr_idx]->freq > node_list[curr_idx+1]->freq)
		printf("\nError: list is out of order.\n%d\n%d\n",node_list[curr_idx]->freq,node_list[curr_idx+1]->freq);

		curr_idx++;
		count_errors++;
	}
	if(!count_errors)
		printf("\nNo errors in reverse traverse list.\n");
}
#endif

// dump the buffer to the screen
void PrintBuffer()
{
	UINT32 i, j;
	j = static_cast<unsigned int>(c_buffer_idx);

	printf("buffer = ");
	for (i = 0; i <= j; i++) printf("%.8X ", c_buffer[i]);
}

// save compression result
static void SaveResult()
{
	nCompressedFileSize = 16 + (c_buffer_idx + 1) * 4; // convert to Bytes
	nCodeCount = code_count;
	nCodeResetCount = code_reset_count;
	nBufferIdx = c_buffer_idx;
	// nBufferResetCount is retained for backwards compatability
	nBufferResetCount = c_buffer_idx / MAX_BUFFER_LEN;
}

// print file size, how many codes, etc
void PrintResult()
{
	printf("\ncompressed file size = %.3f KB\n\n", static_cast<double>(nCompressedFileSize) / 1024.0); // convert to KB
	printf("      max code count = %u\n", MAX_FREQ);
	printf("  current code count = %u\n", nCodeCount);
	printf("  # code count reset = %d\n\n", nCodeResetCount);
	printf("    max buffer size  = %.3f KB\n", static_cast<float>(4 * MAX_BUFFER_LEN) / 1024.0);
	printf("    max buffer index = %d\n", MAX_BUFFER_LEN);
	printf("current buffer index = %u\n", nBufferIdx);
	printf("    # buffer re-used = %d\n\n", nBufferResetCount);
}

//#
//#             Compression Status Freezing
//#
//##############################################################################

static inline void Write32(unsigned char*& ptr, const unsigned long v)
{
	*ptr++ = static_cast<unsigned char>(v & 0xff);
	*ptr++ = static_cast<unsigned char>((v >> 8) & 0xff);
	*ptr++ = static_cast<unsigned char>((v >> 16) & 0xff);
	*ptr++ = static_cast<unsigned char>((v >> 24) & 0xff);
}

static inline unsigned long Read32(const unsigned char*& ptr)
{
	unsigned long v;
	v = static_cast<unsigned long>(*ptr++);
	v |= static_cast<unsigned long>((*ptr++) << 8);
	v |= static_cast<unsigned long>((*ptr++) << 16);
	v |= static_cast<unsigned long>((*ptr++) << 24);
	return v;
}

static inline void Write16(unsigned char*& ptr, const unsigned short v)
{
	*ptr++ = static_cast<unsigned char>(v & 0xff);
	*ptr++ = static_cast<unsigned char>((v >> 8) & 0xff);
}

static inline unsigned short Read16(const unsigned char*& ptr)
{
	unsigned short v;
	v = static_cast<unsigned short>(*ptr++);
	v |= static_cast<unsigned short>((*ptr++) << 8);
	return v;
}

/*
struct DHTFreezeStruct
{
	struct DHTFreezeStatus header;
	struct DHTFreezeNode
	{
		unsigned int freq;
		unsigned char val;
		short left, right, parent
	} node_list[n_nodes]
}
*/

struct DHTFreezeStatus
{
	int n_nodes;
	unsigned long null_node_p;
	int bit_counter;
	unsigned int code_count;
	int code_reset_count;
	int c_buffer_idx;
};

static constexpr int DHTFreezeStatusSize = 6 * 4;
static constexpr int DHTNodeSize = 12;

static inline int IndexForNode(const DHTNode* node)
{
	if (!node) return 0xffff;
	return node->l_index;
}

static inline DHTNode* NodeForIndex(int idx)
{
	switch (idx)
	{
	case 0xffff:
		return nullptr;
	default:
		return node_list[idx];
	}
}

static void WriteNode(unsigned char*& ptr, const DHTNode* node)
{
	Write32(ptr, node->freq);
	Write16(ptr, node->value);
	Write16(ptr, IndexForNode(node->left));
	Write16(ptr, IndexForNode(node->right));
	Write16(ptr, IndexForNode(node->parent));
}

static void ReadNode(const unsigned char*& ptr, DHTNode* node)
{
	node->freq = Read32(ptr);
	node->value = Read16(ptr);
	node->left = NodeForIndex(Read16(ptr));
	node->right = NodeForIndex(Read16(ptr));
	node->parent = NodeForIndex(Read16(ptr));
}

static void FreezeDHT(const struct DHTFreezeStatus& status, unsigned char*& ptr)
{
	Write32(ptr, status.n_nodes);
	Write32(ptr, status.null_node_p);
	Write32(ptr, status.bit_counter);
	Write32(ptr, status.code_count);
	Write32(ptr, status.code_reset_count);
	Write32(ptr, status.c_buffer_idx);

	int n_nodes = status.n_nodes;
	while (n_nodes)
	{
		WriteNode(ptr, node_list[MAX_LIST_LEN - 1 - n_nodes]);
		--n_nodes;
	}
}

static int UnfreezeDHT(struct DHTFreezeStatus* status, const unsigned char*& ptr, const int buf_size)
{
	if (buf_size < DHTFreezeStatusSize)
	{
		return 1;
	}

	status->n_nodes = Read32(ptr);
	status->null_node_p = Read32(ptr);
	status->bit_counter = Read32(ptr);
	status->code_count = Read32(ptr);
	status->code_reset_count = Read32(ptr);
	status->c_buffer_idx = Read32(ptr);

	if (buf_size < (DHTFreezeStatusSize + status->n_nodes * DHTNodeSize))
	{
		return 1;
	}

	DestroyDHT();

	int n = status->n_nodes;
	if (n)
	{
		node_list[--list_idx] = &DHTroot;
		--n;
	}
	while (n)
	{
		node_list[--list_idx] = static_cast<struct DHTNode*>(malloc(sizeof(struct DHTNode)));
		--n;
	}

	n = status->n_nodes;
	while (n)
	{
		int idx = MAX_LIST_LEN - 1 - n;
		ReadNode(ptr, node_list[idx]);
		node_list[idx]->l_index = idx;
		if (!node_list[idx]->right)
		{
			look_up_table[node_list[idx]->value] = node_list[idx];
		}
		--n;
	}

	null_node_ptr = (status->null_node_p == 0xffffffff) ? &DHTroot : node_list[status->null_node_p];

	return 0;
}

int FreezeDecode(unsigned char** buf, int* size)
{
	// compute the size required for the buffer
	int n_nodes = MAX_LIST_LEN - 1 - list_idx;
	int dht_size = DHTFreezeStatusSize + n_nodes * DHTNodeSize;
	int buf_size = 4 + dht_size + (c_buffer_idx + 1) * 4;

	*buf = static_cast<unsigned char*>(malloc(buf_size));
	*size = buf_size;

	if (!(*buf))
	{
		return 1;
	}

	DHTFreezeStatus status;
	status.n_nodes = n_nodes;
	status.null_node_p = (null_node_ptr == &DHTroot) ? 0xffffffff : null_node_ptr->l_index;
	status.bit_counter = dcs.bit_counter;
	status.code_count = dcs.nCode;
	status.code_reset_count = dcs.nCodeReset;
	status.c_buffer_idx = c_buffer_idx;

	unsigned char* ptr = *buf;
	Write32(ptr, dht_size); // write DHT status size
	FreezeDHT(status, ptr); // write DHT status
	memcpy(ptr, c_buffer, (c_buffer_idx + 1) * 4); // write code buffer

	return 0;
}

int UnfreezeDecode(const unsigned char* buf, int size)
{
	if (size < 4)
	{
		return 1;
	}

	const unsigned char* ptr = buf;
	int dht_size = static_cast<int>(Read32(ptr)); // read DHT status size

	DHTFreezeStatus status;
	if (UnfreezeDHT(&status, ptr, dht_size)) // read DHT status
	{
		return 1;
	}

	// NOTE:
	// UnfreezeDecode is a special case.  In order to
	// implement "rewind" functionality, it's necessary
	// to restore the state of the decode, but leave
	// the contents of the code buffer intact

#if 0
	// restore code buffer
	int n_buffers   = status.c_buffer_idx/MAX_BUFFER_LEN;
	AllocBufferC((n_buffers+1)*MAX_BUFFER_LEN);
	memcpy(c_buffer, ptr, (status.c_buffer_idx+1)*4);	// read code buffer
#endif

	// Here we make a weak check against attempting to
	// seek past the end of the code buffer
	if (status.c_buffer_idx >= c_buffer_size)
	{
		return 1;
	}

	dcs.bit_counter = status.bit_counter;
	dcs.nCode = status.code_count;
	dcs.nCodeReset = status.code_reset_count;

	c_buffer_idx = status.c_buffer_idx;
	dcs.code_slice = c_buffer[c_buffer_idx] << dcs.bit_counter;

	return 0;
}

int FreezeEncode(unsigned char** buf, int* size)
{
	// compute the size required for the buffer
	int n_nodes = MAX_LIST_LEN - 1 - list_idx;
	int dht_size = DHTFreezeStatusSize + n_nodes * DHTNodeSize;
	int buf_size = 4 + dht_size + (c_buffer_idx + 1) * 4;

	*buf = static_cast<unsigned char*>(malloc(buf_size));
	*size = buf_size;

	if (!(*buf))
	{
		return 1;
	}

	DHTFreezeStatus status;
	status.n_nodes = n_nodes;
	status.null_node_p = (null_node_ptr == &DHTroot) ? 0xffffffff : null_node_ptr->l_index;
	status.bit_counter = 32 - dcs.bit_counter; // while encoding, bit_counter equals bits remaining in slice
	status.code_count = code_count;
	status.code_reset_count = code_reset_count;
	status.c_buffer_idx = c_buffer_idx;

	unsigned char* ptr = *buf;
	Write32(ptr, dht_size); // write DHT status size
	FreezeDHT(status, ptr); // write DHT status
	memcpy(ptr, c_buffer, (c_buffer_idx + 1) * 4); // write code buffer

	return 0;
}

int UnfreezeEncode(const unsigned char* buf, int size)
{
	if (size < 4)
	{
		return 1;
	}

	const unsigned char* ptr = buf;
	int dht_size = static_cast<int>(Read32(ptr)); // read DHT status size

	DHTFreezeStatus status;
	if (UnfreezeDHT(&status, ptr, dht_size)) // read DHT status
	{
		return 1;
	}

	int buf_size = 4 + dht_size + (status.c_buffer_idx + 1) * 4;
	if (buf_size > size)
	{
		return 1;
	}

	dcs.bit_counter = 32 - status.bit_counter; // while encoding, bit_counter equals bits remaining in slice
	code_count = status.code_count;
	code_reset_count = status.code_reset_count;
	c_buffer_idx = status.c_buffer_idx;

	int n_buffers = status.c_buffer_idx / MAX_BUFFER_LEN;
	AllocBufferC((n_buffers + 1) * MAX_BUFFER_LEN);
	memcpy(c_buffer, ptr, (status.c_buffer_idx + 1) * 4); // read code buffer
	DoWriteCompressedFile(); // flush to disk
	fseek(cFile, embed_offset + 16 + (n_buffers * MAX_BUFFER_LEN) * 4,SEEK_SET);
	// position write ptr for periodic flush

	// mask off the unread bits from the current code
	// this should be unneccessary, but it doesn't hurt
	c_buffer[c_buffer_idx] &= (0xffffffff << dcs.bit_counter);

	return 0;
}
