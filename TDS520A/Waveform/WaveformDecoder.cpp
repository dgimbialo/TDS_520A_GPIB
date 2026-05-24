// Waveform/WaveformDecoder.cpp
#include "pch.h"
#include "WaveformDecoder.h"
#include "Logger.h"

bool WaveformDecoder::Decode(const WaveformBuffer& buf, DecodedWaveform& out)
{
    if (!buf.IsValid())
    {
        LOG_ERR("Decoder", L"Invalid WaveformBuffer passed to Decode");
        return false;
    }

    const WaveformPreamble& pre = buf.Preamble;
    const int byteWidth = pre.byteWidth;
    const int nSamples  = static_cast<int>(buf.RawData.size()) / byteWidth;

    if (nSamples <= 0)
    {
        LOG_ERR("Decoder", L"Zero samples after raw data / byteWidth division");
        return false;
    }

    out.Samples.resize(static_cast<size_t>(nSamples));
    out.SequenceNumber = buf.SequenceNumber;
    out.Channel        = buf.Channel;
    out.SecPerDiv      = pre.secPerDiv;
    out.VoltsPerDiv    = pre.voltsPerDiv;

    float xMin = FLT_MAX, xMax = -FLT_MAX;
    float yMin = FLT_MAX, yMax = -FLT_MAX;

    const uint8_t* ptr = buf.RawData.data();
    for (int i = 0; i < nSamples; ++i)
    {
        int16_t raw = ReadSample(ptr + i * byteWidth, byteWidth,
                                  pre.isSigned, pre.isBigEndian);

        float t = static_cast<float>(pre.ToTime(i));
        float v = static_cast<float>(pre.ToVoltage(raw));

        out.Samples[i] = { t, v };

        if (t < xMin) xMin = t;
        if (t > xMax) xMax = t;
        if (v < yMin) yMin = v;
        if (v > yMax) yMax = v;
    }

    out.XMin = xMin;
    out.XMax = xMax;
    out.YMin = yMin;
    out.YMax = yMax;

    LOG_DBG("Decoder", L"Decoded %d samples. V=[%g..%g] T=[%g..%g]",
        nSamples, yMin, yMax, xMin, xMax);
    return true;
}

int16_t WaveformDecoder::ReadSample(const uint8_t* ptr, int byteWidth,
                                     bool isSigned, bool isBigEndian)
{
    if (byteWidth == 1)
    {
        if (isSigned)
            return static_cast<int16_t>(static_cast<int8_t>(ptr[0]));
        else
            return static_cast<int16_t>(ptr[0]);
    }
    else  // byteWidth == 2
    {
        uint16_t raw;
        if (isBigEndian)
            raw = (static_cast<uint16_t>(ptr[0]) << 8) | ptr[1];
        else
            raw = (static_cast<uint16_t>(ptr[1]) << 8) | ptr[0];

        if (isSigned)
            return static_cast<int16_t>(raw);
        else
            return static_cast<int16_t>(raw);  // Caller uses as needed
    }
}
