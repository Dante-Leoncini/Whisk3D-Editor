// ============================================================================
//  VALIDACION DE LAS RUTINAS (solo el EDITOR: los juegos exportados no la llevan)
//
//  Antes de dibujar una Rutina, el editor revisa que no rompa nada: referencias
//  que existan (y sigan vivas: un objeto borrado deja un puntero colgado), partes
//  y rangos de triangulos de la malla que existan (tambien los que trae un ARRAY
//  de memoria: se revisan cada frame, los escribe lua), memorias bien escritas,
//  opciones y luces en rango, la pila de matrices equilibrada (cada Apilar con su
//  Desapilar, tambien en lo que saltea un "Saltar si") y sin pasarse de
//  profundidad, y subrutinas sin ciclos. Si algo falla la rutina NO se ejecuta,
//  queda marcada (invalida + motivo) y el outliner la pinta en rojo.
//
//  Y lo que el editor muestra de los pasos: el ICONO de cada grupo y el menu Add
//  agrupado (Transform, Draw, Texture, Fog, Lights...), el mismo en el panel y en
//  Add > Core del viewport.
// ============================================================================
#include "objects/Routine.h"
#include "objects/Mesh.h"
#include "objects/Textures.h"
#include "objects/MeshResource.h"
#include "objects/MeshFlows.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"
#include "W3dLang.h"
#include "script/SimGame.h"
#include <set>
#include <stdio.h>
#include <string.h>
#include <limits.h>

// cuantos niveles de la pila de matrices puede usar una rutina (GL ES 1.1 garantiza 16 para la modelview y el
// arbol del editor ya usa algunos: uno por cada padre)
static const int kProfMax = 8;

// (con una cache como W3dObjectLive: cada paso de textura recorria la lista entera. Una entrada dice "t estaba en la
// generacion g"; cualquier textura que sale de la lista la sube)
static bool TextureLive(const void* t) {
    struct Entry { const void* t; unsigned gen; };
    static Entry cache[64];
    Entry& c = cache[((unsigned)(size_t)t * 2654435761u) >> 26];
    if (c.t == t && c.gen == g_w3dTexturesGen) return true;
    for (size_t i = 0; i < Textures.size(); i++)
        if (Textures[i] == t) { c.t = t; c.gen = g_w3dTexturesGen; return true; }
    return false;
}

// los punteros de una rutina siguen siendo validos (el objeto no se borro ni se renombro)
static bool UpToDatePointersAll(Routine* r) {
    for (int m = 0; m < Routine::ModeN; m++)
        for (size_t i = 0; i < r->lists[m].size(); i++) {
            const W3dStep& p = r->lists[m][i];
            if (!p.ref2.empty() && (!p.ptr2 || !W3dObjectLive((const Object*)p.ptr2) || ((const Object*)p.ptr2)->name != p.ref2))
                return false;
            const int rt = W3dStepRef(p.type);
            if (p.ref.empty() || rt == RefNothing || rt == RefFunction || rt == RefShader) continue;
            if (!p.ptr) return false;                                 // (se reintenta: quiza ya existe)
            if (rt == RefTexture) { if (!TextureLive(p.ptr)) return false; continue; }
            if (rt == RefMesh) {
                const MeshResource* m = (const MeshResource*)p.ptr;
                if (!W3dMeshResourceLive(m) || m->name != p.ref) return false;
                continue;
            }
            Object* o = (Object*)p.ptr;
            if (!W3dObjectLive(o) || o->name != p.ref) return false;
        }
    return true;
}
// Lo que deja un puntero colgado es una MUERTE (un objeto, una malla o una textura que se va): si desde la ultima
// revision completa no murio nada, los punteros siguen vivos. Los NOMBRES (un renombrado: el paso pasa a nombrar a
// otro, pero no cuelga nada) jugando no se revisan (antes cada 16 cuadros: todas las rutinas en el MISMO cuadro, miles
// de nombres comparados, un tiron cada 16 en el N95); editando, en cada uno como siempre
static bool PointersUpToDate(Routine* r) {
    const unsigned g0 = W3dObjectsGen(), g1 = W3dMeshesGen(), g2 = g_w3dTexturesGen;
    if (r->pointersOk && r->pointersGen[0] == g0 && r->pointersGen[1] == g1 && r->pointersGen[2] == g2 &&
        W3dGameRunning())
        return true;
    r->pointersOk = UpToDatePointersAll(r);
    r->pointersGen[0] = g0; r->pointersGen[1] = g1; r->pointersGen[2] = g2;
    r->pointersAge = 0;
    return r->pointersOk;
}

// un rango de triangulos [a, b] de una parte de 'tris' triangulos (b < 0 = hasta el ultimo). "" = esta bien
static std::string BadRange(int a, int b, int tris, const std::string& mesh) {
    char buf[320];   // (traducido puede ocupar el doble: el cirilico va en 2 bytes)
    if (a < 0 || a >= tris) {
        snprintf(buf, sizeof buf, T("first triangle %d does not exist: the part of '%s' has %d triangles (0..%d)"), a, mesh.c_str(), tris, tris - 1);
        return buf;
    }
    if (b >= tris) {
        snprintf(buf, sizeof buf, T("last triangle %d is past the end: the part of '%s' has %d triangles (0..%d, or max)"), b, mesh.c_str(), tris, tris - 1);
        return buf;
    }
    if (b >= 0 && b < a) {
        snprintf(buf, sizeof buf, T("last triangle %d is before the first one (%d)"), b, a);
        return buf;
    }
    return std::string();
}

// el estado de cada ARRAY al llegar a un paso (vertices, normales, uv, colores), recorriendo la lista en orden: a que
// malla apunta (los punteros) y si esta prendido (el paso "Array on/off"). La SELECCION de la malla va por un lado y
// el DIBUJO (los indices de una parte) por otro: cada draw se valida contra los arrays prendidos en ese momento. Una
// subrutina lo hereda y lo devuelve
struct Pointers {
    const MeshResource* m[4];   // la malla a la que apunta cada array (NULL = esta rutina todavia no lo apunto)
    signed char on[4];          // 1 prendido, 0 apagado, -1 no se sabe
    bool known[4];              // se sabe a que apunta (false = lo puso alguien de afuera: el que llama, lua)
    // una rutina del arbol arranca con los arrays COMO ESTABAN en ese punto del cuadro (lo que sabia el cache cuando
    // empezo a dibujarse: los prendio el constructor, los dejo otra malla; el motor no los toca) y sin punteros propios.
    // Una que no se dibuja sola (una libreria oculta: las de material) arranca sin saber nada
    Pointers(bool knownValue, const Routine* r) {
        for (int k = 0; k < 4; k++) {
            m[k] = 0; known[k] = knownValue;
            on[k] = (knownValue && r->arraysSeen) ? r->arraysStart[k] : (signed char)-1;
        }
        uvNormals = false; uvLayer = 0;
    }
    bool uvNormals;            // el puntero de UV son las NORMALES de su malla ("Matcap")
    int uvLayer;               // la capa de UV a la que apunta el de UV (0 = la de render; k = uvExtra[k])
    void Forget() { for (int k = 0; k < 4; k++) { m[k] = 0; on[k] = -1; known[k] = false; } uvNormals = false; uvLayer = 0; }
};
static const char* const kArrayName[4] = { "vertex array", "normal array", "UV array", "color array" };   // (avisos: pasan por T())
static const char* const kArrayStep[4] = { "Vertex pointer", "Normal pointer", "UV pointer", "Color pointer" };
// (en RAM o, si la malla solto su RAM -onlyGPU, MeshResource.h-, en su VBO)
static bool HasArray(const MeshResource* r, int k) {
    static const unsigned kMask[4] = { W3DMD_POS, W3DMD_NOR, W3DMD_UV, W3DMD_COL };
    return W3dMeshResourceHasArray(r, kMask[k]);
}
// la capa de UV k (> 0) de la malla: presente y con una UV por vertice
static bool HasLayerUV(const MeshResource* r, int layer) {
    return W3dMeshResourceHasLayerUV(r, layer);
}

// mientras se valida: lo que se LEE de las memorias (los rangos), para el cache de cuando se juega (ValidateRoutine)
static std::vector<std::pair<const float*, int> >* gReadings = NULL;
static void Read(const float* p, int n) { if (gReadings && p && n > 0) gReadings->push_back(std::make_pair(p, n)); }

// "Dibujar elementos": la malla, la parte, que los punteros alcancen sus indices y el rango
static bool DrawOk(const W3dStep& p, int num, const Pointers& pu, std::string& reason) {
    char buf[512];   // (traducido puede ocupar el doble: el cirilico va en 2 bytes)
    MeshResource* m = (MeshResource*)p.ptr;
    if (!W3dMeshResourceHasArray(m, W3DMD_FACES) || m->facesSize < 1) {
        snprintf(buf, sizeof buf, T("step %d: mesh '%s' has no geometry loaded"), num, p.ref.c_str()); reason = buf; return false;
    }
    const int nParts = (int)m->parts.size();
    if (p.integer < -1 || p.integer >= (nParts > 0 ? nParts : 1)) {
        snprintf(buf, sizeof buf, T("step %d: mesh '%s' has %d parts (part %d does not exist; -1 = all)"), num,
                 p.ref.c_str(), nParts, p.integer);
        reason = buf; return false;
    }
    if (p.mode < 0 || p.mode >= RPrimN) {
        snprintf(buf, sizeof buf, T("step %d (Draw elements): primitive %d does not exist"), num, p.mode); reason = buf; return false;
    }
    // los ARRAYS: el de vertices prendido, apuntado y alcanzando el indice mas alto de la parte; cada array que puede
    // estar PRENDIDO (normales, uv, colores) apuntado por esta rutina a una malla con ese array y AL MENOS los vertices
    // del de vertices (si tiene menos, GL lee de mas). Uno que no se sabe si esta prendido (lo dejo lo que se dibujo
    // antes, los hijos) hay que fijarlo con "Array on/off"
    if (pu.on[0] == 0) {
        snprintf(buf, sizeof buf, T("step %d (Draw elements): the vertex array is off (Array on/off)"), num); reason = buf; return false;
    }
    if (pu.known[0]) {
        const MeshResource* v = pu.m[0];
        if (!v) {
            snprintf(buf, sizeof buf, T("step %d (Draw elements): there is no Vertex pointer before it"), num); reason = buf; return false;
        }
        const int maxIdx = W3dMeshPartMaxIndex(m, p.integer);
        if (maxIdx >= v->vertexSize) {
            snprintf(buf, sizeof buf, T("step %d (Draw elements): the part uses vertices up to %d but the vertex pointer ('%s') has %d"),
                     num, maxIdx, v->name.c_str(), v->vertexSize);
            reason = buf; return false;
        }
        for (int k = 1; k < 4; k++) {
            if (pu.on[k] == 0 || !pu.known[k]) continue;
            const MeshResource* q = pu.m[k];
            if (!q) {
                if (pu.on[k] == 1)
                    snprintf(buf, sizeof buf, T("step %d (Draw elements): the %s is on but this routine did not point it: add a %s "
                             "before (or turn the array off with Array on/off)"), num, T(kArrayName[k]), T(kArrayStep[k]));
                else
                    snprintf(buf, sizeof buf, T("step %d (Draw elements): the %s may be on (it depends on what was drawn "
                             "before): turn it on or off with Array on/off before drawing"), num, T(kArrayName[k]));
                reason = buf; return false;
            }
            if (!(k == 2 && pu.uvNormals ? HasArray(q, 1) : (k == 2 && pu.uvLayer > 0) ? HasLayerUV(q, pu.uvLayer) : HasArray(q, k))) {
                snprintf(buf, sizeof buf, T("step %d (Draw elements): the %s is on but its pointer's mesh ('%s') has no %s "
                         "(turn the array off with Array on/off)"), num, T(kArrayName[k]), q->name.c_str(), T(kArrayName[k]));
                reason = buf; return false;
            }
            if (q->vertexSize < v->vertexSize) {
                snprintf(buf, sizeof buf, T("step %d (Draw elements): the %s ('%s', %d vertices) has fewer vertices than the "
                         "Vertex pointer ('%s', %d)"), num, T(kArrayStep[k]), q->name.c_str(), q->vertexSize,
                         v->name.c_str(), v->vertexSize);
                reason = buf; return false;
            }
        }
    }
    if (p.sub == RangePart) return true;
    if (p.mode != RPrimTriangles) {
        snprintf(buf, sizeof buf, T("step %d (Draw elements): a manual range or an array needs the Triangles primitive"), num);
        reason = buf; return false;
    }
    const int tris = W3dMeshFlowIndices(m, p.integer, RPrimTriangles) / 3;
    if (p.sub == RangeManual) {
        Read(p.n[0].p, 1); Read(p.n[1].p, 1);
        std::string e = BadRange((int)p.n[0].Value(), (int)p.n[1].Value(), tris, p.ref);
        if (e.empty()) return true;
        snprintf(buf, sizeof buf, T("step %d (Draw elements): %s"), num, e.c_str()); reason = buf; return false;
    }
    if (p.sub == RangeArray) {
        const W3dNum& arr = p.n[2];
        if (arr.ref.empty()) {
            snprintf(buf, sizeof buf, T("step %d (Draw elements): choose the array (a memory: @name[i])"), num); reason = buf; return false;
        }
        const float* a = arr.p;
        const W3dNum& ban = p.n[3];   // (opcional: con banderas cada rango lleva la suya)
        if (!ban.ref.empty() && !ban.p) {
            snprintf(buf, sizeof buf, T("step %d (Draw elements): the flags have to be a memory (@name[i])"), num); reason = buf; return false;
        }
        const int width = ban.p ? 3 : 2;
        const int i0 = W3dMemoryIndex(arr.ref);
        const int banSize = ban.p ? W3D_MEMORY_SIZE - W3dMemoryIndex(ban.ref) : 0;
        const int maxPairs = (W3D_MEMORY_SIZE - i0 - 1) / width;
        const int k = (int)a[0];
        Read(a, 1 + ((k > 0 && k <= maxPairs) ? k * width : 0));
        if (k < 0 || k > maxPairs) {
            snprintf(buf, sizeof buf, T("step %d (Draw elements): array %s says %d ranges (0..%d fit from there)"), num,
                     arr.ref.c_str(), k, maxPairs);
            reason = buf; return false;
        }
        for (int j = 0; j < k; j++) {
            const float* e0 = a + 1 + width * j;
            if (ban.p && (e0[2] < 0 || (int)e0[2] >= banSize)) {
                snprintf(buf, sizeof buf, T("step %d (Draw elements): array %s, range %d: flag %d out of %s"), num, arr.ref.c_str(), j,
                         (int)e0[2], ban.ref.c_str());
                reason = buf; return false;
            }
            const int ra = (int)e0[0], rb = (int)e0[1];
            if (ra >= 0 && ra < tris && rb < tris && (rb < 0 || rb >= ra)) continue;   // (lo de BadRange, sin armar texto)
            std::string e = BadRange(ra, rb, tris, p.ref);
            if (e.empty()) continue;
            snprintf(buf, sizeof buf, T("step %d (Draw elements): array %s, range %d: %s"), num, arr.ref.c_str(), j, e.c_str());
            reason = buf; return false;
        }
        return true;
    }
    snprintf(buf, sizeof buf, T("step %d (Draw elements): unknown range %d"), num, p.sub); reason = buf; return false;
}

// la profundidad maxima que agrega una lista y si cierra en 0; -1 = error (motivo escrito)
static int Depth(Routine* r, std::vector<W3dStep>& L, std::string& reason, std::set<Routine*>& stack, int level,
                       Pointers& pu);

static int ProfRoutine(Routine* r, std::string& reason, std::set<Routine*>& stack, int level, Pointers& pu) {
    if (stack.count(r)) {
        char buf[400];   // (snprintf acotado: un nombre larguisimo se corta, no desborda)
        snprintf(buf, sizeof buf, T("routine '%s' calls itself (cycle)"), r->name.c_str());
        reason = buf; return -1;
    }
    if (level > 8) { reason = T("too many nested routines"); return -1; }
    if (r->dirty || !PointersUpToDate(r)) r->Resolve();   // (una subrutina que nunca se dibujo sola: sus nombres)
    stack.insert(r);
    int prof = Depth(r, r->CurrentList(), reason, stack, level, pu);
    stack.erase(r);
    return prof;
}

static int Depth(Routine* r, std::vector<W3dStep>& L, std::string& reason, std::set<Routine*>& stack, int level,
                       Pointers& pu) {
    char buf[400];   // (traducido puede ocupar el doble: el cirilico va en 2 bytes)
    int d = 0, maxd = 0;
    // donde termina cada salto (lo salteado tiene que dejar la pila como la encontro): por paso destino, la profundidad
    // que tiene que tener la pila al llegar (INT_MIN = ningun salto llega ahi; INT_MAX = llegan dos con distinta, que es
    // un error seguro). Antes cada paso recorria todos los saltos vistos: con cientos de "Saltar si" era cuadratico
    std::vector<int> expected;
    for (size_t i = 0; i < L.size(); i++) {
        W3dStep& p = L[i];
        const int num = (int)i + 1;
        if (p.type >= StepN) { snprintf(buf, sizeof buf, T("step %d: unknown type"), num); reason = buf; return -1; }
        for (int k = 0; k < W3dStepNumbers(p); k++)
            if (!p.n[k].ref.empty() && !p.n[k].p) {
                snprintf(buf, sizeof buf, T("step %d (%s): bad memory reference '%s' (use @name[0..%d])"), num,
                         T(W3dStepLabel(p.type)), p.n[k].ref.c_str(), W3D_MEMORY_SIZE - 1);
                reason = buf; return -1;
            }
        const int rt = W3dStepRef(p.type);
        if (rt == RefFunction && p.ref.empty()) {
            snprintf(buf, sizeof buf, T("step %d (%s): write the name of the Lua function"), num, T(W3dStepLabel(p.type)));
            reason = buf; return -1;
        }
        if (p.type == StepLua && !p.ref2.empty() && !p.ptr2) {
            snprintf(buf, sizeof buf, T("step %d (%s): object '%s' not found"), num, T(W3dStepLabel(p.type)), p.ref2.c_str());
            reason = buf; return -1;
        }
        const bool boxInMemory = (p.type == StepVisible && p.ref.empty() && p.n[1].p);   // (la caja, 6 numeros de una memoria)
        const bool clearNoBackground = (p.type == StepClear && p.mode != ClearBackgroundOwn);   // (la imagen, solo con el fondo propio)
        if (rt != RefNothing && rt != RefFunction && rt != RefShader && !p.ptr && !(p.type == StepMatcap && !p.on) && !boxInMemory &&
            !clearNoBackground) {   // (apagar el matcap: sin malla)
            if (p.ref.empty()) snprintf(buf, sizeof buf, T("step %d (%s): nothing chosen"), num, T(W3dStepLabel(p.type)));
            else snprintf(buf, sizeof buf, T("step %d (%s): '%s' not found"), num, T(W3dStepLabel(p.type)), p.ref.c_str());
            reason = buf; return -1;
        }
        if (!p.ref2.empty() && !p.ptr2) {
            snprintf(buf, sizeof buf, T("step %d (%s): object '%s' not found"), num, T(W3dStepLabel(p.type)), p.ref2.c_str());
            reason = buf; return -1;
        }
        const int opt = W3dStepOptions(p.type);
        if (opt != OptNothing && (p.mode < 0 || p.mode >= W3dOptionsN(opt))) {
            snprintf(buf, sizeof buf, T("step %d (%s): option %d does not exist (0..%d)"), num, T(W3dStepLabel(p.type)),
                     p.mode, W3dOptionsN(opt) - 1);
            reason = buf; return -1;
        }
        if (W3dStepInteger(p.type) == IntegerLight && (p.integer < 0 || p.integer >= W3D_ROUTINE_LIGHTS)) {
            snprintf(buf, sizeof buf, T("step %d (%s): light %d does not exist (0..%d)"), num, T(W3dStepLabel(p.type)),
                     p.integer, W3D_ROUTINE_LIGHTS - 1);
            reason = buf; return -1;
        }
        switch (p.type) {
        case StepStack: d++; if (d > maxd) maxd = d; break;
        case StepPop:
            if (--d < 0) { snprintf(buf, sizeof buf, T("step %d: Pop matrix without its Push"), num); reason = buf; return -1; }
            break;
        case StepArray:
            pu.on[p.mode] = p.on ? 1 : 0;
            break;
        case StepPointerVertices: case StepPointerNormals: case StepPointerUV: case StepPointerColors: {
            // (que la malla tenga ese array se pide al dibujar con el array prendido: apuntar no lee nada)
            const int k = p.type - StepPointerVertices;
            pu.m[k] = (const MeshResource*)p.ptr; pu.known[k] = true;
            if (k == 2) {
                pu.uvNormals = false;
                pu.uvLayer = p.integer;
                if (p.integer < 0 || (p.integer > 0 && !HasLayerUV((const MeshResource*)p.ptr, p.integer))) {
                    snprintf(buf, sizeof buf, T("step %d (UV pointer): mesh '%s' has no UV layer %d (0 = its render UV)"), num,
                             p.ref.c_str(), p.integer);
                    reason = buf; return -1;
                }
            }
            break;
        }
        case StepMatcap:
            if (p.on && p.ptr) { pu.m[2] = (const MeshResource*)p.ptr; pu.known[2] = true; pu.uvNormals = true; pu.uvLayer = 0; }
            break;
        case StepDraw:
            if (!DrawOk(p, num, pu, reason)) return -1;
            break;
        case StepPointerPose: case StepDrawVisible: {
            // un OBJETO malla con su recurso: la pose tiene sus vertices (paralela al recurso) y la lista visible son
            // triangulos de el. La pose cuenta como el puntero de vertices; el dibujo, como "Draw elements" de todo
            Object* o = (Object*)p.ptr;
            const Mesh* me = (o && o->getType() == ObjectType::mesh) ? (const Mesh*)o : NULL;
            if (!me || !me->mesh) {
                snprintf(buf, sizeof buf, T("step %d (%s): '%s' is not a mesh object with a library mesh"), num,
                         T(W3dStepLabel(p.type)), p.ref.c_str());
                reason = buf; return -1;
            }
            if (p.type == StepPointerPose) {
                const int k = (p.mode >= 0 && p.mode < 4) ? p.mode : 0;
                pu.m[k] = me->mesh; pu.known[k] = true;
                if (k == 2) { pu.uvNormals = false; pu.uvLayer = 0; }
                break;
            }
            W3dStep q = W3dNewStep(StepDraw);
            q.ptr = me->mesh; q.ref = me->mesh->name;
            if (!DrawOk(q, num, pu, reason)) return -1;
            break;
        }
        case StepChildren:
            // los hijos ponen SUS punteros y prenden o apagan arrays: despues hay que volver a apuntar (y fijar los arrays)
            for (int k = 0; k < 4; k++) { pu.m[k] = 0; pu.known[k] = true; pu.on[k] = -1; }
            pu.uvNormals = false; pu.uvLayer = 0;
            break;
        case StepLua:
            pu.Forget();   // (lua puede poner cualquier cosa)
            break;
        case StepVisible:
            if (p.n[0].ref.empty() || !p.n[0].p) {
                snprintf(buf, sizeof buf, T("step %d (Visibility test): 'Write to' has to be a memory (@name[i])"), num);
                reason = buf; return -1;
            }
            if (p.ptr && (p.integer < -1 || p.integer >= (int)((const MeshResource*)p.ptr)->parts.size() + (((const MeshResource*)p.ptr)->parts.empty() ? 1 : 0))) {
                snprintf(buf, sizeof buf, T("step %d (Visibility test): part %d does not exist (-1 = the whole mesh)"), num, p.integer);
                reason = buf; return -1;
            }
            break;
        case StepRoutine: {
            // la subrutina se valida con los punteros de este punto (y deja los suyos)
            int sub = ProfRoutine((Routine*)p.ptr, reason, stack, level + 1, pu);
            if (sub < 0) return -1;
            if (d + sub > maxd) maxd = d + sub;
            break;
        }
        case StepSkipIf: case StepSkipHidden: case StepSkipIfNone: case StepSkipOutside: case StepSkipRail: case StepSkipNoListing:
            if (p.integer < 0 || i + (size_t)p.integer >= L.size()) {
                snprintf(buf, sizeof buf, T("step %d: skips %d steps but only %d follow"), num, p.integer, (int)(L.size() - i - 1));
                reason = buf; return -1;
            }
            {
                if (expected.empty()) expected.assign(L.size(), INT_MIN);
                int& e = expected[i + (size_t)p.integer];
                if (e == INT_MIN) e = d; else if (e != d) e = INT_MAX;
            }
            break;
        default: break;
        }
        if (!expected.empty() && expected[i] != INT_MIN && expected[i] != d) {
            snprintf(buf, sizeof buf, T("step %d: the steps a 'Skip if zero' can skip leave the matrix stack unbalanced"), num);
            reason = buf; return -1;
        }
    }
    if (d != 0) { snprintf(buf, sizeof buf, T("%d Push matrix without its Pop"), d); reason = buf; return -1; }
    if (maxd > kProfMax) { snprintf(buf, sizeof buf, T("matrix stack too deep (%d levels, max %d)"), maxd, kProfMax); reason = buf; return -1; }
    return maxd;
}

// la rutina la DIBUJA el arbol (ella y todos sus padres visibles); una oculta es una libreria que se llama
static bool DrawsTree(Object* o) {
    for (; o; o = o->Parent) if (!o->visible) return false;
    return true;
}

// lo que una validacion leyo de las memorias sigue igual (la copia contra lo de ahora)
static bool MemoriesEqual(const Routine* r) {
    size_t k = 0;
    for (size_t i = 0; i < r->valReadings.size(); i++) {
        const int n = r->valReadings[i].second;
        if (memcmp(r->valReadings[i].first, &r->valCopy[k], (size_t)n * sizeof(float)) != 0) return false;
        k += (size_t)n;
    }
    return true;
}

static bool ValidateRoutine(Routine* r) {
    if (r->dirty || !PointersUpToDate(r)) r->Resolve();
    const bool known = DrawsTree(r);
    // JUGANDO, el resultado de la vez anterior si no cambio nada de lo que lee: los pasos (suyos o de una subrutina) y
    // los datos de las mallas (g_w3dRoutinesGen), los punteros (PointersUpToDate, arriba), si la dibuja el arbol, los arrays
    // al empezar y lo que leyo de las memorias (los rangos que escribe lua). Editando, cada cuadro como siempre. Validar
    // todo cada cuadro era lo mas caro del cuadro: miles de pasos (en el N95, milisegundos; la revision entera cada 120
    // cuadros que habia de reaseguro era un tiron de decenas de ms en la rutina del mundo)
    const bool playing = W3dGameRunning();
    if (playing && r->valOk && r->valGen == g_w3dRoutinesGen && r->valKnown == known && r->valSeen == r->arraysSeen &&
        memcmp(r->valArrays, r->arraysStart, sizeof r->valArrays) == 0 && MemoriesEqual(r))
        return !r->invalid;
    std::vector<std::pair<const float*, int> > readings;
    std::vector<std::pair<const float*, int> >* prev = gReadings;
    gReadings = &readings;
    std::string reason;
    std::set<Routine*> stack;
    stack.insert(r);
    bool ok = true;
    // todas las listas que algun modo usa (no solo la del modo actual: al cambiar de modo no tiene que reventar)
    for (int m = 0; m < Routine::ModeN && ok; m++) {
        bool used = (m == Routine::ModeDefault);
        for (int k = 0; k < Routine::ModeN; k++) if (r->use[k] == m) used = true;
        if (!used) continue;
        Pointers pu(known, r);
        if (Depth(r, r->lists[m], reason, stack, 0, pu) < 0) ok = false;
    }
    gReadings = prev;
    r->invalid = !ok;
    r->reason = ok ? std::string() : reason;
    // para la vez siguiente: lo que leyo (y una copia) y con que estado
    r->valReadings.swap(readings);
    r->valCopy.clear();
    for (size_t i = 0; i < r->valReadings.size(); i++)
        r->valCopy.insert(r->valCopy.end(), r->valReadings[i].first, r->valReadings[i].first + r->valReadings[i].second);
    r->valOk = playing;
    r->valGen = g_w3dRoutinesGen;
    r->valKnown = known;
    r->valSeen  = r->arraysSeen;
    memcpy(r->valArrays, r->arraysStart, sizeof r->valArrays);
    r->valAge = (int)((r->serial * 37u) % 120u);   // (repartidas: no se revalidan todas en el mismo cuadro)
    return ok;
}

// lo engancha el editor al arrancar (los juegos exportados no compilan este archivo: el hook queda en NULL)
struct RoutineValidateInstall { RoutineValidateInstall() { W3dRoutineValidateHook = ValidateRoutine; } };
static RoutineValidateInstall gRoutineValidateInstall;

// (harness / panel) valida sin dibujar
bool W3dRoutineValidate(Routine* r) { return ValidateRoutine(r); }

// ---------------------------------------------------------------------------
//  ICONOS: uno por grupo (Transform = la camara, Fog = la niebla, Lights = la lampara...) y algunos pasos el suyo
// ---------------------------------------------------------------------------
int W3dRoutineGroupIcon(int g) {
    switch (g) {
    case GroupTransform: return (int)IconType::camera;
    case GroupDraw:     return (int)IconType::mesh;
    case GroupTexture:     return (int)IconType::texture;
    case GroupFog:      return (int)IconType::fog;
    case GroupLights:       return (int)IconType::light;
    case GroupAlpha:        return (int)IconType::filter;
    case GroupColor:       return (int)IconType::material;
    case GroupDepth: return (int)IconType::clearz;
    case GroupMix:      return (int)IconType::equal;
    case GroupFaces:       return (int)IconType::visible;
    default:               return (int)IconType::arrowRight;
    }
}
int W3dRoutineStepIcon(int type) {
    switch (type) {
    case StepArray:           return (int)IconType::list;
    case StepPointerVertices: return (int)IconType::selVertex;
    case StepPointerPose:     return (int)IconType::selVertex;
    case StepDrawVisible: return (int)IconType::visible;
    case StepPointerNormals: return (int)IconType::normalVertex;
    case StepPointerUV:       return (int)IconType::texture;
    case StepPointerColors:  return (int)IconType::material;
    case StepChildren:  return (int)IconType::object;
    case StepRoutine: return (int)IconType::list;
    case StepVisible: return (int)IconType::visible;
    case StepSizePoint:   return (int)IconType::selVertex;
    case StepWidthLine: return (int)IconType::selEdge;
    case StepLua:     return (int)IconType::script;
    case StepMatcap:  return (int)IconType::normalVertex;
    case StepSkipHidden: return (int)IconType::hidden;
    default:         return W3dRoutineGroupIcon(W3dStepGroupOf(type));
    }
}
// el icono de una rutina (outliner y "Llamar rutina"): la de UN paso (las de Add > Core) lleva el de su paso; la que
// solo pone ESTADO (no dibuja ni llama a nada: un material hecho rutina), el de material
int W3dRoutineIcon(Routine* r) {
    if (r->constructor) return (int)IconType::modifier;   // (el constructor: la llave)
    const std::vector<W3dStep>& L = r->lists[Routine::ModeDefault];
    if (L.size() == 1) return W3dRoutineStepIcon(L[0].type);
    for (size_t i = 0; i < L.size(); i++)
        if (W3dStepGroupOf(L[i].type) == GroupDraw || L[i].type == StepLua || L[i].type == StepVisible)
            return (int)IconType::list;
    return (int)IconType::material;
}

// ---------------------------------------------------------------------------
//  el menu de PASOS agrupado: un submenu por grupo (un grupo de un solo paso va suelto). El id de cada item es el
//  tipo de paso; con 'withEmpty', "Empty routine" (id StepN) arriba de todo. 'subs' son los submenus (los crea la
//  primera vez; despues los rearma).
// ---------------------------------------------------------------------------
void W3dRoutineMenuSteps(PopupMenu* root, std::vector<PopupMenu*>& subs, void (*action)(int), bool withEmpty) {
    if (!root) return;
    root->Clear();
    root->action = action;
    if (subs.size() < (size_t)GroupN) subs.resize(GroupN, (PopupMenu*)NULL);
    if (withEmpty) root->Add(T("Empty routine"), StepN, (int)IconType::list);
    if (withEmpty) root->Add(T("Routine loop"), StepN + 1, (int)IconType::list);   // la lista corre N veces
    for (int g = 0; g < GroupN; g++) {
        int n = 0, unique = -1;
        for (int t = 0; t < StepN; t++) if (W3dStepGroupOf(t) == g) { n++; unique = t; }
        if (n == 0) continue;
        if (n == 1) { root->Add(T(W3dStepLabel(unique)), unique, W3dRoutineStepIcon(unique)); continue; }
        if (!subs[g]) subs[g] = new PopupMenu();
        PopupMenu* sub = subs[g];
        sub->Clear();
        sub->action = action;
        for (int t = 0; t < StepN; t++)
            if (W3dStepGroupOf(t) == g) sub->Add(T(W3dStepLabel(t)), t, W3dRoutineStepIcon(t));
        root->Add(T(W3dGroupLabel(g)), 1000 + g, W3dRoutineGroupIcon(g), sub);
    }
}
