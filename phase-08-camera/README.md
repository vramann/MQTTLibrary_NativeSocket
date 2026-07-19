# Phase 08 — Camera (libcamera stack)

**Goal:** capture stills and video on the Pi 5 with the modern libcamera
stack — CLI first, then Python (`picamera2`) for productivity, then a look at
the C++ API — and build the capture pipeline the AI phase will consume.

**Hardware:** Camera Module 3 (needs the 22-pin FPC cable for Pi 5's smaller
MIPI connectors; either of the two ports works).

**Reality check:** the old `raspistill`/`picamera` (v1) stack is gone.
Everything goes through **libcamera**: `rpicam-*` CLI apps, `picamera2`
(Python), or the libcamera C++ API. Old tutorials using `raspistill` or
OpenCV's V4L2 capture path directly will mislead you.

## Concepts
- MIPI CSI-2 → ISP pipeline: raw Bayer, ISP processing (AWB, AE, denoise),
  streams (main/lores/raw) and formats.
- libcamera architecture: camera manager, configurations, requests,
  completed-request callbacks — an async request/completion model much like
  other embedded frameworks you know.
- Video encode on Pi 5: **no hardware H.264 encoder** (unlike Pi 4) — encoding
  is software; H.265 *decode* is hardware. Plan CPU budget accordingly.

## Tasks
1. **CLI first**: `rpicam-hello` (verifies the whole stack), `rpicam-still`
   for stills (play with `--shutter`, `--gain`, `--awb`), `rpicam-vid` for
   H.264 video; stream to your PC (`rpicam-vid -t 0 --inline -o - | ...` via
   TCP, or the `--listen` option) and view with VLC/ffplay.
2. **Inspect the pipeline**: `rpicam-hello --list-cameras`; capture a raw DNG
   plus JPEG of the same scene; open the DNG and appreciate what the ISP did.
3. **picamera2 basics** (Python): scripted capture — timelapse with metadata
   (exposure, gain per frame) logged to JSON; then continuous capture into a
   numpy array and a trivial motion detector (frame differencing + threshold).
4. **OpenCV integration**: `picamera2` frame → OpenCV (`cv2`) for edge
   detection overlay, displayed via X-forwarding/VNC or saved to file;
   measure sustainable FPS at 640×480 vs 1920×1080.
5. **libcamera C++ taste**: build one of the libcamera examples (or
   `rpicam-apps` from source) and read the request/completion loop; you don't
   need to write a full C++ app — the goal is to understand what picamera2
   wraps, and where you'd drop to C++ if you had to.
6. **Camera-as-sensor service**: motion-detector publishes events to MQTT via
   your library (snapshot saved to disk, event JSON on `pi5/camera/motion`).
   Another integration slice banked for the capstone.

## Stretch
- Two cameras simultaneously (both MIPI ports), synced captures.
- Manual camera tuning: fixed exposure/AWB for a controlled scene; HDR mode
  on Camera Module 3.
- `libcamera` tracing/log levels to watch requests flow.

## Checkpoint
- What does the ISP do between the sensor's Bayer data and your JPEG?
- Why does Pi 5 video encoding load the CPU when Pi 4's didn't?
- In picamera2, what are main/lores streams for, and how will the lores
  stream matter for AI inference in the next phase?
- Where do the per-frame exposure/gain numbers come from in your timelapse
  metadata?
