#ifndef PAQ8PX_ENCODER_H
#define PAQ8PX_ENCODER_H

// An Encoder does arithmetic encoding.  Methods:
// Encoder(COMPRESS, f) creates encoder for compression to archive f, which
//   must be open past any header for writing in binary mode.
// Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
//   which must be open past any header for reading in binary mode.
// code(i) in COMPRESS mode compresses bit i (0 or 1) to file f.
// code() in DECOMPRESS mode returns the next decompressed bit from file f.
//   Global y is set to the last bit coded or decoded by code().
// compress(c) in COMPRESS mode compresses one byte.
// decompress() in DECOMPRESS mode decompresses and returns one byte.
// flush() should be called exactly once after compression is done and
//   before closing f.  It does nothing in DECOMPRESS mode.
// size() returns current length of archive
// setFile(f) sets alternate source to FILE* f for decompress() in COMPRESS
//   mode (for testing transforms).
// If level (global) is 0, then data is stored without arithmetic coding.

#include <cstdio>
#include <cassert>
#include "Predictor.h"

typedef unsigned int U32;

typedef enum {
    COMPRESS, DECOMPRESS
} Mode;

class Encoder {
private:
    Predictor predictor;
    const Mode mode;       // Compress or decompress?
    FILE *archive;         // Compressed data file
    U32 x1, x2;            // Range, initially [0, 1), scaled by 2^32
    U32 x;                 // Decompress mode: last 4 input bytes of archive
    FILE *alt;             // decompress() source in COMPRESS mode
    float p1, p2;
    int level;

    // Compress bit y or return decompressed bit
    int code(int i = 0) {
        int p = predictor.p();
        assert(p >= 0 && p < 4096);
        p += p < 2048;
        U32 xmid = x1 + ((x2 - x1) >> 12) * p + (((x2 - x1) & 0xfff) * p >> 12);
        assert(xmid >= x1 && xmid < x2);
        if(mode == DECOMPRESS) {
            y = x <= xmid;
        } else {
            y = i;
        }
        y ? (x2 = xmid) : (x1 = xmid + 1);
        predictor.update();
        while(((x1 ^ x2) & 0xff000000) == 0) {  // pass equal leading bytes of range
            if(mode == COMPRESS) { putc(x2 >> 24, archive); }
            x1 <<= 8;
            x2 = (x2 << 8) + 255;
            if(mode == DECOMPRESS) { x = (x << 8) + (getc(archive) & 255); }  // EOF is OK
        }
        return y;
    }

public:
    Encoder(Mode m, FILE *f, int level);

    Mode getMode() const { return mode; }

    long size() const {
        return ftell(archive);
    }  // length of archive so far
    void flush();  // call this when compression is finished
    void setFile(FILE *f) {
        alt = f;
    }

    // Compress one byte
    void compress(int c) {
        assert(mode == COMPRESS);
        if(level == 0) {
            putc(c, archive);
        } else {
            for(int i = 7; i >= 0; --i) {
                code((c >> i) & 1);
            }
        }
    }

    // Decompress and return one byte
    int decompress() {
        if(mode == COMPRESS) {
            assert(alt);
            return getc(alt);
        } else if(level == 0) {
            return getc(archive);
        } else {
            int c = 0;
            for(int i = 0; i < 8; ++i) {
                c += c + code();
            }
            return c;
        }
    }

    void set_status_range(float perc1, float perc2) {
        p1 = perc1;
        p2 = perc2;
    }

    void print_status(int n, int size) {
        printf("%6.2f%%\b\b\b\b\b\b\b", (p1 + (p2 - p1) * n / (size + 1)) * 100), fflush(stdout);
    }

    void print_status() {
        printf("%6.2f%%\b\b\b\b\b\b\b", float(size()) / (p2 + 1) * 100), fflush(stdout);
    }
};

Encoder::Encoder(Mode m, FILE *f, int l) :
        mode(m), archive(f), x1(0), x2(0xffffffff), x(0), alt(0), level(l) {
    if(mode == DECOMPRESS) {
        long start = size();
        fseek(f, 0, SEEK_END);
        set_status_range(0, size());
        fseek(archive, start, SEEK_SET);
    }
    if(level > 0 && mode == DECOMPRESS) {  // x = first 4 bytes of archive
        for(int i = 0; i < 4; ++i) {
            x = (x << 8) + (getc(archive) & 255);
        }
    }
    for(int i = 0; i < 1024; ++i) {
        dt[i] = 16384 / (i + i + 3);
    }

}

void Encoder::flush() {
    if(mode == COMPRESS && level > 0) {
        putc(x1 >> 24, archive);
    }  // Flush first unequal byte of range
}


#endif //PAQ8PX_ENCODER_H
