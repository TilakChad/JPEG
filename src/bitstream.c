#include <stdlib.h>

#include "../utility/log.h"
#include "./bitstream.h"
#include "jpeg.h"

typedef struct Symbol
{
    uint8_t len;
    uint8_t val;
} Symbol;

uint64_t ExtractBits(BitStream *bit_stream, uint64_t count, JPEG *img)
{
    // Take bits from the MSB part right?
    while (count > bit_stream->len)
    {
        // fetch new bytes
        if (img->hstream.pos >= img->hstream.size)
        {
            Log(Error, "Decoding went too far ahead");
            return -1;
        }
        bit_stream->buffer = (bit_stream->buffer << 8) | img->hstream.buffer[img->hstream.pos++];
        bit_stream->len    = bit_stream->len + 8;
    }

    uint64_t to_return = bit_stream->buffer >> (bit_stream->len - count);
    bit_stream->buffer = bit_stream->buffer & ((1 << (bit_stream->len - count)) - 1);
    bit_stream->len    = bit_stream->len - count;
    return to_return;
}

Symbol DecodeHuffmanSymbol(BitStream *bit_stream, JPEG *jpeg, HTable *htable)
{
    uint16_t first = 0;
    uint16_t count = 0;
    uint16_t index = 0;
    uint32_t code  = 0;

    for (int i = 1; i <= 16; ++i) // 16 is the max code length that can be assigned to each symbol in JPEG
    {
        code  = code | ExtractBits(bit_stream, 1, jpeg);
        count = htable->code_length[i - 1];

        if (code - first < count)
        {
            printf("Index is : %d while decoding %d.\n", index + code - first, code);
            if (index + code - first >= htable->total_codes)
            {
                Log(Error, "Index out of range");
                exit(-1);
            }
            return (Symbol){i, htable->huffman_val[index + code - first]};
        }

        first = first + count;
        index = index + count;
        first = first << 1;
        code  = code << 1;
    }
    Log(Error, "Failed to decode huffman code");
    exit(0);
    return (Symbol){0};
}

bool ExtractHuffmanEncoded(JPEG *jpeg)
{
    // start from jpeg->current pos and extract all the relevant huffman encoded stream
    uint8_t current = 0;
    uint8_t last    = jpeg->buffer[jpeg->pos++];
    while (jpeg->pos < jpeg->size)
    {
        current = jpeg->buffer[jpeg->pos++];
        if (last == 0xFF)
        {
            // see if current is any weird marker
            if (current == EOI)
            {
                jpeg->hstream.size = jpeg->hstream.pos;
                jpeg->hstream.pos  = 0;
                Log(Warning, "Length of the encoded stream : %d.", jpeg->hstream.size);
                Log(Warning, "End of the File Parsed");
                return true;
            }
            else if (current >= RST0 && current <= RST7)
            {
                // Do nothing for now
                current = jpeg->buffer[jpeg->pos++]; // Basically skipping it
            }
            if (current == 0xFF)
            {
                continue;
            }
            else if (current == 0x00)
            {
                jpeg->hstream.buffer[jpeg->hstream.pos++] = last;
                current                                   = jpeg->buffer[jpeg->pos++];
            }
        }
        else
            jpeg->hstream.buffer[jpeg->hstream.pos++] = last;

        last = current;
    }
    Log(Error, "Invalid JPEG Encoded Stream, its a mess");
    return false;
}

int32_t InterpretValue(uint64_t val, uint8_t len, JPEG *jpeg)
{
    // If its top most bit is set to 1, its positive else its negative
    if (!len)
    {
        Log(Error, "Invalid length to be interpreted.");
        Log(Error, "Error occured at position %d.", jpeg->hstream.pos);
        exit(0);
        return 0;
    }
    if (val & (1 << (len - 1)))
        return (int32_t)val;

    // Else its negative number (i.e preceeded by 0)
    // So bitwise complement it and return negative value
    int32_t nval = ~val & ((1 << len) - 1);
    Log(Warning, "Val is %02X, len is %d and returned %d.", val, len, -nval);
    return -nval;
}

int32_t DecodeDC(BitStream *bit_stream, JPEG *jpeg, HTable *htable_dc)
{
    // It will return a symbol with its length
    Symbol  sym = DecodeHuffmanSymbol(bit_stream, jpeg, htable_dc);
    uint8_t len = sym.val & 0x0F; // Lower nibble contains length
    if (len == 0)
    {
        Log(Warning, "DC coefficients with length 0 found");
        return 0;
    }
    if (len > 11)
    {
        Log(Error, "DC coefficient greater than 11 bits");
        exit(-2);
    }
    uint64_t val = ExtractBits(bit_stream, len, jpeg);
    // Check whether its positive or negative
    return InterpretValue(val, len, jpeg);
}

// Fills the remaining ac coefficients in given mcu
void DecodeAC(BitStream *bit_stream, JPEG *jpeg, HTable *htable_ac, MCUBlock *mcu)
{
    // Now interpret the run length encoding
    uint8_t start = 1; // zero is to be filled by the DecodeDC functions
    while (start < 64)
    {
        Symbol  sym         = DecodeHuffmanSymbol(bit_stream, jpeg, htable_ac);
        uint8_t len         = sym.val & 0x0F;
        uint8_t zero_counts = (sym.val & 0xF0) >> 4;

        if (len > 10)
        {
            Log(Error, "AC Coefficients can't have length greater than 10");
            exit(-3);
        }

        if (len == 0 && zero_counts == 0)
        {
            for (uint8_t rem = start; rem < 64; ++rem)
                mcu->block[jpeg->zigzag.order[rem]] = 0;
            break;
        }

        if (len == 0 && zero_counts == 15)
        {
            for (uint8_t rem = start; rem < start + 16; ++rem)
                mcu->block[jpeg->zigzag.order[rem]] = 0;
            start = start + 16;
            continue;
        }

        if (len == 0)
        {
            Log(Error, "Len zero found, with count %d.", zero_counts);
        }

        while (zero_counts--) // TODO :: Look at it
        {
            mcu->block[jpeg->zigzag.order[start++]] = 0;
        }

        uint64_t val                            = ExtractBits(bit_stream, len, jpeg);
        mcu->block[jpeg->zigzag.order[start++]] = InterpretValue(val, len, jpeg);
    }
}

/* bool DecodeHuffmanStream(JPEG *jpeg) */
/* { */
/*     // for each mcu and each component, decode the value */
/*     // No of mcu are total no. of 8x8 blocks that can be adjusted in the image */
/*     // Divide the whole picture into grid of 8x8 blocks and that will give total no. of block used in jpeg */
/*     // Need to write a bmp writer too, that will help use visualize the image */
/*     int nmcu_h = (jpeg->img.width + 7) / 8; */
/*     int nmcu_w = (jpeg->img.height + 7) / 8; */

/*     // Allocate resource for mcu blocks */

/*     BitStream bit_stream = {0}; */

/*     for (uint32_t i = 0; i < jpeg->img.channels; ++i) */
/*     { */
/*         jpeg->img.components[i].mcu_counts = nmcu_h * nmcu_w; */
/*         jpeg->img.components[i].mcu_blocks = malloc(sizeof(*jpeg->img.components[i].mcu_blocks) * nmcu_h * nmcu_w);
 */
/*     } */

/*     // Previous differential DC values component wise */
/*     int32_t prevDC[4] = {0}; */

/*     // Print all DC tables and their components */
/*     // for (int mcu = 0; mcu < nmcu_h * nmcu_w; ++mcu) */
/*     for (int mcu = 0; mcu < nmcu_h * nmcu_w; ++mcu) */
/*     { */
/*         // For each component */
/*         for (int comp = 0; comp < jpeg->img.channels; ++comp) */
/*         { */
/*             MCUBlock *active_mcu = jpeg->img.components[comp].mcu_blocks + mcu; */
/*             // Decode the relevant MCU */
/*             // Choose the proper huffman table and decode the run length */
/*             // Component 0 is usually grayscale image */
/*             // Choose the AC ID and DC ID */

/*             // Now off to decoding actual image */
/*             active_mcu->block[0] = */
/*                 DecodeDC(&bit_stream, jpeg, &jpeg->huffman_tables.tables[jpeg->img.components[comp].htable_dc_index])
 * + */
/*                 prevDC[comp]; */
/*             prevDC[comp] = active_mcu->block[0]; */
/*             DecodeAC(&bit_stream, jpeg, &jpeg->huffman_tables.tables[jpeg->img.components[comp].htable_ac_index], */
/*                      active_mcu); */
/*         } */
/*         Log(Warning, "The ptr is at %d after mcu %d.", jpeg->hstream.pos, mcu); */
/*     } */
/*     putchar('\n'); */
/*     InverseQuantization(jpeg); */
/*     Log(Warning, "****************************** Printing the first decoded MCU ******************************"); */

/*     for (int i = 0; i < 8; ++i) */
/*     { */
/*         for (int j = 0; j < 8; ++j) */
/*             printf("%7d ", jpeg->img.components[0].mcu_blocks[0].block[i * 8 + j]); */
/*         putchar('\n'); */
/*     } */
/*     return true; */
/* } */

// bool DecodeHuffmanStreamChromaSubsampled(JPEG *jpeg)

bool DecodeHuffmanStream(JPEG *jpeg)
{
    // for each mcu and each component, decode the value
    // No of mcu are total no. of 8x8 blocks that can be adjusted in the image
    // Divide the whole picture into grid of 8x8 blocks and that will give total no. of block used in jpeg
    // Need to write a bmp writer too, that will help use visualize the image
    // Always assuming the maximum subsampling is 2 we have

    int nmcu_h = (jpeg->img.width + 7) / 8; // Number of horizontal block of 8-pixel if no subsampling done

    if (nmcu_h % jpeg->img.horizontal_subsampling) // Assuming its either 1 or 2
        nmcu_h += 1;

    int nmcu_w = (jpeg->img.height + 7) / 8;
    if (nmcu_w % jpeg->img.vertical_subsampling)
        nmcu_w += 1;

    // Allocate resource for mcu blocks

    BitStream bit_stream               = {0};
    jpeg->img.components[0].mcu_counts = nmcu_h * nmcu_w;
    jpeg->img.components[1].mcu_counts = (nmcu_h / jpeg->img.horizontal_subsampling) * (nmcu_w / jpeg->img.vertical_subsampling);
    jpeg->img.components[2].mcu_counts = (nmcu_h / jpeg->img.horizontal_subsampling) * (nmcu_w / jpeg->img.vertical_subsampling);


    for (uint32_t i = 0; i < jpeg->img.channels; ++i)
        jpeg->img.components[i].mcu_blocks = malloc(sizeof(*jpeg->img.components[i].mcu_blocks) * jpeg->img.components[i].mcu_counts);

    int total_mcus = 0;
    for (uint32_t i = 0; i < jpeg->img.channels; ++i)
        total_mcus += jpeg->img.components[i].mcu_counts;
    // Previous differential DC values component wise
    int32_t prevDC[4] = {0};

    // Print all DC tables and their components
    // for (int mcu = 0; mcu < nmcu_h * nmcu_w; ++mcu)
    int luma_count = 0;
    int cb_index = 0, cr_index = 0, luma_index = 0;
    const int luma_max_sample = jpeg->img.vertical_subsampling * jpeg->img.horizontal_subsampling;

    bool sampleCb = false, sampleCr = false, sampleLuma = true;

    for (int mcu = 0; mcu < total_mcus; ++mcu)
    {
        // TODO :: Write this loop better
        int comp = 0;
        if (luma_count < luma_max_sample)
        {
            luma_count++;
            sampleLuma = true;
            comp = 0;
        }
        else
        {
            if (!sampleCb && !sampleCr)
            {
                sampleLuma = false;
                sampleCb = true;
                comp     = 1;
            }
            else if (sampleCb && !sampleCr)
            {
                sampleCr = true;
                sampleCb = false;
                comp     = 2;
            }
            else
            {
                sampleCb   = false;
                sampleCr   = false;
                sampleLuma = true;
                luma_count = 1;
                comp       = 0;
            }
        }

        MCUBlock *active_mcu;
        if (sampleLuma)
            active_mcu = jpeg->img.components[comp].mcu_blocks + luma_index++;
        else if (sampleCb)
            active_mcu = jpeg->img.components[comp].mcu_blocks + cb_index++;
        else if (sampleCr)
            active_mcu = jpeg->img.components[comp].mcu_blocks + cr_index++;
        else
        {
            Log(Error,"Invalid sampling component.");
            exit(-4);
        }

        // Now off to decoding actual image
        active_mcu->block[0] =
            DecodeDC(&bit_stream, jpeg, &jpeg->huffman_tables.tables[jpeg->img.components[comp].htable_dc_index]) +
            prevDC[comp];
        prevDC[comp] = active_mcu->block[0];
        DecodeAC(&bit_stream, jpeg, &jpeg->huffman_tables.tables[jpeg->img.components[comp].htable_ac_index],
                 active_mcu);
        Log(Warning, "The ptr is at %d after mcu %d.", jpeg->hstream.pos, mcu);
    }

    putchar('\n');
    InverseQuantization(jpeg);
    Log(Warning, "****************************** Printing the first decoded MCU ******************************");

    for (int i = 0; i < 8; ++i)
    {
        for (int j = 0; j < 8; ++j)
            printf("%7d ", jpeg->img.components[0].mcu_blocks[0].block[i * 8 + j]);
        putchar('\n');
    }

    return true;
}
