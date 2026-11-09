#pragma once

#include "spectrum_sentinel/common.hpp"
#include <vector>

namespace spectrum_sentinel {

struct SpectrogramResult {
    std::vector<std::vector<double>> power_db; // [window_index][bin_index]
    std::vector<double> window_times;          // center or start time in seconds
    std::vector<double> frequencies;           // shifted frequency for each bin in Hz
    size_t num_windows{0};
    size_t num_bins{0};
};

class SpectrumAnalyzer {
public:
    // Computes periodic Hann window of length N
    // w[n] = 0.5 * (1.0 - cos(2*pi*n / N)) for n=0..N-1
    static std::vector<double> periodic_hann_window(size_t n);

    // Sum of squares of window coefficients: sum(w^2)
    static double window_sum_squares(const std::vector<double>& window);

    // Normalised power spectrum: |FFT(x * w)|^2 / sum(w^2)
    // Returns unshifted power spectrum of size N
    static std::vector<double> compute_power_spectrum(
        const SampleVector& windowed_fft,
        double sum_w2);

    // fftshift swaps [0, N/2-1] with [N/2, N-1]
    template<typename T>
    static std::vector<T> fftshift(const std::vector<T>& in) {
        size_t n = in.size();
        if (n == 0) return {};
        size_t half = n / 2;
        std::vector<T> out(n);
        for (size_t i = 0; i < n; ++i) {
            out[i] = in[(i + half) % n];
        }
        return out;
    }

    // Maps shifted bin index [0..N-1] to frequency [-fs/2 .. fs/2 - fs/N]
    static double bin_to_frequency(int bin, size_t fft_size, double fs);

    // Maps frequency [-fs/2 .. fs/2] to shifted bin index [0..N-1]
    static int frequency_to_bin(double freq, size_t fft_size, double fs);

    // Converts linear power values to dB with a safety floor
    static std::vector<double> power_to_db(
        const std::vector<double>& power,
        double floor_db = -160.0);

    // Computes full sliding-window spectrogram (shifted and in dB)
    static SpectrogramResult compute_spectrogram(
        const SampleVector& samples,
        size_t fft_size,
        size_t hop_size,
        double fs,
        double floor_db = -160.0);

    // Decimates spectrogram bins to <= max_bins (e.g. 512)
    // Uses block-averaging (linear power averaged then converted to dB, or dB average)
    static std::vector<std::vector<double>> decimate_bins(
        const std::vector<std::vector<double>>& spec_db,
        size_t max_bins = 512);
};

} // namespace spectrum_sentinel
