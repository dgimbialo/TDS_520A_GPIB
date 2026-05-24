// App/ConnectDialog.h - GPIB Connect Dialog
#pragma once
#include "pch.h"
#include "resource.h"
#include "GpibController.h"

// Message posted from scan worker thread to the dialog
// wParam = ConnectDialog::Msg, lParam = optional heap-allocated data (dialog frees it)
enum class CDlgMsg : WPARAM
{
    AddrProbed,   // lParam = address probed (no device)
    DeviceFound,  // lParam = heap FoundDevice*  (dialog deletes it)
    ScanDone,     // lParam = 0
    LogLine,      // lParam = heap wchar_t*  (dialog deletes it)
};

class ConnectDialog : public CDialog
{
public:
    explicit ConnectDialog(CWnd* pParent = nullptr);
    ~ConnectDialog() override;

    enum { IDD = IDD_CONNECT_DIALOG };

    // Result – filled when user clicks Connect + OK
    bool      HasSelection() const  { return m_selectedAddr >= 0; }
    int       GetBoardIndex() const { return m_boardIndex; }
    int       GetSelectedAddr() const { return m_selectedAddr; }
    std::string GetSelectedIdn() const { return m_selectedIdn; }

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
    void OnCancel() override;

    afx_msg void OnBtnScan();
    afx_msg void OnBtnConnect();
    afx_msg void OnDeviceListSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnScanMessage(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    void SetupListColumns();
    void AddDeviceToList(const FoundDevice& fd);
    void AppendLog(const wchar_t* line);
    void SetStatus(const wchar_t* text);
    void SetScanRunning(bool running);
    void StopScan();

    // Controls
    CEdit             m_editBoard;
    CSpinButtonCtrl   m_spinBoard;
    CEdit             m_editStartAddr;
    CSpinButtonCtrl   m_spinStartAddr;
    CListCtrl         m_listDevices;
    CEdit             m_editLog;
    CProgressCtrl     m_progress;

    // State
    int           m_boardIndex{ 17 };
    int           m_startAddr{ 1 };
    int           m_selectedAddr{ -1 };
    std::string   m_selectedIdn;
    std::atomic<bool> m_stopScan{ false };
    std::atomic<HWND> m_safeHwnd{ nullptr };
    bool          m_scanning{ false };

    std::vector<FoundDevice> m_foundDevices;

    static constexpr UINT WM_SCAN_MSG = WM_APP + 10;
};
