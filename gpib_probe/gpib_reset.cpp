#include <cstdio>
#include <windows.h>
#ifdef GET
#undef GET
#endif
#include "ni488.h"

int main(void)
{
    printf("Attempting to bring GPIB board 0 back online...\n");

    // Use ibdev to get a handle to the board itself (pad=0 means controller)
    // First try ibfind
    int bd = ibfind("GPIB0");
    printf("ibfind(GPIB0) = %d  ibsta=0x%04X iberr=%d\n", bd, ibsta, iberr);

    if (bd < 0 || (ibsta & ERR))
    {
        bd = ibfind("gpib0");
        printf("ibfind(gpib0) = %d  ibsta=0x%04X iberr=%d\n", bd, ibsta, iberr);
    }

    if (bd >= 0 && !(ibsta & ERR))
    {
        printf("Calling ibonl(%d, 1) to bring online...\n", bd);
        ibonl(bd, 1);
        printf("ibonl done  ibsta=0x%04X iberr=%d\n", ibsta, iberr);

        printf("Calling ibsic to send IFC...\n");
        ibsic(bd);
        printf("ibsic done  ibsta=0x%04X iberr=%d\n", ibsta, iberr);
        Sleep(500);

        // Quick listener scan
        printf("\nListener scan:\n");
        int found = 0;
        for (int a = 1; a <= 10; ++a)
        {
            short listen = 0;
            ibln(0, a, NO_SAD, &listen);
            if (!(ibsta & ERR) && listen)
            {
                printf("  LISTENER at addr %d\n", a);
                ++found;
            }
        }
        printf("Listeners found (1-10): %d\n", found);
    }
    else
    {
        printf("ibfind failed entirely. Trying ibdev approach...\n");
        // ibdev opens a virtual device handle - won't help bring board online
        // but let's see what we get
        int ud = ibdev(0, 1, 0, T10s, 1, 0x0A);
        printf("ibdev(board=0, pad=1) = %d  ibsta=0x%04X iberr=%d\n", ud, ibsta, iberr);
        if (ud >= 0 && !(ibsta & ERR))
        {
            short listen = 0;
            ibln(0, 1, NO_SAD, &listen);
            printf("ibln(addr=1): listener=%d  ibsta=0x%04X iberr=%d\n", (int)listen, ibsta, iberr);
        }
    }

    printf("\nDone. Press Enter.\n");
    getchar();
    return 0;
}
