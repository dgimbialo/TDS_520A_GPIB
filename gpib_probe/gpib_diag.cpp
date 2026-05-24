// gpib_diag.cpp -- NI-488.2 GPIB Diagnostic Tool v2
#include <cstdio>
#include <cstring>
#include <windows.h>
#ifdef GET
#undef GET
#endif
#include "ni488.h"

static void rtrim(char* buf, long cnt)
{
    for (long i = cnt - 1; i >= 0; --i)
    {
        if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == ' ')
            buf[i] = 0;
        else
            break;
    }
}

static void pstat(const char* ctx)
{
    printf("  [%-14s] ibsta=0x%04X  iberr=%-2d  ibcntl=%ld",
           ctx, ibsta, iberr, ibcntl);
    if (ibsta & ERR)
    {
        switch (iberr)
        {
        case EDVR: printf("  EDVR(driver)");      break;
        case ECIC: printf("  ECIC(not CIC)");     break;
        case ENOL: printf("  ENOL(no listener)"); break;
        case EADR: printf("  EADR(addr err)");    break;
        case ESAC: printf("  ESAC(not SysCtrl)"); break;
        case EABO: printf("  EABO(timeout)");     break;
        case ENEB: printf("  ENEB(no board)");    break;
        case EBUS: printf("  EBUS(bus error)");   break;
        default:   printf("  iberr=%d", iberr);   break;
        }
    }
    printf("\n");
}

static void phase1_ibln(void)
{
    printf("\n=== Phase 1: ibln() listener scan addr 1-30 ===\n");
    printf("    (checks NDAC handshake -- no data written to bus)\n\n");
    int found = 0;
    for (int addr = 1; addr <= 30; ++addr)
    {
        short listen = 0;
        ibln(0, addr, NO_SAD, &listen);
        if (!(ibsta & ERR) && listen)
        {
            printf("    ** LISTENER at addr %2d **\n", addr);
            ++found;
        }
    }
    printf("\n    Listeners found: %d\n", found);
}

static void phase2_probe(int addr)
{
    printf("\n--- probe addr=%d ---\n", addr);
    int ud = ibdev(0, addr, 0, T3s, 1, 0x0A);
    printf("  ibdev ud=%d  ", ud); pstat("ibdev");
    if (ud < 0 || (ibsta & ERR)) return;

    ibclr(ud); pstat("ibclr"); Sleep(300);

    ibwrt(ud, (PVOID)"*IDN?", 5); pstat("ibwrt *IDN?");
    if (!(ibsta & ERR))
    {
        Sleep(100);
        char buf[512]; memset(buf,0,sizeof(buf));
        ibrd(ud, buf, (long)(sizeof(buf)-1)); pstat("ibrd");
        if (!(ibsta & ERR) && ibcntl > 0)
        {
            rtrim(buf, ibcntl);
            printf("  IDN: \"%s\"\n", buf);
        }
    }
    ibonl(ud, 0);
}

static void phase3_full(int addr)
{
    printf("\n=== Phase 3: full test addr=%d (T10s) ===\n", addr);
    int ud = ibdev(0, addr, 0, T10s, 1, 0x0A);
    printf("  ibdev ud=%d  ", ud); pstat("ibdev");
    if (ud < 0 || (ibsta & ERR)) return;

    short listen = 0;
    ibln(0, addr, NO_SAD, &listen);
    printf("  ibln listener=%d  ", (int)listen); pstat("ibln");
    if (!listen)
    {
        printf("  No listener -- scope not at addr %d or cable disconnected.\n", addr);
        ibonl(ud, 0); return;
    }

    ibclr(ud); pstat("ibclr"); Sleep(500);

    char buf[512]; memset(buf,0,sizeof(buf));
    ibwrt(ud, (PVOID)"*IDN?", 5); pstat("ibwrt *IDN?");
    if (!(ibsta & ERR))
    {
        Sleep(200);
        ibrd(ud, buf, (long)(sizeof(buf)-1)); pstat("ibrd");
        if (!(ibsta & ERR) && ibcntl > 0)
        {
            rtrim(buf, ibcntl);
            printf("  IDN = \"%s\"\n", buf);
        }
    }

    memset(buf,0,sizeof(buf));
    ibwrt(ud, (PVOID)"GPIBADDR?", 9); pstat("ibwrt GPIBADDR?");
    if (!(ibsta & ERR))
    {
        Sleep(100);
        ibrd(ud, buf, (long)(sizeof(buf)-1));
        if (!(ibsta & ERR) && ibcntl > 0)
        {
            rtrim(buf, ibcntl);
            printf("  Scope GPIB address (self-reported): \"%s\"\n", buf);
        }
    }

    ibonl(ud, 0); pstat("ibonl");
}

int main(void)
{
    printf("====================================================\n");
    printf("  NI-488.2 GPIB Diagnostic v2\n");
    printf("====================================================\n");

    // Try both "GPIB0" and "gpib0" -- the driver may be case-sensitive
    int board = ibfind("GPIB0");
    printf("\nibfind(\"GPIB0\") = %d  ", board); pstat("ibfind");
    if (board < 0 || (ibsta & ERR))
    {
        board = ibfind("gpib0");
        printf("ibfind(\"gpib0\") = %d  ", board); pstat("ibfind");
    }
    if (board < 0 || (ibsta & ERR))
    {
        printf("WARNING: ibfind failed -- will try ibdev(0,...) directly.\n");
    }
    else
    {
        printf("\nSending IFC (resets all devices on bus)...\n");
        ibsic(board); pstat("ibsic");
        Sleep(500);
        // Do NOT call ibonl(board,0) -- that takes the controller offline!
    }

    phase1_ibln();

    printf("\n=== Phase 2: full probe of every responding address ===\n");
    for (int addr = 1; addr <= 30; ++addr)
    {
        short listen = 0;
        ibln(0, addr, NO_SAD, &listen);
        if (!(ibsta & ERR) && listen)
            phase2_probe(addr);
    }

    phase3_full(1);

    printf("\n====================================================\n");
    printf("HINTS:\n");
    printf("  No listeners found:\n");
    printf("    -> Check scope is powered ON and GPIB cable is plugged in.\n");
    printf("    -> Verify scope GPIB address on front panel:\n");
    printf("       Utility > System I/O > GPIB > Address\n");
    printf("    -> Try pressing LOCAL on scope front panel.\n");
    printf("  Listener found but EBUS on ibwrt:\n");
    printf("    -> Scope is on bus but ignoring commands.\n");
    printf("    -> Press LOCAL key on scope to exit lockout.\n");
    printf("====================================================\n");
    printf("Press Enter to exit.\n");
    getchar();
    return 0;
}
