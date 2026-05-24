// Utils/Logger.h - Thread-safe logging facility
#pragma once
#include "pch.h"
#include <fstream>

enum class LogLevel : int
{
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Fatal   = 4
};

class Logger
{
public:
    // Singleton
    static Logger& Instance();

    void SetMinLevel(LogLevel level) { m_minLevel = level; }
    void EnableFileLog(const std::wstring& path);
    void DisableFileLog();

    void Log(LogLevel level, const wchar_t* category, const wchar_t* fmt, ...);

    // Convenience macros use these
    void Debug  (const wchar_t* cat, const wchar_t* fmt, ...);
    void Info   (const wchar_t* cat, const wchar_t* fmt, ...);
    void Warning(const wchar_t* cat, const wchar_t* fmt, ...);
    void Error  (const wchar_t* cat, const wchar_t* fmt, ...);
    void Fatal  (const wchar_t* cat, const wchar_t* fmt, ...);

    // Attach a callback so GUI can display log in real-time
    using LogCallback = std::function<void(LogLevel, const std::wstring&)>;
    void SetCallback(LogCallback cb);
    void ClearCallback();

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void WriteEntry(LogLevel level, const wchar_t* category, const std::wstring& message);

    std::mutex       m_mutex;
    LogLevel         m_minLevel{ LogLevel::Debug };
    std::wofstream   m_file;
    LogCallback      m_callback;
};

// ---- Convenience macros ----
#define LOG_DBG(cat, fmt, ...)  Logger::Instance().Log(LogLevel::Debug,   L##cat, fmt, ##__VA_ARGS__)
#define LOG_INF(cat, fmt, ...)  Logger::Instance().Log(LogLevel::Info,    L##cat, fmt, ##__VA_ARGS__)
#define LOG_WRN(cat, fmt, ...)  Logger::Instance().Log(LogLevel::Warning, L##cat, fmt, ##__VA_ARGS__)
#define LOG_ERR(cat, fmt, ...)  Logger::Instance().Log(LogLevel::Error,   L##cat, fmt, ##__VA_ARGS__)
#define LOG_FAT(cat, fmt, ...)  Logger::Instance().Log(LogLevel::Fatal,   L##cat, fmt, ##__VA_ARGS__)
