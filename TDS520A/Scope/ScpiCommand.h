// Scope/ScpiCommand.h - SCPI command builders for TDS 520A
#pragma once
#include "pch.h"
#include "ScopeChannel.h"

namespace Scpi
{
    // Identity
    inline std::string IDN()       { return "*IDN?"; }
    inline std::string RST()       { return "*RST"; }
    inline std::string CLS()       { return "*CLS"; }
    inline std::string OPC()       { return "*OPC"; }
    inline std::string OPCQuery()  { return "*OPC?"; }
    inline std::string ESR()       { return "*ESR?"; }

    // Waveform acquisition setup
    inline std::string DataSource(ScopeChannel ch)
    {
        return std::string("DATA:SOURCE ") + ChannelName(ch);
    }
    inline std::string DataSourceQuery()   { return "DATA:SOURCE?"; }
    inline std::string DataEncdg()         { return "DATA:ENCDG RIBinary"; } // Signed binary
    inline std::string DataWidth(int w)    { return "DATA:WIDTH " + std::to_string(w); } // 1 or 2 bytes
    inline std::string DataStartStop(int nrPt = 250) // clamp to actual record length
    {
        char buf[64];
        sprintf_s(buf, "DATA:START 1;DATA:STOP %d", nrPt > 0 ? nrPt : 500);
        return buf;
    }
    inline std::string WfmPre()            { return "WFMPRE?"; }
    inline std::string Curve()             { return "CURVE?"; }
    // WAVFRM? returns preamble + binary data in a single transfer.
    // Format: <preamble_ascii>%<binary_block>
    // This saves one full GPIB round-trip per acquisition cycle.
    inline std::string WavFrm()            { return "WAVFRM?"; }

    // Channel settings
    inline std::string ChScale(ScopeChannel ch, double v)
    {
        char buf[64];
        sprintf_s(buf, "CH%d:SCALE %g", (int)ch, v);
        return buf;
    }
    inline std::string ChScaleQuery(ScopeChannel ch)
    {
        return std::string("CH") + std::to_string((int)ch) + ":SCALE?";
    }
    inline std::string ChPosition(ScopeChannel ch, double v)
    {
        char buf[64];
        sprintf_s(buf, "CH%d:POSITION %g", (int)ch, v);
        return buf;
    }

    // Horizontal (time)
    inline std::string HorScale(double v)
    {
        char buf[64];
        sprintf_s(buf, "HORizontal:SCAle %g", v);
        return buf;
    }
    inline std::string HorScaleQuery()    { return "HORizontal:SCAle?"; }
    inline std::string HorRecordLen()     { return "HORizontal:RECOrdlength?"; }

    // Acquire
    inline std::string AcqState(bool run)  { return run ? "ACQuire:STATE RUN" : "ACQuire:STATE STOP"; }
    inline std::string AcqStateQuery()     { return "ACQuire:STATE?"; }
    inline std::string AcqMode(const char* mode) // SAMple, AVErage, ENVelope
    {
        return std::string("ACQuire:MODe ") + mode;
    }
    inline std::string AcqSingle()         { return "ACQuire:STOPAfter SEQuence"; }

    // Trigger
    inline std::string TrigStateQuery()    { return "TRIGger:STATE?"; }
    inline std::string ForceTrig()         { return "TRIGger FORCe"; }
    inline std::string TrigLevelQuery()    { return "TRIGger:MAIn:LEVel?"; }
    inline std::string TrigLevel(double v)
    {
        char buf[64];
        sprintf_s(buf, "TRIGger:MAIn:LEVel %g", v);
        return buf;
    }
}
