#ifndef SK_BIGVERB_H
#define SK_BIGVERB_H

#ifndef SKFLT
#define SKFLT float
#endif
typedef struct sk_bigverb sk_bigverb;
typedef struct sk_bigverb_delay sk_bigverb_delay;
sk_bigverb * sk_bigverb_new(int sr);
void sk_bigverb_del(sk_bigverb *bv);
void sk_bigverb_size(sk_bigverb *bv, SKFLT size);
void sk_bigverb_cutoff(sk_bigverb *bv, SKFLT cutoff);
void sk_bigverb_tick(sk_bigverb *bv,
                     SKFLT inL, SKFLT inR,
                     SKFLT *outL, SKFLT *outR);

#ifdef SK_BIGVERB_PRIV
struct sk_bigverb_delay {
SKFLT *buf;
size_t sz;
int wpos;
int irpos;
int frpos;
int rng;
int inc;
int counter;
int maxcount;
SKFLT dels;
SKFLT drift;
SKFLT y;
};
struct sk_bigverb {
    int sr;
SKFLT size;
SKFLT cutoff;
SKFLT pcutoff;
SKFLT filt;
SKFLT *buf;
sk_bigverb_delay delay[8];
};
#endif
#endif
