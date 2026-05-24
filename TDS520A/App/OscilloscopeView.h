// App/OscilloscopeView.h - Main waveform display view
// Owns the render timer, consumes from ring buffer, drives WaveformRenderer.
#pragma once
#include "pch.h"
#include "WaveformRenderer.h"
#include "RenderContext.h"
#include "WaveformSample.h"

class COscilloscopeView : public CWnd
{
public:
    COscilloscopeView();
    virtual ~COscilloscopeView();

    float GetFPS() const { return m_fps; }
    const DecodedWaveform* GetCurrentWave() const
    { return m_hasData ? &m_currentWave : nullptr; }

protected:
    afx_msg int  OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);

private:
    void DoRender(CDC* pDC);
    void ConsumeRingBuffer();

    WaveformRenderer m_renderer;
    RenderContext    m_renderCtx;
    DecodedWaveform  m_currentWave;
    bool             m_hasData{ false };

    // FPS counter
    float  m_fps{ 0.0f };
    DWORD  m_fpsLastTick{ 0 };
    int    m_fpsFrameCount{ 0 };

    // Pan state
    bool   m_panning{ false };
    CPoint m_panStart;
    double m_panStartXCenter{ 0.0 };
    double m_panStartYCenter{ 0.0 };

    DECLARE_MESSAGE_MAP()
};
