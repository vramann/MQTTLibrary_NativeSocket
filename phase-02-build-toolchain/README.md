# Phase 02 — C/C++ Toolchain: Native and Cross

**Goal:** a professional build/debug workflow for the Pi: build natively for
quick experiments, cross-compile from your workstation for real projects, and
debug remotely with gdb. You already know C — this phase is about the *Pi/Linux
workflow*, not the language.

## Concepts
- Native builds on the Pi (aarch64) vs cross-compiling (`aarch64-linux-gnu-`).
- CMake toolchain files; sysroots and why you need one for cross-linking
  against Pi libraries (libgpiod, etc.).
- Remote debugging: `gdbserver` on the Pi, `gdb-multiarch` on the host.
- Sanitizers and `valgrind` on aarch64.

## Tasks
1. **Native baseline**: on the Pi, `sudo apt install build-essential cmake gdb`;
   build a hello-world with plain `gcc`, then with a minimal `CMakeLists.txt`.
2. **Cross-compile from your workstation**: install
   `gcc-aarch64-linux-gnu` (or use a Docker image); write a CMake toolchain
   file; build the same program; `scp` it over and run it.
3. **Sysroot**: rsync `/usr/include` + `/usr/lib` from the Pi to the host,
   point the toolchain file at it, and cross-link against a Pi-installed
   library (you'll use libgpiod in Phase 3 — set this up now:
   `sudo apt install libgpiod-dev gpiod` on the Pi).
4. **Remote gdb**: run the program under `gdbserver :2345 ./app` on the Pi,
   connect from the host with `gdb-multiarch`, set breakpoints, inspect.
5. **Editor integration**: set up VS Code Remote-SSH (or your editor's
   equivalent) against the Pi, plus a second workflow that's pure
   cross-compile + deploy script. Know both; pick your favourite.
6. **Deploy script**: a `deploy.sh` (rsync + restart binary) you'll reuse in
   every later phase.
7. **Sanity project**: a small multithreaded C program (pthread producer/
   consumer) built both ways, run under `-fsanitize=thread` once, and under
   gdb remotely once.

## Stretch
- Build with clang for aarch64; compare warnings.
- Try `perf top` / `perf record` on the Pi on your producer/consumer program.
- Set up `ccache` and measure rebuild speedup on the Pi.

## Checkpoint
- When cross-linking, why does the *host* linker need the Pi's libraries, and
  what exactly does the sysroot provide?
- What travels over the wire between `gdb-multiarch` and `gdbserver`?
- Which triplet is your cross-compiler, and why does the Pi 5 use aarch64
  rather than armhf?
