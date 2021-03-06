#include <stdio.h>
#include <string.h>
#include <quirc.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <png.h>

struct Image {
    uint8_t *buffer;
    int width;
    int height;
};

/*
 * Load png data using filename
 * */
struct Image load_png(const char *filename) {
    struct Image img;

    int width, height, interlace_type, number_passes = 1;
    png_uint_32 trns;
    png_byte color_type, bit_depth;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    FILE *infile = NULL;
    int pass;

    infile = fopen(filename, "rb");
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    setjmp(png_jmpbuf(png_ptr));
    png_init_io(png_ptr, infile);
    png_read_info(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    interlace_type = png_get_interlace_type(png_ptr, info_ptr);

    printf("colortype is %hhu \n", color_type);
    printf("bit_depth is %hhu \n", bit_depth);
    printf("interlace_type is %hhu \n", interlace_type);

    /*Read any color_type into 8bit depth, Grayscale format.
    See http://www.libpng.org/pub/png/libpng-manual.txt

    PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.*/
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if ((trns = png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)))
        png_set_tRNS_to_alpha(png_ptr);

    if (bit_depth == 16)
#if PNG_LIBPNG_VER >= 10504
        png_set_scale_16(png_ptr);
#else
        png_set_strip_16(png_ptr);
#endif

    if ((trns) || color_type & PNG_COLOR_MASK_ALPHA)
        png_set_strip_alpha(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE ||
        color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
        png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);
    }

    if (interlace_type != PNG_INTERLACE_NONE)
        number_passes = png_set_interlace_handling(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    int imageArea = width * height;
    uint8_t *buffer = malloc(sizeof(uint8_t) * imageArea);

    printf("number passes: %d\n", number_passes);

    for (pass = 0; pass < number_passes; pass++) {
        int y;

        for (y = 0; y < height; y++) {
            png_bytep row_pointer = buffer + y * width;
            png_read_rows(png_ptr, &row_pointer, NULL, 1);
        }
    }

    img.width = width;
    img.height = height;
    img.buffer = buffer;

    png_read_end(png_ptr, info_ptr);

    if (infile)
        fclose(infile);

    return img;
}

char *decode_qr(uint8_t *buffer, int width, int height) {
    /*
     * To decode images, you'll need to instantiate a ``struct quirc`object,
     * which is done with the ``quirc_new`` function.
     *
     * quirc_begin() must first be called to obtain access to a buffer into
     * which the input image should be placed. Optionally, the current width and height may be returned.
     */
    struct quirc *q;
    q = quirc_new();

    /*
     * Load png image file by filename,
     * convert to grayscale image,
     * feed grayscale image ke buffer using quirc_end and quirc_begin.
     * */
    printf("img width: %d\n", width);
    printf("img height: %d\n", height);
    printf("img buffer: %p\n", buffer);

    /*
     * Having obtained a decoder object,
     * you need to set the image size that you'll be working with,
     * which is done using ``quirc_resize``.
     */
    quirc_resize(q, width, height);

    /*
     * These functions are used to process images for QR-code recognition.
     * quirc_begin() must first be called to obtain access to a buffer into
     * which the input image should be placed. Optionally, the current
     * width and height may be returned.
     * */
    uint8_t *quircBuffer;
    quircBuffer = quirc_begin(q, &width, &height);

    uint8_t *p;
    p = buffer;

    unsigned int image_area = height * width;


    /*check value and copy elements*/
    for (int i = 0; i < image_area; ++i) {
        /*printf("Value of image[%d] = %d\n", i, *p);*/
        *quircBuffer = *p;
        p++;
        quircBuffer++;
    }

    /*
     * After filling the buffer, quirc_end() should be called to process
     * the image for QR-code recognition. The locations and content of each
     * code may be obtained using accessor functions described below.
     */
    quirc_end(q);

    /* This structure is used to return information about detected QR codes in the input image. */
    struct quirc_code code;

    /* This structure holds the decoded QR-code data */
    struct quirc_data data;

    /* Extract the QR-code specified by the given index. */
    quirc_extract(q, 0, &code);
    /* Decode a QR-code, returning the payload data. */
    quirc_decode(&code, &data);

    /* Copy data payload from quirc_data to dataPayloadBuffer */
    uint8_t *dataPayloadBuffer = malloc(sizeof(uint8_t) * QUIRC_MAX_PAYLOAD);
    uint8_t *dataPayloadBufferPtr = dataPayloadBuffer;

    uint8_t *dataPayloadPtr = data.payload;
    for (int j = 0; j < QUIRC_MAX_PAYLOAD; ++j) {
        *dataPayloadBufferPtr = *dataPayloadPtr;
        dataPayloadBufferPtr++;
        dataPayloadPtr++;
    }

    /*
     * Later, when you no longer need to decode anything,
     * you should release the allocated memory with ``quirc_destroy``
     * */
    quirc_destroy(q);

    /* Return data payload in char pointer form (string in c) */
    return (char *) dataPayloadBuffer;
}

void decoder(char **argv) {
    /*
     * Print input filename
     * */
    printf("Filename is %s \n", (const char *) argv[1]);

    /*
     * Load png and assign the returned object to Image struct
     * */
    struct Image img = load_png(argv[1]);

    /*
     * Print returned data payload from decode_qr function
     * */
    char *dataPayload;
    dataPayload = decode_qr(img.buffer, img.width, img.height);
    printf("Data payload is %s \n", dataPayload);
}

int main(int argc, char **argv) {
    decoder(argv);
}