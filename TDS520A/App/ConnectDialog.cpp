// App/ConnectDialog.cpp - GPIB Connect Dialog
#include "pch.h"
#include "ConnectDialog.h"
#include "Logger.h"

BEGIN_MESSAGE_MAP(ConnectDialog, CDialog)
    ON_BN_CLICKED(IDC_BTN_SCAN,    OnBtnScan)
    ON_BN_CLICKED(IDC_BTN_CONNECT, OnBtnConnect)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_DEVICES, OnDeviceListSelChanged)
    ON_MESSAGE(ConnectDialog::WM_SCAN_MSG, OnScanMessage)
END_MESSAGE_MAP()

ConnectDialog::ConnectDialog(CWnd* pParent)
    : CDialog(IDD_CONNECT_DIALOG, pParent)
{
}

ConnectDialog::~ConnectDialog()
{
    m_stopScan.store(true);  // signal worker to stop; m_safeHwnd already null after OnOK/OnCancel
}

void ConnectDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_BOARD,       m_editBoard);
    DDX_Control(pDX, IDC_SPIN_BOARD,       m_spinBoard);
    DDX_Control(pDX, IDC_EDIT_START_ADDR,  m_editStartAddr);
    DDX_Control(pDX, IDC_SPIN_START_ADDR,  m_spinStartAddr);
    DDX_Control(pDX, IDC_LIST_DEVICES,     m_listDevices);
    DDX_Control(pDX, IDC_EDIT_LOG,         m_editLog);
    DDX_Control(pDX, IDC_PROGRESS_SCAN,    m_progress);
    DDX_Text   (pDX, IDC_EDIT_BOARD,       m_boardIndex);
    DDX_Text   (pDX, IDC_EDIT_START_ADDR,  m_startAddr);
}

BOOL ConnectDialog::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Board spinner: 0-31 (NI numbers boards by adapter index, e.g. GPIB17)
    m_spinBoard.SetBuddy(&m_editBoard);
    m_spinBoard.SetRange32(0, 31);
    m_spinBoard.SetPos32(17);
    m_editBoard.SetWindowText(L"17");

    // Start address spinner: 1-30
    m_spinStartAddr.SetBuddy(&m_editStartAddr);
    m_spinStartAddr.SetRange32(1, 30);
    m_spinStartAddr.SetPos32(1);
    m_editStartAddr.SetWindowText(L"1");

    // Progress bar
    m_progress.SetRange(0, 30);
    m_progress.SetPos(0);

    SetupListColumns();
    SetScanRunning(false);
    m_safeHwnd.store(GetSafeHwnd());

    // Disable Connect and OK until something is selected
    GetDlgItem(IDC_BTN_CONNECT)->EnableWindow(FALSE);
    GetDlgItem(IDOK)->EnableWindow(FALSE);

    AppendLog(L"Ready. Press \"Scan\" to search for GPIB devices.");
    return TRUE;
}

// ??? List columns ??????????????????????????????????????????????????????????

void ConnectDialog::SetupListColumns()
{
    m_listDevices.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMN col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt  = LVCFMT_LEFT;

    col.cx = 60;  col.pszText = const_cast<LPWSTR>(L"Addr");  m_listDevices.InsertColumn(0, &col);
    col.cx = 340; col.pszText = const_cast<LPWSTR>(L"IDN");   m_listDevices.InsertColumn(1, &col);
}

void ConnectDialog::AddDeviceToList(const FoundDevice& fd)
{
    wchar_t addrBuf[16];
    _snwprintf_s(addrBuf, _countof(addrBuf), L"%d", fd.primaryAddr);

    int idx = m_listDevices.InsertItem(m_listDevices.GetItemCount(), addrBuf);
    m_listDevices.SetItemText(idx, 1, CA2W(fd.idn.c_str()));
    m_listDevices.SetItemData(idx, static_cast<DWORD_PTR>(m_foundDevices.size() - 1));

    // Auto-select first found device
    if (m_listDevices.GetItemCount() == 1)
    {
        m_listDevices.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED,
                                      LVIS_SELECTED | LVIS_FOCUSED);
    }
}

// ??? Log helpers ???????????????????????????????????????????????????????????

void ConnectDialog::AppendLog(const wchar_t* line)
{
    int len = m_editLog.GetWindowTextLength();
    m_editLog.SetSel(len, len);

    CString entry;
    if (len > 0) entry = L"\r\n";
    entry += line;
    m_editLog.ReplaceSel(entry);
}

void ConnectDialog::SetStatus(const wchar_t* text)
{
    SetDlgItemText(IDC_STATIC_STATUS, text);
}

// ??? UI state helpers ??????????????????????????????????????????????????????

void ConnectDialog::SetScanRunning(bool running)
{
    m_scanning = running;
    GetDlgItem(IDC_BTN_SCAN)->SetWindowText(running ? L"Stop" : L"&Scan");
    GetDlgItem(IDC_EDIT_BOARD)->EnableWindow(!running);
    GetDlgItem(IDC_SPIN_BOARD)->EnableWindow(!running);
    GetDlgItem(IDCANCEL)->EnableWindow(!running);
    if (!running)
        m_progress.SetPos(0);
}

// ??? Scan button ???????????????????????????????????????????????????????????

void ConnectDialog::OnBtnScan()
{
    if (m_scanning)
    {
        // Act as "Stop" button
        StopScan();
        return;
    }

    // Read board index and start address from the controls
    CString txt;
    m_editBoard.GetWindowText(txt);
    m_boardIndex = _wtoi(txt);

    CString txtStart;
    m_editStartAddr.GetWindowText(txtStart);
    m_startAddr = _wtoi(txtStart);
    if (m_startAddr < 1)  m_startAddr = 1;
    if (m_startAddr > 30) m_startAddr = 30;

    // Clear previous results
    m_listDevices.DeleteAllItems();
    m_foundDevices.clear();
    m_selectedAddr = -1;
    m_selectedIdn.clear();
    GetDlgItem(IDC_BTN_CONNECT)->EnableWindow(FALSE);
    GetDlgItem(IDOK)->EnableWindow(FALSE);

    m_progress.SetRange(0, 30);
    m_progress.SetPos(0);

    wchar_t msgBuf[128];
    _snwprintf_s(msgBuf, _countof(msgBuf),
                 L"Scanning GPIB board %d (addresses 1–30)...", m_boardIndex);
    AppendLog(msgBuf);
    SetStatus(msgBuf);
    SetScanRunning(true);

    m_stopScan.store(false);
    int board     = m_boardIndex;
    int startAddr = m_startAddr;

    // Worker thread – uses progress callback to post messages to the dialog
    std::thread([this, board, startAddr]()
    {
        GpibController ctrl;
        ctrl.FindDevices(board,
            [this](int addr, int /*total*/, const FoundDevice* fd)
            {
                HWND hwnd = m_safeHwnd.load();
                if (!hwnd) return;  // dialog already closed
                if (fd)
                {
                    auto* copy = new FoundDevice(*fd);
                    hwnd = m_safeHwnd.load();
                    if (!hwnd) { delete copy; return; }
                    ::PostMessage(hwnd, WM_SCAN_MSG,
                                  static_cast<WPARAM>(CDlgMsg::DeviceFound),
                                  reinterpret_cast<LPARAM>(copy));

                    wchar_t buf[256];
                    _snwprintf_s(buf, _countof(buf), L"Addr %2d: %S", addr, fd->idn.c_str());
                    auto* logLine = new wchar_t[wcslen(buf) + 1];
                    wcscpy_s(logLine, wcslen(buf) + 1, buf);
                    hwnd = m_safeHwnd.load();
                    if (!hwnd) { delete[] logLine; return; }
                    ::PostMessage(hwnd, WM_SCAN_MSG,
                                  static_cast<WPARAM>(CDlgMsg::LogLine),
                                  reinterpret_cast<LPARAM>(logLine));
                }
                else
                {
                    hwnd = m_safeHwnd.load();
                    if (!hwnd) return;
                    ::PostMessage(hwnd, WM_SCAN_MSG,
                                  static_cast<WPARAM>(CDlgMsg::AddrProbed),
                                  static_cast<LPARAM>(addr));
                }
            },
            &m_stopScan, startAddr);

        HWND hwnd = m_safeHwnd.load();
        if (hwnd)
            ::PostMessage(hwnd, WM_SCAN_MSG,
                          static_cast<WPARAM>(CDlgMsg::ScanDone), 0);
    }).detach();
}

void ConnectDialog::StopScan()
{
    m_safeHwnd.store(nullptr);
    m_stopScan.store(true);
}

// ??? Messages from scan worker ?????????????????????????????????????????????

LRESULT ConnectDialog::OnScanMessage(WPARAM wParam, LPARAM lParam)
{
    switch (static_cast<CDlgMsg>(wParam))
    {
    case CDlgMsg::AddrProbed:
    {
        int addr = static_cast<int>(lParam);
        m_progress.SetPos(addr);
        wchar_t buf[64];
        _snwprintf_s(buf, _countof(buf), L"Probing address %d...", addr);
        SetStatus(buf);
        break;
    }
    case CDlgMsg::DeviceFound:
    {
        auto* fd = reinterpret_cast<FoundDevice*>(lParam);
        m_foundDevices.push_back(*fd);
        AddDeviceToList(*fd);
        delete fd;
        break;
    }
    case CDlgMsg::LogLine:
    {
        auto* line = reinterpret_cast<wchar_t*>(lParam);
        AppendLog(line);
        delete[] line;
        break;
    }
    case CDlgMsg::ScanDone:
    {
        SetScanRunning(false);
        m_progress.SetPos(30);

        wchar_t buf[128];
        _snwprintf_s(buf, _countof(buf),
                     L"Scan complete: %zu device(s) found.",
                     m_foundDevices.size());
        AppendLog(buf);
        SetStatus(buf);

        // Auto-enable Connect if something is in the list
        bool hasItems = (m_listDevices.GetItemCount() > 0);
        GetDlgItem(IDC_BTN_CONNECT)->EnableWindow(hasItems);

        // Auto-select first item if nothing selected yet
        if (hasItems && m_listDevices.GetNextItem(-1, LVNI_SELECTED) < 0)
        {
            m_listDevices.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED,
                                          LVIS_SELECTED | LVIS_FOCUSED);
        }
        break;
    }
    }
    return 0;
}

// ??? Connect button ????????????????????????????????????????????????????????

void ConnectDialog::OnBtnConnect()
{
    int idx = m_listDevices.GetNextItem(-1, LVNI_SELECTED);
    if (idx < 0)
    {
        AfxMessageBox(L"Please select a device from the list first.", MB_ICONWARNING);
        return;
    }

    DWORD_PTR devIdx = m_listDevices.GetItemData(idx);
    if (devIdx >= m_foundDevices.size())
        return;

    const FoundDevice& fd = m_foundDevices[devIdx];
    m_selectedAddr = fd.primaryAddr;
    m_selectedIdn  = fd.idn;

    // Stop any ongoing scan immediately, then close the dialog
    StopScan();
    CDialog::OnOK();
}

// ??? List selection change ?????????????????????????????????????????????????

void ConnectDialog::OnDeviceListSelChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    int idx = m_listDevices.GetNextItem(-1, LVNI_SELECTED);
    GetDlgItem(IDC_BTN_CONNECT)->EnableWindow(idx >= 0);
    *pResult = 0;
}

// ??? OK / Cancel ???????????????????????????????????????????????????????????

void ConnectDialog::OnOK()
{
    if (m_selectedAddr < 0)
    {
        AfxMessageBox(L"Click \"Connect\" to select a device first.", MB_ICONWARNING);
        return;
    }
    StopScan();
    CDialog::OnOK();
}

void ConnectDialog::OnCancel()
{
    StopScan();
    CDialog::OnCancel();
}
