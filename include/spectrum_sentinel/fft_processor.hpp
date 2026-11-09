#pragma once

#include "spectrum_sentinel/common.hpp"
#include <vector>

namespace spectrum_sentinel {

class FFTProcessor {
public:
    // Forward radix-2 Cooley-Tukey FFT on complex<double> in-place
    // Throws std::invalid_argument if size is not a positive power of 2
    static void fft(SampleVector& data);

    // Inverse radix-2 Cooley-Tukey FFT on complex<double> in-place
    // Divides by N for perfect inversion
    // Throws std::invalid_argument if size is not a positive power of 2
    static void ifft(SampleVector& data);

    // Naive O(N^2) Discrete Fourier Transform for verification/testing
    static SampleVector dft_naive(const SampleVector& data);

    // Naive O(N^2) Inverse Discrete Fourier Transform
    static SampleVector idft_naive(const SampleVector& data);

private:
    static void bit_reverse_copy(SampleVector& data);
    static void transform(SampleVector& data, bool inverse);
};

} // namespace spectrum_sentinel
