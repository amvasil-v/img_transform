#include "scale_stream.h"

#include <stdlib.h>
#include <stdio.h>


void scale_stream_init(scale_stream_t *ctx, size_t in_width, size_t in_height, size_t out_width, size_t out_height)
{
    //fill ctx
    ctx->in_width = in_width;
    ctx->in_height = in_height;
    ctx->out_width = out_width;
    ctx->out_height = out_height;
    ctx->x_ratio = ((float)(in_width - 1)) / out_width;
    ctx->y_ratio = ((float)(in_height - 1)) / out_height;

    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        ctx->in_rows[i].buf = NULL;
        ctx->in_rows[i].row = 0;
    }
    ctx->in_width_bytes = (ctx->in_width + 7) / 8;
}

uint8_t scale_stream_row_ready(scale_stream_t *ctx, size_t row_idx)
{
    size_t required_in_row = (size_t)(ctx->y_ratio * row_idx);
    uint8_t found = 0;

    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        if (ctx->in_rows[i].buf && ctx->in_rows[i].row == required_in_row) {
            found = 1;
        }
    }
    return found;
}

static int find_row_idx(scale_stream_t *ctx, size_t row)
{
    uint8_t found_empty = 0;
    int oldest_index = -1;
    size_t oldest_row = SIZE_MAX;
    int idx = -1;

    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        if (!ctx->in_rows[i].buf) {
            idx = i;
            found_empty = 1;
            break;
        }
        if (ctx->in_rows[i].row == row) {
            return i;
        }
        if (ctx->in_rows[i].row <= oldest_row) {
            oldest_index = i;
            oldest_row = ctx->in_rows[i].row;
        }
    }

    if (found_empty) {
        ctx->in_rows[idx].buf = (uint8_t *)malloc(ctx->in_width_bytes);
        if (!ctx->in_rows[idx].buf)
            return -1;
    } else {
        idx = oldest_index;
    }
    ctx->in_rows[idx].row = row;

    return idx;
}

#define IN_PIXEL_MASK(POS)      (1 << (POS % 8))
#define IN_PIXEL_IDX(POS)       (POS / 8)

static inline void put_pixel_row_in(scale_stream_t *ctx, uint8_t *buf, size_t x, uint8_t val)
{
    if (val)
        buf[IN_PIXEL_IDX(x)] |= IN_PIXEL_MASK(x);
    else
        buf[IN_PIXEL_IDX(x)] &= ~IN_PIXEL_MASK(x);
}

static inline uint8_t get_pixel_row_in(uint8_t *buf, size_t x)
{
    return (buf[IN_PIXEL_IDX(x)] & IN_PIXEL_MASK(x)) ? 0xFF : 0;
}

int scale_stream_feed(scale_stream_t *ctx, size_t x, size_t y, uint8_t value)
{
    int row = find_row_idx(ctx, y);

    if (row < 0)
    {
        return row;
    }
    if (x >= ctx->in_width || y >= ctx->in_height)
        return -1;
    put_pixel_row_in(ctx, ctx->in_rows[row].buf, x, value);
    return 0;
}

static inline void set_out_row_pixel(uint8_t *buf, size_t x, uint8_t val)
{
    if (val)
        buf[IN_PIXEL_IDX(x)] |= IN_PIXEL_MASK(x);
    else
        buf[IN_PIXEL_IDX(x)] &= ~IN_PIXEL_MASK(x);
}

static uint8_t* find_input_row(scale_stream_t *ctx, int row)
{
    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        if (ctx->in_rows[i].row == row && ctx->in_rows[i].buf) {
            return ctx->in_rows[i].buf;
        }
    }
    return NULL;
}

#define BILINEAR_SCALE_GRAY_LEVEL       180

int scale_stream_process_out_row(scale_stream_t *ctx, size_t row, uint8_t *out_row_buf)
{
    int A, B, C, D, x, y, gray;
    float x_diff, y_diff;
    uint8_t *in_row[2];

    y = (int)(ctx->y_ratio * row);
    y_diff = (ctx->y_ratio * row) - y;

    for (int i = 0; i < 2; i++) {
        in_row[i] = find_input_row(ctx, y + i);
        if (!in_row[i]) {
            printf("Input row %lu not found\n", row);
            return -1;
        }
    }

    for (int j = 0; j < ctx->out_width; j++) {
        x = (int)(ctx->x_ratio * j);           
        x_diff = (ctx->x_ratio * j) - x;            

        A = get_pixel_row_in(in_row[0], x);
        B = get_pixel_row_in(in_row[0], x + 1);
        C = get_pixel_row_in(in_row[1], x);
        D = get_pixel_row_in(in_row[1], x + 1);


        // Y = A(1-w)(1-h) + B(w)(1-h) + C(h)(1-w) + Dwh
        gray = (int)(A * (1 - x_diff) * (1 - y_diff) + B * (x_diff) * (1 - y_diff) +
                     C * (y_diff) * (1 - x_diff) + D * (x_diff * y_diff));

        set_out_row_pixel(out_row_buf, j, (gray >= BILINEAR_SCALE_GRAY_LEVEL));
    }

    return 0;
}

void scale_stream_release(scale_stream_t *ctx)
{
    for (int i = 0; i < SCALE_STREAM_MAX_ROWS; i++) {
        if (ctx->in_rows[i].buf) {
            free(ctx->in_rows[i].buf);
        }
    }
}

size_t scale_stream_check_row(scale_stream_t *ctx, size_t out_row)
{
    return (int)(ctx->y_ratio * out_row) + 1;
}
