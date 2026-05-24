// HttpServer/HtmlGenerator.h - Generates the Web UI as C++ strings
// No external CDN. All CSS/JS is inline.
#pragma once
#include "pch.h"
#include "WaveformSample.h"
#include "ScopeChannel.h"

namespace HtmlGen
{
    // Full page HTML for the main oscilloscope viewer
    std::string MainPage(const std::string& localIp, uint16_t wsPort);

    // JSON status response
    std::string StatusJson(
        const std::string& idn,
        ScopeChannel ch,
        double voltsPerDiv,
        double secPerDiv,
        uint32_t acqRate,
        bool connected);

    // JSON waveform data (compressed: only send voltage values as array)
    std::string WaveformJson(const DecodedWaveform& wave);
}
