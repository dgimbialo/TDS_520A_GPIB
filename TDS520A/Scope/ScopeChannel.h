// Scope/ScopeChannel.h - Channel enumeration and helpers
#pragma once

enum class ScopeChannel : int
{
    CH1 = 1,
    CH2 = 2,
    CH3 = 3,
    CH4 = 4
};

inline const char* ChannelName(ScopeChannel ch)
{
    switch (ch)
    {
    case ScopeChannel::CH1: return "CH1";
    case ScopeChannel::CH2: return "CH2";
    case ScopeChannel::CH3: return "CH3";
    case ScopeChannel::CH4: return "CH4";
    default:                return "CH1";
    }
}
