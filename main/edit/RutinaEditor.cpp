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
#include "objects/Rutina.h"
#include "objects/Mesh.h"
#include "objects/Textures.h"
#include "objects/MallaRecurso.h"
#include "objects/MallaFlujos.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"
#include "W3dLang.h"
#include "script/SimJuego.h"
#include <set>
#include <stdio.h>
#include <string.h>
#include <limits.h>

// cuantos niveles de la pila de matrices puede usar una rutina (GL ES 1.1 garantiza 16 para la modelview y el
// arbol del editor ya usa algunos: uno por cada padre)
static const int kProfMax = 8;

// (con una cache como W3dObjetoVivo: cada paso de textura recorria la lista entera. Una entrada dice "t estaba en la
// generacion g"; cualquier textura que sale de la lista la sube)
static bool TexturaViva(const void* t) {
    struct Entrada { const void* t; unsigned gen; };
    static Entrada cache[64];
    Entrada& c = cache[((unsigned)(size_t)t * 2654435761u) >> 26];
    if (c.t == t && c.gen == g_w3dTexturasGen) return true;
    for (size_t i = 0; i < Textures.size(); i++)
        if (Textures[i] == t) { c.t = t; c.gen = g_w3dTexturasGen; return true; }
    return false;
}

// los punteros de una rutina siguen siendo validos (el objeto no se borro ni se renombro)
static bool PunterosAlDiaTodo(Rutina* r) {
    for (int m = 0; m < Rutina::ModoN; m++)
        for (size_t i = 0; i < r->listas[m].size(); i++) {
            const W3dPaso& p = r->listas[m][i];
            if (!p.ref2.empty() && (!p.ptr2 || !W3dObjetoVivo((const Object*)p.ptr2) || ((const Object*)p.ptr2)->name != p.ref2))
                return false;
            const int rt = W3dPasoRef(p.tipo);
            if (p.ref.empty() || rt == RefNada || rt == RefFuncion) continue;
            if (!p.ptr) return false;                                 // (se reintenta: quiza ya existe)
            if (rt == RefTextura) { if (!TexturaViva(p.ptr)) return false; continue; }
            if (rt == RefMalla) {
                const MallaRecurso* m = (const MallaRecurso*)p.ptr;
                if (!W3dMallaRecursoVivo(m) || m->nombre != p.ref) return false;
                continue;
            }
            const Object* o = (const Object*)p.ptr;
            if (!W3dObjetoVivo(o) || o->name != p.ref) return false;
        }
    return true;
}
// Lo que deja un puntero colgado es una MUERTE (un objeto, una malla o una textura que se va): si desde la ultima
// revision completa no murio nada, los punteros siguen vivos. Los NOMBRES (un renombrado: el paso pasa a nombrar a
// otro, pero no cuelga nada) jugando se revisan cada 16 cuadros; editando, en cada uno como siempre. Era la mitad de
// la validacion: cada paso con su nombre comparado, cuadro por cuadro
static bool PunterosAlDia(Rutina* r) {
    const unsigned g0 = W3dObjetosGen(), g1 = W3dMallasGen(), g2 = g_w3dTexturasGen;
    if (r->punterosOk && r->punterosGen[0] == g0 && r->punterosGen[1] == g1 && r->punterosGen[2] == g2 &&
        W3dJuegoCorriendo() && ++r->punterosEdad < 16)
        return true;
    r->punterosOk = PunterosAlDiaTodo(r);
    r->punterosGen[0] = g0; r->punterosGen[1] = g1; r->punterosGen[2] = g2;
    r->punterosEdad = 0;
    return r->punterosOk;
}

// un rango de triangulos [a, b] de una parte de 'tris' triangulos (b < 0 = hasta el ultimo). "" = esta bien
static std::string RangoMalo(int a, int b, int tris, const std::string& malla) {
    char buf[220];
    if (a < 0 || a >= tris) {
        snprintf(buf, sizeof buf, "first triangle %d does not exist: the part of '%s' has %d triangles (0..%d)", a, malla.c_str(), tris, tris - 1);
        return buf;
    }
    if (b >= tris) {
        snprintf(buf, sizeof buf, "last triangle %d is past the end: the part of '%s' has %d triangles (0..%d, or max)", b, malla.c_str(), tris, tris - 1);
        return buf;
    }
    if (b >= 0 && b < a) {
        snprintf(buf, sizeof buf, "last triangle %d is before the first one (%d)", b, a);
        return buf;
    }
    return std::string();
}

// el estado de cada ARRAY al llegar a un paso (vertices, normales, uv, colores), recorriendo la lista en orden: a que
// malla apunta (los punteros) y si esta prendido (el paso "Array on/off"). La SELECCION de la malla va por un lado y
// el DIBUJO (los indices de una parte) por otro: cada draw se valida contra los arrays prendidos en ese momento. Una
// subrutina lo hereda y lo devuelve
struct Punteros {
    const MallaRecurso* m[4];   // la malla a la que apunta cada array (NULL = esta rutina todavia no lo apunto)
    signed char on[4];          // 1 prendido, 0 apagado, -1 no se sabe
    bool conocido[4];           // se sabe a que apunta (false = lo puso alguien de afuera: el que llama, lua)
    // una rutina del arbol arranca con los arrays COMO ESTABAN en ese punto del cuadro (lo que sabia el cache cuando
    // empezo a dibujarse: los prendio el constructor, los dejo otra malla; el motor no los toca) y sin punteros propios.
    // Una que no se dibuja sola (una libreria oculta: las de material) arranca sin saber nada
    Punteros(bool sabido, const Rutina* r) {
        for (int k = 0; k < 4; k++) {
            m[k] = 0; conocido[k] = sabido;
            on[k] = (sabido && r->arraysVistos) ? r->arraysInicio[k] : (signed char)-1;
        }
        uvNormales = false;
    }
    bool uvNormales;            // el puntero de UV son las NORMALES de su malla ("Matcap")
    void Olvidar() { for (int k = 0; k < 4; k++) { m[k] = 0; on[k] = -1; conocido[k] = false; } uvNormales = false; }
};
static const char* const kArrayNombre[4] = { "vertex", "normal", "UV", "color" };
static const char* const kArrayPaso[4] = { "Vertex pointer", "Normal pointer", "UV pointer", "Color pointer" };
static bool TieneArray(const MallaRecurso* r, int k) {
    return k == 0 ? r->vertex != 0 : k == 1 ? r->normals != 0 : k == 2 ? r->uv != 0 : r->vertexColor != 0;
}

// mientras se valida: lo que se LEE de las memorias (los rangos), para el cache de cuando se juega (ValidarRutina)
static std::vector<std::pair<const float*, int> >* gLecturas = NULL;
static void Leido(const float* p, int n) { if (gLecturas && p && n > 0) gLecturas->push_back(std::make_pair(p, n)); }

// "Dibujar elementos": la malla, la parte, que los punteros alcancen sus indices y el rango
static bool DibujarOk(const W3dPaso& p, int num, const Punteros& pu, std::string& motivo) {
    char buf[300];
    MallaRecurso* m = (MallaRecurso*)p.ptr;
    if (!m->faces || m->facesSize < 1) {
        snprintf(buf, sizeof buf, "step %d: mesh '%s' has no geometry loaded", num, p.ref.c_str()); motivo = buf; return false;
    }
    const int nPartes = (int)m->partes.size();
    if (p.entero < -1 || p.entero >= (nPartes > 0 ? nPartes : 1)) {
        snprintf(buf, sizeof buf, "step %d: mesh '%s' has %d parts (part %d does not exist; -1 = all)", num,
                 p.ref.c_str(), nPartes, p.entero);
        motivo = buf; return false;
    }
    if (p.modo < 0 || p.modo >= RPrimN) {
        snprintf(buf, sizeof buf, "step %d (Draw elements): primitive %d does not exist", num, p.modo); motivo = buf; return false;
    }
    // los ARRAYS: el de vertices prendido, apuntado y alcanzando el indice mas alto de la parte; cada array que puede
    // estar PRENDIDO (normales, uv, colores) apuntado por esta rutina a una malla con ese array y AL MENOS los vertices
    // del de vertices (si tiene menos, GL lee de mas). Uno que no se sabe si esta prendido (lo dejo lo que se dibujo
    // antes, los hijos) hay que fijarlo con "Array on/off"
    if (pu.on[0] == 0) {
        snprintf(buf, sizeof buf, "step %d (Draw elements): the vertex array is off (Array on/off)", num); motivo = buf; return false;
    }
    if (pu.conocido[0]) {
        const MallaRecurso* v = pu.m[0];
        if (!v) {
            snprintf(buf, sizeof buf, "step %d (Draw elements): there is no Vertex pointer before it", num); motivo = buf; return false;
        }
        const int maxIdx = W3dMallaParteMaxIndice(m, p.entero);
        if (maxIdx >= v->vertexSize) {
            snprintf(buf, sizeof buf, "step %d (Draw elements): the part uses vertices up to %d but the vertex pointer ('%s') has %d",
                     num, maxIdx, v->nombre.c_str(), v->vertexSize);
            motivo = buf; return false;
        }
        for (int k = 1; k < 4; k++) {
            if (pu.on[k] == 0 || !pu.conocido[k]) continue;
            const MallaRecurso* q = pu.m[k];
            if (!q) {
                if (pu.on[k] == 1)
                    snprintf(buf, sizeof buf, "step %d (Draw elements): the %s array is on but this routine did not point it: add a %s "
                             "before (or turn the array off with Array on/off)", num, kArrayNombre[k], kArrayPaso[k]);
                else
                    snprintf(buf, sizeof buf, "step %d (Draw elements): the %s array may be on (it depends on what was drawn "
                             "before): turn it on or off with Array on/off before drawing", num, kArrayNombre[k]);
                motivo = buf; return false;
            }
            if (!(k == 2 && pu.uvNormales ? q->normals != 0 : TieneArray(q, k))) {
                snprintf(buf, sizeof buf, "step %d (Draw elements): the %s array is on but its pointer's mesh ('%s') has no %s array "
                         "(turn the array off with Array on/off)", num, kArrayNombre[k], q->nombre.c_str(), kArrayNombre[k]);
                motivo = buf; return false;
            }
            if (q->vertexSize < v->vertexSize) {
                snprintf(buf, sizeof buf, "step %d (Draw elements): the %s pointer ('%s', %d vertices) has fewer vertices than the "
                         "vertex pointer ('%s', %d)", num, kArrayNombre[k], q->nombre.c_str(), q->vertexSize,
                         v->nombre.c_str(), v->vertexSize);
                motivo = buf; return false;
            }
        }
    }
    if (p.sub == RangoParte) return true;
    if (p.modo != RPrimTriangulos) {
        snprintf(buf, sizeof buf, "step %d (Draw elements): a manual range or an array needs the Triangles primitive", num);
        motivo = buf; return false;
    }
    const int tris = W3dMallaFlujoIndices(m, p.entero, RPrimTriangulos) / 3;
    if (p.sub == RangoManual) {
        Leido(p.n[0].p, 1); Leido(p.n[1].p, 1);
        std::string e = RangoMalo((int)p.n[0].Valor(), (int)p.n[1].Valor(), tris, p.ref);
        if (e.empty()) return true;
        snprintf(buf, sizeof buf, "step %d (Draw elements): %s", num, e.c_str()); motivo = buf; return false;
    }
    if (p.sub == RangoArray) {
        const W3dNum& arr = p.n[2];
        if (arr.ref.empty()) {
            snprintf(buf, sizeof buf, "step %d (Draw elements): choose the array (a memory: @name[i])", num); motivo = buf; return false;
        }
        const float* a = arr.p;
        const W3dNum& ban = p.n[3];   // (opcional: con banderas cada rango lleva la suya)
        if (!ban.ref.empty() && !ban.p) {
            snprintf(buf, sizeof buf, "step %d (Draw elements): the flags have to be a memory (@name[i])", num); motivo = buf; return false;
        }
        const int ancho = ban.p ? 3 : 2;
        const int i0 = W3dMemoriaIndice(arr.ref);
        const int banTam = ban.p ? W3D_MEMORIA_TAM - W3dMemoriaIndice(ban.ref) : 0;
        const int maxPares = (W3D_MEMORIA_TAM - i0 - 1) / ancho;
        const int k = (int)a[0];
        Leido(a, 1 + ((k > 0 && k <= maxPares) ? k * ancho : 0));
        if (k < 0 || k > maxPares) {
            snprintf(buf, sizeof buf, "step %d (Draw elements): array %s says %d ranges (0..%d fit from there)", num,
                     arr.ref.c_str(), k, maxPares);
            motivo = buf; return false;
        }
        for (int j = 0; j < k; j++) {
            const float* e0 = a + 1 + ancho * j;
            if (ban.p && (e0[2] < 0 || (int)e0[2] >= banTam)) {
                snprintf(buf, sizeof buf, "step %d (Draw elements): array %s, range %d: flag %d out of %s", num, arr.ref.c_str(), j,
                         (int)e0[2], ban.ref.c_str());
                motivo = buf; return false;
            }
            const int ra = (int)e0[0], rb = (int)e0[1];
            if (ra >= 0 && ra < tris && rb < tris && (rb < 0 || rb >= ra)) continue;   // (lo de RangoMalo, sin armar texto)
            std::string e = RangoMalo(ra, rb, tris, p.ref);
            if (e.empty()) continue;
            snprintf(buf, sizeof buf, "step %d (Draw elements): array %s, range %d: %s", num, arr.ref.c_str(), j, e.c_str());
            motivo = buf; return false;
        }
        return true;
    }
    snprintf(buf, sizeof buf, "step %d (Draw elements): unknown range %d", num, p.sub); motivo = buf; return false;
}

// la profundidad maxima que agrega una lista y si cierra en 0; -1 = error (motivo escrito)
static int Profundidad(Rutina* r, std::vector<W3dPaso>& L, std::string& motivo, std::set<Rutina*>& pila, int nivel,
                       Punteros& pu);

static int ProfRutina(Rutina* r, std::string& motivo, std::set<Rutina*>& pila, int nivel, Punteros& pu) {
    if (pila.count(r)) { motivo = "routine '" + r->name + "' calls itself (cycle)"; return -1; }
    if (nivel > 8) { motivo = "too many nested routines"; return -1; }
    if (r->sucia || !PunterosAlDia(r)) r->Resolver();   // (una subrutina que nunca se dibujo sola: sus nombres)
    pila.insert(r);
    int prof = Profundidad(r, r->ListaActual(), motivo, pila, nivel, pu);
    pila.erase(r);
    return prof;
}

static int Profundidad(Rutina* r, std::vector<W3dPaso>& L, std::string& motivo, std::set<Rutina*>& pila, int nivel,
                       Punteros& pu) {
    char buf[260];
    int d = 0, maxd = 0;
    // donde termina cada salto (lo salteado tiene que dejar la pila como la encontro): por paso destino, la profundidad
    // que tiene que tener la pila al llegar (INT_MIN = ningun salto llega ahi; INT_MAX = llegan dos con distinta, que es
    // un error seguro). Antes cada paso recorria todos los saltos vistos: con cientos de "Saltar si" era cuadratico
    std::vector<int> esperada;
    for (size_t i = 0; i < L.size(); i++) {
        W3dPaso& p = L[i];
        const int num = (int)i + 1;
        if (p.tipo >= PasoN) { snprintf(buf, sizeof buf, "step %d: unknown type", num); motivo = buf; return -1; }
        for (int k = 0; k < W3dPasoNumeros(p); k++)
            if (!p.n[k].ref.empty() && !p.n[k].p) {
                snprintf(buf, sizeof buf, "step %d (%s): bad memory reference '%s' (use @name[0..%d])", num,
                         W3dPasoEtiqueta(p.tipo), p.n[k].ref.c_str(), W3D_MEMORIA_TAM - 1);
                motivo = buf; return -1;
            }
        const int rt = W3dPasoRef(p.tipo);
        if (rt == RefFuncion && p.ref.empty()) {
            snprintf(buf, sizeof buf, "step %d (%s): write the name of the Lua function", num, W3dPasoEtiqueta(p.tipo));
            motivo = buf; return -1;
        }
        if (p.tipo == PasoLua && !p.ref2.empty() && !p.ptr2) {
            snprintf(buf, sizeof buf, "step %d (%s): object '%s' not found", num, W3dPasoEtiqueta(p.tipo), p.ref2.c_str());
            motivo = buf; return -1;
        }
        const bool cajaEnMemoria = (p.tipo == PasoVisible && p.ref.empty() && p.n[1].p);   // (la caja, 6 numeros de una memoria)
        if (rt != RefNada && rt != RefFuncion && !p.ptr && !(p.tipo == PasoMatcap && !p.on) && !cajaEnMemoria) {   // (apagar el matcap: sin malla)
            if (p.ref.empty()) snprintf(buf, sizeof buf, "step %d (%s): nothing chosen", num, W3dPasoEtiqueta(p.tipo));
            else snprintf(buf, sizeof buf, "step %d (%s): '%s' not found", num, W3dPasoEtiqueta(p.tipo), p.ref.c_str());
            motivo = buf; return -1;
        }
        if (!p.ref2.empty() && !p.ptr2) {
            snprintf(buf, sizeof buf, "step %d (%s): object '%s' not found", num, W3dPasoEtiqueta(p.tipo), p.ref2.c_str());
            motivo = buf; return -1;
        }
        const int opc = W3dPasoOpciones(p.tipo);
        if (opc != OpcNada && (p.modo < 0 || p.modo >= W3dOpcionesN(opc))) {
            snprintf(buf, sizeof buf, "step %d (%s): option %d does not exist (0..%d)", num, W3dPasoEtiqueta(p.tipo),
                     p.modo, W3dOpcionesN(opc) - 1);
            motivo = buf; return -1;
        }
        if (W3dPasoEntero(p.tipo) == EnteroLuz && (p.entero < 0 || p.entero >= W3D_RUTINA_LUCES)) {
            snprintf(buf, sizeof buf, "step %d (%s): light %d does not exist (0..%d)", num, W3dPasoEtiqueta(p.tipo),
                     p.entero, W3D_RUTINA_LUCES - 1);
            motivo = buf; return -1;
        }
        switch (p.tipo) {
        case PasoApilar: d++; if (d > maxd) maxd = d; break;
        case PasoDesapilar:
            if (--d < 0) { snprintf(buf, sizeof buf, "step %d: Pop matrix without its Push", num); motivo = buf; return -1; }
            break;
        case PasoArray:
            pu.on[p.modo] = p.on ? 1 : 0;
            break;
        case PasoPunteroVertices: case PasoPunteroNormales: case PasoPunteroUV: case PasoPunteroColores: {
            // (que la malla tenga ese array se pide al dibujar con el array prendido: apuntar no lee nada)
            const int k = p.tipo - PasoPunteroVertices;
            pu.m[k] = (const MallaRecurso*)p.ptr; pu.conocido[k] = true;
            if (k == 2) pu.uvNormales = false;
            break;
        }
        case PasoMatcap:
            if (p.on && p.ptr) { pu.m[2] = (const MallaRecurso*)p.ptr; pu.conocido[2] = true; pu.uvNormales = true; }
            break;
        case PasoDibujar:
            if (!DibujarOk(p, num, pu, motivo)) return -1;
            break;
        case PasoHijos:
            // los hijos ponen SUS punteros y prenden o apagan arrays: despues hay que volver a apuntar (y fijar los arrays)
            for (int k = 0; k < 4; k++) { pu.m[k] = 0; pu.conocido[k] = true; pu.on[k] = -1; }
            pu.uvNormales = false;
            break;
        case PasoLua:
            pu.Olvidar();   // (lua puede poner cualquier cosa)
            break;
        case PasoVisible:
            if (p.n[0].ref.empty() || !p.n[0].p) {
                snprintf(buf, sizeof buf, "step %d (Visibility test): 'Write to' has to be a memory (@name[i])", num);
                motivo = buf; return -1;
            }
            if (p.ptr && (p.entero < -1 || p.entero >= (int)((const MallaRecurso*)p.ptr)->partes.size() + (((const MallaRecurso*)p.ptr)->partes.empty() ? 1 : 0))) {
                snprintf(buf, sizeof buf, "step %d (Visibility test): part %d does not exist (-1 = the whole mesh)", num, p.entero);
                motivo = buf; return -1;
            }
            break;
        case PasoRutina: {
            // la subrutina se valida con los punteros de este punto (y deja los suyos)
            int sub = ProfRutina((Rutina*)p.ptr, motivo, pila, nivel + 1, pu);
            if (sub < 0) return -1;
            if (d + sub > maxd) maxd = d + sub;
            break;
        }
        case PasoSaltarSi: case PasoSaltarOculto:
            if (p.entero < 0 || i + (size_t)p.entero >= L.size()) {
                snprintf(buf, sizeof buf, "step %d: skips %d steps but only %d follow", num, p.entero, (int)(L.size() - i - 1));
                motivo = buf; return -1;
            }
            {
                if (esperada.empty()) esperada.assign(L.size(), INT_MIN);
                int& e = esperada[i + (size_t)p.entero];
                if (e == INT_MIN) e = d; else if (e != d) e = INT_MAX;
            }
            break;
        default: break;
        }
        if (!esperada.empty() && esperada[i] != INT_MIN && esperada[i] != d) {
            snprintf(buf, sizeof buf, "step %d: the steps a 'Skip if zero' can skip leave the matrix stack unbalanced", num);
            motivo = buf; return -1;
        }
    }
    if (d != 0) { snprintf(buf, sizeof buf, "%d Push matrix without its Pop", d); motivo = buf; return -1; }
    if (maxd > kProfMax) { snprintf(buf, sizeof buf, "matrix stack too deep (%d levels, max %d)", maxd, kProfMax); motivo = buf; return -1; }
    return maxd;
}

// la rutina la DIBUJA el arbol (ella y todos sus padres visibles); una oculta es una libreria que se llama
static bool LaDibujaElArbol(Object* o) {
    for (; o; o = o->Parent) if (!o->visible) return false;
    return true;
}

// lo que una validacion leyo de las memorias sigue igual (la copia contra lo de ahora)
static bool MemoriasIguales(const Rutina* r) {
    size_t k = 0;
    for (size_t i = 0; i < r->valLecturas.size(); i++) {
        const int n = r->valLecturas[i].second;
        if (memcmp(r->valLecturas[i].first, &r->valCopia[k], (size_t)n * sizeof(float)) != 0) return false;
        k += (size_t)n;
    }
    return true;
}

static bool ValidarRutina(Rutina* r) {
    if (r->sucia || !PunterosAlDia(r)) r->Resolver();
    const bool sabido = LaDibujaElArbol(r);
    // JUGANDO, el resultado de la vez anterior si no cambio nada de lo que lee: los pasos (suyos o de una subrutina) y
    // los datos de las mallas (g_w3dRutinasGen), los punteros (PunterosAlDia, arriba), si la dibuja el arbol, los arrays
    // al empezar y lo que leyo de las memorias (los rangos que escribe lua). Igual se revalida entera cada 120 cuadros
    // (cada rutina en otro momento). Editando, cada cuadro como siempre. Validar todo cada cuadro era lo mas caro del
    // cuadro: miles de pasos (en el N95, milisegundos)
    const bool jugando = W3dJuegoCorriendo();
    if (jugando && r->valOk && r->valGen == g_w3dRutinasGen && r->valSabido == sabido && r->valVistos == r->arraysVistos &&
        memcmp(r->valArrays, r->arraysInicio, sizeof r->valArrays) == 0 && ++r->valEdad < 120 && MemoriasIguales(r))
        return !r->invalida;
    std::vector<std::pair<const float*, int> > lecturas;
    std::vector<std::pair<const float*, int> >* antes = gLecturas;
    gLecturas = &lecturas;
    std::string motivo;
    std::set<Rutina*> pila;
    pila.insert(r);
    bool ok = true;
    // todas las listas que algun modo usa (no solo la del modo actual: al cambiar de modo no tiene que reventar)
    for (int m = 0; m < Rutina::ModoN && ok; m++) {
        bool usada = (m == Rutina::ModoDefecto);
        for (int k = 0; k < Rutina::ModoN; k++) if (r->usar[k] == m) usada = true;
        if (!usada) continue;
        Punteros pu(sabido, r);
        if (Profundidad(r, r->listas[m], motivo, pila, 0, pu) < 0) ok = false;
    }
    gLecturas = antes;
    r->invalida = !ok;
    r->motivo = ok ? std::string() : motivo;
    // para la vez siguiente: lo que leyo (y una copia) y con que estado
    r->valLecturas.swap(lecturas);
    r->valCopia.clear();
    for (size_t i = 0; i < r->valLecturas.size(); i++)
        r->valCopia.insert(r->valCopia.end(), r->valLecturas[i].first, r->valLecturas[i].first + r->valLecturas[i].second);
    r->valOk = jugando;
    r->valGen = g_w3dRutinasGen;
    r->valSabido = sabido;
    r->valVistos = r->arraysVistos;
    memcpy(r->valArrays, r->arraysInicio, sizeof r->valArrays);
    r->valEdad = (int)((r->serial * 37u) % 120u);   // (repartidas: no se revalidan todas en el mismo cuadro)
    return ok;
}

// lo engancha el editor al arrancar (los juegos exportados no compilan este archivo: el hook queda en NULL)
struct RutinaValidarInstalar { RutinaValidarInstalar() { W3dRutinaValidarHook = ValidarRutina; } };
static RutinaValidarInstalar gRutinaValidarInstalar;

// (harness / panel) valida sin dibujar
bool W3dRutinaValidar(Rutina* r) { return ValidarRutina(r); }

// ---------------------------------------------------------------------------
//  ICONOS: uno por grupo (Transform = la camara, Fog = la niebla, Lights = la lampara...) y algunos pasos el suyo
// ---------------------------------------------------------------------------
int W3dRutinaGrupoIcono(int g) {
    switch (g) {
    case GrupoTransformar: return (int)IconType::camera;
    case GrupoDibujar:     return (int)IconType::mesh;
    case GrupoTextura:     return (int)IconType::textura;
    case GrupoNiebla:      return (int)IconType::niebla;
    case GrupoLuces:       return (int)IconType::light;
    case GrupoAlfa:        return (int)IconType::filtro;
    case GrupoColor:       return (int)IconType::material;
    case GrupoProfundidad: return (int)IconType::limpiarz;
    case GrupoMezcla:      return (int)IconType::igual;
    case GrupoCaras:       return (int)IconType::visible;
    default:               return (int)IconType::arrowRight;
    }
}
int W3dRutinaPasoIcono(int tipo) {
    switch (tipo) {
    case PasoArray:           return (int)IconType::lista;
    case PasoPunteroVertices: return (int)IconType::selVertex;
    case PasoPunteroNormales: return (int)IconType::normalVertex;
    case PasoPunteroUV:       return (int)IconType::textura;
    case PasoPunteroColores:  return (int)IconType::material;
    case PasoHijos:  return (int)IconType::object;
    case PasoRutina: return (int)IconType::lista;
    case PasoVisible: return (int)IconType::visible;
    case PasoTamPunto:   return (int)IconType::selVertex;
    case PasoAnchoLinea: return (int)IconType::selEdge;
    case PasoLua:     return (int)IconType::script;
    case PasoMatcap:  return (int)IconType::normalVertex;
    case PasoSaltarOculto: return (int)IconType::hidden;
    default:         return W3dRutinaGrupoIcono(W3dPasoGrupoDe(tipo));
    }
}
// el icono de una rutina (outliner y "Llamar rutina"): la de UN paso (las de Add > Core) lleva el de su paso; la que
// solo pone ESTADO (no dibuja ni llama a nada: un material hecho rutina), el de material
int W3dRutinaIcono(Rutina* r) {
    if (r->constructor) return (int)IconType::modificador;   // (el constructor: la llave)
    const std::vector<W3dPaso>& L = r->listas[Rutina::ModoDefecto];
    if (L.size() == 1) return W3dRutinaPasoIcono(L[0].tipo);
    for (size_t i = 0; i < L.size(); i++)
        if (W3dPasoGrupoDe(L[i].tipo) == GrupoDibujar || L[i].tipo == PasoLua || L[i].tipo == PasoVisible)
            return (int)IconType::lista;
    return (int)IconType::material;
}

// ---------------------------------------------------------------------------
//  el menu de PASOS agrupado: un submenu por grupo (un grupo de un solo paso va suelto). El id de cada item es el
//  tipo de paso; con 'conVacia', "Empty routine" (id PasoN) arriba de todo. 'subs' son los submenus (los crea la
//  primera vez; despues los rearma).
// ---------------------------------------------------------------------------
void W3dRutinaMenuPasos(PopupMenu* raiz, std::vector<PopupMenu*>& subs, void (*accion)(int), bool conVacia) {
    if (!raiz) return;
    raiz->Limpiar();
    raiz->action = accion;
    if (subs.size() < (size_t)GrupoN) subs.resize(GrupoN, (PopupMenu*)NULL);
    if (conVacia) raiz->Agregar(T("Empty routine"), PasoN, (int)IconType::lista);
    for (int g = 0; g < GrupoN; g++) {
        int n = 0, unico = -1;
        for (int t = 0; t < PasoN; t++) if (W3dPasoGrupoDe(t) == g) { n++; unico = t; }
        if (n == 0) continue;
        if (n == 1) { raiz->Agregar(T(W3dPasoEtiqueta(unico)), unico, W3dRutinaPasoIcono(unico)); continue; }
        if (!subs[g]) subs[g] = new PopupMenu();
        PopupMenu* sub = subs[g];
        sub->Limpiar();
        sub->action = accion;
        for (int t = 0; t < PasoN; t++)
            if (W3dPasoGrupoDe(t) == g) sub->Agregar(T(W3dPasoEtiqueta(t)), t, W3dRutinaPasoIcono(t));
        raiz->Agregar(T(W3dGrupoEtiqueta(g)), 1000 + g, W3dRutinaGrupoIcono(g), sub);
    }
}
