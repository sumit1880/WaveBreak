#include "wavebreak/signal_generator.hpp"
#include "wavebreak/fft_processor.hpp"
#include <cmath>
#include <iostream>

namespace wavebreak {

BoxMullerRng::BoxMullerRng(uint64_t seed)
    : rng_(seed), uniform_dist_(0.0, 1.0), has_cached_(false), cached_val_(0.0) {}

void BoxMullerRng::reset(uint64_t seed) {
    rng_.seed(seed);
    has_cached_ = false;
    cached_val_ = 0.0;
}

double BoxMullerRng::next_gaussian() {
    if (has_cached_) {
        has_cached_ = false;
        return cached_val_;
    }
    double u1 = 0.0;
    while (u1 <= 1e-15) {
        u1 = uniform_dist_(rng_);
    }
    double u2 = uniform_dist_(rng_);

    double r = std::sqrt(-2.0 * std::log(u1));
    double theta = 2.0 * PI * u2;

    cached_val_ = r * std::sin(theta);
    has_cached_ = true;
    return r * std::cos(theta);
}

double BoxMullerRng::next_gaussian(double mean, double stddev) {
    return mean + stddev * next_gaussian();
}

Complex BoxMullerRng::next_complex_gaussian(double total_variance) {
    double stddev_each = std::sqrt(total_variance / 2.0);
    double i = next_gaussian(0.0, stddev_each);
    double q = next_gaussian(0.0, stddev_each);
    return Complex(i, q);
}

SignalGenerator::SignalGenerator(const Config& config)
    : config_(config) {}

SimulationResult SignalGenerator::generate() {
    return generate_scenario(config_);
}

SimulationResult SignalGenerator::generate_scenario(const Config& config) {
    validate_parameters(config.fs, config.fft_size, config.hop_size, config.desired_freq);

    SimulationResult result;
    result.fs = config.fs;
    result.scenario = config.scenario;
    result.seed = config.seed;
    result.samples.resize(config.num_samples);

    BoxMullerRng rng(config.seed);

    // Determine SNR in dB
    double snr_db = 20.0;
    if (config.scenario == "clean") {
        snr_db = 60.0;
    } else if (config.scenario == "low_noise") {
        snr_db = 30.0;
    } else if (config.scenario == "high_noise") {
        snr_db = 0.0;
    }

    // Noise power: SNR_dB = 10 * log10(A_desired^2 / sigma^2)
    double a_des_sq = config.desired_amp * config.desired_amp;
    double sigma_sq = a_des_sq * std::pow(10.0, -snr_db / 10.0);

    double total_duration = static_cast<double>(config.num_samples) / config.fs;

    // 1. Generate desired signal + background AWGN noise
    for (size_t n = 0; n < config.num_samples; ++n) {
        double t = static_cast<double>(n) / config.fs;
        double phase = 2.0 * PI * config.desired_freq * t;
        Complex desired_sample(
            config.desired_amp * std::cos(phase),
            config.desired_amp * std::sin(phase)
        );
        Complex noise_sample = rng.next_complex_gaussian(sigma_sq);
        result.samples[n] = desired_sample + noise_sample;
    }

    // 2. Generate scenario-specific interference and ground truth
    if (config.scenario == "clean" || config.scenario == "low_noise" || config.scenario == "high_noise") {
        // No interference events
        return result;
    }

    if (config.scenario == "narrowband") {
        double int_freq = 250000.0; // +250 kHz
        double int_amp = 0.3;
        for (size_t n = 0; n < config.num_samples; ++n) {
            double t = static_cast<double>(n) / config.fs;
            double phase = 2.0 * PI * int_freq * t;
            result.samples[n] += Complex(int_amp * std::cos(phase), int_amp * std::sin(phase));
        }

        GroundTruthEvent ev;
        ev.id = 1;
        ev.type = "narrowband";
        ev.start_time = 0.0;
        ev.end_time = total_duration;
        ev.frequency = int_freq;
        ev.freq_min = int_freq - 1000.0;
        ev.freq_max = int_freq + 1000.0;
        ev.amplitude = int_amp;
        ev.power_db = 20.0 * std::log10(int_amp);
        result.ground_truth.push_back(ev);
    }
    else if (config.scenario == "broadband") {
        // 150 kHz wide band-limited noise centered at +275 kHz [200 kHz, 350 kHz]
        // ~10 dB above noise floor per bin
        double f_center = 275000.0;
        double bw = 150000.0;
        double f_min = f_center - bw / 2.0; // 200 kHz
        double f_max = f_center + bw / 2.0; // 350 kHz

        size_t n_fft = config.fft_size;
        size_t n_samples = config.num_samples;

        // In an N_fft window, power per bin in the band should be 10 dB above noise floor
        // With sum(w^2) normalization, noise floor per bin is sigma_sq
        // 10 dB above floor means power spectral density is 10.0 * sigma_sq
        double int_bin_var = 10.0 * sigma_sq * static_cast<double>(n_samples);

        SampleVector freq_domain(n_samples, Complex(0.0, 0.0));
        double df = config.fs / static_cast<double>(n_samples);

        for (size_t k = 0; k < n_samples; ++k) {
            double freq = (k < n_samples / 2)
                ? static_cast<double>(k) * df
                : static_cast<double>(static_cast<int64_t>(k) - static_cast<int64_t>(n_samples)) * df;

            if (freq >= f_min && freq <= f_max) {
                freq_domain[k] = rng.next_complex_gaussian(int_bin_var);
            }
        }

        FFTProcessor::ifft(freq_domain);

        for (size_t n = 0; n < n_samples; ++n) {
            result.samples[n] += freq_domain[n];
        }

        GroundTruthEvent ev;
        ev.id = 1;
        ev.type = "broadband";
        ev.start_time = 0.0;
        ev.end_time = total_duration;
        ev.frequency = f_center;
        ev.freq_min = f_min;
        ev.freq_max = f_max;
        ev.amplitude = std::sqrt(10.0 * sigma_sq);
        ev.power_db = 10.0 * std::log10(10.0 * (sigma_sq / static_cast<double>(n_fft)));
        result.ground_truth.push_back(ev);
    }
    else if (config.scenario == "burst") {
        // Narrowband tone at +250 kHz, 3 bursts of 20 ms, period 80 ms, first starts at 40 ms
        double int_freq = 250000.0;
        double int_amp = 0.3;
        double burst_duration = 0.020; // 20 ms
        double burst_period = 0.080;   // 80 ms
        double first_start = 0.040;    // 40 ms
        int num_bursts = 3;

        std::vector<std::pair<double, double>> burst_intervals;
        for (int b = 0; b < num_bursts; ++b) {
            double t_start = first_start + static_cast<double>(b) * burst_period;
            double t_end = t_start + burst_duration;
            burst_intervals.push_back({t_start, t_end});

            GroundTruthEvent ev;
            ev.id = b + 1;
            ev.type = "narrowband";
            ev.start_time = t_start;
            ev.end_time = t_end;
            ev.frequency = int_freq;
            ev.freq_min = int_freq - 1000.0;
            ev.freq_max = int_freq + 1000.0;
            ev.amplitude = int_amp;
            ev.power_db = 20.0 * std::log10(int_amp);
            result.ground_truth.push_back(ev);
        }

        for (size_t n = 0; n < config.num_samples; ++n) {
            double t = static_cast<double>(n) / config.fs;
            bool active = false;
            for (const auto& interval : burst_intervals) {
                if (t >= interval.first && t < interval.second) {
                    active = true;
                    break;
                }
            }
            if (active) {
                double phase = 2.0 * PI * int_freq * t;
                result.samples[n] += Complex(int_amp * std::cos(phase), int_amp * std::sin(phase));
            }
        }
    }
    else if (config.scenario == "multi") {
        // Two narrowband tones: -200 kHz (amp 0.25) and +350 kHz (amp 0.35)
        double f1 = -200000.0;
        double a1 = 0.25;
        double f2 = 350000.0;
        double a2 = 0.35;

        for (size_t n = 0; n < config.num_samples; ++n) {
            double t = static_cast<double>(n) / config.fs;
            double phase1 = 2.0 * PI * f1 * t;
            double phase2 = 2.0 * PI * f2 * t;
            result.samples[n] += Complex(a1 * std::cos(phase1), a1 * std::sin(phase1))
                               + Complex(a2 * std::cos(phase2), a2 * std::sin(phase2));
        }

        GroundTruthEvent ev1;
        ev1.id = 1;
        ev1.type = "narrowband";
        ev1.start_time = 0.0;
        ev1.end_time = total_duration;
        ev1.frequency = f1;
        ev1.freq_min = f1 - 1000.0;
        ev1.freq_max = f1 + 1000.0;
        ev1.amplitude = a1;
        ev1.power_db = 20.0 * std::log10(a1);
        result.ground_truth.push_back(ev1);

        GroundTruthEvent ev2;
        ev2.id = 2;
        ev2.type = "narrowband";
        ev2.start_time = 0.0;
        ev2.end_time = total_duration;
        ev2.frequency = f2;
        ev2.freq_min = f2 - 1000.0;
        ev2.freq_max = f2 + 1000.0;
        ev2.amplitude = a2;
        ev2.power_db = 20.0 * std::log10(a2);
        result.ground_truth.push_back(ev2);
    }
    else if (config.scenario == "drift") {
        // Tone with a linear frequency ramp: +200 kHz to +400 kHz, amp 0.3
        double f0 = 200000.0;
        double f1 = 400000.0;
        double amp = 0.3;
        double chirp_rate = (f1 - f0) / total_duration;

        for (size_t n = 0; n < config.num_samples; ++n) {
            double t = static_cast<double>(n) / config.fs;
            double phase = 2.0 * PI * (f0 * t + 0.5 * chirp_rate * t * t);
            result.samples[n] += Complex(amp * std::cos(phase), amp * std::sin(phase));
        }

        GroundTruthEvent ev;
        ev.id = 1;
        ev.type = "drift";
        ev.start_time = 0.0;
        ev.end_time = total_duration;
        ev.frequency = (f0 + f1) / 2.0;
        ev.freq_min = f0;
        ev.freq_max = f1;
        ev.amplitude = amp;
        ev.power_db = 20.0 * std::log10(amp);
        result.ground_truth.push_back(ev);
    }

    return result;
}

} // namespace wavebreak
