// Waveform/WaveformBuffer.h - Raw binary waveform from instrument + preamble
#pragma once
#include "pch.h"
#include "WaveformPreamble.h"
#include "ScopeChannel.h"

struct WaveformBuffer
{
    WaveformPreamble  Preamble;
    std::vector<uint8_t> RawData;    // Raw binary ADC bytes from CURVE?
    ScopeChannel      Channel{ ScopeChannel::CH1 };
    std::chrono::steady_clock::time_point Timestamp;
    uint32_t          SequenceNumber{ 0 };

    bool IsValid() const { return Preamble.IsValid() && !RawData.empty(); }
    void Clear()
    {
        RawData.clear();
        Preamble = WaveformPreamble{};
    }
};
