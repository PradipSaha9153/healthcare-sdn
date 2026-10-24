# Intelligent Healthcare Monitoring using SDN, IoT, and Machine Learning

[![Python 3.10+](https://img.shields.io/badge/python-3.10%2B-blue.svg)](https://www.python.org/)
[![OpenFlow 1.3](https://img.shields.io/badge/OpenFlow-1.3-orange.svg)](https://opennetworking.org/)
[![MQTT QoS 1](https://img.shields.io/badge/MQTT-QoS%201-green.svg)](https://mqtt.org/)
[![SQLite WAL](https://img.shields.io/badge/SQLite-WAL%20Mode-lightgrey.svg)](https://www.sqlite.org/wal.html)
[![ML Accuracy](https://img.shields.io/badge/ML%20Accuracy-95.33%25-brightgreen.svg)]()
[![Tests Passing](https://img.shields.io/badge/tests-22%20passed-brightgreen.svg)]()

> **Notice:** Research & Academic Demonstration Platform. Not a Certified Medical Device.

---

## 1. Project Overview & The Closed-Loop Architecture
In conventional hospital networks, medical telemetry (ECG, blood oxygen saturation, heart rate) shares network infrastructure with non-critical data. During periods of network congestion, packet loss and jitter can delay critical medical alerts and endanger patient lives.

This project implements an **autonomous, closed-loop cyber-physical healthcare system**:
1. **IoT Telemetry Streaming:** WBAN wearable sensor node (hardware-ready schema matching an ESP32 FreeRTOS node) streams 360 Hz batched ECG and 1 Hz vitals over MQTT with QoS 1.
2. **Edge ML & Hybrid Diagnostic Inference:** Pan-Tompkins live R-peak detection + 9-feature Random Forest classifier achieving **95.33% test accuracy** for arrhythmia detection, coupled with deterministic clinical safety rules for $SpO_2$, heart rate, and body temperature.
3. **Temporal Debouncing:** Enforces a 2-consecutive-window persistence filter to eliminate false alarms from transient sensor artifacts or motion.
4. **Dynamic SDN QoS Allocation:** When an alert triggers, an asynchronous REST call to the **Ryu OpenFlow 1.3 Controller** installs a high-priority flow rule on **Open vSwitch (OVS)**, routing that patient's IP traffic to a dedicated Linux HTB Priority Queue (Queue 1: guaranteed 6–10 Mbps).
5. **Real-Time Clinical Dashboard:** A high-fidelity web dashboard displaying live 360 Hz Lead-II oscilloscope waveforms, dynamic database telemetry snapshots, context-aware clinical protocol recommendations, and interactive SDN network monitoring.
6. **Automatic Normalization:** When patient telemetry normalizes, the controller automatically clears the priority flow and reverts traffic to standard best-effort forwarding (Queue 0).

---

## 2. Repository Layout

```
MiniProject/
├── data/
│   ├── download_datasets.py        # MIT-BIH & BIDMC downloader with synthetic fallback
│   ├── synthetic_generator.py      # 360 Hz McSharry ECG and multi-parameter generator
│   ├── scenario_engine.py          # Config-driven timeline engine with ground truth
│   └── scenarios.json              # Timeline definitions for P101, P102, P103
├── ml/
│   ├── features.py                 # 9-feature morphological & HRV extractor + Pan-Tompkins
│   ├── train.py                    # Random Forest training with strict inter-patient split
│   ├── evaluate.py                 # Comprehensive evaluation (Accuracy, ROC-AUC, Confusion Matrix)
│   ├── inference_service.py        # Live MQTT streaming hybrid inference & SDN trigger
│   └── models/
│       ├── ecg_rf_model.joblib     # Serialized trained model (95.33% accuracy)
│       └── model_metadata.json     # Training metadata & feature names
├── controller/
│   ├── topology.py                 # Mininet topology with 10 Mbps bottleneck TCLink
│   ├── ryu_healthcare_controller.py# Ryu OpenFlow 1.3 L2 switch + QoS REST WSGI app
│   └── setup_ovs_queues.sh         # Linux HTB QoS queue setup script (Queue 0 & 1)
├── gateway/
│   └── replay_publisher.py         # Hardware-ready WBAN telemetry replay node
├── db/
│   ├── models.py                   # SQLAlchemy schema with composite time-range indexes
│   ├── session.py                  # Database session factory with SQLite WAL mode
│   ├── seed.py                     # Seeds virtual patients, vitals, and metrics
│   └── retention.py                # Telemetry retention cleanup & WAL checkpointing
├── dashboard/
│   ├── app.py                      # Production Flask server with auth, CSRF, REST API, SSE
│   ├── static/
│   │   ├── css/style.css           # Glassmorphism dark/light theme stylesheet
│   │   └── js/
│   │       ├── chart.umd.min.js    # Locally vendored Chart.js (Zero CDN dependencies)
│   │       └── dashboard.js        # Continuous Lead-II oscilloscope engine & live UI logic
│   └── templates/                  # Jinja2 templates (live, alerts, history, network, admin)
├── eval/
│   ├── benchmark_network.py        # Ping latency, throughput, loss benchmark suite
│   ├── score_detection_delay.py    # Scores anomaly onset vs alert vs flow setup
│   └── results/                    # Real plots, CSVs, and markdown summaries
├── scripts/
│   ├── run_all.py                  # Universal orchestrator with auto-cleanup of ports
│   ├── run_all.sh / stop_all.sh    # Linux/WSL2 startup & shutdown scripts
│   ├── run_all.ps1 / stop_all.ps1  # Windows PowerShell scripts
│   ├── local_broker.py             # Embedded zero-dependency asyncio MQTT broker
│   └── controller_service.py       # Standalone controller REST simulation service
├── tests/
│   ├── test_schema_and_db.py       # DB models, WAL mode, retention tests
│   ├── test_scenario_and_replay.py # Scenario timeline & publisher contract tests
│   ├── test_ml_and_rules.py        # Feature extractor & clinical threshold tests
│   ├── test_controller_rest.py     # Ryu REST API & flow event persistence tests
│   ├── test_dashboard_routes.py    # Flask dashboard endpoints & session tests
│   └── test_closed_loop_e2e.py     # Full end-to-end closed-loop integration test
├── docs/
│   ├── SETUP_GUIDE.md              # Installation walkthrough (Windows & WSL2)
│   ├── WSL2_SETUP.md               # Mininet + OVS + Ryu configuration in WSL2
│   ├── ARCHITECTURE.md             # System & Sequence Mermaid diagrams
│   ├── ER_DIAGRAM.md               # Database schema & indexing strategy
│   ├── ML_LIMITATIONS.md           # Clinical & regulatory limitations report
│   ├── TROUBLESHOOTING.md          # Diagnostic table for common errors
│   └── VIVA_CHEATSHEET.md          # Q&A defense cheat-sheet for viva evaluations
├── .env.example / .env             # Environment configuration template
├── requirements.txt                # Python dependencies (Backend, ML, Dashboard)
└── requirements-ryu.txt            # Dedicated Python 3.9 dependencies for Ryu
```

---

## 3. Quickstart: Run in 3 Commands

### Step 1: Install Dependencies
```bash
pip install -r requirements.txt
```

### Step 2: Run Automated Verification Suite (22 Tests)
```bash
python -m pytest tests/ -v
```

### Step 3: Start the Complete System
```bash
python scripts/run_all.py
```
> *`run_all.py` includes automatic pre-flight port cleanup (`cleanup_ports`) to prevent socket collisions on ports 1883, 8080, and 5000, along with clean recursive process tree termination on `Ctrl+C`.*

Open your browser to:
- **Live Clinical Dashboard:** [http://127.0.0.1:5000](http://127.0.0.1:5000) (Login: `admin` / `admin123`)
- **SDN Controller REST Status:** [http://127.0.0.1:8080/api/v1/priority/status](http://127.0.0.1:8080/api/v1/priority/status)

---

## 4. Verified Evaluation Results

### Machine Learning Performance (Independent Held-Out Test Patients - DS2)
The model was evaluated strictly on held-out patients (`synth_201` to `synth_205`) with **zero patient overlap** (strict inter-patient split adhering to AAMI guidelines):

| Metric | Value | Technical Context |
| :--- | :---: | :--- |
| **Accuracy** | **95.33%** | Strict inter-patient test split ($N=300$ samples) |
| **Specificity** | **97.04%** | Normal sinus rhythm true negative rate ($262 / 270$) |
| **Sensitivity (Recall)** | **80.00%** | Arrhythmia detection rate ($24 / 30$) |
| **Precision** | **75.00%** | Positive predictive value ($24 / 32$) |
| **F1-Score** | **77.42%** | Harmonic mean of precision and recall |
| **ROC-AUC** | **0.9673** | High discriminative threshold separation |
| **Inference Latency** | **~0.15 ms** | Sub-millisecond Random Forest decision speed |

#### Confusion Matrix ($N = 300$ Test Samples):
- **True Negatives (TN):** `262` (Normal beats correctly identified)
- **False Positives (FP):** `8`
- **False Negatives (FN):** `6`
- **True Positives (TP):** `24` (Ectopic ventricular beats correctly flagged)

*Visual artifacts generated by `ml/evaluate.py`:*
- Confusion Matrix: `eval/results/confusion_matrix.png`
- ROC Curve: `eval/results/roc_curve.png`
- Metrics Export: `eval/results/ml_metrics.json`

---

### Closed-Loop Alert & SDN Response Delay
Evaluated via `eval/score_detection_delay.py` comparing ground-truth anomaly injection against live OpenFlow 1.3 flow installation:

| Patient & Anomaly Event | Anomaly Onset ($T_{gt}$) | Alert Detected ($T_{alert}$) | Detection Delay | SDN Flow Setup | Total Closed-Loop Reaction |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **P101: Sinus Tachycardia + Fever** | 15.0 s | 16.0 s | **1.00 s** | **3.33 ms** | **1.003 s** |
| **P102: Severe Acute Hypoxemia** | 20.0 s | 21.0 s | **1.00 s** | **3.90 ms** | **1.004 s** |
| **P103: Ventricular Arrhythmia (PVC)** | 12.0 s | 13.0 s | **1.00 s** | **3.69 ms** | **1.004 s** |

- **Mean Detection Delay:** **1.00 s** *(governed by the 2-window debounce filter to eliminate false alarms)*
- **Mean SDN Flow-Setup Latency:** **3.64 ms** *(OpenFlow 1.3 barrier-confirmed flow installation)*
- **Mean Total Closed-Loop Reaction Time:** **1.004 s**

---

### Network QoS Performance (Under 10 Mbps Bottleneck Congestion)
Measurements conducted under saturated background traffic (iperf UDP cross-traffic):

| Metric | Priority OFF (Best-Effort Queue 0) | Priority ON (SDN Queue 1) | Improvement |
| :--- | :---: | :---: | :---: |
| **End-to-End Latency** | `48.2 ± 3.5 ms` | `3.8 ± 0.4 ms` | **92.1% reduction** |
| **Jitter** | `12.4 ms` | `1.18 ms` | **90.5% reduction** |
| **Packet Loss Rate** | `4.20%` | `0.00%` | **Zero loss guaranteed** |
| **Effective Throughput**| `3.8 Mbps` | `9.6 Mbps` | **Guaranteed HTB allocation** |

---

## 5. Key System Highlights & Engineering Decisions

### 1. High-Fidelity Continuous Lead-II Oscilloscope Engine
- **Mathematical Continuity:** Replaced discrete sample index snapping with a continuous physiological Lead-II generator based on McSharry Gaussian wave packets.
- **Anatomical Wave Morphology:** Models crisp P-waves ($+0.16\text{ mV}$), sharp QRS complexes with high-amplitude R-peaks ($+1.35\text{ mV}$), asymmetric T-waves, and intermittent premature ventricular contractions (PVC bigeminy/trigeminy with discordant T-waves and compensatory pauses).
- **Physical Sweep Calibration:** Sweep engine uses `performance.now()` delta-time rendering calibrated to standard $25\text{ mm/s}$ clinical monitor sweep speeds ($\approx 135\text{ px/s}$).
- **Seamless In-Place Alarm Transitions:** Critical alarm state changes seamlessly switch trace stroke color to emergency glowing red (`#EF4444`) on-the-fly at the sweep beam's exact coordinates—**without clearing the canvas, blinking, or resetting cursor position**.

### 2. Fully Dynamic Database-Backed Alert Inspector
- **Real-Time Telemetry Snapshots:** When an alert is selected in `/alerts`, Heart Rate, $SpO_2$, Blood Pressure ($BP_{sys} / BP_{dia}$), Core Temp, and Mean Arterial Pressure (MAP) are dynamically pulled from the exact database record in the `vitals` table matching that alert's timestamp.
- **Context-Aware Visual Highlights:** Cards independently turn glowing red/amber only when their specific physiological threshold is breached.
- **Adaptive Clinical Protocols:** Clinical action guidelines automatically tailor to the diagnosis (STAT 12-lead ECG and Amiodarone for arrhythmia; antipyretic hydration for tachycardia/fever; oxygen titration for hypoxemia).
- **SDN QoS Indicator:** Real-time badge dynamically displays Queue 1 priority state when active and Queue 0 (Best-Effort) when acknowledged.

### 3. Fault-Tolerant Process Lifecycle & Port Management
- **Pre-Flight Port Sanitation:** `run_all.py` proactively scans ports `1883`, `8080`, and `5000` at startup, automatically terminating orphaned background instances to eliminate `[WinError 10048]` socket errors.
- **Recursive Process Tree Cleanup:** On shutdown (`Ctrl+C`), child and grandchild processes across all six services are terminated cleanly via process tree signals (`taskkill /F /T /PID` on Windows).

---

## 6. Documentation Index
- [Architecture & Sequence Diagrams](docs/ARCHITECTURE.md)
- [Database Schema & ER Diagram](docs/ER_DIAGRAM.md)
- [Viva Defense Cheat-Sheet](docs/VIVA_CHEATSHEET.md)
- [Clinical & ML Limitations Disclosure](docs/ML_LIMITATIONS.md)
- [WSL2 Ubuntu Mininet Setup Guide](docs/WSL2_SETUP.md)
- [Troubleshooting & Diagnostics](docs/TROUBLESHOOTING.md)
