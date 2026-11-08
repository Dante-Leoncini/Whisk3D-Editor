// ============================================================================
//  W3dPruebasSoloGPU.cpp — comandos de harness de las MALLAS SOLO EN GPU (la opcion de proyecto
//  "mallasSoloGPU", libs/Whisk3DCore/objects/MallaRecurso.h): una malla que solo dibujan rutinas suelta su
//  RAM cuando sus VBO quedaron subidos. Los despacha W3dPruebasRecursosCmd (test/W3dPruebasRecursos.cpp).
//  La prueba: tools/pruebas/prueba_mallas_solo_gpu.w3s.
//
//  Comandos:
//    sologpu on|off | sologpu es 0|1
//        la opcion del proyecto (lo mismo que el tilde "GPU-only meshes" de la tarjeta Render) / su assert
//    sologpuinfo <malla> [sologpu 0|1] [ram 0|1] [rambytes N] [usuarios N] [recargas N] [flujo <parte> <prim> N]
//        el estado del recurso: si solto su RAM (y cuanta), si tiene sus arrays en RAM, sus VBO y los
//        bytes que el almacen le cuenta. Los asserts: soloGPU, arrays en RAM, bytes del almacen, objetos que
//        la usan, cuantas veces se releyo una malla suelta (en toda la sesion) y cuantos indices manda el flujo
//        de esa parte con esa primitiva (W3dMallaFlujoIndices: 0 = no se puede sin la RAM)
//    sologpucuadro <clave>
//        dibuja el layout (lo perezoso queda subido) y despues UN cuadro del viewport 3D, y anota con esa
//        clave: el hash de los pixeles del viewport y las llamadas GL de ese cuadro (total, draws, indices,
//        binds de textura, cambios de estado, otras -punteros, binds de VBO, matrices-, bytes subidos, tris)
//    sologpuigual <clave1> <clave2>
//        los dos cuadros son IDENTICOS: los mismos pixeles y las mismas llamadas GL
//    sologpuperder
//        EL CONTEXTO GL SE PERDIO: los VBO/IBO de las mallas se olvidan (W3dMallasVBOPerdidos), como lo
//        haria el motor con un contexto nuevo; el proximo dibujo los vuelve a subir
//    sologpumini <malla> [guardar <clave> | igual <clave>]
//        la miniatura de la malla en la biblioteca (el camino del editor que pide su RAM): su hash, anotado
//        con una clave o comparado con uno anotado
//    sologpubytes <a.w3d> <b.w3d> <entrada>
//        la entrada tiene los MISMOS bytes en los dos contenedores (guardar no reescribio la malla)
// ============================================================================
#include "test/W3dPruebasRecursos.h"
#include "objects/MallaRecurso.h"
#include "objects/MallaFlujos.h"        // W3dMallaFlujoIndices
#include "io/Miniaturas.h"
#include "io/RecursosProyecto.h"       // W3D_VISTA_MALLAS
#include "io/W3dZip.h"
#include "io/W3dRecursos.h"        // W3dRecurso::bytes (lo que el almacen le cuenta)
#include "graphics/w3dGraphics.h"
#include "ViewPorts/ViewPorts.h"       // rootViewport
#include "ViewPorts/ViewPort3D.h"      // Viewport3D
#include "WhiskUI/draw/glesdraw.h"     // W3dPantallaAlto
#include <map>
#include <stdio.h>
#include <string.h>

extern Viewport3D* Viewport3DActive;
extern void CargarTexturasPendientes();

namespace {

namespace gfx = w3dEngine;

// el hash de unos bytes (FNV-1a de 32 bits)
unsigned Hash(const unsigned char* p, size_t n) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

// lo que se anota de un cuadro
enum { CLlamadas = 0, CDraws, CIndices, CBinds, CEstados, COtras, CSubidos, CTris, CN };
const char* const kNombres[CN] = { "llamadas", "draws", "indices", "binds", "estados", "otras", "subidos", "tris" };
struct Cuadro {
    unsigned hash;
    long n[CN];
    int w, h;
};
std::map<std::string, Cuadro> gCuadros;

void Contadores(long n[CN]) {
    long tot = 0;
    for (int c = 0; c < gfx::StatCatCount_; c++) tot += gfx::g_statLlamadas[c];
    n[CLlamadas] = tot;
    n[CDraws] = gfx::g_statDrawTris;
    n[CIndices] = gfx::g_statIndices;
    n[CBinds] = gfx::g_statTexBinds;
    n[CEstados] = gfx::g_statStateChanges;
    n[COtras] = gfx::g_statOtras;
    n[CSubidos] = gfx::g_statUploadBytes;
    n[CTris] = (long)gfx::g_statTrisOpacos + gfx::g_statTrisBlend;
}

bool CmdSoloGPU(std::istringstream& ss, std::string& err) {
    std::string q; ss >> q;
    if (q == "es") {
        int v = -1; ss >> v;
        const int real = W3dMallasSoloGPUActiva() ? 1 : 0;
        printf("      [sologpu] opcion %s\n", real ? "prendida" : "apagada");
        if (real != v) { err = real ? "sologpu es: la opcion esta prendida" : "sologpu es: la opcion esta apagada"; return false; }
        return true;
    }
    if (q != "on" && q != "off") { err = "sologpu: uso: sologpu on|off|es 0|1"; return false; }
    W3dMallasSoloGPUActivar(q == "on");
    printf("      [sologpu] opcion %s\n", W3dMallasSoloGPUActiva() ? "prendida" : "apagada");
    return true;
}

bool CmdInfo(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    MallaRecurso* r = W3dMallaRecursoPorNombre(n);
    if (!r) { err = "sologpuinfo: no hay una malla '" + n + "' en el registro"; return false; }
    const bool ram = r->vertex != NULL;
    const long recBytes = r->rec ? r->rec->bytes : -1;
    printf("      [sologpuinfo] '%s' soloGPU=%d ram=%d verts=%d indices=%d partes=%d usuarios=%d rutinas=%d "
           "vbo=%u/%u/%u/%u/%u soltados=%ld B almacen=%ld B\n", r->nombre.c_str(), r->soloGPU ? 1 : 0, ram ? 1 : 0,
           r->vertexSize, r->facesSize, (int)r->partes.size(), (int)r->usuarios.size(), r->refsRutina,
           r->vboPos, r->vboNor, r->vboUV, r->vboCol, r->vboIdx, r->bytesSoltados, recBytes);
    std::string k;
    while (ss >> k) {
        int parte = 0, prim = 0;
        if (k == "flujo" && !(ss >> parte >> prim)) { err = "sologpuinfo: flujo <parte> <prim> N"; return false; }
        long v = -1;
        if (!(ss >> v)) { err = "sologpuinfo: falta el valor de '" + k + "'"; return false; }
        long real;
        if (k == "flujo") {
            real = W3dMallaFlujoIndices(r, parte, prim);
            printf("      [sologpuinfo] '%s' flujo parte %d primitiva %d: %ld indices\n", n.c_str(), parte, prim, real);
        }
        else if (k == "sologpu") real = r->soloGPU ? 1 : 0;
        else if (k == "ram") real = ram ? 1 : 0;
        else if (k == "rambytes") real = recBytes;
        else if (k == "usuarios") real = (long)r->usuarios.size();
        else if (k == "recargas") real = W3dMallasSoloGPURecargas();
        else { err = "sologpuinfo: assert desconocido '" + k + "' (sologpu/ram/rambytes/usuarios/recargas)"; return false; }
        if (real != v) {
            char b[160]; snprintf(b, sizeof b, "sologpuinfo '%s': %s = %ld, se esperaba %ld", n.c_str(), k.c_str(), real, v);
            err = b; return false;
        }
    }
    return true;
}

bool CmdCuadro(std::istringstream& ss, std::string& err) {
    std::string clave; ss >> clave;
    if (clave.empty()) { err = "sologpucuadro: falta la clave"; return false; }
    if (!rootViewport) { err = "sologpucuadro: no hay layout de viewports"; return false; }
    CargarTexturasPendientes();
    rootViewport->Render();   // puebla Viewport3DActive; lo perezoso (VBO, flujos, soltar la RAM) pasa aca
    Viewport3D* vp = Viewport3DActive;
    if (!vp || vp->width < 1 || vp->height < 1) { err = "sologpucuadro: no hay viewport 3D"; return false; }
    long a[CN], b[CN];
    Contadores(a);
    vp->Render();             // EL cuadro que se compara
    gfx::Finish();
    Contadores(b);
    Cuadro c;
    for (int i = 0; i < CN; i++) c.n[i] = b[i] - a[i];
    c.w = vp->width; c.h = vp->height;
    std::vector<unsigned char> px((size_t)c.w * c.h * 4, 0);
    gfx::ReadPixelsRGBA(vp->x, W3dPantallaAlto - vp->y - vp->height, c.w, c.h, &px[0]);
    c.hash = Hash(&px[0], px.size());
    // (cuantos pixeles no son el fondo: que el cuadro dibujo algo, no un viewport vacio igual a otro vacio)
    long distintos = 0;
    for (size_t i = 4; i < px.size(); i += 4)
        if (px[i] != px[0] || px[i + 1] != px[1] || px[i + 2] != px[2]) distintos++;
    gCuadros[clave] = c;
    printf("      [sologpucuadro] %s: %dx%d hash=%08x px_no_fondo=%ld", clave.c_str(), c.w, c.h, c.hash, distintos);
    for (int i = 0; i < CN; i++) printf(" %s=%ld", kNombres[i], c.n[i]);
    printf("\n");
    return true;
}

bool CmdIgual(std::istringstream& ss, std::string& err) {
    std::string k1, k2; ss >> k1 >> k2;
    if (!gCuadros.count(k1) || !gCuadros.count(k2)) { err = "sologpuigual: no hay un cuadro '" + k1 + "' o '" + k2 + "'"; return false; }
    const Cuadro& a = gCuadros[k1];
    const Cuadro& b = gCuadros[k2];
    if (a.w != b.w || a.h != b.h || a.hash != b.hash) {
        char t[200]; snprintf(t, sizeof t, "sologpuigual: la imagen de '%s' (%08x) no es la de '%s' (%08x)", k1.c_str(), a.hash, k2.c_str(), b.hash);
        err = t; return false;
    }
    for (int i = 0; i < CN; i++)
        if (a.n[i] != b.n[i]) {
            char t[200]; snprintf(t, sizeof t, "sologpuigual: %s: '%s' = %ld, '%s' = %ld", kNombres[i], k1.c_str(), a.n[i], k2.c_str(), b.n[i]);
            err = t; return false;
        }
    printf("      [sologpuigual] '%s' = '%s' (pixeles y llamadas GL)\n", k1.c_str(), k2.c_str());
    return true;
}

bool CmdPerder() {
    W3dMallasVBOPerdidos();
    printf("      [sologpuperder] los VBO/IBO de las mallas se olvidaron (contexto GL nuevo)\n");
    return true;
}

bool CmdMini(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    std::vector<unsigned char> px; int w = 0, h = 0;
    if (!W3dMiniaturaPixeles(W3D_VISTA_MALLAS, n, px, w, h) || px.empty()) { err = "sologpumini: sin miniatura de '" + n + "'"; return false; }
    long llenos = 0;
    for (size_t i = 3; i < px.size(); i += 4) if (px[i]) llenos++;
    const unsigned h0 = Hash(&px[0], px.size());
    printf("      [sologpumini] '%s' %dx%d hash=%08x px_con_malla=%ld\n", n.c_str(), w, h, h0, llenos);
    if (!llenos) { err = "sologpumini: la miniatura de '" + n + "' salio vacia"; return false; }
    std::string q, clave; ss >> q >> clave;
    static std::map<std::string, unsigned> anotadas;
    if (q == "guardar") anotadas[clave] = h0;
    else if (q == "igual") {
        if (!anotadas.count(clave)) { err = "sologpumini: no hay una miniatura anotada '" + clave + "'"; return false; }
        if (anotadas[clave] != h0) { err = "sologpumini: la miniatura de '" + n + "' no es la anotada '" + clave + "'"; return false; }
        printf("      [sologpumini] igual a '%s'\n", clave.c_str());
    }
    return true;
}

bool CmdBytes(std::istringstream& ss, std::string& err) {
    std::string a, b, e; ss >> a >> b >> e;
    if (e.empty()) { err = "sologpubytes: uso: sologpubytes <a.w3d> <b.w3d> <entrada>"; return false; }
    const std::string rutas[2] = { a, b };
    std::vector<unsigned char> datos[2];
    for (int k = 0; k < 2; k++) {
        std::vector<W3dZipEntrada> ents;
        if (!W3dZipLeer(rutas[k], &ents)) { err = "sologpubytes: no pude leer " + rutas[k]; return false; }
        bool esta = false;
        for (size_t i = 0; i < ents.size() && !esta; i++) if (ents[i].nombre == e) { datos[k].swap(ents[i].datos); esta = true; }
        if (!esta) { err = "sologpubytes: " + rutas[k] + " no tiene la entrada '" + e + "'"; return false; }
    }
    const bool iguales = datos[0] == datos[1];
    printf("      [sologpubytes] %s: %d B / %d B %s\n", e.c_str(), (int)datos[0].size(), (int)datos[1].size(),
           iguales ? "IGUALES" : "DISTINTOS");
    if (!iguales) { err = "sologpubytes: la entrada '" + e + "' cambio al guardar"; return false; }
    return true;
}

} // namespace

// el despachador de esta area (lo llama W3dPruebasRecursosCmd)
bool W3dPruebasSoloGPUCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "sologpu")       return CmdSoloGPU(ss, err);
    if (cmd == "sologpuinfo")   return CmdInfo(ss, err);
    if (cmd == "sologpucuadro") return CmdCuadro(ss, err);
    if (cmd == "sologpuigual")  return CmdIgual(ss, err);
    if (cmd == "sologpuperder") return CmdPerder();
    if (cmd == "sologpumini")   return CmdMini(ss, err);
    if (cmd == "sologpubytes")  return CmdBytes(ss, err);
    manejado = false;
    return false;
}
