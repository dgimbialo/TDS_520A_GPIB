// Renderer/WaveformRenderer.cpp - GDI double-buffered oscilloscope renderer
#include "pch.h"
#include "WaveformRenderer.h"

WaveformRenderer::WaveformRenderer() = default;

WaveformRenderer::~WaveformRenderer()
{
    DeleteGdiObjects();
    if (m_offBmp)
    {
        ::SelectObject(m_offDC, m_oldBmp);
        ::DeleteObject(m_offBmp);
        m_offBmp = nullptr;
    }
    if (m_offDC)
    {
        ::DeleteDC(m_offDC);
        m_offDC = nullptr;
    }
}

void WaveformRenderer::Resize(HDC hdc, int width, int height)
{
    if (width <= 0 || height <= 0) return;

    if (m_offBmp)
    {
        ::SelectObject(m_offDC, m_oldBmp);
        ::DeleteObject(m_offBmp);
        m_offBmp = nullptr;
    }
    if (!m_offDC)
        m_offDC = ::CreateCompatibleDC(hdc);

    m_offBmp = ::CreateCompatibleBitmap(hdc, width, height);
    m_oldBmp = static_cast<HBITMAP>(::SelectObject(m_offDC, m_offBmp));
    m_bufW = width;
    m_bufH = height;
}

WaveformRenderer::PlotArea WaveformRenderer::GetPlotArea() const
{
    return {
        kMarginLeft,
        kMarginTop,
        m_bufW - kMarginLeft - kMarginRight,
        m_bufH - kMarginTop  - kMarginBottom
    };
}

void WaveformRenderer::Render(HDC hdc, const DecodedWaveform& wave, RenderContext& ctx)
{
    if (!m_offDC || m_bufW <= 0 || m_bufH <= 0) return;

    if (!m_objectsCreated || ctx.bgColor != m_lastBgColor)
    {
        UpdateGdiObjects(ctx);
        m_lastBgColor = ctx.bgColor;
    }

    PlotArea pa = GetPlotArea();
    ctx.View.screenW = pa.w;
    ctx.View.screenH = pa.h;

    if (!wave.Samples.empty())
    {
        ctx.View.FitToWaveform(wave.XMin, wave.XMax, wave.YMin, wave.YMax,
                                pa.w, pa.h);
        ctx.secPerDiv   = wave.SecPerDiv;
        ctx.voltsPerDiv = wave.VoltsPerDiv;
    }

    DrawBackground(m_offDC, ctx);
    DrawGrid(m_offDC, ctx, pa);
    DrawBorder(m_offDC, pa);
    DrawTriggerLine(m_offDC, ctx, pa);
    DrawWaveform(m_offDC, wave, ctx, pa);
    if (ctx.cursorsEnabled)
        DrawCursors(m_offDC, ctx, pa);
    DrawAxisLabels(m_offDC, wave, ctx, pa);
    DrawInfoOverlay(m_offDC, wave, ctx, pa);
    DrawPerfCounters(m_offDC, ctx);

    ::BitBlt(hdc, 0, 0, m_bufW, m_bufH, m_offDC, 0, 0, SRCCOPY);
}

void WaveformRenderer::RenderNoSignal(HDC hdc, const wchar_t* msg, RenderContext& ctx)
{
    if (!m_offDC || m_bufW <= 0 || m_bufH <= 0) return;

    if (!m_objectsCreated) UpdateGdiObjects(ctx);

    PlotArea pa = GetPlotArea();
    DrawBackground(m_offDC, ctx);
    DrawGrid(m_offDC, ctx, pa);
    DrawBorder(m_offDC, pa);

    if (!m_fontBig) { ::BitBlt(hdc, 0, 0, m_bufW, m_bufH, m_offDC, 0, 0, SRCCOPY); return; }
    HFONT oldFont = static_cast<HFONT>(::SelectObject(m_offDC, m_fontBig));
    ::SetBkMode(m_offDC, TRANSPARENT);
    ::SetTextColor(m_offDC, ctx.labelColor);

    RECT rc{ pa.x, pa.y, pa.x + pa.w, pa.y + pa.h };
    ::DrawTextW(m_offDC, msg, -1, &rc, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    ::SelectObject(m_offDC, oldFont);

    DrawPerfCounters(m_offDC, ctx);
    ::BitBlt(hdc, 0, 0, m_bufW, m_bufH, m_offDC, 0, 0, SRCCOPY);
}

void WaveformRenderer::DrawBackground(HDC hdc, const RenderContext& ctx)
{
    // Full window background — dark bezel
    RECT rc{ 0, 0, m_bufW, m_bufH };
    ::FillRect(hdc, &rc, m_brushOuter);

    // Plot area background (pure black)
    PlotArea pa = GetPlotArea();
    RECT plotRc{ pa.x, pa.y, pa.x + pa.w, pa.y + pa.h };
    ::FillRect(hdc, &plotRc, m_brushBg);
}

void WaveformRenderer::DrawBorder(HDC hdc, const PlotArea& pa)
{
    HPEN oldPen = static_cast<HPEN>(::SelectObject(hdc, m_penBorder));
    ::MoveToEx(hdc, pa.x,            pa.y,             nullptr);
    ::LineTo  (hdc, pa.x + pa.w - 1, pa.y);
    ::LineTo  (hdc, pa.x + pa.w - 1, pa.y + pa.h - 1);
    ::LineTo  (hdc, pa.x,            pa.y + pa.h - 1);
    ::LineTo  (hdc, pa.x,            pa.y);
    ::SelectObject(hdc, oldPen);
}

void WaveformRenderer::DrawGrid(HDC hdc, const RenderContext& ctx, const PlotArea& pa)
{
    HPEN oldPen = static_cast<HPEN>(::SelectObject(hdc, m_penGrid));

    const int hDiv = ctx.hDivisions;
    const int vDiv = ctx.vDivisions;

    // Vertical grid lines
    for (int i = 0; i <= hDiv; ++i)
    {
        int x = pa.x + (pa.w * i) / hDiv;
        ::MoveToEx(hdc, x, pa.y, nullptr);
        ::LineTo(hdc, x, pa.y + pa.h);
    }
    // Horizontal grid lines
    for (int i = 0; i <= vDiv; ++i)
    {
        int y = pa.y + (pa.h * i) / vDiv;
        ::MoveToEx(hdc, pa.x, y, nullptr);
        ::LineTo(hdc, pa.x + pa.w, y);
    }

    // Center tick marks (oscilloscope style)
    ::SelectObject(hdc, m_penBorder);
    int cx = pa.x + pa.w / 2;
    int cy = pa.y + pa.h / 2;
    const int tickLen = 4;
    for (int i = 0; i <= hDiv; ++i)
    {
        int x = pa.x + (pa.w * i) / hDiv;
        ::MoveToEx(hdc, x, cy - tickLen, nullptr);
        ::LineTo(hdc, x, cy + tickLen);
    }
    for (int i = 0; i <= vDiv; ++i)
    {
        int y = pa.y + (pa.h * i) / vDiv;
        ::MoveToEx(hdc, cx - tickLen, y, nullptr);
        ::LineTo(hdc, cx + tickLen, y);
    }

    ::SelectObject(hdc, oldPen);
}

void WaveformRenderer::DrawTriggerLine(HDC hdc, const RenderContext& ctx, const PlotArea& pa)
{
    HPEN oldPen = static_cast<HPEN>(::SelectObject(hdc, m_penTrig));
    int y = pa.y + ctx.View.WorldToScreenY(ctx.triggerLevel);
    if (y < pa.y || y > pa.y + pa.h) { ::SelectObject(hdc, oldPen); return; }

    ::MoveToEx(hdc, pa.x, y, nullptr);
    ::LineTo(hdc, pa.x + pa.w, y);

    // Trigger arrow on left edge
    POINT arrow[3] = {
        { pa.x + 2, y },
        { pa.x + 10, y - 5 },
        { pa.x + 10, y + 5 }
    };
    ::Polygon(hdc, arrow, 3);
    ::SelectObject(hdc, oldPen);
}

void WaveformRenderer::DrawWaveform(HDC hdc, const DecodedWaveform& wave,
                                     const RenderContext& ctx, const PlotArea& pa)
{
    if (wave.Samples.empty()) return;

    const size_t n = wave.Samples.size();
    if (m_polyPoints.size() < n)
        m_polyPoints.resize(n);

    for (size_t i = 0; i < n; ++i)
    {
        m_polyPoints[i].x = pa.x + ctx.View.WorldToScreenX(wave.Samples[i].time);
        m_polyPoints[i].y = pa.y + ctx.View.WorldToScreenY(wave.Samples[i].voltage);
    }

    // Clip to plot area via SaveDC/RestoreDC + clipping region
    HRGN rgn = ::CreateRectRgn(pa.x, pa.y, pa.x + pa.w, pa.y + pa.h);
    int saved = ::SaveDC(hdc);
    ::SelectClipRgn(hdc, rgn);

    HPEN oldPen = static_cast<HPEN>(::SelectObject(hdc, m_penWave));
    ::Polyline(hdc, m_polyPoints.data(), static_cast<int>(n));
    ::SelectObject(hdc, oldPen);

    ::RestoreDC(hdc, saved);
    ::DeleteObject(rgn);
}

void WaveformRenderer::DrawAxisLabels(HDC hdc, const DecodedWaveform& wave,
                                       const RenderContext& ctx, const PlotArea& pa)
{
    if (!m_fontLabel) return;
    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, m_fontLabel));
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, ctx.labelColor);

    const int hDiv = ctx.hDivisions;
    const int vDiv = ctx.vDivisions;

    wchar_t buf[32];

    // ---- Voltage axis (left margin) ----------------------------------------
    // Each horizontal grid line corresponds to one voltage step
    if (ctx.voltsPerDiv > 0.0)
    {
        double vCenter = ctx.View.yCenter;
        double halfDivs = vDiv * 0.5;
        for (int i = 0; i <= vDiv; ++i)
        {
            double v = vCenter + (halfDivs - i) * ctx.voltsPerDiv;
            int    y = pa.y + (pa.h * i) / vDiv;

            if (std::abs(v) < 1.0)
                _snwprintf_s(buf, _countof(buf), L"%.0fmV", v * 1000.0);
            else
                _snwprintf_s(buf, _countof(buf), L"%.2fV", v);

            RECT rc{ 0, y - 8, pa.x - 2, y + 8 };
            ::DrawTextW(hdc, buf, -1, &rc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // ---- Time axis (bottom margin) -----------------------------------------
    if (ctx.secPerDiv > 0.0)
    {
        double tCenter = ctx.View.xCenter;
        double halfDivs = hDiv * 0.5;
        for (int i = 0; i <= hDiv; ++i)
        {
            double t = tCenter + (i - halfDivs) * ctx.secPerDiv;
            int    x = pa.x + (pa.w * i) / hDiv;

            if (std::abs(t) < 1e-6)
                _snwprintf_s(buf, _countof(buf), L"%.0fns", t * 1e9);
            else if (std::abs(t) < 1e-3)
                _snwprintf_s(buf, _countof(buf), L"%.1f\u00b5s", t * 1e6);
            else if (std::abs(t) < 1.0)
                _snwprintf_s(buf, _countof(buf), L"%.1fms", t * 1e3);
            else
                _snwprintf_s(buf, _countof(buf), L"%.2fs", t);

            RECT rc{ x - 30, pa.y + pa.h + 2, x + 30, pa.y + pa.h + kMarginBottom };
            ::DrawTextW(hdc, buf, -1, &rc, DT_CENTER | DT_TOP | DT_SINGLELINE);
        }
    }

    ::SelectObject(hdc, oldFont);
}

void WaveformRenderer::DrawInfoOverlay(HDC hdc, const DecodedWaveform& wave,
                                        const RenderContext& ctx, const PlotArea& pa)
{
    if (!m_fontLabel) return;
    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, m_fontLabel));
    ::SetBkMode(hdc, TRANSPARENT);

    wchar_t buf[64];

    // Sequence number top-right inside plot
    ::SetTextColor(hdc, RGB(120, 120, 120));
    _snwprintf_s(buf, _countof(buf), L"#%u", wave.SequenceNumber);
    RECT rcSeq{ pa.x + pa.w - 70, pa.y + 3, pa.x + pa.w - 2, pa.y + 18 };
    ::DrawTextW(hdc, buf, -1, &rcSeq, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Channel label top-left
    ::SetTextColor(hdc, ctx.waveColor);
    const wchar_t* chNames[] = { L"CH1", L"CH2", L"CH3", L"CH4" };
    // We just display static "CH?" — actual channel is in the control panel
    RECT rcCh{ pa.x + 4, pa.y + 3, pa.x + 50, pa.y + 18 };
    ::DrawTextW(hdc, chNames[0], -1, &rcCh, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    ::SelectObject(hdc, oldFont);
}

void WaveformRenderer::DrawCursors(HDC hdc, const RenderContext& ctx, const PlotArea& pa)
{
    if (!m_penCursor) return;
    HPEN oldPen = static_cast<HPEN>(::SelectObject(hdc, m_penCursor));
    ::SetBkMode(hdc, TRANSPARENT);

    // Vertical time cursors
    auto drawVCursor = [&](double t)
    {
        int x = pa.x + ctx.View.WorldToScreenX(t);
        if (x < pa.x || x > pa.x + pa.w) return;
        ::MoveToEx(hdc, x, pa.y, nullptr);
        ::LineTo(hdc, x, pa.y + pa.h);
    };
    drawVCursor(ctx.cursor1Time);
    drawVCursor(ctx.cursor2Time);

    // Horizontal voltage cursors
    auto drawHCursor = [&](double v)
    {
        int y = pa.y + ctx.View.WorldToScreenY(v);
        if (y < pa.y || y > pa.y + pa.h) return;
        ::MoveToEx(hdc, pa.x, y, nullptr);
        ::LineTo(hdc, pa.x + pa.w, y);
    };
    drawHCursor(ctx.cursor1Volt);
    drawHCursor(ctx.cursor2Volt);

    ::SelectObject(hdc, oldPen);
}

void WaveformRenderer::DrawPerfCounters(HDC /*hdc*/, const RenderContext& /*ctx*/)
{
    // FPS and Acq/s are shown in the main window status bar — not drawn here.
}

void WaveformRenderer::CreateGdiObjects(const RenderContext& ctx)
{
    m_penGrid    = ::CreatePen(PS_SOLID, 1, ctx.gridColor);
    m_penBorder  = ::CreatePen(PS_SOLID, 1, RGB( 60, 100,  60));  // inner plot edge
    m_penWave    = ::CreatePen(PS_SOLID, 1, ctx.waveColor);
    m_penTrig    = ::CreatePen(PS_SOLID, 1, ctx.trigColor);
    m_penCursor  = ::CreatePen(PS_DOT,   1, RGB(0, 200, 255));
    m_brushBg    = ::CreateSolidBrush(ctx.bgColor);
    m_brushOuter = ::CreateSolidBrush(RGB(28, 32, 28));   // dark green-tinted bezel

    m_fontLabel = ::CreateFontW(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
        L"Consolas");

    m_fontBig = ::CreateFontW(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");

    m_objectsCreated = true;
}

void WaveformRenderer::UpdateGdiObjects(const RenderContext& ctx)
{
    DeleteGdiObjects();
    CreateGdiObjects(ctx);
}

void WaveformRenderer::DeleteGdiObjects()
{
    auto del = [](HGDIOBJ& obj) { if (obj) { ::DeleteObject(obj); obj = nullptr; } };
    del(reinterpret_cast<HGDIOBJ&>(m_penGrid));
    del(reinterpret_cast<HGDIOBJ&>(m_penBorder));
    del(reinterpret_cast<HGDIOBJ&>(m_penWave));
    del(reinterpret_cast<HGDIOBJ&>(m_penTrig));
    del(reinterpret_cast<HGDIOBJ&>(m_penCursor));
    del(reinterpret_cast<HGDIOBJ&>(m_brushBg));
    del(reinterpret_cast<HGDIOBJ&>(m_brushOuter));
    del(reinterpret_cast<HGDIOBJ&>(m_fontLabel));
    del(reinterpret_cast<HGDIOBJ&>(m_fontBig));
    m_objectsCreated = false;
}
