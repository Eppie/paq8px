#ifndef PAQ8PX_FILTERS_H
#define PAQ8PX_FILTERS_H

//
// Before compression, data is encoded in blocks with the following format:
//
//   <type> <size> <encoded-data>
//
// Type is 1 byte (type Filetype): DEFAULT=0, JPEG, EXE
// Size is 4 bytes in big-endian format.
// Encoded-data decodes to <size> bytes.  The encoded size might be
// different.  Encoded data is designed to be more compressible.
//
//   void encode(FILE* in, FILE* out, int n);
//
// Reads n bytes of in (open in "rb" mode) and encodes one or
// more blocks to temporary file out (open in "wb+" mode).
// The file pointer of in is advanced n bytes.  The file pointer of
// out is positioned after the last byte written.
//
//   en.setFile(FILE* out);
//   int decode(Encoder& en);
//
// Decodes and returns one byte.  Input is from en.decompress(), which
// reads from out if in COMPRESS mode.  During compression, n calls
// to decode() must exactly match n bytes of in, or else it is compressed
// as type 0 without encoding.
//
//   Filetype detect(FILE* in, int n, Filetype type);
//
// Reads n bytes of in, and detects when the type changes to
// something else.  If it does, then the file pointer is repositioned
// to the start of the change and the new type is returned.  If the type
// does not change, then it repositions the file pointer n bytes ahead
// and returns the old type.
//
// For each type X there are the following 2 functions:
//
//   void encode_X(FILE* in, FILE* out, int n, ...);
//
// encodes n bytes from in to out.
//
//   int decode_X(Encoder& en);
//
// decodes one byte from en and returns it.  decode() and decode_X()
// maintain state information using static variables.
#include <zlib.h>

#define bswap(x) \
+   ((((x) & 0xff000000) >> 24) | \
+    (((x) & 0x00ff0000) >>  8) | \
+    (((x) & 0x0000ff00) <<  8) | \
+    (((x) & 0x000000ff) << 24))

#define IMG_DET(type, start_pos, header_len, width, height) return dett=(type),\
deth=(header_len),detd=(width)*(height),info=(width),\
fseek(in, start+(start_pos), SEEK_SET),HDR

#define AUD_DET(type, start_pos, header_len, data_len, wmode) return dett=(type),\
deth=(header_len),detd=(data_len),info=(wmode),\
fseek(in, start+(start_pos), SEEK_SET),HDR


// Function ecc_compute(), edc_compute() and eccedc_init() taken from
// ** UNECM - Decoder for ECM (Error Code Modeler) format.
// ** Version 1.0
// ** Copyright (C) 2002 Neill Corlett

/* LUTs used for computing ECC/EDC */
static U8 ecc_f_lut[256];
static U8 ecc_b_lut[256];
static U32 edc_lut[256];
static int luts_init = 0;

void eccedc_init(void) {
    if(luts_init) { return; }
    U32 i, j, edc;
    for(i = 0; i < 256; i++) {
        j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
        ecc_f_lut[i] = j;
        ecc_b_lut[i ^ j] = i;
        edc = i;
        for(j = 0; j < 8; j++) { edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0); }
        edc_lut[i] = edc;
    }
    luts_init = 1;
}

void ecc_compute(U8 *src, U32 major_count, U32 minor_count, U32 major_mult, U32 minor_inc, U8 *dest) {
    U32 size = major_count * minor_count;
    U32 major, minor;
    for(major = 0; major < major_count; major++) {
        U32 index = (major >> 1) * major_mult + (major & 1);
        U8 ecc_a = 0;
        U8 ecc_b = 0;
        for(minor = 0; minor < minor_count; minor++) {
            U8 temp = src[index];
            index += minor_inc;
            if(index >= size) { index -= size; }
            ecc_a ^= temp;
            ecc_b ^= temp;
            ecc_a = ecc_f_lut[ecc_a];
        }
        ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
        dest[major] = ecc_a;
        dest[major + major_count] = ecc_a ^ ecc_b;
    }
}

U32 edc_compute(const U8 *src, int size) {
    U32 edc = 0;
    while(size--) { edc = (edc >> 8) ^ edc_lut[(edc ^ (*src++)) & 0xFF]; }
    return edc;
}

int expand_cd_sector(U8 *data, int a, int test) {
    U8 d2[2352];
    eccedc_init();
    d2[0] = d2[11] = 0;
    for(int i = 1; i < 11; i++) { d2[i] = 255; }
    int mode = (data[15] != 1 ? 2 : 1);
    int form = (data[15] == 3 ? 2 : 1);
    if(a == -1) { for(int i = 12; i < 15; i++) { d2[i] = data[i]; }}
    else {
        int c1 = (a & 15) + ((a >> 4) & 15) * 10;
        int c2 = ((a >> 8) & 15) + ((a >> 12) & 15) * 10;
        int c3 = ((a >> 16) & 15) + ((a >> 20) & 15) * 10;
        c1 = (c1 + 1) % 75;
        if(c1 == 0) {
            c2 = (c2 + 1) % 60;
            if(c2 == 0) { c3++; }
        }
        d2[12] = (c3 % 10) + 16 * (c3 / 10);
        d2[13] = (c2 % 10) + 16 * (c2 / 10);
        d2[14] = (c1 % 10) + 16 * (c1 / 10);
    }
    d2[15] = mode;
    if(mode == 2) { for(int i = 16; i < 24; i++) { d2[i] = data[i - 4 * (i >= 20)]; }}
    if(form == 1) {
        if(mode == 2) {
            d2[1] = d2[12], d2[2] = d2[13], d2[3] = d2[14];
            d2[12] = d2[13] = d2[14] = d2[15] = 0;
        } else {
            for(int i = 2068; i < 2076; i++) { d2[i] = 0; }
        }
        for(int i = 16 + 8 * (mode == 2); i < 2064 + 8 * (mode == 2); i++) { d2[i] = data[i]; }
        U32 edc = edc_compute(d2 + 16 * (mode == 2), 2064 - 8 * (mode == 2));
        for(int i = 0; i < 4; i++) { d2[2064 + 8 * (mode == 2) + i] = (edc >> (8 * i)) & 0xff; }
        ecc_compute(d2 + 12, 86, 24, 2, 86, d2 + 2076);
        ecc_compute(d2 + 12, 52, 43, 86, 88, d2 + 2248);
        if(mode == 2) {
            d2[12] = d2[1], d2[13] = d2[2], d2[14] = d2[3], d2[15] = 2;
            d2[1] = d2[2] = d2[3] = 255;
        }
    }
    for(int i = 0; i < 2352; i++) { if(d2[i] != data[i] && test) { form = 2; }}
    if(form == 2) {
        for(int i = 24; i < 2348; i++) { d2[i] = data[i]; }
        U32 edc = edc_compute(d2 + 16, 2332);
        for(int i = 0; i < 4; i++) { d2[2348 + i] = (edc >> (8 * i)) & 0xff; }
    }
    for(int i = 0; i < 2352; i++) { if(d2[i] != data[i] && test) { return 0; } else { data[i] = d2[i]; }}
    return mode + form - 1;
}

int parse_zlib_header(int header) {
    switch(header) {
        case 0x2815 :
            return 0;
        case 0x2853 :
            return 1;
        case 0x2891 :
            return 2;
        case 0x28cf :
            return 3;
        case 0x3811 :
            return 4;
        case 0x384f :
            return 5;
        case 0x388d :
            return 6;
        case 0x38cb :
            return 7;
        case 0x480d :
            return 8;
        case 0x484b :
            return 9;
        case 0x4889 :
            return 10;
        case 0x48c7 :
            return 11;
        case 0x5809 :
            return 12;
        case 0x5847 :
            return 13;
        case 0x5885 :
            return 14;
        case 0x58c3 :
            return 15;
        case 0x6805 :
            return 16;
        case 0x6843 :
            return 17;
        case 0x6881 :
            return 18;
        case 0x68de :
            return 19;
        case 0x7801 :
            return 20;
        case 0x785e :
            return 21;
        case 0x789c :
            return 22;
        case 0x78da :
            return 23;
    }
    return -1;
}

int zlib_inflateInit(z_streamp strm, int zh) {
    if(zh == -1) { return inflateInit2(strm, -MAX_WBITS); } else { return inflateInit(strm); }
}

bool IsGrayscalePalette(FILE *in, int n = 256, int isRGBA = 0) {
    long offset = ftell(in);
    int stride = 3 + isRGBA, res = (n > 0) << 8, order = 1;
    for(int i = 0; (i < n * stride) && (res >> 8); i++) {
        int b = getc(in);
        if(b == EOF) {
            res = 0;
            break;
        }
        if(!i) {
            res = 0x100 | b;
            order = 1 - 2 * (b > 0);
            continue;
        }

        //"j" is the index of the current byte in this color entry
        int j = i % stride;
        if(!j) {
            res = (res & ((b - (res & 0xFF) == order) << 8)) | b; // load first component of this entry
        } else if(j == 3) {
            res &= ((!b || (b == 0xFF)) * 0x1FF); // alpha/attribute component must be zero or 0xFF
        } else {
            res &= ((b == (res & 0xFF)) << 9) - 1;
        }
    }
    fseek(in, offset, SEEK_SET);
    return res >> 8;
}

// Detect blocks
Filetype detect(FILE *in, int n, Filetype type, int &info) {
    U32 buf3 = 0, buf2 = 0, buf1 = 0, buf0 = 0;  // last 16 bytes
    long start = ftell(in);

    // For EXE detection
    Array<int> abspos(256),  // CALL/JMP abs. addr. low byte -> last offset
            relpos(256);    // CALL/JMP relative addr. low byte -> last offset
    int e8e9count = 0;  // number of consecutive CALL/JMPs
    int e8e9pos = 0;    // offset of first CALL or JMP instruction
    int e8e9last = 0;   // offset of most recent CALL or JMP

    int soi = 0, sof = 0, sos = 0, app = 0;  // For JPEG detection - position where found
    int wavi = 0, wavsize = 0, wavch = 0, wavbps = 0, wavm = 0;  // For WAVE detection
    int aiff = 0, aiffm = 0, aiffs = 0;  // For AIFF detection
    int s3mi = 0, s3mno = 0, s3mni = 0;  // For S3M detection
    int bmp = 0, imgbpp = 0, bmpx = 0, bmpy = 0, bmpof = 0, hdrless = 0;  // For BMP detection
    int rgbi = 0, rgbx = 0, rgby = 0;  // For RGB detection
    int tga = 0, tgax = 0, tgay = 0, tgaz = 0, tgat = 0;  // For TGA detection
    int pgm = 0, pgmcomment = 0, pgmw = 0, pgmh = 0, pgm_ptr = 0, pgmc = 0, pgmn = 0;  // For PBM, PGM, PPM detection
    char pgm_buf[32];
    int cdi = 0, cda = 0, cdm = 0;  // For CD sectors detection
    U32 cdf = 0;
    unsigned char zbuf[32], zin[1 << 16], zout[1 << 16]; // For ZLIB stream detection
    int zbufpos = 0, zzippos = -1;
    int pdfim = 0, pdfimw = 0, pdfimh = 0, pdfimb = 0, pdfimp = 0;
    int b64s = 0, b64i = 0, b64line = 0, b64nl = 0; // For base64 detection
    int gif = 0, gifa = 0, gifi = 0, gifw = 0, gifc = 0, gifb = 0; // For GIF detection

    // For image detection
    static int deth = 0, detd = 0;  // detected header/data size in bytes
    static Filetype dett;  // detected block type
    if(deth) { return fseek(in, start + deth, SEEK_SET), deth = 0, dett; }
    else if(detd) { return fseek(in, start + detd, SEEK_SET), detd = 0, DEFAULT; }

    for(int i = 0; i < n; ++i) {
        int c = getc(in);
        if(c == EOF) { return (Filetype) (-1); }
        buf3 = buf3 << 8 | buf2 >> 24;
        buf2 = buf2 << 8 | buf1 >> 24;
        buf1 = buf1 << 8 | buf0 >> 24;
        buf0 = buf0 << 8 | c;

        // ZLIB stream detection
        zbuf[zbufpos] = c;
        zbufpos = (zbufpos + 1) % 32;
        int zh = parse_zlib_header(((int) zbuf[zbufpos]) * 256 + (int) zbuf[(zbufpos + 1) % 32]);
        if((i >= 31 && zh != -1) || zzippos == i) {
            int streamLength = 0, ret = 0;

            // Quick check possible stream by decompressing first 32 bytes
            z_stream strm;
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            strm.next_in = Z_NULL;
            strm.avail_in = 0;
            if(zlib_inflateInit(&strm, zh) == Z_OK) {
                unsigned char tmp[32];
                for(int j = 0; j < 32; j++) { tmp[j] = zbuf[(zbufpos + j) % 32]; }
                strm.next_in = tmp;
                strm.avail_in = 32;
                strm.next_out = zout;
                strm.avail_out = 1 << 16;
                ret = inflate(&strm, Z_FINISH);
                ret = (inflateEnd(&strm) == Z_OK && (ret == Z_STREAM_END || ret == Z_BUF_ERROR) && strm.total_in >= 16);
            }
            if(ret) {
                // Verify valid stream and determine stream length
                long savedpos = ftell(in);
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                strm.next_in = Z_NULL;
                strm.avail_in = 0;
                strm.total_in = strm.total_out = 0;
                if(zlib_inflateInit(&strm, zh) == Z_OK) {
                    for(int j = i - 31; j < n; j += 1 << 16) {
                        unsigned int blsize = min(n - j, 1 << 16);
                        fseek(in, start + j, SEEK_SET);
                        if(fread(zin, 1, blsize, in) != blsize) { break; }
                        strm.next_in = zin;
                        strm.avail_in = blsize;
                        do {
                            strm.next_out = zout;
                            strm.avail_out = 1 << 16;
                            ret = inflate(&strm, Z_FINISH);
                        } while(strm.avail_out == 0 && ret == Z_BUF_ERROR);
                        if(ret == Z_STREAM_END) { streamLength = strm.total_in; }
                        if(ret != Z_BUF_ERROR) { break; }
                    }
                    if(inflateEnd(&strm) != Z_OK) { streamLength = 0; }
                }
                fseek(in, savedpos, SEEK_SET);
            }
            if(streamLength > 0) {
                info = 0;
                if(pdfimw > 0 && pdfimh > 0) {
                    if(pdfimb == 8 && (int) strm.total_out == pdfimw * pdfimh) { info = (8 << 24) + pdfimw; }
                    if(pdfimb == 8 && (int) strm.total_out == pdfimw * pdfimh * 3) { info = (24 << 24) + pdfimw * 3; }
                    if(pdfimb == 4 && (int) strm.total_out == ((pdfimw + 1) / 2) * pdfimh) {
                        info = (8 << 24) + ((pdfimw + 1) / 2);
                    }
                }
                return fseek(in, start + i - 31, SEEK_SET), detd = streamLength, ZLIB;
            }
        }
        if(zh == -1 && zbuf[zbufpos] == 'P' && zbuf[(zbufpos + 1) % 32] == 'K' && zbuf[(zbufpos + 2) % 32] == '\x3'
           && zbuf[(zbufpos + 3) % 32] == '\x4' && zbuf[(zbufpos + 8) % 32] == '\x8' &&
           zbuf[(zbufpos + 9) % 32] == '\0') {
            int nlen = (int) zbuf[(zbufpos + 26) % 32] + ((int) zbuf[(zbufpos + 27) % 32]) * 256
                       + (int) zbuf[(zbufpos + 28) % 32] + ((int) zbuf[(zbufpos + 29) % 32]) * 256;
            if(nlen < 256 && i + 30 + nlen < n) { zzippos = i + 30 + nlen; }
        }
        if(i - pdfimp > 1024) { pdfim = pdfimw = pdfimh = pdfimb = 0; }
        if(pdfim > 1 && !(isspace(c) || isdigit(c))) { pdfim = 1; }
        if(pdfim == 2 && isdigit(c)) { pdfimw = pdfimw * 10 + (c - '0'); }
        if(pdfim == 3 && isdigit(c)) { pdfimh = pdfimh * 10 + (c - '0'); }
        if(pdfim == 4 && isdigit(c)) { pdfimb = pdfimb * 10 + (c - '0'); }
        if((buf0 & 0xffff) == 0x3c3c) { pdfimp = i, pdfim = 1; } // <<
        if(pdfim && (buf1 & 0xffff) == 0x2f57 && buf0 == 0x69647468) { pdfim = 2, pdfimw = 0; } // /Width
        if(pdfim && (buf1 & 0xffffff) == 0x2f4865 && buf0 == 0x69676874) { pdfim = 3, pdfimh = 0; } // /Height
        if(pdfim && buf3 == 0x42697473 && buf2 == 0x50657243 && buf1 == 0x6f6d706f
           && buf0 == 0x6e656e74 && zbuf[(zbufpos + 15) % 32] == '/')
            pdfim = 4, pdfimb = 0; // /BitsPerComponent

        // CD sectors detection (mode 1 and mode 2 form 1+2 - 2352 bytes)
        if(buf1 == 0x00ffffff && buf0 == 0xffffffff && !cdi) cdi = i, cda = -1, cdm = 0;
        if(cdi && i > cdi) {
            const int p = (i - cdi) % 2352;
            if(p == 8 && (buf1 != 0xffffff00 || ((buf0 & 0xff) != 1 && (buf0 & 0xff) != 2))) cdi = 0;
            else if(p == 16 && i + 2336 < n) {
                U8 data[2352];
                long savedpos = ftell(in);
                fseek(in, start + i - 23, SEEK_SET);
                fread(data, 1, 2352, in);
                fseek(in, savedpos, SEEK_SET);
                int t = expand_cd_sector(data, cda, 1);
                if(t != cdm) cdm = t * (i - cdi < 2352);
                if(cdm && cda != 10 && (cdm == 1 || buf0 == buf1)) {
                    if(type != CD) return info = cdm, fseek(in, start + cdi - 7, SEEK_SET), CD;
                    cda = (data[12] << 16) + (data[13] << 8) + data[14];
                    if(cdm != 1 && i - cdi > 2352 && buf0 != cdf) cda = 10;
                    if(cdm != 1) cdf = buf0;
                } else cdi = 0;
            }
            if(!cdi && type == CD) return fseek(in, start + i - p - 7, SEEK_SET), DEFAULT;
        }
        if(type == CD) continue;

        // Detect JPEG by code SOI APPx (FF D8 FF Ex) followed by
        // SOF0 (FF C0 xx xx 08) and SOS (FF DA) within a reasonable distance.
        // Detect end by any code other than RST0-RST7 (FF D9-D7) or
        // a byte stuff (FF 00).

        if(!soi && i >= 3 && (buf0 & 0xffffff00) == 0xffd8ff00 &&
           ((U8) buf0 == 0xC0 || (U8) buf0 == 0xC4 || ((U8) buf0 >= 0xDB && (U8) buf0 <= 0xFE)))
            soi = i, app = i + 2, sos = sof = 0;
        if(soi) {
            if(app == i && (buf0 >> 24) == 0xff &&
               ((buf0 >> 16) & 0xff) > 0xc0 && ((buf0 >> 16) & 0xff) < 0xff)
                app = i + (buf0 & 0xffff) + 2;
            if(app < i && (buf1 & 0xff) == 0xff && (buf0 & 0xff0000ff) == 0xc0000008) sof = i;
            if(sof && sof > soi && i - sof < 0x1000 && (buf0 & 0xffff) == 0xffda) {
                sos = i;
                if(type != JPEG) return fseek(in, start + soi - 3, SEEK_SET), JPEG;
            }
            if(i - soi > 0x40000 && !sos) soi = 0;
        }
        if(type == JPEG && sos && i > sos && (buf0 & 0xff00) == 0xff00
           && (buf0 & 0xff) != 0 && (buf0 & 0xf8) != 0xd0)
            return DEFAULT;

        // Detect .wav file header
        if(buf0 == 0x52494646) wavi = i, wavm = 0;
        if(wavi) {
            const int p = i - wavi;
            if(p == 4) wavsize = bswap(buf0);
            else if(p == 8 && buf0 != 0x57415645) wavi = 0;
            else if(p == 16 && (buf1 != 0x666d7420 || bswap(buf0) != 16)) wavi = 0;
            else if(p == 22) wavch = bswap(buf0) & 0xffff;
            else if(p == 34) wavbps = bswap(buf0) & 0xffff;
            else if(p == 40 + wavm && buf1 != 0x64617461) wavm += bswap(buf0) + 8, wavi = (wavm > 0xfffff ? 0 : wavi);
            else if(p == 40 + wavm) {
                int wavd = bswap(buf0);
                if((wavch == 1 || wavch == 2) && (wavbps == 8 || wavbps == 16) && wavd > 0 && wavsize >= wavd + 36
                   && wavd % ((wavbps / 8) * wavch) == 0)
                    AUD_DET(AUDIO, wavi - 3, 44 + wavm, wavd, wavch + wavbps / 4 - 3);
                wavi = 0;
            }
        }

        // Detect .aiff file header
        if(buf0 == 0x464f524d) aiff = i, aiffs = 0; // FORM
        if(aiff) {
            const int p = i - aiff;
            if(p == 12 && (buf1 != 0x41494646 || buf0 != 0x434f4d4d)) aiff = 0; // AIFF COMM
            else if(p == 24) {
                const int bits = buf0 & 0xffff, chn = buf1 >> 16;
                if((bits == 8 || bits == 16) && (chn == 1 || chn == 2)) aiffm = chn + bits / 4 + 1; else aiff = 0;
            } else if(p == 42 + aiffs && buf1 != 0x53534e44)
                aiffs += (buf0 + 8) + (buf0 & 1), aiff = (aiffs > 0x400 ? 0 : aiff);
            else if(p == 42 + aiffs) AUD_DET(AUDIO, aiff - 3, 54 + aiffs, buf0 - 8, aiffm);
        }

        // Detect .mod file header
        if((buf0 == 0x4d2e4b2e || buf0 == 0x3643484e || buf0 == 0x3843484e  // M.K. 6CHN 8CHN
            || buf0 == 0x464c5434 || buf0 == 0x464c5438) && (buf1 & 0xc0c0c0c0) == 0 && i >= 1083) {
            long savedpos = ftell(in);
            const int chn = ((buf0 >> 24) == 0x36 ? 6 : (((buf0 >> 24) == 0x38 || (buf0 & 0xff) == 0x38) ? 8 : 4));
            int len = 0; // total length of samples
            int numpat = 1; // number of patterns
            for(int j = 0; j < 31; j++) {
                fseek(in, start + i - 1083 + 42 + j * 30, SEEK_SET);
                const int i1 = getc(in);
                const int i2 = getc(in);
                len += i1 * 512 + i2 * 2;
            }
            fseek(in, start + i - 131, SEEK_SET);
            for(int j = 0; j < 128; j++) {
                int x = getc(in);
                if(x + 1 > numpat) numpat = x + 1;
            }
            if(numpat < 65) AUD_DET(AUDIO, i - 1083, 1084 + numpat * 256 * chn, len, 4);
            fseek(in, savedpos, SEEK_SET);
        }

        // Detect .s3m file header
        if(buf0 == 0x1a100000) s3mi = i, s3mno = s3mni = 0;
        if(s3mi) {
            const int p = i - s3mi;
            if(p == 4) s3mno = bswap(buf0) & 0xffff, s3mni = (bswap(buf0) >> 16);
            else if(p == 16 && (((buf1 >> 16) & 0xff) != 0x13 || buf0 != 0x5343524d)) s3mi = 0;
            else if(p == 16) {
                long savedpos = ftell(in);
                int b[31], sam_start = (1 << 16), sam_end = 0, ok = 1;
                for(int j = 0; j < s3mni; j++) {
                    fseek(in, start + s3mi - 31 + 0x60 + s3mno + j * 2, SEEK_SET);
                    int i1 = getc(in);
                    i1 += getc(in) * 256;
                    fseek(in, start + s3mi - 31 + i1 * 16, SEEK_SET);
                    i1 = getc(in);
                    if(i1 == 1) { // type: sample
                        for(int k = 0; k < 31; k++) b[k] = fgetc(in);
                        int len = b[15] + (b[16] << 8);
                        int ofs = b[13] + (b[14] << 8);
                        if(b[30] > 1) ok = 0;
                        if(ofs * 16 < sam_start) sam_start = ofs * 16;
                        if(ofs * 16 + len > sam_end) sam_end = ofs * 16 + len;
                    }
                }
                if(ok && sam_start < (1 << 16)) AUD_DET(AUDIO, s3mi - 31, sam_start, sam_end - sam_start, 0);
                s3mi = 0;
                fseek(in, savedpos, SEEK_SET);
            }
        }

        // Detect .bmp image
        if(!bmp && (((buf0 & 0xffff) == 16973) ||
                    (!(buf0 & 0xFFFFFF) && ((buf0 >> 24) == 0x28)))) //possible 'BM' or headerless DIB
            imgbpp = bmpx = bmpy = 0, hdrless = !(U8) buf0, bmpof = hdrless * 68, bmp = i - hdrless * 16;
        if(bmp) {
            const int p = i - bmp;
            if(p == 12) bmpof = bswap(buf0);
            else if(p == 16 && buf0 != 0x28000000) bmp = 0; //BITMAPINFOHEADER (0x28)
            else if(p == 20) bmpx = bswap(buf0), bmp = ((bmpx == 0 || bmpx > 0x30000) ? 0 : bmp); //width
            else if(p == 24) bmpy = abs((int) bswap(buf0)), bmp = ((bmpy == 0 || bmpy > 0x10000) ? 0 : bmp); //height
            else if(p == 27) imgbpp = c, bmp = ((imgbpp != 1 && imgbpp != 8 && imgbpp != 24 && imgbpp != 32) ? 0 : bmp);
            else if((p == 31) && buf0) bmp = 0;
                // check number of colors in palette (4 bytes), must be 0 (default) or <= 1<<bpp.
                // also check if image is too small, since it might not be worth it to use the image models
            else if(p == 48) {
                if((!buf0 || ((bswap(buf0) <= (U32) (1 << imgbpp)) && (imgbpp <= 8))) &&
                   (((bmpx * bmpy * imgbpp) >> 3) > 512)) {
                    // possible icon/cursor?
                    if(hdrless && (bmpx * 2 == bmpy) && (
                            (bmpx == 8) || (bmpx == 10) || (bmpx == 14) || (bmpx == 16) || (bmpx == 20) ||
                            (bmpx == 22) || (bmpx == 24) || (bmpx == 32) || (bmpx == 40) || (bmpx == 48) ||
                            (bmpx == 60) || (bmpx == 64) || (bmpx == 72) || (bmpx == 80) || (bmpx == 96) ||
                            (bmpx == 128) || (bmpx == 256)
                    ))
                        bmpy = bmpx;

                    // if DIB and not 24bpp, we must calculate the data offset based on BPP or num. of entries in color palette
                    if(hdrless && (imgbpp <= 24))
                        bmpof += (buf0) ? bswap(buf0) * 4 : 4 << imgbpp;

                    if(imgbpp == 1) IMG_DET(IMAGE1, bmp - 1, bmpof, (((bmpx - 1) >> 5) + 1) * 4, bmpy);
                    else if(imgbpp == 8) {
                        fseek(in, start + bmp + 53, SEEK_SET);
                        IMG_DET((IsGrayscalePalette(in, (buf0) ? bswap(buf0) : 1 << imgbpp, 1)) ? IMAGE8GRAY : IMAGE8,
                                bmp - 1, bmpof, (bmpx + 3) & -4, bmpy);
                    } else if(imgbpp == 24) IMG_DET(IMAGE24, bmp - 1, bmpof, ((bmpx * 3) + 3) & -4, bmpy);
                    else if(imgbpp == 32) IMG_DET(IMAGE32, bmp - 1, bmpof, bmpx * 4, bmpy);
                }
                bmp = 0;
            }
        }

        // Detect .pbm .pgm .ppm image
        if((buf0 & 0xfff0ff) == 0x50300a) {
            pgmn = (buf0 & 0xf00) >> 8;
            if(pgmn >= 4 && pgmn <= 6) pgm = i, pgm_ptr = pgmw = pgmh = pgmc = pgmcomment = 0;
        }
        if(pgm) {
            if(i - pgm == 1 && c == 0x23) pgmcomment = 1; //pgm comment
            if(!pgmcomment && pgm_ptr) {
                int s = 0;
                if(c == 0x20 && !pgmw) s = 1;
                else if(c == 0x0a && !pgmh) s = 2;
                else if(c == 0x0a && !pgmc && pgmn != 4) s = 3;
                if(s) {
                    pgm_buf[pgm_ptr++] = 0;
                    int v = atoi(pgm_buf);
                    if(s == 1) pgmw = v; else if(s == 2) pgmh = v; else if(s == 3) pgmc = v;
                    if(v == 0 || (s == 3 && v > 255)) pgm = 0; else pgm_ptr = 0;
                }
            }
            if(!pgmcomment) pgm_buf[pgm_ptr++] = c;
            if(pgm_ptr >= 32) pgm = 0;
            if(pgmcomment && c == 0x0a) pgmcomment = 0;
            if(pgmw && pgmh && !pgmc && pgmn == 4) IMG_DET(IMAGE1, pgm - 2, i - pgm + 3, (pgmw + 7) / 8, pgmh);
            if(pgmw && pgmh && pgmc && pgmn == 5) IMG_DET(IMAGE8GRAY, pgm - 2, i - pgm + 3, pgmw, pgmh);
            if(pgmw && pgmh && pgmc && pgmn == 6) IMG_DET(IMAGE24, pgm - 2, i - pgm + 3, pgmw * 3, pgmh);
        }

        // Detect .rgb image
        if((buf0 & 0xffff) == 0x01da) rgbi = i, rgbx = rgby = 0;
        if(rgbi) {
            const int p = i - rgbi;
            if(p == 1 && c != 0) rgbi = 0;
            else if(p == 2 && c != 1) rgbi = 0;
            else if(p == 4 && (buf0 & 0xffff) != 1 && (buf0 & 0xffff) != 2 && (buf0 & 0xffff) != 3) rgbi = 0;
            else if(p == 6) rgbx = buf0 & 0xffff, rgbi = (rgbx == 0 ? 0 : rgbi);
            else if(p == 8) rgby = buf0 & 0xffff, rgbi = (rgby == 0 ? 0 : rgbi);
            else if(p == 10) {
                int z = buf0 & 0xffff;
                if(rgbx && rgby && (z == 1 || z == 3 || z == 4)) IMG_DET(IMAGE8, rgbi - 1, 512, rgbx, rgby * z);
                rgbi = 0;
            }
        }

        // Detect .tiff file header (2/8/24 bit color, not compressed).
        if(buf1 == 0x49492a00 && n > i + (int) bswap(buf0)) {
            long savedpos = ftell(in);
            fseek(in, start + i + bswap(buf0) - 7, SEEK_SET);

            // read directory
            int dirsize = getc(in);
            int tifx = 0, tify = 0, tifz = 0, tifzb = 0, tifc = 0, tifofs = 0, tifofval = 0, b[12];
            if(getc(in) == 0) {
                for(int i = 0; i < dirsize; i++) {
                    for(int j = 0; j < 12; j++) b[j] = getc(in);
                    if(b[11] == EOF) break;
                    int tag = b[0] + (b[1] << 8);
                    int tagfmt = b[2] + (b[3] << 8);
                    int taglen = b[4] + (b[5] << 8) + (b[6] << 16) + (b[7] << 24);
                    int tagval = b[8] + (b[9] << 8) + (b[10] << 16) + (b[11] << 24);
                    if(tagfmt == 3 || tagfmt == 4) {
                        if(tag == 256) tifx = tagval;
                        else if(tag == 257) tify = tagval;
                        else if(tag == 258) tifzb = taglen == 1 ? tagval : 8; // bits per component
                        else if(tag == 259) tifc = tagval; // 1 = no compression
                        else if(tag == 273 && tagfmt == 4) tifofs = tagval, tifofval = (taglen <= 1);
                        else if(tag == 277) tifz = tagval; // components per pixel
                    }
                }
            }
            if(tifx && tify && tifzb && (tifz == 1 || tifz == 3) && (tifc == 1) && (tifofs && tifofs + i < n)) {
                if(!tifofval) {
                    fseek(in, start + i + tifofs - 7, SEEK_SET);
                    for(int j = 0; j < 4; j++) b[j] = getc(in);
                    tifofs = b[0] + (b[1] << 8) + (b[2] << 16) + (b[3] << 24);
                }
                if(tifofs && tifofs < (1 << 18) && tifofs + i < n) {
                    if(tifz == 1 && tifzb == 1) IMG_DET(IMAGE1, i - 7, tifofs, ((tifx - 1) >> 3) + 1, tify);
                    else if(tifz == 1 && tifzb == 8) IMG_DET(IMAGE8, i - 7, tifofs, tifx, tify);
                    else if(tifz == 3 && tifzb == 8) IMG_DET(IMAGE24, i - 7, tifofs, tifx * 3, tify);
                }
            }
            fseek(in, savedpos, SEEK_SET);
        }

        // Detect .tga image (8-bit 256 colors or 24-bit uncompressed)
        if(buf1 == 0x00010100 && buf0 == 0x00000118) tga = i, tgax = tgay, tgaz = 8, tgat = 1;
        else if(buf1 == 0x00000200 && buf0 == 0x00000000) tga = i, tgax = tgay, tgaz = 24, tgat = 2;
        else if(buf1 == 0x00000300 && buf0 == 0x00000000) tga = i, tgax = tgay, tgaz = 8, tgat = 3;
        if(tga) {
            if(i - tga == 8) tga = (buf1 == 0 ? tga : 0), tgax = (bswap(buf0) & 0xffff), tgay = (bswap(buf0) >> 16);
            else if(i - tga == 10) {
                if(tgaz == (int) ((buf0 & 0xffff) >> 8) && tgax && tgay) {
                    if(tgat == 1) {
                        fseek(in, start + tga + 11, SEEK_SET);
                        IMG_DET((IsGrayscalePalette(in)) ? IMAGE8GRAY : IMAGE8, tga - 7, 18 + 256 * 3, tgax, tgay);
                    } else if(tgat == 2) IMG_DET(IMAGE24, tga - 7, 18, tgax * 3, tgay);
                    else if(tgat == 3) IMG_DET(IMAGE8, tga - 7, 18, tgax, tgay);
                }
                tga = 0;
            }
        }

        // Detect .gif
        if(type == DEFAULT && dett == GIF && i == 0) {
            dett = DEFAULT;
            if(c == 0x2c || c == 0x21) gif = 2, gifi = 2;
        }
        if(!gif && (buf1 & 0xffff) == 0x4749 && (buf0 == 0x46383961 || buf0 == 0x46383761)) gif = 1, gifi = i + 5;
        if(gif) {
            if(gif == 1 && i == gifi) gif = 2, gifi = i + 5 + ((c & 128) ? (3 * (2 << (c & 7))) : 0);
            if(gif == 2 && i == gifi) {
                if((buf0 & 0xff0000) == 0x210000) gif = 5, gifi = i;
                else if((buf0 & 0xff0000) == 0x2c0000) gif = 3, gifi = i;
                else gif = 0;
            }
            if(gif == 3 && i == gifi + 6) gifw = (bswap(buf0) & 0xffff);
            if(gif == 3 && i == gifi + 7)
                gif = 4, gifc = gifb = 0, gifa = gifi = i + 2 + ((c & 128) ? (3 * (2 << (c & 7))) : 0);
            if(gif == 4 && i == gifi) {
                if(c > 0 && gifb && gifc != gifb) gifw = 0;
                if(c > 0) gifb = gifc, gifc = c, gifi += c + 1;
                else if(!gifw) gif = 2, gifi = i + 3;
                else return fseek(in, start + gifa - 1, SEEK_SET), detd = i - gifa + 2, info = gifw, dett = GIF;
            }
            if(gif == 5 && i == gifi) {
                if(c > 0) gifi += c + 1; else gif = 2, gifi = i + 3;
            }
        }

        // Detect EXE if the low order byte (little-endian) XX is more
        // recently seen (and within 4K) if a relative to absolute address
        // conversion is done in the context CALL/JMP (E8/E9) XX xx xx 00/FF
        // 4 times in a row.  Detect end of EXE at the last
        // place this happens when it does not happen for 64KB.

        if(((buf1 & 0xfe) == 0xe8 || (buf1 & 0xfff0) == 0x0f80) && ((buf0 + 1) & 0xfe) == 0) {
            int r = buf0 >> 24;  // relative address low 8 bits
            int a = ((buf0 >> 24) + i) & 0xff;  // absolute address low 8 bits
            int rdist = i - relpos[r];
            int adist = i - abspos[a];
            if(adist < rdist && adist < 0x800 && abspos[a] > 5) {
                e8e9last = i;
                ++e8e9count;
                if(e8e9pos == 0 || e8e9pos > abspos[a]) e8e9pos = abspos[a];
            } else e8e9count = 0;
            if(type == DEFAULT && e8e9count >= 4 && e8e9pos > 5)
                return fseek(in, start + e8e9pos - 5, SEEK_SET), EXE;
            abspos[a] = i;
            relpos[r] = i;
        }
        if(i - e8e9last > 0x4000) {
            if(type == EXE) return fseek(in, start + e8e9last, SEEK_SET), DEFAULT;
            e8e9count = e8e9pos = 0;
        }

        // Detect base64 encoded data
        if(b64s == 0 && buf0 == 0x73653634 && ((buf1 & 0xffffff) == 0x206261 || (buf1 & 0xffffff) == 0x204261))
            b64s = 1, b64i = i - 6; //' base64' ' Base64'
        if(b64s == 0 && ((buf1 == 0x3b626173 && buf0 == 0x6536342c) || (buf1 == 0x215b4344 && buf0 == 0x4154415b)))
            b64s = 3, b64i = i + 1; // ';base64,' '![CDATA['
        if(b64s > 0) {
            if(b64s == 1 && buf0 == 0x0d0a0d0a) b64s = ((i - b64i >= 128) ? 0 : 2), b64i = i + 1, b64line = 0;
            else if(b64s == 2 && (buf0 & 0xffff) == 0x0d0a && b64line == 0) b64line = i + 1 - b64i, b64nl = i;
            else if(b64s == 2 && (buf0 & 0xffff) == 0x0d0a && b64line > 0 && (buf0 & 0xffffff) != 0x3d0d0a) {
                if(i - b64nl < b64line && buf0 != 0x0d0a0d0a) i -= 1, b64s = 5;
                else if(buf0 == 0x0d0a0d0a) i -= 3, b64s = 5;
                else if(i - b64nl == b64line) b64nl = i;
                else b64s = 0;
            } else if(b64s == 2 && (buf0 & 0xffffff) == 0x3d0d0a) i -= 1, b64s = 5; // '=' or '=='
            else if(b64s == 2 && !(isalnum(c) || c == '+' || c == '/' || c == 10 || c == 13 || c == '=')) b64s = 0;
            if(b64line > 0 && (b64line <= 4 || b64line > 255)) b64s = 0;
            if(b64s == 3 && i >= b64i && !(isalnum(c) || c == '+' || c == '/' || c == '=')) b64s = 4;
            if((b64s == 4 && i - b64i > 128) || (b64s == 5 && i - b64i > 512 && i - b64i < (1 << 27)))
                return fseek(in, start + b64i, SEEK_SET), detd = i - b64i, BASE64;
            if(b64s > 3) b64s = 0;
        }
    }
    return type;
}

typedef enum {
    FDECOMPRESS, FCOMPARE, FDISCARD
} FMode;

void encode_cd(FILE *in, FILE *out, int len, int info) {
    const int BLOCK = 2352;
    U8 blk[BLOCK];
    fputc((len % BLOCK) >> 8, out);
    fputc(len % BLOCK, out);
    for(int offset = 0; offset < len; offset += BLOCK) {
        if(offset + BLOCK > len) {
            fread(&blk[0], 1, len - offset, in);
            fwrite(&blk[0], 1, len - offset, out);
        } else {
            fread(&blk[0], 1, BLOCK, in);
            if(info == 3) { blk[15] = 3; }
            if(offset == 0) { fwrite(&blk[12], 1, 4 + 4 * (blk[15] != 1), out); }
            fwrite(&blk[16 + 8 * (blk[15] != 1)], 1, 2048 + 276 * (info == 3), out);
            if(offset + BLOCK * 2 > len && blk[15] != 1) { fwrite(&blk[16], 1, 4, out); }
        }
    }
}

int decode_cd(FILE *in, int size, FILE *out, FMode mode, int &diffFound) {
    const int BLOCK = 2352;
    U8 blk[BLOCK];
    long i = 0, i2 = 0;
    int a = -1, bsize = 0, q = fgetc(in);
    q = (q << 8) + fgetc(in);
    size -= 2;
    while(i < size) {
        if(size - i == q) {
            fread(blk, q, 1, in);
            fwrite(blk, q, 1, out);
            i += q;
            i2 += q;
        } else if(i == 0) {
            fread(blk + 12, 4, 1, in);
            if(blk[15] != 1) { fread(blk + 16, 4, 1, in); }
            bsize = 2048 + (blk[15] == 3) * 276;
            i += 4 * (blk[15] != 1) + 4;
        } else {
            a = (blk[12] << 16) + (blk[13] << 8) + blk[14];
        }
        fread(blk + 16 + (blk[15] != 1) * 8, bsize, 1, in);
        i += bsize;
        if(bsize > 2048) { blk[15] = 3; }
        if(blk[15] != 1 && size - q - i == 4) {
            fread(blk + 16, 4, 1, in);
            i += 4;
        }
        expand_cd_sector(blk, a, 0);
        if(mode == FDECOMPRESS) { fwrite(blk, BLOCK, 1, out); }
        else if(mode == FCOMPARE) {
            for(int j = 0; j < BLOCK; ++j) {
                if(blk[j] != getc(out) && !diffFound) {
                    diffFound = i2 + j + 1;
                }
            }
        }
        i2 += BLOCK;
    }
    return i2;
}


// 24-bit image data transform:
// simple color transform (b, g, r) -> (g, g-r, g-b)

void encode_bmp(FILE *in, FILE *out, int len, int width) {
    int r, g, b;
    for(int i = 0; i < len / width; i++) {
        for(int j = 0; j < width / 3; j++) {
            b = fgetc(in), g = fgetc(in), r = fgetc(in);
            fputc(g, out);
            fputc(g - r, out);
            fputc(g - b, out);
        }
        for(int j = 0; j < width % 3; j++) { fputc(fgetc(in), out); }
    }
}

int decode_bmp(Encoder &en, int size, int width, FILE *out, FMode mode, int &diffFound) {
    int r, g, b, p;
    for(int i = 0; i < size / width; i++) {
        p = i * width;
        for(int j = 0; j < width / 3; j++) {
            b = en.decompress(), g = en.decompress(), r = en.decompress();
            if(mode == FDECOMPRESS) {
                fputc(b - r, out);
                fputc(b, out);
                fputc(b - g, out);
                if(!j && !(i & 0xf)) { en.print_status(); }
            } else if(mode == FCOMPARE) {
                if(((b - r) & 255) != getc(out) && !diffFound) { diffFound = p + 1; }
                if(b != getc(out) && !diffFound) { diffFound = p + 2; }
                if(((b - g) & 255) != getc(out) && !diffFound) { diffFound = p + 3; }
                p += 3;
            }
        }
        for(int j = 0; j < width % 3; j++) {
            if(mode == FDECOMPRESS) {
                fputc(en.decompress(), out);
            } else if(mode == FCOMPARE) {
                if(en.decompress() != getc(out) && !diffFound) { diffFound = p + j + 1; }
            }
        }
    }
    return size;
}

// EXE transform: <encoded-size> <begin> <block>...
// Encoded-size is 4 bytes, MSB first.
// begin is the offset of the start of the input file, 4 bytes, MSB first.
// Each block applies the e8e9 transform to strings falling entirely
// within the block starting from the end and working backwards.
// The 5 byte pattern is E8/E9 xx xx xx 00/FF (x86 CALL/JMP xxxxxxxx)
// where xxxxxxxx is a relative address LSB first.  The address is
// converted to an absolute address by adding the offset mod 2^25
// (in range +-2^24).

void encode_exe(FILE *in, FILE *out, int len, int begin) {
    const int BLOCK = 0x10000;
    Array<U8> blk(BLOCK);
    fprintf(out, "%c%c%c%c", begin >> 24, begin >> 16, begin >> 8, begin);

    // Transform
    for(int offset = 0; offset < len; offset += BLOCK) {
        int size = min(len - offset, BLOCK);
        int bytesRead = fread(&blk[0], 1, size, in);
        if(bytesRead != size) { quit("encode_exe read error"); }
        for(int i = bytesRead - 1; i >= 5; --i) {
            if((blk[i - 4] == 0xe8 || blk[i - 4] == 0xe9 || (blk[i - 5] == 0x0f && (blk[i - 4] & 0xf0) == 0x80))
               && (blk[i] == 0 || blk[i] == 0xff)) {
                int a = (blk[i - 3] | blk[i - 2] << 8 | blk[i - 1] << 16 | blk[i] << 24) + offset + begin + i + 1;
                a <<= 7;
                a >>= 7;
                blk[i] = a >> 24;
                blk[i - 1] = a ^ 176;
                blk[i - 2] = (a >> 8) ^ 176;
                blk[i - 3] = (a >> 16) ^ 176;
            }
        }
        fwrite(&blk[0], 1, bytesRead, out);
    }
}

int decode_exe(Encoder &en, int size, FILE *out, FMode mode, int &diffFound) {
    const int BLOCK = 0x10000;  // block size
    int begin, offset = 6, a;
    U8 c[6];
    begin = en.decompress() << 24;
    begin |= en.decompress() << 16;
    begin |= en.decompress() << 8;
    begin |= en.decompress();
    size -= 4;
    for(int i = 4; i >= 0; i--) { c[i] = en.decompress(); }  // Fill queue

    while(offset < size + 6) {
        memmove(c + 1, c, 5);
        if(offset <= size) { c[0] = en.decompress(); }
        // E8E9 transform: E8/E9 xx xx xx 00/FF -> subtract location from x
        if((c[0] == 0x00 || c[0] == 0xFF) && (c[4] == 0xE8 || c[4] == 0xE9 || (c[5] == 0x0F && (c[4] & 0xF0) == 0x80))
           && (((offset - 1) ^ (offset - 6)) & -BLOCK) == 0 && offset <= size) { // not crossing block boundary
            a = ((c[1] ^ 176) | (c[2] ^ 176) << 8 | (c[3] ^ 176) << 16 | c[0] << 24) - offset - begin;
            a <<= 7;
            a >>= 7;
            c[3] = a;
            c[2] = a >> 8;
            c[1] = a >> 16;
            c[0] = a >> 24;
        }
        if(mode == FDECOMPRESS) { putc(c[5], out); }
        else if(mode == FCOMPARE && c[5] != getc(out) && !diffFound) { diffFound = offset - 6 + 1; }
        if(mode == FDECOMPRESS && !(offset & 0xfff)) { en.print_status(); }
        offset++;
    }
    return size;
}

int encode_zlib(FILE *in, FILE *out, int len) {
    const int BLOCK = 1 << 16, LIMIT = 128;
    U8 zin[BLOCK * 2], zout[BLOCK], zrec[BLOCK * 2], diffByte[81 * LIMIT];
    int diffPos[81 * LIMIT];

    // Step 1 - parse offset type form zlib stream header
    long pos = ftell(in);
    unsigned int h1 = fgetc(in), h2 = fgetc(in);
    fseek(in, pos, SEEK_SET);
    int zh = parse_zlib_header(h1 * 256 + h2);
    int memlevel, clevel, window = zh == -1 ? 0 : MAX_WBITS + 10 + zh / 4, ctype = zh % 4;
    int minclevel = window == 0 ? 1 : ctype == 3 ? 7 : ctype == 2 ? 6 : ctype == 1 ? 2 : 1;
    int maxclevel = window == 0 ? 9 : ctype == 3 ? 9 : ctype == 2 ? 6 : ctype == 1 ? 5 : 1;

    // Step 2 - check recompressiblitiy, determine parameters and save differences
    z_stream main_strm, rec_strm[81];
    int diffCount[81], recpos[81], main_ret = Z_STREAM_END;
    main_strm.zalloc = Z_NULL;
    main_strm.zfree = Z_NULL;
    main_strm.opaque = Z_NULL;
    main_strm.next_in = Z_NULL;
    main_strm.avail_in = 0;
    if(zlib_inflateInit(&main_strm, zh) != Z_OK) { return false; }
    for(int i = 0; i < 81; i++) {
        memlevel = (i % 9) + 1;
        clevel = (i / 9) + 1;
        rec_strm[i].zalloc = Z_NULL;
        rec_strm[i].zfree = Z_NULL;
        rec_strm[i].opaque = Z_NULL;
        rec_strm[i].next_in = Z_NULL;
        rec_strm[i].avail_in = 0;
        int ret = deflateInit2(&rec_strm[i], clevel, Z_DEFLATED, window - MAX_WBITS, memlevel, Z_DEFAULT_STRATEGY);
        diffCount[i] = (clevel >= minclevel && clevel <= maxclevel && ret == Z_OK) ? 0 : LIMIT;
        recpos[i] = BLOCK * 2;
        diffPos[i * LIMIT] = -1;
        diffByte[i * LIMIT] = 0;
    }
    for(int i = 0; i < len; i += BLOCK) {
        unsigned int blsize = min(len - i, BLOCK);
        for(int j = 0; j < 81; j++) {
            if(diffCount[j] >= LIMIT) { continue; }
            memmove(&zrec[0], &zrec[BLOCK], BLOCK);
            recpos[j] -= BLOCK;
        }
        memmove(&zin[0], &zin[BLOCK], BLOCK);
        fread(&zin[BLOCK], 1, blsize, in); // Read block from input file

        // Decompress/inflate block
        main_strm.next_in = &zin[BLOCK];
        main_strm.avail_in = blsize;
        do {
            main_strm.next_out = &zout[0];
            main_strm.avail_out = BLOCK;
            main_ret = inflate(&main_strm, Z_FINISH);

            // Recompress/deflate block with all possible parameters
            for(int j = 0; j < 81; j++) {
                if(diffCount[j] >= LIMIT) { continue; }
                rec_strm[j].next_in = &zout[0];
                rec_strm[j].avail_in = BLOCK - main_strm.avail_out;
                rec_strm[j].next_out = &zrec[recpos[j]];
                rec_strm[j].avail_out = BLOCK * 2 - recpos[j];
                int ret = deflate(&rec_strm[j], (int) main_strm.total_in == len ? Z_FINISH : Z_NO_FLUSH);
                if(ret != Z_BUF_ERROR && ret != Z_STREAM_END && ret != Z_OK) {
                    diffCount[j] = LIMIT;
                    continue;
                }

                // Compare
                int end = 2 * BLOCK - (int) rec_strm[j].avail_out;
                int tail = max(main_ret == Z_STREAM_END ? len - (int) rec_strm[j].total_out : 0, 0);
                for(int k = recpos[j]; k < end + tail; k++) {
                    if((k < end && i + k - BLOCK < len && zrec[k] != zin[k]) || k >= end) {
                        if(++diffCount[j] < LIMIT) {
                            const int p = j * LIMIT + diffCount[j];
                            diffPos[p] = i + k - BLOCK;
                            diffByte[p] = zin[k];
                        }
                    }
                }
                recpos[j] = 2 * BLOCK - rec_strm[j].avail_out;
            }
        } while(main_strm.avail_out == 0 && main_ret == Z_BUF_ERROR);
        if(main_ret != Z_BUF_ERROR && main_ret != Z_STREAM_END) { break; }
    }
    int minCount = LIMIT, index;
    for(int i = 80; i >= 0; i--) {
        deflateEnd(&rec_strm[i]);
        if(diffCount[i] < minCount) {
            minCount = diffCount[i];
            memlevel = (i % 9) + 1;
            clevel = (i / 9) + 1;
            index = i;
        }
    }
    inflateEnd(&main_strm);
    if(minCount == LIMIT) { return false; }

    // Step 3 - write parameters, differences and precompressed (inflated) data
    fputc(diffCount[index], out);
    fputc(window, out);
    fputc(index, out);
    for(int i = 0; i <= diffCount[index]; i++) {
        const int v = i == diffCount[index] ? len - diffPos[index * LIMIT + i]
                                            : diffPos[index * LIMIT + i + 1] - diffPos[index * LIMIT + i] - 1;
        fputc(v >> 24, out);
        fputc(v >> 16, out);
        fputc(v >> 8, out);
        fputc(v, out);
    }
    for(int i = 0; i < diffCount[index]; i++) { fputc(diffByte[index * LIMIT + i + 1], out); }

    fseek(in, pos, SEEK_SET);
    main_strm.zalloc = Z_NULL;
    main_strm.zfree = Z_NULL;
    main_strm.opaque = Z_NULL;
    main_strm.next_in = Z_NULL;
    main_strm.avail_in = 0;
    if(zlib_inflateInit(&main_strm, zh) != Z_OK) { return false; }
    for(int i = 0; i < len; i += BLOCK) {
        unsigned int blsize = min(len - i, BLOCK);
        fread(&zin[0], 1, blsize, in);
        main_strm.next_in = &zin[0];
        main_strm.avail_in = blsize;
        do {
            main_strm.next_out = &zout[0];
            main_strm.avail_out = BLOCK;
            main_ret = inflate(&main_strm, Z_FINISH);
            fwrite(&zout[0], 1, BLOCK - main_strm.avail_out, out);
        } while(main_strm.avail_out == 0 && main_ret == Z_BUF_ERROR);
        if(main_ret != Z_BUF_ERROR && main_ret != Z_STREAM_END) { break; }
    }
    return main_ret == Z_STREAM_END;
}

int decode_zlib(FILE *in, int size, FILE *out, FMode mode, int &diffFound) {
    const int BLOCK = 1 << 16, LIMIT = 128;
    U8 zin[BLOCK], zout[BLOCK];
    int diffCount = min(fgetc(in), LIMIT - 1);
    int window = fgetc(in) - MAX_WBITS;
    int index = fgetc(in);
    int memlevel = (index % 9) + 1;
    int clevel = (index / 9) + 1;
    int len = 0;
    int diffPos[LIMIT];
    diffPos[0] = -1;
    for(int i = 0; i <= diffCount; i++) {
        int v = fgetc(in) << 24;
        v |= fgetc(in) << 16;
        v |= fgetc(in) << 8;
        v |= fgetc(in);
        if(i == diffCount) { len = v + diffPos[i]; } else { diffPos[i + 1] = v + diffPos[i] + 1; }
    }
    U8 diffByte[LIMIT];
    diffByte[0] = 0;
    for(int i = 0; i < diffCount; i++) { diffByte[i + 1] = fgetc(in); }
    size -= 7 + 5 * diffCount;

    z_stream rec_strm;
    int diffIndex = 1, recpos = 0;
    rec_strm.zalloc = Z_NULL;
    rec_strm.zfree = Z_NULL;
    rec_strm.opaque = Z_NULL;
    rec_strm.next_in = Z_NULL;
    rec_strm.avail_in = 0;
    int ret = deflateInit2(&rec_strm, clevel, Z_DEFLATED, window, memlevel, Z_DEFAULT_STRATEGY);
    if(ret != Z_OK) { return 0; }
    for(int i = 0; i < size; i += BLOCK) {
        int blsize = min(size - i, BLOCK);
        fread(&zin[0], 1, blsize, in);
        rec_strm.next_in = &zin[0];
        rec_strm.avail_in = blsize;
        do {
            rec_strm.next_out = &zout[0];
            rec_strm.avail_out = BLOCK;
            ret = deflate(&rec_strm, i + blsize == size ? Z_FINISH : Z_NO_FLUSH);
            if(ret != Z_BUF_ERROR && ret != Z_STREAM_END && ret != Z_OK) { break; }
            const int have = min(BLOCK - rec_strm.avail_out, len - recpos);
            while(diffIndex <= diffCount && diffPos[diffIndex] >= recpos && diffPos[diffIndex] < recpos + have) {
                zout[diffPos[diffIndex] - recpos] = diffByte[diffIndex];
                diffIndex++;
            }
            if(mode == FDECOMPRESS) { fwrite(&zout[0], 1, have, out); }
            else if(mode == FCOMPARE) {
                for(int j = 0; j < have; j++) {
                    if(zout[j] != getc(out) && !diffFound) {
                        diffFound = recpos + j + 1;
                    }
                }
            }
            recpos += have;

        } while(rec_strm.avail_out == 0);
    }
    while(diffIndex <= diffCount) {
        if(mode == FDECOMPRESS) { fputc(diffByte[diffIndex], out); }
        else if(mode == FCOMPARE) { if(diffByte[diffIndex] != getc(out) && !diffFound) { diffFound = recpos + 1; }}
        diffIndex++;
        recpos++;
    }
    deflateEnd(&rec_strm);
    return recpos == len ? len : 0;
}

//
// decode/encode base64
//
static const char table1[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool isbase64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/') || (c == 10) || (c == 13));
}

int decode_base64(FILE *in, FILE *out, FMode mode, int &diffFound) {
    U8 inn[3];
    int i, len1 = 0, len = 0, blocksout = 0;
    int fle = 0;
    int linesize = 0;
    int outlen = 0;
    int tlf = 0;
    linesize = getc(in);
    outlen = getc(in);
    outlen += (getc(in) << 8);
    outlen += (getc(in) << 16);
    tlf = (getc(in));
    outlen += ((tlf & 63) << 24);
    U8 *ptr, *fptr;
    ptr = (U8 *) calloc((outlen >> 2) * 4 + 10, 1);
    if(!ptr) { quit("Out of memory (d_B64)"); }
    fptr = &ptr[0];
    tlf = (tlf & 192);
    if(tlf == 128) {
        tlf = 10;        // LF: 10
    } else if(tlf == 64) {
        tlf = 13;        // LF: 13
    } else {
        tlf = 0;
    }

    while(fle < outlen) {
        len = 0;
        for(i = 0; i < 3; i++) {
            inn[i] = getc(in);
            if(!feof(in)) {
                len++;
                len1++;
            } else {
                inn[i] = 0;
            }
        }
        if(len) {
            U8 in0, in1, in2;
            in0 = inn[0], in1 = inn[1], in2 = inn[2];
            fptr[fle++] = (table1[in0 >> 2]);
            fptr[fle++] = (table1[((in0 & 0x03) << 4) | ((in1 & 0xf0) >> 4)]);
            fptr[fle++] = ((len > 1 ? table1[((in1 & 0x0f) << 2) | ((in2 & 0xc0) >> 6)] : '='));
            fptr[fle++] = ((len > 2 ? table1[in2 & 0x3f] : '='));
            blocksout++;
        }
        if(blocksout >= (linesize / 4) && linesize != 0) { //no lf if linesize==0
            if(blocksout && !feof(in) && fle <= outlen) { //no lf if eof
                if(tlf) { fptr[fle++] = (tlf); }
                else { fptr[fle++] = 13, fptr[fle++] = 10; }
            }
            blocksout = 0;
        }
    }
    //Write out or compare
    if(mode == FDECOMPRESS) {
        fwrite(&ptr[0], 1, outlen, out);

    } else if(mode == FCOMPARE) {
        for(i = 0; i < outlen; i++) {
            U8 b = fptr[i];


            if(b != fgetc(out) && !diffFound) { diffFound = ftell(out); }
        }
    }
    free(ptr);
    return outlen;
}

inline char valueb(char c) {
    const char *p = strchr(table1, c);
    if(p) {
        return p - table1;
    } else {
        return 0;
    }
}

void encode_base64(FILE *in, FILE *out, int len) {
    int in_len = 0;
    int i = 0;
    int j = 0;
    int b = 0;
    int lfp = 0;
    int tlf = 0;
    char src[4];
    U8 *ptr, *fptr;
    int b64mem = (len >> 2) * 3 + 10;
    ptr = (U8 *) calloc(b64mem, 1);
    if(!ptr) { quit("Out of memory (e_B64)"); }
    fptr = &ptr[0];
    int olen = 5;

    while(b = fgetc(in), in_len++, (b != '=') && isbase64(b) && in_len <= len) {
        if(b == 13 || b == 10) {
            if(lfp == 0) { lfp = in_len, tlf = b; }
            if(tlf != b) { tlf = 0; }
            continue;
        }
        src[i++] = b;
        if(i == 4) {
            for(j = 0; j < 4; j++) { src[j] = valueb(src[j]); }
            src[0] = (src[0] << 2) + ((src[1] & 0x30) >> 4);
            src[1] = ((src[1] & 0xf) << 4) + ((src[2] & 0x3c) >> 2);
            src[2] = ((src[2] & 0x3) << 6) + src[3];

            fptr[olen++] = src[0];
            fptr[olen++] = src[1];
            fptr[olen++] = src[2];
            i = 0;
        }
    }

    if(i) {
        for(j = i; j < 4; j++) {
            src[j] = 0;
        }

        for(j = 0; j < 4; j++) {
            src[j] = valueb(src[j]);
        }

        src[0] = (src[0] << 2) + ((src[1] & 0x30) >> 4);
        src[1] = ((src[1] & 0xf) << 4) + ((src[2] & 0x3c) >> 2);
        src[2] = ((src[2] & 0x3) << 6) + src[3];

        for(j = 0; (j < i - 1); j++) {
            fptr[olen++] = src[j];
        }
    }
    fptr[0] = lfp & 255; //nl lenght
    fptr[1] = len & 255;
    fptr[2] = len >> 8 & 255;
    fptr[3] = len >> 16 & 255;
    if(tlf != 0) {
        if(tlf == 10) { fptr[4] = 128; }
        else { fptr[4] = 64; }
    } else {
        fptr[4] = len >> 24 & 63;
    } //1100 0000
    fwrite(&ptr[0], 1, olen, out);
    free(ptr);
}

int encode_gif(FILE *in, FILE *out, int len) {
    int codesize = fgetc(in), diffpos = 0, hdrsize = 6, clearpos = 0, bsize = 0;
    int beginin = ftell(in), beginout = ftell(out);
    U8 output[4096];
    fputc(hdrsize >> 8, out);
    fputc(hdrsize & 255, out);
    fputc(bsize, out);
    fputc(clearpos >> 8, out);
    fputc(clearpos & 255, out);
    fputc(codesize, out);
    for(int phase = 0; phase < 2; phase++) {
        fseek(in, beginin, SEEK_SET);
        int bits = codesize + 1, shift = 0, buf = 0;
        int blocksize = 0, maxcode = (1 << codesize) + 1, last = -1, dict[4096];
        bool end = false;
        while((blocksize = fgetc(in)) > 0 && ftell(in) - beginin < len && !end) {
            for(int i = 0; i < blocksize; i++) {
                buf |= fgetc(in) << shift;
                shift += 8;
                while(shift >= bits && !end) {
                    int code = buf & ((1 << bits) - 1);
                    buf >>= bits;
                    shift -= bits;
                    if(!bsize && code != (1 << codesize)) {
                        hdrsize += 4;
                        fputc(0, out);
                        fputc(0, out);
                        fputc(0, out);
                        fputc(0, out);
                    }
                    if(!bsize) { bsize = blocksize; }
                    if(code == (1 << codesize)) {
                        if(maxcode > (1 << codesize) + 1) {
                            if(clearpos && clearpos != 69631 - maxcode) { return 0; }
                            clearpos = 69631 - maxcode;
                        }
                        bits = codesize + 1, maxcode = (1 << codesize) + 1, last = -1;
                    } else if(code == (1 << codesize) + 1) { end = true; }
                    else if(code > maxcode + 1) { return 0; }
                    else {
                        int j = (code <= maxcode ? code : last), size = 1;
                        while(j >= (1 << codesize)) {
                            output[4096 - (size++)] = dict[j] & 255;
                            j = dict[j] >> 8;
                        }
                        output[4096 - size] = j;
                        if(phase == 1) { fwrite(&output[4096 - size], 1, size, out); } else { diffpos += size; }
                        if(code == maxcode + 1) { if(phase == 1) { fputc(j, out); } else { diffpos++; }}
                        if(last != -1) {
                            if(++maxcode >= 8191) { return 0; }
                            if(maxcode <= 4095) {
                                dict[maxcode] = (last << 8) + j;
                                if(phase == 0) {
                                    bool diff = false;
                                    for(int m = (1 << codesize) + 2; m < min(maxcode, 4095); m++) {
                                        if(dict[maxcode] == dict[m]) {
                                            diff = true;
                                            break;
                                        }
                                    }
                                    if(diff) {
                                        hdrsize += 4;
                                        j = diffpos - size - (code == maxcode);
                                        fputc((j >> 24) & 255, out);
                                        fputc((j >> 16) & 255, out);
                                        fputc((j >> 8) & 255, out);
                                        fputc(j & 255, out);
                                        diffpos = size + (code == maxcode);
                                    }
                                }
                            }
                            if(maxcode >= ((1 << bits) - 1) && bits < 12) { bits++; }
                        }
                        last = code;
                    }
                }
            }
        }
    }
    diffpos = ftell(out);
    fseek(out, beginout, SEEK_SET);
    fputc(hdrsize >> 8, out);
    fputc(hdrsize & 255, out);
    fputc(255 - bsize, out);
    fputc((clearpos >> 8) & 255, out);
    fputc(clearpos & 255, out);
    fseek(out, diffpos, SEEK_SET);
    return ftell(in) - beginin == len - 1;
}

#define gif_write_block(count) { output[0]=(count);\
if (mode==FDECOMPRESS) fwrite(&output[0], 1, (count)+1, out);\
else if (mode==FCOMPARE) for (int j=0; j<(count)+1; j++) if (output[j]!=getc(out) && !diffFound) diffFound=outsize+j+1;\
outsize+=(count)+1; blocksize=0; }

#define gif_write_code(c) { buf+=(c)<<shift; shift+=bits;\
while (shift>=8) { output[++blocksize]=buf&255; buf>>=8;shift-=8;\
if (blocksize==bsize) gif_write_block(bsize); }}

int decode_gif(FILE *in, int size, FILE *out, FMode mode, int &diffFound) {
    int diffcount = fgetc(in), curdiff = 0, diffpos[4096];
    diffcount = ((diffcount << 8) + fgetc(in) - 6) / 4;
    int bsize = 255 - fgetc(in);
    int clearpos = fgetc(in);
    clearpos = (clearpos << 8) + fgetc(in);
    clearpos = (69631 - clearpos) & 0xffff;
    int codesize = fgetc(in), bits = codesize + 1, shift = 0, buf = 0, blocksize = 0;
    if(diffcount > 4096 || clearpos <= (1 << codesize) + 2) { return 1; }
    int maxcode = (1 << codesize) + 1, dict[4096], input;
    for(int i = 0; i < diffcount; i++) {
        diffpos[i] = fgetc(in);
        diffpos[i] = (diffpos[i] << 8) + fgetc(in);
        diffpos[i] = (diffpos[i] << 8) + fgetc(in);
        diffpos[i] = (diffpos[i] << 8) + fgetc(in);
        if(i > 0) { diffpos[i] += diffpos[i - 1]; }
    }
    U8 output[256];
    size -= 6 + diffcount * 4;
    int last = fgetc(in), total = size + 1, outsize = 1;
    if(mode == FDECOMPRESS) { fputc(codesize, out); }
    else if(mode == FCOMPARE) { if(codesize != getc(out) && !diffFound) { diffFound = 1; }}
    if(diffcount == 0 || diffpos[0] != 0) gif_write_code(1 << codesize) else { curdiff++; }
    while(size-- >= 0 && (input = fgetc(in)) >= 0) {
        int code = -1, key = (last << 8) + input;
        for(int i = (1 << codesize) + 2; i <= min(maxcode, 4095); i++) { if(dict[i] == key) { code = i; }}
        if(curdiff < diffcount && total - size > diffpos[curdiff]) { curdiff++, code = -1; }
        if(code == -1) {
            gif_write_code(last);
            if(maxcode == clearpos) {
                gif_write_code(1 << codesize);
                bits = codesize + 1, maxcode = (1 << codesize) + 1;
            } else {
                ++maxcode;
                if(maxcode <= 4095) { dict[maxcode] = key; }
                if(maxcode >= (1 << bits) && bits < 12) { bits++; }
            }
            code = input;
        }
        last = code;
    }
    gif_write_code(last);
    gif_write_code((1 << codesize) + 1);
    if(shift > 0) {
        output[++blocksize] = buf & 255;
        if(blocksize == bsize) gif_write_block(bsize);
    }
    if(blocksize > 0) gif_write_block(blocksize);
    if(mode == FDECOMPRESS) { fputc(0, out); }
    else if(mode == FCOMPARE) { if(0 != getc(out) && !diffFound) { diffFound = outsize + 1; }}
    return outsize + 1;
}


#endif //PAQ8PX_FILTERS_H
