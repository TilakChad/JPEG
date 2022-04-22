#include <stdbool.h>
#include <stdlib.h>

#include "../utility/log.h"
#include "./jpeg.h"

void DecodeHuffmanTable(HTable table)
{
    const char msg[]   = "01010011011110";
    uint8_t    i       = 0;
    uint16_t   buffer  = 0;

    bool       found   = false;
    int        n       = 4;
    int        decoded = 0;

    while (n--)
    {
        int first   = 0;
        int count   = 0;
        int index   = 0;
        int counter = 1;
        buffer      = 0;
        while (1)
        {
            buffer = buffer << 1;
            if (msg[i++] - '0')
                buffer = buffer | 1;
            else
                buffer = buffer | 0;
            count = table.code_length[counter++ - 1];
            if (buffer - first < count)
            {
                found   = true;
                decoded = index + buffer - first;
                break;
            }

            first = first + count;
            index = index + count;
            first <<= 1;
        }
        if (found)
        {
            Log(Info, "Found the huffman values : ");
            Log(Info, "Decoded index was : %d.", decoded);
        }
        else
            Log(Error, "Failed to find the required huffman index");
    }
}

void PrintCode(int length, int code)
{

    for (int i = length - 1; i >= 0; --i)
    {
        if (code & (1 << i))
            printf("1");
        else
            printf("0");
    }
}

void PrettyPrintHuffman(HTable htable)
{
    Log(Warning, "\n****************************** Pretty Printing Huffman ******************************");
    int      start = 0;

    uint16_t code_length[16];
    uint16_t offset[16] = {0};

    for (int i = 1; i < 16; ++i)
    {
        offset[i] = htable.code_length[i - 1] + offset[i - 1];
        offset[i] = offset[i] << 1;
    }
    memcpy(code_length, htable.code_length, sizeof(code_length));

    for (int i = 0; i < htable.total_codes; ++i)
    {
        if (htable.code_length[start] == 0)
            for (int k = start; k < 16; ++k)
                if (htable.code_length[k] != 0)
                {
                    start = k;
                    break;
                }

       htable.huffman_code[i] = offset[start]++;
       htable.code_length[start]--;
    }

    start = 0;
    for (uint8_t i = 0; i < htable.total_codes; ++i)
    {
        if (code_length[start] == 0)
            for (int k = start; k < 16; ++k)
                if (code_length[k] != 0)
                {
                    start = k;
                    break;
                }
        printf("Index : %d -> ", i);
        PrintCode(start + 1, htable.huffman_code[i]);
        code_length[start]--;

        putchar('\n');
    }
    printf("\n8th Huffman val is %02X.\n",htable.huffman_val[7]);
}

bool HuffmanSegment(JPEG *jpeg)
{
    Log(Info, "------------------------------ Into the Huffman Segment ------------------------------");
    uint16_t      length         = GetMarkerLength(jpeg->buffer + jpeg->pos);
    int           count          = 2;

    HuffmanTable *huffman_tables = &jpeg->huffman_tables;

    while (count < length)
    {
        uint8_t DC_AC = (jpeg->buffer[jpeg->pos + count] & 0x10) >> 4;
        uint8_t id    = jpeg->buffer[jpeg->pos + count] & 0x0F;

        count         = count + 1;
        Log(Info, "Length : %u, DC_AC : %02X, id : %02X", length, DC_AC, id);

        if (DC_AC == 0)
            huffman_tables->tables[huffman_tables->count].type = DC;
        else
            huffman_tables->tables[huffman_tables->count].type = AC;

        huffman_tables->tables[huffman_tables->count].id = id;

        for (int i = 0; i < 16; ++i)
            huffman_tables->tables[huffman_tables->count].code_length[i] = jpeg->buffer[jpeg->pos + count++];

#ifdef _JPEG_DEBUG
        Log(Info, "Found huffman encoding for DC/AC Values");
        for (int i = 0; i < 16; ++i)
            printf("\nCodewords of length %d : %d.", i + 1, code_length[i]);
#endif
        int total_codes = 0;
        for (int i = 0; i < 16; ++i)
            total_codes += huffman_tables->tables[huffman_tables->count].code_length[i];

        // We now need an dynamic array of length total codes which will contain the equivalent symbol for given
        // code length for DC components only

        huffman_tables->tables[huffman_tables->count].total_codes = total_codes;
        // TODO :: Glare here
        huffman_tables->tables[huffman_tables->count].huffman_val =
            malloc(sizeof(*huffman_tables->tables[0].huffman_val) * total_codes);
        huffman_tables->tables[huffman_tables->count].huffman_code =
            malloc(sizeof(*huffman_tables->tables[0].huffman_code) * total_codes);

        for (int i = 0; i < total_codes; ++i)
            huffman_tables->tables[huffman_tables->count].huffman_val[i] = jpeg->buffer[jpeg->pos + count++];

#ifdef _JPEG_DEBUG
        uint16_t offset[16] = {0};
        for (int i = 1; i < 16; ++i)
        {
            offset[i] = offset[i - 1] + huffman_tables->tables[huffman_tables->count].code_length[i - 1];
            offset[i] <<= 1;
        }
        // Read each code using huffman table
        // Now write the actual huffman codes to it or we could have just used offset array
        /* int start = 0; */

        /* for (int i = 0; i < total_codes; ++i) */
        /* { */
        /*     if (huffman_tables->tables[huffman_tables->count].code_length[start] == 0) */
        /*         for (int k = start; k < 16; ++k) */
        /*             if (huffman_tables->tables[huffman_tables->count].code_length[k] != 0) */
        /*             { */
        /*                 start = k; */
        /*                 break; */
        /*             } */

        /*     // This should have been trivial task, but my little stupid mind won't work. */
        /*     huffman_tables->tables[huffman_tables->count]..huffman_code[i] = offset[start]++; */
        /*     code_length[start]--; */
        /* } */

        for (int i = 0; i < total_codes; ++i)
        {
            printf("Length using huffman encoding are : %d.\n",
                   huffman_tables->tables[huffman_tables->count].huffman_code[i]);
        }
        PrettyPrintHuffman(huffman_tables->tables[huffman_tables->count]);
#endif

        huffman_tables->count++;
    }

    Log(Warning, "Length : %d and count : %d", length, count);
    jpeg->pos += length;
    return true;
}

bool QuantizationSegment(JPEG *jpeg)
{
    Log(Info, "------------------------------ Into the Quantization Segment ------------------------------");
    uint16_t length = GetMarkerLength(jpeg->buffer + jpeg->pos);
    Log(Info, "Length of the quantization segment is : %u.", length);

    // TODO :: Zig-Zagify it
    int                count        = 2;
    QuantizationTable *quant_tables = &jpeg->quantization_tables;
    while (count < length)
    {
        uint8_t id                                           = jpeg->buffer[jpeg->pos + count] & 0x0F;
        uint8_t precision                                    = jpeg->buffer[jpeg->pos + count] & 0xF0;

        quant_tables->qtables[quant_tables->count].precision = precision;
        quant_tables->qtables[quant_tables->count].id        = id;

        Log(Info, "Precision of Quantization Segment is : %d and %d.", precision, id);
        count++;
        uint32_t index = 0;
        for (int i = 0; i < 8; ++i)
        {
            for (int j = 0; j < 8; ++j)
            {
                quant_tables->qtables[quant_tables->count].data[jpeg->zigzag.order[index++]] = jpeg->buffer[jpeg->pos + count++];
            }
        }

        Log(Warning, "------------------------------ Quantization Table extracted is : ------------------------------");
        index = 0;
        for (int i = 0; i < 8; ++i)
        {
            for (int j = 0; j < 8; ++j)
            {
                printf("%10u", quant_tables->qtables[quant_tables->count].data[index++]);
            }
            putchar('\n');
        }
        quant_tables->count++;
    }
    jpeg->pos += length;
    Log(Warning, "Count : %d and Length %d.", count, length);
    return true;
}
