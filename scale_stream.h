#ifndef SCALE_STREAM_H
#define SCALE_STREAM_H

#include <stddef.h>
#include <stdint.h>

#define SCALE_STREAM_DEFAULT_GRAY_LEVEL     180
#define SCALE_STREAM_MAX_ROWS               5

enum _scale_stream_row_state_t
{
    SCALE_STREAM_ROW_EMPTY = 0,
    SCALE_STREAM_ROW_FILLED,
    SCALE_STREAM_ROW_PROCESSED
};
typedef enum _scale_stream_row_state_t scale_stream_row_state_t;

struct _scale_stream_row_t
{
    size_t row;
    uint8_t *buf;
    scale_stream_row_state_t state;
};
typedef struct _scale_stream_row_t scale_stream_row_t;

struct _scale_stream_t
{
    size_t in_height;
    size_t in_width;
    size_t in_width_bytes;
    size_t out_width;
    size_t out_height;
    scale_stream_row_t in_rows[SCALE_STREAM_MAX_ROWS];
    float x_ratio;
    float y_ratio;

};
typedef struct _scale_stream_t scale_stream_t;

void scale_stream_init(scale_stream_t *ctx, size_t in_width, size_t in_height, size_t out_width, size_t out_height);
uint8_t scale_stream_row_ready(scale_stream_t *ctx, size_t row_idx);
int scale_stream_feed(scale_stream_t *ctx, size_t x, size_t y, uint8_t value);
int scale_stream_process_out_row(scale_stream_t *ctx, size_t row, uint8_t *out_row_buf);
void scale_stream_release(scale_stream_t *ctx);
size_t scale_stream_check_row(scale_stream_t *ctx, size_t out_row);

#endif