# TDS 520A GPIB — Copilot Instructions

## Memory System

This project uses a persistent AI memory system at `D:\ai_memory_system\`.

**At the start of every session**, load project memory:
```
python D:\ai_memory_system\run.py --project TDS_520A_GPIB list_memory
```

**After every file edit**, save a memory entry:
```
python D:\ai_memory_system\run.py --project TDS_520A_GPIB add_memory --type <bug_fix|feature|note|decision> --files <file> --description "<what changed and why>" --confidence 1.0
```

**At the end of every session**, create a summary:
```
python D:\ai_memory_system\run.py --project TDS_520A_GPIB session_summary --description "<brief summary>"
```

---

## Critical Project Facts (from memory)

### Hardware
- NI-488.2 board index: **17** (`GPIB17`) — NOT 0
- Tektronix TDS 520A scope GPIB address: **17**
- GPIB address 0 is reserved for the controller

### Waveform Preamble (`WFMPRE?`)
- TDS 520A returns **positional semicolon-separated** fields, NOT key=value
- Field indices: `BYT_NR=0, BN_FMT=1, BYT_OR=2, NR_PT=3, XINCR=8, PT_OFF=9, YMULT=12, YZERO=13, YOFF=14`

### Thread Safety Rules
- **Never call GPIB functions from HTTP/WebSocket threads** — always enqueue via `m_cmdQueue`
- **Never hold `m_wsMutex` while sending frames** — snapshot clients first, send outside the lock
- `AcquisitionThread::SetScope(&m_scope)` must be called before `Start()`

### Web UI JSON Protocol
- Waveform: `{"type":"waveform", "samples":[...voltages...], "yMin", "yMax", "secPerDiv", "voltsPerDiv"}`
- Status: `{"type":"status", "idn", "channel", "voltsPerDiv", "acqRate"}`

### Build
- `pch.h` must include `<queue>` (for `std::queue` in `HttpServer`) and `<afxcmn.h>` (for `CSpinButtonCtrl`)

---

## Architecture

```
CTds520AApp
  ??? TektronixScope        (GPIB communication, waveform fetch)
  ??? AcquisitionThread     (continuous fetch loop ? WaveformRingBuffer)
  ??? HttpServer            (embedded HTTP + WebSocket, non-blocking)
  ?     ??? CommandDispatchLoop  (dedicated thread for web commands)
  ?     ??? BroadcastWaveform    (snapshot clients, send outside lock)
  ??? WaveformRingBuffer    (lock-free waveform queue)
  ??? ConnectDialog         (MFC modal: board=17, addr=17, scan from startAddr)
```
