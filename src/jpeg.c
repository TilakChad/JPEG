#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "./bitstream.h"
#include "./jpeg.h"

#include "../utility/bmp.h"
#include "../utility/log.h"

// JPEG decompressor using Inverse Cosine Transform
void ProgressiveDCT(JPEG *img);
void BaselineDCT(JPEG *img);

void StartOfScanSegment(JPEG *img);
void DefineRestartIntervalSegment(JPEG *img);

void InitJPEGDecoder(JPEG *jpeg);
void DecodeJPEG(JPEG *jpeg, HTable *htable, QTable *qtable);

void JPEGtoBMP(JPEG *jpeg, const char *output_file);

JPEG LoadJpegFile(const char *path)
{
    JPEG  image = {0};
    FILE *fp    = fopen(path, "rb");
    if (!fp)
    {
        fprintf(stderr, "Error : Failed to open file %s.\n", path);
        return (JPEG){0};
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);

    rewind(fp);

    image.buffer    = malloc(sizeof(*image.buffer) * (size + 1));
    size_t readSize = fread(image.buffer, sizeof(*image.buffer), size + 1, fp);

    image.size      = readSize;
    if (readSize != size)
    {
        fprintf(stderr, "Warning : Unknown error in opening file %s.\n", path);
        return (JPEG){0};
    }
    InitJPEGDecoder(&image);
    return image;
}

bool ValidateJPEGHeader(JPEG *image)
{
    if (image->buffer[0] != 0xFF || image->buffer[1] != 0xD8)
    {
        fprintf(stdout, "\nInvalid JPEG file\n");
        return false;
    }
    Log(Info, "Valid JPG File");
    image->pos += 2;
    return true;
}

bool IsAPPMarker(uint8_t byte)
{
    for (uint8_t x = 0xE0; x <= 0xEF; ++x)
        if (x == byte)
            return true;
    return false;
}

bool IsRSTMarker(uint8_t byte)
{
    for (uint8_t x = 0xD0; x <= 0xD7; ++x)
        if (x == byte)
            return true;
    return false;
}

uint16_t GetMarkerLength(uint8_t *buffer)
{
    return (buffer[0] << 8) | buffer[1];
}

void HandleAPPHeaders(JPEG *image)
{
    // Be aware of thumbnail datas here
    while (image->pos < image->size)
    {
        uint8_t next_byte = image->buffer[image->pos + 1];
        if (image->buffer[image->pos] == 0xFF)
        {
            // Try to examine the next pos

            if (IsAPPMarker(next_byte))
            {
                // Do something stupid here, that is parse whole Exif format or just glare and skip over it
                // Two bytes following this are length of the segment
                // Skip over everything that is not known
                image->pos += 2;
                if (next_byte == 0xE0) // EXIF metadata
                {
                    // Don't bother with it
                    uint16_t len = GetMarkerLength(image->buffer + image->pos);
                    // skip these
                    image->pos = image->pos + len;
                }
                else
                {
                    // Read length and skip over it
                    uint16_t len = GetMarkerLength(image->buffer + image->pos);
                    image->pos   = image->pos + len;
                    Log(Warning, "Skipping over 0xEE 0x%02X of length %u.", next_byte, len);
                }
            }
            else if (next_byte == SOF0) // baseline DCT
            {
                image->pos = image->pos + 2;
                BaselineDCT(image);
            }
            else if (next_byte == SOF1)
            {
                image->pos = image->pos + 2;
                ProgressiveDCT(image);
            }
            else if (next_byte == DQT)
            {
                image->pos += 2;
                QuantizationSegment(image);
            }
            else if (next_byte == DHT)
            {
                image->pos += 2;
                HuffmanSegment(image);
            }
            else if (next_byte == EOI)
            {
                fprintf(stderr, "\nEnd of the image reached at %lu.", image->pos);
            }
            else if (IsRSTMarker(next_byte))
            {
                Log(Info, "Skipped over the RST Marker");
                image->pos += 2;
            }
            else if (next_byte == DRI)
            {
                image->pos += 2;
                DefineRestartIntervalSegment(image);
            }
            else if (next_byte == SOS)
            {
                // Start of Scan segment
                image->pos += 2;
                StartOfScanSegment(image);
            }
            else
            {
                // Read the length and skip over it
                image->pos      = image->pos + 2;
                uint16_t length = GetMarkerLength(image->buffer + image->pos);
                Log(Warning, "Skipping over marker 0xFF %02X with length %u", next_byte, length);
                // skip over it
                image->pos += length;
            }
        }
        else
        {
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Insufficient argument provided\nUSAGE : exe ./img.jpg\n");
        return -1;
    }
    JPEG image = LoadJpegFile(argv[1]);
    if (!ValidateJPEGHeader(&image))
    {
        fprintf(stderr, "Not a valid JPEG file\n");
        return -3;
    }
    HandleAPPHeaders(&image);
    return 0;
}

void ProgressiveDCT(JPEG *img)
{
    Log(Info,
        "------------------------------ Progressive Discrete Cosine Transformed JPEG ------------------------------");
}

void BaselineDCT(JPEG *img)
{
    Log(Info,
        "\n------------------------------ Baseline Discrete Cosine Transformed JPEG ------------------------------");
    // Read the length of the image data
    uint16_t size      = GetMarkerLength(img->buffer + img->pos);
    uint8_t  bit_depth = img->buffer[img->pos + 2];

    uint16_t height    = (img->buffer[img->pos + 3] << 8) | img->buffer[img->pos + 4];
    uint16_t width     = (img->buffer[img->pos + 5] << 8) | img->buffer[img->pos + 6];

    Log(Info, "Height is : %u and Width is %u.", height, width);
    Log(Info, "Size of segment : %u.", size);

    uint8_t channels = img->buffer[img->pos + 7];
    // Fill image information
    img->img.width    = width;
    img->img.height   = height;
    img->img.depth    = bit_depth;
    img->img.channels = channels;

    img->pos += 8;
    // Fill the components section
    for (int comp = 0; comp < channels; ++comp)
    {
        img->img.components[comp].identifier = img->buffer[img->pos];
        img->img.components[comp].HiVi       = img->buffer[img->pos + 1];
        img->img.components[comp].qtableptr  = img->buffer[img->pos + 2];
        img->pos                             = img->pos + 3;

        Log(Info, "For component %d : \n\tIdentifier : %02X \n\t HiVi    : %02X \n\t QTablePtr : %02X", comp,
            img->img.components[comp].identifier, img->img.components[comp].HiVi, img->img.components[comp].qtableptr);
    }
}

void StartOfScanSegment(JPEG *img)
{
    uint16_t length = GetMarkerLength(img->buffer + img->pos);

    Log(Info, " ------------------------------ Start of Scan Segment ------------------------------");
    Log(Info, "Length should be 12, right? -> %u.", length);

    uint8_t  no_of_components = img->buffer[img->pos + 2];
    uint16_t count            = 3;

    for (int i = 0; i < no_of_components; ++i)
    {
        // For each components two bytes
        // First is the component identifier, defined in above jpeg struct
        // Next bytes, DC huffman | AC huffman, 4 bits each
        uint8_t id    = img->buffer[img->pos + count++];
        int     AC_id = img->buffer[img->pos + count] & 0x0F;
        int     DC_id = (img->buffer[img->pos + count] & 0xF0) >> 4;
        count         = count + 1;

        Log(Info, "Component identifier is : %d.", id);
        printf("\t\tDC huffman : %d and AC huffman : %d.\n", DC_id, AC_id);

        int found = true;
        for (int i = 0; i < img->img.channels; ++i)
        {
            if (id == img->img.components[i].identifier)
            {
                img->img.components[i].AC_id = AC_id;
                img->img.components[i].DC_id = DC_id;
                // store the pointer to the relevant DC

                // Look for the AC and DC huffman table
                bool ac_found = false, dc_found = false;
                for (uint8_t k = 0; k < img->huffman_tables.count; ++k)
                {
                    if (img->huffman_tables.tables[k].type == AC)
                    {
                        if (img->huffman_tables.tables[k].id == AC_id)
                        {
                            ac_found                               = true;
                            img->img.components[i].htable_ac_index = k;
                        }
                    }
                    else
                    {
                        if (img->huffman_tables.tables[k].id == DC_id)
                        {
                            dc_found                               = true;
                            img->img.components[i].htable_dc_index = k;
                        }
                    }
                    if (ac_found && dc_found)
                        break;
                }

                if (!ac_found || !dc_found)
                {
                    Log(Error, "Failed to find AC table for component %d.", id);
                    found = false;
                    break;
                }
            }
        }
        if (!found)
        {
            Log(Error, "Failed to find corresponding identifier %d.", id);
        }
    }
    Log(Info, "Last 3 bytes should be 0 63 and 0 -> Is it %d %d %d?", img->buffer[img->pos + count],
        img->buffer[img->pos + count + 1], img->buffer[img->pos + count + 2]);
    count    = count + 3;
    img->pos = img->pos + count;
    // Now comes the actually encoded data
    // Loop over till the number of components are consumed
    ExtractHuffmanEncoded(img);
    DecodeHuffmanStream(img);
    Log(Info, "JPEG decoded without any error :D");

    InverseCosineTransform(img);

    InverseSignedNormalization(img);
    // Now comes the merging part, but before that inverse discrete cosine transform
    // Lets try writing the grayscale image to the bmp format though
    JPEGtoBMP(img, "whatever.bmp");
}

void InitJPEGDecoder(JPEG *jpeg)
{
    jpeg->huffman_tables.tables =
        malloc(sizeof(*jpeg->huffman_tables.tables) * 6); // A max of 6 huffman tables are allocated
    jpeg->quantization_tables.qtables =
        malloc(sizeof(*jpeg->quantization_tables.qtables) * 6); // Its around 4 but lets leave it

    jpeg->quantization_tables.count = 0;
    jpeg->huffman_tables.count      = 0;

    // Make place for encoded stream of huffman
    jpeg->hstream.pos      = 0;
    jpeg->hstream.size     = 0;
    jpeg->hstream.capacity = jpeg->size;
    jpeg->hstream.buffer   = malloc(sizeof(*jpeg->hstream.buffer) * jpeg->size);

    // Initialize the zigzag order
    int     x = 0, y = 0;
    int     arrow = 1;

    uint8_t index = 0;
    while (x != 7 || y != 7)
    {
        jpeg->zigzag.order[index++] = x * 8 + y;
        if ((y % 2 == 0) && (x == 0 || x == 7))
        {
            y = y + 1;
            arrow *= -1;
        }
        else if ((x % 2 == 1) && (y == 0 || y == 7))
        {
            x = x + 1;
            arrow *= -1;
        }
        else
        {
            x = x - arrow;
            y = y + arrow;
        }
    }
    jpeg->zigzag.order[index] = 63;
    Log(Warning, "ZigZag Ordering is : \n");
    for (uint8_t x = 0; x < 8; ++x)
    {
        for (uint8_t y = 0; y < 8; ++y)
        {
            printf("%6d  ", jpeg->zigzag.order[x * 8 + y]);
        }
        putchar('\n');
    }
}

void DecodeJPEG(JPEG *jpeg, HTable *htable, QTable *qtable)
{
}

void DefineRestartIntervalSegment(JPEG *jpeg)
{
    uint16_t length = GetMarkerLength(jpeg->buffer + jpeg->pos);
    Log(Warning, "Got to JPEG Restart Interval and skipped with length : %d.", length);
    jpeg->pos += length;
}

void JPEGtoBMP(JPEG *jpeg, const char *output)
{
    // Lets try writing only the luminance part, not the chrominance part
    uint32_t nmcu_h = (jpeg->img.height + 7) / 8;
    uint32_t nmcu_w = (jpeg->img.width + 7) / 8;

    // Need to write the color pixels
    // Form an 1D array

    uint32_t totalpixels = 0;
    uint8_t *image_data  = malloc(sizeof(*image_data) * (jpeg->img.width * jpeg->img.height * jpeg->img.channels));

    for (uint32_t mcu_h = 0; mcu_h < nmcu_h; ++mcu_h)
    {
        for (uint32_t mcu_w = 0; mcu_w < nmcu_w; ++mcu_w)
        {
            uint8_t *ptr  = image_data + (mcu_h * 8 * jpeg->img.width + mcu_w * 8) * jpeg->img.channels;

            uint32_t colX = 8;
            if ((mcu_w + 1) * 8 > jpeg->img.width)
                colX = jpeg->img.width % 8;
            for (uint32_t row = 0; row < 8; ++row)
            {
                for (uint32_t col = 0; col < colX; ++col)
                {
                    totalpixels++;
                    for (uint8_t i = 0; i < jpeg->img.channels; ++i)
                        *(ptr++) = jpeg->img.components[i].mcu_blocks[mcu_h * nmcu_w + mcu_w].block[row * 8 + col];
                    /* for (uint8_t i = 0; i < 3; ++i) */
                    /*     *(ptr++) = jpeg->img.components[0].mcu_blocks[mcu_h * nmcu_w + mcu_w].block[row * 8 + col]; */
                }
                ptr = ptr - colX * jpeg->img.channels;
                ptr = ptr + jpeg->img.width * jpeg->img.channels;
            }
        }
    }

    YCbCrToRGB(jpeg, image_data);
    Log(Warning, "Total pixel counts were approximately : %u.", totalpixels);
    BMP bmp = {0};
    // Memory for extra padding bytes are to be allocated seperately
    InitBMP(&bmp, jpeg->img.width * jpeg->img.height * jpeg->img.channels + 1000, jpeg->img.channels, true);
    WriteBMPHeader(&bmp);
    WriteBMPData(&bmp, image_data, jpeg->img.width, jpeg->img.height, jpeg->img.channels);
    WriteBMPToFile(&bmp, output);
}

void ApplyInvQuantization(MCUBlock *mcu, QTable *qtable)
{
    for (uint8_t i = 0; i < 64; ++i)
        mcu->block[i] = mcu->block[i] * qtable->data[i];
}

bool InverseQuantization(JPEG *jpeg)
{
    // for every dct block and every components inverse the quantization
    uint32_t nmcu_h = (jpeg->img.height + 7) / 8;
    uint32_t nmcu_w = (jpeg->img.width + 7) / 8;

    for (uint8_t comp = 0; comp < jpeg->img.channels; ++comp)
    {
        if (jpeg->img.components[comp].qtableptr >= jpeg->quantization_tables.count)
        {
            Log(Error, "Invalid quantizationt table.");
            exit(-1);
        }
        for (uint32_t mcu = 0; mcu < nmcu_h * nmcu_w; ++mcu)
        {
            ApplyInvQuantization(&jpeg->img.components[comp].mcu_blocks[mcu],
                                 &jpeg->quantization_tables.qtables[jpeg->img.components[comp].qtableptr]);
        }
    }
    return true;
}

float alpha(uint8_t u)
{
    if (!u)
        return 1.0f / sqrtf(2);
    return 1.0f;
}

void InverseCosineTransform(JPEG *jpeg)
{
    uint32_t nmcu_h = (jpeg->img.height + 7) / 8;
    uint32_t nmcu_w = (jpeg->img.width + 7) / 8;

    Log(Warning, "------------------------------ Before Cosine Transform ------------------------------");
    for (uint16_t mcu = 0; mcu < 200; ++mcu)
    {
        for (uint8_t i = 0; i < 8; ++i)
        {
            for (uint8_t j = 0; j < 8; ++j)
            {
                printf("%6d  ", jpeg->img.components[0].mcu_blocks[mcu].block[i * 8 + j]);
            }
            putchar('\n');
        }
        putchar('\n');
    }

    // For each block now apply the inverse discrete cosine transform
    for (uint8_t comp = 0; comp < jpeg->img.channels; ++comp)
    {
        for (uint32_t mcu = 0; mcu < nmcu_h * nmcu_w; ++mcu)
        {
            MCUBlock  dctblock = jpeg->img.components[comp].mcu_blocks[mcu];
            MCUBlock *curblock = jpeg->img.components[comp].mcu_blocks + mcu;

            for (uint8_t x = 0; x < 8; ++x)
            {
                for (uint8_t y = 0; y < 8; ++y)
                {
                    float val = 0.0f;
                    for (uint8_t u = 0; u < 8; ++u)
                    {
                        for (uint8_t v = 0; v < 8; ++v)
                        {
                            float arg0 = (2 * x + 1) * u * M_PI;
                            float arg1 = (2 * y + 1) * v * M_PI;
                            val        = val +
                                  alpha(u) * alpha(v) * dctblock.block[u * 8 + v] * cosf(arg0 / 16) * cosf(arg1 / 16);
                        }
                    }
                    curblock->block[x * 8 + y] = roundf(0.25f * val);
                }
            }
        }
    }
    Log(Warning, "------------------------------ Inverse Cosine Transform first MCU ------------------------------");
    for (uint16_t mcu = 0; mcu < 110; ++mcu)
    {
        for (uint8_t i = 0; i < 8; ++i)
        {
            for (uint8_t j = 0; j < 8; ++j)
            {
                printf("%6d  ", jpeg->img.components[0].mcu_blocks[mcu].block[i * 8 + j]);
            }
            putchar('\n');
        }
        putchar('\n');
    }
}

uint8_t clamp0_255(int16_t val)
{
    if (val < 0)
        return 0;
    if (val > 255)
        return 255;
    return val;
}
void InverseSignedNormalization(JPEG *jpeg)
{
    uint32_t nmcu_h = (jpeg->img.height + 7) / 8;
    uint32_t nmcu_w = (jpeg->img.width + 7) / 8;

    // For each block now apply the inverse discrete cosine transform
    for (uint8_t comp = 0; comp < jpeg->img.channels; ++comp)
    {
        for (uint32_t mcu = 0; mcu < nmcu_h * nmcu_w; ++mcu)
        {
            for (uint8_t i = 0; i < 64; ++i)
            {
                jpeg->img.components[comp].mcu_blocks[mcu].block[i] += 128;
                jpeg->img.components[comp].mcu_blocks[mcu].block[i] =
                    clamp0_255(jpeg->img.components[comp].mcu_blocks[mcu].block[i]);
            }
        }
    }
}

void YCbCrToRGB(JPEG *jpeg, uint8_t *img_data)
{
    int16_t colors[4];
    for (uint32_t h = 0; h < jpeg->img.height; ++h)
    {
        for (uint32_t w = 0; w < jpeg->img.width; ++w)
        {
            colors[0] = img_data[0];
            colors[1] = img_data[1] - 128;
            colors[2] = img_data[2] - 128; // This concludes the JPEG decompressor
                                           // Was fun doing it

            int16_t r = colors[0] + 1.402f * colors[2];
            int16_t g = colors[0] - 0.3441f * colors[1] - 0.71414f * colors[2];
            int16_t b = colors[0] + 1.772f * colors[1];

            img_data[0] = clamp0_255(r);
            img_data[1] = clamp0_255(g);
            img_data[2] = clamp0_255(b);
            img_data = img_data + jpeg->img.channels;
        }
    }
}
