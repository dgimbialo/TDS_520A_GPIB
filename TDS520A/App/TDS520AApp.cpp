// App/TDS520AApp.cpp - MFC Application implementation
#include "pch.h"
#include "TDS520AApp.h"
#include "MainFrm.h"
#include "Logger.h"
#include "NetworkUtils.h"
#include "StringUtils.h"
#include "resource.h"

CTds520AApp theApp;

BEGIN_MESSAGE_MAP(CTds520AApp, CWinApp)
END_MESSAGE_MAP()

CTds520AApp::CTds520AApp()
{
    // Enable restart manager support for Windows 7
    m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_ALL_ASPECTS;
}

BOOL CTds520AApp::InitInstance()
{
    CWinApp::InitInstance();

    // Initialize Winsock (needed for HTTP server)
    if (!NetworkUtils::InitWinsock())
    {
        AfxMessageBox(L"Failed to initialize Winsock.", MB_ICONSTOP);
        return FALSE;
    }

    // Start logger
    Logger::Instance().SetMinLevel(LogLevel::Debug);
    Logger::Instance().EnableFileLog(L"TDS520A.log");
    LOG_INF("App", L"TDS 520A Acquisition System starting...");

    // Initialize ring buffer + acquisition thread
    m_acqThread = std::make_unique<AcquisitionThread>(m_ringBuf);
    m_acqThread->SetScope(&m_scope);

    // Setup HTTP server
    m_httpServer.SetPort(8080);
    m_httpServer.SetScope(&m_scope);
    m_httpServer.SetCommandCallback([this](const WebCommand& cmd)
    {
        switch (cmd.type)
        {
        case WebCommand::Type::Channel:
            if (m_acqThread) m_acqThread->RequestChannelChange(cmd.channel);
            break;
        case WebCommand::Type::Run:
            if (m_acqThread) m_acqThread->RequestRun();
            break;
        case WebCommand::Type::Stop:
            StopAcquisition();
            break;
        case WebCommand::Type::Single:
            if (m_acqThread) m_acqThread->RequestSingle();
            break;
        }
    });

    // Hook acquisition callback: just post to HTTP server's pending slot (no socket I/O here)
    m_acqThread->SetCallback([this](const DecodedWaveform& wave)
    {
        m_httpServer.PostWaveform(wave);
    });

    // Detect the best local IPv4 address before starting the web server.
    const std::string preferredIp = NetworkUtils::GetPreferredLocalIPv4Address();
    m_httpServer.SetBindAddress(preferredIp);

    // Start HTTP server
    if (!m_httpServer.Start())
    {
        LOG_WRN("App", L"HTTP server failed to start on port 8080");
    }
    else
    {
        LOG_INF("App", L"Selected local IP for HTTP server: %S", StringUtils::Utf8ToWide(preferredIp).c_str());
        LOG_INF("App", L"HTTP server: http://%S:8080/", StringUtils::Utf8ToWide(preferredIp).c_str());
    }

    // Create main window
    CMainFrame* pFrame = new CMainFrame;
    if (!pFrame->LoadFrame(IDR_MAINFRAME))
    {
        delete pFrame;
        return FALSE;
    }
    m_pMainWnd = pFrame;
    pFrame->ShowWindow(SW_SHOW);
    pFrame->UpdateWindow();

    return TRUE;
}

int CTds520AApp::ExitInstance()
{
    StopAcquisition();
    DisconnectScope();
    m_httpServer.Stop();
    NetworkUtils::CleanupWinsock();
    Logger::Instance().DisableFileLog();
    LOG_INF("App", L"Shutdown complete");
    return CWinApp::ExitInstance();
}

void CTds520AApp::StartAcquisition(ScopeChannel ch)
{
    if (!m_scope.IsConnected())
    {
        LOG_WRN("App", L"StartAcquisition: scope not connected");
        return;
    }
    // Stop any existing thread before starting a new one
    if (m_acqThread && m_acqThread->GetState() == AcqThreadState::Running)
    {
        m_acqThread->Stop();
        m_acqThread->WaitForStop(2000);
    }

    GpibError err;
    m_acqThread->SetScope(&m_scope);
    // Post Run so AcquisitionThread calls StartContinuous inside the GPIB thread.
    m_acqThread->PostCommand(ScopeCommand::Run);
    m_acqThread->Start(ch);
}

void CTds520AApp::StopAcquisition()
{
    if (m_acqThread)
    {
        m_acqThread->Stop();
        m_acqThread->WaitForStop(3000);
    }
}

bool CTds520AApp::ConnectScope()
{
    GpibError err;
    if (!m_scope.AutoConnect(0, err))
    {
        LOG_ERR("App", L"AutoConnect failed: %s", err.message.c_str());
        return false;
    }
    m_httpServer.SetCachedIdn(StringUtils::WideToUtf8(m_scope.GetIdn()));
    SyncSettingsFromScope();
    return true;
}

bool CTds520AApp::ConnectScope(int board, int addr)
{
    GpibDeviceInfo info;
    info.boardIndex  = board;
    info.primaryAddr = addr;
    info.timeoutCode = T3s;  // 3 s is plenty for any TDS 520A response

    GpibError err;
    if (!m_scope.Connect(info, err))
    {
        LOG_ERR("App", L"Connect(board=%d addr=%d) failed: %s",
                board, addr, err.message.c_str());
        return false;
    }
    m_httpServer.SetCachedIdn(StringUtils::WideToUtf8(m_scope.GetIdn()));
    SyncSettingsFromScope();
    return true;
}

void CTds520AApp::SyncSettingsFromScope()
{
    // Called from the connect background thread � GPIB is free here.
    // Read the scope's current horizontal and vertical settings so the
    // control panel and renderer show correct values from the first frame.
    GpibError err;

    double secPerDiv   = 0.0;
    double voltsPerDiv = 0.0;
    ScopeChannel ch = m_scope.GetChannel();

    bool gotH = m_scope.QueryHorizontalScale(secPerDiv, err) && secPerDiv > 0.0;
    bool gotV = m_scope.QueryChannelScale(ch, voltsPerDiv, err) && voltsPerDiv > 0.0;

    if (gotH || gotV)
    {
        TektronixScope::DisplayParams cur = m_scope.GetCachedDisplayParams();
        m_scope.SetCachedDisplayParams(
            gotH ? secPerDiv   : cur.secPerDiv,
            gotV ? voltsPerDiv : cur.voltsPerDiv);
        LOG_INF("App", L"Synced from scope: secPerDiv=%.3g voltsPerDiv=%.3g",
                gotH ? secPerDiv : cur.secPerDiv,
                gotV ? voltsPerDiv : cur.voltsPerDiv);
    }

    // Force preamble re-read on first fetch so NR_PT / XINCR are refreshed
    m_scope.InvalidatePreambleCache();
}

void CTds520AApp::DisconnectScope()
{
    StopAcquisition();
    m_scope.Disconnect();
}
