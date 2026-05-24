// App/ScopeControlPanel.cpp - Right-side oscilloscope control panel
#include "pch.h"
#include "ScopeControlPanel.h"
#include "TDS520AApp.h"
#include "Logger.h"
#include "NetworkUtils.h"
#pragma warning(push)
#pragma warning(disable: 4244 4267)
#include "../Utils/qrcodegen.hpp"
#pragma warning(pop)

BEGIN_MESSAGE_MAP(ScopeControlPanel, CDialog)
    ON_BN_CLICKED(IDC_BTN_RUN,           OnBtnRun)
    ON_BN_CLICKED(IDC_BTN_STOP,          OnBtnStop)
    ON_BN_CLICKED(IDC_BTN_SINGLE,        OnBtnSingle)
    ON_BN_CLICKED(IDC_BTN_ACQRATE_UP,    OnBtnAcqRateUp)
    ON_BN_CLICKED(IDC_BTN_ACQRATE_DN,    OnBtnAcqRateDn)
    ON_EN_KILLFOCUS(IDC_EDIT_ACQRATE,    OnAcqRateEditKillFocus)
    ON_BN_CLICKED(IDC_BTN_TIMEDIV_UP,    OnBtnTimeDivUp)
    ON_BN_CLICKED(IDC_BTN_TIMEDIV_DN,    OnBtnTimeDivDn)
    ON_BN_CLICKED(IDC_BTN_VOLTDIV_UP,    OnBtnVoltDivUp)
    ON_BN_CLICKED(IDC_BTN_VOLTDIV_DN,    OnBtnVoltDivDn)
    ON_BN_CLICKED(IDC_BTN_TRIGLVL_UP,    OnBtnTrigLvlUp)
    ON_BN_CLICKED(IDC_BTN_TRIGLVL_DN,    OnBtnTrigLvlDn)
    ON_BN_CLICKED(IDC_BTN_CURSOR_TOGGLE, OnBtnCursorToggle)
    ON_CBN_SELCHANGE(IDC_COMBO_CHANNEL,  OnChannelSelChange)
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()

ScopeControlPanel::ScopeControlPanel(CWnd* pParent)
    : CDialog(IDD_SCOPE_CONTROLS, pParent)
{
}

void ScopeControlPanel::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_CHANNEL, m_comboChannel);
}

BOOL ScopeControlPanel::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_comboChannel.AddString(L"CH1");
    m_comboChannel.AddString(L"CH2");
    m_comboChannel.AddString(L"CH3");
    m_comboChannel.AddString(L"CH4");
    m_comboChannel.SetCurSel(0);

    // Build the web URL and show it in the label
    std::string ip   = NetworkUtils::GetPrimaryLocalIP();
    std::string url  = "http://" + ip + ":8080";
    CString     wurl = CString(url.c_str());
    // IDC_STATIC_QR_URL is now a read-only EDITTEXT so the user can select/copy it
    SetDlgItemText(IDC_STATIC_QR_URL, wurl);

    // Generate QR code bitmap and cache it for OnDrawItem
    // QR control rect in dialog units — get pixel size after layout
    if (CWnd* pCtrl = GetDlgItem(IDC_STATIC_QR_IMAGE))
    {
        CRect rc;
        pCtrl->GetClientRect(&rc);
        int cellPx = rc.Width() > rc.Height() ? rc.Width() : rc.Height();
        if (cellPx < 4) cellPx = 90; // fallback if layout not ready yet

        using namespace qrcodegen;
        QrCode qr = QrCode::encodeText(url.c_str(), QrCode::Ecc::MEDIUM);
        int    sz = qr.getSize();           // modules per side
        int    scale = (cellPx / sz) > 1 ? (cellPx / sz) : 1; // pixels per module
        int    bmpSide = sz * scale;

        HDC     hdc    = ::GetDC(m_hWnd);
        HDC     memDC  = ::CreateCompatibleDC(hdc);
        HBITMAP hBmp   = ::CreateCompatibleBitmap(hdc, bmpSide, bmpSide);
        HBITMAP hOld   = static_cast<HBITMAP>(::SelectObject(memDC, hBmp));

        // Fill white background
        RECT all{ 0, 0, bmpSide, bmpSide };
        HBRUSH wBrush = ::CreateSolidBrush(RGB(255,255,255));
        HBRUSH bBrush = ::CreateSolidBrush(RGB(0,0,0));
        ::FillRect(memDC, &all, wBrush);

        for (int row = 0; row < sz; ++row)
            for (int col = 0; col < sz; ++col)
                if (qr.getModule(col, row))
                {
                    RECT cell{ col*scale, row*scale,
                               col*scale + scale, row*scale + scale };
                    ::FillRect(memDC, &cell, bBrush);
                }

        ::DeleteObject(wBrush);
        ::DeleteObject(bBrush);
        ::SelectObject(memDC, hOld);
        ::DeleteDC(memDC);
        ::ReleaseDC(m_hWnd, hdc);

        if (m_hQrBmp) ::DeleteObject(m_hQrBmp);
        m_hQrBmp    = hBmp;
        m_qrBmpSize = bmpSide;
    }

    return TRUE;
}

void ScopeControlPanel::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
    if (nIDCtl == IDC_STATIC_QR_IMAGE)
    {
        DrawQrCode(lpDIS);
        return;
    }
    CDialog::OnDrawItem(nIDCtl, lpDIS);
}

void ScopeControlPanel::DrawQrCode(LPDRAWITEMSTRUCT lpDIS)
{
    HDC  hdc = lpDIS->hDC;
    RECT rc  = lpDIS->rcItem;
    int  w   = rc.right  - rc.left;
    int  h   = rc.bottom - rc.top;

    // White background
    ::FillRect(hdc, &rc, static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));

    if (!m_hQrBmp || m_qrBmpSize <= 0) return;

    HDC    memDC = ::CreateCompatibleDC(hdc);
    HBITMAP hOld = static_cast<HBITMAP>(::SelectObject(memDC, m_hQrBmp));

    // Centre the QR bitmap inside the control rect
    int drawW = w < m_qrBmpSize ? w : m_qrBmpSize;
    int drawH = h < m_qrBmpSize ? h : m_qrBmpSize;
    int offX  = (w - drawW) / 2;
    int offY  = (h - drawH) / 2;

    ::StretchBlt(hdc, rc.left + offX, rc.top + offY, drawW, drawH,
                 memDC, 0, 0, m_qrBmpSize, m_qrBmpSize, SRCCOPY);

    ::SelectObject(memDC, hOld);
    ::DeleteDC(memDC);
}



// ---- Button handlers --------------------------------------------------------

void ScopeControlPanel::OnBtnRun()
{
    if (!theApp.GetScope().IsConnected())
    {
        SetDlgItemText(IDC_STATIC_CONN_STATUS, L"Not connected");
        return;
    }
    // Always restart via StartAcquisition — it handles both the case where
    // the thread is already running and where it was stopped by the Stop button.
    // RequestRun() alone is not enough when the thread has fully stopped.
    theApp.StartAcquisition(theApp.GetScope().GetChannel());
}

void ScopeControlPanel::OnBtnStop()
{
    theApp.StopAcquisition();
}

void ScopeControlPanel::OnBtnSingle()
{
    auto* acq = theApp.GetAcqThread();
    if (acq)
        acq->RequestSingle();
}

// Acq rate steps: 1,2,5,10,20,50,100,200 (acq/s), 0=unlimited
static const uint32_t kAcqRateSteps[] = { 1,2,5,10,20,50,100,200,0 };
static constexpr int  kAcqRateStepCount = static_cast<int>(
    sizeof(kAcqRateSteps)/sizeof(kAcqRateSteps[0]));

BOOL ScopeControlPanel::PreTranslateMessage(MSG* pMsg)
{
    // When Enter is pressed inside the acq-rate edit box, apply the value
    // and move focus away (which also triggers KillFocus).
    // Without this override, CDialog::PreTranslateMessage() routes VK_RETURN
    // to the default button (IDOK) which calls EndDialog() and hides the panel.
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* focused = GetFocus();
        if (focused && focused->GetDlgCtrlID() == IDC_EDIT_ACQRATE)
        {
            // Apply value then shift focus to the panel itself so KillFocus fires
            OnAcqRateEditKillFocus();
            GotoDlgCtrl(GetDlgItem(IDC_BTN_RUN));
            return TRUE;  // message handled, do NOT pass to CDialog
        }
    }
    return CDialog::PreTranslateMessage(pMsg);
}

void ScopeControlPanel::OnAcqRateEditKillFocus()
{
    auto* acq = theApp.GetAcqThread();
    if (!acq) return;
    CString text;
    GetDlgItemText(IDC_EDIT_ACQRATE, text);
    int val = _wtoi(text);
    if (val < 0) val = 0;
    acq->SetTargetRate(static_cast<uint32_t>(val));
}

void ScopeControlPanel::OnBtnAcqRateUp()
{
    auto* acq = theApp.GetAcqThread();
    if (!acq) return;
    uint32_t cur = acq->GetTargetRate();
    // Find next higher step (0 = unlimited = last step)
    for (int i = 0; i < kAcqRateStepCount - 1; ++i)
    {
        if (kAcqRateSteps[i] > cur || cur == 0)
        {
            acq->SetTargetRate(kAcqRateSteps[i]);
            return;
        }
    }
    acq->SetTargetRate(0);  // unlimited
}

void ScopeControlPanel::OnBtnAcqRateDn()
{
    auto* acq = theApp.GetAcqThread();
    if (!acq) return;
    uint32_t cur = acq->GetTargetRate();
    if (cur == 0)
    {
        // Was unlimited ? go to max step
        acq->SetTargetRate(kAcqRateSteps[kAcqRateStepCount - 2]);
        return;
    }
    for (int i = kAcqRateStepCount - 2; i >= 0; --i)
    {
        if (kAcqRateSteps[i] < cur)
        {
            acq->SetTargetRate(kAcqRateSteps[i]);
            return;
        }
    }
    acq->SetTargetRate(kAcqRateSteps[0]);
}

void ScopeControlPanel::OnBtnTimeDivUp()
{
    auto* acq = theApp.GetAcqThread();
    if (acq && acq->GetState() == AcqThreadState::Running)
        acq->PostCommand(ScopeCommand::TimeDivUp);
}

void ScopeControlPanel::OnBtnTimeDivDn()
{
    auto* acq = theApp.GetAcqThread();
    if (acq && acq->GetState() == AcqThreadState::Running)
        acq->PostCommand(ScopeCommand::TimeDivDown);
}

void ScopeControlPanel::OnBtnVoltDivUp()
{
    auto* acq = theApp.GetAcqThread();
    if (acq && acq->GetState() == AcqThreadState::Running)
        acq->PostCommand(ScopeCommand::VoltDivUp);
}

void ScopeControlPanel::OnBtnVoltDivDn()
{
    auto* acq = theApp.GetAcqThread();
    if (acq && acq->GetState() == AcqThreadState::Running)
        acq->PostCommand(ScopeCommand::VoltDivDown);
}

void ScopeControlPanel::OnBtnTrigLvlUp()
{
    auto* acq = theApp.GetAcqThread();
    if (acq && acq->GetState() == AcqThreadState::Running)
        acq->PostCommand(ScopeCommand::TrigLevelUp);
}

void ScopeControlPanel::OnBtnTrigLvlDn()
{
    auto* acq = theApp.GetAcqThread();
    if (acq && acq->GetState() == AcqThreadState::Running)
        acq->PostCommand(ScopeCommand::TrigLevelDown);
}

void ScopeControlPanel::OnBtnCursorToggle()
{
    m_cursorEnabled = !m_cursorEnabled;
    // Notify view via app
    theApp.SetCursorEnabled(m_cursorEnabled);
}

void ScopeControlPanel::OnChannelSelChange()
{
    int sel = m_comboChannel.GetCurSel();
    if (sel < 0) return;
    ScopeChannel ch = static_cast<ScopeChannel>(sel + 1);
    auto* acq = theApp.GetAcqThread();
    if (acq && acq->GetState() == AcqThreadState::Running)
        acq->RequestChannelChange(ch);
    else
    {
        GpibError err;
        theApp.GetScope().SetChannel(ch, err);
    }
}

// ---- Update display ---------------------------------------------------------

void ScopeControlPanel::UpdateStats(uint32_t acqRate,
                                    double secPerDiv, double voltsPerDiv,
                                    double trigLevel,
                                    const DecodedWaveform* wave,
                                    bool connected,
                                    ScopeChannel activeCh)
{
    // Connection status
    SetDlgItemText(IDC_STATIC_CONN_STATUS,
        connected ? L"Connected" : L"Not connected");

    // Sync channel combo
    int comboIdx = static_cast<int>(activeCh) - 1;
    if (m_comboChannel.GetCurSel() != comboIdx)
        m_comboChannel.SetCurSel(comboIdx);

    // Acq/s — actual rate + target
    wchar_t buf[64];
    _snwprintf_s(buf, _countof(buf), L"%u/s", acqRate);
    SetDlgItemText(IDC_STATIC_ACQRATE, buf);
    {
        auto* acq = theApp.GetAcqThread();
        uint32_t tgt = acq ? acq->GetTargetRate() : 0;
        wcscpy_s(buf, tgt > 0 ? std::to_wstring(tgt).c_str() : L"0");
        SetDlgItemText(IDC_STATIC_ACQRATE_TGT, buf);
        // Sync edit field only when it doesn't have focus
        CWnd* pEdit = GetDlgItem(IDC_EDIT_ACQRATE);
        if (pEdit && GetFocus() != pEdit)
            SetDlgItemText(IDC_EDIT_ACQRATE, buf);
    }

    // Time/div
    SetDlgItemText(IDC_STATIC_TIMEDIV, FormatTimediv(secPerDiv));

    // Volts/div
    SetDlgItemText(IDC_STATIC_VOLTDIV, FormatVoltdiv(voltsPerDiv));

    // Trigger level
    _snwprintf_s(buf, _countof(buf), L"%.3f V", trigLevel);
    SetDlgItemText(IDC_STATIC_TRIGLVL, buf);

    // Measurements from waveform data
    if (wave && !wave->Samples.empty())
    {
        double vmax = wave->YMax;
        double vmin = wave->YMin;
        double vpp  = vmax - vmin;
        double freq = EstimateFrequency(*wave);

        if (freq > 0.0)
        {
            if (freq >= 1e6)
                _snwprintf_s(buf, _countof(buf), L"%.3f MHz", freq / 1e6);
            else if (freq >= 1e3)
                _snwprintf_s(buf, _countof(buf), L"%.3f kHz", freq / 1e3);
            else
                _snwprintf_s(buf, _countof(buf), L"%.1f Hz", freq);
        }
        else
        {
            wcscpy_s(buf, L"---");
        }
        SetDlgItemText(IDC_STATIC_FREQ, buf);

        _snwprintf_s(buf, _countof(buf), L"%.4f V", vmax);
        SetDlgItemText(IDC_STATIC_VMAX, buf);

        _snwprintf_s(buf, _countof(buf), L"%.4f V", vmin);
        SetDlgItemText(IDC_STATIC_VMIN, buf);

        _snwprintf_s(buf, _countof(buf), L"%.4f V", vpp);
        SetDlgItemText(IDC_STATIC_VPP, buf);

        // RMS
        double sumSq = 0.0;
        for (auto& s : wave->Samples)
            sumSq += static_cast<double>(s.voltage) * s.voltage;
        double rms = std::sqrt(sumSq / wave->Samples.size());
        _snwprintf_s(buf, _countof(buf), L"%.4f V", rms);
        SetDlgItemText(IDC_STATIC_CURSOR_DT, buf);
    }
    else
    {
        SetDlgItemText(IDC_STATIC_FREQ, L"---");
        SetDlgItemText(IDC_STATIC_VMAX, L"---");
        SetDlgItemText(IDC_STATIC_VMIN, L"---");
        SetDlgItemText(IDC_STATIC_VPP,  L"---");
        SetDlgItemText(IDC_STATIC_CURSOR_DT, L"---");
    }
}

// ---- Static helpers ---------------------------------------------------------

CString ScopeControlPanel::FormatTimediv(double s)
{
    wchar_t buf[32];
    if (s <= 0.0) { return L"---"; }
    if (s < 1e-6)
        _snwprintf_s(buf, _countof(buf), L"%.1f ns/div", s * 1e9);
    else if (s < 1e-3)
        _snwprintf_s(buf, _countof(buf), L"%.2f \u00b5s/div", s * 1e6);
    else if (s < 1.0)
        _snwprintf_s(buf, _countof(buf), L"%.2f ms/div", s * 1e3);
    else
        _snwprintf_s(buf, _countof(buf), L"%.2f s/div", s);
    return CString(buf);
}

CString ScopeControlPanel::FormatVoltdiv(double v)
{
    wchar_t buf[32];
    if (v <= 0.0) return L"---";
    if (v < 1.0)
        _snwprintf_s(buf, _countof(buf), L"%.1f mV/div", v * 1000.0);
    else
        _snwprintf_s(buf, _countof(buf), L"%.2f V/div", v);
    return CString(buf);
}

double ScopeControlPanel::EstimateFrequency(const DecodedWaveform& w)
{
    if (w.Samples.size() < 4) return 0.0;
    // Count positive-going zero crossings
    int crossings = 0;
    for (size_t i = 1; i < w.Samples.size(); ++i)
    {
        if (w.Samples[i - 1].voltage <= 0.0f && w.Samples[i].voltage > 0.0f)
            ++crossings;
    }
    if (crossings < 2) return 0.0;
    double totalTime = static_cast<double>(w.Samples.back().time - w.Samples.front().time);
    if (totalTime <= 0.0) return 0.0;
    return static_cast<double>(crossings) / totalTime;
}
