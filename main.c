#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pngle.h"
#include "png.h"
#include "scale_stream.h"

#define DISPLAY_WIDTH              640
#define DISPLAY_HEIGHT             385

struct _picture_t {
    size_t width;
    size_t height;
    uint8_t *data;
};
typedef struct _picture_t picture_t;

static int save_to_png(uint8_t *buf, size_t width, size_t height);
static picture_t *picture_alloc(uint32_t w, uint32_t h);
static void picture_init(pngle_t *pngle, uint32_t w, uint32_t h);
static void picture_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]);
static void picture_done(pngle_t *pngle);
static int picture_set_pixel(picture_t *pic, uint32_t x, uint32_t y, uint8_t val);
static uint8_t picture_get_pixel(picture_t *pic, uint32_t x, uint32_t y);
static picture_t *picture_scale(picture_t *in, size_t out_w, size_t out_h);
static picture_t *scale_picture_for_display(picture_t *pic, scale_type_t type);

static int process_png(const char *filename);


int main(void)
{
    const char *filename = "in.png";
    printf("Process %s\n", filename);
    process_png(filename);

    return 0;
}

static uint8_t tmp_buf[(DISPLAY_WIDTH + 7) / 8 * SCALE_STREAM_MAX_ROWS];
static uint8_t *out_buf = NULL;
static size_t out_width_bytes;

static picture_t *picture_alloc(uint32_t w, uint32_t h)
{
    size_t data_size = (w * h + 7) / 8;
    picture_t *pic = malloc(sizeof(picture_t) + data_size);
    pic->data = (uint8_t *)pic + sizeof(picture_t);
    memset(pic->data, 0, data_size);
    pic->width = w;
    pic->height = h;
}

scale_stream_t scale_ctx;
static int draw_error = 0;
static int draw_curr_row = 0;

static void picture_init(pngle_t *pngle, uint32_t w, uint32_t h)
{
    printf("Create %u by %u picture\n", w, h);
    draw_error = 0;
    out_width_bytes = (DISPLAY_WIDTH + 7) / 8;
    out_buf = (uint8_t *)malloc(out_width_bytes * DISPLAY_HEIGHT);
    memset(out_buf, 0xFF, out_width_bytes * DISPLAY_HEIGHT);
    scale_stream_init(&scale_ctx, w, h);
    scale_stream_scale_init(&scale_ctx, DISPLAY_WIDTH, DISPLAY_HEIGHT, SCALE_TYPE_PRESERVE);
    if (scale_stream_buffer_init(&scale_ctx, tmp_buf, sizeof(tmp_buf))) {
        printf("Failed to init temp buffer\n");
        draw_error = -2;
    }   
    draw_curr_row = 0;
}

#define PIC_PIXEL_MASK(POS)      (1 << (POS % 8))
#define PIC_PIXEL_IDX(POS)       (POS / 8)

static int picture_set_pixel(picture_t *pic, uint32_t x, uint32_t y, uint8_t val)
{
    if (x >= pic->width || y >= pic->height)
        return -1;
    uint32_t pos = y * pic->width + x;
    if (val)
        pic->data[PIC_PIXEL_IDX(pos)] |= PIC_PIXEL_MASK(pos);
    else
        pic->data[PIC_PIXEL_IDX(pos)] &= ~PIC_PIXEL_MASK(pos);
    return 0;
}

static uint8_t picture_get_pixel(picture_t *pic, uint32_t x, uint32_t y)
{
    if (x >= pic->width || y >= pic->height)
        return 0;
    uint32_t pos = y * pic->width + x;
    return (pic->data[PIC_PIXEL_IDX(pos)] & PIC_PIXEL_MASK(pos)) ? UINT8_MAX : 0;
}

static uint8_t picture_buf_get_pixel(uint8_t *buf, size_t x, size_t y, size_t w)
{
    uint32_t pos = y * w + x;
    return (buf[PIC_PIXEL_IDX(pos)] & PIC_PIXEL_MASK(pos)) ? UINT8_MAX : 0;
}

static void picture_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
    static const uint32_t black_level = 350;
    uint32_t val = 0;
    //printf("draw %u %u\n", x, y);
    for (int i = 0; i < 3; i++)
        val += rgba[i];
    if (draw_error)
        return;
    if (scale_stream_feed(&scale_ctx, x, y, (val > black_level))) {
        printf("Feed error at %u %u\n", x, y);
        draw_error = -1;
        return;
    }

    if (x == scale_ctx.in_width - 1) {
        while (1) {
            size_t check_row = scale_stream_check_row(&scale_ctx, draw_curr_row);
            if (check_row > y) {
                break;
            }
            if (scale_stream_process_out_row(&scale_ctx, draw_curr_row, 
             &out_buf[(draw_curr_row + scale_ctx.offset_y) * out_width_bytes])) {
                printf("Draw error at %u %u\n", x, y);
                draw_error = -1;
                return;
            }
            draw_curr_row++;
        }
    }
}

static void picture_done(pngle_t *pngle)
{
    
    if (draw_error) {
        printf("PNG scale transform failed\n");
    } else {
        printf("Done %lu x %lu image\n", scale_ctx.out_width, scale_ctx.out_height);
        save_to_png(out_buf, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        printf("Picture %dx%d saved to file\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }
    free(out_buf);
}

static int process_png(const char *filename)
{
    pngle_t *pngle = pngle_new();

    pngle_set_draw_callback(pngle, picture_draw);
    pngle_set_init_callback(pngle, picture_init);
    pngle_set_done_callback(pngle, picture_done);

    int fd = open(filename, O_RDONLY);

    if (fd <= 0) {
        printf("Failed to open %s\n", filename);
        pngle_destroy(pngle);
        return -1;
    }

    // Feed data to pngle
    char buf[1024];
    int remain = 0;
    int len;
    while ((len = read(fd, buf + remain, sizeof(buf) - remain)) > 0) {
        int fed = pngle_feed(pngle, buf, remain + len);
        if (fed < 0)
            printf("Error: %s", pngle_error(pngle));

        remain = remain + len - fed;
        if (remain > 0) memmove(buf, buf + fed, remain);
    }

    close(fd);
    pngle_destroy(pngle);
}

#define OUT_PIXEL_MASK(POS)      (1 << (POS % 8))
#define OUT_PIXEL_IDX(POS)       (POS / 8)

static uint8_t get_output_pixel(uint8_t *buf, size_t x, size_t y)
{
    return (buf[y * out_width_bytes + OUT_PIXEL_IDX(x)] & OUT_PIXEL_MASK(x)) ? 0xFF : 0;
}

static int save_to_png(uint8_t *buf, size_t width, size_t height)
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
    int bit_depth = 8;

    printf("Write %lu by %lu picture\n", width, height);

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
            row_pointers[hi][wi] = get_output_pixel(buf, wi, hi);
        }
    }

    // 4. Write png file
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return 0;
}

