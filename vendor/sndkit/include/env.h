#ifndef SK_ENV_H
#define SK_ENV_H
#ifndef SKFLT
#define SKFLT float
#endif

typedef struct sk_env sk_env;
void sk_env_init(sk_env *env, int sr);
void sk_env_attack(sk_env *env, SKFLT atk);
void sk_env_release(sk_env *env, SKFLT rel);
void sk_env_hold(sk_env *env, SKFLT hold);
SKFLT sk_env_tick(sk_env *env, SKFLT trig);

#ifdef SK_ENV_PRIV
struct sk_env {
    int sr;
float timer;
float inc;
SKFLT atk_env;
SKFLT rel_env;
int mode;
SKFLT prev;
SKFLT atk;
SKFLT patk;
SKFLT rel;
SKFLT prel;
SKFLT hold;
SKFLT phold;
};
#endif

#endif
