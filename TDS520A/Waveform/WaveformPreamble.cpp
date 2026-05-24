// Waveform/WaveformPreamble.cpp
#include "pch.h"
#include "WaveformPreamble.h"
#include "StringUtils.h"
#include "Logger.h"

// TDS 520A WFMPRE? response comes in two possible formats:
//
// A) Old-style positional (semicolon-separated, no keys) – actual TDS 520A output:
//    BYT_NR;BIT_NR;ENCDG;BN_FMT;BYT_OR;WFID;NR_PT;PT_FMT;XUNIT;XINCR;PT_OFF;YUNIT;YMULT;YZERO;YOFF
//    e.g.: 1;8;BIN;RI;MSB;"Ch1...";500;Y;"s";10.00E-6;250;"Volts";2.000E-3;0.0E+0;0.0E+0
//
// B) Keyword format (key<space>value pairs, semicolon-separated):
//    BYT_NR 1;BIT_NR 8;NR_PT 500;XINCR 1.0E-5;...

// Splits a semicolon-delimited string respecting quoted fields.
static std::vector<std::string> SplitPreamble(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    bool inQuote = false;
    for (char c : s)
    {
        if (c == '"')       { inQuote = !inQuote; cur += c; }
        else if (!inQuote && c == ';') { out.push_back(cur); cur.clear(); }
        else                { cur += c; }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool WaveformPreamble::Parse(const std::string& response)
{
    // Sanity: a valid WFMPRE response must start with a digit (BYT_NR in positional format)
    // or an uppercase letter (keyword format). Anything else is junk left in the buffer.
    if (response.empty() || (!std::isalnum(static_cast<unsigned char>(response[0]))))
    {
        LOG_WRN("WfmPre", L"Preamble starts with unexpected char, discarding");
        return false;
    }

    auto fields = SplitPreamble(response);
    if (fields.empty())
    {
        LOG_WRN("WfmPre", L"Empty preamble response");
        return false;
    }

    // Detect format: positional if the first field has no space (it would just be "1")
    // Keyword format has tokens like "BYT_NR 1".
    bool isPositional = (fields[0].find(' ') == std::string::npos);

    if (isPositional)
    {
        // Positional field indices (0-based):
        //  0 BYT_NR   1 BIT_NR   2 ENCDG   3 BN_FMT   4 BYT_OR
        //  5 WFID     6 NR_PT    7 PT_FMT   8 XUNIT    9 XINCR
        // 10 PT_OFF  11 YUNIT   12 YMULT   13 YZERO   14 YOFF
        // (Some firmware versions also have XZERO at index 10 and PT_OFF shifts right –
        //  handle both 15-field and 16-field variants)
        auto get = [&](size_t idx) -> std::string
        {
            if (idx >= fields.size()) return {};
            std::string v = StringUtils::Trim(fields[idx]);
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
                v = v.substr(1, v.size() - 2);
            return v;
        };

        // BYT_NR (index 0)
        { int w = 1; StringUtils::ParseInt(get(0), w); byteWidth = w; }

        // BN_FMT (index 3)
        { auto v = get(3); for (auto& c : v) c = static_cast<char>(::toupper(c)); isSigned = (v == "RI"); }

        // BYT_OR (index 4)
        { auto v = get(4); for (auto& c : v) c = static_cast<char>(::toupper(c)); isBigEndian = (v == "MSB"); }

        // NR_PT (index 6)
        StringUtils::ParseInt(get(6), nrPt);

        // XINCR (index 9)
        StringUtils::ParseDouble(get(9), xIncr);

        // Distinguish 15-field vs 16-field layout:
        // 15 fields: PT_OFF@10, YMULT@12, YZERO@13, YOFF@14
        // 16 fields: XZERO@10, PT_OFF@11, YMULT@13, YZERO@14, YOFF@15
        if (fields.size() >= 16)
        {
            StringUtils::ParseDouble(get(10), xZero);
            StringUtils::ParseInt   (get(11), ptOff);
            StringUtils::ParseDouble(get(13), yMult);
            StringUtils::ParseDouble(get(14), yZero);
            StringUtils::ParseDouble(get(15), yOff);
        }
        else
        {
            StringUtils::ParseInt   (get(10), ptOff);
            StringUtils::ParseDouble(get(12), yMult);
            StringUtils::ParseDouble(get(13), yZero);
            StringUtils::ParseDouble(get(14), yOff);
        }
    }
    else
    {
        // Keyword format: each field is "KEY VALUE"
        for (const auto& tok : fields)
        {
            auto t = StringUtils::Trim(tok);
            if (t.empty()) continue;

            auto sp = t.find(' ');
            if (sp == std::string::npos) continue;

            std::string key   = StringUtils::Trim(t.substr(0, sp));
            std::string value = StringUtils::Trim(t.substr(sp + 1));

            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);

            for (auto& c : key) c = static_cast<char>(::toupper(c));

            if      (key == "XINCR")  StringUtils::ParseDouble(value, xIncr);
            else if (key == "XZERO")  StringUtils::ParseDouble(value, xZero);
            else if (key == "PT_OFF") StringUtils::ParseInt(value, ptOff);
            else if (key == "NR_PT")  StringUtils::ParseInt(value, nrPt);
            else if (key == "YMULT")  StringUtils::ParseDouble(value, yMult);
            else if (key == "YZERO")  StringUtils::ParseDouble(value, yZero);
            else if (key == "YOFF")   StringUtils::ParseDouble(value, yOff);
            else if (key == "BYT_NR") { int w = 1; StringUtils::ParseInt(value, w); byteWidth = w; }
            else if (key == "BN_FMT")
            {
                for (auto& c : value) c = static_cast<char>(::toupper(c));
                isSigned = (value == "RI");
            }
            else if (key == "BYT_OR")
            {
                for (auto& c : value) c = static_cast<char>(::toupper(c));
                isBigEndian = (value == "MSB");
            }
        }
    }

    if (nrPt <= 0 || xIncr <= 0.0)
    {
        LOG_ERR("WfmPre", L"Invalid preamble: NR_PT=%d XINCR=%g", nrPt, xIncr);
        return false;
    }

    // Computed display parameters
    secPerDiv   = xIncr * nrPt / 10.0;           // 10 horizontal divisions
    voltsPerDiv = yMult * 25.6;                   // 256 ADC counts / 10 div

    LOG_DBG("WfmPre", L"Parsed: NR_PT=%d XINCR=%g YMULT=%g YOFF=%g YZERO=%g",
        nrPt, xIncr, yMult, yOff, yZero);
    return true;
}
