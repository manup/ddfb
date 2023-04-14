#ifndef U_MATH_H
#define U_MATH_H

#define U_PI 3.14159265358979323846
#define U_PI2 (2.0 * U_PI)

#define U_abs(x) ((x) >= 0 ? (x) : -(x))
#define U_absf(x) ((x) >= 0.0f ? (x) : -(x))
#define U_min(a, b) (((a) < (b)) ? (a) : (b))
#define U_max(a, b) (((a) < (b)) ? (b) : (a))

/* math */
float U_powf(float x, float y);
float U_sinf(float x);
float U_cosf(float x);
float U_acosf(float x);
float U_atan2f(float y, float x);
float U_sqrtf(float x);
float U_fminf(float a, float b);
float U_fmaxf(float a, float b);
float U_fmodf(float x, float y);
float U_modff(float x, float *iptr);

#endif /* U_MATH_H */
