#ifndef BITSTREAM_H_
#define BITSTREAM_H_

#include "./jpeg.h"
// Handles bit stream and decoding of huffman values

typedef struct BitStream
{
    uint64_t buffer;
    uint64_t len;
} BitStream;

uint64_t ExtractBit(BitStream *bit_stream, uint64_t count, JPEG *img);
int64_t  DecodeMCU(BitStream *bit_stream, JPEG *jpeg, HTable *htable_DC, HTable* htable_AC);
bool     ExtractHuffmanEncoded(JPEG* jpeg);
bool     DecodeHuffmanStream(JPEG* jpeg);
#endif // BITSTREAM_H_
