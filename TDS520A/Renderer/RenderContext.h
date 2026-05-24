// Renderer/RenderContext.h - Holds the current view transform (zoom/pan)
#pragma once
#include "pch.h"

struct ViewTransform
{
    // World space: time (seconds) and voltage (volts)
    double xCenter{ 0.0 };       // Seconds at center of display
    double yCenter{ 0.0 };       // Volts at center of display
    double xScale{ 1.0 };        // Pixels per second
    double yScale{ 100.0 };      // Pixels per volt

    // Screen dimensions
    int    screenW{ 800 };
    int    screenH{ 600 };

    // Convert world -> screen
    inline int WorldToScreenX(double t) const
    {
        return static_cast<int>((t - xCenter) * xScale + screenW * 0.5);
    }
    inline int WorldToScreenY(double v) const
    {
        return static_cast<int>(screenH * 0.5 - (v - yCenter) * yScale);
    }
    inline POINT WorldToScreen(double t, double v) const
    {
        return { WorldToScreenX(t), WorldToScreenY(v) };
    }

    // Convert screen -> world
    inline double ScreenToWorldX(int px) const
    {
        return xCenter + (px - screenW * 0.5) / xScale;
    }
    inline double ScreenToWorldY(int py) const
    {
        return yCenter + (screenH * 0.5 - py) / yScale;
    }

    // Fit waveform to screen
    void FitToWaveform(double tMin, double tMax, double vMin, double vMax, int w, int h)
    {
        screenW = w;
        screenH = h;
        xCenter = (tMin + tMax) * 0.5;
        yCenter = (vMin + vMax) * 0.5;
        double tSpan = tMax - tMin;
        double vSpan = vMax - vMin;
        if (tSpan > 0) xScale = static_cast<double>(w) * 0.9 / tSpan;
        if (vSpan > 0) yScale = static_cast<double>(h) * 0.8 / vSpan;
    }
};

struct RenderContext
{
    ViewTransform View;

    // Colors (GDI COLORREF)
    COLORREF bgColor       { RGB(  0,  0,  0) };   // Black background
    COLORREF gridColor     { RGB( 40, 80, 40) };   // Dark green grid
    COLORREF waveColor     { RGB(  0,255,  0) };   // Green waveform
    COLORREF trigColor     { RGB(255,255,  0) };   // Yellow trigger line
    COLORREF labelColor    { RGB(200,200,200) };   // Light gray text
    COLORREF borderColor   { RGB( 80,100, 80) };   // Grid border

    // Grid divisions (oscilloscope style)
    int    hDivisions{ 10 };
    int    vDivisions{ 8 };

    // Display info
    double secPerDiv{ 0.0 };
    double voltsPerDiv{ 0.0 };
    double triggerLevel{ 0.0 };

    // Perf counters
    float  fps{ 0.0f };
    uint32_t acqRate{ 0 };

    // Cursor state
    bool   cursorsEnabled{ false };
    double cursor1Time{ 0.0 };   // seconds
    double cursor2Time{ 0.0 };
    double cursor1Volt{ 0.0 };   // volts
    double cursor2Volt{ 0.0 };
};
