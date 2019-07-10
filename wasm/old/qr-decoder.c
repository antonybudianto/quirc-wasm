#include <stdio.h>
#include <string.h>
#include <quirc.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <png.h>

/*
 * Decode qr and print the qr payload data
 * */
static void decode_qr(struct quirc *q) {
    /*
     * This structure is used to return information about detected QR codes in the input image.
     * */
    struct quirc_code code;

    /* This structure holds the decoded QR-code data */
    struct quirc_data data;

    /* Extract the QR-code specified by the given index. */
    quirc_extract(q, 0, &code);
    /* Decode a QR-code, returning the payload data. */
    quirc_decode(&code, &data);

    /* Print qr code data payload */
    printf("%s \n", (const char *) &data.payload);
}

/*
 * Load png data using filename
 * */
int load_png(struct quirc *q, const char *filename) {
    int width, height, rowbytes, interlace_type, number_passes = 1;
    png_uint_32 trns;
    png_byte color_type, bit_depth;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    FILE *infile = NULL;
    uint8_t *image;
    int ret = -1;
    int pass;

    if ((infile = fopen(filename, "rb")) == NULL)
        goto out;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        goto out;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        goto out;

    if (setjmp(png_jmpbuf(png_ptr)))
        goto out;

    png_init_io(png_ptr, infile);

    png_read_info(png_ptr, info_ptr);

    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    interlace_type = png_get_interlace_type(png_ptr, info_ptr);

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
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (rowbytes != width) {
        fprintf(stderr,
                "load_png: expected rowbytes to be %u but got %u\n",
                width, rowbytes);
        goto out;
    }

    /*
     * Having obtained a decoder object,
     * you need to set the image size that you'll be working with,
     * which is done using ``quirc_resize``.
     */
    if (quirc_resize(q, width, height) < 0)
        goto out;

    /*
     * Using ``quirc_begin`` and ``quirc_end``,
     * you can feed a grayscale image directly into the buffer that ``quirc`` uses for image processing:
     * */
    image = quirc_begin(q, NULL, NULL);

    for (pass = 0; pass < number_passes; pass++) {
        int y;

        for (y = 0; y < height; y++) {
            png_bytep row_pointer = image + y * width;
            png_read_rows(png_ptr, &row_pointer, NULL, 1);
        }
    }

    png_read_end(png_ptr, info_ptr);

    ret = 0;
    /* FALLTHROUGH */
    out:
    /* cleanup */
    if (png_ptr) {
        if (info_ptr)
            png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
        else
            png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
    }
    if (infile)
        fclose(infile);
    return (ret);
}


int main(int argc, char **argv) {
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
     * Print input filename
     * */
    printf("%s \n", (const char *) argv[1]);

    /*
     * Load png image file by filename,
     * convert to grayscale image,
     * feed grayscale image ke buffer using quirc_end and quirc_begin.
     * */
    load_png(q, argv[1]);

    /*
     * After filling the buffer, quirc_end() should be called to process
     * the image for QR-code recognition. The locations and content of each
     * code may be obtained using accessor functions described below.
     */
    quirc_end(q);

    /*
     * extract data from quirc struct
     * */
    decode_qr(q);

    /*
     * Later, when you no longer need to decode anything,
     * you should release the allocated memory with ``quirc_destroy``
     * */
    quirc_destroy(q);
    return 0;
}