// Waveform/WaveformDecoder.h - Converts WaveformBuffer -> DecodedWaveform
#pragma once
#include "pch.h"
#include "WaveformBuffer.h"
#include "WaveformSample.h"

class WaveformDecoder
{
public:
    // Decode raw bytes using preamble scaling.
    // Returns false if buffer is invalid.
    bool Decode(const WaveformBuffer& buf, DecodedWaveform& out);

private:
    int16_t ReadSample(const uint8_t* ptr, int byteWidth, bool isSigned, bool isBigEndian);
};
