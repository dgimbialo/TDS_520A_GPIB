// App/ScopeControlPanel.h - Right-side oscilloscope control panel
// Child dialog embedded in the main frame alongside the waveform view.
#pragma once
#include "pch.h"
#include "resource.h"
#include "WaveformSample.h"
#include "ScopeChannel.h"

class ScopeControlPanel : public CDialog
{
public:
    explicit ScopeControlPanel(CWnd* pParent = nullptr);
    ~ScopeControlPanel() override = default;

    enum { IDD = IDD_SCOPE_CONTROLS };

    // Called by main frame timer to refresh displayed data
    void UpdateStats(uint32_t acqRate,
                     double secPerDiv, double voltsPerDiv,
                     double trigLevel,
                     const DecodedWaveform* wave,
                     bool connected,
                     ScopeChannel activeCh);

    // Cursor toggle state
    bool IsCursorEnabled() const { return m_cursorEnabled; }

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    afx_msg void OnBtnRun();
    afx_msg void OnBtnStop();
    afx_msg void OnBtnSingle();
    afx_msg void OnBtnAcqRateUp();
    afx_msg void OnBtnAcqRateDn();
    afx_msg void OnAcqRateEditKillFocus();
    afx_msg void OnBtnTimeDivUp();
    afx_msg void OnBtnTimeDivDn();
    afx_msg void OnBtnVoltDivUp();
    afx_msg void OnBtnVoltDivDn();
    afx_msg void OnBtnTrigLvlUp();
    afx_msg void OnBtnTrigLvlDn();
    afx_msg void OnBtnCursorToggle();
    afx_msg void OnChannelSelChange();

    DECLARE_MESSAGE_MAP()

private:
    // Helpers to format and set label text
    static CString FormatTimediv(double s);
    static CString FormatVoltdiv(double v);

    // Compute frequency from waveform zero-crossings
    static double EstimateFrequency(const DecodedWaveform& w);

    bool m_cursorEnabled{ false };

    // Combo
    CComboBox m_comboChannel;
};
