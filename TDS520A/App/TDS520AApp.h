// App/TDS520AApp.h - MFC Application class
#pragma once
#include "pch.h"
#include "TektronixScope.h"
#include "AcquisitionThread.h"
#include "HttpServer.h"
#include "RingBuffer.h"

class CTds520AApp : public CWinApp
{
public:
    CTds520AApp();

    // Access the global scope instance
    TektronixScope&    GetScope()         { return m_scope; }
    AcquisitionThread* GetAcqThread()     { return m_acqThread.get(); }
    HttpServer&        GetHttpServer()    { return m_httpServer; }
    WaveformRingBuffer& GetRingBuffer()  { return m_ringBuf; }

    void StartAcquisition(ScopeChannel ch = ScopeChannel::CH1);
    void StopAcquisition();
    bool ConnectScope();                        // Auto-detect and connect
    bool ConnectScope(int board, int addr);     // Connect to a specific address
    void DisconnectScope();
    void SetCursorEnabled(bool en) { m_cursorEnabled = en; }
    bool IsCursorEnabled() const  { return m_cursorEnabled; }

    // MFC overrides
    virtual BOOL InitInstance() override;
    virtual int  ExitInstance() override;

private:
    // Read HorScale + ChannelScale from the scope right after connect and
    // store them in the scope's atomic cache so the first rendered frame
    // already shows the correct V/div and s/div.
    void SyncSettingsFromScope();

    TektronixScope             m_scope;
    WaveformRingBuffer         m_ringBuf;
    std::unique_ptr<AcquisitionThread> m_acqThread;
    HttpServer                 m_httpServer;
    bool                       m_cursorEnabled{ false };

    DECLARE_MESSAGE_MAP()
};

extern CTds520AApp theApp;
