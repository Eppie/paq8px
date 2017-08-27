#ifndef PAQ8PX_MIXER_H
#define PAQ8PX_MIXER_H

#include "Array.h"
#include "Stretch.h"

//////////////////////////// Mixer /////////////////////////////

// Mixer m(N, M, S=1, w=0) combines models using M neural networks with
//   N inputs each, of which up to S may be selected.  If S > 1 then
//   the outputs of these neural networks are combined using another
//   neural network (with parameters S, 1, 1).  If S = 1 then the
//   output is direct.  The weights are initially w (+-32K).
//   It is used as follows:
// m.update() trains the network where the expected output is the
//   last bit (in the global variable y).
// m.add(stretch(p)) inputs prediction from one of N models.  The
//   prediction should be positive to predict a 1 bit, negative for 0,
//   nominally +-256 to +-2K.  The maximum allowed value is +-32K but
//   using such large values may cause overflow if N is large.
// m.set(cxt, range) selects cxt as one of 'range' neural networks to
//   use.  0 <= cxt < range.  Should be called up to S times such
//   that the total of the ranges is <= M.
// m.p() returns the output prediction that the next bit is 1 as a
//   12 bit number (0 to 4095).


#if !defined(__GNUC__)
#if (2 == _M_IX86_FP)
# define __SSE2__
#endif
#endif
#if defined(__SSE2__)

#include <emmintrin.h>
#include <cassert>

static int dot_product(const short *const t, const short *const w, int n) {
    __m128i sum = _mm_setzero_si128();
    while((n -= 8) >= 0) {
        __m128i tmp = _mm_madd_epi16(*(__m128i *) &t[n], *(__m128i *) &w[n]);
        tmp = _mm_srai_epi32(tmp, 8);
        sum = _mm_add_epi32(sum, tmp);
    }
    sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
    sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));
    return _mm_cvtsi128_si32(sum);
}

static void train(const short *const t, short *const w, int n, const int e) {
    if(e) {
        const __m128i one = _mm_set1_epi16(1);
        const __m128i err = _mm_set1_epi16(short(e));
        while((n -= 8) >= 0) {
            __m128i tmp = _mm_adds_epi16(*(__m128i *) &t[n], *(__m128i *) &t[n]);
            tmp = _mm_mulhi_epi16(tmp, err);
            tmp = _mm_adds_epi16(tmp, one);
            tmp = _mm_srai_epi16(tmp, 1);
            tmp = _mm_adds_epi16(tmp, *(__m128i *) &w[n]);
            *(__m128i *) &w[n] = tmp;
        }
    }
}

#else

static int dot_product (const short* const t, const short* const w, int n) {
  int sum = 0;
  while ((n -= 2) >= 0) {
    sum += (t[n] * w[n] + t[n + 1] * w[n + 1]) >> 8;
  }
  return sum;
}

static void train (const short* const t, short* const w, int n, const int err) {
  if (err) {
    while ((n -= 1) >= 0) {
      int wt = w[n] + ((((t[n] * err * 2) >> 16) + 1) >> 1);
      if (wt < -32768) {
        w[n] = -32768;
      } else if (wt > 32767) {
        w[n] = 32767;
      } else {
        w[n] = wt;
      }
    }
  }
}
#endif

class Mixer {
    const int N, M, S;   // max inputs, max contexts, max context sets
    Array<short> tx; // N inputs from add()
    Array<short> wx; // N*M weights
    Array<int> cxt;  // S contexts
    int ncxt;        // number of contexts (0 to S)
    int base;        // offset of next context
    int nx;          // Number of inputs in tx, 0 to N
    Array<int> pr;   // last result (scaled 12 bits)
    Mixer *mp;       // points to a Mixer to combine results
public:
    Mixer(int n, int m, int s = 1, int w = 0);

    // Adjust weights to minimize coding cost of last prediction
    void update() {
        for(int i = 0; i < ncxt; ++i) {
            int err = ((y << 12) - pr[i]) * 7;
            assert(err >= -32768 && err < 32768);
            train(&tx[0], &wx[cxt[i] * N], nx, err);
        }
        nx = base = ncxt = 0;
    }

    // Input x (call up to N times)
    void add(int x) {
        assert(nx < N);
        tx[nx++] = x;
    }

    // Set a context (call S times, sum of ranges <= M)
    void set(int cx, int range) {
        assert(range >= 0);
        assert(ncxt < S);
        assert(cx >= 0);
        assert(base + cx < M);
        cxt[ncxt++] = base + cx;
        base += range;
    }

    // predict next bit
    int p() {
        while(nx & 7) { tx[nx++] = 0; }  // pad
        if(mp) {  // combine outputs
            mp->update();
            for(int i = 0; i < ncxt; ++i) {
                pr[i] = squash(dot_product(&tx[0], &wx[cxt[i] * N], nx) >> 5);
                mp->add(stretch(pr[i]));
            }
            mp->set(0, 1);
            return mp->p();
        } else {  // S=1 context
            return pr[0] = squash(dot_product(&tx[0], &wx[0], nx) >> 8);
        }
    }

    ~Mixer();
};

Mixer::~Mixer() {
    delete mp;
}


Mixer::Mixer(int n, int m, int s, int w) :
        N((n + 7) & -8), M(m), S(s), tx(N), wx(N * M),
        cxt(S), ncxt(0), base(0), nx(0), pr(S), mp(0) {
    assert(n > 0 && N > 0 && (N & 7) == 0 && M > 0);
    int i;
    for(i = 0; i < S; ++i) {
        pr[i] = 2048;
    }
    for(i = 0; i < N * M; ++i) {
        wx[i] = w;
    }
    if(S > 1) { mp = new Mixer(S, 1, 1); }
}


#endif //PAQ8PX_MIXER_H
