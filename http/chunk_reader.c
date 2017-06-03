//
// Created by frank on 17-6-3.
//

#include "base.h"
#include "chunk_reader.h"

enum {

    hex_start_ = 0,
    hex_,
    hex_almost_done_,
    hex_done_,

    chunk_data_,
    chunk_data_almost_done_,

    all_almost_done_,
    all_done_,

    error_,
};

int chunk_reader_execute(chunk_reader *cr, char *beg, char *end)
{
    char *p = beg + cr->where;

    for (; p < end; ++p) {
        switch (cr->state) {
            case hex_start_:
                if (!isxdigit(*p)) {
                    goto error;
                }
                cr->first_hex = p;
                cr->state = hex_;
                break;

            case hex_:
                if (isxdigit(*p)) {
                    break;
                }
                if (*p == '\r') {
                    char *hex_end;
                    cr->expect_chunked_size = strtoul(cr->first_hex, &hex_end, 16);
                    assert(hex_end == p);
                    cr->state = hex_almost_done_;
                    break;
                }
                goto error;

            case hex_almost_done_:
                if (*p == '\n') {
                    cr->state = hex_done_;
                    break;
                }
                goto error;

            case hex_done_:
                if (cr->expect_chunked_size == 0) {
                    if (*p == '\r') {
                        cr->state = all_almost_done_;
                        break;
                    }
                    goto error;
                }
                cr->state = chunk_data_;
                /* fall through */

            case chunk_data_:
            {
                size_t readable = end - p;
                if (readable <= cr->expect_chunked_size) {
                    cr->expect_chunked_size -= readable;
                    p = end;
                    goto done;
                }

                p += cr->expect_chunked_size;
                cr->expect_chunked_size = 0;

                if (*p == '\r') {
                    cr->state = chunk_data_almost_done_;
                    break;
                }

                goto error;
            }

            case chunk_data_almost_done_:
                if (*p == '\n') {
                    cr->state = hex_start_;
                    break;
                }
                goto error;

            case all_almost_done_:
                if (*p == '\n') {
                    cr->state = all_done_;
                    ++p;
                    goto done;
                }
                goto error;

            default:
                goto error;
        }
    }


    done:
    cr->where = p - beg;
    return (cr->state == all_done_ ? FCY_OK : FCY_AGAIN);

    error:
    cr->state = error_;
    return FCY_ERROR;
}
