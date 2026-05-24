// Utils/Logger.cpp
#include "pch.h"
#include "Logger.h"
#include <cstdarg>
#include <ctime>

Logger& Logger::Instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::~Logger()
{
    if (m_file.is_open())
        m_file.close();
}

void Logger::EnableFileLog(const std::wstring& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open())
        m_file.close();
    m_file.open(path, std::ios::app | std::ios::out);
}

void Logger::DisableFileLog()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open())
        m_file.close();
}

void Logger::SetCallback(LogCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(cb);
}

void Logger::ClearCallback()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = nullptr;
}

void Logger::Log(LogLevel level, const wchar_t* category, const wchar_t* fmt, ...)
{
    if (level < m_minLevel) return;

    wchar_t buf[2048];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
    va_end(args);

    WriteEntry(level, category, buf);
}

static const wchar_t* LevelStr(LogLevel l)
{
    switch (l)
    {
    case LogLevel::Debug:   return L"DBG";
    case LogLevel::Info:    return L"INF";
    case LogLevel::Warning: return L"WRN";
    case LogLevel::Error:   return L"ERR";
    case LogLevel::Fatal:   return L"FAT";
    default:                return L"???";
    }
}

void Logger::WriteEntry(LogLevel level, const wchar_t* category, const std::wstring& message)
{
    // Timestamp
    wchar_t tsBuf[32];
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    _snwprintf_s(tsBuf, _countof(tsBuf), _TRUNCATE,
        L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::wstring entry = std::wstring(tsBuf)
        + L" [" + LevelStr(level) + L"] "
        + L"[" + (category ? category : L"") + L"] "
        + message;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Debug output
    ::OutputDebugStringW((entry + L"\n").c_str());

    // File
    if (m_file.is_open())
    {
        m_file << entry << L"\n";
        m_file.flush();
    }

    // GUI callback
    if (m_callback)
        m_callback(level, entry);
}
