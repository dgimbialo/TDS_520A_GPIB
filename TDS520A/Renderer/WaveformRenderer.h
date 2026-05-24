// Renderer/WaveformRenderer.h - Pure GDI double-buffered waveform renderer
// Renders: grid, trigger marker, waveform polyline, channel labels, perf counters.
// Uses CreateCompatibleBitmap / SelectObject for offscreen double buffering.
// No GDI+; no DirectX dependency for maximum Windows 7 compatibility.
#pragma once
#include "pch.h"
#include "RenderContext.h"
#include "WaveformSample.h"

class WaveformRenderer
{
public:
    WaveformRenderer();
    ~WaveformRenderer();

    // Call when window is created or resized.
    // Recreates offscreen buffer.
    void Resize(HDC hdc, int width, int height);

    // Main render call. Pass the decoded waveform and render context.
    // Renders to offscreen buffer then blits to hdc.
    void Render(HDC hdc, const DecodedWaveform& wave, RenderContext& ctx);

    // Render "no signal" / "not connected" screen
    void RenderNoSignal(HDC hdc, const wchar_t* msg, RenderContext& ctx);

private:
    // Plot area (inset inside the full buffer to leave room for axis labels)
    struct PlotArea { int x, y, w, h; };
    PlotArea GetPlotArea() const;

    void DrawBackground(HDC hdc, const RenderContext& ctx);
    void DrawBorder(HDC hdc, const PlotArea& pa);
    void DrawGrid(HDC hdc, const RenderContext& ctx, const PlotArea& pa);
    void DrawTriggerLine(HDC hdc, const RenderContext& ctx, const PlotArea& pa);
    void DrawWaveform(HDC hdc, const DecodedWaveform& wave, const RenderContext& ctx, const PlotArea& pa);
    void DrawAxisLabels(HDC hdc, const DecodedWaveform& wave, const RenderContext& ctx, const PlotArea& pa);
    void DrawInfoOverlay(HDC hdc, const DecodedWaveform& wave, const RenderContext& ctx, const PlotArea& pa);
    void DrawPerfCounters(HDC hdc, const RenderContext& ctx);
    void DrawCursors(HDC hdc, const RenderContext& ctx, const PlotArea& pa);

    // GDI pens / brushes (created once, reused)
    void CreateGdiObjects(const RenderContext& ctx);
    void DeleteGdiObjects();
    void UpdateGdiObjects(const RenderContext& ctx);

    // Offscreen buffer
    HDC     m_offDC{ nullptr };
    HBITMAP m_offBmp{ nullptr };
    HBITMAP m_oldBmp{ nullptr };
    int     m_bufW{ 0 };
    int     m_bufH{ 0 };

    // GDI objects (pens, brushes, fonts)
    HPEN    m_penGrid{ nullptr };
    HPEN    m_penBorder{ nullptr };   // thin inner border of plot area
    HPEN    m_penWave{ nullptr };
    HPEN    m_penTrig{ nullptr };
    HPEN    m_penCursor{ nullptr };
    HBRUSH  m_brushBg{ nullptr };
    HBRUSH  m_brushOuter{ nullptr };  // margin / bezel background
    HFONT   m_fontLabel{ nullptr };
    HFONT   m_fontBig{ nullptr };

    // Margins for axis labels + padding (pixels)
    static constexpr int kMarginLeft   = 60;  // voltage axis labels
    static constexpr int kMarginBottom = 30;  // time axis labels
    static constexpr int kMarginTop    = 32;  // top padding + channel label
    static constexpr int kMarginRight  = 24;  // right padding

    COLORREF m_lastBgColor{ RGB(0,0,0) };

    // Pre-allocated point array for PolylineTo (avoids per-frame allocation)
    std::vector<POINT> m_polyPoints;

    bool m_objectsCreated{ false };
};
