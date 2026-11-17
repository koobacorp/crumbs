#pragma once
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
inline int g_pass = 0, g_fail = 0;
#define CHECK(x) do { if (x) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s | ", __FILE__, __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
namespace tu {
inline float peak(const std::vector<float>& x) { float p = 0; for (float v : x) p = std::max(p, std::fabs(v)); return p; }
inline bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }
inline double rms(const std::vector<float>& x, size_t a, size_t b) {
    b = std::min(b, x.size()); a = std::min(a, b); double e = 0; size_t n = b - a;
    for (size_t i = a; i < b; ++i) e += (double)x[i] * x[i]; return n ? std::sqrt(e / n) : 0;
}
}
