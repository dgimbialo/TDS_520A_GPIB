// Waveform/WaveformSample.h - Decoded waveform point
#pragma once
#include "pch.h"
#include "ScopeChannel.h"

struct WaveformSample
{
    float time;     // seconds
    float voltage;  // volts
};

// A decoded waveform ready for rendering
struct DecodedWaveform
{
    std::vector<WaveformSample> Samples;
    double                      XMin{ 0.0 };
    double                      XMax{ 0.0 };
    double                      YMin{ 0.0 };
    double                      YMax{ 0.0 };
    double                      SecPerDiv{ 0.0 };
    double                      VoltsPerDiv{ 0.0 };
    ScopeChannel                Channel{ ScopeChannel::CH1 };
    uint32_t                    SequenceNumber{ 0 };
};
