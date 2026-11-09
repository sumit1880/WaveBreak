#include "test_harness.hpp"
#include <iostream>

void run_fft_tests();
void run_spectrum_analyzer_tests();
void run_signal_generator_tests();
void run_config_loader_tests();
void run_cli_and_io_tests();
void run_detector_and_evaluator_tests();

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  WAVEBREAK - TEST SUITE RUN   " << std::endl;
    std::cout << "========================================" << std::endl;

    run_fft_tests();
    run_spectrum_analyzer_tests();
    run_signal_generator_tests();
    run_config_loader_tests();
    run_cli_and_io_tests();
    run_detector_and_evaluator_tests();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Total assertions: " << test_harness::g_test_count << std::endl;
    std::cout << "Failures:         " << test_harness::g_fail_count << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    if (test_harness::g_fail_count == 0) {
        std::cout << ">>> ALL TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
        return 0;
    } else {
        std::cerr << ">>> SOME TESTS FAILED! <<<" << std::endl;
        return 1;
    }
}
