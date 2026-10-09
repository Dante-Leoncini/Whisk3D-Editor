// ============================================================================
//  PropsRutina.cpp — la tarjeta "Routine" del panel de propiedades (pestania 2
//  del objeto Rutina, objects/Rutina.h). Patron de PropsHitbox: el panel la ARMA
//  (PropsRutinaConstruir, en ConstruirGrupos) y le pide el bindeo por frame
//  (PropsRutinaActualizar, en ActualizarPestanias).
//
//  - "Render mode": que lista se edita (la de todos, "All", o la de un modo).
//  - "Use": que lista usa ese modo (la de todos, la propia o la de otro modo).
//  - los PASOS: lista con icono por grupo, Add (desplegable AGRUPADO) / Remove /
//    Move Up / Move Down.
//  - los campos del paso elegido, cada uno lo que corresponde:
//      la MALLA se elige de un desplegable por CARPETAS (las de la biblioteca):
//      carpeta > malla > parte (una malla de una sola parte se elige directo);
//      las texturas por sus carpetas; los objetos y las rutinas de una lista;
//      la info de la malla en FILAS (triangulos / vertices / indices);
//      la PRIMITIVA (puntos, lineas, tira, abanico... y las simuladas, con aviso);
//      los COLORES con el selector de color de Whisk3D (o los 4 numeros, para
//      usar memorias "@nombre[i]");
//      la luz, la opcion, el on/off, el rango manual, el array, Max...
//  - el MOTIVO, en rojo, si la rutina no pasa la validacion del editor.
// ============================================================================
#include "ViewPorts/Properties.h"
#include "objects/Rutina.h"
#include "objects/Mesh.h"
#include "objects/Textures.h"
#include "objects/MallaRecurso.h"
#include "objects/MallaFlujos.h"
#include "W3dLang.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "ViewPorts/W3dInput.h"   // W3dK_*: las teclas de la lista de pasos
#include <algorithm>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void PropsLayoutSucio();   // (Properties.cpp: re-armar el layout del panel)
void PropsAbrirMenuBajoBoton(PopupMenu* menu, Button* boton);   // (Properties.cpp: el estandar de los desplegables)
bool W3dRutinaValidar(Rutina* r);
// (edit/RutinaEditor.cpp) los iconos y el menu de pasos agrupado
int  W3dRutinaPasoIcono(int tipo);
int  W3dRutinaIcono(Rutina* r);
void W3dRutinaMenuPasos(PopupMenu* raiz, std::vector<PopupMenu*>& subs, void (*accion)(int), bool conVacia);

static Properties* gPr = NULL;

static Rutina* RuActiva() {
    return (ObjActivo && ObjActivo->getType() == ObjectType::rutina) ? (Rutina*)ObjActivo : NULL;
}
static std::vector<W3dPaso>* RuLista(Rutina* r) {
    if (!r) return NULL;
    if (r->listaEditada < 0 || r->listaEditada >= Rutina::ModoN) r->listaEditada = 0;
    return &r->listas[r->listaEditada];
}
static W3dPaso* RuPaso(Rutina* r) {
    std::vector<W3dPaso>* L = RuLista(r);
    if (!L || r->pasoActivo < 0 || r->pasoActivo >= (int)L->size()) return NULL;
    return &(*L)[r->pasoActivo];
}
static const char* kModosUI[Rutina::ModoN] = { "All", "Solid", "Material", "Rendered", "Wireframe", "Z-Buffer" };
static void Cambio(Rutina* r) { r->sucia = true; g_w3dRutinasGen++; g_redraw = true; }

static std::string Base(const std::string& ruta) {
    const size_t b = ruta.find_last_of("/\\");
    return b == std::string::npos ? ruta : ruta.substr(b + 1);
}
static std::string NumTexto(const W3dNum& n) {
    if (!n.ref.empty()) return n.ref;
    char b[32]; snprintf(b, sizeof b, "%g", n.v); return b;
}
// la malla de un paso ya resuelta (o NULL)
static MallaRecurso* MallaDe(const W3dPaso* p) {
    if (!p || W3dPasoRef(p->tipo) != RefMalla || !p->ptr) return NULL;
    MallaRecurso* m = (MallaRecurso*)p->ptr;
    return W3dMallaRecursoVivo(m) ? m : NULL;
}
static bool UsaPartes(int t) { return t == PasoDibujar || t == PasoVisible; }

// ---- la lista de pasos (modo 14 de PropListMeshParts: va por 'obj') ----
static int RuFilas(Object* o) {
    if (!o || o->getType() != ObjectType::rutina) return 0;
    std::vector<W3dPaso>* L = RuLista((Rutina*)o);
    return L ? (int)L->size() : 0;
}
static const W3dPaso* RuFila(Object* o, int i) {
    if (!o || o->getType() != ObjectType::rutina) return NULL;
    std::vector<W3dPaso>* L = RuLista((Rutina*)o);
    return (L && i >= 0 && i < (int)L->size()) ? &(*L)[i] : NULL;
}
// un color de numeros fijos (con alguna memoria se muestran los numeros: el color cambia jugando)
static bool ColorFijo(const W3dPaso& p) {
    if (!W3dPasoEsColor(p.tipo)) return false;
    for (int k = 0; k < 4; k++) if (!p.n[k].ref.empty()) return false;
    return true;
}
static std::string RuFilaTexto(Object* o, int i) {
    const W3dPaso* pp = RuFila(o, i);
    if (!pp) return std::string();
    const W3dPaso& p = *pp;
    // (sin numero ni "on/off" ni los numeros de un color: el icono va a la izquierda y el checkbox o el color a la
    //  derecha, RuFilaValor)
    char b[64];
    if (p.tipo == PasoArray)   // "Array: Normals"
        return std::string(T("Array")) + ": " + T(W3dOpcionEtiqueta(OpcArray, p.modo));
    std::string t = T(W3dPasoEtiqueta(p.tipo));
    if (W3dPasoUsaOn(p.tipo)) {   // "Textura on/off" -> "Textura": lo dice el checkbox
        const size_t k = t.rfind(" on/off");
        if (k != std::string::npos && k + 7 == t.size()) t.erase(k);
    }
    if (p.tipo == PasoLimpiar) {   // "Clear color + depth"
        static const char* const kBuf[3] = { "color", "depth", "stencil" };
        std::string q;
        for (int k = 0; k < 3; k++) if (p.entero & (1 << k)) q += std::string(q.empty() ? " " : " + ") + T(kBuf[k]);
        return t + (q.empty() ? std::string(" (") + T("nothing") + ")" : q);
    }
    if (W3dPasoEntero(p.tipo) == EnteroLuz) { snprintf(b, sizeof b, " %d", p.entero); t += b; }
    if (!p.ref.empty()) t += " " + (W3dPasoRef(p.tipo) == RefTextura ? Base(p.ref) : p.ref);
    if (UsaPartes(p.tipo) && p.entero >= 0) { snprintf(b, sizeof b, " [%d]", p.entero); t += b; }
    if (W3dPasoOpciones(p.tipo) != OpcNada) t += std::string(" ") + T(W3dOpcionEtiqueta(W3dPasoOpciones(p.tipo), p.modo));
    if (p.tipo == PasoDibujar) {
        if (p.sub == RangoManual)
            t += " " + NumTexto(p.n[0]) + ".." + ((p.n[1].ref.empty() && p.n[1].v < 0) ? std::string("max") : NumTexto(p.n[1]));
        else if (p.sub == RangoArray) t += " " + p.n[2].ref + (p.n[3].ref.empty() ? std::string() : " / " + p.n[3].ref);
        return t;
    }
    if (p.tipo == PasoVisible) return t + " -> " + p.n[0].ref + (p.ref2.empty() ? std::string() : " (" + p.ref2 + ")");
    if (p.tipo == PasoLua) return t + (p.ref2.empty() ? std::string() : " (" + p.ref2 + ")");
    if (p.tipo == PasoSaltarSi || p.tipo == PasoSaltarOculto) { snprintf(b, sizeof b, " skip %d", p.entero); t += b; }
    const int nn = W3dPasoNumeros(p);
    if (nn > 0 && !ColorFijo(p)) {
        t += " (";
        for (int k = 0; k < nn; k++) { if (k) t += ", "; t += NumTexto(p.n[k]); }
        t += ")";
    }
    return t;
}
// lo que se ve a la DERECHA de la fila (PropList): el checkbox de un on/off, el cuadro de un color fijo
static int RuFilaValor(Object* o, int i, float* rgba) {
    const W3dPaso* p = RuFila(o, i);
    if (!p) return 0;
    if (W3dPasoUsaOn(p->tipo)) return p->on ? 2 : 1;
    if (ColorFijo(*p)) {
        for (int k = 0; k < 4; k++) rgba[k] = p->n[k].v < 0 ? 0.0f : (p->n[k].v > 1 ? 1.0f : p->n[k].v);
        return 3;
    }
    return 0;
}
static int RuFilaIcono(Object* o, int i) {
    const W3dPaso* p = RuFila(o, i);
    if (p && p->tipo == PasoRutina && p->ptr) return W3dRutinaIcono((Rutina*)p->ptr);   // (una de material: su icono)
    return p ? W3dRutinaPasoIcono(p->tipo) : -1;
}
static void RuFilaSeleccionar(Object* o, int i) {
    if (!o || o->getType() != ObjectType::rutina) return;
    ((Rutina*)o)->pasoActivo = i;
    PropsLayoutSucio();   // cambian los campos visibles
}

// ---- "Render mode" y "Use" ----
static PopupMenu* MenuRuLista = NULL;
static PopupMenu* MenuRuUsar = NULL;
static void AccionListaElegida(int id) {
    Rutina* r = RuActiva(); if (!r) return;
    r->listaEditada = id; r->pasoActivo = RuLista(r)->empty() ? -1 : 0;
    PropsLayoutSucio(); g_redraw = true;
}
static void AccionMenuLista() {
    Rutina* r = RuActiva();
    if (!gPr || !gPr->propRuLista || !r) return;
    if (!MenuRuLista) { MenuRuLista = new PopupMenu(); MenuRuLista->action = AccionListaElegida; }
    MenuRuLista->Limpiar();
    for (int m = 0; m < Rutina::ModoN; m++)
        MenuRuLista->Agregar(T(kModosUI[m]), m, (int)IconType::camera)->verde = (m == r->listaEditada);
    PropsAbrirMenuBajoBoton(MenuRuLista, gPr->propRuLista->button);
}
// id: 0 = la de todos; 1 = la propia; 2.. = la de otro modo (2 + modo)
static void AccionUsarElegido(int id) {
    Rutina* r = RuActiva(); if (!r || r->listaEditada <= 0) return;
    const int m = r->listaEditada;
    if (id == 0) r->usar[m] = -1;
    else if (id == 1) r->usar[m] = (signed char)m;
    else r->usar[m] = (signed char)(id - 2);
    Cambio(r);
}
static void AccionMenuUsar() {
    Rutina* r = RuActiva();
    if (!gPr || !gPr->propRuUsar || !r || r->listaEditada <= 0) return;
    if (!MenuRuUsar) { MenuRuUsar = new PopupMenu(); MenuRuUsar->action = AccionUsarElegido; }
    MenuRuUsar->Limpiar();
    MenuRuUsar->Agregar(T("The list of All"), 0, (int)IconType::lista);
    MenuRuUsar->Agregar(T("Its own list"), 1, (int)IconType::lista);
    for (int m = 1; m < Rutina::ModoN; m++)
        if (m != r->listaEditada) MenuRuUsar->Agregar(std::string(T("Same as")) + " " + T(kModosUI[m]), 2 + m, (int)IconType::camera);
    PropsAbrirMenuBajoBoton(MenuRuUsar, gPr->propRuUsar->button);
}

// ---- Add (agrupado) / Remove / Move ----
static PopupMenu* MenuRuAdd = NULL;
static std::vector<PopupMenu*> gSubsAdd;
static void AccionAddElegido(int tipo) {
    Rutina* r = RuActiva(); if (!r || tipo < 0 || tipo >= PasoN) return;
    std::vector<W3dPaso>* L = RuLista(r);
    // va DESPUES del paso elegido (o al final): el orden de la lista es el de ejecucion
    int pos = (r->pasoActivo >= 0 && r->pasoActivo < (int)L->size()) ? r->pasoActivo + 1 : (int)L->size();
    L->insert(L->begin() + pos, W3dPasoNuevo(tipo));
    r->pasoActivo = pos;
    Cambio(r);
    PropsLayoutSucio();
}
static void AccionMenuAdd() {
    if (!gPr || !gPr->propRuFilaAdd || gPr->propRuFilaAdd->botones.empty() || !RuActiva()) return;
    if (!MenuRuAdd) { MenuRuAdd = new PopupMenu(); W3dRutinaMenuPasos(MenuRuAdd, gSubsAdd, AccionAddElegido, false); }
    PropsAbrirMenuBajoBoton(MenuRuAdd, gPr->propRuFilaAdd->botones[0]);
}
static void AccionRemove() {
    Rutina* r = RuActiva(); std::vector<W3dPaso>* L = RuLista(r);
    if (!r || !L || r->pasoActivo < 0 || r->pasoActivo >= (int)L->size()) return;
    extern void W3dAnimBorrarPaso(Object*, unsigned);
    W3dAnimBorrarPaso(r, (*L)[(size_t)r->pasoActivo].id);   // (sus curvas de animacion se van con el)
    L->erase(L->begin() + r->pasoActivo);
    if (r->pasoActivo >= (int)L->size()) r->pasoActivo = (int)L->size() - 1;
    Cambio(r);
    PropsLayoutSucio();
}
// el paso 'desde' pasa a quedar ANTES del que hoy esta en 'hasta' (hasta = cantidad: al final). Lo usan arrastrarlo
// con el mouse y agarrarlo con G
void PropsRutinaMoverPaso(int desde, int hasta) {
    Rutina* r = RuActiva(); std::vector<W3dPaso>* L = RuLista(r);
    if (!r || !L || desde < 0 || desde >= (int)L->size() || hasta < 0 || hasta > (int)L->size()) return;
    if (hasta == desde || hasta == desde + 1) { r->pasoActivo = desde; return; }
    W3dPaso p = (*L)[(size_t)desde];
    L->erase(L->begin() + desde);
    if (hasta > desde) hasta--;
    L->insert(L->begin() + hasta, p);
    r->pasoActivo = hasta;
    Cambio(r);
    PropsLayoutSucio();
}
// el click en el CHECKBOX de la derecha de una fila: prende/apaga el paso (sin elegirlo)
bool PropsRutinaClicValor(int fila) {
    Rutina* r = RuActiva(); std::vector<W3dPaso>* L = RuLista(r);
    if (!r || !L || fila < 0 || fila >= (int)L->size() || !W3dPasoUsaOn((*L)[(size_t)fila].tipo)) return false;
    (*L)[(size_t)fila].on = !(*L)[(size_t)fila].on;
    Cambio(r);
    return true;
}
// AGARRAR un paso (G; el 1 en el telefono): se mueve con el mouse o las flechas; click / Enter lo deja, Escape / click
// derecho lo devuelve a donde estaba
static int gAgarre = -1, gAgarreOrigen = -1, gAgarreAcum = 0;
bool PropsRutinaAgarrando() { return gAgarre >= 0 && RuActiva() != NULL; }
void PropsRutinaAgarrar() {
    Rutina* r = RuActiva(); std::vector<W3dPaso>* L = RuLista(r);
    if (!r || !L || r->pasoActivo < 0 || r->pasoActivo >= (int)L->size()) return;
    gAgarre = gAgarreOrigen = r->pasoActivo; gAgarreAcum = 0;
}
void PropsRutinaAgarreMover(int filas) {   // + abajo, - arriba
    Rutina* r = RuActiva(); std::vector<W3dPaso>* L = RuLista(r);
    if (!PropsRutinaAgarrando() || !L) return;
    const int b = gAgarre + filas;
    if (b < 0 || b >= (int)L->size()) return;
    PropsRutinaMoverPaso(gAgarre, filas > 0 ? b + 1 : b);
    gAgarre = b;
}
void PropsRutinaAgarreMouse(int dy) {      // el mouse se movio dy px (una fila cada alto de renglon)
    if (!PropsRutinaAgarrando()) return;
    gAgarreAcum += dy;
    const int rowH = RenglonHeightGS + gapGS;
    while (gAgarreAcum >= rowH) { gAgarreAcum -= rowH; PropsRutinaAgarreMover(1); }
    while (gAgarreAcum <= -rowH) { gAgarreAcum += rowH; PropsRutinaAgarreMover(-1); }
}
void PropsRutinaAgarreFin(bool confirmar) {
    if (!PropsRutinaAgarrando()) { gAgarre = -1; return; }
    if (!confirmar && gAgarre != gAgarreOrigen)
        PropsRutinaMoverPaso(gAgarre, gAgarreOrigen > gAgarre ? gAgarreOrigen + 1 : gAgarreOrigen);
    gAgarre = -1;
}
// las TECLAS de la lista de pasos (con el panel activo): X / Supr borra el paso elegido, G lo agarra; agarrado, las
// flechas lo mueven, Enter lo deja y Escape lo devuelve. true = la tecla era de la lista
bool PropsRutinaTecla(int tecla) {
    Rutina* r = RuActiva();
    if (!r || !gPr || !gPr->propRutina || !gPr->propRutina->visible) return false;
    if (PropsRutinaAgarrando()) {
        if (tecla == W3dK_UP)     { PropsRutinaAgarreMover(-1); return true; }
        if (tecla == W3dK_DOWN)   { PropsRutinaAgarreMover(+1); return true; }
        if (tecla == W3dK_RETURN) { PropsRutinaAgarreFin(true); return true; }
        if (tecla == W3dK_ESCAPE) { PropsRutinaAgarreFin(false); return true; }
        return true;   // (agarrado, nada mas)
    }
    if (r->pasoActivo < 0) return false;
    if (tecla == W3dK_X || tecla == W3dK_DELETE) { AccionRemove(); return true; }
    if (tecla == W3dK_G) { PropsRutinaAgarrar(); return true; }
    if (tecla == W3dK_I) {   // keyframe de los campos del paso en el frame actual (otra vez: los saca)
        W3dPaso* p = RuPaso(r);
        if (r->sucia) r->Resolver();   // (un paso recien agregado toma su id)
        extern void W3dAnimKeyPaso(Object*, unsigned, int);
        extern int CurrentFrame;
        if (p) W3dAnimKeyPaso(r, p->id, CurrentFrame);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  los DESPLEGABLES de referencias: un arbol de CARPETAS armado con menus de un pool (se reusan entre aperturas)
// ---------------------------------------------------------------------------
static std::vector<PopupMenu*> gPool;
static size_t gPoolUsados = 0;
struct OpcRef { std::string ref; int parte; };
static std::vector<OpcRef> gRefOpc;     // lo que va al paso por cada item (el id del item es su indice)
static bool gRefEsSegunda = false;      // el menu abierto es el de 'ref2' (el objeto del test de visibilidad)
static void AccionRefElegida(int id);
static PopupMenu* MenuNuevo() {
    PopupMenu* m;
    if (gPoolUsados < gPool.size()) m = gPool[gPoolUsados];
    else { m = new PopupMenu(); gPool.push_back(m); }
    gPoolUsados++;
    m->Limpiar(); m->titulo.clear();
    m->action = AccionRefElegida;
    return m;
}
static bool MenorSinMayus(const std::string& a, const std::string& b) {
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++) {
        const int x = tolower((unsigned char)a[i]), y = tolower((unsigned char)b[i]);
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}
// el submenu de la carpeta "a/b/c" (creandolo, y a sus padres, la primera vez)
static PopupMenu* MenuCarpeta(PopupMenu* raiz, std::map<std::string, PopupMenu*>& carpetas, const std::string& carpeta) {
    if (carpeta.empty()) return raiz;
    std::map<std::string, PopupMenu*>::iterator it = carpetas.find(carpeta);
    if (it != carpetas.end()) return it->second;
    const size_t b = carpeta.find_last_of('/');
    PopupMenu* padre = MenuCarpeta(raiz, carpetas, b == std::string::npos ? std::string() : carpeta.substr(0, b));
    PopupMenu* m = MenuNuevo();
    padre->Agregar(b == std::string::npos ? carpeta : carpeta.substr(b + 1), 900000 + (int)carpetas.size(), (int)IconType::carpeta, m);
    carpetas[carpeta] = m;
    return m;
}
static int Opcion(const std::string& ref, int parte) {
    OpcRef o; o.ref = ref; o.parte = parte;
    gRefOpc.push_back(o);
    return (int)gRefOpc.size() - 1;
}
static bool MallaMenor(const MallaRecurso* a, const MallaRecurso* b) {
    if (a->carpeta != b->carpeta) return MenorSinMayus(a->carpeta, b->carpeta);
    return MenorSinMayus(a->nombre, b->nombre);
}
// las MALLAS 3D por carpetas; con 'conPartes', cada malla de varias partes abre las suyas
static void ArmarMallas(PopupMenu* raiz, const W3dPaso& p, bool conPartes) {
    std::vector<MallaRecurso*> ms;
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    for (size_t i = 0; i < reg.size(); i++) if (reg[i] && !reg[i]->borrado) ms.push_back(reg[i]);
    std::sort(ms.begin(), ms.end(), MallaMenor);
    std::map<std::string, PopupMenu*> carpetas;
    for (size_t i = 0; i < ms.size(); i++) {
        MallaRecurso* r = ms[i];
        PopupMenu* m = MenuCarpeta(raiz, carpetas, r->carpeta);
        // las partes se conocen al cargarla: una que nunca se cargo se carga una vez (queda la lista de partes)
        if (conPartes && r->partes.empty() && !r->Cargada()) { if (W3dMallaRecursoRetener(r)) W3dMallaRecursoSoltar(r); }
        const int nPartes = (int)r->partes.size();
        if (!conPartes || nPartes <= 1) {
            MenuItem* it = m->Agregar(r->nombre, Opcion(r->nombre, nPartes == 1 ? 0 : -1), (int)IconType::mesh);
            it->verde = (r->nombre == p.ref);
            continue;
        }
        PopupMenu* sub = MenuNuevo();
        MenuItem* it = m->Agregar(r->nombre, 800000 + (int)i, (int)IconType::mesh, sub);
        it->verde = (r->nombre == p.ref);
        sub->Agregar(T("All parts"), Opcion(r->nombre, -1), (int)IconType::mesh)->verde = (r->nombre == p.ref && p.entero < 0);
        for (int k = 0; k < nPartes; k++) {
            char b[160];
            snprintf(b, sizeof b, "%d. %s (%d %s)", k, r->partes[(size_t)k].nombre.c_str(), r->partes[(size_t)k].cantidad / 3, T("triangles"));
            sub->Agregar(b, Opcion(r->nombre, k), (int)IconType::material)->verde = (r->nombre == p.ref && p.entero == k);
        }
    }
}
static void ArmarTexturas(PopupMenu* raiz, const W3dPaso& p) {
    std::vector<std::string> rutas;
    for (size_t i = 0; i < Textures.size(); i++)
        if (Textures[i] && !Textures[i]->path.empty()) rutas.push_back(Textures[i]->path);
    std::sort(rutas.begin(), rutas.end(), MenorSinMayus);
    rutas.erase(std::unique(rutas.begin(), rutas.end()), rutas.end());
    std::map<std::string, PopupMenu*> carpetas;
    for (size_t i = 0; i < rutas.size(); i++) {
        const size_t b = rutas[i].find_last_of("/\\");
        PopupMenu* m = MenuCarpeta(raiz, carpetas, b == std::string::npos ? std::string() : rutas[i].substr(0, b));
        m->Agregar(Base(rutas[i]), Opcion(rutas[i], 0), (int)IconType::textura)->verde = (rutas[i] == p.ref);
    }
}
static void Juntar(Object* o, int rt, Rutina* yo, std::vector<std::string>& v) {
    if (!o) return;
    bool ok = false;
    if (rt == RefRutina) ok = o->getType() == ObjectType::rutina && o != yo;
    else                 ok = o != yo && o->Parent != NULL;   // (la raiz no)
    if (ok) v.push_back(o->name);
    for (size_t i = 0; i < o->Childrens.size(); i++) Juntar(o->Childrens[i], rt, yo, v);
}
static void ArmarObjetos(PopupMenu* raiz, Rutina* r, int rt, const std::string& actual, bool conNinguno) {
    std::vector<std::string> v;
    Object* top = r;
    while (top->Parent) top = top->Parent;
    Juntar(top, rt, r, v);
    std::sort(v.begin(), v.end(), MenorSinMayus);
    v.erase(std::unique(v.begin(), v.end()), v.end());
    if (conNinguno) raiz->Agregar(T("(none)"), Opcion(std::string(), 0), -1)->verde = actual.empty();
    const int ico = rt == RefRutina ? (int)IconType::lista : (int)IconType::object;
    for (size_t i = 0; i < v.size(); i++) raiz->Agregar(v[i], Opcion(v[i], 0), ico)->verde = (v[i] == actual);
}
static void AccionRefElegida(int id) {
    Rutina* r = RuActiva(); W3dPaso* p = RuPaso(r);
    if (!p || id < 0 || id >= (int)gRefOpc.size()) return;
    if (gRefEsSegunda) p->ref2 = gRefOpc[(size_t)id].ref;
    else {
        p->ref = gRefOpc[(size_t)id].ref;
        if (UsaPartes(p->tipo)) p->entero = gRefOpc[(size_t)id].parte;
    }
    Cambio(r);
    PropsLayoutSucio();
}
static void AbrirMenuRef(bool segunda) {
    Rutina* r = RuActiva(); W3dPaso* p = RuPaso(r);
    if (!gPr || !p) return;
    PropButton* boton = segunda ? gPr->propRuRef2 : gPr->propRuRef;
    if (!boton) return;
    gRefEsSegunda = segunda;
    gPoolUsados = 0;
    gRefOpc.clear();
    PopupMenu* raiz = MenuNuevo();
    const int rt = segunda ? RefObjeto : W3dPasoRef(p->tipo);
    if (rt == RefMalla)        ArmarMallas(raiz, *p, UsaPartes(p->tipo));
    else if (rt == RefTextura) ArmarTexturas(raiz, *p);
    else                       ArmarObjetos(raiz, r, rt, segunda ? p->ref2 : p->ref, segunda);
    if (raiz->items.empty()) raiz->Agregar(T("(nothing to choose)"), -1, -1);
    PropsAbrirMenuBajoBoton(raiz, boton->button);
}
static void AccionMenuRef()  { AbrirMenuRef(false); }
static void AccionMenuRef2() { AbrirMenuRef(true); }

// ---- la LUZ (GL_LIGHT0..7) y la OPCION del paso (desplegables) ----
static PopupMenu* MenuRuLuz = NULL;
static PopupMenu* MenuRuOpc = NULL;
static void AccionLuzElegida(int id) {
    Rutina* r = RuActiva(); W3dPaso* p = RuPaso(r);
    if (!p || id < 0 || id >= W3D_RUTINA_LUCES) return;
    p->entero = id; Cambio(r);
}
static void AccionMenuLuz() {
    W3dPaso* p = RuPaso(RuActiva());
    if (!gPr || !gPr->propRuLuz || !p) return;
    if (!MenuRuLuz) { MenuRuLuz = new PopupMenu(); MenuRuLuz->action = AccionLuzElegida; }
    MenuRuLuz->Limpiar();
    for (int i = 0; i < W3D_RUTINA_LUCES; i++) {
        char b[48]; snprintf(b, sizeof b, "%s %d (GL_LIGHT%d)", T("Light"), i, i);
        MenuRuLuz->Agregar(b, i, (int)IconType::light)->verde = (i == p->entero);
    }
    PropsAbrirMenuBajoBoton(MenuRuLuz, gPr->propRuLuz->button);
}
static void AccionOpcElegida(int id) {
    Rutina* r = RuActiva(); W3dPaso* p = RuPaso(r);
    if (!p || id < 0 || id >= W3dOpcionesN(W3dPasoOpciones(p->tipo))) return;
    p->modo = id;
    if (p->tipo == PasoDibujar && id != RPrimTriangulos) p->sub = RangoParte;   // (el rango es de triangulos)
    Cambio(r);
    PropsLayoutSucio();
}
static void AccionMenuOpcion() {
    W3dPaso* p = RuPaso(RuActiva());
    if (!gPr || !gPr->propRuOpcion || !p) return;
    const int l = W3dPasoOpciones(p->tipo);
    if (!MenuRuOpc) { MenuRuOpc = new PopupMenu(); MenuRuOpc->action = AccionOpcElegida; }
    MenuRuOpc->Limpiar();
    for (int i = 0; i < W3dOpcionesN(l); i++)
        MenuRuOpc->Agregar(T(W3dOpcionEtiqueta(l, i)), i, W3dRutinaPasoIcono(p->tipo))->verde = (i == p->modo);
    PropsAbrirMenuBajoBoton(MenuRuOpc, gPr->propRuOpcion->button);
}
// "Max": el ultimo triangulo = el ultimo de la parte (aunque la malla cambie)
static void AccionMax() {
    Rutina* r = RuActiva(); W3dPaso* p = RuPaso(r);
    if (!p || p->tipo != PasoDibujar) return;
    p->n[1].ref.clear(); p->n[1].v = -1.0f;
    Cambio(r);
}

// (harness) abre un desplegable de la tarjeta: 0 Add, 1 la referencia, 2 la opcion, 3 la luz, 4 Render mode, 5 ref2
void PropsRutinaAbrirMenu(int cual) {
    switch (cual) {
    case 0: AccionMenuAdd(); break;
    case 1: AccionMenuRef(); break;
    case 2: AccionMenuOpcion(); break;
    case 3: AccionMenuLuz(); break;
    case 5: AccionMenuRef2(); break;
    default: AccionMenuLista(); break;
    }
}

void PropsRutinaConstruir(Properties* p) {
    if (!p) return;
    gPr = p;
    RutinaFilasCount = RuFilas; RutinaFilaTexto = RuFilaTexto; RutinaFilaSeleccionar = RuFilaSeleccionar;
    RutinaFilaIcono = RuFilaIcono; RutinaFilaValor = RuFilaValor;
    p->propRutina = new GroupPropertie(T("Routine"));
    p->propRutina->anchoValores = 0.30f;
    p->propRuLista = new PropButton("All", IconType::camera);
    p->propRuLista->button->desplegable = true; p->propRuLista->conLabel = true; p->propRuLista->name = T("Render mode");
    p->propRuLista->action = AccionMenuLista;
    p->propRutina->properties.push_back(p->propRuLista);
    p->propRuUsar = new PropButton("The list of All", IconType::lista);
    p->propRuUsar->button->desplegable = true; p->propRuUsar->conLabel = true; p->propRuUsar->name = T("Use");
    p->propRuUsar->action = AccionMenuUsar;
    p->propRutina->properties.push_back(p->propRuUsar);
    p->propRuPasos = new PropListMeshParts("Steps");
    p->propRuPasos->modo = 14;
    p->propRuPasos->filasMax = 8;
    p->propRutina->properties.push_back(p->propRuPasos);
    p->propRuFilaAdd = new PropButtonRow();
    Button* bAdd = p->propRuFilaAdd->Agregar(T("Add"), AccionMenuAdd);
    bAdd->desplegable = true;
    p->propRuFilaAdd->Agregar(T("Remove"), AccionRemove);
    p->propRutina->properties.push_back(p->propRuFilaAdd);
    // los campos del paso elegido (cada uno se ve solo si el paso lo usa)
    p->propRuRef = new PropButton("", IconType::mesh);
    p->propRuRef->button->desplegable = true; p->propRuRef->conLabel = true;
    p->propRuRef->action = AccionMenuRef;
    p->propRutina->properties.push_back(p->propRuRef);
    for (int k = 0; k < 5; k++) {
        p->propRuInfo[k] = new PropLabel("", k >= 3);   // (las de los punteros pueden ser largas: con corte de linea)
        p->propRutina->properties.push_back(p->propRuInfo[k]);
    }
    p->propRuRef2 = new PropButton("", IconType::object);
    p->propRuRef2->button->desplegable = true; p->propRuRef2->conLabel = true; p->propRuRef2->name = T("Object");
    p->propRuRef2->action = AccionMenuRef2;
    p->propRutina->properties.push_back(p->propRuRef2);
    p->propRuTexto = new PropText(T("Function"), "");
    p->propRutina->properties.push_back(p->propRuTexto);
    p->propRuLuz = new PropButton("", IconType::light);
    p->propRuLuz->button->desplegable = true; p->propRuLuz->conLabel = true; p->propRuLuz->name = T("Light");
    p->propRuLuz->action = AccionMenuLuz;
    p->propRutina->properties.push_back(p->propRuLuz);
    p->propRuOpcion = new PropButton("", IconType::lista);
    p->propRuOpcion->button->desplegable = true; p->propRuOpcion->conLabel = true;
    p->propRuOpcion->action = AccionMenuOpcion;
    p->propRutina->properties.push_back(p->propRuOpcion);
    p->propRuAviso = new PropLabel("", true);
    p->propRutina->properties.push_back(p->propRuAviso);
    p->propRuOn = new PropBool(T("On"));
    p->propRutina->properties.push_back(p->propRuOn);
    p->propRuColor = new PropColor(T("Color"));
    p->propRuColor->value = NULL;
    p->propRutina->properties.push_back(p->propRuColor);
    p->propRuColorNum = new PropBool(T("Numbers / memories"));
    p->propRutina->properties.push_back(p->propRuColorNum);
    p->propRuManual = new PropBool(T("Manual range"));
    p->propRutina->properties.push_back(p->propRuManual);
    p->propRuArray = new PropText(T("Array"), "");
    p->propRutina->properties.push_back(p->propRuArray);
    p->propRuEntero = new PropText(T("Value"), "");
    p->propRutina->properties.push_back(p->propRuEntero);
    for (int k = 0; k < 5; k++) {
        p->propRuNum[k] = new PropText("", "");
        p->propRutina->properties.push_back(p->propRuNum[k]);
    }
    p->propRuMax = new PropButtonRow();
    p->propRuMax->Agregar(T("Max (last triangle of the part)"), AccionMax);
    p->propRutina->properties.push_back(p->propRuMax);
    static const char* const kBuffers[3] = { "Color buffer", "Depth buffer", "Stencil buffer" };
    for (int k = 0; k < 3; k++) {
        p->propRuBuffer[k] = new PropBool(T(kBuffers[k]));
        p->propRutina->properties.push_back(p->propRuBuffer[k]);
    }
    p->propRuMotivo = new PropLabel("", true);
    p->propRuMotivo->oculto = true;
    p->propRuMotivo->rojo = true;
    p->propRutina->properties.push_back(p->propRuMotivo);
    p->GroupProperties.push_back(p->propRutina);
}

// un campo de texto con commit EN VIVO (lo tipeado entra al paso a cada tecla; sin foco muestra el valor real)
static bool Sync(PropText* pt, const std::string& actual, std::string& ultimo, std::string& tipeado) {
    if (!pt) return false;
    const bool foco = TextFieldEnVivo(&pt->field);
    if (foco) {
        if (pt->field.text != ultimo) { ultimo = pt->field.text; tipeado = pt->field.text; return true; }
        return false;
    }
    ultimo.clear();
    if (pt->field.text != actual) { pt->field.SetText(actual); g_redraw = true; }
    return false;
}
// un checkbox sobre algo que no es un bool: el PropBool apunta a 'espejo'. Si cambio desde el frame anterior fue un
// click (devuelve true y el valor nuevo queda en 'espejo'); si no, copia el valor real
static bool SyncBool(bool real, bool& espejo, bool& ultimo) {
    if (espejo != ultimo) { ultimo = espejo; return true; }
    espejo = ultimo = real;
    return false;
}
static bool EsMax(const std::string& s) {
    std::string t;
    for (size_t i = 0; i < s.size(); i++) if (!isspace((unsigned char)s[i])) t += (char)tolower((unsigned char)s[i]);
    return t == "max";
}
static const char* EtiquetaEntero(int t) {
    switch (W3dPasoEntero(t)) {
    case EnteroParte:  return "Part (-1 = all)";
    case EnteroCuenta: return "Steps to skip";
    default:           return "Value";
    }
}
static const char* EtiquetaRef(int t, int rt) {
    if (t == PasoDibujar || t == PasoVisible) return "Mesh part";
    switch (rt) {
    case RefMalla:   return "Mesh";
    case RefObjeto:  return "Object";
    case RefTextura: return "Texture";
    case RefRutina:  return "Routine";
    default:         return "Name";
    }
}
static int IconoRef(int rt) {
    switch (rt) {
    case RefMalla:   return (int)IconType::mesh;
    case RefTextura: return (int)IconType::textura;
    case RefRutina:  return (int)IconType::lista;
    default:         return (int)IconType::object;
    }
}
static void Fila(PropLabel* l, const std::string& txt, unsigned& firma, unsigned bit) {
    l->oculto = txt.empty();
    if (txt.empty()) return;
    firma |= bit;
    if (l->name != txt) { l->name = txt; PropsLayoutSucio(); }
}

void PropsRutinaActualizar(Properties* p, bool visible) {
    if (!p || !p->propRutina) return;
    Rutina* r = visible ? RuActiva() : NULL;
    p->propRutina->visible = (r != NULL);
    if (!r) {
        if (p->propRuOn) p->propRuOn->value = NULL;
        if (p->propRuManual) p->propRuManual->value = NULL;
        for (int k = 0; k < 3; k++) if (p->propRuBuffer[k]) p->propRuBuffer[k]->value = NULL;
        if (p->propRuColor) p->propRuColor->value = NULL;
        if (p->propRuColorNum) p->propRuColorNum->value = NULL;
        return;
    }
    if (p->propRuPasos) { p->propRuPasos->obj = r; p->propRuPasos->selectIndex = r->pasoActivo; }
    p->propRuLista->button->text = T(kModosUI[r->listaEditada]);
    p->propRuUsar->oculto = (r->listaEditada == 0);
    if (r->listaEditada > 0) {
        const int u = r->usar[r->listaEditada];
        std::string t = (u < 0) ? T("The list of All") : (u == r->listaEditada) ? T("Its own list")
                                : std::string(T("Same as")) + " " + T(kModosUI[u]);
        p->propRuUsar->button->text = t;
    }
    std::vector<W3dPaso>* L = RuLista(r);
    if (p->propRuFilaAdd && p->propRuFilaAdd->botones.size() >= 2)
        p->propRuFilaAdd->botones[1]->visible = (RuPaso(r) != NULL);     // Remove: solo con un paso elegido
    W3dPaso* paso = RuPaso(r);
    const int t = paso ? paso->tipo : -1;
    const bool dib = (t == PasoDibujar);
    const bool triangulos = dib && paso->modo == RPrimTriangulos;
    const bool manual = triangulos && paso->sub == RangoManual;
    static std::string ultNum[5], ultEnt, ultArr, ultTexto;
    std::string tipeado;
    unsigned firma = 0;     // que campos se ven (si cambia, el layout se re-arma)

    // la referencia: un desplegable con lo que hay de ese tipo (las mallas y las texturas, por carpetas)
    const int rt = paso ? W3dPasoRef(t) : RefNada;
    p->propRuRef->oculto = (rt == RefNada || rt == RefFuncion);
    if (!p->propRuRef->oculto) {
        firma |= 1;
        p->propRuRef->name = T(EtiquetaRef(t, rt));
        p->propRuRef->button->icon = IconoRef(rt);
        std::string txt = paso->ref.empty() ? std::string(T("(choose)")) : (rt == RefTextura ? Base(paso->ref) : paso->ref);
        if (UsaPartes(t) && !paso->ref.empty()) {
            MallaRecurso* m = MallaDe(paso);
            if (paso->entero < 0) txt += std::string(" / ") + T("all");
            else if (m && paso->entero < (int)m->partes.size()) txt += " / " + m->partes[(size_t)paso->entero].nombre;
            else { char b[16]; snprintf(b, sizeof b, " / %d", paso->entero); txt += b; }
        }
        p->propRuRef->button->text = txt;
    }
    // la info de la malla, en filas
    std::string fil[5];
    MallaRecurso* m = MallaDe(paso);
    if (m) {
        char b[200];
        if (UsaPartes(t)) {
            snprintf(b, sizeof b, "%s: %d", T("Triangles"), W3dMallaFlujoIndices(m, paso->entero, RPrimTriangulos) / 3); fil[0] = b;
            if (!dib) { snprintf(b, sizeof b, "%s: %d", T("Vertices"), m->vertexSize); fil[1] = b; }
            if (dib) {
                snprintf(b, sizeof b, "%s: %d (%s %d)", T("Indices"), W3dMallaFlujoIndices(m, paso->entero, paso->modo),
                         T("list"), W3dMallaFlujoIndices(m, paso->entero, RPrimTriangulos));
                fil[1] = b;   // (con que arrays se dibuja es estado: lo ponen los pasos de puntero, no este)
            }
        } else {
            snprintf(b, sizeof b, "%s: %d", T("Vertices"), m->vertexSize); fil[0] = b;
            const void* datos = t == PasoPunteroNormales ? (const void*)m->normals : t == PasoPunteroUV ? (const void*)m->uv :
                                t == PasoPunteroColores ? (const void*)m->vertexColor : (const void*)m->vertex;
            if (!datos) fil[1] = T("This mesh does not have this array: do not draw with the array on");
        }
    }
    for (int k = 0; k < 5; k++) Fila(p->propRuInfo[k], fil[k], firma, 1u << (16 + k));
    // el objeto del test de visibilidad (opcional) y, en "Llamar Lua", el objeto con el script y la funcion
    p->propRuRef2->oculto = (t != PasoVisible && t != PasoLua);
    if (!p->propRuRef2->oculto) {
        firma |= 1u << 23;
        p->propRuRef2->name = T(t == PasoLua ? "Script of" : "Object");
        p->propRuRef2->button->text = paso->ref2.empty() ? std::string(T(t == PasoLua ? "(this routine)" : "(none)")) : paso->ref2;
    }
    p->propRuTexto->oculto = (rt != RefFuncion);
    if (!p->propRuTexto->oculto) {
        firma |= 1u << 24;
        if (Sync(p->propRuTexto, paso->ref, ultTexto, tipeado)) { paso->ref = tipeado; Cambio(r); }
    }
    // la luz
    p->propRuLuz->oculto = !(paso && W3dPasoEntero(t) == EnteroLuz);
    if (!p->propRuLuz->oculto) {
        firma |= 4;
        char b[48]; snprintf(b, sizeof b, "%s %d", T("Light"), paso->entero);
        p->propRuLuz->button->text = b;
    }
    // la opcion (en "Dibujar elementos", la primitiva)
    const int opc = paso ? W3dPasoOpciones(t) : OpcNada;
    p->propRuOpcion->oculto = (opc == OpcNada);
    if (opc != OpcNada) {
        firma |= 8;
        p->propRuOpcion->name = T(W3dOpcionesTitulo(opc));
        p->propRuOpcion->button->icon = W3dRutinaPasoIcono(t);
        p->propRuOpcion->button->text = T(W3dOpcionEtiqueta(opc, paso->modo));
    }
    // el aviso: primitiva simulada / el formato del array
    std::string aviso;
    if (dib && W3dPrimSimulada(paso->modo))
        aviso = T("Simulated: OpenGL ES 1.1 has no quads or polygons. They are converted to triangles (more indices, slower).");
    else if (triangulos && !manual)
        aviso = T("Array: a memory @name[i] = count, then first/last triangle pairs (last -1 = max). Empty = the whole part.");
    Fila(p->propRuAviso, aviso, firma, 1u << 25);
    // on/off
    p->propRuOn->value = (paso && W3dPasoUsaOn(t)) ? &paso->on : NULL;
    if (p->propRuOn->value) { firma |= 16; p->propRuOn->name = T(W3dPasoEtiquetaOn(t)); }
    // los COLORES: el selector de color (o los 4 numeros, para usar memorias)
    const bool esColor = paso && W3dPasoEsColor(t);
    static float colEsp[4], colUlt[4];
    static bool colHay = false;
    static bool numEsp = false, numUlt = false, numReal = false;
    bool conNumeros = false;
    if (esColor) {
        bool memorias = false;
        for (int k = 0; k < 4; k++) if (!paso->n[k].ref.empty()) memorias = true;
        if (memorias) numReal = true;
        p->propRuColorNum->value = &numEsp;
        if (SyncBool(numReal, numEsp, numUlt)) numReal = numEsp;
        conNumeros = numReal;
        firma |= 1u << 26;
        if (conNumeros) p->propRuColor->value = NULL;
        else {
            firma |= 1u << 27;
            p->propRuColor->value = colEsp;
            p->propRuColor->name = T(W3dPasoEtiqueta(t));
            if (colHay && memcmp(colEsp, colUlt, sizeof colEsp) != 0) {
                for (int k = 0; k < 4; k++) { paso->n[k].ref.clear(); paso->n[k].v = colEsp[k]; }
                Cambio(r);
            }
            for (int k = 0; k < 4; k++) colEsp[k] = colUlt[k] = paso->n[k].v;
            colHay = true;
        }
    } else {
        p->propRuColor->value = NULL;
        p->propRuColorNum->value = NULL;
        colHay = false;
    }
    // Dibujar elementos: rango manual (checkbox) o array / la parte entera (solo con triangulos)
    static bool espManual = false, ultManual = false;
    p->propRuManual->value = triangulos ? &espManual : NULL;
    if (triangulos) {
        firma |= 32;
        if (SyncBool(manual, espManual, ultManual)) {
            if (espManual) {
                paso->sub = RangoManual;
                if (paso->n[0].ref.empty() && paso->n[0].v < 0) paso->n[0].v = 0;
            } else paso->sub = paso->n[2].ref.empty() ? RangoParte : RangoArray;
            Cambio(r);
        }
    }
    p->propRuArray->oculto = !(triangulos && !manual);
    if (!p->propRuArray->oculto) {
        firma |= 64;
        if (Sync(p->propRuArray, paso->n[2].ref, ultArr, tipeado)) {
            W3dNum& a = paso->n[2];
            std::string s = tipeado;
            while (!s.empty() && isspace((unsigned char)s[0])) s.erase(0, 1);
            if (s.empty()) { a.ref.clear(); paso->sub = RangoParte; }
            else { a.ref = s; paso->sub = RangoArray; }
            Cambio(r);
        }
    }
    // el entero (la parte se elige en el desplegable de la malla; aca solo los pasos a saltear)
    const int eu = paso ? W3dPasoEntero(t) : EnteroNada;
    p->propRuEntero->oculto = !(eu == EnteroCuenta);
    if (!p->propRuEntero->oculto) {
        firma |= 128;
        p->propRuEntero->name = T(EtiquetaEntero(t));
        char b[16]; snprintf(b, sizeof b, "%d", paso->entero);
        if (Sync(p->propRuEntero, b, ultEnt, tipeado)) { paso->entero = atoi(tipeado.c_str()); Cambio(r); }
    }
    // los numeros: un numero o "@memoria[i]" (el ultimo triangulo, ademas, "max"). Los colores, solo en modo numeros
    int nn = !paso ? 0 : dib ? (manual ? 2 : 0) : W3dPasoNumeros(*paso);
    if (esColor && !conNumeros) nn = 0;
    for (int k = 0; k < 5; k++) {
        PropText* f = p->propRuNum[k];
        const bool usa = k < nn || (dib && paso->sub == RangoArray && k == 3);   // (el array: sus banderas, opcional)
        f->oculto = !usa;
        if (!usa) continue;
        firma |= 256u << k;
        f->name = T(W3dPasoNumeroNombre(*paso, k));
        W3dNum& n = paso->n[k];
        const bool esUltimo = dib && k == 1;
        const std::string actual = (esUltimo && n.ref.empty() && n.v < 0) ? std::string("max") : NumTexto(n);
        if (Sync(f, actual, ultNum[k], tipeado)) {
            if (!tipeado.empty() && tipeado[0] == '@') n.ref = tipeado;
            else if (dib && k == 3) n.ref.clear();   // (sin banderas)
            else if (esUltimo && EsMax(tipeado)) { n.ref.clear(); n.v = -1.0f; }
            else { n.ref.clear(); n.v = (float)atof(tipeado.c_str()); }
            Cambio(r);
        }
    }
    // "Max" (manual)
    if (p->propRuMax) for (size_t b = 0; b < p->propRuMax->botones.size(); b++) p->propRuMax->botones[b]->visible = manual;
    if (manual) firma |= 1u << 13;
    // Clear: que buffers (la mascara de glClear)
    static bool espBuf[3] = { false, false, false }, ultBuf[3] = { false, false, false };
    for (int k = 0; k < 3; k++) {
        p->propRuBuffer[k]->value = (eu == EnteroBuffers) ? &espBuf[k] : NULL;
        if (eu != EnteroBuffers) continue;
        firma |= 1u << 14;
        if (SyncBool((paso->entero & (1 << k)) != 0, espBuf[k], ultBuf[k])) {
            if (espBuf[k]) paso->entero |= (1 << k); else paso->entero &= ~(1 << k);
            Cambio(r);
        }
    }
    // el motivo (la misma validacion que corre antes de dibujarla)
    const bool ok = W3dRutinaValidar(r);
    p->propRuMotivo->oculto = ok;
    if (!ok) { firma |= 1u << 15; if (p->propRuMotivo->name != r->motivo) { p->propRuMotivo->name = r->motivo; PropsLayoutSucio(); } }
    static unsigned ultimaFirma = 0;
    if (firma != ultimaFirma) { ultimaFirma = firma; PropsLayoutSucio(); }
}
