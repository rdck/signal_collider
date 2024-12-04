#include <math.h>
#define SK_ENV_PRIV
#include "env.h"
enum {
    MODE_ZERO,
    MODE_ATTACK,
    MODE_HOLD,
    MODE_RELEASE
};
#define EPS 5e-8
void sk_env_init(sk_env *env, int sr)
{
    env->sr = sr;
env->timer = 0;
env->inc = 0;
env->atk_env = 0;
env->rel_env = 0;
env->mode = MODE_ZERO;
env->prev = 0;
sk_env_attack(env, 0.1);
env->patk = -1;
sk_env_release(env, 0.1);
env->prel= -1;
sk_env_hold(env, 0.1);
env->phold = -1;
}
void sk_env_attack(sk_env *env, SKFLT atk)
{
    env->atk = atk;
}
void sk_env_release(sk_env *env, SKFLT rel)
{
    env->rel= rel;
}
void sk_env_hold(sk_env *env, SKFLT hold)
{
    env->hold = hold;
}
SKFLT sk_env_tick(sk_env *env, SKFLT trig)
{
    SKFLT out;
    out = 0;

if (trig != 0) {
    env->mode = MODE_ATTACK;

    if (env->patk != env->atk) {
        env->patk = env->atk;
        env->atk_env = exp(-1.0 / (env->atk * env->sr));
    }
}

    switch (env->mode) {
case MODE_ZERO:
    break;
case MODE_ATTACK: {
    out = env->atk_env*env->prev + (1.0 - env->atk_env);

    if ((out - env->prev) <= EPS) {
        env->mode = MODE_HOLD;
        env->timer = 0;

        if (env->phold != env->hold) {
            if (env->hold <= 0) {
                env->inc = 1.0;
            } else {
                env->phold = env->hold;
                env->inc = 1.0 / (env->hold * env->sr);
            }
        }
    }

    env->prev = out;
    break;
}
case MODE_HOLD: {
    out = env->prev;
    env->timer += env->inc;

    if (env->timer >= 1.0) {
        env->mode = MODE_RELEASE;

        if (env->prel != env->rel) {
            env->prel = env->rel;
            env->rel_env = exp(-1 / (env->rel * env->sr));
        }
    }
    break;
}
case MODE_RELEASE: {
    out = env->rel_env*env->prev;
    env->prev = out;

    if (out <= EPS) {
       env->mode = MODE_ZERO;
    }
    break;
}
        default:
            break;
    }
    return out;
}
