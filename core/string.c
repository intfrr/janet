/*
* Copyright (c) 2017 Calvin Rose
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#include <gst/gst.h>

/* Temporary buffer size */
#define GST_BUFSIZE 36

static const uint8_t *real_to_string(Gst *vm, GstReal x) {
    uint8_t buf[GST_BUFSIZE];
    int count = snprintf((char *) buf, GST_BUFSIZE, "%.21gF", x);
    return gst_string_b(vm, buf, (uint32_t) count);
}

static const uint8_t *integer_to_string(Gst *vm, GstInteger x) {
    uint8_t buf[GST_BUFSIZE];
    int neg = 0;
    uint8_t *hi, *low;
    uint32_t count = 0;
    if (x == 0) return gst_string_c(vm, "0");
    if (x < 0) {
        neg = 1;
        x = -x;
    }
    while (x > 0) {
        uint8_t digit = x % 10;
        buf[count++] = '0' + digit;
        x /= 10;
    }
    if (neg)
        buf[count++] = '-';
    /* Reverse */
    hi = buf + count - 1;
    low = buf;
    while (hi > low) {
        uint8_t temp = *low;
        *low++ = *hi;
        *hi-- = temp;
    }
    return gst_string_b(vm, buf, (uint32_t) count);
}

static const char *HEX_CHARACTERS = "0123456789abcdef";
#define HEX(i) (((uint8_t *) HEX_CHARACTERS)[(i)])

/* Returns a string description for a pointer. Max titlelen is GST_BUFSIZE
 * - 5 - 2 * sizeof(void *). */
static const uint8_t *string_description(Gst *vm, const char *title, void *pointer) {
    uint8_t buf[GST_BUFSIZE];
    uint8_t *c = buf;
    uint32_t i;
    union {
        uint8_t bytes[sizeof(void *)];
        void *p;
    } pbuf;

    pbuf.p = pointer;
    *c++ = '<';
    for (i = 0; title[i]; ++i)
        *c++ = ((uint8_t *)title) [i];
    *c++ = ' ';
    *c++ = '0';
    *c++ = 'x';
    for (i = sizeof(void *); i > 0; --i) {
        uint8_t byte = pbuf.bytes[i - 1];
        if (!byte) continue;
        *c++ = HEX(byte >> 4);
        *c++ = HEX(byte & 0xF);
    }
    *c++ = '>';
    return gst_string_b(vm, buf, c - buf);
}

/* Gets the string value of userdata. Allocates memory, so is slower than
 * string_description. */
static const uint8_t *string_udata(Gst *vm, const char *title, void *pointer) {
    uint32_t strlen = 0;
    uint8_t *c, *buf;
    uint32_t i;
    union {
        uint8_t bytes[sizeof(void *)];
        void *p;
    } pbuf;

    while (title[strlen]) ++strlen;
    c = buf = gst_alloc(vm, strlen + 5 + 2 * sizeof(void *));
    pbuf.p = pointer;
    *c++ = '<';
    for (i = 0; title[i]; ++i)
        *c++ = ((uint8_t *)title) [i];
    *c++ = ' ';
    *c++ = '0';
    *c++ = 'x';
    for (i = sizeof(void *); i > 0; --i) {
        uint8_t byte = pbuf.bytes[i - 1];
        if (!byte) continue;
        *c++ = HEX(byte >> 4);
        *c++ = HEX(byte & 0xF);
    }
    *c++ = '>';
    return gst_string_b(vm, buf, c - buf);
}

#undef GST_BUFSIZE

void gst_escape_string(Gst *vm, GstBuffer *b, const uint8_t *str) {
    uint32_t i;
    gst_buffer_push(vm, b, '"');
    for (i = 0; i < gst_string_length(str); ++i) {
        uint8_t c = str[i];
        switch (c) {
            case '"':
                gst_buffer_push(vm, b, '\\');
                gst_buffer_push(vm, b, '"');
                break;
            case '\n':
                gst_buffer_push(vm, b, '\\');
                gst_buffer_push(vm, b, 'n');
                break;
            case '\r':
                gst_buffer_push(vm, b, '\\');
                gst_buffer_push(vm, b, 'r');
                break;
            case '\0':
                gst_buffer_push(vm, b, '\\');
                gst_buffer_push(vm, b, '0');
                break;
            default:
                gst_buffer_push(vm, b, c);
                break;
        }
    }
    gst_buffer_push(vm, b, '"');
}

/* Returns a string pointer with the description of the string */
const uint8_t *gst_short_description(Gst *vm, GstValue x) {
    switch (x.type) {
    case GST_NIL:
        return gst_string_c(vm, "nil");
    case GST_BOOLEAN:
        if (x.data.boolean)
            return gst_string_c(vm, "true");
        else
            return gst_string_c(vm, "false");
    case GST_REAL:
        return real_to_string(vm, x.data.real);
    case GST_INTEGER:
        return integer_to_string(vm, x.data.integer);
    case GST_ARRAY:
        return string_description(vm, "array", x.data.pointer);
    case GST_TUPLE:
        return string_description(vm, "tuple", x.data.pointer);
    case GST_STRUCT:
        return string_description(vm, "struct", x.data.pointer);
    case GST_TABLE:
        return string_description(vm, "table", x.data.pointer);
    case GST_SYMBOL:
        return x.data.string;
    case GST_STRING:
    {
        GstBuffer *buf = gst_buffer(vm, gst_string_length(x.data.string) + 4);
        gst_escape_string(vm, buf, x.data.string);
        return gst_buffer_to_string(vm, buf);
    }
    case GST_BYTEBUFFER:
        return string_description(vm, "buffer", x.data.pointer);
    case GST_CFUNCTION:
        return string_description(vm, "cfunction", x.data.pointer);
    case GST_FUNCTION:
        return string_description(vm, "function", x.data.pointer);
    case GST_THREAD:
        return string_description(vm, "thread", x.data.pointer);
    case GST_USERDATA:
        return string_udata(vm, gst_udata_type(x.data.pointer)->name, x.data.pointer);
    case GST_FUNCENV:
        return string_description(vm, "funcenv", x.data.pointer);
    case GST_FUNCDEF:
        return string_description(vm, "funcdef", x.data.pointer);
    }
}

/* Static debug print helper */
static GstInteger gst_description_helper(Gst *vm, GstBuffer *b, GstTable *seen, GstValue x, GstInteger next, int depth) {
    GstValue check = gst_table_get(seen, x);
    const uint8_t *str;
    /* Prevent a stack overflow */
    if (depth++ > GST_RECURSION_GUARD)
        return -1;
    if (check.type == GST_INTEGER) {
        str = integer_to_string(vm, check.data.integer);
        gst_buffer_append_cstring(vm, b, "<visited ");
        gst_buffer_append(vm, b, str, gst_string_length(str));
        gst_buffer_append_cstring(vm, b, ">");
    } else {
        uint8_t open, close;
        uint32_t len, i;
        const GstValue *data;
        switch (x.type) {
            default:
                str = gst_short_description(vm, x);
                gst_buffer_append(vm, b, str, gst_string_length(str));
                return next;
            case GST_STRING:
                gst_escape_string(vm, b, x.data.string);
                return next;
            case GST_SYMBOL:
                gst_buffer_append(vm, b, x.data.string, gst_string_length(x.data.string));
                return next;
            case GST_NIL:
                gst_buffer_append_cstring(vm, b, "nil");
                return next;
            case GST_BOOLEAN:
                gst_buffer_append_cstring(vm, b, x.data.boolean ? "true" : "false");
                return next;
            case GST_STRUCT:
                open = '<'; close = '>';
                break;
            case GST_TABLE:
                open = '{'; close = '}';
                break;
            case GST_TUPLE:
                open = '('; close = ')';
                break;
            case GST_ARRAY:
                open = '['; close = ']';
                break;
        }
        gst_table_put(vm, seen, x, gst_wrap_integer(next++));
        gst_buffer_push(vm, b, open);
        if (gst_hashtable_view(x, &data, &len)) {
            int isfirst = 1;
            for (i = 0; i < len; i += 2) {
                if (data[i].type != GST_NIL) {
                    if (isfirst)
                        isfirst = 0;
                    else
                        gst_buffer_push(vm, b, ' ');
                    next = gst_description_helper(vm, b, seen, data[i], next, depth);
                    if (next == -1)
                        gst_buffer_append_cstring(vm, b, "...");
                    gst_buffer_push(vm, b, ' ');
                    next = gst_description_helper(vm, b, seen, data[i + 1], next, depth);
                    if (next == -1)
                        gst_buffer_append_cstring(vm, b, "...");
                }
            }
        } else if (gst_seq_view(x, &data, &len)) {
            for (i = 0; i < len; ++i) {
                next = gst_description_helper(vm, b, seen, data[i], next, depth);
                if (next == -1)
                    return -1;
                if (i != len - 1)
                    gst_buffer_push(vm, b, ' ');
            }
        }
        gst_buffer_push(vm, b, close);
    }
    return next;
}

/* Debug print. Returns a description of an object as a buffer. */
const uint8_t *gst_description(Gst *vm, GstValue x) {
    GstBuffer *buf = gst_buffer(vm, 10);
    gst_description_helper(vm, buf, gst_table(vm, 10), x, 0, 0);
    return gst_buffer_to_string(vm, buf);
}

const uint8_t *gst_to_string(Gst *vm, GstValue x) {
    if (x.type == GST_STRING || x.type == GST_SYMBOL) {
        return x.data.string;
    } else if (x.type == GST_BYTEBUFFER) {
        return gst_buffer_to_string(vm, x.data.buffer);
    } else {
        return gst_description(vm, x);
    }
}