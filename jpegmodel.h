#ifndef PAQ8PX_JPEGMODEL_H
#define PAQ8PX_JPEGMODEL_H

// Model JPEG. Return 1 if a JPEG file is detected or else 0.
// Only the baseline and 8 bit extended Huffman coded DCT modes are
// supported.  The model partially decodes the JPEG image to provide
// context for the Huffman coded symbols.

// Print a JPEG segment at buf[p...] for debugging
/*
void dump(const char* msg, int p) {
  printf("%s:", msg);
  int len=buf[p+2]*256+buf[p+3];
  for (int i=0; i<len+2; ++i)
    printf(" %02X", buf[p+i]);
  printf("\n");
}
*/

#define finish(success){ \
  int length = pos - images[idx].offset; \
  if (success && idx && pos-lastPos==1) \
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bEmbedded JPEG at offset %d, size: %d bytes, level %d\nCompressing... ", images[idx].offset-pos+blpos, length, idx), fflush(stdout); \
  memset(&images[idx], 0, sizeof(JPEGImage)); \
  mcusize=0,dqt_state=-1; \
  idx-=(idx>0); \
  images[idx].app-=length; \
  if (images[idx].app < 0) \
    images[idx].app = 0; \
}

// Detect invalid JPEG data.  The proper response is to silently
// fall back to a non-JPEG model.
#define jassert(x) if (!(x)) { \
/*  printf("JPEG error at %d, line %d: %s\n", pos, __LINE__, #x); */ \
  if (idx>0) \
    finish(false) \
  else \
    images[idx].jpeg=0; \
  return images[idx].next_jpeg;}

struct HUF {
    U32 min, max;
    int val;
}; // Huffman decode tables
// huf[Tc][Th][m] is the minimum, maximum+1, and pointer to codes for
// coefficient type Tc (0=DC, 1=AC), table Th (0-3), length m+1 (m=0-15)

struct JPEGImage {
    int offset, // offset of SOI marker
            jpeg, // 1 if JPEG is header detected, 2 if image data
            next_jpeg, // updated with jpeg on next byte boundary
            app, // Bytes remaining to skip in this marker
            sof, sos, data, // pointers to buf
            htsize; // number of pointers in ht
    int ht[8]; // pointers to Huffman table headers
    U8 qtab[256]; // table
    int qmap[10]; // block -> table number
};

int jpegModel(Mixer &m) {

    // State of parser
    enum {
        SOF0 = 0xc0, SOF1, SOF2, SOF3, DHT, RST0 = 0xd0, SOI = 0xd8, EOI, SOS, DQT,
        DNL, DRI, APP0 = 0xe0, COM = 0xfe, FF
    };  // Second byte of 2 byte codes
    const static int MaxEmbeddedLevel = 3;
    static JPEGImage images[MaxEmbeddedLevel];
    static int idx = -1;
    static int lastPos = 0;

    // Huffman decode state
    static U32 huffcode = 0;  // Current Huffman code including extra bits
    static int huffbits = 0;  // Number of valid bits in huffcode
    static int huffsize = 0;  // Number of bits without extra bits
    static int rs = -1;  // Decoded huffcode without extra bits.  It represents
    // 2 packed 4-bit numbers, r=run of zeros, s=number of extra bits for
    // first nonzero code.  huffcode is complete when rs >= 0.
    // rs is -1 prior to decoding incomplete huffcode.

    static int mcupos = 0;  // position in MCU (0-639).  The low 6 bits mark
    // the coefficient in zigzag scan order (0=DC, 1-63=AC).  The high
    // bits mark the block within the MCU, used to select Huffman tables.

    // Decoding tables
    static Array<HUF> huf(128);  // Tc*64+Th*16+m -> min, max, val
    static int mcusize = 0;  // number of coefficients in an MCU
    static int hufsel[2][10];  // DC/AC, mcupos/64 -> huf decode table
    static Array<U8> hbuf(2048);  // Tc*1024+Th*256+hufcode -> RS

    // Image state
    static Array<int> color(10);  // block -> component (0-3)
    static Array<int> pred(4);  // component -> last DC value
    static int dc = 0;  // DC value of the current block
    static int width = 0;  // Image width in MCU
    static int row = 0, column = 0;  // in MCU (column 0 to width-1)
    static Buf cbuf(0x20000); // Rotating buffer of coefficients, coded as:
    // DC: level shifted absolute value, low 4 bits discarded, i.e.
    //   [-1023...1024] -> [0...255].
    // AC: as an RS code: a run of R (0-15) zeros followed by an S (0-15)
    //   bit number, or 00 for end of block (in zigzag order).
    //   However if R=0, then the format is ssss11xx where ssss is S,
    //   xx is the first 2 extra bits, and the last 2 bits are 1 (since
    //   this never occurs in a valid RS code).
    static int cpos = 0;  // position in cbuf
    static int rs1;  // last RS code
    static int rstpos = 0, rstlen = 0; // reset position
    static int ssum = 0, ssum1 = 0, ssum2 = 0, ssum3 = 0;
    // sum of S in RS codes in block and sum of S in first component

    static IntBuf cbuf2(0x20000);
    static Array<int> adv_pred(4), sumu(8), sumv(8), run_pred(6);
    static int prev_coef = 0, prev_coef2 = 0, prev_coef_rs = 0;
    static Array<int> ls(10);  // block -> distance to previous block
    static Array<int> blockW(10), blockN(10), SamplingFactors(4);
    static Array<int> lcp(7), zpos(64);

    //for parsing Quantization tables
    static int dqt_state = -1, dqt_end = 0, qnum = 0;

    const static U8 zzu[64] = {  // zigzag coef -> u,v
            0, 1, 0, 0, 1, 2, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4,
            3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 5, 6, 7, 7, 6, 7};
    const static U8 zzv[64] = {
            0, 0, 1, 2, 1, 0, 0, 1, 2, 3, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3,
            4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 6, 7, 7};

    // Standard Huffman tables (cf. JPEG standard section K.3)
    // IMPORTANT: these are only valid for 8-bit data precision
    const static U8 bits_dc_luminance[16] = {
            0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
    };
    const static U8 values_dc_luminance[12] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    };

    const static U8 bits_dc_chrominance[16] = {
            0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
    };
    const static U8 values_dc_chrominance[12] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    };

    const static U8 bits_ac_luminance[16] = {
            0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d
    };
    const static U8 values_ac_luminance[162] = {
            0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
            0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
            0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
            0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
            0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
            0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
            0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
            0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
            0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
            0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
            0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
            0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
            0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
            0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
            0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
            0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
            0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
            0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
            0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
            0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
            0xf9, 0xfa
    };

    const static U8 bits_ac_chrominance[16] = {
            0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77
    };
    const static U8 values_ac_chrominance[162] = {
            0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
            0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
            0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
            0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
            0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
            0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
            0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
            0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
            0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
            0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
            0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
            0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
            0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
            0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
            0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
            0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
            0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
            0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
            0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
            0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
            0xf9, 0xfa
    };

    if(idx < 0) {
        memset(&images[0], 0, sizeof(images));
        idx = 0;
        lastPos = pos;
    }

    // Be sure to quit on a byte boundary
    if(!bpos) { images[idx].next_jpeg = images[idx].jpeg > 1; }
    if(bpos && !images[idx].jpeg) { return images[idx].next_jpeg; }
    if(!bpos && images[idx].app > 0) {
        --images[idx].app;
        if(idx < MaxEmbeddedLevel && buf(4) == FF && buf(3) == SOI && buf(2) == FF &&
           (buf(1) == 0xC0 || buf(1) == 0xC4 || (buf(1) >= 0xDB && buf(1) <= 0xFE))) {
               memset(&images[++idx], 0, sizeof(JPEGImage));
        }
    }
    if(images[idx].app > 0) { return images[idx].next_jpeg; }
    if(!bpos) {

        // Parse.  Baseline DCT-Huffman JPEG syntax is:
        // SOI APPx... misc... SOF0 DHT... SOS data EOI
        // SOI (= FF D8) start of image.
        // APPx (= FF Ex) len ... where len is always a 2 byte big-endian length
        //   including the length itself but not the 2 byte preceding code.
        //   Application data is ignored.  There may be more than one APPx.
        // misc codes are DQT, DNL, DRI, COM (ignored).
        // SOF0 (= FF C0) len 08 height width Nf [C HV Tq]...
        //   where len, height, width (in pixels) are 2 bytes, Nf is the repeat
        //   count (1 byte) of [C HV Tq], where C is a component identifier
        //   (color, 0-3), HV is the horizontal and vertical dimensions
        //   of the MCU (high, low bits, packed), and Tq is the quantization
        //   table ID (not used).  An MCU (minimum compression unit) consists
        //   of 64*H*V DCT coefficients for each color.
        // DHT (= FF C4) len [TcTh L1...L16 V1,1..V1,L1 ... V16,1..V16,L16]...
        //   defines Huffman table Th (1-4) for Tc (0=DC (first coefficient)
        //   1=AC (next 63 coefficients)).  L1..L16 are the number of codes
        //   of length 1-16 (in ascending order) and Vx,y are the 8-bit values.
        //   A V code of RS means a run of R (0-15) zeros followed by S (0-15)
        //   additional bits to specify the next nonzero value, negative if
        //   the first additional bit is 0 (e.g. code x63 followed by the
        //   3 bits 1,0,1 specify 7 coefficients: 0, 0, 0, 0, 0, 0, 5.
        //   Code 00 means end of block (remainder of 63 AC coefficients is 0).
        // SOS (= FF DA) len Ns [Cs TdTa]... 0 3F 00
        //   Start of scan.  TdTa specifies DC/AC Huffman tables (0-3, packed
        //   into one byte) for component Cs matching C in SOF0, repeated
        //   Ns (1-4) times.
        // EOI (= FF D9) is end of image.
        // Huffman coded data is between SOI and EOI.  Codes may be embedded:
        // RST0-RST7 (= FF D0 to FF D7) mark the start of an independently
        //   compressed region.
        // DNL (= FF DC) 04 00 height
        //   might appear at the end of the scan (ignored).
        // FF 00 is interpreted as FF (to distinguish from RSTx, DNL, EOI).

        // Detect JPEG (SOI followed by a valid marker)
        if(!images[idx].jpeg && buf(4) == FF && buf(3) == SOI && buf(2) == FF &&
           (buf(1) == 0xC0 || buf(1) == 0xC4 || (buf(1) >= 0xDB && buf(1) <= 0xFE))) {
            images[idx].jpeg = 1;
            images[idx].offset = pos - 4;
            images[idx].sos = images[idx].sof = images[idx].htsize = images[idx].data = 0, images[idx].app =
                    (buf(1) >> 4 == 0xE) * 2;
            mcusize = huffcode = huffbits = huffsize = mcupos = cpos = 0, rs = -1;
            memset(&huf[0], 0, sizeof(huf));
            memset(&pred[0], 0, pred.size() * sizeof(int));
            rstpos = rstlen = 0;
        }

        // Detect end of JPEG when data contains a marker other than RSTx
        // or byte stuff (00), or if we jumped in position since the last byte seen
        if(images[idx].jpeg && images[idx].data &&
           ((buf(2) == FF && buf(1) && (buf(1) & 0xf8) != RST0) || (pos - lastPos > 1))) {
            jassert((buf(1) == EOI) || (pos - lastPos > 1));
            finish(true);
        }
        lastPos = pos;
        if(!images[idx].jpeg) { return images[idx].next_jpeg; }

        // Detect APPx, COM or other markers, so we can skip them
        if(!images[idx].data && !images[idx].app && buf(4) == FF &&
           (((buf(3) >= 0xC1) && (buf(3) <= 0xCF) && (buf(3) != DHT)) || ((buf(3) >= 0xDC) && (buf(3) <= 0xFE)))) {
            images[idx].app = buf(2) * 256 + buf(1) + 2;
            if(idx > 0)
                jassert(pos + images[idx].app < images[idx].offset + images[idx - 1].app);
        }

        // Save pointers to sof, ht, sos, data,
        if(buf(5) == FF && buf(4) == SOS) {
            int len = buf(3) * 256 + buf(2);
            if(len == 6 + 2 * buf(1) && buf(1) && buf(1) <= 4) {  // buf(1) is Ns
                images[idx].sos = pos - 5, images[idx].data = images[idx].sos + len + 2, images[idx].jpeg = 2;
            }
        }
        if(buf(4) == FF && buf(3) == DHT && images[idx].htsize < 8) { images[idx].ht[images[idx].htsize++] = pos - 4; }
        if(buf(4) == FF && buf(3) == SOF0) { images[idx].sof = pos - 4; }

        // Parse Quantizazion tables
        if(buf(4) == FF && buf(3) == DQT) {
            dqt_end = pos + buf(2) * 256 + buf(1) - 1, dqt_state = 0;
        } else if(dqt_state >= 0) {
            if(pos >= dqt_end) {
                dqt_state = -1;
            } else {
                if(dqt_state % 65 == 0) {
                    qnum = buf(1);
                } else {
                    jassert(buf(1) > 0);
                    jassert(qnum >= 0 && qnum < 4);
                    images[idx].qtab[qnum * 64 + ((dqt_state % 65) - 1)] = buf(1) - 1;
                }
                dqt_state++;
            }
        }

        // Restart
        if(buf(2) == FF && (buf(1) & 0xf8) == RST0) {
            huffcode = huffbits = huffsize = mcupos = 0, rs = -1;
            memset(&pred[0], 0, pred.size() * sizeof(int));
            rstlen = column + row * width - rstpos;
            rstpos = column + row * width;
        }
    }

    {
        // Build Huffman tables
        // huf[Tc][Th][m] = min, max+1 codes of length m, pointer to byte values
        if(pos == images[idx].data && bpos == 1) {
            int i;
            for(i = 0; i < images[idx].htsize; ++i) {
                int p = images[idx].ht[i] + 4;  // pointer to current table after length field
                int end = p + buf[p - 2] * 256 + buf[p - 1] - 2;  // end of Huffman table
                int count = 0;  // sanity check
                while(p < end && end < pos && end < p + 2100 && ++count < 10) {
                    int tc = buf[p] >> 4, th = buf[p] & 15;
                    if(tc >= 2 || th >= 4) { break; }
                    jassert(tc >= 0 && tc < 2 && th >= 0 && th < 4);
                    HUF *h = &huf[tc * 64 + th * 16]; // [tc][th][0];
                    int val = p + 17;  // pointer to values
                    int hval = tc * 1024 + th * 256;  // pointer to RS values in hbuf
                    int j;
                    for(j = 0; j < 256; ++j) { // copy RS codes
                        hbuf[hval + j] = buf[val + j];
                    }
                    int code = 0;
                    for(j = 0; j < 16; ++j) {
                        h[j].min = code;
                        h[j].max = code += buf[p + j + 1];
                        h[j].val = hval;
                        val += buf[p + j + 1];
                        hval += buf[p + j + 1];
                        code *= 2;
                    }
                    p = val;
                    jassert(hval >= 0 && hval < 2048);
                }
                jassert(p == end);
            }
            huffcode = huffbits = huffsize = 0, rs = -1;

            // load default tables
            if(!images[idx].htsize) {
                for(int tc = 0; tc < 2; tc++) {
                    for(int th = 0; th < 2; th++) {
                        HUF *h = &huf[tc * 64 + th * 16];
                        int hval = tc * 1024 + th * 256;
                        int code = 0, c = 0, x = 0;

                        for(int i = 0; i < 16; i++) {
                            switch(tc * 2 + th) {
                                case 0:
                                    x = bits_dc_luminance[i];
                                    break;
                                case 1:
                                    x = bits_dc_chrominance[i];
                                    break;
                                case 2:
                                    x = bits_ac_luminance[i];
                                    break;
                                case 3:
                                    x = bits_ac_chrominance[i];
                            }

                            h[i].min = code;
                            h[i].max = (code += x);
                            h[i].val = hval;
                            hval += x;
                            code += code;
                            c += x;
                        }

                        hval = tc * 1024 + th * 256;
                        c--;

                        while(c >= 0) {
                            switch(tc * 2 + th) {
                                case 0:
                                    x = values_dc_luminance[c];
                                    break;
                                case 1:
                                    x = values_dc_chrominance[c];
                                    break;
                                case 2:
                                    x = values_ac_luminance[c];
                                    break;
                                case 3:
                                    x = values_ac_chrominance[c];
                            }

                            hbuf[hval + c] = x;
                            c--;
                        }
                    }
                }
                images[idx].htsize = 4;
            }

            // Build Huffman table selection table (indexed by mcupos).
            // Get image width.
            if(!images[idx].sof && images[idx].sos) { return images[idx].next_jpeg; }
            int ns = buf[images[idx].sos + 4];
            int nf = buf[images[idx].sof + 9];
            jassert(ns <= 4 && nf <= 4);
            mcusize = 0;  // blocks per MCU
            int hmax = 0;  // MCU horizontal dimension
            for(i = 0; i < ns; ++i) {
                for(int j = 0; j < nf; ++j) {
                    if(buf[images[idx].sos + 2 * i + 5] == buf[images[idx].sof + 3 * j + 10]) { // Cs == C ?
                        int hv = buf[images[idx].sof + 3 * j + 11];  // packed dimensions H x V
                        SamplingFactors[j] = hv;
                        if(hv >> 4 > hmax) { hmax = hv >> 4; }
                        hv = (hv & 15) * (hv >> 4);  // number of blocks in component C
                        jassert(hv >= 1 && hv + mcusize <= 10);
                        while(hv) {
                            jassert(mcusize < 10);
                            hufsel[0][mcusize] = buf[images[idx].sos + 2 * i + 6] >> 4 & 15;
                            hufsel[1][mcusize] = buf[images[idx].sos + 2 * i + 6] & 15;
                            jassert (hufsel[0][mcusize] < 4 && hufsel[1][mcusize] < 4);
                            color[mcusize] = i;
                            int tq = buf[images[idx].sof + 3 * j + 12];  // quantization table index (0..3)
                            jassert(tq >= 0 && tq < 4);
                            images[idx].qmap[mcusize] = tq; // quantizazion table mapping
                            --hv;
                            ++mcusize;
                        }
                    }
                }
            }
            jassert(hmax >= 1 && hmax <= 10);
            int j;
            for(j = 0; j < mcusize; ++j) {
                ls[j] = 0;
                for(int i = 1; i < mcusize; ++i) { if(color[(j + i) % mcusize] == color[j]) { ls[j] = i; }}
                ls[j] = (mcusize - ls[j]) << 6;
            }
            for(j = 0; j < 64; ++j) { zpos[zzu[j] + 8 * zzv[j]] = j; }
            width = buf[images[idx].sof + 7] * 256 + buf[images[idx].sof + 8];  // in pixels
            width = (width - 1) / (hmax * 8) + 1;  // in MCU
            jassert(width > 0);
            mcusize *= 64;  // coefficients per MCU
            row = column = 0;

            // we can have more blocks than components then we have subsampling
            int x = 0, y = 0;
            for(j = 0; j < (mcusize >> 6); j++) {
                int i = color[j];
                int w = SamplingFactors[i] >> 4, h = SamplingFactors[i] & 0xf;
                blockW[j] = x == 0 ? mcusize - 64 * (w - 1) : 64;
                blockN[j] = y == 0 ? mcusize * width - 64 * w * (h - 1) : w * 64;
                x++;
                if(x >= w) {
                    x = 0;
                    y++;
                }
                if(y >= h) {
                    x = 0;
                    y = 0;
                }
            }
        }
    }


    // Decode Huffman
    {
        if(mcusize && buf(1 + (!bpos)) != FF) {  // skip stuffed byte
            jassert(huffbits <= 32);
            huffcode += huffcode + y;
            ++huffbits;
            if(rs < 0) {
                jassert(huffbits >= 1 && huffbits <= 16);
                const int ac = (mcupos & 63) > 0;
                jassert(mcupos >= 0 && (mcupos >> 6) < 10);
                jassert(ac == 0 || ac == 1);
                const int sel = hufsel[ac][mcupos >> 6];
                jassert(sel >= 0 && sel < 4);
                const int i = huffbits - 1;
                jassert(i >= 0 && i < 16);
                const HUF *h = &huf[ac * 64 + sel * 16]; // [ac][sel];
                jassert(h[i].min <= h[i].max && h[i].val < 2048 && huffbits > 0);
                if(huffcode < h[i].max) {
                    jassert(huffcode >= h[i].min);
                    int k = h[i].val + huffcode - h[i].min;
                    jassert(k >= 0 && k < 2048);
                    rs = hbuf[k];
                    huffsize = huffbits;
                }
            }
            if(rs >= 0) {
                if(huffsize + (rs & 15) == huffbits) { // done decoding
                    rs1 = rs;
                    int x = 0;  // decoded extra bits
                    if(mcupos & 63) {  // AC
                        if(rs == 0) { // EOB
                            mcupos = (mcupos + 63) & -64;
                            jassert(mcupos >= 0 && mcupos <= mcusize && mcupos <= 640);
                            while(cpos & 63) {
                                cbuf2[cpos] = 0;
                                cbuf[cpos] = (!rs) ? 0 : (63 - (cpos & 63)) << 4;
                                cpos++;
                                rs++;
                            }
                        } else {  // rs = r zeros + s extra bits for the next nonzero value
                            // If first extra bit is 0 then value is negative.
                            jassert((rs & 15) <= 10);
                            const int r = rs >> 4;
                            const int s = rs & 15;
                            jassert(mcupos >> 6 == (mcupos + r) >> 6);
                            mcupos += r + 1;
                            x = huffcode & ((1 << s) - 1);
                            if(s && !(x >> (s - 1))) { x -= (1 << s) - 1; }
                            for(int i = r; i >= 1; --i) {
                                cbuf2[cpos] = 0;
                                cbuf[cpos++] = i << 4 | s;
                            }
                            cbuf2[cpos] = x;
                            cbuf[cpos++] = (s << 4) | (huffcode << 2 >> s & 3) | 12;
                            ssum += s;
                        }
                    } else {  // DC: rs = 0S, s<12
                        jassert(rs < 12);
                        ++mcupos;
                        x = huffcode & ((1 << rs) - 1);
                        if(rs && !(x >> (rs - 1))) { x -= (1 << rs) - 1; }
                        jassert(mcupos >= 0 && mcupos >> 6 < 10);
                        const int comp = color[mcupos >> 6];
                        jassert(comp >= 0 && comp < 4);
                        dc = pred[comp] += x;
                        jassert((cpos & 63) == 0);
                        cbuf2[cpos] = dc;
                        cbuf[cpos++] = (dc + 1023) >> 3;
                        if((mcupos >> 6) == 0) {
                            ssum1 = 0;
                            ssum2 = ssum3;
                        } else {
                            if(color[(mcupos >> 6) - 1] == color[0]) { ssum1 += (ssum3 = ssum); }
                            ssum2 = ssum1;
                        }
                        ssum = rs;
                    }
                    jassert(mcupos >= 0 && mcupos <= mcusize);
                    if(mcupos >= mcusize) {
                        mcupos = 0;
                        if(++column == width) { column = 0, ++row; }
                    }
                    huffcode = huffsize = huffbits = 0, rs = -1;

                    // UPDATE_ADV_PRED !!!!
                    {
                        const int acomp = mcupos >> 6, q = 64 * images[idx].qmap[acomp];
                        const int zz = mcupos & 63, cpos_dc = cpos - zz;
                        const bool norst = rstpos != column + row * width;
                        if(zz == 0) {
                            for(int i = 0; i < 8; ++i) { sumu[i] = sumv[i] = 0; }
                            // position in the buffer of first (DC) coefficient of the block
                            // of this same component that is to the west of this one, not
                            // necessarily in this MCU
                            int offset_DC_W = cpos_dc - blockW[acomp];
                            // position in the buffer of first (DC) coefficient of the block
                            // of this same component that is to the north of this one, not
                            // necessarily in this MCU
                            int offset_DC_N = cpos_dc - blockN[acomp];
                            for(int i = 0; i < 64; ++i) {
                                sumu[zzu[i]] += (zzv[i] & 1 ? -1 : 1) * (zzv[i] ? 16 * (16 + zzv[i]) : 185) *
                                                (images[idx].qtab[q + i] + 1) * cbuf2[offset_DC_N + i];
                                sumv[zzv[i]] += (zzu[i] & 1 ? -1 : 1) * (zzu[i] ? 16 * (16 + zzu[i]) : 185) *
                                                (images[idx].qtab[q + i] + 1) * cbuf2[offset_DC_W + i];
                            }
                        } else {
                            sumu[zzu[zz - 1]] -=
                                    (zzv[zz - 1] ? 16 * (16 + zzv[zz - 1]) : 185) * (images[idx].qtab[q + zz - 1] + 1) *
                                    cbuf2[cpos - 1];
                            sumv[zzv[zz - 1]] -=
                                    (zzu[zz - 1] ? 16 * (16 + zzu[zz - 1]) : 185) * (images[idx].qtab[q + zz - 1] + 1) *
                                    cbuf2[cpos - 1];
                        }

                        for(int i = 0; i < 3; ++i) {
                            run_pred[i] = run_pred[i + 3] = 0;
                            for(int st = 0; st < 10 && zz + st < 64; ++st) {
                                const int zz2 = zz + st;
                                int p = sumu[zzu[zz2]] * i + sumv[zzv[zz2]] * (2 - i);
                                p /= (images[idx].qtab[q + zz2] + 1) * 185 * (16 + zzv[zz2]) * (16 + zzu[zz2]) / 128;
                                if(zz2 == 0 && (norst || ls[acomp] == 64)) { p -= cbuf2[cpos_dc - ls[acomp]]; }
                                p = (p < 0 ? -1 : +1) * ilog(abs(p) + 1);
                                if(st == 0) {
                                    adv_pred[i] = p;
                                } else if(abs(p) > abs(adv_pred[i]) + 2 && abs(adv_pred[i]) < 210) {
                                    if(run_pred[i] == 0) { run_pred[i] = st * 2 + (p > 0); }
                                    if(abs(p) > abs(adv_pred[i]) + 21 && run_pred[i + 3] == 0) {
                                        run_pred[i + 3] = st * 2 + (p > 0);
                                    }
                                }
                            }
                        }
                        x = 0;
                        for(int i = 0; i < 8; ++i) { x += (zzu[zz] < i) * sumu[i] + (zzv[zz] < i) * sumv[i]; }
                        x = (sumu[zzu[zz]] * (2 + zzu[zz]) + sumv[zzv[zz]] * (2 + zzv[zz]) - x * 2) * 4 /
                            (zzu[zz] + zzv[zz] + 16);
                        x /= (images[idx].qtab[q + zz] + 1) * 185;
                        if(zz == 0 && (norst || ls[acomp] == 64)) { x -= cbuf2[cpos_dc - ls[acomp]]; }
                        adv_pred[3] = (x < 0 ? -1 : +1) * ilog(abs(x) + 1);

                        for(int i = 0; i < 4; ++i) {
                            const int a = (i & 1 ? zzv[zz] : zzu[zz]), b = (i & 2 ? 2 : 1);
                            if(a < b) { x = 65535; }
                            else {
                                const int zz2 = zpos[zzu[zz] + 8 * zzv[zz] - (i & 1 ? 8 : 1) * b];
                                x = (images[idx].qtab[q + zz2] + 1) * cbuf2[cpos_dc + zz2] /
                                    (images[idx].qtab[q + zz] + 1);
                                x = (x < 0 ? -1 : +1) * (ilog(abs(x) + 1) + (x != 0 ? 17 : 0));
                            }
                            lcp[i] = x;
                        }
                        if(zzu[zz] * zzv[zz]) {
                            const int zz2 = zpos[zzu[zz] + 8 * zzv[zz] - 9];
                            x = (images[idx].qtab[q + zz2] + 1) * cbuf2[cpos_dc + zz2] / (images[idx].qtab[q + zz] + 1);
                            lcp[4] = (x < 0 ? -1 : +1) * (ilog(abs(x) + 1) + (x != 0 ? 17 : 0));

                            x = (images[idx].qtab[q + zpos[8 * zzv[zz]]] + 1) * cbuf2[cpos_dc + zpos[8 * zzv[zz]]] /
                                (images[idx].qtab[q + zz] + 1);
                            lcp[5] = (x < 0 ? -1 : +1) * (ilog(abs(x) + 1) + (x != 0 ? 17 : 0));

                            x = (images[idx].qtab[q + zpos[zzu[zz]]] + 1) * cbuf2[cpos_dc + zpos[zzu[zz]]] /
                                (images[idx].qtab[q + zz] + 1);
                            lcp[6] = (x < 0 ? -1 : +1) * (ilog(abs(x) + 1) + (x != 0 ? 17 : 0));
                        } else {
                            lcp[4] = lcp[5] = lcp[6] = 65535;
                        }

                        int prev1 = 0, prev2 = 0, cnt1 = 0, cnt2 = 0, r = 0, s = 0;
                        prev_coef_rs = cbuf[cpos - 64];
                        for(int i = 0; i < acomp; i++) {
                            x = 0;
                            x += cbuf2[cpos - (acomp - i) * 64];
                            if(zz == 0 && (norst || ls[i] == 64)) { x -= cbuf2[cpos_dc - (acomp - i) * 64 - ls[i]]; }
                            if(color[i] == color[acomp] - 1) {
                                prev1 += x;
                                cnt1++;
                                r += cbuf[cpos - (acomp - i) * 64] >> 4;
                                s += cbuf[cpos - (acomp - i) * 64] & 0xF;
                            }
                            if(color[acomp] > 1 && color[i] == color[0]) {
                                prev2 += x;
                                cnt2++;
                            }
                        }
                        if(cnt1 > 0) { prev1 /= cnt1, r /= cnt1, s /= cnt1, prev_coef_rs = (r << 4) | s; }
                        if(cnt2 > 0) { prev2 /= cnt2; }
                        prev_coef = (prev1 < 0 ? -1 : +1) * ilog(11 * abs(prev1) + 1) + (cnt1 << 20);
                        prev_coef2 = (prev2 < 0 ? -1 : +1) * ilog(11 * abs(prev2) + 1);

                        if(column == 0 && blockW[acomp] > 64 * acomp) {
                            run_pred[1] = run_pred[2], run_pred[0] = 0, adv_pred[1] = adv_pred[2], adv_pred[0] = 0;
                        }
                        if(row == 0 && blockN[acomp] > 64 * acomp) {
                            run_pred[1] = run_pred[0], run_pred[2] = 0, adv_pred[1] = adv_pred[0], adv_pred[2] = 0;
                        }
                    } // !!!!

                }
            }
        }
    }

    // Estimate next bit probability
    if(!images[idx].jpeg || !images[idx].data) { return images[idx].next_jpeg; }
    if(buf(1 + (!bpos)) == FF) {
        m.add(128);
        m.set(0, 9);
        m.set(0, 1025);
        m.set(buf(1), 1024);
        return 1;
    }
    if(rstlen > 0 && rstlen == column + row * width - rstpos && mcupos == 0 && (int) huffcode == (1 << huffbits) - 1) {
        m.add(4095);
        m.set(0, 9);
        m.set(0, 1025);
        m.set(buf(1), 1024);
        return 1;
    }

    // Context model
    const int N = 32; // size of t, number of contexts
    static BH<9> t(MEM);  // context hash -> bit history
    // As a cache optimization, the context does not include the last 1-2
    // bits of huffcode if the length (huffbits) is not a multiple of 3.
    // The 7 mapped values are for context+{"", 0, 00, 01, 1, 10, 11}.
    static Array<U32> cxt(N);  // context hashes
    static Array<U8 *> cp(N);  // context pointers
    static StateMap sm[N];
    static Mixer m1(32, 2050, 3);
    static APM a1(0x8000), a2(0x20000);


    // Update model
    if(cp[N - 1]) {
        for(int i = 0; i < N; ++i) {
            *cp[i] = nex(*cp[i], y);
        }
    }
    m1.update();

    // Update context
    const int comp = color[mcupos >> 6];
    const int coef = (mcupos & 63) | comp << 6;
    const int hc = (huffcode * 4 + ((mcupos & 63) == 0) * 2 + (comp == 0)) | 1 << (huffbits + 2);
    const bool firstcol = column == 0 && blockW[mcupos >> 6] > mcupos;
    static int hbcount = 2;
    if(++hbcount > 2 || huffbits == 0) { hbcount = 0; }
    jassert(coef >= 0 && coef < 256);
    const int zu = zzu[mcupos & 63], zv = zzv[mcupos & 63];
    if(hbcount == 0) {
        int n = hc * 32;
        cxt[0] = hash(++n, coef, adv_pred[2] / 12 + (run_pred[2] << 8), ssum2 >> 6, prev_coef / 72);
        cxt[1] = hash(++n, coef, adv_pred[0] / 12 + (run_pred[0] << 8), ssum2 >> 6, prev_coef / 72);
        cxt[2] = hash(++n, coef, adv_pred[1] / 11 + (run_pred[1] << 8), ssum2 >> 6);
        cxt[3] = hash(++n, rs1, adv_pred[2] / 7, run_pred[5] / 2, prev_coef / 10);
        cxt[4] = hash(++n, rs1, adv_pred[0] / 7, run_pred[3] / 2, prev_coef / 10);
        cxt[5] = hash(++n, rs1, adv_pred[1] / 11, run_pred[4]);
        cxt[6] = hash(++n, adv_pred[2] / 14, run_pred[2], adv_pred[0] / 14, run_pred[0]);
        cxt[7] = hash(++n, cbuf[cpos - blockN[mcupos >> 6]] >> 4, adv_pred[3] / 17, run_pred[1], run_pred[5]);
        cxt[8] = hash(++n, cbuf[cpos - blockW[mcupos >> 6]] >> 4, adv_pred[3] / 17, run_pred[1], run_pred[3]);
        cxt[9] = hash(++n, lcp[0] / 22, lcp[1] / 22, adv_pred[1] / 7, run_pred[1]);
        cxt[10] = hash(++n, lcp[0] / 22, lcp[1] / 22, mcupos & 63, lcp[4] / 30);
        cxt[11] = hash(++n, zu / 2, lcp[0] / 13, lcp[2] / 30, prev_coef / 40 + ((prev_coef2 / 28) << 20));
        cxt[12] = hash(++n, zv / 2, lcp[1] / 13, lcp[3] / 30, prev_coef / 40 + ((prev_coef2 / 28) << 20));
        cxt[13] = hash(++n, rs1, prev_coef / 42, prev_coef2 / 34,
                       hash(lcp[0] / 60, lcp[2] / 14, lcp[1] / 60, lcp[3] / 14));
        cxt[14] = hash(++n, mcupos & 63, column >> 1);
        cxt[15] = hash(++n, column >> 3, min(5 + 2 * (!comp), zu + zv),
                       hash(lcp[0] / 10, lcp[2] / 40, lcp[1] / 10, lcp[3] / 40));
        cxt[16] = hash(++n, ssum >> 3, mcupos & 63);
        cxt[17] = hash(++n, rs1, mcupos & 63, run_pred[1]);
        cxt[18] = hash(++n, coef, ssum2 >> 5, adv_pred[3] / 30,
                       (comp) ? hash(prev_coef / 22, prev_coef2 / 50) : ssum / ((mcupos & 0x3F) + 1));
        cxt[19] = hash(++n, lcp[0] / 40, lcp[1] / 40, adv_pred[1] / 28,
                       hash((comp) ? prev_coef / 40 + ((prev_coef2 / 40) << 20) : lcp[4] / 22, min(7, zu + zv),
                            ssum / (2 * (zu + zv) + 1)));
        cxt[20] = hash(++n, zv, cbuf[cpos - blockN[mcupos >> 6]], adv_pred[2] / 28, run_pred[2]);
        cxt[21] = hash(++n, zu, cbuf[cpos - blockW[mcupos >> 6]], adv_pred[0] / 28, run_pred[0]);
        cxt[22] = hash(++n, adv_pred[2] / 7, run_pred[2]);
        cxt[23] = hash(n, adv_pred[0] / 7, run_pred[0]);
        cxt[24] = hash(n, adv_pred[1] / 7, run_pred[1]);
        cxt[25] = hash(++n, zv, lcp[1] / 14, adv_pred[2] / 16, run_pred[5]);
        cxt[26] = hash(++n, zu, lcp[0] / 14, adv_pred[0] / 16, run_pred[3]);
        cxt[27] = hash(++n, lcp[0] / 14, lcp[1] / 14, adv_pred[3] / 16);
        cxt[28] = hash(++n, coef, prev_coef / 10, prev_coef2 / 20);
        cxt[29] = hash(++n, coef, ssum >> 2, prev_coef_rs);
        cxt[30] = hash(++n, coef, adv_pred[1] / 17, hash(lcp[(zu < zv)] / 24, lcp[2] / 20, lcp[3] / 24));
        cxt[31] = hash(++n, coef, adv_pred[3] / 11,
                       hash(lcp[(zu < zv)] / 50, lcp[2 + 3 * (zu * zv > 1)] / 50, lcp[3 + 3 * (zu * zv > 1)] / 50));
    }

    // Predict next bit
    m1.add(128);
    assert(hbcount <= 2);
    int p;
    switch(hbcount) {
        case 0:
            for(int i = 0; i < N; ++i) {
                cp[i] = t[cxt[i]] + 1, m1.add(p = stretch(sm[i].p(*cp[i])));
                if(level > 7) { m.add(p >> 2); }
            }
            break;
        case 1: {
            int hc = 1 + (huffcode & 1) * 3;
            for(int i = 0; i < N; ++i) {
                cp[i] += hc, m1.add(p = stretch(sm[i].p(*cp[i])));
                if(level > 7) { m.add(p >> 2); }
            }
        }
            break;
        default: {
            int hc = 1 + (huffcode & 1);
            for(int i = 0; i < N; ++i) {
                cp[i] += hc, m1.add(p = stretch(sm[i].p(*cp[i])));
                if(level > 7) { m.add(p >> 2); }
            }
        }
            break;
    }

    m1.set(firstcol, 2);
    m1.set(coef + 256 * min(3, huffbits), 1024);
    m1.set((hc & 0x1FE) * 2 + min(3, ilog2(zu + zv)), 1024);
    int pr = m1.p();
    m.add(stretch(pr) >> 2);
    m.add((pr >> 4) - (255 - ((pr >> 4))));
    pr = a1.p(pr, (hc & 511) | (((adv_pred[1] / 16) & 63) << 9), 1023);
    m.add(stretch(pr) >> 2);
    m.add((pr >> 4) - (255 - ((pr >> 4))));
    pr = a2.p(pr, (hc & 511) | (coef << 9), 1023);
    m.add(stretch(pr) >> 2);
    m.add((pr >> 4) - (255 - ((pr >> 4))));
    m.set(1 + (zu + zv < 5) + (huffbits > 8) * 2 + firstcol * 4, 9);
    m.set(1 + (hc & 0xFF) + 256 * min(3, (zu + zv) / 3), 1025);
    m.set(coef + 256 * min(3, huffbits / 2), 1024);
    return 1;

}

#endif //PAQ8PX_JPEGMODEL_H
