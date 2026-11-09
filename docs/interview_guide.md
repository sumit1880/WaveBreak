# Interview Guide – WaveBreak

## Pipeline in One Paragraph

WaveBreak generates complex baseband signals (I/Q samples) in C++17 using a Mersenne-Twister
RNG and hand-written Box-Muller AWGN. Each scenario adds synthetic interference according to its
parameters, then writes the samples to CSV alongside a JSON ground-truth record. The analysis pipeline
divides the samples into overlapping windows (hop = 512 samples), applies a **periodic Hann window**,
runs a **radix-2 Cooley-Tukey FFT** (size 1024), and normalises the power spectrum by Σw² so that a
single-bin tone reads its true power regardless of window size. The resulting sliding spectrogram
(windows × bins, in dB with a floor) is fed window-by-window to the interference detector. The detector
estimates the per-window noise floor as `median(remaining_bin_powers) × (1/ln 2)` to correct for the
~1.6 dB downward bias of the median under exponential noise. It then classifies narrowband events
(≤ 5 contiguous bins above floor + 20 dB, with parabolic frequency interpolation) and broadband events
(≥ 1 contiguous sub-band span ≥ 50 kHz where trimmed-subband-mean > floor + 3 dB). Consecutive flagged
windows are merged (gap ≤ 2, min 2) into events, which the classifier labels with a spectral category,
temporal flag, and severity. The evaluator then matches detected events against ground truth using a
time-overlap and frequency-proximity rule, and computes standard retrieval metrics.

---

## 10 Project-Specific Q&As

### Q1 — What is the frequency resolution and what limits it?

**Frequency resolution = fs / N = 1 000 000 / 1024 ≈ 976.6 Hz/bin.**

It is set by the FFT size `N = 1024` and the sample rate `fs = 1 MHz`. Doubling `N` halves the
resolution but doubles latency and compute cost. The Hann window widens each spectral peak to about
2 bins, so the effective resolution is roughly 2 × 976.6 ≈ 1.95 kHz. Parabolic interpolation recovers
sub-bin accuracy for isolated tones.

---

### Q2 — What is the Nyquist limit and what happens if you violate it?

**Nyquist: |f| < fs/2 = 500 kHz.** The simulator rejects any tone frequency with |f| ≥ 500 kHz and
returns exit code 2 (usage/config error). Baseband complex (I/Q) sampling captures both positive and
negative frequencies, so the usable band is −500 kHz to +500 kHz. Aliasing would fold energy outside
this range back into the band and produce phantom interference; the simulator prevents this at the
input-validation stage.

---

### Q3 — What is the FFT's time complexity and why does it matter?

**Radix-2 Cooley-Tukey FFT: O(N log₂ N) vs. naive DFT: O(N²).**

For N = 1024: FFT ≈ 10 240 operations vs. DFT ≈ 1 048 576 — a 100× speedup. For N = 2^20 = 1 048 576
the ratio is > 20 000×. The benchmark confirms this: the full 2^20-sample pipeline runs in ~1382 ms
(median over 20 iterations, GCC 16.2.0 Release), dominated by the ≈ 2047 FFT calls needed for the
sliding spectrogram, not by any single FFT.

---

### Q4 — Why use a Hann window and not a rectangular one?

A rectangular window treats every sample as if the signal began and ended exactly at the frame boundary.
Discontinuities at the edges create spectral leakage: energy "bleeds" from a strong tone into
neighbouring bins, masking weaker signals. The **Hann window** (`w[n] = 0.5 − 0.5 cos(2πn/N)`) tapers
to zero at both edges, suppressing sidelobes to ≈ −31 dB (vs. −13 dB rectangular). The tradeoff is a
wider main lobe (~2 bins), which slightly reduces frequency resolution.

---

### Q5 — Why periodic Hann and not symmetric?

A **periodic** Hann window of length N has w[N] = w[0] = 0 when extended, making it perfectly periodic
with period N. This is the correct choice for spectral analysis because DFT implicitly assumes
periodicity. A **symmetric** (DFT-even) window of length N+1 is correct for FIR filter design. Using
the symmetric form for FFT analysis introduces a small phase error and breaks the overlap-add
reconstruction property.

---

### Q6 — Why multiply the median by 1/ln 2 to get the noise floor?

Under complex AWGN, the power of each frequency bin follows an **exponential distribution** with mean
σ². The median of an exponential distribution is `σ² ln 2 ≈ 0.693 σ²`, which is about 1.59 dB *below*
the mean. If we used the raw median as the noise floor, we would systematically under-estimate it and
get more false alarms. Multiplying by `1/ln 2 ≈ 1.4427` corrects for this bias, giving an unbiased
estimate of σ².

**Known limitation:** this correction assumes that fewer than 50% of bins contain interference power. If
broadband interference covers more than half the band, the median itself is corrupted and the floor
estimate is wrong.

---

### Q7 — Why is the detection threshold set to 20 dB, not the specified 13 dB?

The 13 dB default in the spec is a good starting point for real-world RF, but in this simulator the
"desired" tone at +100 kHz has amplitude 1.0 and 60 dB SNR (clean scenario). Its Hann-window sidelobes
reach roughly −45 dB relative to the peak, which translates to ~19 dB *above the noise floor* in the
clean scenario. With 13 dB threshold, those sidelobes triggered false alarms in every clean-scenario
run. Setting threshold_db = 20 dB clears all 90 clean/low_noise/high_noise runs (30 seeds × 3
scenarios) with zero false alarms while preserving F1 = 1.0 on all interference scenarios. This
threshold is documented as a parameter tuned to this simulator's SNR assumptions, not validated on
real hardware.

---

### Q8 — What is the precision/recall tradeoff and how is F1 defined?

- **Precision = TP / (TP + FP):** fraction of detected events that are real. High precision → few false
  alarms.
- **Recall = TP / (TP + FN):** fraction of real events that were detected. High recall → few missed
  events.
- **F1 = 2 · (Precision · Recall) / (Precision + Recall):** harmonic mean, penalises both extremes.

In this project, raising the threshold increases precision (fewer false alarms) but risks lowering recall
(might miss weak signals). All 8 interference scenarios achieve F1 = 1.000 across 30 seeds with the
current 20 dB threshold, meaning both are simultaneously perfect under these synthetic conditions.

---

### Q9 — Why don't perfect synthetic results prove real-world detection performance?

This simulator makes idealised assumptions that don't hold in practice:

1. **IID Gaussian noise** — real RF noise is coloured, non-stationary, and contains hardware artefacts.
2. **No multipath or Doppler** — real channels distort tone frequencies and add reflections.
3. **No ADC non-linearity or quantisation noise** — these raise the in-band noise floor unevenly.
4. **Known scenario labels** — the threshold was tuned against the same distribution it is tested on
   (seed sweep, not an independent dataset).
5. **Perfectly modelled interference** — real jammers use spread-spectrum, frequency hopping, etc.

WaveBreak is a signal-processing testbed and interview project, not a deployable RF system.

---

### Q10 — How does broadband interference detection work?

The detector divides the spectrum (excluding the desired-tone guard band) into **16 equal sub-bands**,
each approximately 1 MHz / 16 = 62.5 kHz wide at fs = 1 MHz. For each sub-band it computes a
**trimmed mean**: it removes the top 5 bin-power values before averaging. This prevents a single
narrowband spike inside a sub-band from falsely inflating the sub-band mean and triggering a broadband
alert.

A sub-band is flagged if `trimmed_mean > noise_floor + 3 dB`. Contiguous flagged sub-bands whose total
span ≥ 50 kHz are reported as a broadband event. The 3 dB threshold (vs. 20 dB for narrowband) is much
lower because broadband interference raises many bins simultaneously, not just one, so even modest
per-bin excess is significant.
