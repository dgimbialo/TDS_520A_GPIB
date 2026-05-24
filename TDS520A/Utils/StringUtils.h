// Utils/StringUtils.h
#pragma once
#include "pch.h"

namespace StringUtils
{
    std::string  WideToUtf8(const std::wstring& wide);
    std::wstring Utf8ToWide(const std::string& utf8);
    std::string  WideToAnsi(const std::wstring& wide);
    std::wstring AnsiToWide(const std::string& ansi);

    // Trim whitespace / CRLF from SCPI responses
    std::string  TrimRight(const std::string& s);
    std::string  Trim(const std::string& s);

    // Split string
    std::vector<std::string> Split(const std::string& s, char delim);

    // Format bytes as hex string
    std::string HexDump(const uint8_t* data, size_t len, size_t maxBytes = 64);

    // Safe integer parse (returns false on failure)
    bool ParseInt(const std::string& s, int& out);
    bool ParseDouble(const std::string& s, double& out);
}
