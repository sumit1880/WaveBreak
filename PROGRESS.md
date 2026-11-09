# Spectrum Sentinel – Progress

## Phase 0 — DONE ✓
Toolchain verified: g++ 16.2.0, cmake 4.4.5, ninja, git 2.47.1, python 3.13.15. Git repo initialised.

## Phase 1 — DONE ✓
FFT/IFFT (radix-2 C-T), Hann window, power spectrum, spectrogram, 8 signal scenarios with ground truth,
flat-JSON config loader, CSV/JSON I/O writers, CLI `simulate` command.
CTest: 5151 assertions passed. Gate verified: narrowband produces 4 output files; identical samples.csv
across two runs with the same seed.

## Phase 2 — DONE ✓
Interference detector (median noise floor × 1/ln2 bias, ±5-bin guard, narrowband parabolic interpolation,
broadband trimmed-mean 16-subband, merge gap 2 / min 2 windows), event classifier (spectral + temporal +
severity), detection evaluator (TP/FP/FN, precision, recall, F1, FA/min, delay, confusion matrices),
experiment runner (8 scenarios × 30 seeds → summary.csv + summary.json). Default threshold_db = 20.0 dB
(tuned for 0 false alarms on clean/low_noise/high_noise). CLI: analyze, evaluate, benchmark, demo.
CTest: 5212 assertions passed. evaluate → all 8 scenarios F1=1.000 (interference) or null/0 FA (clean).
Benchmark (GCC 16.2.0, Release): 2^16=20 ms, 2^18=86 ms, 2^20=1382 ms (median over 20 iterations).

## Phase 3 — IN PROGRESS
Dashboard (Streamlit), PowerShell scripts, README with Mermaid diagram + real results,
docs/interview_guide.md. Full clean-rebuild gate required.
