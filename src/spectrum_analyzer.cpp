#include "wavebreak/spectrum_analyzer.hpp"
#include "wavebreak/fft_processor.hpp"
#include <cmath>
#include <algorithm>

namespace wavebreak {

std::vector<double> SpectrumAnalyzer::periodic_hann_window(size_t n) {
    if (n == 0) return {};
    std::vector<double> w(n);
    double denom = static_cast<double>(n);
    for (size_t i = 0; i < n; ++i) {
        w[i] = 0.5 * (1.0 - std::cos(2.0 * PI * static_cast<double>(i) / denom));
    }
    return w;
}

double SpectrumAnalyzer::window_sum_squares(const std::vector<double>& window) {
    double sum = 0.0;
    for (double val : window) {
        sum += val * val;
    }
    return sum;
}

std::vector<double> SpectrumAnalyzer::compute_power_spectrum(
    const SampleVector& windowed_fft,
    double sum_w2)
{
    size_t n = windowed_fft.size();
    if (n == 0 || sum_w2 <= 0.0) return {};
    std::vector<double> power(n);
    double inv_s2 = 1.0 / sum_w2;
    for (size_t i = 0; i < n; ++i) {
        double real = windowed_fft[i].real();
        double imag = windowed_fft[i].imag();
        power[i] = (real * real + imag * imag) * inv_s2;
    }
    return power;
}

double SpectrumAnalyzer::bin_to_frequency(int bin, size_t fft_size, double fs) {
    if (fft_size == 0) return 0.0;
    double df = fs / static_cast<double>(fft_size);
    return -fs / 2.0 + static_cast<double>(bin) * df;
}

int SpectrumAnalyzer::frequency_to_bin(double freq, size_t fft_size, double fs) {
    if (fft_size == 0 || fs <= 0.0) return 0;
    double df = fs / static_cast<double>(fft_size);
    double bin_float = (freq + fs / 2.0) / df;
    int bin = static_cast<int>(std::round(bin_float));
    if (bin < 0) bin = 0;
    if (bin >= static_cast<int>(fft_size)) bin = static_cast<int>(fft_size) - 1;
    return bin;
}

std::vector<double> SpectrumAnalyzer::power_to_db(
    const std::vector<double>& power,
    double floor_db)
{
    std::vector<double> db(power.size());
    double min_power = std::pow(10.0, floor_db / 10.0);
    for (size_t i = 0; i < power.size(); ++i) {
        double p = power[i];
        if (p < min_power || std::isnan(p) || std::isinf(p)) {
            p = min_power;
        }
        db[i] = 10.0 * std::log10(p);
    }
    return db;
}

SpectrogramResult SpectrumAnalyzer::compute_spectrogram(
    const SampleVector& samples,
    size_t fft_size,
    size_t hop_size,
    double fs,
    double floor_db)
{
    if (samples.size() < fft_size || fft_size == 0 || hop_size == 0) {
        return {};
    }

    SpectrogramResult result;
    result.num_bins = fft_size;
    size_t num_windows = (samples.size() - fft_size) / hop_size + 1;
    result.num_windows = num_windows;

    // Precompute window and its sum of squares
    std::vector<double> win = periodic_hann_window(fft_size);
    double sum_w2 = window_sum_squares(win);

    // Compute shifted frequency axis
    result.frequencies.resize(fft_size);
    for (size_t k = 0; k < fft_size; ++k) {
        result.frequencies[k] = bin_to_frequency(static_cast<int>(k), fft_size, fs);
    }

    result.window_times.resize(num_windows);
    result.power_db.resize(num_windows);

    SampleVector buffer(fft_size);
    for (size_t w = 0; w < num_windows; ++w) {
        size_t start_idx = w * hop_size;
        result.window_times[w] = static_cast<double>(start_idx + fft_size / 2) / fs;

        for (size_t n = 0; n < fft_size; ++n) {
            buffer[n] = samples[start_idx + n] * win[n];
        }

        FFTProcessor::fft(buffer);
        std::vector<double> power = compute_power_spectrum(buffer, sum_w2);
        std::vector<double> shifted_power = fftshift(power);
        result.power_db[w] = power_to_db(shifted_power, floor_db);
    }

    return result;
}

std::vector<std::vector<double>> SpectrumAnalyzer::decimate_bins(
    const std::vector<std::vector<double>>& spec_db,
    size_t max_bins)
{
    if (spec_db.empty() || max_bins == 0) return {};
    size_t original_bins = spec_db[0].size();
    if (original_bins <= max_bins) {
        return spec_db;
    }

    size_t factor = (original_bins + max_bins - 1) / max_bins;
    size_t decimated_bins = original_bins / factor;

    std::vector<std::vector<double>> result(spec_db.size(), std::vector<double>(decimated_bins));
    for (size_t w = 0; w < spec_db.size(); ++w) {
        for (size_t b = 0; b < decimated_bins; ++b) {
            double lin_sum = 0.0;
            for (size_t k = 0; k < factor; ++k) {
                size_t src_idx = b * factor + k;
                if (src_idx < original_bins) {
                    lin_sum += std::pow(10.0, spec_db[w][src_idx] / 10.0);
                }
            }
            double avg_lin = lin_sum / static_cast<double>(factor);
            if (avg_lin <= 1e-20) {
                result[w][b] = -200.0;
            } else {
                result[w][b] = 10.0 * std::log10(avg_lin);
            }
        }
    }
    return result;
}

} // namespace wavebreak
