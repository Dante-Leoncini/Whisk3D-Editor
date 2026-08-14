#include "LOD.h"
#include "objects/CameraBase.h" // g_renderCamPos: la vista que se esta dibujando
#include "Camera.h"             // CameraActive (la camara del juego)
#include "Culling.h"            // W3dBoundsSubarbol / W3dDist2PuntoAabb (las MISMAS del Culling)
#include <math.h>
#include <sstream>
#include <algorithm> // std::replace (parseo "20, 45, 90" -> numeros)
#include <string.h>  // memcmp/memcpy de la matriz cacheada

// desde donde se mide (ver el comentario largo del .h)
//
// JUGANDO SE MIDE DESDE LA CAMARA DEL JUEGO, SIEMPRE.
// Reporte del dueno, tercera vez: "siguen superpuestas el castillo y carpas de
// los distintos LOD en vez de cambiar entre el de menos poligonos cuando esta
// lejos y el de alta calidad cuando estas mas cerca".
// La causa de que NO CONMUTE es esta linea. `g_renderCamPos` es la vista que
// esta dibujando, y dar Play en el editor NO cambia la vista del viewport:
// JuegoPrepararViewports solo apaga los overlays, no toca ViewFromCameraActive.
// O sea que con el juego corriendo el LOD seguia midiendo desde la ORBITA DEL
// EDITOR, que no se mueve: el personaje camina por el nivel y las distancias no
// cambian nunca. Medido en un nivel real con `simplay 3` + `lodinfo`: los 28 LOD del
// escenario daban distancias de 600 a 780 unidades (la orbita esta en el origen
// y el nivel esta a ~600 de ahi) contra un umbral de 30 -> los 28 elegian el
// nivel GRUESO, jugaras donde jugaras.
// Con `soloCamaraActiva: true` en el .w3d ya andaba, pero eso obliga a que cada
// LOD lo declare y no es lo que uno espera: JUGANDO, el punto de vista es el del
// juego. Ahora se usa CameraActive si el LOD lo pide O si el juego esta
// corriendo; en el editor en pausa sigue mandando la vista que dibuja (que es lo
// que uno quiere para ver el LOD orbitando).
// La eleccion de camara vive en UNA sola puerta (W3dCamaraDeMedida, Culling.cpp):
// asi LOD y Culling nunca discrepan, y sobre todo NADIE escribe la vista del
// viewport para conseguirla (ver el comentario de W3dCamaraDeMedida).
Vector3 LOD::OjoDeMedida() const {
    Camera* c = W3dCamaraDeMedida(soloCamaraActiva);
    return c ? c->GetGlobalPosition() : g_renderCamPos;
}

// refresca el cache del AABB de cada hijo si hace falta (cantidad de hijos o
// matriz de mundo del LOD distintas de las de la ultima vez).
void LOD::RefrescarCache() const {
    Matrix4 W; const_cast<LOD*>(this)->GetWorldMatrix(W);
    if (cacheVal && cacheOk.size() == Childrens.size() && memcmp(cacheW, W.m, sizeof(cacheW)) == 0) return;
    memcpy(cacheW, W.m, sizeof(cacheW));
    const size_t n = Childrens.size();
    cacheMin.assign(n, Vector3(0,0,0));
    cacheMax.assign(n, Vector3(0,0,0));
    cacheOk.assign(n, 0);
    for (size_t i = 0; i < n; i++) {
        Vector3 mn, mx;
        if (W3dBoundsSubarbol(Childrens[i], mn, mx)) { cacheMin[i] = mn; cacheMax[i] = mx; cacheOk[i] = 1; }
    }
    cacheVal = true;
}

// distancia de la camara al AABB en mundo del hijo i (0 si esta adentro).
// <0 = ese hijo no tiene ni una malla medible.
float LOD::DistanciaAHijo(int i) const {
    if (i < 0 || i >= (int)Childrens.size()) return -1.0f;
    RefrescarCache();
    if (i >= (int)cacheOk.size() || !cacheOk[(size_t)i]) return -1.0f;
    return sqrtf(W3dDist2PuntoAabb(OjoDeMedida(), cacheMin[(size_t)i], cacheMax[(size_t)i]));
}

// que hijo corresponde a la distancia actual.
//  - la distancia se mide de la camara al AABB EN MUNDO del hijo candidato, no al
//    origen del LOD: el origen es el centro de la pareja y hacia conmutar a "lejos"
//    con el jugador parado sobre el borde del trozo;
//  - con HISTERESIS del 10 %: para SUBIR de detalle hay que acercarse a umbral*0.9,
//    para BAJAR hay que alejarse a umbral*1.1. Sin esto, justo en el umbral la
//    pareja parpadea;
//  - un hijo SIN geometria medible (empty, solo curvas...) cae a la distancia al
//    origen del LOD, que es el comportamiento viejo: nunca se queda sin criterio.
int LOD::HijoElegido() const {
    int n = (int)Childrens.size();
    if (n == 0) { ultimoElegido = -1; return -1; }
    RefrescarCache();
    const Vector3 ojo = OjoDeMedida();
    Vector3 dOrigen = GetGlobalPosition() - ojo;
    const float distOrigen = sqrtf(dOrigen.x*dOrigen.x + dOrigen.y*dOrigen.y + dOrigen.z*dOrigen.z);

    int elegido = n - 1; // mas alla del ultimo umbral (o sin umbrales): el ultimo hijo
    for (int i = 0; i < n && i < (int)distancias.size(); i++) {
        float dist = (i < (int)cacheOk.size() && cacheOk[(size_t)i])
                   ? sqrtf(W3dDist2PuntoAabb(ojo, cacheMin[(size_t)i], cacheMax[(size_t)i]))
                   : distOrigen;
        // el umbral efectivo se estira un 10 % a favor del nivel que YA estabamos
        // dibujando (ultimoElegido <= i) y se encoge un 10 % para ganarselo.
        const float umbral = distancias[i] * ((ultimoElegido >= 0 && ultimoElegido <= i) ? 1.1f : 0.9f);
        if (dist <= umbral) { elegido = i; break; }
    }
    ultimoElegido = elegido;
    return elegido;
}

void LOD::RenderHijos() {
    // misma HERENCIA que el Culling: si este LOD designo camara, su subarbol
    // (otro LOD/Culling adentro del hijo elegido) mide desde la misma camara.
    W3dHerenciaMedida herencia(soloCamaraActiva);
    int e = HijoElegido();
    if (e >= 0) Childrens[e]->Render();
}

std::string LOD::DistanciasTexto() const {
    std::ostringstream ss;
    for (size_t i = 0; i < distancias.size(); i++) {
        if (i) ss << ", ";
        ss << distancias[i];
    }
    return ss.str();
}

// parseo TOLERANTE (mismo idioma que el "background" del lector de texto):
// comas -> espacios y se leen los floats que haya. Un texto roto no puede
// romper nada: deja los umbrales que alcanzo a leer.
void LOD::SetDistanciasTexto(const std::string& s) {
    std::string t = s;
    std::replace(t.begin(), t.end(), ',', ' ');
    std::stringstream ss(t);
    distancias.clear();
    float v;
    while (ss >> v) distancias.push_back(v);
}
