#pragma once

// Plain test kit: one FAIL line per failing check, exit code 1 if any failed. Same style as Beatality.

#include <cmath>
#include <cstdio>

namespace test {

inline int& failures() {
    static int count = 0;
    return count;
}

} // namespace test

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);                 \
            ++test::failures();                                                        \
        }                                                                              \
    } while (0)

#define CHECK_NEAR(actual, expected, eps)                                              \
    do {                                                                               \
        const double a_ = static_cast<double>(actual);                                 \
        const double e_ = static_cast<double>(expected);                               \
        if (!(std::fabs(a_ - e_) <= (eps))) {                                          \
            std::printf("FAIL %s:%d %s = %.9g, expected %.9g\n", __FILE__, __LINE__,   \
                        #actual, a_, e_);                                              \
            ++test::failures();                                                        \
        }                                                                              \
    } while (0)

// For checks whose measured value is worth seeing even when they pass.
#define NOTE(...) std::printf("  " __VA_ARGS__)
