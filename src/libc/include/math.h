#ifndef PAL9588_MATH_H
#define PAL9588_MATH_H

#define M_PI 3.14159265358979323846

#ifdef __cplusplus
extern "C" {
#endif
double fabs(double value);
double floor(double value);
double ceil(double value);
double round(double value);
double fmax(double left, double right);
double fmin(double left, double right);
double sqrt(double value);
double pow(double base, double exponent);
double log(double value);
double sin(double value);
double cos(double value);

#ifdef __cplusplus
}
#endif

#endif
