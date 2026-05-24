// Waveform/WaveformPreamble.h - Parses WFMPRE? response from TDS 520A
#pragma once
#include "pch.h"

// All scaling factors needed to convert raw ADC -> voltage/time
struct WaveformPreamble
{
    // Horizontal
    double xIncr{ 1.0 };    // Time per sample (seconds)
    double xZero{ 0.0 };    // Time of first sample
    int    ptOff{ 0 };       // Trigger point offset
    int    nrPt{ 0 };        // Number of points

    // Vertical
    double yMult{ 1.0 };    // Voltage scale factor
    double yZero{ 0.0 };    // Voltage offset (YZERO)
    double yOff{ 0.0 };     // Vertical offset in ADC counts (YOFF)

    // Encoding info
    int    byteWidth{ 1 };  // Bytes per sample (DATA:WIDTH)
    bool   isSigned{ true }; // RIBinary = signed
    bool   isBigEndian{ true };

    // Computed display info
    double voltsPerDiv{ 0.0 };
    double secPerDiv{ 0.0 };

    // Parse the ASCII WFMPRE? response string
    bool Parse(const std::string& response);

    // Parse WAVFRM? response: "<preamble_ascii>%<#NNN...binary_block>"
    // Splits on '%', parses the preamble part, and copies the binary data
    // into rawDataOut.  Returns false if the format is not recognised.
    bool ParseWavfrm(const std::string& response,
                     std::vector<uint8_t>& rawDataOut);

    // Convert raw ADC value to voltage
    inline double ToVoltage(int rawAdc) const
    {
        return (static_cast<double>(rawAdc) - yOff) * yMult + yZero;
    }

    // Convert sample index to time (seconds)
    inline double ToTime(int sampleIndex) const
    {
        return xZero + (sampleIndex - ptOff) * xIncr;
    }

    bool IsValid() const { return nrPt > 0 && xIncr > 0.0; }
};
