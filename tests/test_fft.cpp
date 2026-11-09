#include "test_harness.hpp"
#include "wavebreak/fft_processor.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

using namespace wavebreak;

void run_fft_tests() {
    std::cout << "[TEST] Running FFT tests...\n";

    // 1. Invalid sizes
    SampleVector empty;
    ASSERT_THROWS(FFTProcessor::fft(empty), std::invalid_argument);

    SampleVector non_pow2_3(3, Complex(1.0, 0.0));
    ASSERT_THROWS(FFTProcessor::fft(non_pow2_3), std::invalid_argument);

    SampleVector non_pow2_100(100, Complex(1.0, 0.0));
    ASSERT_THROWS(FFTProcessor::fft(non_pow2_100), std::invalid_argument);

    SampleVector non_pow2_1023(1023, Complex(1.0, 0.0));
    ASSERT_THROWS(FFTProcessor::fft(non_pow2_1023), std::invalid_argument);

    // 2. N=1 edge case
    SampleVector n1 = { Complex(4.2, -1.5) };
    ASSERT_NO_THROW(FFTProcessor::fft(n1));
    ASSERT_COMPLEX_NEAR(n1[0], Complex(4.2, -1.5), 1e-12);

    // 3. FFT vs Naive DFT for multiple power-of-2 sizes
    for (size_t n : {8, 16, 64, 128}) {
        SampleVector data(n);
        for (size_t i = 0; i < n; ++i) {
            double angle = 2.0 * PI * static_cast<double>(i) / static_cast<double>(n);
            data[i] = Complex(std::cos(3.0 * angle) + 0.5 * std::sin(7.0 * angle),
                              std::sin(2.0 * angle) - 0.2 * std::cos(5.0 * angle));
        }

        SampleVector naive_res = FFTProcessor::dft_naive(data);
        SampleVector fft_res = data;
        FFTProcessor::fft(fft_res);

        for (size_t i = 0; i < n; ++i) {
            ASSERT_COMPLEX_NEAR(fft_res[i], naive_res[i], 1e-9);
        }
    }

    // 4. Single tone lands in the exact bin
    {
        size_t n = 64;
        size_t target_bin = 9;
        SampleVector tone(n);
        for (size_t i = 0; i < n; ++i) {
            double angle = 2.0 * PI * static_cast<double>(target_bin * i) / static_cast<double>(n);
            tone[i] = Complex(std::cos(angle), std::sin(angle));
        }
        FFTProcessor::fft(tone);

        // Target bin should have magnitude equal to n, others close to 0
        for (size_t i = 0; i < n; ++i) {
            double mag = std::abs(tone[i]);
            if (i == target_bin) {
                ASSERT_NEAR(mag, static_cast<double>(n), 1e-9);
            } else {
                ASSERT_NEAR(mag, 0.0, 1e-9);
            }
        }
    }

    // 5. Two tones land in the right bins
    {
        size_t n = 128;
        size_t bin1 = 15;
        size_t bin2 = 42;
        SampleVector tones(n);
        for (size_t i = 0; i < n; ++i) {
            double a1 = 2.0 * PI * static_cast<double>(bin1 * i) / static_cast<double>(n);
            double a2 = 2.0 * PI * static_cast<double>(bin2 * i) / static_cast<double>(n);
            tones[i] = Complex(std::cos(a1), std::sin(a1)) * 1.5 + Complex(std::cos(a2), std::sin(a2)) * 0.7;
        }
        FFTProcessor::fft(tones);

        ASSERT_NEAR(std::abs(tones[bin1]), 1.5 * static_cast<double>(n), 1e-9);
        ASSERT_NEAR(std::abs(tones[bin2]), 0.7 * static_cast<double>(n), 1e-9);

        // Check non-tone bins
        for (size_t i = 0; i < n; ++i) {
            if (i != bin1 && i != bin2) {
                ASSERT_NEAR(std::abs(tones[i]), 0.0, 1e-9);
            }
        }
    }

    // 6. Parseval's Theorem
    {
        size_t n = 256;
        SampleVector data(n);
        for (size_t i = 0; i < n; ++i) {
            data[i] = Complex(std::sin(0.1 * static_cast<double>(i)),
                              std::cos(0.23 * static_cast<double>(i)));
        }

        double time_energy = 0.0;
        for (const auto& s : data) {
            time_energy += std::norm(s);
        }

        SampleVector transformed = data;
        FFTProcessor::fft(transformed);

        double freq_energy = 0.0;
        for (const auto& s : transformed) {
            freq_energy += std::norm(s);
        }
        freq_energy /= static_cast<double>(n);

        ASSERT_NEAR(time_energy, freq_energy, 1e-9);
    }

    // 7. Inverse Round-Trip: IFFT(FFT(x)) == x
    {
        size_t n = 512;
        SampleVector original(n);
        for (size_t i = 0; i < n; ++i) {
            original[i] = Complex(std::cos(static_cast<double>(i) * 0.7) * 2.3,
                                  std::sin(static_cast<double>(i) * 1.4) * -1.8);
        }

        SampleVector processed = original;
        FFTProcessor::fft(processed);
        FFTProcessor::ifft(processed);

        for (size_t i = 0; i < n; ++i) {
            ASSERT_COMPLEX_NEAR(processed[i], original[i], 1e-12);
        }
    }
}
