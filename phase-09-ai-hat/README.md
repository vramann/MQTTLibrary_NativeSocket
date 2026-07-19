# Phase 09 — AI HAT (Hailo) Inference

**Goal:** run real-time vision inference on the AI HAT/AI Kit (Hailo-8 = 26
TOPS on AI HAT+; Hailo-8L = 13 TOPS on the original AI Kit), understand the
compile-deploy model flow, and wire detections into your event/MQTT system.

**Prereq:** Phase 8 (camera pipeline). The HAT sits on the PCIe FPC connector
— seat the cable carefully, use the thermal pad/standoffs per the guide.

## Concepts
- Why an NPU: TOPS vs the A76 CPU; what runs on-device (quantized, compiled
  models in HEF format) vs on the CPU (pre/post-processing, NMS sometimes).
- Software stack layers: PCIe driver + firmware → HailoRT runtime (C/C++ API
  and CLI) → GStreamer/rpicam-apps integration → `picamera2`/Python examples.
- Model zoo vs custom models: pretrained HEFs (YOLO detection, pose,
  segmentation) vs compiling your own with the Hailo Dataflow Compiler (DFC)
  — quantization, calibration datasets.
- System thinking: camera lores stream → inference → overlay/main stream;
  keeping the whole pipeline zero-ish-copy.

## Tasks
1. **Install & verify**: `sudo apt install hailo-all`, reboot; verify with
   `hailortcli fw-control identify` and `lspci` (the Hailo shows up as a PCIe
   device — same lesson as RP1: it's all PCIe on this board).
2. **First inference, zero code**: `rpicam-hello` with a Hailo object-detection
   post-processing stage (see rpicam-apps post-processing docs) — live YOLO
   boxes on the preview.
3. **hailo-rpi5-examples**: clone the repo, run the detection and pose
   pipelines; read the Python enough to identify: where frames enter, where
   the HEF is loaded, where detections come out as structured data.
4. **Your own consumer**: modify/replace the example callback so detections
   become JSON events (`class`, `confidence`, `bbox`, timestamp) published to
   MQTT via your library (bridge via a small local socket or a C consumer —
   your design call; justify it).
5. **Measure**: FPS and per-stage latency at 640×640 vs full-res input;
   CPU load with/without the HAT doing the heavy lifting; power draw if you
   can measure it (`vcgencmd pmic_read_adc`).
6. **HailoRT C/C++ API taste**: build one HailoRT C example that loads a HEF
   and runs inference on a static image — so you know what the Python layers
   wrap and could embed inference in a C daemon if the capstone wants it.

## Stretch
- Retrain/compile a custom model: fine-tune a small YOLO on a custom class,
  walk the DFC flow (ONNX → quantize with calibration set → HEF). This is a
  serious mini-project; budget accordingly.
- Two models pipelined (detect → classify crop), or detection + tracking
  (SORT/ByteTrack on CPU).
- Compare against CPU-only inference (e.g. YOLO via ONNX Runtime on the A76s)
  to quantify what the NPU buys.

## Checkpoint
- What is a HEF and why can't the Hailo run your ONNX file directly?
- What does quantization do to the model and why does the DFC need a
  calibration dataset?
- In your pipeline, which steps still run on CPU, and which would you attack
  first if end-to-end latency had to halve?
- Why does the AI HAT need the Pi 5 specifically (think Phase 0's board tour)?
