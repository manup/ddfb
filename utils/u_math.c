
float U_powf(float x, float y)
{
    return powf(x, y);
}

float U_sinf(float x)
{
    return sinf(x);
}

float U_cosf(float x)
{
    return cosf(x);
}

float U_acosf(float x)
{
    return acosf(x);
}

float U_atan2f(float y, float x)
{
    return atan2f(y, x);
}

float U_sqrtf(float x)
{
    return sqrtf(x);
}

float U_fminf(float a, float b)
{
    return fminf(a, b);
}

float U_fmaxf(float a, float b)
{
    return fmaxf(a, b);
}

float U_fmodf(float x, float y)
{
    return fmodf(x, y);
}

float U_modff(float x, float *iptr)
{
    x = modff(x, iptr);
    return x;
}
