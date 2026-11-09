#include "test_harness.hpp"
#include "spectrum_sentinel/spectrum_analyzer.hpp"
#include <cmath>
#include <limits>

using namespace spectrum_sentinel;

void run_spectrum_analyzer_tests() {
    std::cout << "[TEST] Running Spectrum Analyzer tests...\n";

    // 1. Periodic Hann window properties
    size_t n = 1024;
    auto win = SpectrumAnalyzer::periodic_hann_window(n);
    ASSERT_TRUE(win.size() == n);
    ASSERT_NEAR(win[0], 0.0, 1e-12);
    ASSERT_NEAR(win[n / 2], 1.0, 1e-12);

    double sum_w2 = SpectrumAnalyzer::window_sum_squares(win);
    // Theoretical sum of squares for periodic Hann of length N is 3/8 * N
    double expected_sum_w2 = (3.0 / 8.0) * static_cast<double>(n);
    ASSERT_NEAR(sum_w2, expected_sum_w2, 1e-6);

    // 2. dB conversion safety
    std::vector<double> test_powers = {
        0.0,
        -5.0,
        1e-30,
        1.0,
        100.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()
    };
    double floor_db = -160.0;
    auto dbs = SpectrumAnalyzer::power_to_db(test_powers, floor_db);

    ASSERT_NEAR(dbs[0], floor_db, 1e-6);   // 0.0 -> floor
    ASSERT_NEAR(dbs[1], floor_db, 1e-6);   // negative -> floor
    ASSERT_NEAR(dbs[2], floor_db, 1e-6);   // tiny -> floor
    ASSERT_NEAR(dbs[3], 0.0, 1e-6);        // 1.0 -> 0 dB
    ASSERT_NEAR(dbs[4], 20.0, 1e-6);       // 100.0 -> +20 dB
    ASSERT_NEAR(dbs[5], floor_db, 1e-6);   // NaN -> floor
    ASSERT_NEAR(dbs[6], floor_db, 1e-6);   // Inf -> floor

    // 3. FFT Shift
    std::vector<int> orig = {0, 1, 2, 3, 4, 5};
    auto shifted = SpectrumAnalyzer::fftshift(orig);
    // orig: [0, 1, 2, 3, 4, 5] -> shifted: [3, 4, 5, 0, 1, 2]
    ASSERT_TRUE(shifted.size() == 6);
    ASSERT_TRUE(shifted[0] == 3 && shifted[1] == 4 && shifted[2] == 5 &&
                shifted[3] == 0 && shifted[4] == 1 && shifted[5] == 2);

    // 4. Bin to Frequency mapping and round-trip
    double fs = 1e6;
    size_t fft_sz = 1024;
    double df = fs / static_cast<double>(fft_sz);

    // Bin 0 should be -fs/2
    ASSERT_NEAR(SpectrumAnalyzer::bin_to_frequency(0, fft_sz, fs), -500000.0, 1e-6);
    // Bin 512 (N/2) should be 0 Hz
    ASSERT_NEAR(SpectrumAnalyzer::bin_to_frequency(512, fft_sz, fs), 0.0, 1e-6);
    // Bin 1023 should be 500000 - df
    ASSERT_NEAR(SpectrumAnalyzer::bin_to_frequency(1023, fft_sz, fs), 500000.0 - df, 1e-6);

    // Frequency to bin mapping
    ASSERT_TRUE(SpectrumAnalyzer::frequency_to_bin(-500000.0, fft_sz, fs) == 0);
    ASSERT_TRUE(SpectrumAnalyzer::frequency_to_bin(0.0, fft_sz, fs) == 512);
    ASSERT_TRUE(SpectrumAnalyzer::frequency_to_bin(100000.0, fft_sz, fs) == static_cast<int>(512 + std::round(100000.0 / df)));

    // Round-trip test
    for (double f = -450000.0; f <= 450000.0; f += 50000.0) {
        int b = SpectrumAnalyzer::frequency_to_bin(f, fft_sz, fs);
        double f_rec = SpectrumAnalyzer::bin_to_frequency(b, fft_sz, fs);
        ASSERT_NEAR(f_rec, f, df); // within one bin
    }

    // 5. Sliding-window Spectrogram and decimation
    SampleVector samples(4096, Complex(0.0, 0.0));
    // Exact on-bin tone at bin 100 (frequency 100 * df = 97656.25 Hz)
    double on_bin_freq = 100.0 * df;
    for (size_t i = 0; i < samples.size(); ++i) {
        double phase = 2.0 * PI * on_bin_freq * static_cast<double>(i) / fs;
        samples[i] = Complex(std::cos(phase), std::sin(phase));
    }

    SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(samples, 1024, 512, fs, -160.0);
    // Number of windows for 4096 with N=1024, H=512 is (4096 - 1024)/512 + 1 = 7 windows
    ASSERT_TRUE(spec.num_windows == 7);
    ASSERT_TRUE(spec.num_bins == 1024);
    ASSERT_TRUE(spec.power_db.size() == 7);

    // With sum(w^2) normalization, on-bin peak power is 10 * log10( (N/2)^2 / (3/8*N) )
    // = 10 * log10( (2/3) * N ) = 10 * log10( (2/3) * 1024 ) ~= 28.342 dB
    double expected_peak_db = 10.0 * std::log10((2.0 / 3.0) * static_cast<double>(fft_sz));
    int target_bin = SpectrumAnalyzer::frequency_to_bin(on_bin_freq, 1024, fs);
    for (size_t w = 0; w < spec.num_windows; ++w) {
        double peak_val = spec.power_db[w][target_bin];
        ASSERT_NEAR(peak_val, expected_peak_db, 0.01);
    }

    // Test Decimation to <= 512 bins
    auto decimated = SpectrumAnalyzer::decimate_bins(spec.power_db, 512);
    ASSERT_TRUE(decimated.size() == 7);
    ASSERT_TRUE(decimated[0].size() == 512);
}
