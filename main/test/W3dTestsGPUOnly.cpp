// ============================================================================
//  W3dTestsGPUOnly.cpp — comandos de harness de las MALLAS SOLO EN GPU (la opcion de proyecto
//  "mallasSoloGPU", libs/Whisk3DCore/objects/MeshResource.h): una malla que solo dibujan rutinas suelta su
//  RAM cuando sus VBO quedaron subidos. Los despacha W3dTestsResourcesCmd (test/W3dTestsResources.cpp).
//  La prueba: tools/pruebas/prueba_mallas_solo_gpu.w3s.
//
//  Comandos:
//    sologpu on|off | sologpu es 0|1
//        la opcion del proyecto (lo mismo que el tilde "GPU-only meshes" de la tarjeta Render) / su assert
//    sologpuinfo <malla> [sologpu 0|1] [ram 0|1] [rambytes N] [usuarios N] [recargas N] [flujo <parte> <prim> N]
//        el estado del recurso: si solto su RAM (y cuanta), si tiene sus arrays en RAM, sus VBO y los
//        bytes que el almacen le cuenta. Los asserts: onlyGPU, arrays en RAM, bytes del almacen, objetos que
//        la usan, cuantas veces se releyo una malla suelta (en toda la sesion) y cuantos indices manda el flujo
//        de esa parte con esa primitiva (W3dMeshFlowIndices: 0 = no se puede sin la RAM)
//    sologpucuadro <clave>
//        dibuja el layout (lo perezoso queda subido) y despues UN cuadro del viewport 3D, y anota con esa
//        clave: el hash de los pixeles del viewport y las llamadas GL de ese cuadro (total, draws, indices,
//        binds de textura, cambios de estado, otras -punteros, binds de VBO, matrices-, bytes subidos, tris)
//    sologpuigual <clave1> <clave2>
//        los dos cuadros son IDENTICOS: los mismos pixeles y las mismas llamadas GL
//    sologpuperder
//        EL CONTEXTO GL SE PERDIO: los VBO/IBO de las mallas se olvidan (W3dMeshesVBOLost), como lo
//        haria el motor con un contexto nuevo; el proximo dibujo los vuelve a subir
//    sologpumini <malla> [guardar <clave> | igual <clave>]
//        la miniatura de la malla en la biblioteca (el camino del editor que pide su RAM): su hash, anotado
//        con una clave o comparado con uno anotado
//    sologpubytes <a.w3d> <b.w3d> <entrada>
//        la entrada tiene los MISMOS bytes en los dos contenedores (guardar no reescribio la malla)
// ============================================================================
#include "test/W3dTestsResources.h"
#include "objects/MeshResource.h"
#include "objects/MeshFlows.h"        // W3dMeshFlowIndices
#include "io/Thumbnails.h"
#include "io/ProjectResources.h"       // W3D_VIEW_MESHES
#include "io/W3dZip.h"
#include "io/W3dResources.h"        // W3dResource::bytes (lo que el almacen le cuenta)
#include "graphics/w3dGraphics.h"
#include "ViewPorts/ViewPorts.h"       // rootViewport
#include "ViewPorts/ViewPort3D.h"      // Viewport3D
#include "WhiskUI/draw/glesdraw.h"     // W3dScreenHeight
#include <map>
#include <stdio.h>
#include <string.h>

extern Viewport3D* Viewport3DActive;
extern void LoadPendingTextures();

namespace {

namespace gfx = w3dEngine;

// el hash de unos bytes (FNV-1a de 32 bits)
unsigned Hash(const unsigned char* p, size_t n) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

// lo que se anota de un cuadro
enum { CCalls = 0, CDraws, CIndices, CBinds, CStates, COther, CUploaded, CTris, CN };
const char* const kNames[CN] = { "calls", "draws", "indices", "binds", "states", "other", "uploaded", "tris" };
struct Frame {
    unsigned hash;
    long n[CN];
    int w, h;
};
std::map<std::string, Frame> gFrames;

void Counters(long n[CN]) {
    long tot = 0;
    for (int c = 0; c < gfx::StatCatCount_; c++) tot += gfx::g_statCalls[c];
    n[CCalls] = tot;
    n[CDraws] = gfx::g_statDrawTris;
    n[CIndices] = gfx::g_statIndices;
    n[CBinds] = gfx::g_statTexBinds;
    n[CStates] = gfx::g_statStateChanges;
    n[COther] = gfx::g_otherStat;
    n[CUploaded] = gfx::g_statUploadBytes;
    n[CTris] = (long)gfx::g_opaqueStatTris + gfx::g_statTrisBlend;
}

bool CmdOnlyGPU(std::istringstream& ss, std::string& err) {
    std::string q; ss >> q;
    if (q == "es") {
        int v = -1; ss >> v;
        const int real = W3dMeshesOnlyGPUActive() ? 1 : 0;
        printf("      [sologpu] opcion %s\n", real ? "on" : "off");
        if (real != v) { err = real ? "onlygpu es: la opcion esta prendida" : "onlygpu es: la opcion esta apagada"; return false; }
        return true;
    }
    if (q != "on" && q != "off") { err = "onlygpu: uso: onlygpu on|off|es 0|1"; return false; }
    W3dMeshesOnlyGPUActivate(q == "on");
    printf("      [sologpu] opcion %s\n", W3dMeshesOnlyGPUActive() ? "on" : "off");
    return true;
}

bool CmdInfo(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    MeshResource* r = W3dMeshResourceByName(n);
    if (!r) { err = "onlygpuinfo: no hay una malla '" + n + "' en el registro"; return false; }
    const bool ram = r->vertex != NULL;
    const long recBytes = r->rec ? r->rec->bytes : -1;
    printf("      [sologpuinfo] '%s' soloGPU=%d ram=%d verts=%d indices=%d partes=%d usuarios=%d rutinas=%d "
           "vbo=%u/%u/%u/%u/%u soltados=%ld B almacen=%ld B\n", r->name.c_str(), r->onlyGPU ? 1 : 0, ram ? 1 : 0,
           r->vertexSize, r->facesSize, (int)r->parts.size(), (int)r->users.size(), r->refsRoutine,
           r->vboPos, r->vboNor, r->vboUV, r->vboCol, r->vboIdx, r->releasedBytes, recBytes);
    std::string k;
    while (ss >> k) {
        int part = 0, prim = 0;
        if (k == "flow" && !(ss >> part >> prim)) { err = "onlygpuinfo: flujo <parte> <prim> N"; return false; }
        long v = -1;
        if (!(ss >> v)) { err = "onlygpuinfo: falta el valor de '" + k + "'"; return false; }
        long real;
        if (k == "flow") {
            real = W3dMeshFlowIndices(r, part, prim);
            printf("      [sologpuinfo] '%s' flujo parte %d primitiva %d: %ld indices\n", n.c_str(), part, prim, real);
        }
        else if (k == "gpuonly") real = r->onlyGPU ? 1 : 0;
        else if (k == "ram") real = ram ? 1 : 0;
        else if (k == "rambytes") real = recBytes;
        else if (k == "users") real = (long)r->users.size();
        else if (k == "reloads") real = W3dMeshesOnlyGPUReloads();
        else { err = "onlygpuinfo: assert desconocido '" + k + "' (sologpu/ram/rambytes/usuarios/recargas)"; return false; }
        if (real != v) {
            char b[160]; snprintf(b, sizeof b, "onlygpuinfo '%s': %s = %ld, se esperaba %ld", n.c_str(), k.c_str(), real, v);
            err = b; return false;
        }
    }
    return true;
}

bool CmdFrame(std::istringstream& ss, std::string& err) {
    std::string key; ss >> key;
    if (key.empty()) { err = "onlygpuframe: falta la clave"; return false; }
    if (!rootViewport) { err = "onlygpuframe: no hay layout de viewports"; return false; }
    LoadPendingTextures();
    rootViewport->Render();   // puebla Viewport3DActive; lo perezoso (VBO, flujos, soltar la RAM) pasa aca
    Viewport3D* vp = Viewport3DActive;
    if (!vp || vp->width < 1 || vp->height < 1) { err = "onlygpuframe: no hay viewport 3D"; return false; }
    long a[CN], b[CN];
    Counters(a);
    vp->Render();             // EL cuadro que se compara
    gfx::Finish();
    Counters(b);
    Frame c;
    for (int i = 0; i < CN; i++) c.n[i] = b[i] - a[i];
    c.w = vp->width; c.h = vp->height;
    std::vector<unsigned char> px((size_t)c.w * c.h * 4, 0);
    gfx::ReadPixelsRGBA(vp->x, W3dScreenHeight - vp->y - vp->height, c.w, c.h, &px[0]);
    c.hash = Hash(&px[0], px.size());
    // (cuantos pixeles no son el fondo: que el cuadro dibujo algo, no un viewport vacio igual a otro vacio)
    long distinct = 0;
    for (size_t i = 4; i < px.size(); i += 4)
        if (px[i] != px[0] || px[i + 1] != px[1] || px[i + 2] != px[2]) distinct++;
    gFrames[key] = c;
    printf("      [sologpucuadro] %s: %dx%d hash=%08x px_no_fondo=%ld", key.c_str(), c.w, c.h, c.hash, distinct);
    for (int i = 0; i < CN; i++) printf(" %s=%ld", kNames[i], c.n[i]);
    printf("\n");
    return true;
}

bool CmdEqual(std::istringstream& ss, std::string& err) {
    std::string k1, k2; ss >> k1 >> k2;
    if (!gFrames.count(k1) || !gFrames.count(k2)) { err = "onlygpuequal: no hay un cuadro '" + k1 + "' o '" + k2 + "'"; return false; }
    const Frame& a = gFrames[k1];
    const Frame& b = gFrames[k2];
    if (a.w != b.w || a.h != b.h || a.hash != b.hash) {
        char t[200]; snprintf(t, sizeof t, "onlygpuequal: la imagen de '%s' (%08x) no es la de '%s' (%08x)", k1.c_str(), a.hash, k2.c_str(), b.hash);
        err = t; return false;
    }
    for (int i = 0; i < CN; i++)
        if (a.n[i] != b.n[i]) {
            char t[200]; snprintf(t, sizeof t, "onlygpuequal: %s: '%s' = %ld, '%s' = %ld", kNames[i], k1.c_str(), a.n[i], k2.c_str(), b.n[i]);
            err = t; return false;
        }
    printf("      [sologpuigual] '%s' = '%s' (pixeles y llamadas GL)\n", k1.c_str(), k2.c_str());
    return true;
}

bool CmdLose() {
    W3dMeshesVBOLost();
    printf("      [sologpuperder] los VBO/IBO de las mallas se olvidaron (contexto GL nuevo)\n");
    return true;
}

bool CmdMini(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    std::vector<unsigned char> px; int w = 0, h = 0;
    if (!W3dThumbnailPixels(W3D_VIEW_MESHES, n, px, w, h) || px.empty()) { err = "onlygpumini: sin miniatura de '" + n + "'"; return false; }
    long full = 0;
    for (size_t i = 3; i < px.size(); i += 4) if (px[i]) full++;
    const unsigned h0 = Hash(&px[0], px.size());
    printf("      [sologpumini] '%s' %dx%d hash=%08x px_con_malla=%ld\n", n.c_str(), w, h, h0, full);
    if (!full) { err = "onlygpumini: la miniatura de '" + n + "' salio vacia"; return false; }
    std::string q, key; ss >> q >> key;
    static std::map<std::string, unsigned> annotated;
    if (q == "save") annotated[key] = h0;
    else if (q == "equal") {
        if (!annotated.count(key)) { err = "onlygpumini: no hay una miniatura anotada '" + key + "'"; return false; }
        if (annotated[key] != h0) { err = "onlygpumini: la miniatura de '" + n + "' no es la anotada '" + key + "'"; return false; }
        printf("      [sologpumini] igual a '%s'\n", key.c_str());
    }
    return true;
}

bool CmdBytes(std::istringstream& ss, std::string& err) {
    std::string a, b, e; ss >> a >> b >> e;
    if (e.empty()) { err = "onlygpubytes: uso: onlygpubytes <a.w3d> <b.w3d> <entrada>"; return false; }
    const std::string paths[2] = { a, b };
    std::vector<unsigned char> data[2];
    for (int k = 0; k < 2; k++) {
        std::vector<W3dZipEntry> ents;
        if (!W3dZipRead(paths[k], &ents)) { err = "onlygpubytes: no pude leer " + paths[k]; return false; }
        bool is = false;
        for (size_t i = 0; i < ents.size() && !is; i++) if (ents[i].name == e) { data[k].swap(ents[i].data); is = true; }
        if (!is) { err = "sologpubytes: " + paths[k] + " no tiene la entrada '" + e + "'"; return false; }
    }
    const bool equal = data[0] == data[1];
    printf("      [sologpubytes] %s: %d B / %d B %s\n", e.c_str(), (int)data[0].size(), (int)data[1].size(),
           equal ? "IGUALES" : "DISTINTOS");
    if (!equal) { err = "onlygpubytes: la entrada '" + e + "' cambio al guardar"; return false; }
    return true;
}

} // namespace

// el despachador de esta area (lo llama W3dTestsResourcesCmd)
bool W3dTestsOnlyGPUCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "onlygpu")       return CmdOnlyGPU(ss, err);
    if (cmd == "onlygpuinfo")   return CmdInfo(ss, err);
    if (cmd == "onlygpuframe") return CmdFrame(ss, err);
    if (cmd == "onlygpuequal")  return CmdEqual(ss, err);
    if (cmd == "onlygpulose") return CmdLose();
    if (cmd == "onlygpumini")   return CmdMini(ss, err);
    if (cmd == "onlygpubytes")  return CmdBytes(ss, err);
    handled = false;
    return false;
}
