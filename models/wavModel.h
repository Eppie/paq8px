#ifndef PAQ8PX_WAVMODEL_H
#define PAQ8PX_WAVMODEL_H

// Model a 16/8-bit stereo/mono uncompressed .wav file.
// Based on 'An asymptotically Optimal Predictor for Stereo Lossless Audio Compression'
// by Florin Ghido.

static int S, D;
static int wmode;

inline int s2(int i) { return int(short(buf(i) + 256 * buf(i - 1))); }

inline int t2(int i) { return int(short(buf(i - 1) + 256 * buf(i))); }

inline int X1(int i) {
    switch(wmode) {
        case 0:
            return buf(i) - 128;
        case 1:
            return buf(i << 1) - 128;
        case 2:
            return s2(i << 1);
        case 3:
            return s2(i << 2);
        case 4:
            return (buf(i) ^ 128) - 128;
        case 5:
            return (buf(i << 1) ^ 128) - 128;
        case 6:
            return t2(i << 1);
        case 7:
            return t2(i << 2);
        default:
            return 0;
    }
}

inline int X2(int i) {
    switch(wmode) {
        case 0:
            return buf(i + S) - 128;
        case 1:
            return buf((i << 1) - 1) - 128;
        case 2:
            return s2((i + S) << 1);
        case 3:
            return s2((i << 2) - 2);
        case 4:
            return (buf(i + S) ^ 128) - 128;
        case 5:
            return (buf((i << 1) - 1) ^ 128) - 128;
        case 6:
            return t2((i + S) << 1);
        case 7:
            return t2((i << 2) - 2);
        default:
            return 0;
    }
}

void wavModel(Mixer &m, int info) {
    static int pr[3][2], n[2], counter[2];
    static double F[49][49][2], L[49][49];
    int j, k, l, i = 0;
    long double sum;
    const double a = 0.996, a2 = 1 / a;
    const int SC = 0x20000;
    static SmallStationaryContextMap scm1(SC), scm2(SC), scm3(SC), scm4(SC), scm5(SC), scm6(SC), scm7(SC);
    static ContextMap cm(MEM * 4, 10 + 1);
    static int bits, channels, w, ch;
    static int z1, z2, z3, z4, z5, z6, z7;

    if(!bpos && !blpos) {
        bits = ((info % 4) / 2) * 8 + 8;
        channels = info % 2 + 1;
        w = channels * (bits >> 3);
        wmode = info;
        if(channels == 1) { S = 48, D = 0; } else { S = 36, D = 12; }
        for(int j = 0; j < channels; j++) {
            for(k = 0; k <= S + D; k++) { for(l = 0; l <= S + D; l++) { F[k][l][j] = 0, L[k][l] = 0; }}
            F[1][0][j] = 1;
            n[j] = counter[j] = pr[2][j] = pr[1][j] = pr[0][j] = 0;
            z1 = z2 = z3 = z4 = z5 = z6 = z7 = 0;
        }
    }
    // Select previous samples and predicted sample as context
    if(!bpos && blpos >= w) {
        /*const int*/ ch = blpos % w;
        const int msb = ch % (bits >> 3);
        const int chn = ch / (bits >> 3);
        if(!msb) {
            z1 = X1(1), z2 = X1(2), z3 = X1(3), z4 = X1(4), z5 = X1(5);
            k = X1(1);
            for(l = 0; l <= min(S, counter[chn] - 1); l++) {
                F[0][l][chn] *= a;
                F[0][l][chn] += X1(l + 1) * k;
            }
            for(l = 1; l <= min(D, counter[chn]); l++) {
                F[0][l + S][chn] *= a;
                F[0][l + S][chn] += X2(l + 1) * k;
            }
            if(channels == 2) {
                k = X2(2);
                for(l = 1; l <= min(D, counter[chn]); l++) {
                    F[S + 1][l + S][chn] *= a;
                    F[S + 1][l + S][chn] += X2(l + 1) * k;
                }
                for(l = 1; l <= min(S, counter[chn] - 1); l++) {
                    F[l][S + 1][chn] *= a;
                    F[l][S + 1][chn] += X1(l + 1) * k;
                }
                z6 = X2(1) + X1(1) - X2(2), z7 = X2(1);
            } else { z6 = 2 * X1(1) - X1(2), z7 = X1(1); }
            if(++n[chn] == (256 >> level)) {
                if(channels == 1) {
                    for(k = 1; k <= S + D; k++) {
                        for(l = k; l <= S + D; l++) {
                            F[k][l][chn] = (F[k - 1][l - 1][chn] - X1(k) * X1(l)) * a2;
                        }
                    }
                } else {
                    for(k = 1; k <= S + D; k++) {
                        if(k != S + 1) {
                            for(l = k; l <= S + D; l++) {
                                if(l != S + 1) {
                                    F[k][l][chn] = (F[k - 1][l - 1][chn] - (k - 1 <= S ? X1(k) : X2(k - S)) *
                                                                           (l - 1 <= S ? X1(l) : X2(l - S))) * a2;
                                }
                            }
                        }
                    }
                }
                for(i = 1; i <= S + D; i++) {
                    sum = F[i][i][chn];
                    for(k = 1; k < i; k++) { sum -= L[i][k] * L[i][k]; }
                    sum = floor(sum + 0.5);
                    sum = 1 / sum;
                    if(sum > 0) {
                        L[i][i] = sqrt(sum);
                        for(j = (i + 1); j <= S + D; j++) {
                            sum = F[i][j][chn];
                            for(k = 1; k < i; k++) { sum -= L[j][k] * L[i][k]; }
                            sum = floor(sum + 0.5);
                            L[j][i] = sum * L[i][i];
                        }
                    } else { break; }
                }
                if(i > S + D && counter[chn] > S + 1) {
                    for(k = 1; k <= S + D; k++) {
                        F[k][0][chn] = F[0][k][chn];
                        for(j = 1; j < k; j++) { F[k][0][chn] -= L[k][j] * F[j][0][chn]; }
                        F[k][0][chn] *= L[k][k];
                    }
                    for(k = S + D; k > 0; k--) {
                        for(j = k + 1; j <= S + D; j++) { F[k][0][chn] -= L[j][k] * F[j][0][chn]; }
                        F[k][0][chn] *= L[k][k];
                    }
                }
                n[chn] = 0;
            }
            sum = 0;
            for(l = 1; l <= S + D; l++) { sum += F[l][0][chn] * (l <= S ? X1(l) : X2(l - S)); }
            pr[2][chn] = pr[1][chn];
            pr[1][chn] = pr[0][chn];
            pr[0][chn] = int(floor(sum));
            counter[chn]++;
        }
        const int y1 = pr[0][chn], y2 = pr[1][chn], y3 = pr[2][chn];
        int x1 = buf(1), x2 = buf(2), x3 = buf(3);
        if(wmode == 4 || wmode == 5) { x1 ^= 128, x2 ^= 128; }
        if(bits == 8) { x1 -= 128, x2 -= 128; }
        const int t = ((bits == 8) || ((!msb) ^ (wmode < 6)));
        i = ch << 4;
        if((msb) ^ (wmode < 6)) {
            cm.set(hash(++i, y1 & 0xff));
            cm.set(hash(++i, y1 & 0xff, ((z1 - y2 + z2 - y3) >> 1) & 0xff));
            cm.set(hash(++i, x1, y1 & 0xff));
            cm.set(hash(++i, x1, x2 >> 3, x3));
            if(bits == 8) {
                cm.set(hash(++i, y1 & 0xFE, ilog2(abs((int) (z1 - y2))) * 2 + (z1 > y2)));
            } else {
                cm.set(hash(++i, (y1 + z1 - y2) & 0xff));
            }
            cm.set(hash(++i, x1));
            cm.set(hash(++i, x1, x2));
            cm.set(hash(++i, z1 & 0xff));
            cm.set(hash(++i, (z1 * 2 - z2) & 0xff));
            cm.set(hash(++i, z6 & 0xff));
            cm.set(hash(++i, y1 & 0xFF, ((z1 - y2 + z2 - y3) / (bits >> 3)) & 0xFF));
        } else {
            cm.set(hash(++i, (y1 - x1 + z1 - y2) >> 8));
            cm.set(hash(++i, (y1 - x1) >> 8));
            cm.set(hash(++i, (y1 - x1 + z1 * 2 - y2 * 2 - z2 + y3) >> 8));
            cm.set(hash(++i, (y1 - x1) >> 8, (z1 - y2 + z2 - y3) >> 9));
            cm.set(hash(++i, z1 >> 12));
            cm.set(hash(++i, x1));
            cm.set(hash(++i, x1 >> 7, x2, x3 >> 7));
            cm.set(hash(++i, z1 >> 8));
            cm.set(hash(++i, (z1 * 2 - z2) >> 8));
            cm.set(hash(++i, y1 >> 8));
            cm.set(hash(++i, (y1 - x1) >> 6));
        }
        scm1.set(t * ch);
        scm2.set(t * ((z1 - x1 + y1) >> 9) & 0xff);
        scm3.set(t * ((z1 * 2 - z2 - x1 + y1) >> 8) & 0xff);
        scm4.set(t * ((z1 * 3 - z2 * 3 + z3 - x1) >> 7) & 0xff);
        scm5.set(t * ((z1 + z7 - x1 + y1 * 2) >> 10) & 0xff);
        scm6.set(t * ((z1 * 4 - z2 * 6 + z3 * 4 - z4 - x1) >> 7) & 0xff);
        scm7.set(t * ((z1 * 5 - z2 * 10 + z3 * 10 - z4 * 5 + z5 - x1 + y1) >> 9) & 0xff);
    }

    // Predict next bit
    scm1.mix(m);
    scm2.mix(m);
    scm3.mix(m);
    scm4.mix(m);
    scm5.mix(m);
    scm6.mix(m);
    scm7.mix(m);
    cm.mix(m);
    if(level >= 4) { recordModel(m); }
    static int col = 0;
    if(++col >= w * 8) { col = 0; }
    //m.set(3, 8);
    m.set(ch + 4 * ilog2(col & (bits - 1)), 4 * 8);
    m.set(col % bits < 8, 2);
    m.set(col % bits, bits);
    m.set(col, w * 8);
    m.set(c0, 256);
}

#endif //PAQ8PX_WAVMODEL_H
