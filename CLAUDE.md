# CLAUDE.md — UDS (ISO 14229) Client Stack over Linux Kernel ISO-TP

This document is the implementation plan and working guide for a **UDS
(Unified Diagnostic Services, ISO 14229-1) client/tester stack** that uses the
**Linux kernel ISO-TP implementation (`CAN_ISOTP` sockets, ISO 15765-2)** for
CAN communication. It is a new, standalone activity; it does not build on or
reference any existing codebase.

## Locked decisions

These were confirmed with the project owner and must not be silently changed:

| Decision        | Value                                                                 |
|-----------------|-----------------------------------------------------------------------|
| UDS role        | Client (tester) only — no server/ECU-side dispatcher                  |
| Language        | C++17, Linux only                                                     |
| Transport       | Kernel ISO-TP sockets (`socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)`) — no userspace ISO-TP reimplementation |
| Service scope   | Core session/data, security & communication, routine & I/O control, upload/download (flashing) |
| Project shape   | Standalone project at the repository root, independent of any other code in the repo |

If a requirement is ambiguous during implementation (e.g. an OEM-specific
seed/key algorithm, DID layouts, addressing scheme of a target ECU), **ask the
user — do not assume.**

## System prerequisites

- Linux kernel with `CONFIG_CAN_ISOTP` (module `can-isotp`; part of mainline
  since 5.10). Verify with `modprobe can_isotp` and presence of
  `<linux/can/isotp.h>`.
- `iproute2` (`ip link`) for creating `vcan` interfaces in development and CI.
- CMake ≥ 3.16, a C++17 compiler (GCC ≥ 9 or Clang ≥ 10).
- GoogleTest for unit/integration tests (fetched by CMake, not vendored).

Development happens against a virtual CAN interface:

```sh
sudo modprobe vcan can_isotp
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

## Architecture

Three layers, mapped to the ISO documents they implement:

```
+--------------------------------------------------------------+
| Application layer (ISO 14229-1)                              |
|   UdsClient — typed service APIs, request/response codecs,   |
|   NRC handling, DID/RID registries, transfer sequencing      |
+--------------------------------------------------------------+
| Session layer (ISO 14229-2)                                  |
|   Timing (P2_client / P2*_client), 0x78 responsePending      |
|   handling, S3 TesterPresent keep-alive, session state       |
+--------------------------------------------------------------+
| Transport layer (ISO 15765-2 — provided by the kernel)       |
|   IsoTpSocket — RAII wrapper over CAN_ISOTP sockets:         |
|   addressing, padding, flow-control opts, CAN FD, timeouts   |
+--------------------------------------------------------------+
```

Key design rules:

- **The kernel does segmentation.** All ISO-TP framing (single/first/
  consecutive/flow-control frames, STmin, block size) is delegated to the
  kernel via `SOL_CAN_ISOTP` socket options. The transport layer's job is
  only configuration, blocking/timeout semantics, and error mapping.
- **One physical connection = one socket pair.** A physically addressed
  connection binds `can_addr.tp.tx_id` / `can_addr.tp.rx_id`. Functional
  addressing uses a separate TX-only socket opened with
  `CAN_ISOTP_SF_BROADCAST` (single-frame only, per ISO 15765-4); responses
  arrive on the physical sockets.
- **No exceptions across the public API for protocol outcomes.** A negative
  response (NRC) is a normal, expected result, returned as a value
  (`Result<T>` with a `UdsError` carrying the NRC or transport errno).
  Exceptions are reserved for programming errors.
- **Blocking API first.** Synchronous request/response with timeouts derived
  from P2/P2*; the only background activity is the optional TesterPresent
  keep-alive thread. No async/executor framework in scope.
- **OEM-specific logic is pluggable.** Seed→key computation (0x27),
  authentication providers (0x29), and DID/RID codecs are user-supplied
  callbacks/interfaces; the stack never hardcodes OEM algorithms.

### Transport layer details (`IsoTpSocket`)

- `socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)` + `bind()` with `sockaddr_can`
  (`can_ifindex`, `tp.tx_id`, `tp.rx_id`; `CAN_EFF_FLAG` for 29-bit IDs).
- Configurable via `setsockopt`:
  - `CAN_ISOTP_OPTS` (`can_isotp_options`): TX/RX padding
    (`CAN_ISOTP_TX_PADDING`, pad byte, typically 0xCC or 0xAA per OEM),
    extended addressing (`CAN_ISOTP_EXTEND_ADDR`, `ext_address` /
    `rx_ext_address`), `CAN_ISOTP_WAIT_TX_DONE`, `CAN_ISOTP_SF_BROADCAST`.
  - `CAN_ISOTP_RECV_FC` (`can_isotp_fc_options`): advertised BS, STmin, WFTmax.
  - `CAN_ISOTP_LL_OPTS` (`can_isotp_ll_options`): `mtu = CANFD_MTU`, `tx_dl`
    (e.g. 64) and TX flags for CAN FD.
- Receive timeouts implemented with `poll()` so P2/P2* deadlines are enforced
  per response, not per socket.
- Addressing schemes supported: normal 11-bit, normal fixed 29-bit
  (ISO 15765-4 physical/functional), extended addressing. Mixed addressing is
  out of scope unless requested.

### Session layer details (`UdsSession`)

- Timing parameters with ISO 14229-2 defaults, all overridable:
  `p2_client` (default 500 ms wall timeout around P2_server_max = 50 ms),
  `p2_star_client` (default 5 s), `s3_client` (default 2 s TesterPresent
  period, must stay < S3_server = 5 s).
- Request cycle: send → wait ≤ P2 → on NRC 0x78 (responsePending) extend the
  deadline to P2* and keep waiting, repeatedly, until a final response or a
  configurable pending-response cap is hit.
- Validates response correlation: positive response SID = request SID + 0x40;
  negative response is `7F <SID> <NRC>`. Mismatched SIDs are rejected.
- Optional keep-alive: background thread sending TesterPresent (0x3E) with
  suppressPosRspMsgIndicationBit (sub-function 0x80) every `s3_client` while a
  non-default session is active; suspended during 0x36 TransferData bursts.

### Application layer details (`UdsClient` + services)

- Every service gets: a typed request builder, a typed response parser, and a
  raw escape hatch (`rawRequest(std::vector<uint8_t>) -> Result<Response>`).
- Full NRC table from ISO 14229-1 Annex A.1 as an enum with `to_string`
  (0x10 generalReject … 0x7F serviceNotSupportedInActiveSession).
- Sub-function byte handling includes the suppressPosRspMsgIndicationBit
  (bit 7) wherever the standard allows it.

## Directory layout

Standalone tree at the repository root; nothing links against or includes any
other code present in the repository.

```
uds/
├── CMakeLists.txt              # standalone project: `project(uds_client)`
├── include/uds/
│   ├── transport/IsoTpSocket.hpp
│   ├── transport/IsoTpConfig.hpp     # ids, padding, FC, FD, addressing
│   ├── session/UdsSession.hpp
│   ├── session/Timing.hpp
│   ├── core/Result.hpp               # Result<T> / UdsError / Nrc enum
│   ├── core/Bytes.hpp                # byte-buffer reader/writer helpers
│   ├── services/…                    # one header per service group
│   └── UdsClient.hpp                 # facade
├── src/                              # mirrors include/uds
├── examples/                         # read_vin, dtc_report, flash_demo
├── tests/
│   ├── unit/                         # codecs, NRC, timing logic (no CAN)
│   ├── integration/                  # against vcan0 + EcuStub
│   └── support/EcuStub.{hpp,cpp}     # scripted ISO-TP responder (test fixture only)
└── scripts/setup_vcan.sh
```

`EcuStub` is a minimal scripted responder used exclusively as a test fixture
(canned request→response mappings, programmable delays and 0x78 sequences).
It is not a UDS server implementation and stays inside `tests/`.

## Implementation phases

Every phase ends with code that compiles, is unit- and integration-tested
against `vcan0`, and is demonstrable through an example or test. No phase is
preparatory: each one ships working diagnostic functionality.

### Phase 1 — Working client core: transport, session timing, 0x10 / 0x3E

**Delivers a usable tester that can open a session on an ECU.**

- `IsoTpSocket` with bind/connect semantics, all socket options above,
  `poll()`-based timed receive, errno→error mapping (`ECOMM` = FC timeout,
  `ETIMEDOUT`, `EAGAIN`).
- `Result<T>` / `UdsError` / full `Nrc` enum; byte reader/writer.
- `UdsSession` request cycle with P2/P2*, 0x78 handling, SID correlation.
- Services: **0x10 DiagnosticSessionControl** (parses P2/P2* from the
  response and applies them to the session timing) and **0x3E TesterPresent**
  including the keep-alive thread.
- `EcuStub` fixture with scripted responses, delays, and 0x78 sequences.
- Example: `session_demo` — enters extended session on `vcan0` and holds it
  via TesterPresent.
- CMake project, warnings-as-errors, clang-format config, CI job that loads
  `vcan`/`can_isotp` and runs all tests.

**Acceptance:** integration test enters a non-default session against
`EcuStub`, survives a 3×0x78-then-positive response, and times out correctly
when the stub stays silent.

### Phase 2 — ECU state & communication control: 0x11, 0x28, 0x85

**Delivers reset and communication-management capability.**

- **0x11 ECUReset** (hardReset, keyOffOnReset, softReset,
  enable/disableRapidPowerShutDown; powerDownTime parsing).
- **0x28 CommunicationControl** (enable/disable RX/TX per communicationType,
  optional nodeIdentificationNumber variants 0x04/0x05).
- **0x85 ControlDTCSetting** (on/off, optional DTCSettingControlOptionRecord).
- Functional addressing support in the transport (broadcast socket +
  collection of responses on physical sockets) — first exercised here because
  0x28/0x85/0x3E are the classic functionally-addressed services.

**Acceptance:** integration tests for each service including NRC paths
(0x12, 0x22, 0x7E/0x7F); functional TesterPresent reaches two `EcuStub`
instances on distinct address pairs.

### Phase 3 — Data & DTC services: 0x22, 0x2E, 0x14, 0x19

**Delivers the day-to-day diagnostic read/write workflow.**

- **0x22 ReadDataByIdentifier** with multi-DID requests and a response
  splitter driven by a DID registry (user registers DID → length/codec;
  unknown DIDs fall back to single-DID requests returning raw bytes).
- **0x2E WriteDataByIdentifier.**
- **0x14 ClearDiagnosticInformation** (3-byte groupOfDTC, e.g. 0xFFFFFF).
- **0x19 ReadDTCInformation** — sub-functions 0x01 (numberOfDTCByStatusMask),
  0x02 (DTCByStatusMask), 0x04 (snapshotRecordByDTCNumber), 0x06
  (extDataRecordByDTCNumber), 0x0A (supportedDTC); typed `Dtc` value
  (3-byte code + status byte, formatted `P/C/B/U` style).
- Example: `read_vin` (DID 0xF190) and `dtc_report`.

**Acceptance:** round-trip tests for multi-DID read, DTC list parsing against
scripted stub payloads, and NRC coverage (0x31 requestOutOfRange, 0x33).

### Phase 4 — Security: 0x27 SecurityAccess, 0x29 Authentication

**Delivers access to protected services.**

- **0x27 SecurityAccess**: requestSeed/sendKey pairs for arbitrary levels
  (odd/even sub-function pairs), zero-seed = already-unlocked handling,
  pluggable `SeedKeyAlgorithm` interface (the stack ships only a test/demo
  algorithm; real algorithms are injected by the user), NRC 0x35/0x36/0x37
  surfaced with retry-delay awareness.
- **0x29 Authentication**: APCE (authentication with PKI certificate
  exchange) flow — verifyCertificateUnidirectional, proofOfOwnership,
  deAuthenticate — behind an `AuthenticationProvider` interface so crypto
  and certificate handling are injectable; only the message
  framing/sequencing is implemented by the stack. Confirm with the user
  which 0x29 sub-functions their ECU actually uses before implementing
  beyond APCE.
- Convenience: `unlock(level)` helper chaining seed→compute→key with
  correct sequencing rules (NRC 0x24 requestSequenceError paths tested).

**Acceptance:** unlock succeeds against stub implementing a reference
seed/key; invalid key path returns 0x35 then 0x36 after configured attempts;
0x37 delay is honored.

### Phase 5 — Routine & I/O control: 0x31, 0x2F

**Delivers actuator tests and ECU-side routines.**

- **0x31 RoutineControl**: startRoutine / stopRoutine / requestRoutineResults
  with RID + routineControlOptionRecord in, routineStatusRecord out; optional
  RID registry mirroring the DID registry; polling helper for long-running
  routines (start → poll requestResults until routineStatus indicates done).
- **0x2F InputOutputControlByIdentifier**: returnControlToECU /
  resetToDefault / freezeCurrentState / shortTermAdjustment with
  controlOptionRecord and controlEnableMaskRecord support.

**Acceptance:** integration test drives a stub routine through
start→pending→results, and an I/O override through
shortTermAdjustment→returnControlToECU.

### Phase 6 — Upload/Download: 0x34, 0x35, 0x36, 0x37 + flashing helper

**Delivers reprogramming-grade data transfer.**

- **0x34 RequestDownload / 0x35 RequestUpload**: dataFormatIdentifier
  (compression/encryption nibbles), addressAndLengthFormatIdentifier,
  arbitrary-width address/size encoding; parses
  lengthFormatIdentifier + maxNumberOfBlockLength from the response.
- **0x36 TransferData**: block sequence counter (starts at 1, wraps
  0xFF→0x00), chunking to maxNumberOfBlockLength minus protocol overhead,
  NRC 0x73 wrongBlockSequenceCounter recovery (retry same block once),
  suppression of TesterPresent interleaving during bursts.
- **0x37 RequestTransferExit** with optional transferResponseParameterRecord.
- High-level `DataTransfer` helper: download(memory region, data source,
  progress callback) / upload(...) orchestrating 0x34/0x36/0x37 with proper
  error unwinding; CAN FD (`tx_dl=64`) exercised here for throughput.
- Example: `flash_demo` — transfers a file to `EcuStub` and verifies content.

**Acceptance:** multi-megabyte transfer over `vcan0` completes with correct
BSC wrap-around; injected 0x73 and mid-transfer 0x78 are recovered; aborted
transfer leaves the client able to start a fresh 0x34 sequence.

### Phase 7 — Multi-ECU workflows, robustness, and release quality

**Delivers the stack as a finished, documented product.**

- Concurrent connections to multiple ECUs (one `UdsClient` per address pair,
  thread-safety audit: each client single-threaded-by-contract, keep-alive
  thread interaction documented and race-tested with TSan).
- Fault-injection test suite: interface down mid-request, FC starvation
  (`ECOMM`), truncated stub responses, garbage SIDs, zero-length reads.
- ASan/UBSan/TSan CI jobs; fuzz target for all response parsers
  (libFuzzer, parsers must never crash on arbitrary bytes).
- Doxygen API docs, top-level README with a quick-start against `vcan0`,
  packaging (`find_package(uds_client)` via installed CMake config).

**Acceptance:** full test suite green under all sanitizers; fuzzers run clean
for a fixed CI budget; a scripted two-ECU demo (session + read + unlock +
routine) passes.

## Testing strategy

- **Unit tests (no CAN needed):** codecs/builders/parsers for every service,
  NRC mapping, timing state machine (with injected fake clock), byte
  reader/writer bounds checks.
- **Integration tests (require `vcan0` + `can_isotp`):** every service tested
  against `EcuStub` for the positive path, at least two NRC paths, timeout,
  and 0x78-pending path. Tests self-skip with a clear message if the kernel
  module or a `vcan` interface is unavailable.
- **CI:** container/runner must load `vcan` and `can_isotp` and create
  `vcan0` before the test step (`scripts/setup_vcan.sh`).

## Conventions

- C++17, no exceptions across the public API for protocol results, no RTTI
  requirements, no third-party runtime dependencies (test-only: GoogleTest).
- All multi-byte protocol fields are big-endian on the wire; use the
  `Bytes` reader/writer, never raw pointer casts.
- Naming: `PascalCase` types, `camelCase` methods, `snake_case` files inside
  `src/`, service constants named exactly as in ISO 14229-1
  (e.g. `kDiagnosticSessionControl = 0x10`).
- Every commit keeps the full test suite green; each phase lands as a
  reviewable series of commits, not one monolith.

## Out of scope (do not implement unless the user asks)

- UDS server/ECU-side stack (the `EcuStub` test fixture is not one).
- DoIP (ISO 13400), LIN, FlexRay, or any non-CAN transport.
- Userspace ISO-TP implementation or raw-CAN fallback.
- OEM-specific seed/key or certificate algorithms (interfaces only).
- Services not listed in the phases (0x24, 0x2A, 0x2C, 0x38, 0x83, 0x84, 0x86, 0x87).
