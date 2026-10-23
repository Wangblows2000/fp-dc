/* Portable sin / cos / atan / atan2 (fdlibm algorithms, freely redistributable Sun code, rewritten).
 * Gameplay uses these instead of the C library so the host build and the Dreamcast build compute
 * bit-identical results from the same inputs (IEEE double add/mul/div only, no library calls): an input
 * script recorded on the host then replays the same way on the DC (demo builds, plat_dc demo_load).
 * Accuracy: < 1 ulp for the arguments the game uses (|x| < 1e5 rad). */
#include <math.h>
#include "../fp.h"

static const double
    S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03, S3 = -1.98412698298579493134e-04,
    S4 = 2.75573137070700676789e-06, S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10,
    C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03, C3 = 2.48015872894767294178e-05,
    C4 = -2.75573143513906633035e-07, C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11,
    invpio2 = 6.36619772367581382433e-01, pio2_1 = 1.57079632673412561417e+00, pio2_1t = 6.07710050650619224932e-11,
    pio2_2 = 6.07710050630396597660e-11, pio2_2t = 2.02226624879595063154e-21;

static double k_sin(double x, double y, int iy) {
    double z = x * x, v = z * x, r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
    if (iy == 0) return x + v * (S1 + z * r);
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}
static double k_cos(double x, double y) {
    double z = x * x, r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6))))), hz = 0.5 * z, w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}
/* x = n * pi/2 + (y0 + y1), |y0 + y1| <= pi/4 (two-step Cody-Waite; exact enough for |x| < 1e5) */
static int rem_pio2(double x, double *y0, double *y1) {
    double fn = floor(x * invpio2 + 0.5);
    double r = x - fn * pio2_1, w = fn * pio2_1t;
    double t = r; w = fn * pio2_2; r = t - w; w = fn * pio2_2t - ((t - r) - w);
    *y0 = r - w; *y1 = (r - *y0) - w;
    return (int)((long long)fn & 3);
}
double fd_sin(double x) {
    if (fabs(x) < 0.7853981633974483) return k_sin(x, 0, 0);
    double a, b;
    switch (rem_pio2(x, &a, &b)) {
    case 0: return k_sin(a, b, 1);
    case 1: return k_cos(a, b);
    case 2: return -k_sin(a, b, 1);
    default: return -k_cos(a, b);
    }
}
double fd_cos(double x) {
    if (fabs(x) < 0.7853981633974483) return k_cos(x, 0);
    double a, b;
    switch (rem_pio2(x, &a, &b)) {
    case 0: return k_cos(a, b);
    case 1: return -k_sin(a, b, 1);
    case 2: return -k_cos(a, b);
    default: return k_sin(a, b, 1);
    }
}

static const double atanhi[4] = { 4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01, 1.57079632679489655800e+00 };
static const double atanlo[4] = { 2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17, 6.12323399573676603587e-17 };
static const double aT[11] = { 3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01,
    -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02, 6.66107313738753120669e-02,
    -5.83357013379057348645e-02, 4.97687799461593236017e-02, -3.65315727442169155270e-02, 1.62858201153657823623e-02 };

double fd_atan(double x) {
    int neg = x < 0, id;
    double ax = fabs(x);
    if (ax >= 7.378697629483821e19) return neg ? -(atanhi[3] + atanlo[3]) : atanhi[3] + atanlo[3];
    if (ax < 0.4375) {
        if (ax < 3.725290298461914e-09) return x;
        id = -1;
    } else if (ax < 1.1875) {
        if (ax < 0.6875) { id = 0; ax = (2.0 * ax - 1.0) / (2.0 + ax); }
        else { id = 1; ax = (ax - 1.0) / (ax + 1.0); }
    } else if (ax < 2.4375) { id = 2; ax = (ax - 1.5) / (1.0 + 1.5 * ax); }
    else { id = 3; ax = -1.0 / ax; }
    double z = ax * ax, w = z * z;
    double s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    double s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0) { double r = ax - ax * (s1 + s2); return neg ? -r : r; }
    z = atanhi[id] - ((ax * (s1 + s2) - atanlo[id]) - ax);
    return neg ? -z : z;
}

double fd_atan2(double y, double x) {
    static const double pi = 3.1415926535897931160E+00, pi_lo = 1.2246467991473531772E-16, pi_o_2 = 1.5707963267948965580E+00;
    if (x == 1.0) return fd_atan(y);
    int m = (signbit(y) ? 1 : 0) | (signbit(x) ? 2 : 0);
    if (y == 0) { switch (m) { case 0: case 1: return y; case 2: return pi; default: return -pi; } }
    if (x == 0) return m & 1 ? -pi_o_2 : pi_o_2;
    double z, q = fabs(y / x);
    if (q > 1.152921504606847e18) z = pi_o_2 + 0.5 * pi_lo;
    else if (m & 2 && q < 8.673617379884035e-19) z = 0.0;
    else z = fd_atan(q);
    switch (m) {
    case 0: return z;
    case 1: return -z;
    case 2: return pi - (z - pi_lo);
    default: return (z - pi_lo) - pi;
    }
}
