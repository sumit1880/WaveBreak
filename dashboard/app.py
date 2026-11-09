"""
Spectrum Sentinel Dashboard
----------------------------
Reads exported files from results/<scenario>/ only.
Computes NO detections — all data comes from the C++ core.

Run:  streamlit run dashboard/app.py
"""

import json
import os

import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
import streamlit as st

# ---------------------------------------------------------------------------
# Page config
# ---------------------------------------------------------------------------
st.set_page_config(
    page_title="Spectrum Sentinel",
    page_icon="📡",
    layout="wide",
)

st.title("📡 Spectrum Sentinel – Interference Detection Dashboard")
st.caption(
    "**Synthetic simulator only** — not real RF detection. "
    "All data is read from C++ core output files."
)

# ---------------------------------------------------------------------------
# Sidebar: pick results folder
# ---------------------------------------------------------------------------
RESULTS_ROOT = os.path.join(os.path.dirname(__file__), "..", "results")
RESULTS_ROOT = os.path.normpath(RESULTS_ROOT)

scenarios = []
if os.path.isdir(RESULTS_ROOT):
    scenarios = sorted(
        d for d in os.listdir(RESULTS_ROOT)
        if os.path.isdir(os.path.join(RESULTS_ROOT, d))
    )

with st.sidebar:
    st.header("Settings")
    if not scenarios:
        st.warning(f"No results found in:\n`{RESULTS_ROOT}`\n\nRun `simulate` or `evaluate` first.")
        st.stop()

    scenario = st.selectbox("Scenario", scenarios)
    folder = os.path.join(RESULTS_ROOT, scenario)

    st.markdown("---")
    st.markdown(
        "**Files read:**\n"
        "- `samples.csv` + `samples_meta.json`\n"
        "- `spectrogram.csv`\n"
        "- `events.json`\n"
        "- `metrics.json`\n"
        "- `ground_truth.json`\n"
        "- `../summary.csv`"
    )

# ---------------------------------------------------------------------------
# Helper: load file or return None with a warning
# ---------------------------------------------------------------------------

def load_csv(path: str) -> pd.DataFrame | None:
    if not os.path.isfile(path):
        st.warning(f"File not found: `{path}`")
        return None
    return pd.read_csv(path)


def load_json(path: str) -> dict | list | None:
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


# ---------------------------------------------------------------------------
# Load data
# ---------------------------------------------------------------------------
samples_df = load_csv(os.path.join(folder, "samples.csv"))
meta = load_json(os.path.join(folder, "samples_meta.json"))
spec_df = load_csv(os.path.join(folder, "spectrogram.csv"))
events = load_json(os.path.join(folder, "events.json"))
metrics = load_json(os.path.join(folder, "metrics.json"))
gt = load_json(os.path.join(folder, "ground_truth.json"))
summary_df = load_csv(os.path.join(RESULTS_ROOT, "summary.csv"))

fs = float(meta["sample_rate_hz"]) if meta and "sample_rate_hz" in meta else 1e6
n_samples = int(meta["num_samples"]) if meta and "num_samples" in meta else (len(samples_df) if samples_df is not None else 0)
duration_s = n_samples / fs

# ---------------------------------------------------------------------------
# Tabs
# ---------------------------------------------------------------------------
tab_waveform, tab_spectrum, tab_spectrogram, tab_events, tab_metrics, tab_summary = st.tabs(
    ["Waveform", "Spectrum", "Spectrogram", "Events", "Metrics", "Summary"]
)

# ── Waveform ────────────────────────────────────────────────────────────────
with tab_waveform:
    st.subheader("I/Q Waveform")
    if samples_df is not None and "i" in samples_df.columns and "q" in samples_df.columns:
        max_pts = 8192
        step = max(1, len(samples_df) // max_pts)
        ds = samples_df.iloc[::step].copy()
        t = [i * step / fs for i in range(len(ds))]
        fig = go.Figure()
        fig.add_trace(go.Scatter(x=t, y=ds["i"].values, name="I (real)", line=dict(width=1)))
        fig.add_trace(go.Scatter(x=t, y=ds["q"].values, name="Q (imag)", line=dict(width=1)))
        fig.update_layout(
            xaxis_title="Time (s)",
            yaxis_title="Amplitude",
            height=350,
            margin=dict(t=20, b=40),
        )
        st.plotly_chart(fig, use_container_width=True)
        st.caption(f"Showing every {step}th sample ({len(ds):,} of {n_samples:,} plotted). fs = {fs/1e3:.0f} kHz, duration = {duration_s:.3f} s")
    else:
        st.info("No samples data available.")

# ── Spectrum (single FFT of first window) ───────────────────────────────────
with tab_spectrum:
    st.subheader("Power Spectrum – First Window")
    if spec_df is not None and len(spec_df) > 0:
        n_bins = spec_df.shape[1]
        first_row = spec_df.iloc[0].values.astype(float)
        # Reconstruct frequency axis from number of bins
        freqs_khz = [(b - n_bins / 2) * (fs / n_bins) / 1e3 for b in range(n_bins)]

        fig = go.Figure()
        fig.add_trace(go.Scatter(
            x=freqs_khz, y=first_row, mode="lines", name="Power (dB)",
            line=dict(width=1.2),
        ))

        # Overlay detected peaks from events
        if events:
            for ev in events:
                f_khz = ev.get("frequency_hz", ev.get("center_frequency_hz", None))
                if f_khz is not None:
                    f_khz /= 1e3
                    fig.add_vline(
                        x=f_khz,
                        line_dash="dash",
                        line_color="red",
                        annotation_text=f"Det {f_khz:.1f} kHz",
                        annotation_position="top right",
                    )

        fig.update_layout(
            xaxis_title="Frequency (kHz)",
            yaxis_title="Power (dB)",
            height=350,
            margin=dict(t=20, b=40),
        )
        st.plotly_chart(fig, use_container_width=True)
    else:
        st.info("No spectrogram data available.")

# ── Spectrogram ─────────────────────────────────────────────────────────────
with tab_spectrogram:
    st.subheader("Spectrogram Heatmap")
    if spec_df is not None and len(spec_df) > 0:
        n_windows, n_bins = spec_df.shape
        fft_hop = int(meta.get("fft_hop", 512)) if meta else 512
        t_axis = [w * fft_hop / fs for w in range(n_windows)]
        f_axis_khz = [(b - n_bins / 2) * (fs / n_bins) / 1e3 for b in range(n_bins)]
        z = spec_df.values.astype(float)

        fig = go.Figure(data=go.Heatmap(
            z=z,
            x=f_axis_khz,
            y=t_axis,
            colorscale="Viridis",
            colorbar=dict(title="dB"),
        ))
        fig.update_layout(
            xaxis_title="Frequency (kHz)",
            yaxis_title="Time (s)",
            height=450,
            margin=dict(t=20, b=40),
        )
        st.plotly_chart(fig, use_container_width=True)
        st.caption(f"{n_windows} windows × {n_bins} bins. FFT hop = {fft_hop} samples.")
    else:
        st.info("No spectrogram data available.")

# ── Events ──────────────────────────────────────────────────────────────────
with tab_events:
    st.subheader("Detected Events")
    if not events:
        st.success("No interference events detected.")
    else:
        rows = []
        for ev in events:
            freq = ev.get("frequency_hz", ev.get("center_frequency_hz", None))
            rows.append({
                "ID": ev.get("id", ""),
                "Start (s)": f"{ev.get('start_time_s', 0):.4f}",
                "End (s)": f"{ev.get('end_time_s', 0):.4f}",
                "Frequency": f"{freq/1e3:.1f} kHz" if freq is not None else ev.get("band", ""),
                "Category": ev.get("spectral_category", ""),
                "Temporal": ev.get("temporal_flag", ""),
                "Severity": ev.get("severity", ""),
                "Margin (dB)": f"{ev.get('margin_db', 0):.1f}",
                "Reason": ev.get("reason", ""),
            })
        st.dataframe(pd.DataFrame(rows), use_container_width=True)

    if gt:
        with st.expander("Ground Truth Events"):
            st.json(gt)

# ── Metrics ─────────────────────────────────────────────────────────────────
with tab_metrics:
    st.subheader("Evaluation Metrics")
    if not metrics:
        st.info("No metrics.json found. Run `analyze` with ground truth present.")
    else:
        col1, col2, col3, col4 = st.columns(4)
        col1.metric("True Positives", metrics.get("tp", "—"))
        col2.metric("False Positives", metrics.get("fp", "—"))
        col3.metric("False Negatives", metrics.get("fn", "—"))
        f1 = metrics.get("f1")
        col4.metric("F1 Score", f"{f1:.3f}" if f1 is not None else "N/A")

        col5, col6 = st.columns(2)
        prec = metrics.get("precision")
        rec = metrics.get("recall")
        col5.metric("Precision", f"{prec:.3f}" if prec is not None else "N/A")
        col6.metric("Recall", f"{rec:.3f}" if rec is not None else "N/A")

        col7, col8 = st.columns(2)
        col7.metric("False Alarms / min", f"{metrics.get('false_alarms_per_minute', 0):.2f}")
        delay = metrics.get("mean_detection_delay_s")
        col8.metric("Mean Detection Delay", f"{delay:.3f} s" if delay is not None else "N/A")

        with st.expander("Window Confusion Matrix"):
            wc = metrics.get("window_confusion", {})
            if wc:
                wdf = pd.DataFrame(
                    [[wc.get("tp", 0), wc.get("fp", 0)],
                     [wc.get("fn", 0), wc.get("tn", 0)]],
                    index=["Predicted Positive", "Predicted Negative"],
                    columns=["Actual Positive", "Actual Negative"],
                )
                st.dataframe(wdf)

        with st.expander("Event-Level Type Confusion Matrix"):
            ec = metrics.get("event_type_confusion", {})
            if ec:
                st.json(ec)

# ── Summary ─────────────────────────────────────────────────────────────────
with tab_summary:
    st.subheader("Cross-Scenario Summary")
    if summary_df is not None:
        # Highlight F1 column
        numeric_cols = [c for c in summary_df.columns if c not in ("scenario",)]
        st.dataframe(
            summary_df.style.background_gradient(
                subset=[c for c in ["f1", "precision", "recall"] if c in summary_df.columns],
                cmap="RdYlGn",
                vmin=0, vmax=1,
            ),
            use_container_width=True,
        )

        # Bar chart: F1 by scenario
        if "f1" in summary_df.columns and "scenario" in summary_df.columns:
            plot_df = summary_df[summary_df["f1"].notna()].copy()
            if not plot_df.empty:
                fig = px.bar(
                    plot_df,
                    x="scenario",
                    y="f1",
                    color="f1",
                    color_continuous_scale="RdYlGn",
                    range_color=[0, 1],
                    title="F1 Score by Scenario",
                    labels={"f1": "F1 Score", "scenario": "Scenario"},
                )
                fig.update_layout(height=300, margin=dict(t=40, b=40))
                st.plotly_chart(fig, use_container_width=True)
    else:
        st.info("No summary.csv found. Run `evaluate` to generate it.")
