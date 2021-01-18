#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pngle.h"
#include "png.h"

static int save_to_png(uint8_t *buf, size_t _width, size_t _height);

int main(void)
{
    pngle_t *pngle = pngle_new();

    if (!pngle)
    {
        printf("Failed to init pngle\n");
        return -1;
    }

    static const size_t img_height = 100;
    static const size_t img_width = 200;
    uint8_t *img_buf = (uint8_t *)malloc(img_height * img_width);
    memset(img_buf, 0, sizeof(img_buf));
    for (int i = 0; i < img_width * img_height; i++) {
        img_buf[i] = i % 2;
    }
    if(save_to_png(img_buf, img_width, img_height)) {
        printf("Failed to save to png\n");
    }

    free(img_buf);

    pngle_destroy(pngle);

    printf("Success\n");

    return 0;
}


static int save_to_png(uint8_t *buf, size_t _width, size_t _height)
{
    FILE *fp2 = fopen("out.png", "wb");
    if (!fp2)
    {
        // dealing with error
    }

    // 1. Create png struct pointer
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        // dealing with error
        return -1;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        // dealing with error
        return -1;
    }

    // 2. Set png info like width, height, bit depth and color type
    //    in this example, I assumed grayscale image. You can change image type easily
    int width = _width;
    int height = _height;
    int bit_depth = 1;
    png_init_io(png_ptr, fp2);
    png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth,
                 PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    // 3. Convert 1d array to 2d array to be suitable for png struct
    //    I assumed the original array is 1d
    png_bytepp row_pointers = (png_bytepp)png_malloc(png_ptr, sizeof(png_bytepp) * height);
    for (int i = 0; i < height; i++)
    {
        row_pointers[i] = (png_bytep)png_malloc(png_ptr, width);
    }
    for (int hi = 0; hi < height; hi++)
    {
        for (int wi = 0; wi < width; wi++)
        {
            // bmp_source is source data that we convert to png
            row_pointers[hi][wi] = buf[wi + width * hi] ? UINT8_MAX : 0;
        }
    }

    // 4. Write png file
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return 0;
}