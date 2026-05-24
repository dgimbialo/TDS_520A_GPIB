// App/MainFrm.cpp - SDI Main Frame
#include "pch.h"
#include "MainFrm.h"
#include "TDS520AApp.h"
#include "OscilloscopeView.h"
#include "ConnectDialog.h"
#include "NetworkUtils.h"
#include "Logger.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_WM_TIMER()
    ON_WM_SIZE()
    ON_MESSAGE(WM_CONNECT_DONE, OnConnectDone)
    ON_COMMAND(ID_SCOPE_CONNECT,    OnScopeConnect)
    ON_COMMAND(ID_SCOPE_DISCONNECT, OnScopeDisconnect)
    ON_COMMAND(ID_SCOPE_STARTACQ,   OnScopeStart)
    ON_COMMAND(ID_SCOPE_STOPACQ,    OnScopeStop)
    ON_COMMAND_RANGE(ID_SCOPE_CH1, ID_SCOPE_CH4, OnScopeCH)
    ON_COMMAND(ID_SERVER_START,     OnServerStart)
    ON_COMMAND(ID_SERVER_STOP,      OnServerStop)
    ON_COMMAND(ID_HELP_ABOUT,       OnHelpAbout)
    ON_UPDATE_COMMAND_UI(ID_SCOPE_CONNECT,    OnUpdateScopeConnect)
    ON_UPDATE_COMMAND_UI(ID_SCOPE_DISCONNECT, OnUpdateScopeDisconnect)
    ON_UPDATE_COMMAND_UI(ID_SCOPE_STARTACQ,   OnUpdateScopeStart)
    ON_UPDATE_COMMAND_UI(ID_SCOPE_STOPACQ,    OnUpdateScopeStop)
    ON_UPDATE_COMMAND_UI(ID_SERVER_START,     OnUpdateServerStart)
    ON_UPDATE_COMMAND_UI(ID_SERVER_STOP,      OnUpdateServerStop)
END_MESSAGE_MAP()

static UINT indicators[] =
{
    ID_SEPARATOR,
    ID_STATUSBAR_PANE_GPIB,
    ID_STATUSBAR_PANE_FPS,
    ID_STATUSBAR_PANE_ACQ,
    ID_STATUSBAR_PANE_HTTP,
};

CMainFrame::CMainFrame() = default;
CMainFrame::~CMainFrame() = default;

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs)) return FALSE;
    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.lpszClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
        nullptr);
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) return -1;

    if (!m_wndStatusBar.Create(this) ||
        !m_wndStatusBar.SetIndicators(indicators, _countof(indicators)))
    {
        LOG_ERR("MainFrm", L"Failed to create status bar");
        return -1;
    }

    // Set pane widths (doubled)
    m_wndStatusBar.SetPaneInfo(1, ID_STATUSBAR_PANE_GPIB, SBPS_NORMAL, 320);
    m_wndStatusBar.SetPaneInfo(2, ID_STATUSBAR_PANE_FPS,  SBPS_NORMAL, 160);
    m_wndStatusBar.SetPaneInfo(3, ID_STATUSBAR_PANE_ACQ,  SBPS_NORMAL, 160);
    m_wndStatusBar.SetPaneInfo(4, ID_STATUSBAR_PANE_HTTP, SBPS_NORMAL, 320);

    // Create oscilloscope view
    RECT rc;
    GetClientRect(&rc);
    COscilloscopeView* pView = new COscilloscopeView;
    pView->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        rc, this, AFX_IDW_PANE_FIRST);

    // Create right-side control panel (modeless child dialog)
    m_ctrlPanel.Create(IDD_SCOPE_CONTROLS, this);
    m_ctrlPanel.ShowWindow(SW_SHOW);
    {
        // Measure actual pixel width the dialog resource produces at current DPI
        CRect dlgRc;
        m_ctrlPanel.GetWindowRect(&dlgRc);
        if (dlgRc.Width() > 10)
            m_ctrlPanelW = dlgRc.Width();
    }

    // Status timer: update every 500ms
    SetTimer(TIMER_STATUS, 500, nullptr);
    // Control panel refresh: update every 200ms
    SetTimer(TIMER_CTRL_UPDATE, 200, nullptr);

    // Title
    SetWindowText(L"TDS 520A GPIB Oscilloscope Acquisition");

    return 0;
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
    // Default single-pane view
    return CFrameWnd::OnCreateClient(lpcs, pContext);
}

void CMainFrame::OnClose()
{
    KillTimer(TIMER_STATUS);
    KillTimer(TIMER_CTRL_UPDATE);
    theApp.StopAcquisition();
    CFrameWnd::OnClose();
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_STATUS)
        UpdateStatusBar();
    else if (nIDEvent == TIMER_CTRL_UPDATE)
        UpdateControlPanel();
    CFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
    CFrameWnd::OnSize(nType, cx, cy);
    if (cx > 0 && cy > 0)
        LayoutPanes(cx, cy);
}

void CMainFrame::LayoutPanes(int cx, int cy)
{
    // Adjust cy to exclude the status bar
    CRect statusRc;
    if (m_wndStatusBar.GetSafeHwnd())
    {
        m_wndStatusBar.GetWindowRect(&statusRc);
        cy -= statusRc.Height();
    }
    if (cy <= 0) cy = 1;

    // Control panel on the right, waveform view fills the rest
    const int panelW = m_ctrlPanelW;
    const int viewW  = std::max(1, cx - panelW);

    auto* pView = GetDlgItem(AFX_IDW_PANE_FIRST);
    if (pView && pView->GetSafeHwnd())
        pView->SetWindowPos(nullptr, 0, 0, viewW, cy,
            SWP_NOZORDER | SWP_NOACTIVATE);

    if (m_ctrlPanel.GetSafeHwnd())
        m_ctrlPanel.SetWindowPos(nullptr, viewW, 0, panelW, cy,
            SWP_NOZORDER | SWP_NOACTIVATE);
}

void CMainFrame::UpdateControlPanel()
{
    if (!m_ctrlPanel.GetSafeHwnd()) return;

    auto& scope = theApp.GetScope();
    auto* acq   = theApp.GetAcqThread();

    uint32_t acqRate    = acq ? acq->GetStats().acquisitionsPerSec : 0;
    double   secPerDiv  = 0.0;
    double   voltsPerDiv= 0.0;
    double   trigLevel  = 0.0;

    if (scope.IsConnected())
    {
        // Read from the cache that AcquisitionThread updates after each waveform.
        // Do NOT call Query*() from the UI timer — those send GPIB commands and
        // race with the acquisition thread, corrupting the GPIB bus state.
        auto dp = scope.GetCachedDisplayParams();
        secPerDiv   = dp.secPerDiv;
        voltsPerDiv = dp.voltsPerDiv;
        // trigLevel is not in the preamble; keep it zero until a dedicated
        // background query mechanism is added.
    }

    // Get latest waveform for measurements
    auto* pView = dynamic_cast<COscilloscopeView*>(GetDlgItem(AFX_IDW_PANE_FIRST));
    const DecodedWaveform* wave = pView ? pView->GetCurrentWave() : nullptr;

    m_ctrlPanel.UpdateStats(acqRate, secPerDiv, voltsPerDiv, trigLevel,
                            wave, scope.IsConnected(), scope.GetChannel());
}

void CMainFrame::UpdateStatusBar()
{
    auto& scope = theApp.GetScope();
    auto* acq   = theApp.GetAcqThread();
    auto& srv   = theApp.GetHttpServer();

    // GPIB pane
    if (scope.IsConnected())
    {
        auto status = scope.GetStatus();
        wchar_t buf[64];
        _snwprintf_s(buf, _countof(buf), L"GPIB: %s", status.idn.c_str());
        m_wndStatusBar.SetPaneText(1, buf);
    }
    else
    {
        m_wndStatusBar.SetPaneText(1, L"GPIB: Disconnected");
    }

    // FPS pane
    {
        auto* pView = dynamic_cast<COscilloscopeView*>(GetDlgItem(AFX_IDW_PANE_FIRST));
        wchar_t buf[48];
        float fps = pView ? pView->GetFPS() : 0.0f;
        _snwprintf_s(buf, _countof(buf), L"FPS: %.1f", fps);
        m_wndStatusBar.SetPaneText(2, buf);
    }

    // Acq/s pane
    if (acq)
    {
        auto stats = acq->GetStats();
        uint32_t tgt = acq->GetTargetRate();
        wchar_t buf[48];
        if (tgt > 0)
            _snwprintf_s(buf, _countof(buf), L"Acq: %u/s  (lim %u/s)",
                         stats.acquisitionsPerSec, tgt);
        else
            _snwprintf_s(buf, _countof(buf), L"Acq: %u/s  (max)",
                         stats.acquisitionsPerSec);
        m_wndStatusBar.SetPaneText(3, buf);
    }
    else
    {
        m_wndStatusBar.SetPaneText(3, L"Acq: ---");
    }

    // HTTP server
    if (srv.IsRunning())
    {
        std::string ip = NetworkUtils::GetPrimaryLocalIP();
        wchar_t httpBuf[64];
        _snwprintf_s(httpBuf, _countof(httpBuf), L"HTTP:%S:8080 (%d cl)",
            ip.c_str(), srv.GetClientCount());
        m_wndStatusBar.SetPaneText(4, httpBuf);
    }
    else
    {
        m_wndStatusBar.SetPaneText(4, L"HTTP: Stopped");
    }
}

void CMainFrame::OnScopeConnect()
{
    if (m_connecting || theApp.GetScope().IsConnected())
        return;

    ConnectDialog dlg(this);
    if (dlg.DoModal() != IDOK)
        return;

    // User selected a device and clicked OK — connect on a background thread
    m_connecting = true;
    m_wndStatusBar.SetPaneText(1, L"GPIB: Connecting...");

    int board = dlg.GetBoardIndex();
    int addr  = dlg.GetSelectedAddr();

    std::thread([this, board, addr]()
    {
        bool ok = theApp.ConnectScope(board, addr);
        PostMessage(WM_CONNECT_DONE, ok ? 1 : 0, 0);
    }).detach();
}

LRESULT CMainFrame::OnConnectDone(WPARAM wParam, LPARAM)
{
    m_connecting = false;
    if (!wParam)
    {
        AfxMessageBox(L"Failed to connect to the selected GPIB device.\n"
                      L"Check GPIB connection and NI-488.2 driver.", MB_ICONWARNING);
    }
    else
    {
        // Connected successfully — user can start acquisition manually
    }
    UpdateStatusBar();
    return 0;
}

void CMainFrame::OnScopeDisconnect()
{
    theApp.DisconnectScope();
}

void CMainFrame::OnScopeStart()
{
    theApp.StartAcquisition(theApp.GetScope().GetChannel());
}

void CMainFrame::OnScopeStop()
{
    theApp.StopAcquisition();
    GpibError err;
    theApp.GetScope().Stop(err);
}

void CMainFrame::OnScopeCH(UINT nID)
{
    ScopeChannel ch = static_cast<ScopeChannel>(nID - ID_SCOPE_CH1 + 1);
    auto* acq = theApp.GetAcqThread();
    if (acq && acq->GetState() == AcqThreadState::Running)
        acq->RequestChannelChange(ch);
    else
    {
        GpibError err;
        theApp.GetScope().SetChannel(ch, err);
    }
}

void CMainFrame::OnServerStart()
{
    theApp.GetHttpServer().Start();
}

void CMainFrame::OnServerStop()
{
    theApp.GetHttpServer().Stop();
}

void CMainFrame::OnHelpAbout()
{
    std::string ip = NetworkUtils::GetPrimaryLocalIP();
    wchar_t msg[512];
    _snwprintf_s(msg, _countof(msg),
        L"TDS 520A GPIB Oscilloscope Acquisition\n"
        L"Version 1.0\n\n"
        L"Web interface: http://%S:8080/\n\n"
        L"NI-488.2 GPIB communication\n"
        L"Built with MFC / WinAPI / C++17",
        ip.c_str());
    AfxMessageBox(msg, MB_ICONINFORMATION);
}

void CMainFrame::OnUpdateScopeConnect   (CCmdUI* p) { p->Enable(!theApp.GetScope().IsConnected() && !m_connecting); }
void CMainFrame::OnUpdateScopeDisconnect(CCmdUI* p) { p->Enable( theApp.GetScope().IsConnected()); }
void CMainFrame::OnUpdateScopeStart(CCmdUI* p)
{
    auto* acq = theApp.GetAcqThread();
    p->Enable(theApp.GetScope().IsConnected() &&
              (!acq || acq->GetState() != AcqThreadState::Running));
}
void CMainFrame::OnUpdateScopeStop(CCmdUI* p)
{
    auto* acq = theApp.GetAcqThread();
    p->Enable(acq && acq->GetState() == AcqThreadState::Running);
}
void CMainFrame::OnUpdateServerStart(CCmdUI* p) { p->Enable(!theApp.GetHttpServer().IsRunning()); }
void CMainFrame::OnUpdateServerStop (CCmdUI* p) { p->Enable( theApp.GetHttpServer().IsRunning()); }
