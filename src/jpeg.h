#ifndef JPEG_H_
#define JPEG_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum AC_DC
{
    AC,
    DC
} AC_DC;

typedef struct QTable
{
    uint8_t  precision;
    uint8_t  id;
    uint16_t data[64];
} QTable;

typedef struct HTable
{
    AC_DC     type;
    uint8_t   id;
    uint16_t  total_codes;
    uint16_t  code_length[16];
    uint16_t *huffman_code;
    uint16_t *huffman_val;
} HTable;

typedef struct HuffmanTable
{
    uint8_t count;
    HTable *tables;
} HuffmanTable;

typedef struct QuantizationTable
{
    uint8_t count;
    QTable *qtables;
} QuantizationTable;

typedef enum MarkerType
{
    SOF0 = 0xC0,
    SOF1 = 0xC2,
    DHT  = 0xC4,
    DQT  = 0xDB,
    DRI  = 0xDD,
    SOS  = 0xDA,
    COM  = 0xFE,
    EOI  = 0xD9,
    SOI  = 0xD8,
    RST0 = 0xD0,
    RST7 = 0xD7
} MarkerType;

typedef struct MCUBlock
{
    int16_t block[64];
} MCUBlock;

typedef struct JPEGComponent
{
    uint8_t identifier;
    uint8_t HiVi;
    uint8_t qtableptr;
    // Hmm.. instead of actually storing the id, directly store the huffman tables for each one
    uint8_t   DC_id; // huffman ids
    uint8_t   AC_id;

    uint8_t htable_ac_index;
    uint8_t htable_dc_index;
    uint8_t qtable_index;

    uint16_t  mcu_counts;
    MCUBlock *mcu_blocks;
} JPEGComponent;


typedef struct JPEGInfo
{
    uint8_t horizontal_subsampling;
    uint8_t vertical_subsampling;

    uint32_t height;
    uint32_t width;
    uint32_t depth;
    uint32_t channels;
    // possibly buffer here
    // It may range from 0 to 4 depending upon image type
    JPEGComponent components[4];
} JPEGInfo;

typedef struct HuffmanEncodedStream
{
    uint64_t pos;
    uint64_t size;
    uint64_t capacity;
    uint8_t *buffer;
} HuffmanEncodedStream;

typedef struct JPEG
{
    uint64_t             pos;
    uint64_t             size;
    uint8_t             *buffer;

    struct {
        uint8_t order[64];
    } zigzag;

    JPEGInfo             img;
    HuffmanTable         huffman_tables;
    QuantizationTable    quantization_tables;
    HuffmanEncodedStream hstream;
} JPEG;

uint16_t GetMarkerLength(uint8_t *buffer);
bool     HuffmanSegment(JPEG *img);
bool     QuantizationSegment(JPEG *img);

// Helper
void PrettyPrintHuffman(HTable htable);

void InverseCosineTransform(JPEG* jpeg);
bool InverseQuantization(JPEG* jpeg);
void InverseSignedNormalization(JPEG* jpeg);
void YCbCrToRGB(JPEG* jpeg, uint8_t* img_data);
#endif // JPEG_H_
