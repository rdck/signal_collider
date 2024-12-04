#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define SK_BIGVERB_PRIV
#include "bigverb.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
struct bigverb_paramset {
    int delay; /* in samples, 44.1 kHz */
    int drift; /* 1/10 milliseconds */
    int randfreq; /* Hertz * 1000 */
    int seed;
};

static const struct bigverb_paramset params[8] = {
    {0x09a9, 0x0a, 0xc1c, 0x07ae},
    {0x0acf, 0x0b, 0xdac, 0x7333},
    {0x0c91, 0x11, 0x456, 0x5999},
    {0x0de5, 0x06, 0xf85, 0x2666},
    {0x0f43, 0x0a, 0x925, 0x50a3},
    {0x101f, 0x0b, 0x769, 0x5999},
    {0x085f, 0x11, 0x37b, 0x7333},
    {0x078d, 0x06, 0xc95, 0x3851}
};
#define FRACSCALE 0x10000000
#define FRACMASK 0xFFFFFFF
#define FRACNBITS 28
static int get_delay_size(const struct bigverb_paramset *p, int sr);
static void delay_init(sk_bigverb_delay *d,
                       const struct bigverb_paramset *p,
                       SKFLT *buf,
                       size_t sz,
                       int sr);
static SKFLT delay_compute(sk_bigverb_delay *d,
                           SKFLT in,
                           SKFLT fdbk,
                           SKFLT filt,
                           int sr);
static void generate_next_line(sk_bigverb_delay *d, int sr);
sk_bigverb * sk_bigverb_new(int sr)
{
    sk_bigverb *bv;

    bv = calloc(1, sizeof(sk_bigverb));

    bv->sr = sr;
sk_bigverb_size(bv, 0.93);
sk_bigverb_cutoff(bv, 10000.0);
bv->pcutoff = -1;
bv->filt = 1.0;
bv->buf = NULL;
{
unsigned long total_size;
int i;
SKFLT *buf;

total_size = 0;
buf = NULL;
for (i = 0; i < 8; i++) {
    total_size += get_delay_size(&params[i], sr);
}
buf = calloc(1, sizeof(SKFLT) * total_size);
bv->buf = buf;
{
    unsigned long bufpos;
    bufpos = 0;
    for (i = 0; i < 8; i++) {
        unsigned int sz;
        sz = get_delay_size(&params[i], sr);

        delay_init(&bv->delay[i], &params[i],
                   &buf[bufpos], sz, sr);
        bufpos += sz;
    }
}
}

    return bv;
}
void sk_bigverb_del(sk_bigverb *bv)
{
free(bv->buf);
    free(bv);
    bv = NULL;
}
void sk_bigverb_size(sk_bigverb *bv, SKFLT size)
{
    bv->size = size;
}
void sk_bigverb_cutoff(sk_bigverb *bv, SKFLT cutoff)
{
    bv->cutoff = cutoff;
}
void sk_bigverb_tick(sk_bigverb *bv,
                     SKFLT inL, SKFLT inR,
                     SKFLT *outL, SKFLT *outR)
{
    SKFLT lsum, rsum;

    lsum = 0;
    rsum = 0;

if (bv->pcutoff != bv->cutoff) {
    bv->pcutoff = bv->cutoff;
    bv->filt = 2.0 - cos(bv->pcutoff * 2 * M_PI / bv->sr);
    bv->filt = bv->filt - sqrt(bv->filt * bv->filt - 1.0);
}
{
    int i;
    SKFLT jp;

    jp = 0;

    for (i = 0; i < 8; i++) {
        jp += bv->delay[i].y;
    }

    jp *= 0.25;

    inL = jp + inL;
    inR = jp + inR;
}
{
    int i;
    for (i = 0; i < 8; i++) {
        if (i & 1) {
            rsum += delay_compute(&bv->delay[i],
                                  inR,
                                  bv->size,
                                  bv->filt,
                                  bv->sr);
        } else {
            lsum += delay_compute(&bv->delay[i],
                                  inL,
                                  bv->size,
                                  bv->filt,
                                  bv->sr);
        }
    }
}
rsum *= 0.35f;
lsum *= 0.35f;

    *outL = lsum;
    *outR = rsum;
}
static int get_delay_size(const struct bigverb_paramset *p, int sr)
{
    SKFLT sz;
    sz = (SKFLT)p->delay/44100 + (p->drift * 0.0001) * 1.125;
    return floor(16 + sz*sr);
}
static void delay_init(sk_bigverb_delay *d,
                       const struct bigverb_paramset *p,
                       SKFLT *buf,
                       size_t sz,
                       int sr)
{
    SKFLT readpos;
d->buf = buf;
d->sz = sz;
d->wpos = 0;
d->rng = p->seed;
readpos = ((SKFLT)p->delay / 44100);
readpos += d->rng * (p->drift * 0.0001) / 32768.0;
readpos = sz - (readpos * sr);
d->irpos = floor(readpos);
d->frpos = floor((readpos - d->irpos) * FRACSCALE);
d->inc = 0;
d->counter = 0;
d->maxcount = floor((sr / ((SKFLT)p->randfreq * 0.001)));
d->dels = p->delay / 44100.0;
d->drift = p->drift;
generate_next_line(d, sr);
d->y = 0.0;
}
static SKFLT delay_compute(sk_bigverb_delay *del,
                           SKFLT in,
                           SKFLT fdbk,
                           SKFLT filt,
                           int sr)
{
    SKFLT out;
    SKFLT frac_norm;
    SKFLT a, b, c, d;
    SKFLT s[4];
    out = 0;
del->buf[del->wpos] = in - del->y;
del->wpos++;
if (del->wpos >= del->sz) del->wpos -= del->sz;
if (del->frpos >= FRACSCALE) {
    del->irpos += del->frpos >> FRACNBITS;
    del->frpos &= FRACMASK;
}
if (del->irpos >= del->sz) del->irpos -= del->sz;
frac_norm = del->frpos / (SKFLT)FRACSCALE;
{
    SKFLT tmp[2];
    d = ((frac_norm * frac_norm) - 1) / 6.0;
    tmp[0] = ((frac_norm + 1.0) * 0.5);
    tmp[1] = 3.0 * d;
    a = tmp[0] - 1.0 - d;
    c = tmp[0] - tmp[1];
    b = tmp[1] - frac_norm;
}
{
    int n;
    SKFLT *x;
    n = del->irpos;
    x = del->buf;

    if (n > 0 && n < (del->sz - 2)) {
        s[0] = x[n - 1];
        s[1] = x[n];
        s[2] = x[n + 1];
        s[3] = x[n + 2];
    } else {
        int k;
        n--;
        if (n < 0) n += del->sz;
        s[0] = x[n];
        for (k = 0; k < 3; k++) {
            n++;
            if (n >= del->sz) n -= del->sz;
            s[k + 1] = x[n];
        }
    }
}
out = (a*s[0] + b*s[1] + c*s[2] + d*s[3]) * frac_norm + s[1];
del->frpos += del->inc;
out *= fdbk;
out += (del->y - out) * filt;
del->y = out;
del->counter--;
if (del->counter <= 0) {
    generate_next_line(del, sr);
}
    return out;
}
static void generate_next_line(sk_bigverb_delay *d, int sr)
{
    SKFLT curdel;
    SKFLT nxtdel;
    SKFLT inc;
if (d->rng < 0) d->rng += 0x10000;
/* 5^6 = 15625 */
d->rng = (1 + d->rng * 0x3d09);
d->rng &= 0xFFFF;
if (d->rng >= 0x8000) d->rng -= 0x10000;
d->counter = d->maxcount;
curdel = d->wpos -
    (d->irpos + (d->frpos/(SKFLT)FRACSCALE));
while (curdel < 0) curdel += d->sz;
curdel /= sr;
nxtdel = (d->rng * (d->drift * 0.0001) / 32768.0) + d->dels;
inc = ((curdel - nxtdel) / (SKFLT)d->counter)*sr;
inc += 1;
d->inc = floor(inc * FRACSCALE);
}
