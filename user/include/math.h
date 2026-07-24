#ifndef AUKOS_MATH_H
#define AUKOS_MATH_H

double fabs(double x);
double floor(double x);
double ceil(double x);
double round(double x);
double trunc(double x);
double log(double x);
double log10(double x);
double log2(double x);
double log1p(double x);
double pow(double x, double y);
double exp(double x);
double expm1(double x);
double sqrt(double x);
double cbrt(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double fmod(double x, double y);
double modf(double x, double *iptr);
double frexp(double x, int *exp);
double ldexp(double x, int exp);

int isinf(double x);
int isnan(double x);
double copysign(double x, double y);
double hypot(double x, double y);

#endif
