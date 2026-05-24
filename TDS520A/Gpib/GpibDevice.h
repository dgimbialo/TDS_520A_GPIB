// Gpib/GpibDevice.h - Represents one NI-488.2 addressable device
#pragma once
#include "pch.h"

// GPIB status codes / error constants from NI-488.2
// ibsta bits
#define GPIB_ERR_NONE       0
// iberr values
#define GPIB_EDVR           0   // System error
#define GPIB_ECIC           1   // Function requires GPIB board to be CIC
#define GPIB_ENOL           2   // No Listeners on the GPIB
#define GPIB_EADR           3   // GPIB board not addressed correctly
#define GPIB_EARG           4   // Invalid argument
#define GPIB_ESAC           5   // GPIB board not System Controller
#define GPIB_EABO           6   // I/O operation aborted (timeout)
#define GPIB_ENEB           7   // Non-existent GPIB board
#define GPIB_EDMA           8   // DMA hardware error
#define GPIB_EOIP           10  // I/O operation started before previous completed
#define GPIB_ECAP           11  // No capability for operation
#define GPIB_EFSO           12  // File system error
#define GPIB_EBUS           14  // GPIB bus error
#define GPIB_ESTB           15  // Serial poll status byte lost
#define GPIB_ESRQ           16  // SRQ stuck in ON position
#define GPIB_ETAB           20  // Table problem

struct GpibDeviceInfo
{
    int    boardIndex{ 0 };    // GPIB-USB board index (usually 0)
    int    primaryAddr{ 1 };   // Primary GPIB address (1-30)
    int    secondaryAddr{ 0 }; // Secondary address (0 = none)
    int    sendEOI{ 1 };       // Assert EOI with last byte
    int    eotMode{ 0 };       // End-of-string mode
    int    eosByte{ 0x0A };    // EOS byte (LF)
    int    timeoutCode{ T10s }; // NI-488.2 timeout constant (T10s = 11)
};

// Error info returned by GPIB operations
struct GpibError
{
    int  ibsta{ 0 };
    int  iberr{ 0 };
    long ibcntl{ 0 };
    std::wstring message;

    bool IsError() const { return (ibsta & ERR) != 0; }
    static std::wstring ErrorCodeToString(int iberr);
};

class GpibDevice
{
public:
    GpibDevice();
    ~GpibDevice();

    // Open device using NI-488.2 ibdev()
    bool Open(const GpibDeviceInfo& info, GpibError& err);
    void Close();
    bool IsOpen() const { return m_ud >= 0; }

    // Raw write (SCPI command string, LF appended automatically)
    bool Write(const std::string& cmd, GpibError& err);

    // Raw read into pre-allocated buffer; returns bytes read
    int  Read(char* buf, int bufSize, GpibError& err);

    // Write then read (query)
    bool Query(const std::string& cmd, std::string& response, GpibError& err);

    // Binary block read (for CURVE? data)
    bool ReadBinaryBlock(std::vector<uint8_t>& data, GpibError& err);

    // Clear the device (GPIB SDC)
    bool Clear(GpibError& err);

    // Drain any pending bytes from the GPIB input buffer (does NOT reset the device)
    void DrainInput();

    // Serial Poll – returns status byte
    bool SerialPoll(uint8_t& statusByte, GpibError& err);

    // Set timeout (NI-488.2 T-constants: T1s=10, T10s=11, T30s=12)
    bool SetTimeout(int tCode, GpibError& err);

    const GpibDeviceInfo& GetInfo() const { return m_info; }
    int GetUd() const { return m_ud; }

private:
    void FillError(GpibError& err, const wchar_t* context) const;

    int            m_ud{ -1 };   // NI-488.2 device unit descriptor
    GpibDeviceInfo m_info;
};
