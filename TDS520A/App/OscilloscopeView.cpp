// App/OscilloscopeView.cpp
#include "pch.h"
#include "OscilloscopeView.h"
#include "TDS520AApp.h"
#include "resource.h"

BEGIN_MESSAGE_MAP(COscilloscopeView, CWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

COscilloscopeView::COscilloscopeView()
{
    m_fpsLastTick = ::GetTickCount();
}

COscilloscopeView::~COscilloscopeView() = default;

int COscilloscopeView::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CWnd::OnCreate(lpcs) == -1) return -1;

    // Render at ~30 FPS (ring buffer keeps up regardless)
    SetTimer(TIMER_RENDER, 33, nullptr);
    return 0;
}

void COscilloscopeView::OnDestroy()
{
    KillTimer(TIMER_RENDER);
    CWnd::OnDestroy();
}

void COscilloscopeView::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    if (cx > 0 && cy > 0)
    {
        CClientDC dc(this);
        m_renderer.Resize(dc.GetSafeHdc(), cx, cy);
        m_renderCtx.View.screenW = cx;
        m_renderCtx.View.screenH = cy;
    }
}

BOOL COscilloscopeView::OnEraseBkgnd(CDC* /*pDC*/)
{
    // Suppress erase – renderer fills the entire client area
    return TRUE;
}

void COscilloscopeView::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_RENDER)
    {
        ConsumeRingBuffer();

        // FPS counter
        m_fpsFrameCount++;
        DWORD now = ::GetTickCount();
        DWORD elapsed = now - m_fpsLastTick;
        if (elapsed >= 1000)
        {
            m_fps = static_cast<float>(m_fpsFrameCount * 1000) / static_cast<float>(elapsed);
            m_fpsFrameCount = 0;
            m_fpsLastTick = now;
        }
        m_renderCtx.fps = m_fps;

        // Update acq rate from thread stats
        auto* acq = theApp.GetAcqThread();
        if (acq)
            m_renderCtx.acqRate = acq->GetStats().acquisitionsPerSec;

        // Sync cursor state from app
        m_renderCtx.cursorsEnabled = theApp.IsCursorEnabled();

        InvalidateRect(nullptr, FALSE);
        UpdateWindow();
    }
    CWnd::OnTimer(nIDEvent);
}

void COscilloscopeView::OnPaint()
{
    CPaintDC dc(this);
    DoRender(&dc);
}

void COscilloscopeView::DoRender(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    if (!m_hasData || m_currentWave.Samples.empty())
    {
        const wchar_t* msg = theApp.GetScope().IsConnected()
            ? L"Waiting for waveform data..."
            : L"Not connected\n\nUse Scope > Connect to connect";
        m_renderer.RenderNoSignal(pDC->GetSafeHdc(), msg, m_renderCtx);
    }
    else
    {
        m_renderer.Render(pDC->GetSafeHdc(), m_currentWave, m_renderCtx);
    }
}

void COscilloscopeView::ConsumeRingBuffer()
{
    auto& ring = theApp.GetRingBuffer();
    DecodedWaveform wave;
    // Drain ring buffer, keep only the latest frame
    bool got = false;
    while (ring.Pop(wave)) got = true;
    if (got)
    {
        m_currentWave = std::move(wave);
        m_hasData = true;
    }
}

BOOL COscilloscopeView::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/)
{
    // Zoom: scroll up = zoom in time axis
    double factor = (zDelta > 0) ? 1.2 : (1.0 / 1.2);
    m_renderCtx.View.xScale *= factor;
    InvalidateRect(nullptr, FALSE);
    return TRUE;
}

void COscilloscopeView::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_panning = true;
    m_panStart = point;
    m_panStartXCenter = m_renderCtx.View.xCenter;
    m_panStartYCenter = m_renderCtx.View.yCenter;
    SetCapture();
    CWnd::OnLButtonDown(nFlags, point);
}

void COscilloscopeView::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_panning)
    {
        int dx = point.x - m_panStart.x;
        int dy = point.y - m_panStart.y;
        double xScale = m_renderCtx.View.xScale;
        double yScale = m_renderCtx.View.yScale;
        if (xScale > 0 && yScale > 0)
        {
            m_renderCtx.View.xCenter = m_panStartXCenter - dx / xScale;
            m_renderCtx.View.yCenter = m_panStartYCenter + dy / yScale;
        }
    }
    CWnd::OnMouseMove(nFlags, point);
}

void COscilloscopeView::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_panning)
    {
        m_panning = false;
        ReleaseCapture();
    }
    CWnd::OnLButtonUp(nFlags, point);
}
