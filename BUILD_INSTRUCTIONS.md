# TDS 520A GPIB Oscilloscope — Build Instructions

## Prerequisites

### 1. Visual Studio 2022
- Install **Desktop development with C++** workload
- Ensure **MFC** component is installed:
  - In VS Installer → Modify → Individual components → search "MFC"
  - Install: "C++ MFC for latest v143 build tools (x86 & x64)"

### 2. NI-488.2 / NI-VISA Driver
- Install **NI-488.2** from National Instruments:
  https://www.ni.com/en-us/support/downloads/drivers/download.ni-488-2.html
- Default install path:
  ```
  C:\Program Files\National Instruments\Shared\ExternalCompilerSupport\C\
  ```
- Required files:
  - `include\Decl-32.h`   — NI-488.2 C declarations
  - `lib64\msvc\ni4882.lib` — 64-bit import library

### 3. Windows SDK
- Windows 10 SDK (10.0) is used in the project
- The app targets Windows 7 x64 at runtime (`_WIN32_WINNT=0x0601`)

---

## Build Steps

1. Open `TDS520A.sln` in Visual Studio 2022
2. Select configuration: **Release | x64**
3. Build → Build Solution (Ctrl+Shift+B)
4. Output EXE: `bin\Release\TDS520A.exe`

---

## Project Configuration Details

| Setting            | Value                        |
|--------------------|------------------------------|
| Platform           | x64                          |
| Character Set      | Unicode                      |
| MFC Use            | Static (`UseOfMfc = Static`) |
| Runtime Library    | `/MT` (Debug: `/MTd`)        |
| C++ Standard       | C++17                        |
| Target OS          | Windows 7+ (`_WIN32_WINNT=0x0601`) |

**All external runtime dependencies are statically linked** — only `ni4882.lib` 
(NI-488.2 DLL wrapper) is a runtime dependency.

---

## NI-488.2 Library Notes

The vcxproj links `ni4882.lib` and searches in:
```
$(ProgramFiles)\National Instruments\Shared\ExternalCompilerSupport\C\lib64\msvc
```

If your NI install is in a different path, update in:
- Project Properties → Linker → General → Additional Library Directories

At runtime, `ni4882.dll` must be present (installed by the NI-488.2 driver).

---

## Windows 7 Compatibility Notes

- `_WIN32_WINNT = 0x0601` and `NTDDI_VERSION = 0x06010000` restrict API use to Win7+
- `std::thread` on Windows 7 requires VS2022 CRT — handled by `/MT` static linking
- `GetAdaptersAddresses` (used in NetworkUtils) is available on Win7 SP1+
- No DirectX required — renderer uses pure GDI
- No WebView/Chromium — HTTP server is pure Winsock

---

## GPIB Hardware Setup

1. Connect Tektronix TDS 520A to NI GPIB-USB-HS (or similar)
2. Note the GPIB primary address (default: 1, check on front panel)
3. The app auto-scans addresses 1–30 on board 0 for a Tektronix IDN response

To override: change `GpibDeviceInfo.primaryAddr` in `TDS520AApp::ConnectScope()`

---

## Runtime Usage

1. Launch `TDS520A.exe`
2. **Scope → Connect** — auto-discovers oscilloscope
3. **Scope → Start Acquisition** — begins continuous waveform loop
4. Web UI: open browser to `http://<your-PC-IP>:8080/` from phone/tablet
5. Waveform data is pushed via WebSocket to all connected clients

**Log file**: `TDS520A.log` (created in EXE directory)

---

## Project Structure

```
TDS520A/
├── pch.h / pch.cpp          Precompiled header
├── resource.h               Resource IDs
├── res/TDS520A.rc            Menu, accelerators, strings
│
├── App/
│   ├── TDS520AApp.h/.cpp    CWinApp: initialization, owns scope/threads/server
│   ├── MainFrm.h/.cpp       CFrameWnd: menu, toolbar, status bar
│   └── OscilloscopeView.h/.cpp  CWnd: render loop, zoom/pan
│
├── Gpib/
│   ├── GpibDevice.h/.cpp    Low-level NI-488.2 ibdev/ibwrt/ibrd wrapper
│   └── GpibController.h/.cpp Auto-scan, open/close device
│
├── Scope/
│   ├── ScopeChannel.h       Channel enum (CH1..CH4)
│   ├── ScpiCommand.h/.cpp   SCPI command string builders
│   └── TektronixScope.h/.cpp High-level TDS 520A instrument class
│
├── Waveform/
│   ├── WaveformPreamble.h/.cpp  WFMPRE? parser (YMULT, YOFF, YZERO, XINCR...)
│   ├── WaveformBuffer.h/.cpp    Raw binary frame from CURVE?
│   ├── WaveformDecoder.h/.cpp   Raw ADC → voltage/time conversion
│   └── WaveformSample.h         DecodedWaveform struct
│
├── Renderer/
│   ├── RenderContext.h/.cpp ViewTransform (zoom/pan), colors, perf counters
│   └── WaveformRenderer.h/.cpp  GDI double-buffered oscilloscope renderer
│
├── HttpServer/
│   ├── HttpServer.h/.cpp    Accept loop + thread pool, route dispatcher
│   ├── WebSocketServer.h/.cpp  RFC 6455 upgrade + framing
│   ├── HttpRequestParser.h/.cpp  Minimal HTTP/1.1 parser
│   ├── HtmlGenerator.h/.cpp  Self-contained HTML/CSS/JS Web UI
│   ├── Sha1.h/.cpp          SHA-1 for WebSocket handshake
│   └── Base64.h/.cpp        Base64 encode/decode
│
├── Threads/
│   ├── AcquisitionThread.h/.cpp  Background GPIB acquisition loop
│   └── RingBuffer.h/.cpp    Lock-free SPSC ring buffer (power-of-2)
│
└── Utils/
    ├── Logger.h/.cpp         Thread-safe logger with file + callback
    ├── StringUtils.h/.cpp    Wide/UTF-8/ANSI conversion, trim, split
    └── NetworkUtils.h/.cpp   Winsock init, local IP, socket helpers
```

---

## Architecture Overview

```
[GPIB Hardware]
      |
[GpibDevice (NI-488.2)]
      |
[TektronixScope] -- SCPI: WFMPRE?, CURVE?
      |
[AcquisitionThread] ─── background thread
      |
[WaveformDecoder] -- ADC → voltage/time
      |
[WaveformRingBuffer] ─── lock-free SPSC
     / \
    /   \
[GUI Thread]    [HTTP Broadcast]
[OscilloscopeView]  [WebSocket clients]
[WaveformRenderer]  [Mobile browser]
```

---

## Extending the Project

| Feature | Where to add |
|---------|-------------|
| Multiple oscilloscopes | Add `TektronixScope` instances in `TDS520AApp`, additional `AcquisitionThread`s |
| Recording to disk | Add a consumer in `AcquisitionThread::SetCallback` |
| Remote clients beyond LAN | Add TLS layer before `WebSocketServer` |
| GPU acceleration | Replace `WaveformRenderer::DrawWaveform` with Direct2D or Direct3D 11 |
| Averaging/envelope mode | Modify `Scpi::AcqMode()` and handle in `TektronixScope::FetchWaveform` |
| Math channels | Add computed `DecodedWaveform` derived from CH1+CH2 in decoder layer |
