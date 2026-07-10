#include "w3dVersion.h"
#include <cstdio>
#include <cstring>

const char* W3dVersion() {
    static char ver[16] = {0};
    if (ver[0]) return ver;
#ifdef W3D_VERSION
    // el build system ya la paso en "YY.MM.DD" (CMake string(TIMESTAMP) / Android.mk `date`)
    snprintf(ver, sizeof(ver), "%s", W3D_VERSION);
#else
    // fallback: parsear __DATE__ ("Mmm DD YYYY", ej "Jul 10 2026") -> "YY.MM.DD"
    const char* d = __DATE__;
    static const char meses[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int mes = 1;
    for (int i = 0; i < 12; i++)
        if (d[0]==meses[i*3] && d[1]==meses[i*3+1] && d[2]==meses[i*3+2]) { mes = i+1; break; }
    int dia  = (d[4]==' ' ? 0 : (d[4]-'0'))*10 + (d[5]-'0');
    int anio = ((d[9]-'0')*10 + (d[10]-'0')); // ultimos 2 digitos del anio
    snprintf(ver, sizeof(ver), "%02d.%02d.%02d", anio, mes, dia);
#endif
    return ver;
}
