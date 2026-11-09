#pragma once

#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <complex>

namespace test_harness {

inline int g_test_count = 0;
inline int g_fail_count = 0;

inline void assert_true(bool condition, const std::string& msg, const char* file, int line) {
    g_test_count++;
    if (!condition) {
        g_fail_count++;
        std::cerr << "FAIL: " << msg << " (" << file << ":" << line << ")\n";
    }
}

inline void assert_near(double val, double expected, double tol, const std::string& msg, const char* file, int line) {
    g_test_count++;
    if (std::isnan(val) || std::isnan(expected) || std::abs(val - expected) > tol) {
        g_fail_count++;
        std::cerr << "FAIL: " << msg << " [expected " << expected << " +/- " << tol 
                  << ", got " << val << "] (" << file << ":" << line << ")\n";
    }
}

inline void assert_complex_near(
    const std::complex<double>& val,
    const std::complex<double>& expected,
    double tol,
    const std::string& msg,
    const char* file,
    int line)
{
    g_test_count++;
    double diff = std::abs(val - expected);
    if (std::isnan(diff) || diff > tol) {
        g_fail_count++;
        std::cerr << "FAIL: " << msg << " [expected (" << expected.real() << "," << expected.imag()
                  << ") got (" << val.real() << "," << val.imag() << "), diff=" << diff
                  << "] (" << file << ":" << line << ")\n";
    }
}

#define ASSERT_TRUE(c) test_harness::assert_true((c), #c, __FILE__, __LINE__)
#define ASSERT_FALSE(c) test_harness::assert_true(!(c), "!(" #c ")", __FILE__, __LINE__)
#define ASSERT_NEAR(v, e, t) test_harness::assert_near((v), (e), (t), #v, __FILE__, __LINE__)
#define ASSERT_COMPLEX_NEAR(v, e, t) test_harness::assert_complex_near((v), (e), (t), #v, __FILE__, __LINE__)

#define ASSERT_THROWS(expr, ExType) do { \
    bool caught = false; \
    try { (void)(expr); } catch (const ExType&) { caught = true; } catch (...) {} \
    test_harness::assert_true(caught, "Expected exception " #ExType " for " #expr, __FILE__, __LINE__); \
} while(0)

#define ASSERT_NO_THROW(expr) do { \
    bool caught = false; \
    try { (void)(expr); } catch (...) { caught = true; } \
    test_harness::assert_true(!caught, "Unexpected exception for " #expr, __FILE__, __LINE__); \
} while(0)

} // namespace test_harness
