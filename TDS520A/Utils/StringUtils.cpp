// Utils/StringUtils.cpp
#include "pch.h"
#include "StringUtils.h"

namespace StringUtils
{

std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return {};
    int sz = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return {};
    std::string out(sz - 1, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &out[0], sz, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) return {};
    int sz = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (sz <= 0) return {};
    std::wstring out(sz - 1, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], sz);
    return out;
}

std::string WideToAnsi(const std::wstring& wide)
{
    if (wide.empty()) return {};
    int sz = ::WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return {};
    std::string out(sz - 1, '\0');
    ::WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &out[0], sz, nullptr, nullptr);
    return out;
}

std::wstring AnsiToWide(const std::string& ansi)
{
    if (ansi.empty()) return {};
    int sz = ::MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, nullptr, 0);
    if (sz <= 0) return {};
    std::wstring out(sz - 1, L'\0');
    ::MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, &out[0], sz);
    return out;
}

std::string TrimRight(const std::string& s)
{
    size_t end = s.size();
    while (end > 0 && (s[end-1] == ' ' || s[end-1] == '\t' ||
                       s[end-1] == '\r' || s[end-1] == '\n'))
        --end;
    return s.substr(0, end);
}

std::string Trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' ||
                                 s[start] == '\r' || s[start] == '\n'))
        ++start;
    size_t end = s.size();
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\t' ||
                           s[end-1] == '\r' || s[end-1] == '\n'))
        --end;
    return s.substr(start, end - start);
}

std::vector<std::string> Split(const std::string& s, char delim)
{
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim))
        result.push_back(token);
    return result;
}

std::string HexDump(const uint8_t* data, size_t len, size_t maxBytes)
{
    std::string out;
    out.reserve(maxBytes * 3);
    size_t count = std::min(len, maxBytes);
    for (size_t i = 0; i < count; ++i)
    {
        char buf[4];
        sprintf_s(buf, "%02X ", data[i]);
        out += buf;
    }
    if (len > maxBytes)
        out += "...";
    return out;
}

bool ParseInt(const std::string& s, int& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || errno == ERANGE) return false;
    out = static_cast<int>(v);
    return true;
}

bool ParseDouble(const std::string& s, double& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || errno == ERANGE) return false;
    out = v;
    return true;
}

} // namespace StringUtils
