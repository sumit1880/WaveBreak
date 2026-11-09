#include "spectrum_sentinel/fft_processor.hpp"

namespace spectrum_sentinel {

void FFTProcessor::bit_reverse_copy(SampleVector& data) {
    size_t n = data.size();
    size_t j = 0;
    for (size_t i = 0; i < n - 1; ++i) {
        if (i < j) {
            std::swap(data[i], data[j]);
        }
        size_t k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }
}

void FFTProcessor::transform(SampleVector& data, bool inverse) {
    size_t n = data.size();
    if (!is_power_of_two(n)) {
        throw std::invalid_argument("FFT size must be a power of 2, received size: " + std::to_string(n));
    }
    if (n <= 1) {
        return;
    }

    bit_reverse_copy(data);

    for (size_t len = 2; len <= n; len <<= 1) {
        double sign = inverse ? 1.0 : -1.0;
        double base_angle = sign * 2.0 * PI / static_cast<double>(len);
        size_t half_len = len >> 1;

        for (size_t i = 0; i < n; i += len) {
            for (size_t j = 0; j < half_len; ++j) {
                double angle = base_angle * static_cast<double>(j);
                Complex w(std::cos(angle), std::sin(angle));
                Complex u = data[i + j];
                Complex v = data[i + j + half_len] * w;
                data[i + j] = u + v;
                data[i + j + half_len] = u - v;
            }
        }
    }

    if (inverse) {
        double inv_n = 1.0 / static_cast<double>(n);
        for (size_t i = 0; i < n; ++i) {
            data[i] *= inv_n;
        }
    }
}

void FFTProcessor::fft(SampleVector& data) {
    transform(data, false);
}

void FFTProcessor::ifft(SampleVector& data) {
    transform(data, true);
}

SampleVector FFTProcessor::dft_naive(const SampleVector& data) {
    size_t n = data.size();
    SampleVector out(n, Complex(0.0, 0.0));
    for (size_t k = 0; k < n; ++k) {
        Complex sum(0.0, 0.0);
        for (size_t t = 0; t < n; ++t) {
            double angle = -2.0 * PI * static_cast<double>(k * t) / static_cast<double>(n);
            Complex twiddle(std::cos(angle), std::sin(angle));
            sum += data[t] * twiddle;
        }
        out[k] = sum;
    }
    return out;
}

SampleVector FFTProcessor::idft_naive(const SampleVector& data) {
    size_t n = data.size();
    SampleVector out(n, Complex(0.0, 0.0));
    for (size_t k = 0; k < n; ++k) {
        Complex sum(0.0, 0.0);
        for (size_t t = 0; t < n; ++t) {
            double angle = 2.0 * PI * static_cast<double>(k * t) / static_cast<double>(n);
            Complex twiddle(std::cos(angle), std::sin(angle));
            sum += data[t] * twiddle;
        }
        out[k] = sum / static_cast<double>(n);
    }
    return out;
}

} // namespace spectrum_sentinel
