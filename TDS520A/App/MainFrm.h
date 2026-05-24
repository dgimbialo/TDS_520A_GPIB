// App/MainFrm.h - SDI Main Frame Window
#pragma once
#include "pch.h"
#include "resource.h"
#include "ScopeControlPanel.h"

class CMainFrame : public CFrameWnd
{
public:
    CMainFrame();
    virtual ~CMainFrame();

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) override;

    static constexpr UINT WM_CONNECT_DONE = WM_APP + 1; // wParam=1 success, 0 fail

protected:
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnClose();
    afx_msg void OnScopeConnect();
    afx_msg void OnScopeDisconnect();
    afx_msg LRESULT OnConnectDone(WPARAM wParam, LPARAM lParam);
    afx_msg void OnScopeStart();
    afx_msg void OnScopeStop();
    afx_msg void OnScopeCH(UINT nID);
    afx_msg void OnServerStart();
    afx_msg void OnServerStop();
    afx_msg void OnHelpAbout();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnUpdateScopeConnect(CCmdUI* pCmdUI);
    afx_msg void OnUpdateScopeDisconnect(CCmdUI* pCmdUI);
    afx_msg void OnUpdateScopeStart(CCmdUI* pCmdUI);
    afx_msg void OnUpdateScopeStop(CCmdUI* pCmdUI);
    afx_msg void OnUpdateServerStart(CCmdUI* pCmdUI);
    afx_msg void OnUpdateServerStop(CCmdUI* pCmdUI);
    afx_msg void OnSize(UINT nType, int cx, int cy);

private:
    void UpdateStatusBar();
    void LayoutPanes(int cx, int cy);
    void UpdateControlPanel();

    static constexpr int CTRL_PANEL_W = 200;  // fallback; actual width measured at runtime

    CStatusBar         m_wndStatusBar;
    CToolBar           m_wndToolBar;
    ScopeControlPanel  m_ctrlPanel;
    int                m_ctrlPanelW{ CTRL_PANEL_W };  // actual pixel width of control panel
    std::atomic<bool>  m_connecting{ false };

    DECLARE_MESSAGE_MAP()
    DECLARE_DYNAMIC(CMainFrame)
};
