// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <printf.h>
#include <xefi.h>

#define PCBUFMAX 126
// buffer is two larger to leave room for a \0 and room
// for a \r that may be added after a \n

#ifdef OUTPUT_SERIAL
#define PRINTTYPE char
#else
#define PRINTTYPE char16_t
#endif


typedef struct {
    size_t off;
    PRINTTYPE buf[PCBUFMAX + 2];
} _pcstate;

static int _printf_console_out(const char* str, size_t len, void* _state) {
    _pcstate* state = _state;
    PRINTTYPE* buf = state->buf;
    size_t i = state->off;
    int n = len;
    while (n > 0) {
        if (*str == '\n') {
            buf[i++] = '\r';
        }
        buf[i++] = *str++;
        if (i >= PCBUFMAX) {
#ifdef OUTPUT_SERIAL
            gSerialOut->Write(gSerialOut, &i, buf);
#else
            buf[i] = 0;
            gConOut->OutputString(gConOut, buf);
#endif
            i = 0;
        }
        n--;
    }
    state->off = i;
    return len;
}

int printf(const char* fmt, ...) {
    va_list ap;
    _pcstate state;
    int r;
    state.off = 0;
    va_start(ap, fmt);
    r = _printf_engine(_printf_console_out, &state, fmt, ap);
    va_end(ap);
    if (state.off) {
#ifdef OUTPUT_SERIAL
        gSerialOut->Write(gSerialOut, &state.off, state.buf);
#else
        state.buf[state.off] = 0;
        gConOut->OutputString(gConOut, state.buf);
#endif
    }
    return r;
}

int puts16(char16_t* str) {
#ifdef OUTPUT_SERIAL
    _pcstate state;
    state.off = 0;
    // convert wide char to regular chars
    for (; *str; str++) {
        state.buf[state.off++] = *str;
    }
    int r = 0;
    if (state.off) {
        efi_status s = gSerialOut->Write(gSerialOut, &state.off, state.buf);
        r = (s == EFI_SUCCESS) ? 0 : -1;
    }
    return r;
#else
    return gConOut->OutputString(gConOut, str) == EFI_SUCCESS ? 0 : -1;
#endif
}
