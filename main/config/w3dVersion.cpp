#include "w3dVersion.h"
#include <cstdio>

// El header lo regenera el build (cmake/W3dGenVersion.cmake) en cada compilacion, para que la fecha
// sea la del BUILD y no la de cuando se configuro cmake. Android y Symbian no lo usan: pasan
// W3D_VERSION directo desde su propio build system.
#ifdef W3D_VERSION_GEN
#include "w3dVersionGen.h"
#endif

const char* W3dVersion() {
    static char show[16] = {0};
    if (show[0]) return show;
#ifdef W3D_VERSION
    // el build system ya la paso en "YY.MM.DD" (CMake string(TIMESTAMP) / Android.mk `date`)
    snprintf(show, sizeof(show), "%s", W3D_VERSION);
#else
    // fallback: parsear __DATE__ ("Mmm DD YYYY", ej "Jul 10 2026") -> "YY.MM.DD"
    const char* d = __DATE__;
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int month = 1;
    for (int i = 0; i < 12; i++)
        if (d[0]==months[i*3] && d[1]==months[i*3+1] && d[2]==months[i*3+2]) { month = i+1; break; }
    int day  = (d[4]==' ' ? 0 : (d[4]-'0'))*10 + (d[5]-'0');
    int year = ((d[9]-'0')*10 + (d[10]-'0')); // ultimos 2 digitos del anio
    snprintf(show, sizeof(show), "%02d.%02d.%02d", year, month, day);
#endif
    return show;
}

const char* W3dVersionBuild() {
    static char show[32] = {0};
    if (show[0]) return show;
    const char* t = __TIME__;   // "HH:MM:SS" de cuando se compilo este archivo
    snprintf(show, sizeof(show), "%s %c%c:%c%c", W3dVersion(), t[0], t[1], t[3], t[4]);
    return show;
}
