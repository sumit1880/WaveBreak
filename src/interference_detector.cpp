#include "spectrum_sentinel/interference_detector.hpp"
#include "spectrum_sentinel/event_classifier.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace spectrum_sentinel {

double InterferenceDetector::compute_window_noise_floor_lin(
    const std::vector<double>& window_power_db,
    int desired_bin,
    int guard_bins)
{
    size_t n = window_power_db.size();
    if (n == 0) return 1e-20;

    std::vector<double> remaining_powers;
    remaining_powers.reserve(n);

    for (int k = 0; k < static_cast<int>(n); ++k) {
        if (std::abs(k - desired_bin) > guard_bins) {
            double p_db = window_power_db[k];
            double p_lin = std::pow(10.0, p_db / 10.0);
            remaining_powers.push_back(p_lin);
        }
    }

    if (remaining_powers.empty()) {
        return 1e-20;
    }

    size_t mid = remaining_powers.size() / 2;
    std::nth_element(remaining_powers.begin(), remaining_powers.begin() + mid, remaining_powers.end());
    double median_lin = remaining_powers[mid];

    if (remaining_powers.size() % 2 == 0 && mid > 0) {
        auto max_it = std::max_element(remaining_powers.begin(), remaining_powers.begin() + mid);
        median_lin = (median_lin + *max_it) / 2.0;
    }

    // Bias correction: noise floor = median * (1 / ln 2)
    double bias_correction = 1.0 / std::log(2.0);
    double noise_floor_lin = median_lin * bias_correction;
    return (noise_floor_lin > 1e-30) ? noise_floor_lin : 1e-30;
}

std::pair<double, double> InterferenceDetector::parabolic_interpolation(
    double alpha, double beta, double gamma)
{
    double denom = alpha - 2.0 * beta + gamma;
    if (std::abs(denom) < 1e-12) {
        return {0.0, beta};
    }
    double delta = 0.5 * (alpha - gamma) / denom;
    if (delta < -1.0) delta = -1.0;
    if (delta > 1.0) delta = 1.0;

    double peak_db = beta - 0.25 * (alpha - gamma) * delta;
    return {delta, peak_db};
}

std::vector<DetectedEvent> InterferenceDetector::detect(
    const SpectrogramResult& spec,
    const Config& config)
{
    if (spec.num_windows == 0 || spec.num_bins == 0) {
        return {};
    }

    int desired_bin = SpectrumAnalyzer::frequency_to_bin(config.desired_freq, spec.num_bins, config.fs);
    double df = config.fs / static_cast<double>(spec.num_bins);
    double total_duration = static_cast<double>(config.num_samples) / config.fs;
    double window_duration = static_cast<double>(config.fft_size) / config.fs;

    std::vector<std::vector<WindowDetection>> detections_per_window(spec.num_windows);

    for (size_t w = 0; w < spec.num_windows; ++w) {
        const auto& p_db = spec.power_db[w];
        double floor_lin = compute_window_noise_floor_lin(p_db, desired_bin, 5);
        double floor_db = 10.0 * std::log10(floor_lin);

        // 1. Narrowband detection
        double nb_threshold_db = floor_db + config.threshold_db;
        int n_bins = static_cast<int>(spec.num_bins);

        int k = 0;
        while (k < n_bins) {
            // Exclude desired tone +/- 5 bins
            if (std::abs(k - desired_bin) <= 5) {
                ++k;
                continue;
            }

            if (p_db[k] > nb_threshold_db) {
                int start_bin = k;
                while (k < n_bins &&
                       std::abs(k - desired_bin) > 5 &&
                       p_db[k] > nb_threshold_db) {
                    ++k;
                }
                int end_bin = k - 1;
                size_t width = static_cast<size_t>(end_bin - start_bin + 1);

                // Contiguous width <= 5 bins for narrowband
                if (width <= 5) {
                    int peak_bin = start_bin;
                    double max_p = p_db[start_bin];
                    for (int b = start_bin + 1; b <= end_bin; ++b) {
                        if (p_db[b] > max_p) {
                            max_p = p_db[b];
                            peak_bin = b;
                        }
                    }

                    bool is_local_max = true;
                    if (peak_bin > 0 && p_db[peak_bin - 1] > max_p) {
                        is_local_max = false;
                    }
                    if (peak_bin + 1 < n_bins && p_db[peak_bin + 1] > max_p) {
                        is_local_max = false;
                    }

                    if (is_local_max) {
                        double peak_freq = spec.frequencies[peak_bin];
                        double peak_p_db = max_p;

                        if (peak_bin > 0 && peak_bin + 1 < n_bins) {
                            auto interp = parabolic_interpolation(
                                p_db[peak_bin - 1], p_db[peak_bin], p_db[peak_bin + 1]);
                            peak_freq = spec.frequencies[peak_bin] + interp.first * df;
                            peak_p_db = interp.second;
                        }

                        double margin_db = peak_p_db - floor_db;

                        WindowDetection wd;
                        wd.window_idx = w;
                        wd.window_time = spec.window_times[w];
                        wd.type = "narrowband";
                        wd.frequency = peak_freq;
                        wd.freq_min = peak_freq - 0.5 * df * static_cast<double>(width);
                        wd.freq_max = peak_freq + 0.5 * df * static_cast<double>(width);
                        wd.peak_power_db = peak_p_db;
                        wd.margin_db = margin_db;
                        wd.bin_width = width;
                        detections_per_window[w].push_back(wd);
                    }
                }
            } else {
                ++k;
            }
        }

        // 2. Broadband detection (16 sub-bands)
        size_t num_subbands = 16;
        size_t bins_per_subband = spec.num_bins / num_subbands;
        std::vector<bool> subband_flagged(num_subbands, false);
        std::vector<double> subband_power_db(num_subbands, floor_db);
        size_t desired_subband = static_cast<size_t>(desired_bin) / bins_per_subband;

        for (size_t s = 0; s < num_subbands; ++s) {
            if (s == desired_subband) {
                continue;
            }

            size_t sub_start = s * bins_per_subband;
            size_t sub_end = (s + 1) * bins_per_subband;

            std::vector<double> sub_lins;
            for (size_t b = sub_start; b < sub_end; ++b) {
                if (std::abs(static_cast<int>(b) - desired_bin) > 5) {
                    sub_lins.push_back(std::pow(10.0, p_db[b] / 10.0));
                }
            }

            if (!sub_lins.empty()) {
                // Robust subband mean: trim top 5 bins so an isolated narrowband spike
                // does not trigger broadband detection across the sub-band
                std::sort(sub_lins.begin(), sub_lins.end());
                size_t trim_count = sub_lins.size() > 5 ? sub_lins.size() - 5 : sub_lins.size();
                double lin_sum = 0.0;
                for (size_t i = 0; i < trim_count; ++i) {
                    lin_sum += sub_lins[i];
                }
                double mean_lin = lin_sum / static_cast<double>(trim_count);
                double sub_db = 10.0 * std::log10(std::max(mean_lin, 1e-30));
                subband_power_db[s] = sub_db;
                if (sub_db > floor_db + 3.0) {
                    subband_flagged[s] = true;
                }
            }
        }

        // Find contiguous spans of flagged sub-bands
        size_t s = 0;
        double subband_bw = config.fs / static_cast<double>(num_subbands);
        while (s < num_subbands) {
            if (subband_flagged[s]) {
                size_t start_s = s;
                while (s < num_subbands && subband_flagged[s]) {
                    ++s;
                }
                size_t end_s = s - 1;
                double span_hz = static_cast<double>(end_s - start_s + 1) * subband_bw;

                // Report contiguous flagged span >= 50 kHz as broadband
                if (span_hz >= 50000.0) {
                    double f_min = -config.fs / 2.0 + static_cast<double>(start_s) * subband_bw;
                    double f_max = -config.fs / 2.0 + static_cast<double>(end_s + 1) * subband_bw;
                    double center_f = (f_min + f_max) / 2.0;

                    double max_sub_p = -200.0;
                    for (size_t sb = start_s; sb <= end_s; ++sb) {
                        if (subband_power_db[sb] > max_sub_p) {
                            max_sub_p = subband_power_db[sb];
                        }
                    }
                    double margin = max_sub_p - floor_db;

                    WindowDetection wd;
                    wd.window_idx = w;
                    wd.window_time = spec.window_times[w];
                    wd.type = "broadband";
                    wd.frequency = center_f;
                    wd.freq_min = f_min;
                    wd.freq_max = f_max;
                    wd.peak_power_db = max_sub_p;
                    wd.margin_db = margin;
                    wd.bin_width = (end_s - start_s + 1) * bins_per_subband;
                    detections_per_window[w].push_back(wd);
                }
            } else {
                ++s;
            }
        }

        // Subsume narrowband detections that fall inside a broadband span in this window
        std::vector<WindowDetection> filtered_w_dets;
        for (const auto& wd : detections_per_window[w]) {
            if (wd.type == "narrowband") {
                bool inside_broadband = false;
                for (const auto& bb : detections_per_window[w]) {
                    if (bb.type == "broadband") {
                        if (wd.frequency >= bb.freq_min && wd.frequency <= bb.freq_max) {
                            inside_broadband = true;
                            break;
                        }
                    }
                }
                if (!inside_broadband) {
                    filtered_w_dets.push_back(wd);
                }
            } else {
                filtered_w_dets.push_back(wd);
            }
        }
        detections_per_window[w] = std::move(filtered_w_dets);
    }

    // Merge window detections into unified events
    return merge_window_detections(
        detections_per_window,
        spec.window_times,
        window_duration,
        total_duration,
        2, // merge gap
        2  // min windows
    );
}

struct ActiveTrack {
    std::string type;
    size_t start_win{0};
    size_t last_win{0};
    double first_freq{0.0};
    double last_freq{0.0};
    double f_min{0.0};
    double f_max{0.0};
    double peak_power_db{-200.0};
    double max_margin_db{0.0};
    size_t hit_count{0};
    std::vector<WindowDetection> detections;
};

std::vector<DetectedEvent> InterferenceDetector::merge_window_detections(
    const std::vector<std::vector<WindowDetection>>& detections_per_window,
    const std::vector<double>& window_times,
    double window_duration,
    double total_duration,
    size_t merge_gap_windows,
    size_t min_windows)
{
    std::vector<ActiveTrack> closed_tracks;
    std::vector<ActiveTrack> open_tracks;

    for (size_t w = 0; w < detections_per_window.size(); ++w) {
        const auto& w_dets = detections_per_window[w];

        for (auto it = open_tracks.begin(); it != open_tracks.end(); ) {
            if (w > it->last_win + 1 + merge_gap_windows) {
                closed_tracks.push_back(*it);
                it = open_tracks.erase(it);
            } else {
                ++it;
            }
        }

        std::vector<bool> det_matched(w_dets.size(), false);

        for (size_t d = 0; d < w_dets.size(); ++d) {
            const auto& det = w_dets[d];
            int best_track_idx = -1;
            double best_dist = 1e9;

            for (size_t t = 0; t < open_tracks.size(); ++t) {
                auto& track = open_tracks[t];
                if (track.type != det.type) continue;

                if (det.type == "narrowband") {
                    double freq_dist = std::abs(det.frequency - track.last_freq);
                    if (freq_dist <= 5000.0 && freq_dist < best_dist) {
                        best_dist = freq_dist;
                        best_track_idx = static_cast<int>(t);
                    }
                } else if (det.type == "broadband") {
                    if (std::max(det.freq_min, track.f_min) <= std::min(det.freq_max, track.f_max)) {
                        best_track_idx = static_cast<int>(t);
                        break;
                    }
                }
            }

            if (best_track_idx >= 0) {
                auto& track = open_tracks[best_track_idx];
                track.last_win = w;
                track.last_freq = det.frequency;
                track.f_min = std::min(track.f_min, det.freq_min);
                track.f_max = std::max(track.f_max, det.freq_max);
                track.peak_power_db = std::max(track.peak_power_db, det.peak_power_db);
                track.max_margin_db = std::max(track.max_margin_db, det.margin_db);
                track.hit_count++;
                track.detections.push_back(det);
                det_matched[d] = true;
            }
        }

        for (size_t d = 0; d < w_dets.size(); ++d) {
            if (!det_matched[d]) {
                const auto& det = w_dets[d];
                ActiveTrack nt;
                nt.type = det.type;
                nt.start_win = w;
                nt.last_win = w;
                nt.first_freq = det.frequency;
                nt.last_freq = det.frequency;
                nt.f_min = det.freq_min;
                nt.f_max = det.freq_max;
                nt.peak_power_db = det.peak_power_db;
                nt.max_margin_db = det.margin_db;
                nt.hit_count = 1;
                nt.detections.push_back(det);
                open_tracks.push_back(nt);
            }
        }
    }

    for (const auto& t : open_tracks) {
        closed_tracks.push_back(t);
    }

    std::vector<DetectedEvent> events;
    int event_id = 1;

    for (const auto& track : closed_tracks) {
        if (track.hit_count < min_windows) {
            continue;
        }

        DetectedEvent ev;
        ev.id = event_id++;
        double t_start = window_times[track.start_win] - 0.5 * window_duration;
        double t_end = window_times[track.last_win] + 0.5 * window_duration;
        if (t_start < 0.0) t_start = 0.0;
        if (t_end > total_duration) t_end = total_duration;

        ev.start_time = t_start;
        ev.end_time = t_end;
        ev.peak_power_db = track.peak_power_db;
        ev.margin_db = track.max_margin_db;
        ev.freq_min = track.f_min;
        ev.freq_max = track.f_max;

        if (track.type == "narrowband") {
            ev.spectral_category = "narrowband";
            double sum_f = 0.0;
            for (const auto& d : track.detections) sum_f += d.frequency;
            ev.frequency = sum_f / static_cast<double>(track.detections.size());
        } else {
            ev.spectral_category = "broadband";
            ev.frequency = (track.f_min + track.f_max) / 2.0;
        }

        EventClassifier::classify_and_assign_severity(ev, total_duration);
        events.push_back(ev);
    }

    return events;
}

} // namespace spectrum_sentinel
