// ============================================================================
//  CambiosProyecto.cpp — lo no guardado (ver el .h)
// ============================================================================
#include "io/CambiosProyecto.h"
#include "animation/Animation.h"   // w3dGetTicks
#include "io/RecursosProyecto.h"
#include "io/BibliotecaExterna.h"
#include "io/W3dContenedor.h"
#include "io/TexturaEditada.h"
#include "io/GuardarAnimSets.h"
#include "importers/import_obj.h"   // TexturaPendienteDe: la textura base que todavia espera en la cola
#include "W3dRaices.h"
#include "io/RaicesEditor.h"
#include "objects/MallaRecurso.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "animation/W3dAnimSet.h"
#include "animation/SkeletalAnimation.h"
#include "io/W3dRecursos.h"
#include "ViewPorts/IDE.h"
#include "ViewPorts/LayoutArbol.h"
#include "config/W3dLang.h"
#include "WhiskUI/draw/icons.h"
#include "w3dGraphics.h"
#include "w3dlog.h"
#include <map>
#include <set>
#include <cstdio>

void (*W3dCambiosSalirHook)() = 0;
void (*W3dCambiosAbrirHook)(const std::string&) = 0;
void (*W3dCambiosNuevoHook)() = 0;
bool (*W3dCambiosGuardarHook)() = 0;
bool g_w3dCambiosSinCartel = false;

// ============================================================================
//  LAS FIRMAS
// ============================================================================
static void Num(std::string& s, double v) { char b[40]; snprintf(b, sizeof(b), "%.6g;", v); s += b; }
static void Col(std::string& s, const float* c) { for (int k = 0; k < 4; k++) Num(s, c[k]); }
static std::string Ruta(const Texture* t) { return t ? W3dTexturaClave(t->path) : std::string(); }
// la textura BASE de un material: la cargada o, si todavia no esta, la que espera en la cola diferida (o
// dormida). Al abrir, las texturas llegan de a una por cuadro DESPUES de la foto: con solo el puntero un
// material cambiaba de firma sin que nadie lo tocara (y todo proyecto con dos texturas quedaba "sin guardar")
static std::string RutaBase(const Material* m) {
    if (m->texture) return Ruta(m->texture);
    const std::string p = TexturaPendienteDe(m);
    return p.empty() ? std::string() : W3dTexturaClave(p);
}
// el contenido de un material (lo que se guarda de el; la carpeta es de la biblioteca)
static std::string FirmaMaterial(const Material* m) {
    std::string s = m->name + "\x01";
    Col(s, m->diffuse); Col(s, m->specular); Col(s, m->emission); Col(s, m->ambient);
    Num(s, m->shininess); Num(s, m->interpolacion); Num(s, m->reflectMode); Num(s, m->rtRugosidad); Num(s, m->rtMetalico);
    Num(s, m->depth_bias); Num(s, m->orden_pasada); Num(s, m->mezcla); Num(s, m->grosorLinea);
    const bool bs[14] = { m->textureOn, m->filtrado, m->transparent, m->vertexColor, m->lighting, m->repeat, m->uv8bit,
                          m->culling, m->depth_test, m->fondo, m->depth_write, m->chrome, m->normalMap, m->lineas };
    for (int k = 0; k < 14; k++) s += bs[k] ? '1' : '0';
    s += RutaBase(m) + "\x01" + Ruta(m->normalTexture);
    for (size_t c = 0; c < m->capas.size(); c++) { s += "\x02" + Ruta(m->capas[c].tex); Num(s, m->capas[c].blend); Num(s, m->capas[c].uvMapa); s += m->capas[c].on ? '1' : '0'; }
    return s;
}
// el contenido de un animset CARGADO (sus clips y la version de cada clip de jerarquia)
static std::string FirmaAnimSet(const std::string& nombre) {
    std::vector<W3dRecurso*> todos;
    W3dRecursosListar(W3DREC_ANIMSET, todos);
    for (size_t i = 0; i < todos.size(); i++) {
        if (!todos[i] || todos[i]->estado != W3DREC_LISTO || !todos[i]->dato) continue;
        const W3dAnimSet* set = (const W3dAnimSet*)todos[i]->dato;
        if (set->nombre != nombre) continue;
        std::string s;
        for (size_t k = 0; k < set->datos.clips.size(); k++) {
            const SkeletalAnimation* a = set->datos.clips[k];
            if (!a) continue;
            s += a->name; Num(s, a->startFrame); Num(s, a->endFrame); Num(s, a->FrameRate); Num(s, (double)a->tracks.size());
        }
        for (size_t k = 0; k < set->datos.jerarquias.size(); k++) {
            const W3dClipJer* c = set->datos.jerarquias[k];
            if (!c) continue;
            s += c->nombre; Num(s, c->version);
        }
        return s;
    }
    return std::string("-");   // (sin cargar: no se pudo tocar)
}
// la configuracion del proyecto (lo que va en proyecto.json fuera de los recursos y las raices)
static std::string FirmaProyecto() {
    extern int AnimFPS;
    std::string s;
    Num(s, AnimFPS);
    s += w3dEngine::MipmapsGlobal() ? 'm' : '-';
    s += w3dEngine::PixeladoGlobal() ? 'p' : '-';
    s += W3dRaizInicial() + "\x01";
    s += W3dLibreriasFirma();
    return s;
}

// ============================================================================
//  LA FOTO
// ============================================================================
static std::map<int, std::string> gMallas;                 // serial del recurso -> nombre + version
static std::map<const Material*, std::string> gMats;      // material -> su firma
static std::set<std::string> gClaves;                     // TODAS las claves de biblioteca que habia
static std::map<std::string, std::string> gNombres;       // clave -> nombre (la lista de BORRADOS)
static std::map<std::string, int> gIconos;                // clave -> icono
static std::map<std::string, std::string> gAnims;         // animset -> su firma
static std::map<const Object*, std::string> gRaices;      // raiz -> nombre + tipo
static std::set<std::string> gRaicesNombres;              // las raices sin cargar (por nombre)
static std::set<const Object*> gTocadas;                  // las raices EDITADAS desde la foto
static std::string gCarpetas, gProyecto;
static bool gHayFoto = false;
static unsigned gVersion = 1;

static std::string FirmaMalla(const MallaRecurso* r) {
    std::string s = r->nombre + "\x01";
    Num(s, r->version);
    s += r->modificado ? 'M' : '-';
    return s;
}

void W3dCambiosFoto() {
    gMallas.clear(); gMats.clear(); gClaves.clear(); gNombres.clear(); gIconos.clear(); gAnims.clear();
    gRaices.clear(); gRaicesNombres.clear(); gTocadas.clear();
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    for (size_t i = 0; i < reg.size(); i++) if (!reg[i]->borrado) gMallas[reg[i]->serial] = FirmaMalla(reg[i]);
    for (size_t i = (size_t)MaterialesBase(); i < Materials.size(); i++)
        if (Materials[i] && Materials[i] != MaterialDefecto) gMats[Materials[i]] = FirmaMaterial(Materials[i]);
    std::vector<W3dRecursoItem> its;
    W3dBibliotecaListar(-1, its);
    for (size_t i = 0; i < its.size(); i++) {
        const std::string k = W3dBibClave(its[i].tipo, its[i].id);
        gClaves.insert(k);
        gNombres[k] = its[i].nombre;
        gIconos[k] = its[i].icono;
        if (its[i].tipo == W3D_VISTA_ANIMACIONES && its[i].padre.empty()) gAnims[its[i].id] = FirmaAnimSet(its[i].id);
    }
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    for (size_t i = 0; i < fs.size(); i++) {
        const std::string f = fs[i].nombre + "\x01" + W3dRaizTipoClave(W3dRaizTipoDe((int)i));
        if (fs[i].raiz) gRaices[fs[i].raiz] = f;
        else gRaicesNombres.insert(fs[i].nombre);
    }
    gCarpetas = W3dCarpetasFirma(its);
    gProyecto = FirmaProyecto();
    gHayFoto = true;
    gVersion++;
}

static unsigned gEdiciones = 0;
unsigned W3dCambiosEdiciones() { return gEdiciones; }
void W3dCambiosTocado() {
    gEdiciones++;
    const int a = W3dRaizActiva();
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    if (a >= 0 && a < (int)fs.size() && fs[(size_t)a].raiz) {
        if (gTocadas.insert(fs[(size_t)a].raiz).second) gVersion++;
    }
}
unsigned W3dCambiosVersion() { return gVersion; }

// ============================================================================
//  LAS PREGUNTAS
// ============================================================================
// el IDE tiene ese script abierto con cambios sin guardar?
static bool ScriptSucioEnIDE(const std::string& ruta) {
    struct J { static bool Buscar(ViewportBase* n, const std::string& r) {
        if (!n) return false;
        if (n->isLeaf()) return n->ViewportKind() == 8 && ((IDE*)n)->sucio && ((IDE*)n)->archivo == r;
        if (n->ContainerKind() == 1) return Buscar(((ViewportRow*)n)->childA, r) || Buscar(((ViewportRow*)n)->childB, r);
        return Buscar(((ViewportColumn*)n)->childA, r) || Buscar(((ViewportColumn*)n)->childB, r); } };
    return J::Buscar(LayoutRaizCompleta(), ruta);
}

bool W3dCambiosRecursoSucio(int tipo, const std::string& id) {
    if (!gHayFoto) return false;
    const std::string k = W3dBibClave(tipo, id);
    switch (tipo) {
        case W3D_VISTA_MALLAS: {
            MallaRecurso* r = W3dMallaRecursoPorNombre(id);
            if (!r) return false;
            std::map<int, std::string>::iterator f = gMallas.find(r->serial);
            return f == gMallas.end() || f->second != FirmaMalla(r);
        }
        case W3D_VISTA_MATERIALES: {
            Material* m = W3dMaterialDeId(id);
            if (!m) return false;
            std::map<const Material*, std::string>::iterator f = gMats.find(m);
            return f == gMats.end() || f->second != FirmaMaterial(m);
        }
        case W3D_VISTA_TEXTURAS: {
            if (!gClaves.count(k)) return true;
            if (W3dContenedorEntradaEditada(id)) return true;
            Texture* t = W3dTexturaCargadaDe(id);
            return t && TexEditSinGuardar(t);
        }
        case W3D_VISTA_SONIDOS: case W3D_VISTA_SCRIPTS: case W3D_VISTA_FUENTES: case W3D_VISTA_VIDEOS:
            if (!gClaves.count(k)) return true;
            if (W3dContenedorEntradaEditada(id)) return true;
            return tipo == W3D_VISTA_SCRIPTS && ScriptSucioEnIDE(id);
        case W3D_VISTA_ANIMACIONES: {
            if (!gClaves.count(k)) return true;
            const size_t barra = id.find('/');
            const std::string set = (barra == std::string::npos) ? id : id.substr(0, barra);
            std::map<std::string, std::string>::iterator f = gAnims.find(set);
            return f == gAnims.end() || (f->second != "-" && f->second != FirmaAnimSet(set));
        }
        case W3D_VISTA_ESCENAS: case W3D_VISTA_PREFABS: {
            const int i = W3dRaizBuscar(tipo == W3D_VISTA_PREFABS ? W3D_RAIZ_PREFAB : W3D_RAIZ_ESCENA, id);
            return i >= 0 && W3dCambiosRaizSucia(i);
        }
    }
    return !gClaves.count(k);
}

bool W3dCambiosRaizSucia(int idx) {
    if (!gHayFoto) return false;
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    if (idx < 0 || idx >= (int)fs.size()) return false;
    const W3dRaizFila& f = fs[(size_t)idx];
    if (!f.raiz) return !gRaicesNombres.count(f.nombre);   // (sin cargar: solo se pudo renombrar)
    if (gTocadas.count(f.raiz)) return true;
    std::map<const Object*, std::string>::iterator it = gRaices.find(f.raiz);
    if (it == gRaices.end()) return !gRaicesNombres.count(f.nombre);   // nueva (una cargada despues de la foto tiene su nombre)
    return it->second != f.nombre + "\x01" + W3dRaizTipoClave(W3dRaizTipoDe(idx));
}

static std::string Tipo(int t) { return T(W3dVistaSingular(t)); }

void W3dCambiosListar(std::vector<W3dCambio>& out) {
    out.clear();
    if (!gHayFoto) return;
    if (FirmaProyecto() != gProyecto) {
        W3dCambio c; c.icono = (int)IconType::guardar; c.tipo = T("Project"); c.nombre = T("Settings");
        out.push_back(c);
    }
    // las raices
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    std::set<std::string> raicesAhora;
    for (size_t i = 0; i < fs.size(); i++) {
        raicesAhora.insert(fs[i].nombre);
        if (!W3dCambiosRaizSucia((int)i)) continue;
        const int t = W3dRaizTipoDe((int)i);
        W3dCambio c; c.icono = W3dRaizIcono(t);
        c.tipo = T(t == W3D_RAIZ_JUEGO ? "Game" : t == W3D_RAIZ_PREFAB ? "Prefab" : "Scene");
        c.nombre = fs[i].nombre;
        out.push_back(c);
    }
    // los recursos (las raices ya fueron: escenas y prefabs no se repiten)
    std::vector<W3dRecursoItem> its;
    W3dBibliotecaListar(-1, its);
    std::set<std::string> ahora;
    for (size_t i = 0; i < its.size(); i++) {
        const std::string k = W3dBibClave(its[i].tipo, its[i].id);
        ahora.insert(k);
        if (its[i].tipo == W3D_VISTA_ESCENAS || its[i].tipo == W3D_VISTA_PREFABS) continue;
        if (!W3dCambiosRecursoSucio(its[i].tipo, its[i].id)) continue;
        W3dCambio c; c.icono = its[i].icono; c.tipo = Tipo(its[i].tipo); c.nombre = its[i].nombre;
        if (!gClaves.count(k)) c.nombre += std::string(" (") + T("new") + ")";
        out.push_back(c);
    }
    // lo BORRADO (o renombrado: el nombre de antes ya no esta)
    for (std::set<std::string>::iterator it = gClaves.begin(); it != gClaves.end(); ++it) {
        if (ahora.count(*it)) continue;
        int t = 0; std::string id;
        if (!W3dBibDeClave(*it, &t, &id)) continue;
        if (t == W3D_VISTA_ESCENAS || t == W3D_VISTA_PREFABS) {
            if (raicesAhora.count(id)) continue;
        }
        // (un material o una malla renombrados siguen vivos: su fila nueva ya dice que cambiaron)
        if (t == W3D_VISTA_MATERIALES) {
            bool vivo = false;
            for (std::map<const Material*, std::string>::iterator m = gMats.begin(); m != gMats.end() && !vivo; ++m) {
                if (m->second.compare(0, id.size() + 1, id + "\x01") != 0) continue;
                // (el puntero de la foto puede ser de un material ya LIBERADO -un proyecto que se cerro sin sacar
                //  otra foto-: solo se mira si sigue en la lista de materiales)
                bool enLista = false;
                for (size_t q = 0; q < Materials.size() && !enLista; q++) enLista = (Materials[q] == m->first);
                if (enLista && W3dMaterialDeId(m->first->name) == m->first) vivo = true;
            }
            if (vivo) continue;
        }
        if (t == W3D_VISTA_MALLAS) {
            bool viva = false;
            for (std::map<int, std::string>::iterator m = gMallas.begin(); m != gMallas.end() && !viva; ++m) {
                if (m->second.compare(0, id.size() + 1, id + "\x01") != 0) continue;
                const MallaRecurso* r = W3dMallaRecursoPorSerial(m->first);
                viva = r && !r->borrado;
            }
            if (viva) continue;
        }
        W3dCambio c; c.icono = gIconos.count(*it) ? gIconos[*it] : W3dVistaIcono(t);
        c.tipo = Tipo(t);
        c.nombre = (gNombres.count(*it) ? gNombres[*it] : id) + " (" + T("deleted or renamed") + ")";
        out.push_back(c);
    }
    if (W3dCarpetasFirma(its) != gCarpetas) {   // (con la biblioteca ya listada: listarla es lo que cuesta)
        W3dCambio c; c.icono = (int)IconType::carpeta; c.tipo = T("Library"); c.nombre = T("Folders");
        out.push_back(c);
    }
}

bool W3dCambiosHay() {
    std::vector<W3dCambio> v;
    W3dCambiosListar(v);
    return !v.empty();
}
bool W3dCambiosHayCache() {
    static unsigned ultimo = 0, version = 0, ediciones = 0;
    static bool hay = false, listo = false;
    const unsigned ahora = w3dGetTicks();
    // (una foto nueva -guardar/abrir- o una edicion nueva se ven en el acto; lo demas, cada medio segundo)
    if (!listo || version != gVersion || ediciones != gEdiciones || ahora - ultimo > 500) {
        hay = W3dCambiosHay();
        ultimo = ahora; version = gVersion; ediciones = gEdiciones; listo = true;
    }
    return hay;
}

// ============================================================================
//  EL CARTEL (la accion pendiente mientras esta abierto)
// ============================================================================
static bool gCartel = false;
static int gAccion = W3D_CAMBIOS_SALIR;
static std::string gRutaAbrir;
extern void CambiosPopupAbrir(int accion);   // ui/ViewPorts/PopUp/CambiosPopup.cpp

bool W3dCambiosCartelAbierto() { return gCartel; }

bool W3dCambiosPreguntar(int accion, const std::string& ruta) {
    if (g_w3dCambiosSinCartel) return false;
    if (!W3dCambiosHay()) return false;
    gCartel = true;
    gAccion = accion;
    gRutaAbrir = ruta;
    CambiosPopupAbrir(accion);
    return true;
}

static void Seguir() {
    if (gAccion == W3D_CAMBIOS_SALIR) { if (W3dCambiosSalirHook) W3dCambiosSalirHook(); }
    else if (gAccion == W3D_CAMBIOS_ABRIR) {
        if (W3dCambiosAbrirHook) W3dCambiosAbrirHook(gRutaAbrir);
        else {
            // (sin gancho -Symbian-: la apertura diferida de siempre, la que drena el loop de cada sistema)
            extern std::string g_proyAbrirPendiente;
            g_proyAbrirPendiente = gRutaAbrir;
        }
    }
    else if (gAccion == W3D_CAMBIOS_NUEVO) { if (W3dCambiosNuevoHook) W3dCambiosNuevoHook(); }
}
// "Guardar y ..." sin gancho (Symbian): el guardado de siempre sobre el archivo abierto
static bool GuardarPorDefecto() {
    extern std::string w3dPath;
    extern bool GuardarW3D(const std::string&);
    if (w3dPath.empty()) return false;
    return GuardarW3D(w3dPath);
}
void W3dCambiosResponder(int r) {
    gCartel = false;
    if (r == 2) { w3dLogf("[cambios] cancelado"); return; }
    if (r == 0) {
        // "Guardar y ...": si el guardado no sale, no se sigue (se pierde lo que no se guardo). Con el Play
        // andando el juego se para ANTES: se guarda la escena del usuario, no la partida (GuardarW3D no guarda
        // jugando: lo destruido por los scripts esta descolgado y lo instanciado lo borra el Stop)
        { extern void SimStop(); SimStop(); }
        const bool ok = W3dCambiosGuardarHook ? W3dCambiosGuardarHook() : GuardarPorDefecto();
        if (!ok) { w3dLogfW("[cambios] no se guardo: no sigo"); return; }
    }
    Seguir();
}
