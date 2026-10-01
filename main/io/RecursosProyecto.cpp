// ============================================================================
//  RecursosProyecto.cpp — la BIBLIOTECA del proyecto (ver el .h): los proveedores
//  de este modulo (mallas, materiales, texturas, animsets y los archivos: sonidos,
//  scripts, fuentes, videos), el arbol UNICO de carpetas cosmeticas con su undo,
//  el recurso activo, los datos por tipo, dentro/fuera del .w3d y el guardado.
// ============================================================================
#include "io/RecursosProyecto.h"
#include "io/Streaming.h"            // los pedidos del streaming son usuarios de una textura
#include "W3dRaices.h"                  // los recursos se cuentan en TODAS las raices (escenas/prefabs)
#include "io/W3dContenedor.h"
#include "io/JsonW3d.h"
#include "io/GuardarAnimSets.h"        // usuarios / renombrar / carpeta de los animsets
#include "io/MallasProyecto.h"         // TODA malla es un recurso: se asegura antes de listar
#include "io/UI2DFormato.h"            // la carpeta del .w3d abierto (base de las rutas "ext:")
#include "io/RecursosDatos.h"          // los datos de imagenes y sonidos (Properties)
#include "io/Miniaturas.h"             // las miniaturas de la cuadricula se sueltan al cerrar el proyecto
#include "base/W3dInteractionState.h"  // InteractionMode (borrar objetos solo en Object Mode)
#include "w3dGraphics.h"               // MipmapsGlobal (la memoria de una textura)
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Armature.h"
#include "objects/Particulas.h"
#include "objects/Imagen2D.h"
#include "objects/Slice9.h"
#include "objects/Boton2D.h"
#include "objects/Texto2D.h"           // la FUENTE BITMAP de un texto 2D es un usuario de textura
#include "objects/Video2D.h"           // el archivo de un video 2D
#include "animation/W3dAnimSet.h"
#include "animation/SkeletalAnimation.h" // W3D_RETARGET_*: el retarget por defecto de un clip de jerarquia
#include "animation/Flipbook.h"        // SceneFlipbooks: el atlas de un flipbook es un usuario de textura
#include "animation/Animation.h"       // w3dGetTicks: lo leido de un archivo se recuerda un rato
#include "io/W3dRecursos.h"
#include "io/W3dZip.h"                 // el contenedor VIEJO del guardado (la entrada se conserva?)
#include "script/W3dScript.h"          // W3dScriptDatos (las rutas de los .lua de cada objeto)
#include "importers/import_obj.h"      // TexturaPendienteDe: la textura base todavia en la cola
#include "io/TexturaEditada.h"         // la pintura de una textura purgada en caliente suelta sus pixeles
#include "base/W3dNombres.h"
#include "undo/Undo.h"
#include "W3dAviso.h"
#include "config/W3dLang.h"            // T(): el aviso de los scripts al renombrar una textura
#include "WhiskUI/draw/icons.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <algorithm>
#include <map>
#include <set>
#include <cstdio>
#include <cstring>

// LO NO GUARDADO (io/CambiosProyecto.h): un paso de undo de la BIBLIOTECA (una carpeta, el nombre de un recurso,
// un material nuevo) no es una edicion de la RAIZ activa: mientras vive este guard el undo no la marca
extern int g_undoSinRaiz;   // undo/Undo.cpp
struct SinRaiz { SinRaiz() { g_undoSinRaiz++; } ~SinRaiz() { g_undoSinRaiz--; } };

// ============================================================================
//  LOS TIPOS
// ============================================================================
static const char* const kClaves[W3D_VISTAS] = {
    "escena", "mallas", "materiales", "texturas", "animaciones", "prefabs", "escenas", "librerias",
    "sonidos", "scripts", "fuentes", "videos"
};
static const char* const kTitulos[W3D_VISTAS] = {
    "Scene", "3D Meshes", "Materials", "Textures", "Animations", "Prefabs", "Scenes", "Libraries",
    "Sounds", "Scripts", "Fonts", "Videos"
};
static const char* const kSingular[W3D_VISTAS] = {
    "Scene", "3D Mesh", "Material", "Texture", "Animation", "Prefab", "Scene", "Library",
    "Sound", "Script", "Font", "Video"
};
const char* W3dVistaClave(int v)    { return (v >= 0 && v < W3D_VISTAS) ? kClaves[v] : ""; }
const char* W3dVistaTitulo(int v)   { return (v >= 0 && v < W3D_VISTAS) ? kTitulos[v] : ""; }
const char* W3dVistaSingular(int v) { return (v >= 0 && v < W3D_VISTAS) ? kSingular[v] : ""; }
int W3dVistaDeClave(const std::string& c) {
    for (int v = 0; v < W3D_VISTAS; v++) if (c == kClaves[v]) return v;
    return -1;
}
int W3dVistaIcono(int v) {
    switch (v) {
        case W3D_VISTA_ESCENA:      return (int)IconType::archive;
        case W3D_VISTA_MALLAS:      return (int)IconType::mesh;
        case W3D_VISTA_MATERIALES:  return (int)IconType::material;
        case W3D_VISTA_TEXTURAS:    return (int)IconType::textura;
        case W3D_VISTA_ANIMACIONES: return (int)IconType::animacion;
        case W3D_VISTA_PREFABS:     return (int)IconType::prefab;
        case W3D_VISTA_ESCENAS:     return (int)IconType::camera;   // (el de una raiz ESCENA)
        case W3D_VISTA_LIBRERIAS:   return (int)IconType::libreria;
        case W3D_VISTA_SONIDOS:     return (int)IconType::sonido;
        case W3D_VISTA_SCRIPTS:     return (int)IconType::script;
        case W3D_VISTA_FUENTES:     return (int)IconType::lista;    // (el de un texto 2D)
        case W3D_VISTA_VIDEOS:      return (int)IconType::video;
    }
    return (int)IconType::archive;
}

std::string W3dProveedorRecursos::IdPedido(const std::string& id, const std::string& nuevo) {
    return W3dNombreNormalizar(nuevo, id.c_str());
}

bool W3dProveedorRecursos::Info(const std::string& id, W3dRecursoItem& out) {
    std::vector<W3dRecursoItem> v;
    Listar(v);
    for (size_t i = 0; i < v.size(); i++) if (v[i].id == id) { out = v[i]; return true; }
    return false;
}
// los datos por defecto: la linea de datos del item
void W3dProveedorRecursos::Datos(const std::string& id, std::vector<W3dRecursoDato>& out) {
    out.clear();
    W3dRecursoItem it;
    if (Info(id, it) && !it.info.empty()) out.push_back(W3dRecursoDato(T("Info"), it.info));
}

// ============================================================================
//  RECORRIDOS DE LA ESCENA (un pase por listado: contar usuarios es O(objetos))
// ============================================================================
static void JuntarObjetos(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        out.push_back(h);
        JuntarObjetos(h, out);
    }
}
// los objetos de TODAS las raices del proyecto: la escena activa y las escenas/prefabs cargados que
// no se estan mirando (W3dRaices.h). Un recurso que solo usa otra escena NO es huerfano.
static void ObjetosEscena(std::vector<Object*>& out) {
    out.clear();
    std::vector<Object*> raices;
    W3dRaicesVivas(raices);
    for (size_t i = 0; i < raices.size(); i++) JuntarObjetos(raices[i], out);
}
// 'fn' sobre cada raiz viva (para los recorridos que ya reciben una raiz)
static bool EnAlgunaRaiz(bool (*fn)(Object*, const std::string&), const std::string& a) {
    std::vector<Object*> raices;
    W3dRaicesVivas(raices);
    for (size_t i = 0; i < raices.size(); i++) if (fn(raices[i], a)) return true;
    return false;
}

// BORRA esos objetos de la escena ACTIVA con su paso de undo (el Delete de siempre: los detacha y el
// Ctrl+Z los devuelve). false = alguno vive en otra escena (no esta en el arbol que se edita) o no se
// puede borrar ahora (fuera de Object Mode).
extern void Eliminar(bool IncluirCollecciones);   // objects/ObjectMode.cpp
static bool W3dBorrarObjetosConUndo(const std::vector<Object*>& us) {
    if (InteractionMode != ObjectMode || !SceneCollection) return false;
    for (size_t i = 0; i < us.size(); i++) if (!us[i] || W3dRaizDe(us[i]) != SceneCollection) return false;
    DeseleccionarTodo();
    for (size_t i = 0; i < us.size(); i++) { us[i]->select = true; ObjSelects.push_back(us[i]); }
    Eliminar(false);
    return true;
}

static std::string EnteroTexto(long n) { char b[32]; sprintf(b, "%ld", n); return std::string(b); }
// "24 vertices", "1 part": el numero y la palabra TRADUCIDA, en singular o plural. Los datos de un
// recurso (W3dRecursoItem::info) se arman ya en el idioma de la UI: un texto con numeros adentro no
// esta en la tabla y T() lo devolveria en ingles.
static std::string Cantidad(long n, const char* uno, const char* varios) {
    return EnteroTexto(n) + " " + T(n == 1 ? uno : varios);
}

// el nombre corto de una ruta ("texturas/piso.png" -> "piso.png")
static std::string BaseDeRuta(const std::string& r) {
    size_t p = r.find_last_of("/\\");
    return (p == std::string::npos) ? r : r.substr(p + 1);
}

// "yes" / "no" en el idioma de la UI
static std::string SiNo(bool b) { return std::string(T(b ? "Yes" : "No")); }
static std::string Plural(long n, const char* uno, const char* varios) { return Cantidad(n, uno, varios); }

// el TAMANO de un archivo del proyecto: una entrada (la editada en la sesion o la del zip montado) o
// un archivo de disco (una referencia externa). -1 = no esta.
static long TamArchivo(const std::string& r) {
    if (r.empty()) return -1;
    std::vector<unsigned char> ed;
    if (W3dContenedorLeerEditada(r, ed)) return (long)ed.size();
    W3dZipLector* z = W3dContenedorLector();
    if (z && z->Existe(r)) return (long)z->Tam(r);
    FILE* f = fopen(r.c_str(), "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fclose(f);
    return n;
}
// los bytes de un archivo del proyecto (entrada o disco). false = no se pudo leer.
static bool BytesArchivo(const std::string& r, std::vector<unsigned char>& out) {
    out.clear();
    return !r.empty() && w3dFileSystem::ReadFileBytes(r, out) && !out.empty();
}
// LO QUE DICE EL ARCHIVO de una textura o un sonido (sus encabezados) y cuantos bytes tiene. La tarjeta del
// recurso en Properties pide los datos en CADA cuadro que se dibuja: leer el archivo entero (inflarlo del
// .w3d) y decodificar la imagen para saber que alfa tiene, cuadro por cuadro, trababa la UI con una textura
// grande o un video. Se recuerda el ULTIMO que se pidio; se vuelve a leer si cambio su tamano, si no se
// sabe su tamano, si la biblioteca se invalido (W3dRecursosVistaInvalidar: un rename, adentro/afuera...) o
// cada 2 s (un archivo externo que se reemplazo afuera por otro del mismo tamano).
namespace {
struct InfoArchivo {
    std::string id;
    int tipo;                 // la vista que lo pidio (imagen o sonido: se decodifica solo lo suyo)
    long tam;                 // TamArchivo al leerlo
    long bytes;               // los bytes leidos (0 = no se pudo)
    unsigned leido;           // w3dGetTicks al leerlo
    bool esImagen, esSonido;
    W3dImagenDatos im;
    W3dSonidoDatos son;
    InfoArchivo() : tipo(-1), tam(-1), bytes(0), leido(0), esImagen(false), esSonido(false) {}
};
}
static InfoArchivo gInfoArch;
static const InfoArchivo& InfoDeArchivo(int tipo, const std::string& id) {
    const long tam = TamArchivo(id);
    const unsigned ahora = w3dGetTicks();
    if (tam >= 0 && gInfoArch.tipo == tipo && gInfoArch.tam == tam && gInfoArch.id == id && ahora - gInfoArch.leido < 2000)
        return gInfoArch;
    gInfoArch = InfoArchivo();
    gInfoArch.id = id; gInfoArch.tipo = tipo; gInfoArch.tam = tam; gInfoArch.leido = ahora;
    std::vector<unsigned char> b;
    if (BytesArchivo(id, b)) {   // (sin tamano conocido se lee igual, pero no se recuerda: ver arriba)
        gInfoArch.bytes = (long)b.size();
        if (tipo == W3D_VISTA_TEXTURAS) gInfoArch.esImagen = W3dImagenInfo(&b[0], b.size(), gInfoArch.im);
        if (tipo == W3D_VISTA_SONIDOS)  gInfoArch.esSonido = W3dSonidoInfo(&b[0], b.size(), gInfoArch.son);
    }
    return gInfoArch;
}

// ============================================================================
//  PROVEEDOR: MALLAS 3D (el registro de objects/MallaRecurso.h)
// ============================================================================
static void UsuariosMallas(std::map<const MallaRecurso*, int>& cuenta, std::vector<Object*>* todos) {
    std::vector<Object*> objs;
    ObjetosEscena(objs);
    for (size_t i = 0; i < objs.size(); i++) {
        if (objs[i]->getType() != ObjectType::mesh) continue;
        const Mesh* m = (const Mesh*)objs[i];
        if (!m->malla) continue;
        cuenta[m->malla]++;
        // un recurso que el usuario BORRO y que un undo le devolvio a un objeto vuelve a ser del proyecto
        if (m->malla->borrado) m->malla->borrado = false;
    }
    if (todos) todos->swap(objs);
}
// el recurso de la biblioteca con ese nombre (sin los BORRADOS, que ya no son del proyecto)
static MallaRecurso* MallaViva(const std::string& id) {
    MallaRecurso* r = W3dMallaRecursoPorNombre(id);
    return (r && !r->borrado) ? r : NULL;
}

class ProveedorMallas : public W3dProveedorRecursos {
public:
    static void Item(const MallaRecurso* r, int usuarios, W3dRecursoItem& it) {
        it.id = r->nombre;
        it.nombre = r->nombre;
        it.carpeta = r->carpeta;
        it.entrada = r->entrada;
        it.icono = (int)IconType::mesh;
        it.usuarios = usuarios;
        if (r->Cargada() && r->vertexSize > 0)
            it.info = Cantidad(r->vertexSize, "vertex", "vertices") + ", " + Cantidad(r->facesSize / 3, "triangle", "triangles") +
                      ", " + Cantidad((long)r->partes.size(), "part", "parts");
        else it.info = T("not loaded");   // (un huerfano que nadie pidio: sus arrays no estan en memoria)
    }
    void Listar(std::vector<W3dRecursoItem>& out) {
        out.clear();
        std::map<const MallaRecurso*, int> cuenta;
        UsuariosMallas(cuenta, NULL);
        const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
        for (size_t i = 0; i < reg.size(); i++) {
            if (reg[i]->borrado) continue;   // (el usuario lo borro: vive solo para el undo)
            W3dRecursoItem it;
            Item(reg[i], cuenta[reg[i]], it);
            out.push_back(it);
        }
    }
    bool Info(const std::string& id, W3dRecursoItem& out) {
        MallaRecurso* r = MallaViva(id);
        if (!r) return false;
        Item(r, W3dMallaRecursoUsuariosEnEscena(r), out);
        return true;
    }
    bool FijarCarpeta(const std::string& id, const std::string& carpeta) {
        MallaRecurso* r = MallaViva(id);
        if (!r) return false;
        r->carpeta = carpeta;
        return true;
    }
    bool Duplicar(const std::string& id, std::string* idNuevo, std::string* motivo) {
        MallaRecurso* r = MallaViva(id);
        if (!r) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        MallaRecurso* n = W3dMallaDuplicarRecurso(r);
        if (!n) { if (motivo) *motivo = "Its geometry couldn't be read"; return false; }
        if (idNuevo) *idNuevo = n->nombre;
        return true;
    }
    // EN USO: sus objetos se BORRAN (con Ctrl+Z, como el Delete de la escena: una malla no puede quedar
    // sin su malla 3D) y el recurso deja de ser del proyecto; el undo devuelve los objetos con su recurso
    bool SabeBorrarEnUso() const { return true; }
    bool BorrarEnUso(const std::string& id, std::string* motivo) {
        MallaRecurso* r = MallaViva(id);
        if (!r) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        std::vector<Object*> us;
        Usuarios(id, us);
        if (!us.empty() && !W3dBorrarObjetosConUndo(us)) { if (motivo) *motivo = "Its objects couldn't be deleted"; return false; }
        if (!W3dMallaRecursoBorrar(r)) r->borrado = true;   // (lo retiene el undo: se va cuando lo suelte)
        return true;
    }
    void Datos(const std::string& id, std::vector<W3dRecursoDato>& out) {
        out.clear();
        MallaRecurso* r = MallaViva(id);
        if (!r) return;
        if (r->Cargada()) {
            out.push_back(W3dRecursoDato(T("Vertices"), EnteroTexto(r->vertexSize)));
            out.push_back(W3dRecursoDato(T("Triangles"), EnteroTexto(r->facesSize / 3)));
            std::string mats;
            for (size_t k = 0; k < r->partes.size(); k++) {
                if (k) mats += ", ";
                const Material* m = (k < r->materiales.size()) ? r->materiales[k] : NULL;
                mats += m ? m->name : (r->partes[k].material.empty() ? std::string(T("(default)")) : r->partes[k].material);
            }
            out.push_back(W3dRecursoDato(T("Parts"), EnteroTexto((long)r->partes.size()) + (mats.empty() ? std::string() : " (" + mats + ")")));
            out.push_back(W3dRecursoDato(T("Vertex groups"), EnteroTexto((long)r->firmaGrupos.size())));
            out.push_back(W3dRecursoDato(T("Shape keys"), EnteroTexto((long)r->firmaFormas)));
            // SKINNEADA: algun objeto que la usa se deforma con un armature (el skinning es del OBJETO; los
            // grupos de vertices solos -una seleccion guardada, un grupo que lee Lua- no la hacen skinneada)
            bool skin = false;
            std::vector<Object*> us; Usuarios(id, us);
            for (size_t k = 0; k < us.size() && !skin; k++) skin = ((const Mesh*)us[k])->skinArmature != NULL;
            out.push_back(W3dRecursoDato(T("Skinned"), SiNo(skin)));
            out.push_back(W3dRecursoDato(T("CPU memory"), W3dBytesTexto((double)r->Bytes())));
            out.push_back(W3dRecursoDato(T("GPU memory"), r->vboVer ? W3dBytesTexto((double)r->Bytes()) : std::string(T("not uploaded"))));
        } else out.push_back(W3dRecursoDato(T("State"), T("not loaded")));
        const long tam = TamArchivo(r->entrada);
        if (tam >= 0) out.push_back(W3dRecursoDato(T("In the .w3d"), W3dBytesTexto((double)tam)));
        out.push_back(W3dRecursoDato(T("Users"), EnteroTexto(W3dMallaRecursoUsuariosEnEscena(r))));
    }
    bool Renombrar(const std::string& id, const std::string& nuevo, std::string* final) {
        MallaRecurso* r = MallaViva(id);
        if (!r) return false;
        const std::string n = W3dMallaNombreLibre(W3dNombreNormalizar(nuevo, "Malla"), r);
        if (final) *final = n;
        if (n == r->nombre) return true;
        // el MISMO paso de undo que el rename de la tarjeta "3D Mesh" (por serial: el registro se purga)
        UndoCapturarRename(W3dDestGlobal(W3dRenameDest::MallaG, r->serial));
        r->nombre = n;
        return true;
    }
    bool Borrar(const std::string& id, std::string* motivo) {
        MallaRecurso* r = MallaViva(id);
        if (!r) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        if (W3dMallaRecursoUsuariosEnEscena(r) > 0) { if (motivo) *motivo = "It is in use"; return false; }
        // un paso de undo todavia lo nombra (un objeto borrado con Ctrl+Z): deja de ser del proyecto ya y
        // se va cuando el undo lo suelte (si un Ctrl+Z le devuelve el objeto, vuelve)
        if (!W3dMallaRecursoBorrar(r)) r->borrado = true;
        return true;
    }
    void Usuarios(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        MallaRecurso* r = MallaViva(id);
        if (!r) return;
        std::vector<Object*> objs;
        ObjetosEscena(objs);
        for (size_t i = 0; i < objs.size(); i++)
            if (objs[i]->getType() == ObjectType::mesh && ((Mesh*)objs[i])->malla == r) out.push_back(objs[i]);
    }
};

// ============================================================================
//  PROVEEDOR: MATERIALES (la lista global Materials, sin los de la UI ni el por defecto)
// ============================================================================
// las mallas huerfanas cuyos materiales NO se pudieron leer (entrada rota): no se reintenta por
// cuadro (por serial; se olvida al invalidar: abrir, guardar)
static std::set<int> gMatsIlegibles;
// usuarios de cada material: cada MALLA de la escena que lo usa en alguna parte cuenta una
// vez, y cada malla HUERFANA del registro (se guarda con sus materiales) tambien
static void UsuariosMateriales(std::map<const Material*, int>& cuenta) {
    std::vector<Object*> objs;
    ObjetosEscena(objs);
    std::map<const MallaRecurso*, int> usoMalla;
    for (size_t i = 0; i < objs.size(); i++) {
        if (objs[i]->getType() != ObjectType::mesh) continue;
        const Mesh* m = (const Mesh*)objs[i];
        if (m->malla) usoMalla[m->malla]++;
        std::set<const Material*> vistos;
        for (size_t g = 0; g < m->materialsGroup.size(); g++) {
            const Material* mat = m->materialsGroup[g].material;
            if (mat && vistos.insert(mat).second) cuenta[mat]++;
        }
    }
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    for (size_t i = 0; i < reg.size(); i++) {
        if (usoMalla[reg[i]] > 0) continue;
        // huerfana: sus materiales (resueltos aunque no este cargada; se cachean en el recurso)
        if (reg[i]->materiales.empty() && !reg[i]->entrada.empty() && !gMatsIlegibles.count(reg[i]->serial))
            if (!W3dMallaRecursoMateriales(reg[i], NULL)) gMatsIlegibles.insert(reg[i]->serial);
        std::set<const Material*> vistos;
        for (size_t k = 0; k < reg[i]->materiales.size(); k++) {
            const Material* mat = reg[i]->materiales[k];
            if (mat && vistos.insert(mat).second) cuenta[mat]++;
        }
    }
}
// los materiales DEL PROYECTO (arriba de la marca de la UI, sin el por defecto). Antes de
// listar, un material PURGADO que un undo volvio a poner en una malla vuelve a la lista.
static void MaterialesProyecto(std::vector<Material*>& out, std::map<const Material*, int>& cuenta) {
    UsuariosMateriales(cuenta);
    const std::vector<Material*>& pur = MaterialesPurgados();
    std::vector<Material*> revivir;
    for (size_t i = 0; i < pur.size(); i++) if (cuenta[pur[i]] > 0) revivir.push_back(pur[i]);
    for (size_t i = 0; i < revivir.size(); i++) MaterialRevivir(revivir[i]);
    out.clear();
    // (los de una LIBRERIA externa no son del proyecto: se ven en la vista de su libreria)
    for (size_t i = (size_t)MaterialesBase(); i < Materials.size(); i++)
        if (Materials[i] && Materials[i] != MaterialDefecto && Materials[i]->libreria.empty()) out.push_back(Materials[i]);
}
static Material* MaterialDeNombre(const std::string& n) {
    for (size_t i = (size_t)MaterialesBase(); i < Materials.size(); i++)
        if (Materials[i] && Materials[i] != MaterialDefecto && Materials[i]->name == n) return Materials[i];
    return NULL;
}

// el paso de undo de un material CREADO por la biblioteca ("Duplicate", el "New Material" del "+"): deshacer lo
// PURGA (queda vivo en el cementerio de materiales) y rehacer lo revive
struct UndoMatCreado { Material* m; bool vivo; };
static void UndoMatCreadoAplicar(void* d) {
    UndoMatCreado* u = (UndoMatCreado*)d;
    if (u->vivo) { if (MaterialPurgar(u->m)) u->vivo = false; }
    else if (MaterialRevivir(u->m)) u->vivo = true;
    W3dRecursosVistaInvalidar();
}
static void UndoMatCreadoLiberar(void* d) { delete (UndoMatCreado*)d; }
// un material NUEVO de la biblioteca (copia de 'de', o uno en blanco), con su paso de undo
static Material* MaterialNuevoConUndo(const std::string& base, const Material* de) {
    Material* n = new Material(MaterialNombreLibre(W3dNombreNormalizar(base, "Material"), NULL));
    if (de) {
        const std::string nombre = n->name;
        *n = *de;                  // (las texturas se COMPARTEN: cada ranura de la copia retiene la suya)
        n->name = nombre;
        TexturaRetener(n->texture);
        TexturaRetener(n->normalTexture);
        for (size_t c = 0; c < n->capas.size(); c++) TexturaRetener(n->capas[c].tex);
    }
    UndoMatCreado* u = new UndoMatCreado();
    u->m = n; u->vivo = true;
    UndoExterno f; f.aplicar = UndoMatCreadoAplicar; f.liberar = UndoMatCreadoLiberar;
    UndoPushExterno(f, u);
    return n;
}
Material* W3dBibliotecaNuevoMaterial() { SinRaiz sr; return MaterialNuevoConUndo("Material", NULL); }

// el paso de undo del borrado de un material EN USO: las partes que lo usaban vuelven a el (y el
// material vuelve de su cementerio: W3dRecursosRevisarPurgados). Rehacer las vuelve a dejar sin el.
struct UndoMatSacado {
    Material* m;
    std::vector<Mesh*> mallas; std::vector<int> partes;
    bool sacado;
};
static void UndoMatSacadoAplicar(void* d) {
    UndoMatSacado* u = (UndoMatSacado*)d;
    for (size_t i = 0; i < u->mallas.size(); i++) {
        Mesh* m = u->mallas[i];
        if (!m || u->partes[i] < 0 || u->partes[i] >= (int)m->materialsGroup.size()) continue;
        m->materialsGroup[(size_t)u->partes[i]].material = u->sacado ? u->m : MaterialDefecto;
    }
    if (u->sacado) { MaterialRevivir(u->m); u->sacado = false; }
    else { MaterialPurgar(u->m); u->sacado = true; }
    W3dRecursosVistaInvalidar();
}
static void UndoMatSacadoDesvincular(void* d, Object* borrado) {
    UndoMatSacado* u = (UndoMatSacado*)d;
    for (size_t i = 0; i < u->mallas.size(); i++) if ((Object*)u->mallas[i] == borrado) u->mallas[i] = NULL;
}
static void UndoMatSacadoLiberar(void* d) { delete (UndoMatSacado*)d; }

class ProveedorMateriales : public W3dProveedorRecursos {
public:
    static void Item(const Material* m, int usuarios, W3dRecursoItem& it) {
        it.id = m->name;
        it.nombre = m->name;
        it.carpeta = m->carpeta;
        it.icono = (int)IconType::material;
        it.usuarios = usuarios;
        it.entrada.clear();
        // su textura base (la cargada, o la que espera en la cola / dormida)
        it.info = m->texture ? BaseDeRuta(m->texture->path) : BaseDeRuta(TexturaPendienteDe(m));
    }
    void Listar(std::vector<W3dRecursoItem>& out) {
        out.clear();
        std::vector<Material*> mats; std::map<const Material*, int> cuenta;
        MaterialesProyecto(mats, cuenta);
        for (size_t i = 0; i < mats.size(); i++) {
            W3dRecursoItem it;
            Item(mats[i], cuenta[mats[i]], it);
            out.push_back(it);
        }
    }
    bool Info(const std::string& id, W3dRecursoItem& out) {
        Material* m = MaterialDeNombre(id);
        if (!m) return false;
        std::map<const Material*, int> cuenta;
        UsuariosMateriales(cuenta);
        Item(m, cuenta[m], out);
        return true;
    }
    bool FijarCarpeta(const std::string& id, const std::string& carpeta) {
        Material* m = MaterialDeNombre(id);
        if (!m) return false;
        m->carpeta = carpeta;
        return true;
    }
    bool Renombrar(const std::string& id, const std::string& nuevo, std::string* final) {
        Material* m = MaterialDeNombre(id);
        if (!m) return false;
        const std::string n = MaterialNombreLibre(W3dNombreNormalizar(nuevo, "Material"), m);
        if (final) *final = n;
        if (n == m->name) return true;
        int im = -1;
        for (size_t k = 0; k < Materials.size(); k++) if (Materials[k] == m) { im = (int)k; break; }
        // el MISMO paso de undo que "Rename Material" de la tarjeta (indice en la lista global)
        if (im >= 0) UndoCapturarRename(W3dDestGlobal(W3dRenameDest::MaterialG, im));
        m->name = n;
        return true;
    }
    bool Borrar(const std::string& id, std::string* motivo) {
        Material* m = MaterialDeNombre(id);
        if (!m) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        std::map<const Material*, int> cuenta;
        UsuariosMateriales(cuenta);
        if (cuenta[m] > 0) { if (motivo) *motivo = "It is in use"; return false; }
        // los materiales ANIMADOS lo modifican: no es un huerfano
        for (size_t i = 0; i < AnimatedMaterials.size(); i++) {
            const AnimatedMaterial* am = AnimatedMaterials[i];
            if (!am) continue;
            for (size_t t = 0; t < am->targets.size(); t++)
                if (am->targets[t] == m) { if (motivo) *motivo = "An animated material uses it"; return false; }
        }
        int im = -1;
        for (size_t k = 0; k < Materials.size(); k++) if (Materials[k] == m) { im = (int)k; break; }
        if (!MaterialPurgar(m)) { if (motivo) *motivo = "It can't be deleted"; return false; }
        // la lista global se CORRIO: los renames de material del undo van por indice
        if (im >= 0) UndoListaBorrada(W3dDestGlobal(W3dRenameDest::MaterialG, im));
        return true;
    }
    void Usuarios(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        Material* mat = MaterialDeNombre(id);
        if (!mat) return;
        std::vector<Object*> objs;
        ObjetosEscena(objs);
        for (size_t i = 0; i < objs.size(); i++) {
            if (objs[i]->getType() != ObjectType::mesh) continue;
            const Mesh* m = (const Mesh*)objs[i];
            for (size_t g = 0; g < m->materialsGroup.size(); g++)
                if (m->materialsGroup[g].material == mat) { out.push_back(objs[i]); break; }
        }
    }
    bool Duplicar(const std::string& id, std::string* idNuevo, std::string* motivo) {
        Material* m = MaterialDeNombre(id);
        if (!m) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        Material* n = MaterialNuevoConUndo(m->name, m);
        if (idNuevo) *idNuevo = n->name;
        return true;
    }
    // EN USO: las partes que lo usan pasan al material POR DEFECTO (con Ctrl+Z) y el material sale de la lista
    bool SabeBorrarEnUso() const { return true; }
    bool BorrarEnUso(const std::string& id, std::string* motivo) {
        Material* mat = MaterialDeNombre(id);
        if (!mat) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        if (!MaterialDefecto) MaterialDefecto = new Material("Default Material", true);
        UndoMatSacado* u = new UndoMatSacado();
        u->m = mat; u->sacado = false;
        std::vector<Object*> objs;
        ObjetosEscena(objs);
        for (size_t i = 0; i < objs.size(); i++) {
            if (objs[i]->getType() != ObjectType::mesh) continue;
            Mesh* m = (Mesh*)objs[i];
            for (size_t g = 0; g < m->materialsGroup.size(); g++)
                if (m->materialsGroup[g].material == mat) { u->mallas.push_back(m); u->partes.push_back((int)g); }
        }
        UndoMatSacadoAplicar(u);   // (deja las partes en el por defecto y purga el material)
        UndoExterno f; f.aplicar = UndoMatSacadoAplicar; f.desvincular = UndoMatSacadoDesvincular; f.liberar = UndoMatSacadoLiberar;
        UndoPushExterno(f, u);
        return true;
    }
    void Datos(const std::string& id, std::vector<W3dRecursoDato>& out) {
        out.clear();
        Material* m = MaterialDeNombre(id);
        if (!m) return;
        W3dRecursoItem it;
        Info(id, it);
        const std::string tex = m->texture ? BaseDeRuta(m->texture->path) : BaseDeRuta(TexturaPendienteDe(m));
        out.push_back(W3dRecursoDato(T("Texture"), tex.empty() ? std::string(T("(none)")) : tex));
        if (m->normalTexture) out.push_back(W3dRecursoDato(T("Normal map"), BaseDeRuta(m->normalTexture->path)));
        if (!m->capas.empty()) out.push_back(W3dRecursoDato(T("Texture layers"), EnteroTexto((long)m->capas.size())));
        out.push_back(W3dRecursoDato(T("Transparent"), SiNo(m->transparent)));
        out.push_back(W3dRecursoDato(T("Users"), EnteroTexto(it.usuarios)));
    }
};

// ============================================================================
//  PROVEEDOR: TEXTURAS (las cargadas + las entradas texturas/ del contenedor)
// ============================================================================
// LA CLAVE de una textura = su ruta NORMALIZADA, igual que la del cache (TexturaCache.cpp):
// separador '/', sin "./" ni "dir/../". Una entrada "texturas/x.png" es su propia clave.
// la ruta YA esta en esa forma (el caso de casi todas: "texturas/piso.png"): la biblioteca normaliza miles de
// rutas por listado y el camino largo parte cada una en tramos
static bool ClaveYaNormal(const std::string& r) {
    if (r.empty()) return true;
    for (size_t i = 0; i < r.size(); i++) {
        const char c = r[i];
        if (c == '\\') return false;
        if (c == '/' && i + 1 < r.size() && r[i + 1] == '/') return false;   // "//"
        if (c == '.' && (i == 0 || r[i - 1] == '/')) {
            // un tramo que empieza con '.': "." y ".." se resuelven; ".oculto" es un nombre
            const size_t fin = r.find('/', i);
            const size_t n = (fin == std::string::npos ? r.size() : fin) - i;
            if (n == 1 || (n == 2 && r[i + 1] == '.')) return false;
        }
    }
    return r[r.size() - 1] != '/';
}
static std::string ClaveTextura(std::string r) {
    if (ClaveYaNormal(r)) return r;
    for (size_t i = 0; i < r.size(); i++) if (r[i] == '\\') r[i] = '/';
    std::vector<std::string> tramos;
    std::string t;
    const bool abs = !r.empty() && r[0] == '/';
    for (size_t i = 0; i <= r.size(); i++) {
        const char c = (i < r.size()) ? r[i] : '/';
        if (c != '/') { t += c; continue; }
        if (t.empty() || t == ".") { t.clear(); continue; }
        if (t == ".." && !tramos.empty() && tramos.back() != "..") tramos.pop_back();
        else tramos.push_back(t);
        t.clear();
    }
    std::string out = abs ? "/" : "";
    for (size_t i = 0; i < tramos.size(); i++) { if (i) out += '/'; out += tramos[i]; }
    return out;
}

// carpeta cosmetica de cada textura (por clave) y las entradas purgadas en la sesion
static std::map<std::string, std::string> gCarpetaTex;
static std::set<std::string> gTexPurgadas;
// ANTES de la ingesta del guardado: (textura, su clave) de las que tienen carpeta
static std::vector<std::pair<Texture*, std::string> > gTexAntes;

// LO DE LA BIBLIOTECA QUE VIVE AFUERA (a proposito: "ext:"). La MARCA de externa (W3dContenedor.h) guarda la
// ruta tal cual la resolvio quien la cargo (al abrir, "ext:piso.png" queda "./piso.png") y la biblioteca
// nombra todo NORMALIZADO ("piso.png"): estas miran las marcas por clave.
static bool ExternaMarcada(const std::string& clave) {
    std::vector<std::string> ext; W3dRefExternasListar(&ext);
    for (size_t i = 0; i < ext.size(); i++) if (ClaveTextura(ext[i]) == clave) return true;
    return false;
}
// la ruta MARCADA de esa clave, tal cual ("" = no esta marcada)
static std::string ExternaRutaMarcada(const std::string& clave) {
    std::vector<std::string> ext; W3dRefExternasListar(&ext);
    for (size_t i = 0; i < ext.size(); i++) if (ClaveTextura(ext[i]) == clave) return ext[i];
    return std::string();
}
// suelta TODAS las marcas de esa clave; devuelve las que solto (para volver a ponerlas si algo falla)
static std::vector<std::string> DesmarcarExterna(const std::string& clave) {
    std::vector<std::string> ext, sueltas; W3dRefExternasListar(&ext);
    for (size_t i = 0; i < ext.size(); i++)
        if (ClaveTextura(ext[i]) == clave) { W3dRefExternaDesmarcar(ext[i]); sueltas.push_back(ext[i]); }
    return sueltas;
}
// las TEXTURAS de afuera que son de la biblioteca: las que se pasaron afuera desde ella y las que el proyecto
// guardo como suyas ("externos"). Una imagen externa que solo nombra un HUD (.w3dui) no es de aca: por eso
// no alcanza con las marcas. Si nadie la usa sigue siendo del proyecto (un huerfano: se conserva).
static std::set<std::string> gTexExternasBib;

// LOS SCRIPTS: una textura que solo nombra un .lua (setTextura(obj, "texturas/hud.png")) NO
// es huerfana. El texto de los scripts se lee una vez y se re-escanea al invalidar (abrir,
// guardar, purgar), no por cuadro.
static bool gScriptsLeidos = false;
static std::vector<std::string> gScriptsTexto;
// cuantos scripts nombran cada nombre de archivo (ScriptsQueNombran), recordado mientras el texto de los
// scripts sea el mismo: buscar cada textura y cada sonido en TODO el texto de los scripts en cada listado
// costaba decenas de ms con un proyecto grande (y la biblioteca se lista en cada cuadro)
static std::map<std::string, int> gScriptsNombran;
static std::vector<std::string> gEntradasTex;   // las entradas texturas/ del contenedor montado
static bool gEntradasLeidas = false;

static void JuntarRutasScripts(Object* o, std::set<std::string>& out) {
    if (!o) return;
    if (o->scriptDatos)
        for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++)
            if (!o->scriptDatos->scripts[i].ruta.empty()) out.insert(o->scriptDatos->scripts[i].ruta);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarRutasScripts(o->Childrens[i], out);
}
static void AsegurarScripts() {
    if (gScriptsLeidos) return;
    gScriptsLeidos = true;
    gScriptsTexto.clear();
    gScriptsNombran.clear();   // (otro texto: lo recordado ya no vale)
    std::set<std::string> rutas;
    std::vector<std::string> ents;
    W3dContenedorListarCarpeta("scripts/", ents);
    for (size_t i = 0; i < ents.size(); i++) rutas.insert(ents[i]);
    { std::vector<Object*> raices; W3dRaicesVivas(raices);
      for (size_t i = 0; i < raices.size(); i++) JuntarRutasScripts(raices[i], rutas); }
    for (std::set<std::string>::iterator it = rutas.begin(); it != rutas.end(); ++it) {
        const std::string& r = *it;
        if (r.size() < 4 || r.compare(r.size() - 4, 4, ".lua") != 0) continue;
        std::vector<unsigned char> d;
        if (!w3dFileSystem::ReadFileBytes(r, d) || d.empty()) continue;
        gScriptsTexto.push_back(std::string((const char*)&d[0], d.size()));
    }
}
static void AsegurarEntradasTex() {
    if (gEntradasLeidas) return;
    gEntradasLeidas = true;
    W3dContenedorListarCarpeta("texturas/", gEntradasTex);
}
// un caracter que puede ser parte de un nombre de archivo (el nombre no termina ni empieza ahi)
static bool CaracterDeNombre(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || c == '.' || (unsigned char)c >= 0x80;
}
// el texto nombra el archivo 'base' ENTERO: "otrodosmats.png" o "dosmats.png.bak" no nombran
// "dosmats.png" (la ruta de adelante, "texturas/dosmats.png", si)
static bool NombraArchivo(const std::string& texto, const std::string& base) {
    for (size_t p = texto.find(base); p != std::string::npos; p = texto.find(base, p + 1)) {
        const size_t fin = p + base.size();
        if ((p == 0 || !CaracterDeNombre(texto[p - 1])) && (fin >= texto.size() || !CaracterDeNombre(texto[fin])))
            return true;
    }
    return false;
}
// cuantos scripts nombran la textura (por su nombre de archivo: el script puede usar la
// entrada entera o solo el nombre)
static int ScriptsQueNombran(const std::string& clave) {
    AsegurarScripts();
    const std::string base = BaseDeRuta(clave);
    if (base.empty()) return 0;
    std::map<std::string, int>::iterator it = gScriptsNombran.find(base);
    if (it != gScriptsNombran.end()) return it->second;
    int n = 0;
    for (size_t i = 0; i < gScriptsTexto.size(); i++)
        if (NombraArchivo(gScriptsTexto[i], base)) n++;
    gScriptsNombran[base] = n;
    return n;
}

// las rutas DORMIDAS de un material (import_obj.h: las de un huerfano recien abierto, que no se
// cargan hasta que una malla lo use): su normal map y sus capas. La base ya la da TexturaPendienteDe.
// Escribibles: el rename de una textura las sigue.
static void RutasDormidas(const Material* m, std::vector<std::string*>& out) {
    out.clear();
    TexDormida* d = TexturasDormidasDe(m);
    if (!d) return;
    if (!d->normal.empty()) out.push_back(&d->normal);
    for (size_t c = 0; c < d->capas.size(); c++) if (!d->capas[c].textura.empty()) out.push_back(&d->capas[c].textura);
}

// UNA pasada que junta: las texturas cargadas del proyecto y sus usuarios (por clave). Los
// usuarios son materiales (textura base -tambien la que espera en la cola-, normal map, capas,
// cuadros de material animado, y las rutas DORMIDAS de un huerfano), particulas, elementos 2D y la
// fuente bitmap de un texto 2D (por ruta), y los scripts.
struct TexUso {
    int usuarios;
    std::vector<Object*> objetos;   // solo si se pidieron
    TexUso() : usuarios(0) {}
};
// la FUENTE BITMAP de un texto 2D es un PAR: el png del atlas y su .json de glifos. El lector
// (Fuente2D.cpp) acepta la ruta de cualquiera de los dos y deriva el otro por el NOMBRE (misma
// ruta, otra extension), asi que el que el texto no nombra TAMBIEN esta en uso: si no, "Purge
// Orphans" lo borraba y la fuente dejaba de cargar. Devuelve la clave del hermano solo si es una
// entrada texturas/ del contenedor (el .json que el guardado clasifica en extra/ no es de esta
// vista); "" = no hay.
static std::string HermanoDeFuente(const std::string& clave) {
    const size_t pto = clave.find_last_of('.'), barra = clave.find_last_of('/');
    if (pto == std::string::npos || (barra != std::string::npos && pto < barra)) return std::string();
    std::string ext = clave.substr(pto);
    for (size_t i = 0; i < ext.size(); i++) if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = (char)(ext[i] + 32);
    std::string h;
    if (ext == ".png") h = clave.substr(0, pto) + ".json";
    else if (ext == ".json") h = clave.substr(0, pto) + ".png";
    else return std::string();
    AsegurarEntradasTex();
    for (size_t i = 0; i < gEntradasTex.size(); i++) if (ClaveTextura(gEntradasTex[i]) == h) return h;
    return std::string();
}
static void UsuariosTexturas(std::map<std::string, TexUso>& uso, bool conObjetos) {
    // materiales -> que claves usan
    std::map<const Material*, std::set<std::string> > porMat;
    for (size_t i = 0; i < Materials.size(); i++) {
        const Material* m = Materials[i];
        if (!m || !m->libreria.empty()) continue;   // (uno de una LIBRERIA: sus texturas son de ella)
        std::set<std::string>& s = porMat[m];
        if (m->texture && !m->texture->path.empty()) s.insert(ClaveTextura(m->texture->path));
        else { const std::string p = TexturaPendienteDe(m); if (!p.empty()) s.insert(ClaveTextura(p)); }
        if (m->normalTexture && !m->normalTexture->path.empty()) s.insert(ClaveTextura(m->normalTexture->path));
        for (size_t c = 0; c < m->capas.size(); c++)
            if (m->capas[c].tex && !m->capas[c].tex->path.empty()) s.insert(ClaveTextura(m->capas[c].tex->path));
        std::vector<std::string*> rd; RutasDormidas(m, rd);
        for (size_t k = 0; k < rd.size(); k++) s.insert(ClaveTextura(*rd[k]));
    }
    for (size_t i = 0; i < AnimatedMaterials.size(); i++) {
        const AnimatedMaterial* am = AnimatedMaterials[i];
        if (!am || am->targets.empty()) continue;
        std::set<std::string>& s = porMat[am->targets[0]];
        for (size_t f = 0; f < am->frameTextures.size(); f++)
            if (am->frameTextures[f] && !am->frameTextures[f]->path.empty()) s.insert(ClaveTextura(am->frameTextures[f]->path));
    }
    for (std::map<const Material*, std::set<std::string> >::iterator it = porMat.begin(); it != porMat.end(); ++it)
        for (std::set<std::string>::iterator k = it->second.begin(); k != it->second.end(); ++k) uso[*k].usuarios++;
    // los FLIPBOOKS con nombre (assets de la escena): su atlas va tal cual al guardado, asi que purgarlo
    // dejaria al proyecto nombrando una entrada que no esta (y el guardado se frenaria)
    for (size_t i = 0; i < SceneFlipbooks.size(); i++)
        if (SceneFlipbooks[i] && !SceneFlipbooks[i]->atlas.empty()) uso[ClaveTextura(SceneFlipbooks[i]->atlas)].usuarios++;
    // los PEDIDOS del STREAMING (io/Streaming.h): una instancia cargada -o cargandose- retiene la textura aunque su
    // material ya no la tenga en la ranura (el usuario le puso otra): es un usuario, no se borra debajo de ella
    { std::vector<std::string> ids;
      W3dStreamingRecursosRetenidos(W3DREC_TEXTURA, ids);
      for (size_t i = 0; i < ids.size(); i++) uso[ClaveTextura(ids[i])].usuarios++; }
    // objetos de la escena que las nombran por RUTA (o por sus materiales, para "Select Users")
    std::vector<Object*> objs;
    ObjetosEscena(objs);
    for (size_t i = 0; i < objs.size(); i++) {
        Object* o = objs[i];
        std::string r;
        switch (o->getType().v) {
            case ObjectType::particulas: r = ((Particulas*)o)->textura; break;
            case ObjectType::imagen2d:   r = ((Imagen2D*)o)->textura; break;
            case ObjectType::slice9:     r = ((Slice9*)o)->textura; break;
            case ObjectType::texto2d: {
                r = ((Texto2D*)o)->fuenteBitmap;   // el png del atlas de glifos (o su .json)
                // y el HERMANO del par (el .json del png, o el png del .json)
                const std::string h = r.empty() ? std::string() : HermanoDeFuente(ClaveTextura(r));
                if (!h.empty()) {
                    TexUso& uh = uso[h];
                    uh.usuarios++;
                    if (conObjetos) uh.objetos.push_back(o);
                }
                break;
            }
            case ObjectType::boton2d:
                r = ((Boton2D*)o)->texturaFondo;
                // su ICONO es otra imagen: cuenta aparte (el fondo va por el camino comun de abajo)
                if (!((Boton2D*)o)->icono.empty() && ClaveTextura(((Boton2D*)o)->icono) != ClaveTextura(r)) {
                    TexUso& ui = uso[ClaveTextura(((Boton2D*)o)->icono)];
                    ui.usuarios++;
                    if (conObjetos) ui.objetos.push_back(o);
                }
                break;
            case ObjectType::mesh:
                if (conObjetos) {
                    const Mesh* m = (const Mesh*)o;
                    std::set<std::string> ks;
                    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                        std::map<const Material*, std::set<std::string> >::iterator pm = porMat.find(m->materialsGroup[g].material);
                        if (pm != porMat.end()) ks.insert(pm->second.begin(), pm->second.end());
                    }
                    for (std::set<std::string>::iterator k = ks.begin(); k != ks.end(); ++k) uso[*k].objetos.push_back(o);
                }
                break;
            default: break;
        }
        if (r.empty()) continue;
        TexUso& u = uso[ClaveTextura(r)];
        u.usuarios++;
        if (conObjetos) u.objetos.push_back(o);
    }
}

static void SoltarVistaPreviaDe(const std::string& clave);   // (el recurso activo, mas abajo)
static void OlvidarFalloVistaPrevia();                       // idem

// ---------------------------------------------------------------------------
//  RENOMBRAR UNA TEXTURA = MUDAR SU ENTRADA DEL CONTENEDOR
//  El nombre de una textura ES su archivo: renombrarla es darle otra entrada ("texturas/piso.png"
//  -> "texturas/madera.png") y llevar ahi a todos los que la nombran. Los bytes van al overlay
//  del contenedor montado con el nombre nuevo (los de una importada en la sesion se MUDAN, sin
//  copia); la entrada vieja del zip queda como PURGADA (el guardado no la conserva). Solo se
//  renombran las texturas que ya son del proyecto (una entrada); las de disco, despues de guardar.
// ---------------------------------------------------------------------------
static bool TexturaRenombrable(const std::string& clave) {
    return W3dContenedorHayMontado() && W3dEsNombreDeEntrada(clave);
}
// la textura CARGADA con esa clave (NULL = no esta en memoria)
static Texture* TexturaCargadaDe(const std::string& clave) {
    for (size_t i = (size_t)TexturasBase(); i < Textures.size(); i++)
        if (Textures[i] && ClaveTextura(Textures[i]->path) == clave) return Textures[i];
    return NULL;
}
// una entrada que no se puede tomar: esta en el contenedor (y no se purgo), cargada, o en la cola
static bool EntradaTexturaOcupada(const std::string& e) {
    if (W3dContenedorExiste(e) && !gTexPurgadas.count(e)) return true;
    if (TexturaCargadaDe(e)) return true;
    for (size_t i = 0; i < Materials.size(); i++) {
        const std::string p = Materials[i] ? TexturaPendienteDe(Materials[i]) : std::string();
        if (!p.empty() && ClaveTextura(p) == e) return true;
        std::vector<std::string*> rd; RutasDormidas(Materials[i], rd);
        for (size_t k = 0; k < rd.size(); k++) if (ClaveTextura(*rd[k]) == e) return true;
    }
    return false;
}
// la entrada que PIDE 'nuevo' para la textura 'clave': misma carpeta del contenedor y misma
// extension (el contenido no cambia de formato), el nombre como slug de entrada
static std::string EntradaTexturaPedida(const std::string& clave, const std::string& nuevo) {
    const size_t barra = clave.find_last_of('/');
    const std::string dir = (barra == std::string::npos) ? std::string() : clave.substr(0, barra + 1);
    const std::string archivo = BaseDeRuta(clave);
    const size_t pto = archivo.find_last_of('.');
    const std::string ext = (pto == std::string::npos || pto == 0) ? std::string() : archivo.substr(pto);
    std::string base = BaseDeRuta(W3dNombreNormalizar(nuevo, archivo.c_str()));
    // "madera.png" pedido para un .png: la extension ya esta (sin distinguir mayusculas)
    if (!ext.empty() && base.size() > ext.size()) {
        std::string cola = base.substr(base.size() - ext.size());
        for (size_t i = 0; i < cola.size(); i++) if (cola[i] >= 'A' && cola[i] <= 'Z') cola[i] = (char)(cola[i] - 'A' + 'a');
        std::string e2 = ext;
        for (size_t i = 0; i < e2.size(); i++) if (e2[i] >= 'A' && e2[i] <= 'Z') e2[i] = (char)(e2[i] - 'A' + 'a');
        if (cola == e2) base = base.substr(0, base.size() - ext.size());
    }
    return dir + W3dSlugEntrada(base) + ext;
}
// la pedida hecha LIBRE ("madera-2.png", como las importadas)
static std::string EntradaTexturaLibre(const std::string& clave, const std::string& nuevo) {
    const std::string pedida = EntradaTexturaPedida(clave, nuevo);
    if (pedida == clave || !EntradaTexturaOcupada(pedida)) return pedida;
    const size_t pto = pedida.find_last_of('.'), barra = pedida.find_last_of('/');
    const bool conExt = (pto != std::string::npos && (barra == std::string::npos || pto > barra + 1));
    const std::string raiz = conExt ? pedida.substr(0, pto) : pedida;
    const std::string ext = conExt ? pedida.substr(pto) : std::string();
    for (int n = 2; n < 10000; n++) {
        char b[16]; sprintf(b, "-%d", n);
        const std::string c = raiz + b + ext;
        if (c == clave || !EntradaTexturaOcupada(c)) return c;
    }
    return clave;
}
// cambia la ruta en un string que nombra la textura (particula, imagen 2D, flipbook...)
static void RenombrarRuta(std::string& r, const std::string& viejo, const std::string& nuevo) {
    if (!r.empty() && ClaveTextura(r) == viejo) r = nuevo;
}
static void RenombrarRutasObjetos(Object* o, const std::string& viejo, const std::string& nuevo) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        switch (h->getType().v) {
            case ObjectType::particulas: RenombrarRuta(((Particulas*)h)->textura, viejo, nuevo); break;
            case ObjectType::imagen2d:   RenombrarRuta(((Imagen2D*)h)->textura, viejo, nuevo); break;
            case ObjectType::slice9:     RenombrarRuta(((Slice9*)h)->textura, viejo, nuevo); break;
            case ObjectType::texto2d:    RenombrarRuta(((Texto2D*)h)->fuenteBitmap, viejo, nuevo); break;
            case ObjectType::boton2d:
                RenombrarRuta(((Boton2D*)h)->texturaFondo, viejo, nuevo);
                RenombrarRuta(((Boton2D*)h)->icono, viejo, nuevo);
                break;
            default: break;
        }
        RenombrarRutasObjetos(h, viejo, nuevo);
    }
}
// algun Texto2D usa esa textura como FUENTE BITMAP (el png de su atlas de glifos)
static bool FuenteQueNombra(Object* o, const std::string& clave) {
    if (!o) return false;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::texto2d) {
            const std::string& f = ((Texto2D*)h)->fuenteBitmap;
            if (!f.empty() && ClaveTextura(f) == clave) return true;
        }
        if (FuenteQueNombra(h, clave)) return true;
    }
    return false;
}
// el .json de glifos de la fuente bitmap 'png' en el contenedor, buscado como lo busca el lector
// (Fuente2D): al lado del png ("texturas/x.json") o donde lo clasifica el guardado ("extra/x.json").
// "" = no hay (o ya no es del proyecto).
static std::string JsonDeFuente(const std::string& png) {
    const size_t pto = png.find_last_of('.'), barra = png.find_last_of('/');
    const bool conExt = (pto != std::string::npos && (barra == std::string::npos || pto > barra + 1));
    const std::string js = (conExt ? png.substr(0, pto) : png) + ".json";
    if (W3dContenedorExiste(js) && !gTexPurgadas.count(js)) return js;
    const std::string alt = "extra/" + BaseDeRuta(js);
    if (W3dContenedorExiste(alt) && !gTexPurgadas.count(alt)) return alt;
    return std::string();
}
// el .json de la fuente viaja con su png: mismo lugar donde estaba, con el nombre del png NUEVO.
// Devuelve la entrada vieja que dejo de ser del proyecto ("" = no habia json, o no se pudo mudar).
static std::string MudarJsonDeFuente(const std::string& pngViejo, const std::string& pngNuevo, std::string* jsonNuevo) {
    const std::string jv = JsonDeFuente(pngViejo);
    if (jv.empty()) return std::string();
    const size_t b = jv.find_last_of('/');
    std::string base = BaseDeRuta(pngNuevo);
    const size_t pto = base.find_last_of('.');
    if (pto != std::string::npos && pto > 0) base = base.substr(0, pto);
    const std::string jn = ((b == std::string::npos) ? std::string() : jv.substr(0, b + 1)) + base + ".json";
    if (jn == jv) return std::string();
    bool ok;
    if (W3dContenedorEntradaEditada(jv)) ok = W3dContenedorMudarEditada(jv, jn);
    else {
        std::vector<unsigned char> d;
        ok = w3dFileSystem::ReadFileBytes(jv, d) && !d.empty() && W3dContenedorEscribirEntrada(jn, &d[0], d.size());
    }
    if (!ok) {
        w3dLogfW("[outliner] no pude mudar el json de glifos %s -> %s: la fuente cae a la de siempre", jv.c_str(), jn.c_str());
        return std::string();
    }
    if (jsonNuevo) *jsonNuevo = jn;
    return jv;
}
// TODOS los que nombran la textura 'viejo' pasan a nombrar 'nuevo' (sin tocar bytes): la cargada 't' (misma
// ficha del cache, otra clave), la cola de carga diferida (y las rutas DORMIDAS de un material huerfano), los
// objetos 2D / particulas / la fuente bitmap de un texto 2D y los flipbooks; y su carpeta cosmetica la sigue.
// Lo usan el rename (una entrada que se muda) y el pase adentro/afuera del .w3d (entrada <-> archivo de disco).
static void CambiarRutaTextura(Texture* t, const std::string& viejo, const std::string& nuevo) {
    if (t) TexturaRenombrar(t, nuevo);
    for (size_t i = 0; i < Materials.size(); i++) {
        std::string* p = Materials[i] ? TexturaPendienteRefDe(Materials[i]) : NULL;
        if (p) RenombrarRuta(*p, viejo, nuevo);
        std::vector<std::string*> rd; RutasDormidas(Materials[i], rd);   // (normal map y capas dormidas)
        for (size_t k = 0; k < rd.size(); k++) RenombrarRuta(*rd[k], viejo, nuevo);
    }
    { std::vector<Object*> raices; W3dRaicesVivas(raices);   // (las escenas/prefabs que no se miran tambien)
      for (size_t i = 0; i < raices.size(); i++) RenombrarRutasObjetos(raices[i], viejo, nuevo); }
    for (size_t i = 0; i < SceneFlipbooks.size(); i++)
        if (SceneFlipbooks[i]) RenombrarRuta(SceneFlipbooks[i]->atlas, viejo, nuevo);
    std::map<std::string, std::string>::iterator c = gCarpetaTex.find(viejo);
    if (c != gCarpetaTex.end()) { const std::string car = c->second; gCarpetaTex.erase(c); gCarpetaTex[nuevo] = car; }
    else gCarpetaTex.erase(nuevo);
    OlvidarFalloVistaPrevia();
    W3dRecursosVistaInvalidar();   // el listado de entradas del contenedor cambio
}
// LA MUDANZA (exacta: 'nuevo' ya viene libre). La usan el rename y su undo (que muda de vuelta).
static bool MudarTextura(const std::string& viejo, const std::string& nuevo) {
    if (viejo == nuevo || !TexturaRenombrable(viejo) || !W3dEsNombreDeEntrada(nuevo)) return false;
    Texture* t = TexturaCargadaDe(viejo);
    // (el undo no puede volver a un nombre que mientras tanto tomo OTRA textura cargada)
    Texture* otra = TexturaCargadaDe(nuevo);
    if (otra && otra != t) return false;
    // 1) LOS BYTES: la editada en la sesion se muda; la del zip se copia al overlay. Una textura
    //    creada en memoria (pintura, "nueva textura") no tiene bytes todavia: el guardado los pide
    //    por su ruta (TexEditBytesParaGuardar), asi que alcanza con cambiarle la ruta.
    if (W3dContenedorEntradaEditada(viejo)) {
        if (!W3dContenedorMudarEditada(viejo, nuevo)) return false;
    } else {
        std::vector<unsigned char> d;
        if (w3dFileSystem::ReadFileBytes(viejo, d) && !d.empty()) {
            if (!W3dContenedorEscribirEntrada(nuevo, &d[0], d.size())) return false;
        } else if (!t) return false;
    }
    // 1b) una FUENTE BITMAP (la nombra un Texto2D): su .json de glifos va con ella. El lector lo
    //     deriva del nombre del png; si se quedara con el nombre viejo, la fuente dejaria de cargar
    std::string jsonViejo, jsonNuevo;
    if (EnAlgunaRaiz(FuenteQueNombra, viejo)) jsonViejo = MudarJsonDeFuente(viejo, nuevo, &jsonNuevo);
    // 2) la entrada vieja deja de ser del proyecto (el guardado no la conserva); la nueva si
    gTexPurgadas.insert(viejo);
    gTexPurgadas.erase(nuevo);
    if (!jsonViejo.empty()) { gTexPurgadas.insert(jsonViejo); gTexPurgadas.erase(jsonNuevo); }
    // 3) TODOS los que la nombran y su carpeta
    CambiarRutaTextura(t, viejo, nuevo);
    w3dLogf("[outliner] textura renombrada: %s -> %s", viejo.c_str(), nuevo.c_str());
    return true;
}
// el paso de undo: el toggle de siempre entre los dos nombres (muda de vuelta)
struct UndoRenTextura { std::string actual, otro; };
static void UndoRenTexturaAplicar(void* d) {
    UndoRenTextura* u = (UndoRenTextura*)d;
    if (!MudarTextura(u->actual, u->otro)) return;
    W3dRecursoActivoRenombrado(W3D_VISTA_TEXTURAS, u->actual, u->otro);
    std::swap(u->actual, u->otro);
}
static void UndoRenTexturaLiberar(void* d) { delete (UndoRenTextura*)d; }

// ---------------------------------------------------------------------------
//  PURGAR UNA TEXTURA CARGADA (en caliente): sale de la GPU y de las listas; el objeto va al
//  cementerio del cache (TexturaPurgar) hasta cerrar el proyecto, asi ningun puntero viejo queda
//  colgado. La pintura que tenia sin guardar se descarta (era un huerfano: se confirmo).
// ---------------------------------------------------------------------------
static bool PurgarTexturaCargada(Texture* t) {
    if (!t || !TexturaPurgar(t)) return false;
    TexEditBuscar(t);             // la textura ya no esta viva: la pintura suelta sus pixeles
    return true;
}

// ============================================================================
//  LOS ARCHIVOS (sonidos, scripts, fuentes, videos): sus carpetas cosmeticas, lo purgado en la
//  sesion y a quien nombran. Su id es su RUTA normalizada (la entrada "sonidos/paso.wav", o la
//  ruta de disco de uno que vive afuera del .w3d), como el de las texturas.
// ============================================================================
static std::map<std::string, std::string> gCarpetaArch;   // clave -> carpeta cosmetica
static std::set<std::string> gArchPurgadas;               // entradas purgadas / mudadas / pasadas afuera
static std::map<int, std::vector<std::string> > gEntradasArch;   // tipo -> entradas de su carpeta del contenedor
static bool gEntradasArchLeidas = false;

// la carpeta del contenedor de cada tipo de archivo
static const char* CarpetaDeTipo(int tipo) {
    switch (tipo) {
        case W3D_VISTA_SONIDOS: return "sonidos/";
        case W3D_VISTA_SCRIPTS: return "scripts/";
        case W3D_VISTA_FUENTES: return "fuentes/";
        case W3D_VISTA_VIDEOS:  return "videos/";
        case W3D_VISTA_TEXTURAS: return "texturas/";
    }
    return "";
}
static void AsegurarEntradasArch() {
    if (gEntradasArchLeidas) return;
    gEntradasArchLeidas = true;
    gEntradasArch.clear();
    const int tipos[4] = { W3D_VISTA_SONIDOS, W3D_VISTA_SCRIPTS, W3D_VISTA_FUENTES, W3D_VISTA_VIDEOS };
    for (int k = 0; k < 4; k++) W3dContenedorListarCarpeta(CarpetaDeTipo(tipos[k]), gEntradasArch[tipos[k]]);
}
// el TIPO de archivo por su extension (lo que el contenedor clasificaria): -1 = no es de estos
static int TipoArchivoDeRuta(const std::string& r) {
    const std::string c = W3dCategoriaPorExtension(r);
    if (c == "sonidos") return W3D_VISTA_SONIDOS;
    if (c == "scripts") return W3D_VISTA_SCRIPTS;
    if (c == "fuentes") return W3D_VISTA_FUENTES;
    if (c == "videos")  return W3D_VISTA_VIDEOS;
    return -1;
}

// las REFERENCIAS de los objetos a un archivo de ese tipo (una por objeto): los scripts de cada
// objeto, la fuente de un texto 2D, el archivo de un video 2D. 'fn' recibe el string que la guarda.
struct RefArchivo { Object* obj; std::string* ruta; };
static void JuntarRefsArchivo(Object* o, int tipo, std::vector<RefArchivo>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (tipo == W3D_VISTA_SCRIPTS && h->scriptDatos)
            for (size_t k = 0; k < h->scriptDatos->scripts.size(); k++) {
                RefArchivo r; r.obj = h; r.ruta = &h->scriptDatos->scripts[k].ruta;
                if (!r.ruta->empty()) out.push_back(r);
            }
        if (tipo == W3D_VISTA_FUENTES && h->getType() == ObjectType::texto2d && !((Texto2D*)h)->fuente.empty()) {
            RefArchivo r; r.obj = h; r.ruta = &((Texto2D*)h)->fuente; out.push_back(r);
        }
        if (tipo == W3D_VISTA_VIDEOS && h->getType() == ObjectType::video2d && !((Video2D*)h)->video.empty()) {
            RefArchivo r; r.obj = h; r.ruta = &((Video2D*)h)->video; out.push_back(r);
        }
        JuntarRefsArchivo(h, tipo, out);
    }
}
static void RefsArchivo(int tipo, std::vector<RefArchivo>& out) {
    out.clear();
    std::vector<Object*> raices; W3dRaicesVivas(raices);
    for (size_t i = 0; i < raices.size(); i++) JuntarRefsArchivo(raices[i], tipo, out);
}

// ============================================================================
//  DENTRO / FUERA DEL .w3d (texturas y archivos). La ruta de un recurso que vive AFUERA es su ruta
//  de disco (en el archivo, "ext:" relativa al .w3d); la de uno de ADENTRO, su entrada. Pasar de uno a
//  otro cambia la ruta de TODOS los que lo nombran (como un rename), y el paso de undo vuelve.
// ============================================================================
// todos los que nombran 'viejo' (del tipo) pasan a nombrar 'nuevo'; la carpeta cosmetica lo sigue
static void CambiarRutaArchivo(int tipo, const std::string& viejo, const std::string& nuevo) {
    if (tipo == W3D_VISTA_TEXTURAS) { CambiarRutaTextura(TexturaCargadaDe(viejo), viejo, nuevo); return; }
    std::vector<RefArchivo> refs;
    RefsArchivo(tipo, refs);
    for (size_t i = 0; i < refs.size(); i++) if (ClaveTextura(*refs[i].ruta) == viejo) *refs[i].ruta = nuevo;
    std::map<std::string, std::string>::iterator c = gCarpetaArch.find(viejo);
    if (c != gCarpetaArch.end()) { const std::string car = c->second; gCarpetaArch.erase(c); gCarpetaArch[nuevo] = car; }
    else gCarpetaArch.erase(nuevo);
    W3dRecursosVistaInvalidar();
}
static std::set<std::string>& PurgadasDe(int tipo) { return tipo == W3D_VISTA_TEXTURAS ? gTexPurgadas : gArchPurgadas; }

// ---------------------------------------------------------------------------
//  LA ENTRADA QUE UN PASO DE UNDO PUEDE DEVOLVER. Pasar un archivo AFUERA y borrar una textura o un archivo
//  EN USO dejan su entrada como PURGADA: el proximo guardado no la escribe. El paso de undo solo recuerda
//  RUTAS, asi que guardar y despues deshacer dejaba a los usuarios nombrando una entrada que ya no estaba en
//  el .w3d (el guardado siguiente la escribia "ext:" como faltante, o el recurso desaparecia; el borrado en
//  uso perdia la UNICA copia). Cada uno de esos pasos RETIENE su entrada: el guardado que la descarta copia
//  antes sus bytes (el contenedor viejo todavia esta montado) y el paso, al volver a nombrarla, la repone en
//  el overlay del contenedor (el proximo guardado la vuelve a escribir). Sin guardado en el medio no se copia
//  nada: la entrada sigue en el contenedor montado.
// ---------------------------------------------------------------------------
struct EntradaRetenida {
    std::string entrada;
    std::vector<unsigned char> bytes;   // los que tenia en el contenedor que el guardado reemplazo (vacio = no hizo falta)
};
static std::set<EntradaRetenida*> gRetenidas;
// NULL = no es una entrada del contenedor (un archivo de disco: el guardado no lo toca)
static EntradaRetenida* RetenerEntrada(const std::string& e) {
    if (!W3dEsNombreDeEntrada(e)) return NULL;
    EntradaRetenida* r = new EntradaRetenida();
    r->entrada = e;
    gRetenidas.insert(r);
    return r;
}
static void SoltarEntrada(EntradaRetenida* r) {
    if (!r) return;
    gRetenidas.erase(r);
    delete r;
}
// el paso vuelve a NOMBRAR la entrada: si un guardado la saco del .w3d, vuelve al overlay con sus bytes
static void ReponerEntrada(EntradaRetenida* r) {
    if (!r || r->bytes.empty() || W3dContenedorExiste(r->entrada)) return;
    if (!W3dContenedorEscribirEntrada(r->entrada, &r->bytes[0], r->bytes.size())) {
        w3dLogfW("[biblioteca] no pude reponer la entrada %s: el recurso queda sin su archivo", r->entrada.c_str());
        return;
    }
    w3dLogf("[biblioteca] entrada repuesta por el undo: %s (%u bytes)", r->entrada.c_str(), (unsigned)r->bytes.size());
    std::vector<unsigned char>().swap(r->bytes);   // (ya esta en el overlay: otro guardado la vuelve a retener)
    W3dRecursosVistaInvalidar();                   // el listado del contenedor cambio
}

// el PASO de dentro/fuera: 'de' es la ruta de ahora, 'a' la otra; 'aFuera' = 'a' es la de disco. 'ret' = su
// entrada (la de adentro), retenida por si un guardado la saca del .w3d
struct UndoUbicacion { int tipo; std::string de, a; bool aFuera; EntradaRetenida* ret; };
static void MudarUbicacion(int tipo, const std::string& de, const std::string& a, bool aFuera) {
    CambiarRutaArchivo(tipo, de, a);
    std::set<std::string>& pur = PurgadasDe(tipo);
    if (aFuera) {
        W3dRefExternaMarcar(a);
        if (tipo == W3D_VISTA_TEXTURAS) gTexExternasBib.insert(a);   // (aunque nadie la use sigue en la vista)
        pur.insert(de);               // la entrada ya no es del proyecto (el guardado no la conserva)
    } else {
        DesmarcarExterna(de);
        gTexExternasBib.erase(de);
        pur.erase(a);
    }
}
static void UndoUbicacionAplicar(void* d) {
    // de la ruta de ahora ('de') a la otra ('a'); despues el paso queda listo para el camino inverso
    UndoUbicacion* u = (UndoUbicacion*)d;
    if (!u->aFuera) ReponerEntrada(u->ret);   // (vuelve ADENTRO: su entrada tiene que estar en el contenedor)
    MudarUbicacion(u->tipo, u->de, u->a, u->aFuera);
    W3dRecursoActivoRenombrado(u->tipo, u->de, u->a);
    std::swap(u->de, u->a);
    u->aFuera = !u->aFuera;
}
static void UndoUbicacionLiberar(void* d) {
    UndoUbicacion* u = (UndoUbicacion*)d;
    SoltarEntrada(u->ret);
    delete u;
}

static int UbicacionDeRuta(const std::string& id) {
    if (id.empty()) return -1;
    return W3dEsNombreDeEntrada(id) ? 0 : 1;
}
static std::string RutaExternaDe(const std::string& id) {
    if (g_w3dDirProyecto.empty()) return std::string();
    std::string d = g_w3dDirProyecto;
    if (!d.empty() && d[d.size() - 1] != '/' && d[d.size() - 1] != '\\') d += '/';
    return d + BaseDeRuta(id);
}
static bool FijarUbicacionArchivo(int tipo, const std::string& id, int ubic, const std::string& rutaExt,
                                  std::string* idNuevo, std::string* motivo) {
    if (idNuevo) *idNuevo = id;
    const int ahora = UbicacionDeRuta(id);
    if (ahora < 0) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
    if (ahora == ubic) return true;
    if (!W3dContenedorHayMontado()) { if (motivo) *motivo = "Save the project first"; return false; }
    std::vector<unsigned char> datos;
    if (!BytesArchivo(id, datos)) { if (motivo) *motivo = "Its file couldn't be read"; return false; }
    std::string nueva;
    if (ubic == 1) {
        // AFUERA: el archivo se escribe al lado (o donde se eligio). Si ya hay OTRO archivo con ese
        // nombre no se pisa (podria ser de otro proyecto)
        const std::string ruta = rutaExt.empty() ? RutaExternaDe(id) : rutaExt;
        if (ruta.empty()) { if (motivo) *motivo = "Save the project first"; return false; }
        std::vector<unsigned char> hay;
        if (w3dFileSystem::FileExists(ruta) && (!w3dFileSystem::ReadFileBytes(ruta, hay) || hay != datos)) {
            if (motivo) *motivo = "A different file already exists there";
            return false;
        }
        if (hay.empty()) {
            FILE* f = fopen(ruta.c_str(), "wb");
            if (!f) { if (motivo) *motivo = "The file couldn't be written"; return false; }
            const size_t w = fwrite(&datos[0], 1, datos.size(), f);
            fclose(f);
            if (w != datos.size()) { if (motivo) *motivo = "The file couldn't be written"; return false; }
        }
        nueva = ClaveTextura(ruta);
    } else {
        // ADENTRO: se ingiere al contenedor (dedup por contenido: vuelve a la entrada que tenia si
        // es la misma)
        const std::vector<std::string> sueltas = DesmarcarExterna(id);
        nueva = ClaveTextura(W3dImportarAsset(id));
        if (nueva == id || !W3dEsNombreDeEntrada(nueva)) {
            for (size_t i = 0; i < sueltas.size(); i++) W3dRefExternaMarcar(sueltas[i]);
            if (motivo) *motivo = "It couldn't be copied into the .w3d";
            return false;
        }
    }
    MudarUbicacion(tipo, id, nueva, ubic == 1);
    UndoUbicacion* u = new UndoUbicacion();
    u->tipo = tipo; u->de = nueva; u->a = id; u->aFuera = (ubic != 1);
    u->ret = RetenerEntrada(ubic == 1 ? id : nueva);   // la de ADENTRO
    UndoExterno f; f.aplicar = UndoUbicacionAplicar; f.liberar = UndoUbicacionLiberar;
    UndoPushExterno(f, u);
    W3dRecursoActivoRenombrado(tipo, id, nueva);
    if (idNuevo) *idNuevo = nueva;
    w3dLogf("[biblioteca] %s -> %s (%s)", id.c_str(), nueva.c_str(), ubic == 1 ? "afuera" : "adentro");
    return true;
}
// las filas de "adentro / afuera" y "falta el archivo" de un archivo del proyecto
static void DatosUbicacion(const std::string& id, std::vector<W3dRecursoDato>& out) {
    const int u = UbicacionDeRuta(id);
    if (u == 0) {
        out.push_back(W3dRecursoDato(T("Stored"), T("Inside the .w3d")));
        const long tam = TamArchivo(id);
        if (tam >= 0) out.push_back(W3dRecursoDato(T("In the .w3d"), W3dBytesTexto((double)tam)));
    } else if (u == 1) {
        out.push_back(W3dRecursoDato(T("Stored"), T("External")));
        out.push_back(W3dRecursoDato(T("Path"), id));
        const long tam = TamArchivo(id);
        if (tam < 0) out.push_back(W3dRecursoDato(T("Warning"), T("The external file is missing")));
        else out.push_back(W3dRecursoDato(T("File size"), W3dBytesTexto((double)tam)));
    }
}

// el paso de undo del borrado de una textura EN USO: los materiales y los objetos que la nombraban la
// recuperan (la textura vuelve de su cementerio: W3dRecursosRevisarPurgados)
struct TexSacadaMat { Material* m; int donde; Texture* t; };   // donde: -1 base, -2 normal map, >=0 capa
struct TexSacadaObj { Object* o; int campo; std::string ruta; };   // campo: 0 textura/fondo, 1 icono de un boton
struct UndoTexSacada {
    std::string clave;
    std::vector<TexSacadaMat> mats;
    std::vector<TexSacadaObj> objs;
    bool sacada;
    EntradaRetenida* ret;   // su entrada (si es de adentro), por si un guardado la saca del .w3d
};
static std::string* CampoTexObj(Object* o, int campo) {
    switch (o->getType().v) {
        case ObjectType::particulas: return &((Particulas*)o)->textura;
        case ObjectType::imagen2d:   return &((Imagen2D*)o)->textura;
        case ObjectType::slice9:     return &((Slice9*)o)->textura;
        case ObjectType::texto2d:    return &((Texto2D*)o)->fuenteBitmap;
        case ObjectType::boton2d:    return campo == 1 ? &((Boton2D*)o)->icono : &((Boton2D*)o)->texturaFondo;
        default: return NULL;
    }
}
static void UndoTexSacadaAplicar(void* d) {
    UndoTexSacada* u = (UndoTexSacada*)d;
    const bool poner = u->sacada;   // deshacer = ponerla de vuelta
    // (su archivo primero: la textura del cementerio se recarga de su entrada)
    if (poner) ReponerEntrada(u->ret);
    for (size_t i = 0; i < u->mats.size(); i++) {
        Material* m = u->mats[i].m;
        if (!m) continue;
        Texture* t = poner ? u->mats[i].t : NULL;
        if (u->mats[i].donde == -1) m->texture = t;
        else if (u->mats[i].donde == -2) m->normalTexture = t;
        else if (u->mats[i].donde < (int)m->capas.size()) m->capas[(size_t)u->mats[i].donde].tex = t;
    }
    for (size_t i = 0; i < u->objs.size(); i++) {
        if (!u->objs[i].o) continue;
        std::string* r = CampoTexObj(u->objs[i].o, u->objs[i].campo);
        if (r) *r = poner ? u->objs[i].ruta : std::string();
    }
    if (poner) { gTexPurgadas.erase(u->clave); W3dRecursosRevisarPurgados(); }
    else {
        gTexPurgadas.insert(u->clave);
        for (Texture* t = TexturaCargadaDe(u->clave); t; t = TexturaCargadaDe(u->clave)) if (!PurgarTexturaCargada(t)) break;
    }
    u->sacada = !u->sacada;
    W3dRecursosVistaInvalidar();
}
static void UndoTexSacadaDesvincular(void* d, Object* borrado) {
    UndoTexSacada* u = (UndoTexSacada*)d;
    for (size_t i = 0; i < u->objs.size(); i++) if (u->objs[i].o == borrado) u->objs[i].o = NULL;
}
static void UndoTexSacadaLiberar(void* d) {
    UndoTexSacada* u = (UndoTexSacada*)d;
    SoltarEntrada(u->ret);
    delete u;
}
static void JuntarObjsTex(Object* o, const std::string& clave, std::vector<TexSacadaObj>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        for (int campo = 0; campo < 2; campo++) {
            std::string* r = CampoTexObj(h, campo);
            if (campo == 1 && h->getType() != ObjectType::boton2d) r = NULL;
            if (r && !r->empty() && ClaveTextura(*r) == clave) { TexSacadaObj x; x.o = h; x.campo = campo; x.ruta = *r; out.push_back(x); }
        }
        JuntarObjsTex(h, clave, out);
    }
}

// ============================================================================
//  PROVEEDOR: ARCHIVOS (sonidos, scripts, fuentes, videos)
// ============================================================================
class ProveedorArchivos : public W3dProveedorRecursos {
public:
    int tipo;
    explicit ProveedorArchivos(int t) : tipo(t) {}
    // todas las claves del tipo: las entradas de su carpeta del contenedor + lo que nombran los objetos
    // (y, para los sonidos, nada mas: los nombran los scripts). Sin las purgadas que nadie nombra.
    void Claves(std::map<std::string, int>& uso) {
        uso.clear();
        AsegurarEntradasArch();
        const std::vector<std::string>& ents = gEntradasArch[tipo];
        for (size_t i = 0; i < ents.size(); i++) {
            uso[ClaveTextura(ents[i])] += 0;
        }
        std::vector<RefArchivo> refs;
        RefsArchivo(tipo, refs);
        std::set<std::pair<Object*, std::string> > vistos;
        for (size_t i = 0; i < refs.size(); i++) {
            const std::string k = ClaveTextura(*refs[i].ruta);
            if (vistos.insert(std::make_pair(refs[i].obj, k)).second) uso[k]++;
        }
        // un archivo que se paso AFUERA y que nadie nombra (un sonido: lo nombran los scripts) sigue siendo del
        // proyecto (antes de las purgadas: uno de afuera que se saco de sus usuarios tampoco va)
        std::vector<std::string> ext; W3dRefExternasListar(&ext);
        for (size_t i = 0; i < ext.size(); i++) {
            const std::string k = ClaveTextura(ext[i]);
            if (TipoArchivoDeRuta(k) == tipo && !uso.count(k)) uso[k] = 0;
        }
        for (std::set<std::string>::iterator p = gArchPurgadas.begin(); p != gArchPurgadas.end(); ++p) {
            std::map<std::string, int>::iterator u = uso.find(*p);
            if (u != uso.end() && u->second == 0) uso.erase(u);
        }
    }
    int UsuariosDe(const std::string& k, int deObjetos) {
        // los sonidos (y lo que un script nombre por su nombre de archivo) cuentan los scripts que lo nombran
        if (tipo == W3D_VISTA_SCRIPTS) return deObjetos;
        return deObjetos + ScriptsQueNombran(k);
    }
    void Item(const std::string& k, int usuarios, W3dRecursoItem& it) {
        it.id = k;
        it.nombre = BaseDeRuta(k);
        std::map<std::string, std::string>::iterator c = gCarpetaArch.find(k);
        it.carpeta = (c != gCarpetaArch.end()) ? c->second : std::string();
        it.entrada = k;
        it.icono = W3dVistaIcono(tipo);
        it.usuarios = usuarios;
        it.renombrable = W3dContenedorHayMontado() && W3dEsNombreDeEntrada(k) && tipo != W3D_VISTA_SCRIPTS;
        const long tam = TamArchivo(k);
        it.info = tam >= 0 ? W3dBytesTexto((double)tam) : std::string(T("missing"));
    }
    void Listar(std::vector<W3dRecursoItem>& out) {
        out.clear();
        std::map<std::string, int> uso;
        Claves(uso);
        for (std::map<std::string, int>::iterator it = uso.begin(); it != uso.end(); ++it) {
            W3dRecursoItem item;
            Item(it->first, UsuariosDe(it->first, it->second), item);
            out.push_back(item);
        }
    }
    bool FijarCarpeta(const std::string& id, const std::string& carpeta) {
        if (carpeta.empty()) gCarpetaArch.erase(id); else gCarpetaArch[id] = carpeta;
        return true;
    }
    std::string IdPedido(const std::string& id, const std::string& nuevo) { return EntradaTexturaPedida(id, nuevo); }
    // renombrar = MUDAR la entrada (como una textura) y llevar ahi a los objetos que la nombran
    bool Renombrar(const std::string& id, const std::string& nuevo, std::string* final) {
        if (!W3dContenedorHayMontado() || !W3dEsNombreDeEntrada(id) || tipo == W3D_VISTA_SCRIPTS) return false;
        const std::string destino = EntradaTexturaLibre(id, nuevo);
        if (final) *final = destino;
        if (destino == id) return true;
        const int scripts = ScriptsQueNombran(id);
        if (!MudarEntradaArchivo(id, destino)) return false;
        UndoRenArchivo* u = new UndoRenArchivo();
        u->tipo = tipo; u->actual = destino; u->otro = id;
        UndoExterno f; f.aplicar = UndoRenArchivoAplicar; f.liberar = UndoRenArchivoLiberar;
        UndoPushExterno(f, u);
        if (scripts > 0) {
            char b[16]; sprintf(b, "%d", scripts);
            Notificar(std::string(T("Scripts that name the file")) + " (" + b + "): " +
                      W3dNombreCorto(BaseDeRuta(id)) + " -> " + W3dNombreCorto(BaseDeRuta(destino)), true);
        }
        return true;
    }
    // la mudanza de la entrada (bytes al overlay con el nombre nuevo; la vieja queda purgada)
    bool MudarEntradaArchivo(const std::string& viejo, const std::string& nuevo) {
        if (W3dContenedorEntradaEditada(viejo)) {
            if (!W3dContenedorMudarEditada(viejo, nuevo)) return false;
        } else {
            std::vector<unsigned char> d;
            if (!BytesArchivo(viejo, d) || !W3dContenedorEscribirEntrada(nuevo, &d[0], d.size())) return false;
        }
        gArchPurgadas.insert(viejo);
        gArchPurgadas.erase(nuevo);
        CambiarRutaArchivo(tipo, viejo, nuevo);
        return true;
    }
    struct UndoRenArchivo { int tipo; std::string actual, otro; };
    static void UndoRenArchivoAplicar(void* d);
    static void UndoRenArchivoLiberar(void* d) { delete (UndoRenArchivo*)d; }
    bool Borrar(const std::string& id, std::string* motivo) {
        std::map<std::string, int> uso;
        Claves(uso);
        std::map<std::string, int>::iterator u = uso.find(id);
        if (u == uso.end()) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        if (UsuariosDe(id, u->second) > 0) { if (motivo) *motivo = "It is in use"; return false; }
        gArchPurgadas.insert(id);
        gCarpetaArch.erase(id);
        DesmarcarExterna(id);   // (una de afuera deja de ser del proyecto; el archivo de disco no se toca)
        W3dRecursosVistaInvalidar();
        return true;
    }
    // EN USO: los objetos que lo nombran lo SUELTAN (una fuente vuelve a la de Whisk3D, un video queda
    // vacio), con Ctrl+Z; los scripts que lo nombran por texto quedan como estan (se avisa)
    bool SabeBorrarEnUso() const { return tipo != W3D_VISTA_SCRIPTS; }
    bool BorrarEnUso(const std::string& id, std::string* motivo) {
        if (tipo == W3D_VISTA_SCRIPTS) { if (motivo) *motivo = "It is in use"; return false; }
        UndoArchSacado* u = new UndoArchSacado();
        u->tipo = tipo; u->clave = id; u->sacado = false;
        u->ret = RetenerEntrada(id);
        std::vector<RefArchivo> refs;
        RefsArchivo(tipo, refs);
        for (size_t i = 0; i < refs.size(); i++)
            if (ClaveTextura(*refs[i].ruta) == id) { u->objs.push_back(refs[i].obj); u->rutas.push_back(*refs[i].ruta); }
        UndoArchSacadoAplicar(u);
        UndoExterno f; f.aplicar = UndoArchSacadoAplicar; f.desvincular = UndoArchSacadoDesvincular; f.liberar = UndoArchSacadoLiberar;
        UndoPushExterno(f, u);
        const int scripts = ScriptsQueNombran(id);
        if (scripts > 0) {
            char b[16]; sprintf(b, "%d", scripts);
            Notificar(std::string(T("Scripts that name the file")) + " (" + b + "): " + W3dNombreCorto(BaseDeRuta(id)), true);
        }
        return true;
    }
    // ('ret' = su entrada, si es de adentro: un guardado la puede sacar del .w3d antes del Ctrl+Z)
    struct UndoArchSacado { int tipo; std::string clave; std::vector<Object*> objs; std::vector<std::string> rutas; bool sacado;
                            EntradaRetenida* ret; };
    static void UndoArchSacadoAplicar(void* d) {
        UndoArchSacado* u = (UndoArchSacado*)d;
        const bool poner = u->sacado;
        if (poner) ReponerEntrada(u->ret);
        for (size_t i = 0; i < u->objs.size(); i++) {
            Object* o = u->objs[i];
            if (!o) continue;
            std::string* r = NULL;
            if (u->tipo == W3D_VISTA_FUENTES && o->getType() == ObjectType::texto2d) r = &((Texto2D*)o)->fuente;
            if (u->tipo == W3D_VISTA_VIDEOS && o->getType() == ObjectType::video2d) r = &((Video2D*)o)->video;
            if (r) *r = poner ? u->rutas[i] : std::string();
        }
        if (poner) gArchPurgadas.erase(u->clave); else gArchPurgadas.insert(u->clave);
        u->sacado = !u->sacado;
        W3dRecursosVistaInvalidar();
    }
    static void UndoArchSacadoDesvincular(void* d, Object* borrado) {
        UndoArchSacado* u = (UndoArchSacado*)d;
        for (size_t i = 0; i < u->objs.size(); i++) if (u->objs[i] == borrado) u->objs[i] = NULL;
    }
    static void UndoArchSacadoLiberar(void* d) {
        UndoArchSacado* u = (UndoArchSacado*)d;
        SoltarEntrada(u->ret);
        delete u;
    }
    void Usuarios(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        std::vector<RefArchivo> refs;
        RefsArchivo(tipo, refs);
        std::set<Object*> vistos;
        for (size_t i = 0; i < refs.size(); i++)
            if (ClaveTextura(*refs[i].ruta) == id && vistos.insert(refs[i].obj).second) out.push_back(refs[i].obj);
    }
    void Datos(const std::string& id, std::vector<W3dRecursoDato>& out) {
        out.clear();
        const InfoArchivo& ia = InfoDeArchivo(tipo, id);   // (recordado: se pide en cada cuadro)
        const bool hay = ia.bytes > 0;
        const size_t punto = id.rfind('.'), barra = id.find_last_of("/\\");
        std::string fmt = (punto != std::string::npos && (barra == std::string::npos || punto > barra)) ? id.substr(punto + 1) : std::string();
        for (size_t i = 0; i < fmt.size(); i++) if (fmt[i] >= 'a' && fmt[i] <= 'z') fmt[i] = (char)(fmt[i] - 32);
        if (tipo == W3D_VISTA_SONIDOS && hay) {
            const W3dSonidoDatos& sd = ia.son;
            if (ia.esSonido) {
                out.push_back(W3dRecursoDato(T("Format"), sd.formato));
                if (sd.frecuencia > 0) out.push_back(W3dRecursoDato(T("Sample rate"), EnteroTexto(sd.frecuencia) + " Hz"));
                if (sd.canales > 0) out.push_back(W3dRecursoDato(T("Channels"), sd.canales == 1 ? std::string(T("mono (1)")) :
                                                                  sd.canales == 2 ? std::string(T("stereo (2)")) : EnteroTexto(sd.canales)));
                if (sd.bits > 0) out.push_back(W3dRecursoDato(T("Bits"), EnteroTexto(sd.bits)));
                out.push_back(W3dRecursoDato(T("Duration"), W3dDuracionTexto(sd.duracion)));
                if (sd.formato == "WAV") out.push_back(W3dRecursoDato(T("Loop (smpl)"), SiNo(sd.loop)));
            } else out.push_back(W3dRecursoDato(T("Format"), fmt));
        } else {
            out.push_back(W3dRecursoDato(T("Type"), T(W3dVistaSingular(tipo))));
            out.push_back(W3dRecursoDato(T("Format"), fmt));
        }
        if (hay) out.push_back(W3dRecursoDato(T("Bytes"), W3dBytesTexto((double)ia.bytes)));
        DatosUbicacion(id, out);
        W3dRecursoItem it;
        if (Info(id, it)) out.push_back(W3dRecursoDato(T("Users"), EnteroTexto(it.usuarios)));
    }
    int Ubicacion(const std::string& id) { return UbicacionDeRuta(id); }
    std::string RutaExternaSugerida(const std::string& id) { return RutaExternaDe(id); }
    bool FijarUbicacion(const std::string& id, int ubicacion, const std::string& rutaExterna,
                        std::string* idNuevo, std::string* motivo) {
        return FijarUbicacionArchivo(tipo, id, ubicacion, rutaExterna, idNuevo, motivo);
    }
};
static ProveedorArchivos gProvSonidos(W3D_VISTA_SONIDOS);
static ProveedorArchivos gProvScripts(W3D_VISTA_SCRIPTS);
static ProveedorArchivos gProvFuentes(W3D_VISTA_FUENTES);
static ProveedorArchivos gProvVideos(W3D_VISTA_VIDEOS);
static ProveedorArchivos* ProvArchivo(int tipo) {
    switch (tipo) {
        case W3D_VISTA_SONIDOS: return &gProvSonidos;
        case W3D_VISTA_SCRIPTS: return &gProvScripts;
        case W3D_VISTA_FUENTES: return &gProvFuentes;
        case W3D_VISTA_VIDEOS:  return &gProvVideos;
    }
    return NULL;
}
void ProveedorArchivos::UndoRenArchivoAplicar(void* d) {
    UndoRenArchivo* u = (UndoRenArchivo*)d;
    ProveedorArchivos* p = ProvArchivo(u->tipo);
    if (!p || !p->MudarEntradaArchivo(u->actual, u->otro)) return;
    W3dRecursoActivoRenombrado(u->tipo, u->actual, u->otro);
    std::swap(u->actual, u->otro);
}

class ProveedorTexturas : public W3dProveedorRecursos {
public:
    // todas las claves de la vista: las cargadas del proyecto, las que esperan en la cola y
    // las entradas texturas/ del contenedor; sin las purgadas que siguen sin usuarios
    static void Claves(std::map<std::string, Texture*>& out, std::map<std::string, TexUso>& uso) {
        out.clear();
        UsuariosTexturas(uso, false);
        for (size_t i = (size_t)TexturasBase(); i < Textures.size(); i++) {
            Texture* t = Textures[i];
            if (!t || t->path.empty()) continue;
            if (t->path.compare(0, 4, "lib:") == 0) continue;   // (una de una LIBRERIA externa: no es del proyecto)
            out[ClaveTextura(t->path)] = t;
        }
        AsegurarEntradasTex();
        for (size_t i = 0; i < gEntradasTex.size(); i++) {
            const std::string k = ClaveTextura(gEntradasTex[i]);
            if (!out.count(k)) out[k] = NULL;
        }
        // las que esperan en la cola o que nombra algo y no estan cargadas (externas)
        for (std::map<std::string, TexUso>::iterator it = uso.begin(); it != uso.end(); ++it)
            if (!out.count(it->first) && it->first.compare(0, 4, "lib:") != 0) out[it->first] = NULL;
        // las de AFUERA de la biblioteca aunque nadie las use (se pasaron afuera siendo huerfanas)
        for (std::set<std::string>::iterator e = gTexExternasBib.begin(); e != gTexExternasBib.end(); ++e)
            if (!out.count(*e) && ExternaMarcada(*e)) out[*e] = NULL;
        for (std::set<std::string>::iterator p = gTexPurgadas.begin(); p != gTexPurgadas.end(); ++p) {
            std::map<std::string, TexUso>::iterator u = uso.find(*p);
            if (u == uso.end() || u->second.usuarios == 0) out.erase(*p);
        }
    }
    static void Item(const std::string& k, const Texture* t, int usuarios, W3dRecursoItem& it) {
        it.id = k;
        it.nombre = BaseDeRuta(k);
        std::map<std::string, std::string>::iterator c = gCarpetaTex.find(k);
        it.carpeta = (c != gCarpetaTex.end()) ? c->second : std::string();
        it.entrada = k;
        it.icono = (int)IconType::textura;
        it.usuarios = usuarios + ScriptsQueNombran(k);
        // su nombre ES su archivo: renombrarla es mudar su ENTRADA (las de disco, al guardarlas)
        it.renombrable = TexturaRenombrable(k);
        if (t && t->ancho > 0) it.info = EnteroTexto(t->ancho) + "x" + EnteroTexto(t->alto);
        else if (t) it.info.clear();
        else it.info = T("not loaded");
    }
    void Listar(std::vector<W3dRecursoItem>& out) {
        out.clear();
        std::map<std::string, Texture*> claves; std::map<std::string, TexUso> uso;
        Claves(claves, uso);
        for (std::map<std::string, Texture*>::iterator it = claves.begin(); it != claves.end(); ++it) {
            W3dRecursoItem item;
            std::map<std::string, TexUso>::iterator u = uso.find(it->first);
            Item(it->first, it->second, u != uso.end() ? u->second.usuarios : 0, item);
            out.push_back(item);
        }
    }
    bool FijarCarpeta(const std::string& id, const std::string& carpeta) {
        if (carpeta.empty()) gCarpetaTex.erase(id); else gCarpetaTex[id] = carpeta;
        return true;
    }
    bool Borrar(const std::string& id, std::string* motivo) {
        W3dRecursoItem it;
        if (!Info(id, it)) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        if (it.usuarios > 0) { if (motivo) *motivo = "It is in use"; return false; }
        // la VISTA PREVIA de Properties la pudo cargar solo para mirarla: esa referencia se suelta
        // primero (si nadie mas la tenia, deja de estar cargada)
        SoltarVistaPreviaDe(id);
        // CARGADA EN MEMORIA (nadie la usa, pero quedo cargada): se purga EN CALIENTE. Libera la GPU
        // y sale de los selectores; el objeto queda en el cementerio del cache (ver TexturaPurgar)
        for (Texture* t = TexturaCargadaDe(id); t; t = TexturaCargadaDe(id))
            if (!PurgarTexturaCargada(t)) {
                if (motivo) *motivo = "It can't be deleted";
                return false;
            }
        gTexPurgadas.insert(id);
        gCarpetaTex.erase(id);
        // una de AFUERA deja de ser del proyecto (el archivo de disco no se toca)
        if (gTexExternasBib.erase(id)) DesmarcarExterna(id);
        return true;
    }
    std::string IdPedido(const std::string& id, const std::string& nuevo) {
        return EntradaTexturaPedida(id, nuevo);
    }
    bool Renombrar(const std::string& id, const std::string& nuevo, std::string* final) {
        if (!TexturaRenombrable(id)) return false;
        const std::string destino = EntradaTexturaLibre(id, nuevo);
        if (final) *final = destino;
        if (destino == id) return true;
        const int scripts = ScriptsQueNombran(id);   // (antes de mudar: nombran el archivo VIEJO)
        if (!MudarTextura(id, destino)) return false;
        UndoRenTextura* u = new UndoRenTextura();
        u->actual = destino; u->otro = id;
        UndoExterno f; f.aplicar = UndoRenTexturaAplicar; f.liberar = UndoRenTexturaLiberar;
        UndoPushExterno(f, u);
        // los .lua no se reescriben (arman nombres en tiempo de ejecucion): se avisa, como al
        // renombrar una escena que un cambiarEscena() nombra
        if (scripts > 0) {
            char b[16]; sprintf(b, "%d", scripts);
            Notificar(std::string(T("Scripts that name the texture")) + " (" + b + "): " +
                      W3dNombreCorto(BaseDeRuta(id)) + " -> " + W3dNombreCorto(BaseDeRuta(destino)), true);
        }
        return true;
    }
    void Usuarios(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        std::map<std::string, TexUso> uso;
        UsuariosTexturas(uso, true);
        std::map<std::string, TexUso>::iterator u = uso.find(id);
        if (u != uso.end()) out = u->second.objetos;
    }
    // los datos de la IMAGEN (del archivo, sin cargarla): formato, resolucion, alfa (binario o con
    // transparencias), bytes en el .w3d y en memoria (con mipmaps, un tercio mas), filtrado
    void Datos(const std::string& id, std::vector<W3dRecursoDato>& out) {
        out.clear();
        const InfoArchivo& ia = InfoDeArchivo(W3D_VISTA_TEXTURAS, id);   // (recordado: se pide en cada cuadro)
        const W3dImagenDatos& im = ia.im;
        const bool hay = ia.esImagen;
        const Texture* t = TexturaCargadaDe(id);
        const int w = hay ? im.ancho : (t ? t->ancho : 0), h = hay ? im.alto : (t ? t->alto : 0);
        if (hay) out.push_back(W3dRecursoDato(T("Format"), im.formato));
        if (w > 0) out.push_back(W3dRecursoDato(T("Resolution"), EnteroTexto(w) + " x " + EnteroTexto(h)));
        if (hay) {
            std::string a = SiNo(im.alfa);
            if (im.alfa && im.alfaTipo == 1) a += std::string(" (") + T("binary") + ")";
            else if (im.alfa && im.alfaTipo == 2) a += std::string(" (") + T("with transparency") + ")";
            else if (im.alfa && im.alfaTipo == 0) a += std::string(" (") + T("all opaque") + ")";
            out.push_back(W3dRecursoDato(T("Alpha channel"), a));
        }
        if (ia.bytes > 0) out.push_back(W3dRecursoDato(T("Bytes"), W3dBytesTexto((double)ia.bytes)));
        if (w > 0) {
            // la memoria de la GPU (RGBA de 8 bits): sin mipmaps y con ellos (la cadena suma un tercio)
            const bool mip = w3dEngine::MipmapsGlobal();
            const double mem = (double)w * (double)h * 4.0;
            out.push_back(W3dRecursoDato(T("Memory"), W3dBytesTexto(mem) + " (" + W3dBytesTexto(mem * 4.0 / 3.0) + " " + T("with mipmaps") + ")"));
            out.push_back(W3dRecursoDato(T("Mipmaps"), SiNo(mip)));
        }
        out.push_back(W3dRecursoDato(T("Loaded"), SiNo(t != NULL)));
        DatosUbicacion(id, out);
        W3dRecursoItem it;
        if (Info(id, it)) out.push_back(W3dRecursoDato(T("Users"), EnteroTexto(it.usuarios)));
    }
    int Ubicacion(const std::string& id) { return UbicacionDeRuta(id); }
    std::string RutaExternaSugerida(const std::string& id) { return RutaExternaDe(id); }
    bool FijarUbicacion(const std::string& id, int ubicacion, const std::string& rutaExterna,
                        std::string* idNuevo, std::string* motivo) {
        SoltarVistaPreviaDe(id);
        return FijarUbicacionArchivo(W3D_VISTA_TEXTURAS, id, ubicacion, rutaExterna, idNuevo, motivo);
    }
    // EN USO: los materiales la sueltan (base, normal map, capas) y los objetos que la nombran por ruta
    // tambien; la textura sale de la GPU al cementerio. Ctrl+Z devuelve todo (la textura revive).
    bool SabeBorrarEnUso() const { return true; }
    bool BorrarEnUso(const std::string& id, std::string* motivo) {
        for (size_t i = 0; i < SceneFlipbooks.size(); i++)
            if (SceneFlipbooks[i] && !SceneFlipbooks[i]->atlas.empty() && ClaveTextura(SceneFlipbooks[i]->atlas) == id) {
                if (motivo) *motivo = "A flipbook uses it";
                return false;
            }
        SoltarVistaPreviaDe(id);
        UndoTexSacada* u = new UndoTexSacada();
        u->clave = id; u->sacada = false;
        u->ret = RetenerEntrada(id);
        for (size_t i = 0; i < Materials.size(); i++) {
            Material* m = Materials[i];
            if (!m) continue;
            TexSacadaMat x; x.m = m;
            if (m->texture && ClaveTextura(m->texture->path) == id) { x.donde = -1; x.t = m->texture; u->mats.push_back(x); }
            if (m->normalTexture && ClaveTextura(m->normalTexture->path) == id) { x.donde = -2; x.t = m->normalTexture; u->mats.push_back(x); }
            for (size_t c = 0; c < m->capas.size(); c++)
                if (m->capas[c].tex && ClaveTextura(m->capas[c].tex->path) == id) { x.donde = (int)c; x.t = m->capas[c].tex; u->mats.push_back(x); }
        }
        { std::vector<Object*> raices; W3dRaicesVivas(raices);
          for (size_t i = 0; i < raices.size(); i++) JuntarObjsTex(raices[i], id, u->objs); }
        UndoTexSacadaAplicar(u);
        UndoExterno f; f.aplicar = UndoTexSacadaAplicar; f.desvincular = UndoTexSacadaDesvincular; f.liberar = UndoTexSacadaLiberar;
        UndoPushExterno(f, u);
        return true;
    }
};

// ============================================================================
//  PROVEEDOR: ANIMACIONES (los animsets del registro + los que viven solo en memoria)
// ============================================================================
static void UsuariosAnimSets(std::map<std::string, int>& cuenta, std::map<std::string, std::vector<Object*> >* objs) {
    std::vector<Object*> todos;
    ObjetosEscena(todos);
    for (size_t i = 0; i < todos.size(); i++) {
        if (todos[i]->getType() != ObjectType::armature) continue;
        Armature* a = (Armature*)todos[i];
        // el MISMO criterio que W3dAnimSetUsuarios (el del guardado)
        if (a->animations.empty()) continue;
        const W3dAnimSet* set = W3dArmatureAnimSetCalza(a) ? W3dArmatureAnimSet(a) : 0;
        const std::string n = set ? set->nombre : a->animSetNombre;
        if (n.empty()) continue;
        cuenta[n]++;
        if (objs) (*objs)[n].push_back(a);
    }
    // los objetos raiz que la usan como biblioteca de CLIPS DE JERARQUIA (W3dAnimSet.h)
    for (size_t i = 0; i < todos.size(); i++) {
        const Object* o = todos[i];
        if (!o->clipsJer || o->clipsJer->animset.empty()) continue;
        cuenta[o->clipsJer->animset]++;
        if (objs) (*objs)[o->clipsJer->animset].push_back(todos[i]);
    }
}
// "3 clips, 12 bones" / "2 hierarchy clips" (lo que tenga)
static std::string InfoAnimSet(const W3dAnimSet* set) {
    std::string t;
    if (!set->datos.clips.empty() || set->datos.jerarquias.empty())
        t = Cantidad((long)set->datos.clips.size(), "clip", "clips") + ", " +
            Cantidad((long)set->datos.huesos.size(), "bone", "bones");
    if (!set->datos.jerarquias.empty()) {
        if (!t.empty()) t += ", ";
        t += Cantidad((long)set->datos.jerarquias.size(), "hierarchy clip", "hierarchy clips");
    }
    return t;
}
// el animset CARGADO con ese nombre (NULL = no esta en memoria)
static W3dAnimSet* AnimSetCargado(const std::string& nombre) {
    std::vector<W3dRecurso*> todos;
    W3dRecursosListar(W3DREC_ANIMSET, todos);
    for (size_t i = 0; i < todos.size(); i++)
        if (todos[i] && todos[i]->estado == W3DREC_LISTO && todos[i]->dato &&
            ((W3dAnimSet*)todos[i]->dato)->nombre == nombre) return (W3dAnimSet*)todos[i]->dato;
    return NULL;
}

// paso de undo del renombre de un animset (W3dAnimSetRenombrar no tiene uno propio): el
// toggle de siempre entre los dos nombres
struct UndoRenAnimSet { std::string actual, otro; };
static void UndoRenAnimSetAplicar(void* d) {
    UndoRenAnimSet* u = (UndoRenAnimSet*)d;
    std::string quedo;
    if (!W3dAnimSetRenombrar(NULL, u->actual, u->otro, &quedo)) return;
    W3dRecursoActivoRenombrado(W3D_VISTA_ANIMACIONES, u->actual, quedo);
    u->otro = u->actual;
    u->actual = quedo;
}
static void UndoRenAnimSetLiberar(void* d) { delete (UndoRenAnimSet*)d; }

// ---- LOS CLIPS DE JERARQUIA de una biblioteca, como items hijos de su animset (id "Biblioteca/clip") ----
// el clip de un id de clip (NULL = no es uno, o ya no esta); 'rec' = su biblioteca (cargada)
static W3dClipJer* ClipDeId(const std::string& id, W3dRecurso** rec) {
    if (rec) *rec = 0;
    const size_t barra = id.find('/');
    if (barra == std::string::npos || barra == 0 || barra + 1 >= id.size()) return 0;
    W3dRecurso* r = W3dJerBibliotecaCargada(id.substr(0, barra));
    W3dAnimSet* set = W3dJerBiblioteca(r);
    if (!set) return 0;
    const std::string nom = id.substr(barra + 1);
    for (size_t i = 0; i < set->datos.jerarquias.size(); i++)
        if (set->datos.jerarquias[i] && set->datos.jerarquias[i]->nombre == nom) { if (rec) *rec = r; return set->datos.jerarquias[i]; }
    return 0;
}
static std::string InfoClipJer(const W3dClipJer* c) {
    char b[160];
    snprintf(b, sizeof(b), "%s, %d-%d, %d fps, ", T("hierarchy clip"), c->inicio, c->fin, c->fps);
    std::string t = b;
    t += Cantidad((long)c->pistas.size(), "track", "tracks");
    if (c->retarget == W3D_RETARGET_ROTACIONES) t += std::string(", ") + T("Rotations only");
    return t;
}
// los items de los clips de la biblioteca 'set' (hijos del item 'padre')
static void ListarClipsJer(const W3dAnimSet* set, const W3dRecursoItem& padre, std::vector<W3dRecursoItem>& out) {
    if (!set) return;
    for (size_t k = 0; k < set->datos.jerarquias.size(); k++) {
        const W3dClipJer* c = set->datos.jerarquias[k];
        if (!c) continue;
        W3dRecursoItem it;
        it.id = padre.id + "/" + c->nombre;
        it.nombre = c->nombre;
        it.padre = padre.id;
        it.carpeta = padre.carpeta;
        it.entrada = padre.entrada;
        it.icono = (int)IconType::object;
        it.usuarios = padre.usuarios;   // (lo usan las mismas raices que a su biblioteca)
        it.info = InfoClipJer(c);
        out.push_back(it);
    }
}

class ProveedorAnimaciones : public W3dProveedorRecursos {
public:
    // clips, duracion, fps y destino (huesos / jerarquia) de la biblioteca cargada, o de UN clip de jerarquia
    void Datos(const std::string& id, std::vector<W3dRecursoDato>& out) {
        out.clear();
        W3dRecursoItem it;
        const bool hay = Info(id, it);
        W3dClipJer* c = ClipDeId(id, 0);
        if (c) {
            out.push_back(W3dRecursoDato(T("Target"), T("hierarchy")));
            out.push_back(W3dRecursoDato(T("Frames"), EnteroTexto(c->inicio) + " - " + EnteroTexto(c->fin)));
            out.push_back(W3dRecursoDato(T("FPS"), EnteroTexto(c->fps)));
            out.push_back(W3dRecursoDato(T("Duration"), W3dDuracionTexto(c->Duracion())));
            out.push_back(W3dRecursoDato(T("Tracks"), EnteroTexto((long)c->pistas.size())));
        } else {
            const W3dAnimSet* set = AnimSetCargado(id);
            if (set) {
                float dur = 0.0f; int fps = 0;
                for (size_t k = 0; k < set->datos.clips.size(); k++) {
                    const SkeletalAnimation* a = set->datos.clips[k];
                    if (!a || a->FrameRate <= 0) continue;
                    const float d = (float)(a->endFrame - a->startFrame + 1) / (float)a->FrameRate;
                    if (d > dur) dur = d;
                    fps = a->FrameRate;
                }
                for (size_t k = 0; k < set->datos.jerarquias.size(); k++) {
                    const W3dClipJer* j = set->datos.jerarquias[k];
                    if (!j) continue;
                    if (j->Duracion() > dur) dur = j->Duracion();
                    if (!fps) fps = j->fps;
                }
                std::string destino;
                if (!set->datos.clips.empty()) destino = T("bones");
                if (!set->datos.jerarquias.empty()) destino += std::string(destino.empty() ? "" : ", ") + T("hierarchy");
                out.push_back(W3dRecursoDato(T("Clips"), EnteroTexto((long)(set->datos.clips.size() + set->datos.jerarquias.size()))));
                if (!destino.empty()) out.push_back(W3dRecursoDato(T("Target"), destino));
                if (!set->datos.huesos.empty()) out.push_back(W3dRecursoDato(T("Bones"), EnteroTexto((long)set->datos.huesos.size())));
                out.push_back(W3dRecursoDato(T("Longest clip"), W3dDuracionTexto(dur)));
                if (fps) out.push_back(W3dRecursoDato(T("FPS"), EnteroTexto(fps)));
            } else out.push_back(W3dRecursoDato(T("State"), T("not loaded")));
            if (hay) {
                const long tam = TamArchivo(it.entrada);
                if (tam >= 0) out.push_back(W3dRecursoDato(T("In the .w3d"), W3dBytesTexto((double)tam)));
            }
        }
        if (hay) out.push_back(W3dRecursoDato(T("Users"), EnteroTexto(it.usuarios)));
    }
    void Listar(std::vector<W3dRecursoItem>& out) {
        out.clear();
        std::map<std::string, int> cuenta;
        UsuariosAnimSets(cuenta, NULL);
        const std::vector<W3dAnimSetFila>& reg = W3dAnimSetsRegistro();
        std::set<std::string> vistos;
        for (size_t i = 0; i < reg.size(); i++) {
            W3dRecursoItem it;
            it.id = it.nombre = reg[i].nombre;
            it.carpeta = W3dCarpetaNormalizar(reg[i].carpeta);
            it.entrada = reg[i].entrada;
            it.icono = (int)IconType::animacion;
            it.usuarios = cuenta[reg[i].nombre];
            const W3dAnimSet* set = AnimSetCargado(reg[i].nombre);
            if (set) it.info = InfoAnimSet(set);
            else it.info = T("not loaded");
            vistos.insert(reg[i].nombre);
            out.push_back(it);
            ListarClipsJer(set, it, out);
        }
        // los animsets EN MEMORIA que todavia no tienen entrada (Alt+D de un armature con clips
        // propios): se ven, pero se ordenan/renombran recien cuando el guardado los registra
        std::vector<W3dRecurso*> todos;
        W3dRecursosListar(W3DREC_ANIMSET, todos);
        for (size_t i = 0; i < todos.size(); i++) {
            if (!todos[i] || todos[i]->estado != W3DREC_LISTO || !todos[i]->dato) continue;
            if (todos[i]->id.compare(0, 4, "lib:") == 0) continue;   // (uno de una LIBRERIA externa: no es del proyecto)
            const W3dAnimSet* set = (const W3dAnimSet*)todos[i]->dato;
            if (set->nombre.empty() || vistos.count(set->nombre)) continue;
            vistos.insert(set->nombre);
            W3dRecursoItem it;
            it.id = it.nombre = set->nombre;
            it.icono = (int)IconType::animacion;
            it.usuarios = cuenta[set->nombre];
            it.soloLectura = true;
            it.info = InfoAnimSet(set) + " (" + T("unsaved") + ")";
            out.push_back(it);
            ListarClipsJer(set, it, out);
        }
    }
    bool FijarCarpeta(const std::string& id, const std::string& carpeta) {
        if (ClipDeId(id, 0)) return false;   // (un clip va donde va su biblioteca)
        return W3dAnimSetFijarCarpeta(id, carpeta);
    }
    std::string IdPedido(const std::string& id, const std::string& nuevo) {
        const size_t barra = id.find('/');
        if (barra != std::string::npos && ClipDeId(id, 0)) return id.substr(0, barra + 1) + W3dNombreNormalizar(nuevo, "Clip");
        return W3dProveedorRecursos::IdPedido(id, nuevo);
    }
    bool Renombrar(const std::string& id, const std::string& nuevo, std::string* final) {
        // un CLIP DE JERARQUIA: unico entre los de su biblioteca (lo ven todas sus raices), con su paso de undo
        W3dRecurso* rec = 0;
        W3dClipJer* c = ClipDeId(id, &rec);
        if (c) {
            W3dClipsVistasSincronizar();   // (la vista que se edita no puede pisarle el nombre al escribirse)
            UndoCapturarClipJer(rec, c);
            W3dJerClipRenombrar(W3dJerBiblioteca(rec), c, nuevo);
            W3dClipVistaActivaReleer(c);
            if (final) *final = id.substr(0, id.find('/') + 1) + c->nombre;
            return true;
        }
        std::string quedo;
        if (!W3dAnimSetRenombrar(NULL, id, nuevo, &quedo)) return false;
        if (final) *final = quedo;
        if (quedo != id) {
            UndoRenAnimSet* u = new UndoRenAnimSet();
            u->actual = quedo; u->otro = id;
            UndoExterno f; f.aplicar = UndoRenAnimSetAplicar; f.liberar = UndoRenAnimSetLiberar;
            UndoPushExterno(f, u);
        }
        return true;
    }
    bool Borrar(const std::string& id, std::string* motivo) {
        // un CLIP DE JERARQUIA: sale de su biblioteca (lo dejan de ver todas sus raices), con su paso de undo
        W3dRecurso* rec = 0;
        W3dClipJer* c = ClipDeId(id, &rec);
        if (c) {
            if (!UndoClipJerBorrar(rec, c)) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
            w3dLogf("[W3D] clip de jerarquia borrado: '%s'", id.c_str());
            return true;
        }
        std::map<std::string, int> cuenta;
        UsuariosAnimSets(cuenta, NULL);
        if (cuenta[id] > 0) { if (motivo) *motivo = "It is in use"; return false; }
        // CARGADO sin usuarios en la escena = lo retiene un armature BORRADO que vive en el undo (el
        // almacen lo libera con su ultima referencia). Sacarle la fila no lo borraba: volvia enseguida
        // como "(unsaved)" de solo lectura, sin poder purgarse. Igual que una malla en ese caso.
        if (AnimSetCargado(id)) { if (motivo) *motivo = "The undo history still uses it"; return false; }
        std::vector<W3dAnimSetFila> reg = W3dAnimSetsRegistro();
        for (size_t i = 0; i < reg.size(); i++) {
            if (reg[i].nombre != id) continue;
            w3dLogf("[W3D] animset huerfano borrado: '%s' (%s)", id.c_str(), reg[i].entrada.c_str());
            reg.erase(reg.begin() + (long)i);
            W3dAnimSetsRegistroFijar(reg);
            return true;
        }
        if (motivo) *motivo = "It isn't saved in the project yet";
        return false;
    }
    void Usuarios(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        std::map<std::string, int> cuenta;
        std::map<std::string, std::vector<Object*> > objs;
        UsuariosAnimSets(cuenta, &objs);
        // un clip: los de su biblioteca
        const size_t barra = id.find('/');
        out = (barra != std::string::npos && ClipDeId(id, 0)) ? objs[id.substr(0, barra)] : objs[id];
    }
};

// ============================================================================
//  LO PURGADO QUE VUELVE (un undo lo volvio a poner en uso)
// ============================================================================
static bool MaterialNombraTextura(const Material* m, const Texture* t) {
    if (!m || !t) return false;
    if (m->texture == t || m->normalTexture == t) return true;
    for (size_t c = 0; c < m->capas.size(); c++) if (m->capas[c].tex == t) return true;
    return false;
}
// (la purgada no tenia referencias -la purga las cero-: la ranura solo toma una de la viva)
static void RepuntarTextura(Material* m, Texture* de, Texture* a) {
    if (!m) return;
    if (m->texture == de) { TexturaRetener(a); m->texture = a; }
    if (m->normalTexture == de) { TexturaRetener(a); m->normalTexture = a; }
    for (size_t c = 0; c < m->capas.size(); c++) if (m->capas[c].tex == de) { TexturaRetener(a); m->capas[c].tex = a; }
}
// cuantas RANURAS de material (y cuadros de materiales animados) apuntan a 't': lo que la textura revivida tiene que
// tener de referencias (TexturaRevivir la deja en 1; las ranuras son las duenas, Textures.h). Tambien los pasos de
// undo de "poner una textura" que la guardan: cada uno es dueno de una y la suelta al caerse del historial
extern int W3dUndoTexturaDuenos(const Texture* t);   // (SoltarRecurso.cpp)
static int RanurasDe(const Texture* t) {
    int n = W3dUndoTexturaDuenos(t);
    for (size_t i = 0; i < Materials.size(); i++) {
        const Material* m = Materials[i];
        if (!m) continue;
        if (m->texture == t) n++;
        if (m->normalTexture == t) n++;
        for (size_t c = 0; c < m->capas.size(); c++) if (m->capas[c].tex == t) n++;
    }
    for (size_t i = 0; i < AnimatedMaterials.size(); i++)
        if (AnimatedMaterials[i])
            for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size(); f++)
                if (AnimatedMaterials[i]->frameTextures[f] == t) n++;
    return n;
}
void W3dRecursosRevisarPurgados() {
    if (MaterialesPurgados().empty() && TexturasPurgadas().empty()) return;
    // 1) los materiales purgados que una malla volvio a usar vuelven a su lista (con un nombre libre)
    if (!MaterialesPurgados().empty()) {
        std::map<const Material*, int> cuenta;
        UsuariosMateriales(cuenta);
        const std::vector<Material*>& pur = MaterialesPurgados();
        std::vector<Material*> revivir;
        for (size_t i = 0; i < pur.size(); i++) if (cuenta[pur[i]] > 0) revivir.push_back(pur[i]);
        for (size_t i = 0; i < revivir.size(); i++) MaterialRevivir(revivir[i]);
    }
    // 2) las texturas purgadas EN CALIENTE que un material vivo vuelve a nombrar: se recargan en el
    //    MISMO objeto; si mientras tanto se cargo otra con esa ruta, el material pasa a usar esa
    const std::vector<Texture*> tp = TexturasPurgadas();   // (copia: revivir la modifica)
    for (size_t k = 0; k < tp.size(); k++) {
        Texture* t = tp[k];
        bool usada = false;
        for (size_t i = 0; i < Materials.size() && !usada; i++) usada = MaterialNombraTextura(Materials[i], t);
        for (size_t i = 0; i < AnimatedMaterials.size() && !usada; i++)
            if (AnimatedMaterials[i])
                for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size() && !usada; f++)
                    usada = (AnimatedMaterials[i]->frameTextures[f] == t);
        if (!usada) continue;
        Texture* viva = TexturaBuscar(t->path);
        if (viva) {
            for (size_t i = 0; i < Materials.size(); i++) RepuntarTextura(Materials[i], t, viva);
            for (size_t i = 0; i < AnimatedMaterials.size(); i++)
                if (AnimatedMaterials[i])
                    for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size(); f++)
                        if (AnimatedMaterials[i]->frameTextures[f] == t) { TexturaRetener(viva); AnimatedMaterials[i]->frameTextures[f] = viva; }
        } else if (TexturaRevivir(t)) {
            viva = t;
            // revivida con UNA referencia: sus ranuras vuelven a ser sus duenas (una por cada una)
            for (int k2 = RanurasDe(t); k2 > 1; k2--) TexturaRetener(t);
        }
        if (viva) {
            gTexPurgadas.erase(ClaveTextura(viva->path));   // vuelve a ser del proyecto (el guardado la lleva)
            w3dLogf("[outliner] textura purgada vuelta a usar: %s", viva->path.c_str());
        } else {
            w3dLogfW("[outliner] la textura purgada %s se volvio a usar pero su imagen ya no esta", t->path.c_str());
        }
    }
}

// ============================================================================
//  EL REGISTRO DE PROVEEDORES
// ============================================================================
static ProveedorMallas      gProvMallas;
static ProveedorMateriales  gProvMateriales;
static ProveedorTexturas    gProvTexturas;
static ProveedorAnimaciones gProvAnimaciones;
static W3dProveedorRecursos* gProv[W3D_VISTAS] = {
    NULL, &gProvMallas, &gProvMateriales, &gProvTexturas, &gProvAnimaciones, NULL, NULL, NULL,
    &gProvSonidos, &gProvScripts, &gProvFuentes, &gProvVideos
};

void W3dRecursosVistaRegistrar(int vista, W3dProveedorRecursos* p) {
    if (vista <= W3D_VISTA_ESCENA || vista >= W3D_VISTAS) return;   // la escena no es una lista
    gProv[vista] = p;
}
W3dProveedorRecursos* W3dRecursosVistaProveedor(int vista) {
    return (vista > W3D_VISTA_ESCENA && vista < W3D_VISTAS) ? gProv[vista] : NULL;
}
void W3dVistaRecListar(int vista, std::vector<W3dRecursoItem>& out) {
    out.clear();
    W3dProveedorRecursos* p = W3dRecursosVistaProveedor(vista);
    if (!p) return;
    if (vista == W3D_VISTA_MATERIALES || vista == W3D_VISTA_TEXTURAS) W3dRecursosRevisarPurgados();
    // TODA malla es un recurso: la que nacio en este cuadro (un Add, un import, un script) ya lo tiene
    if (vista == W3D_VISTA_MALLAS) W3dMallasAsegurarRecursos();
    p->Listar(out);
    for (size_t i = 0; i < out.size(); i++) { out[i].carpeta = W3dCarpetaNormalizar(out[i].carpeta); out[i].tipo = vista; }
}
bool W3dVistaRecInfo(int vista, const std::string& id, W3dRecursoItem* out) {
    W3dProveedorRecursos* p = W3dRecursosVistaProveedor(vista);
    if (!p || id.empty()) return false;
    W3dRecursoItem it;
    if (!p->Info(id, it)) return false;
    it.carpeta = W3dCarpetaNormalizar(it.carpeta);
    it.tipo = vista;
    if (out) *out = it;
    return true;
}

// ============================================================================
//  LA BIBLIOTECA
// ============================================================================
// el ORDEN de los tipos en el filtro (el de la lista de mas arriba del .h, sin los que no tienen datos)
static const int kOrdenTipos[] = {
    W3D_VISTA_MALLAS, W3D_VISTA_MATERIALES, W3D_VISTA_TEXTURAS, W3D_VISTA_SONIDOS, W3D_VISTA_ANIMACIONES,
    W3D_VISTA_PREFABS, W3D_VISTA_ESCENAS, W3D_VISTA_SCRIPTS, W3D_VISTA_FUENTES, W3D_VISTA_VIDEOS, W3D_VISTA_LIBRERIAS
};
void W3dBibliotecaTipos(std::vector<int>& out) {
    out.clear();
    for (size_t i = 0; i < sizeof(kOrdenTipos) / sizeof(kOrdenTipos[0]); i++)
        if (W3dRecursosVistaProveedor(kOrdenTipos[i])) out.push_back(kOrdenTipos[i]);
}
void W3dBibliotecaListar(int filtro, std::vector<W3dRecursoItem>& out) {
    out.clear();
    std::vector<int> tipos;
    if (filtro > W3D_VISTA_ESCENA && filtro < W3D_VISTAS) tipos.push_back(filtro);
    else W3dBibliotecaTipos(tipos);
    std::vector<W3dRecursoItem> uno;
    for (size_t i = 0; i < tipos.size(); i++) {
        W3dVistaRecListar(tipos[i], uno);
        out.insert(out.end(), uno.begin(), uno.end());
    }
}
std::string W3dBibClave(int tipo, const std::string& id) { return std::string(W3dVistaClave(tipo)) + ":" + id; }
bool W3dBibDeClave(const std::string& c, int* tipo, std::string* id) {
    const size_t p = c.find(':');
    if (p == std::string::npos) return false;
    const int t = W3dVistaDeClave(c.substr(0, p));
    if (t <= W3D_VISTA_ESCENA) return false;
    if (tipo) *tipo = t;
    if (id) *id = c.substr(p + 1);
    return true;
}

// ============================================================================
//  CARPETAS (UN arbol para todos los tipos)
// ============================================================================
static std::vector<std::string> gCreadas;   // ordenadas, sin repetir

std::string W3dCarpetaNormalizar(const std::string& ruta) { return W3dMallaCarpetaNormalizar(ruta); }
std::string W3dCarpetaPadre(const std::string& ruta) {
    size_t p = ruta.find_last_of('/');
    return (p == std::string::npos) ? std::string() : ruta.substr(0, p);
}
std::string W3dCarpetaHoja(const std::string& ruta) {
    size_t p = ruta.find_last_of('/');
    return (p == std::string::npos) ? ruta : ruta.substr(p + 1);
}
bool W3dCarpetaAdentro(const std::string& ruta, const std::string& carpeta) {
    if (carpeta.empty()) return true;
    if (ruta == carpeta) return true;
    return ruta.size() > carpeta.size() && ruta.compare(0, carpeta.size(), carpeta) == 0 &&
           ruta[carpeta.size()] == '/';
}
const std::vector<std::string>& W3dCarpetasCreadas() { return gCreadas; }
// agrega la carpeta y sus ancestros a 'out'
static void ConAncestros(const std::string& c, std::set<std::string>& out) {
    std::string r = c;
    while (!r.empty()) { out.insert(r); r = W3dCarpetaPadre(r); }
}
void W3dCarpetasBiblioteca(const std::vector<W3dRecursoItem>& items, std::vector<std::string>& out) {
    std::set<std::string> s;
    for (size_t i = 0; i < gCreadas.size(); i++) ConAncestros(gCreadas[i], s);
    for (size_t i = 0; i < items.size(); i++) ConAncestros(items[i].carpeta, s);
    out.assign(s.begin(), s.end());
}
void W3dCarpetasTodas(std::vector<std::string>& out) {
    std::vector<W3dRecursoItem> its; W3dBibliotecaListar(-1, its);
    W3dCarpetasBiblioteca(its, out);
}
static void CreadasAgregar(const std::string& c) {
    if (c.empty()) return;
    std::vector<std::string>::iterator it = std::lower_bound(gCreadas.begin(), gCreadas.end(), c);
    if (it == gCreadas.end() || *it != c) gCreadas.insert(it, c);
}
std::string W3dCarpetasFirma() {
    std::vector<W3dRecursoItem> its; W3dBibliotecaListar(-1, its);
    return W3dCarpetasFirma(its);
}
std::string W3dCarpetasFirma(const std::vector<W3dRecursoItem>& its) {
    std::string f;
    for (size_t i = 0; i < gCreadas.size(); i++) { f += gCreadas[i]; f += '\n'; }
    f += '\x01';
    std::vector<std::string> filas;
    for (size_t i = 0; i < its.size(); i++)
        if (!its[i].carpeta.empty()) filas.push_back(W3dBibClave(its[i].tipo, its[i].id) + "\x02" + its[i].carpeta);
    std::sort(filas.begin(), filas.end());
    for (size_t i = 0; i < filas.size(); i++) { f += filas[i]; f += '\n'; }
    return f;
}

// ---- EL UNDO DE LAS CARPETAS: un paso = el estado ANTES y DESPUES (las carpetas creadas + la
//      carpeta de cada item que cambio, de cualquier tipo). Aplicar = ir al otro. ----
struct PasoCarpetas {
    std::vector<std::string> creadasAntes, creadasDespues;
    std::vector<int> tipos;
    std::vector<std::string> ids, antes, despues;
    bool enAntes;
    PasoCarpetas() : enAntes(false) {}
};
static void PasoCarpetasIr(PasoCarpetas* p, bool aAntes) {
    gCreadas = aAntes ? p->creadasAntes : p->creadasDespues;
    for (size_t i = 0; i < p->ids.size(); i++) {
        W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(p->tipos[i]);
        if (prov) prov->FijarCarpeta(p->ids[i], aAntes ? p->antes[i] : p->despues[i]);
    }
    p->enAntes = aAntes;
}
static void PasoCarpetasAplicar(void* d) {
    PasoCarpetas* p = (PasoCarpetas*)d;
    PasoCarpetasIr(p, !p->enAntes);
}
static void PasoCarpetasLiberar(void* d) { delete (PasoCarpetas*)d; }

// el GRUPO abierto (W3dVistaRecGrupoIniciar): mientras dure, las operaciones no arman su propio
// paso; el grupo toma UNA foto al abrir y confirma UN paso al cerrar
static int gGrupoProf = 0;

// una operacion de carpetas: foto antes, foto despues, y si algo cambio UN paso de undo
class TransaccionCarpetas {
public:
    bool activa;   // false = adentro de un grupo (el paso lo arma el grupo)
    std::vector<std::string> creadas0;
    std::map<std::string, std::string> items0;   // clave de biblioteca -> carpeta
    explicit TransaccionCarpetas(bool esGrupo = false) : activa(true) {
        if (!esGrupo && gGrupoProf > 0) { activa = false; return; }
        creadas0 = gCreadas;
        std::vector<W3dRecursoItem> its; W3dBibliotecaListar(-1, its);
        for (size_t i = 0; i < its.size(); i++) items0[W3dBibClave(its[i].tipo, its[i].id)] = its[i].carpeta;
    }
    // el paso con lo que cambio desde la foto (NULL = nada)
    PasoCarpetas* Paso() {
        PasoCarpetas* p = new PasoCarpetas();
        p->creadasAntes = creadas0;
        p->creadasDespues = gCreadas;
        std::vector<W3dRecursoItem> its; W3dBibliotecaListar(-1, its);
        for (size_t i = 0; i < its.size(); i++) {
            std::map<std::string, std::string>::iterator a = items0.find(W3dBibClave(its[i].tipo, its[i].id));
            if (a == items0.end() || a->second == its[i].carpeta) continue;
            p->tipos.push_back(its[i].tipo); p->ids.push_back(its[i].id);
            p->antes.push_back(a->second); p->despues.push_back(its[i].carpeta);
        }
        if (p->ids.empty() && p->creadasAntes == p->creadasDespues) { delete p; return NULL; }
        return p;
    }
    bool Confirmar() {
        if (!activa) return false;
        PasoCarpetas* p = Paso();
        if (!p) return false;
        UndoExterno f; f.aplicar = PasoCarpetasAplicar; f.liberar = PasoCarpetasLiberar;
        SinRaiz sr;
        UndoPushExterno(f, p);
        return true;
    }
    // vuelve todo a la foto sin dejar paso (el Esc del modo mover)
    void Cancelar() {
        PasoCarpetas* p = Paso();
        if (!p) return;
        PasoCarpetasIr(p, true);
        delete p;
    }
};

// los nombres de las hermanas de una carpeta nueva (las carpetas hijas directas de 'padre')
static void Hermanas(const std::string& padre, std::vector<std::string>& out) {
    out.clear();
    std::vector<std::string> todas; W3dCarpetasTodas(todas);
    for (size_t i = 0; i < todas.size(); i++)
        if (W3dCarpetaPadre(todas[i]) == padre) out.push_back(W3dCarpetaHoja(todas[i]));
}

bool W3dCarpetaNueva(const std::string& padre0, const std::string& nombre, std::string* ruta) {
    const std::string padre = W3dCarpetaNormalizar(padre0);
    std::string hoja = W3dCarpetaNormalizar(nombre);
    // el nombre es UN tramo: la barra no arma subcarpetas desde aca (para eso se crea adentro)
    for (size_t i = 0; i < hoja.size(); i++) if (hoja[i] == '/') hoja[i] = '-';
    std::vector<std::string> hs; Hermanas(padre, hs);
    hoja = W3dNombreUnicoEnValores(hoja.empty() ? std::string("New Folder") : hoja, "New Folder", hs, -1);
    const std::string r = padre.empty() ? hoja : padre + "/" + hoja;
    TransaccionCarpetas tx;
    CreadasAgregar(r);
    tx.Confirmar();
    if (ruta) *ruta = r;
    return true;
}

// la carpeta 'ruta' pasa a llamarse 'nueva' (otra hoja o/y otra carpeta de arriba): sus subcarpetas
// creadas y sus items (los que no son de solo lectura, de todos los tipos) cambian de prefijo. Sin
// undo (lo arma quien llama).
static void ReubicarCarpeta(const std::string& ruta, const std::string& nueva) {
    std::vector<std::string> cr = gCreadas;
    gCreadas.clear();
    bool estaba = false;
    for (size_t i = 0; i < cr.size(); i++) {
        if (W3dCarpetaAdentro(cr[i], ruta)) { CreadasAgregar(nueva + cr[i].substr(ruta.size())); estaba = true; }
        else CreadasAgregar(cr[i]);
    }
    if (!estaba) CreadasAgregar(nueva);   // era implicita (solo por sus items): queda creada
    std::vector<W3dRecursoItem> its; W3dBibliotecaListar(-1, its);
    for (size_t i = 0; i < its.size(); i++) {
        if (its[i].soloLectura || !its[i].padre.empty() || !W3dCarpetaAdentro(its[i].carpeta, ruta)) continue;
        W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(its[i].tipo);
        if (prov) prov->FijarCarpeta(its[i].id, nueva + its[i].carpeta.substr(ruta.size()));
    }
}

// la carpeta EXISTE (creada, o de algun recurso). "" (la raiz) siempre.
static bool CarpetaExiste(const std::string& ruta) {
    if (ruta.empty()) return true;
    std::vector<std::string> todas; W3dCarpetasTodas(todas);
    return std::find(todas.begin(), todas.end(), ruta) != todas.end();
}

bool W3dCarpetaRenombrar(const std::string& ruta0, const std::string& nombre, std::string* rutaNueva) {
    const std::string ruta = W3dCarpetaNormalizar(ruta0);
    if (ruta.empty() || !CarpetaExiste(ruta)) return false;
    std::string hoja = W3dCarpetaNormalizar(nombre);
    for (size_t i = 0; i < hoja.size(); i++) if (hoja[i] == '/') hoja[i] = '-';
    if (hoja.empty()) { if (rutaNueva) *rutaNueva = ruta; return false; }
    const std::string padre = W3dCarpetaPadre(ruta);
    // unico entre las hermanas (sin contarse a si misma)
    std::vector<std::string> hs; Hermanas(padre, hs);
    const std::string vieja = W3dCarpetaHoja(ruta);
    for (size_t i = 0; i < hs.size(); i++) if (hs[i] == vieja) { hs.erase(hs.begin() + (long)i); break; }
    hoja = W3dNombreUnicoEnValores(hoja, "New Folder", hs, -1);
    const std::string nueva = padre.empty() ? hoja : padre + "/" + hoja;
    if (rutaNueva) *rutaNueva = nueva;
    if (nueva == ruta) return true;
    TransaccionCarpetas tx;
    ReubicarCarpeta(ruta, nueva);
    tx.Confirmar();
    return true;
}

bool W3dCarpetaVacia(const std::string& ruta0) {
    const std::string ruta = W3dCarpetaNormalizar(ruta0);
    std::vector<W3dRecursoItem> its; W3dBibliotecaListar(-1, its);
    for (size_t i = 0; i < its.size(); i++)
        if (W3dCarpetaAdentro(its[i].carpeta, ruta)) return false;
    return true;
}

bool W3dCarpetaBorrar(const std::string& ruta0, std::string* motivo) {
    const std::string ruta = W3dCarpetaNormalizar(ruta0);
    if (ruta.empty()) return false;
    if (!CarpetaExiste(ruta)) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
    if (!W3dCarpetaVacia(ruta)) { if (motivo) *motivo = "The folder isn't empty"; return false; }
    TransaccionCarpetas tx;
    std::vector<std::string> cr = gCreadas;
    gCreadas.clear();
    for (size_t i = 0; i < cr.size(); i++)
        if (!W3dCarpetaAdentro(cr[i], ruta)) CreadasAgregar(cr[i]);
    // la carpeta de ARRIBA sigue existiendo aunque quede vacia (se borra una sola por vez)
    CreadasAgregar(W3dCarpetaPadre(ruta));
    tx.Confirmar();
    return true;
}

bool W3dCarpetaMover(const std::string& ruta0, const std::string& destino0, std::string* rutaNueva) {
    const std::string ruta = W3dCarpetaNormalizar(ruta0), destino = W3dCarpetaNormalizar(destino0);
    if (rutaNueva) *rutaNueva = ruta;
    if (ruta.empty()) return false;
    // adentro de si misma (o de una de sus subcarpetas): no hay donde ponerla
    if (W3dCarpetaAdentro(destino, ruta)) return false;
    if (!CarpetaExiste(ruta) || !CarpetaExiste(destino)) return false;
    if (W3dCarpetaPadre(ruta) == destino) return true;   // ya esta ahi
    // su nombre, hecho unico entre las carpetas que ya cuelgan del destino
    std::vector<std::string> hs; Hermanas(destino, hs);
    const std::string hoja = W3dNombreUnicoEnValores(W3dCarpetaHoja(ruta), "New Folder", hs, -1);
    const std::string nueva = destino.empty() ? hoja : destino + "/" + hoja;
    if (rutaNueva) *rutaNueva = nueva;
    TransaccionCarpetas tx;
    ReubicarCarpeta(ruta, nueva);
    // la de ARRIBA de la que sale sigue existiendo aunque quede vacia (como al mover un recurso)
    CreadasAgregar(W3dCarpetaPadre(ruta));
    CreadasAgregar(destino);
    tx.Confirmar();
    return true;
}

// el GRUPO: una foto al abrir (la transaccion de afuera) y un paso al cerrar
static TransaccionCarpetas* gGrupoTx = NULL;
void W3dVistaRecGrupoIniciar() {
    if (gGrupoProf++ > 0) return;                 // anidado: manda el de afuera
    delete gGrupoTx;
    gGrupoTx = new TransaccionCarpetas(true);
}
void W3dVistaRecGrupoFin() {
    if (gGrupoProf <= 0) return;
    if (--gGrupoProf > 0) return;
    TransaccionCarpetas* tx = gGrupoTx;
    gGrupoTx = NULL;
    if (tx) { tx->Confirmar(); delete tx; }
}
void W3dVistaRecGrupoCancelar() {
    if (gGrupoProf <= 0) return;
    gGrupoProf = 0;
    TransaccionCarpetas* tx = gGrupoTx;
    gGrupoTx = NULL;
    if (tx) { tx->Cancelar(); delete tx; }
}
void (*W3dVistaRecGrupoCancelarHook)() = 0;
bool W3dVistaRecAntesDeUndo() {
    if (gGrupoProf <= 0) return false;
    if (W3dVistaRecGrupoCancelarHook) W3dVistaRecGrupoCancelarHook();   // el outliner sale de su modo mover
    W3dVistaRecGrupoCancelar();   // (si nadie lo cerro: todo vuelve a la foto)
    w3dLogf("[biblioteca] Ctrl+Z en el modo mover: se cancelo el modo (nada mas se deshizo)");
    return true;
}

// ============================================================================
//  ACCIONES SOBRE UN ITEM
// ============================================================================
bool W3dVistaRecMover(int vista, const std::string& id, const std::string& carpeta0) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    if (!prov) return false;
    W3dRecursoItem it;
    if (!W3dVistaRecInfo(vista, id, &it) || it.soloLectura || !it.padre.empty()) return false;   // (un hijo va con su padre)
    const std::string carpeta = W3dCarpetaNormalizar(carpeta0);
    if (it.carpeta == carpeta) return true;
    TransaccionCarpetas tx;
    if (!prov->FijarCarpeta(id, carpeta)) return false;
    // la carpeta de la que sale sigue existiendo aunque quede vacia (se borra a mano)
    CreadasAgregar(it.carpeta);
    CreadasAgregar(carpeta);
    tx.Confirmar();
    return true;
}

// renombrar o borrar un recurso del PROYECTO (malla, material, textura, animset, archivo, PREFAB) con escenas
// o prefabs sin abrir: se cargan antes, asi sus objetos siguen al nuevo nombre (o lo retienen como
// usuarios). Las referencias van por NOMBRE: una escena sin cargar quedaria nombrando lo que ya no
// existe (ver W3dRaices.h). Un PREFAB tambien: sus usuarios son sus instancias, y las de una escena sin
// abrir no se contaban (el prefab parecia huerfano, se borraba sin aviso y al abrir esa escena su instancia
// quedaba vacia). Las escenas no lo necesitan.
static void CargarRaicesSiHaceFalta(int vista) {
    if (vista == W3D_VISTA_ESCENAS) return;
    if (W3dRaicesCargarTodas() > 0) gScriptsLeidos = false;
}

bool W3dVistaRecRenombrar(int vista, const std::string& id, const std::string& nuevo, std::string* final) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    if (!prov) return false;
    CargarRaicesSiHaceFalta(vista);
    W3dRecursoItem it;
    if (!W3dVistaRecInfo(vista, id, &it) || it.soloLectura || !it.renombrable) return false;
    // EL MISMO NOMBRE QUE SE VE = no se toca nada. Los dos renames (en linea y el Name de Properties)
    // confirman al perder el foco, y el Esc solo devuelve el campo al texto de antes: sin esto, un
    // nombre que no esta en la forma que pide su tipo (una textura "texturas/Piso_A.png", que como
    // entrada pediria "piso_a.png"; un material con blancos en los bordes) se renombraba solo, con
    // su paso de undo, justo cuando el usuario pidio cancelar.
    if (nuevo == it.nombre) { if (final) *final = id; return true; }
    std::string quedo;
    SinRaiz sr;   // (el nombre de un recurso es de la biblioteca: la raiz activa no se edito)
    const std::string pedido = prov->IdPedido(id, nuevo);   // (antes: el renombre cambia lo ocupado)
    if (!prov->Renombrar(id, nuevo, &quedo)) return false;
    if (quedo != pedido) W3dAvisoYaExiste(pedido, quedo);   // informativo: el nombre ya estaba
    if (final) *final = quedo;
    W3dRecursoActivoRenombrado(vista, id, quedo);
    return true;
}

bool W3dVistaRecBorrar(int vista, const std::string& id, std::string* motivo) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    if (!prov) return false;
    CargarRaicesSiHaceFalta(vista);
    // la foto de los scripts puede estar vieja: se re-escanea ANTES de decidir que es huerfano
    gScriptsLeidos = false;
    W3dRecursoItem it;
    if (!W3dVistaRecInfo(vista, id, &it)) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
    if (it.soloLectura) { if (motivo) *motivo = "It is read-only"; return false; }
    // (una PARTE de un recurso -un clip de su biblioteca- se borra aunque el recurso este en uso: el proveedor
    // lo hace con su paso de undo)
    if (it.usuarios > 0 && it.padre.empty()) { if (motivo) *motivo = "It is in use"; return false; }
    if (!prov->Borrar(id, motivo)) return false;
    int v = -1; std::string a;
    if (W3dRecursoActivo(&v, &a) && v == vista && a == id) W3dRecursoDesactivar();
    return true;
}

bool W3dVistaRecSabeBorrarEnUso(int vista) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    return prov && prov->SabeBorrarEnUso();
}
bool W3dVistaRecBorrarForzado(int vista, const std::string& id, std::string* motivo) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    if (!prov) return false;
    CargarRaicesSiHaceFalta(vista);
    gScriptsLeidos = false;
    W3dRecursoItem it;
    if (!W3dVistaRecInfo(vista, id, &it)) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
    if (it.soloLectura) { if (motivo) *motivo = "It is read-only"; return false; }
    if (it.usuarios == 0 || !it.padre.empty()) return W3dVistaRecBorrar(vista, id, motivo);
    if (!prov->SabeBorrarEnUso()) { if (motivo) *motivo = "It is in use"; return false; }
    if (!prov->BorrarEnUso(id, motivo)) return false;
    int v = -1; std::string a;
    if (W3dRecursoActivo(&v, &a) && v == vista && a == id) W3dRecursoDesactivar();
    W3dRecursosVistaInvalidar();
    return true;
}

int W3dVistaRecPurgar(int vista, std::vector<std::string>* nombres) {
    if (nombres) nombres->clear();
    if (vista < 0) {
        // TODOS los tipos (la biblioteca sin filtro)
        std::vector<int> tipos; W3dBibliotecaTipos(tipos);
        int n = 0;
        for (size_t i = 0; i < tipos.size(); i++) {
            std::vector<std::string> ns;
            n += W3dVistaRecPurgar(tipos[i], &ns);
            if (nombres) nombres->insert(nombres->end(), ns.begin(), ns.end());
        }
        return n;
    }
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    if (!prov || !prov->Purgable()) return 0;   // (escenas/prefabs: se borran de a uno, a pedido)
    CargarRaicesSiHaceFalta(vista);
    gScriptsLeidos = false;
    std::vector<W3dRecursoItem> its; W3dVistaRecListar(vista, its);
    int n = 0;
    for (size_t i = 0; i < its.size(); i++) {
        if (its[i].usuarios > 0 || its[i].soloLectura || !its[i].padre.empty()) continue;   // (las partes no se purgan)
        std::string motivo;
        if (!W3dVistaRecBorrar(vista, its[i].id, &motivo)) {
            w3dLogf("[biblioteca] no se purgo '%s': %s", its[i].nombre.c_str(), motivo.c_str());
            continue;
        }
        n++;
        if (nombres) nombres->push_back(its[i].nombre);
    }
    return n;
}

int W3dVistaRecSeleccionarUsuarios(int vista, const std::string& id) {
    std::vector<std::string> ids(1, id);
    return W3dVistaRecSeleccionarUsuarios(vista, ids);
}
// la UNION de los OBJETOS de la escena que usan los recursos (un objeto que usa dos va una vez).
// 'claves' = claves de biblioteca (W3dBibClave)
static void UnionUsuarios(const std::vector<std::string>& claves, std::vector<Object*>& us) {
    us.clear();
    std::set<Object*> vistos;
    for (size_t k = 0; k < claves.size(); k++) {
        int t = 0; std::string id;
        if (!W3dBibDeClave(claves[k], &t, &id)) continue;
        W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(t);
        if (!prov) continue;
        std::vector<Object*> uno;
        prov->Usuarios(id, uno);
        // solo los de la escena ACTIVA se pueden seleccionar (los de otra escena o prefab cargado
        // cuentan como usuarios, pero no estan en el arbol que se ve)
        for (size_t i = 0; i < uno.size(); i++)
            if (uno[i] && W3dRaizDe(uno[i]) == SceneCollection && vistos.insert(uno[i]).second) us.push_back(uno[i]);
    }
}
static std::vector<std::string> ClavesDe(int vista, const std::vector<std::string>& ids) {
    std::vector<std::string> c;
    for (size_t i = 0; i < ids.size(); i++) c.push_back(W3dBibClave(vista, ids[i]));
    return c;
}
int W3dBibObjetosUsuarios(const std::vector<std::string>& claves) {
    std::vector<Object*> us;
    UnionUsuarios(claves, us);
    return (int)us.size();
}
int W3dVistaRecObjetosUsuarios(int vista, const std::vector<std::string>& ids) {
    return W3dBibObjetosUsuarios(ClavesDe(vista, ids));
}
int W3dBibSeleccionarUsuarios(const std::vector<std::string>& claves) {
    std::vector<Object*> us;
    UnionUsuarios(claves, us);
    // sin OBJETOS que seleccionar (lo usan una malla huerfana del registro, un script, un flipbook,
    // otro material...): la seleccion del usuario NO se toca
    if (us.empty()) return 0;
    UndoCapturarSeleccion();
    DeseleccionarTodo();
    for (size_t i = 0; i < us.size(); i++) {
        us[i]->Seleccionar();
        // que se vean en el arbol: los padres se despliegan
        for (Object* p = us[i]->Parent; p && p != SceneCollection; p = p->Parent) p->desplegado = true;
    }
    return (int)us.size();
}
int W3dVistaRecSeleccionarUsuarios(int vista, const std::vector<std::string>& ids) {
    return W3dBibSeleccionarUsuarios(ClavesDe(vista, ids));
}

bool W3dVistaRecDuplicar(int vista, const std::string& id, std::string* idNuevo, std::string* motivo) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    if (!prov) { if (motivo) *motivo = "This resource type can't be duplicated"; return false; }
    W3dRecursoItem it;
    if (!W3dVistaRecInfo(vista, id, &it)) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
    SinRaiz sr;
    if (!prov->Duplicar(id, idNuevo, motivo)) return false;
    // la copia va a la MISMA carpeta
    if (idNuevo && !it.carpeta.empty()) prov->FijarCarpeta(*idNuevo, it.carpeta);
    W3dRecursosVistaInvalidar();
    return true;
}
void W3dVistaRecDatos(int vista, const std::string& id, std::vector<W3dRecursoDato>& out) {
    out.clear();
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    if (prov) prov->Datos(id, out);
}
int W3dVistaRecUbicacion(int vista, const std::string& id) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    return prov ? prov->Ubicacion(id) : -1;
}
std::string W3dVistaRecRutaExternaSugerida(int vista, const std::string& id) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    return prov ? prov->RutaExternaSugerida(id) : std::string();
}
bool W3dVistaRecFijarUbicacion(int vista, const std::string& id, int ubicacion, const std::string& rutaExterna,
                               std::string* idNuevo, std::string* motivo) {
    W3dProveedorRecursos* prov = W3dRecursosVistaProveedor(vista);
    if (!prov) return false;
    SinRaiz sr;
    const bool ok = prov->FijarUbicacion(id, ubicacion, rutaExterna, idNuevo, motivo);
    if (ok) W3dRecursosVistaInvalidar();
    return ok;
}

// ============================================================================
//  EL RECURSO ACTIVO
// ============================================================================
static int          gActVista = -1;
static std::string  gActId;
static unsigned int gActObjSerial = 0;   // el objeto activo cuando se eligio el recurso
// W3dSeleccionSerial cuando se eligio: si la escena ELIGE un objeto despues, el recurso se suelta aunque el
// objeto activo sea el mismo (un recurso elegido en Edit Mode, con la malla en edicion todavia activa)
static unsigned int gActSelSerial = 0;
static unsigned     gActVersion = 0;
static Texture*     gPrevTex = NULL;     // la vista previa que se CARGO para el activo (se suelta)
static std::string  gPrevFallo;          // la clave que no cargo (no reintentar por cuadro)

// algun material (vivo, purgado o animado) guarda ese puntero
static bool TexturaEnMateriales(const Texture* t) {
    for (size_t i = 0; i < Materials.size(); i++) if (MaterialNombraTextura(Materials[i], t)) return true;
    const std::vector<Material*>& mp = MaterialesPurgados();
    for (size_t i = 0; i < mp.size(); i++) if (MaterialNombraTextura(mp[i], t)) return true;
    for (size_t i = 0; i < AnimatedMaterials.size(); i++)
        if (AnimatedMaterials[i])
            for (size_t f = 0; f < AnimatedMaterials[i]->frameTextures.size(); f++)
                if (AnimatedMaterials[i]->frameTextures[f] == t) return true;
    return false;
}
static void SoltarVistaPrevia() {
    if (gPrevTex) {
        // sigue siendo del cache? (la pudo liberar el cierre del proyecto antes de llegar aca)
        bool viva = false;
        for (size_t i = 0; i < Textures.size() && !viva; i++) viva = (Textures[i] == gPrevTex);
        if (viva) {
            // la referencia de la VISTA PREVIA es suya: se suelta. Si alguien mas la tiene (la ranura de un material
            // que la tomo mientras se miraba, otro pedido) solo baja una; si era la unica y nadie la nombra, sale de la
            // GPU al cementerio (el objeto queda para cualquier puntero viejo). Si era la unica pero una ranura la nombra
            // sin referencia propia (un camino viejo), esa referencia pasa a ser de la ranura
            W3dRecurso* r = W3dRecursoBuscar(W3DREC_TEXTURA, gPrevTex->path);
            const int refs = (r && r->dato == gPrevTex) ? r->refTotal : 0;
            if (refs > 1) TexturaSoltar(gPrevTex);
            else if (TexturaEnMateriales(gPrevTex)) { /* la ranura se queda con ella */ }
            else TexturaPurgar(gPrevTex);
        }
    }
    gPrevTex = NULL;
    gPrevFallo.clear();
}
// la vista previa del activo ES esa textura (la cargo solo el panel para mirarla): se suelta
static void SoltarVistaPreviaDe(const std::string& clave) {
    if (gPrevTex && ClaveTextura(gPrevTex->path) == clave) SoltarVistaPrevia();
}
// las entradas cambiaron de nombre: la que no cargo puede cargar ahora
static void OlvidarFalloVistaPrevia() { gPrevFallo.clear(); }
void W3dRecursoActivar(int vista, const std::string& id) {
    if (vista != gActVista || id != gActId) { SoltarVistaPrevia(); gActVersion++; }
    gActVista = vista;
    gActId = id;
    gActObjSerial = ObjActivo ? ObjActivo->serial : 0;
    gActSelSerial = W3dSeleccionSerial;
}
void W3dRecursoDesactivar() {
    if (gActVista >= 0) gActVersion++;
    SoltarVistaPrevia();
    gActVista = -1;
    gActId.clear();
}
unsigned W3dRecursoActivoVersion() { return gActVersion; }
bool W3dRecursoActivo(int* vista, std::string* id) {
    if (gActVista < 0) return false;
    // el usuario volvio a la escena (eligio un objeto, aunque sea el que seguia activo en Edit Mode, u
    // otro objeto quedo activo): el panel vuelve a mostrar objetos
    if ((ObjActivo ? ObjActivo->serial : 0) != gActObjSerial || W3dSeleccionSerial != gActSelSerial) {
        W3dRecursoDesactivar();
        return false;
    }
    if (!W3dVistaRecInfo(gActVista, gActId, NULL)) { W3dRecursoDesactivar(); return false; }
    if (vista) *vista = gActVista;
    if (id) *id = gActId;
    return true;
}
void W3dRecursoActivoRenombrado(int vista, const std::string& viejo, const std::string& nuevo) {
    if (vista == gActVista && viejo == gActId && viejo != nuevo) { gActId = nuevo; gActVersion++; }
}
Material* W3dRecursoActivoMaterial() {
    int v; std::string id;
    if (!W3dRecursoActivo(&v, &id) || v != W3D_VISTA_MATERIALES) return NULL;
    return MaterialDeNombre(id);
}
MallaRecurso* W3dRecursoActivoMalla() {
    int v; std::string id;
    if (!W3dRecursoActivo(&v, &id) || v != W3D_VISTA_MALLAS) return NULL;
    return W3dMallaRecursoPorNombre(id);
}
Texture* W3dTexturaCargadaDe(const std::string& id) { return TexturaCargadaDe(ClaveTextura(id)); }
std::string W3dTexturaClave(const std::string& ruta) { return ClaveTextura(ruta); }
Material* W3dMaterialDeId(const std::string& id) { return MaterialDeNombre(id); }
Texture* W3dRecursoActivoTextura() {
    int v; std::string id;
    if (!W3dRecursoActivo(&v, &id) || v != W3D_VISTA_TEXTURAS) return NULL;
    for (size_t i = (size_t)TexturasBase(); i < Textures.size(); i++)
        if (Textures[i] && ClaveTextura(Textures[i]->path) == id) return Textures[i];
    if (gPrevTex) return gPrevTex;
    if (gPrevFallo == id) return NULL;
    // la vista previa de antes de esa misma entrada quedo en el cementerio: vuelve (mismo objeto)
    const std::vector<Texture*>& tp = TexturasPurgadas();
    for (size_t i = 0; i < tp.size(); i++)
        if (tp[i] && ClaveTextura(tp[i]->path) == id && TexturaRevivir(tp[i])) {
            gPrevTex = tp[i];
            // (la de la vista previa + la de cada paso de undo que la guarda: ver RanurasDe)
            for (int k = W3dUndoTexturaDuenos(gPrevTex); k > 0; k--) TexturaRetener(gPrevTex);
            return gPrevTex;
        }
    // no esta cargada (una entrada huerfana): se toma UNA referencia para la vista previa
    gPrevTex = TexturaTomar(id);
    if (!gPrevTex) gPrevFallo = id;
    return gPrevTex;
}

// ============================================================================
//  GUARDADO / CARGA / CIERRE
// ============================================================================
void W3dRecursosVistaGuardarAntes() {
    gTexAntes.clear();
    for (size_t i = (size_t)TexturasBase(); i < Textures.size(); i++) {
        Texture* t = Textures[i];
        if (!t || t->path.empty()) continue;
        const std::string k = ClaveTextura(t->path);
        if (gCarpetaTex.count(k)) gTexAntes.push_back(std::make_pair(t, k));
    }
}

// un string JSON (el mismo escape que el resto del guardado)
static void JEsc(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const unsigned char c = (unsigned char)v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += (char)c; }
        else if (c == '\n') s += "\\n";
        else if (c == '\r') s += "\\r";
        else if (c == '\t') s += "\\t";
        else if (c < 0x20) { char b[8]; sprintf(b, "\\u%04x", (unsigned)c); s += b; }
        else s += (char)c;
    }
    s += '"';
}
static void JsonListaTextos(std::string& s, const std::vector<std::string>& v) {
    s += "[";
    for (size_t i = 0; i < v.size(); i++) { if (i) s += ", "; JEsc(s, W3dRutaCosmeticaJson(v[i])); }
    s += "]";
}

void W3dRecursosVistaGuardarJson(std::string& s, W3dContenedorEscritor* esc) {
    // LA CARPETA SIGUE A SU TEXTURA: la ingesta del guardado cambio la ruta de disco por el
    // nombre de entrada ("/home/.../piso.png" -> "texturas/piso.png")
    for (size_t i = 0; i < gTexAntes.size(); i++) {
        Texture* t = gTexAntes[i].first;
        bool viva = false;
        for (size_t k = 0; k < Textures.size() && !viva; k++) viva = (Textures[k] == t);
        if (!viva) continue;
        const std::string nueva = ClaveTextura(t->path);
        if (nueva == gTexAntes[i].second) continue;
        std::map<std::string, std::string>::iterator c = gCarpetaTex.find(gTexAntes[i].second);
        if (c == gCarpetaTex.end()) continue;
        const std::string carpeta = c->second;
        gCarpetaTex.erase(c);
        gCarpetaTex[nueva] = carpeta;
    }
    gTexAntes.clear();
    // ...y la de disco que NO esta en Textures (la usan solo particulas, elementos 2D, flipbooks):
    // su ruta la reescribio la ingesta de ESE objeto, no la de un material. El escritor sabe a que
    // entrada fue cada ruta de disco que le llego: la carpeta sigue a esa entrada.
    if (esc) {
        std::vector<std::pair<std::string, std::string> > ing;
        esc->Ingeridas(ing);
        std::map<std::string, std::string> aEntrada;   // clave de disco -> clave de su entrada
        for (size_t i = 0; i < ing.size(); i++) aEntrada[ClaveTextura(ing[i].first)] = ClaveTextura(ing[i].second);
        std::vector<std::pair<std::string, std::string> > mudar;   // (clave vieja, clave nueva)
        for (std::map<std::string, std::string>::iterator it = gCarpetaTex.begin(); it != gCarpetaTex.end(); ++it) {
            if (it->second.empty() || W3dEsNombreDeEntrada(it->first)) continue;
            std::map<std::string, std::string>::iterator e = aEntrada.find(it->first);
            if (e != aEntrada.end() && e->second != it->first) mudar.push_back(std::make_pair(it->first, e->second));
        }
        for (size_t i = 0; i < mudar.size(); i++) {
            const std::string carpeta = gCarpetaTex[mudar[i].first];
            gCarpetaTex.erase(mudar[i].first);
            // (si la entrada ya tenia la suya, manda esa: es la que el usuario ve en la vista)
            if (!gCarpetaTex.count(mudar[i].second)) gCarpetaTex[mudar[i].second] = carpeta;
        }
    }
    // "carpetas": el ARBOL UNICO de las carpetas creadas (solo si hay alguna)
    if (!gCreadas.empty()) {
        s += "  \"carpetas\": ";
        JsonListaTextos(s, gCreadas);
        s += ",\n";
    }
    // "texturas": la carpeta de cada textura que la tiene. Solo las que van a estar ADENTRO del
    // archivo: la verificacion del guardado exige que toda entrada nombrada exista.
    W3dZipLector* viejo = W3dContenedorLector();
    std::vector<std::pair<std::string, std::string> > filas;
    for (std::map<std::string, std::string>::iterator it = gCarpetaTex.begin(); it != gCarpetaTex.end(); ++it) {
        if (it->second.empty() || !W3dEsNombreDeEntrada(it->first) || gTexPurgadas.count(it->first)) continue;
        // (la escrita por este guardado, la del contenedor viejo, o la importada en la sesion que
        // PreservarPasajeras conserva)
        const bool va = (esc && esc->Tiene(it->first)) || (viejo && viejo->Existe(it->first)) ||
                        W3dContenedorEntradaEditada(it->first);
        if (va) filas.push_back(*it);
    }
    if (!filas.empty()) {
        s += "  \"texturas\": [\n";
        for (size_t i = 0; i < filas.size(); i++) {
            s += "    { \"entrada\": "; JEsc(s, filas[i].first);
            s += ", \"carpeta\": "; JEsc(s, W3dRutaCosmeticaJson(filas[i].second)); s += " }";
            s += (i + 1 < filas.size()) ? ",\n" : "\n";
        }
        s += "  ],\n";
    }
    // "archivos": la carpeta de cada sonido / script / fuente / video que la tiene (las entradas que van a
    // estar ADENTRO; los de afuera se nombran por su ruta de disco y el guardado los reescribe "ext:")
    std::vector<std::pair<std::string, std::string> > fa;
    for (std::map<std::string, std::string>::iterator it = gCarpetaArch.begin(); it != gCarpetaArch.end(); ++it) {
        if (it->second.empty() || !W3dEsNombreDeEntrada(it->first) || gArchPurgadas.count(it->first)) continue;
        const bool va = (esc && esc->Tiene(it->first)) || (viejo && viejo->Existe(it->first)) ||
                        W3dContenedorEntradaEditada(it->first);
        if (va) fa.push_back(*it);
    }
    if (!fa.empty()) {
        s += "  \"archivos\": [\n";
        for (size_t i = 0; i < fa.size(); i++) {
            s += "    { \"entrada\": "; JEsc(s, fa[i].first);
            s += ", \"carpeta\": "; JEsc(s, W3dRutaCosmeticaJson(fa[i].second)); s += " }";
            s += (i + 1 < fa.size()) ? ",\n" : "\n";
        }
        s += "  ],\n";
    }
    // "externos": lo de la biblioteca que vive AFUERA del .w3d a proposito ("ext:", como cualquier referencia
    // externa: relativa a la carpeta del .w3d si esta abajo), con su carpeta. Los que alguien usa ya van
    // nombrados por su usuario (aca solo si tienen carpeta: "texturas"/"archivos" llevan solo las de adentro);
    // un HUERFANO de afuera no lo nombra nadie y sin esta lista se perdia al volver a abrir. Pasan por la
    // ingesta: quedan en EXTERNOS.txt (y en el aviso del guardado si el archivo falta).
    if (esc) {
        static const int kTipos[5] = { W3D_VISTA_TEXTURAS, W3D_VISTA_SONIDOS, W3D_VISTA_SCRIPTS, W3D_VISTA_FUENTES, W3D_VISTA_VIDEOS };
        std::string ex;
        for (int k = 0; k < 5; k++) {
            std::vector<W3dRecursoItem> its;
            W3dVistaRecListar(kTipos[k], its);
            const std::set<std::string>& pur = PurgadasDe(kTipos[k]);
            for (size_t i = 0; i < its.size(); i++) {
                const W3dRecursoItem& it = its[i];
                if (UbicacionDeRuta(it.id) != 1 || pur.count(it.id)) continue;
                if (it.usuarios > 0 && it.carpeta.empty()) continue;
                std::string ruta = ExternaRutaMarcada(it.id);
                if (ruta.empty()) continue;   // (una ruta de disco que no se eligio afuera: no es de aca)
                const std::string ref = esc->Ingerir(ruta, W3dCategoriaPorExtension(ruta), "biblioteca \"" + it.nombre + "\"");
                if (ref.compare(0, 4, "ext:") != 0) continue;
                ex += ex.empty() ? "" : ",\n";
                ex += "    { \"tipo\": "; JEsc(ex, W3dVistaClave(kTipos[k]));
                ex += ", \"archivo\": "; JEsc(ex, ref);
                if (!it.carpeta.empty()) { ex += ", \"carpeta\": "; JEsc(ex, W3dRutaCosmeticaJson(it.carpeta)); }
                ex += " }";
            }
        }
        if (!ex.empty()) s += "  \"externos\": [\n" + ex + "\n  ],\n";
    }
}

void W3dRecursosVistaGuardarEnMemoria(W3dContenedorEscritor* esc) {
    if (!esc) return;
    for (size_t i = (size_t)TexturasBase(); i < Textures.size(); i++) {
        Texture* t = Textures[i];
        if (!t || t->path.empty() || !W3dEsNombreDeEntrada(t->path)) continue;
        // (la escribio un usuario que la nombra, o se purgo: no es de este camino)
        if (esc->Tiene(t->path) || gTexPurgadas.count(ClaveTextura(t->path))) continue;
        std::string png;
        if (!TexEditBytesParaGuardar(t->path, png)) continue;   // no vive solo en memoria
        if (esc->AgregarBytes(t->path, png, true))
            w3dLogf("[outliner] textura huerfana en memoria guardada: %s (%u bytes)", t->path.c_str(), (unsigned)png.size());
    }
}

void W3dRecursosVistaGuardarDescartes(W3dContenedorEscritor* esc) {
    if (!esc) return;
    // LO QUE UN PASO DE UNDO PUEDE DEVOLVER (ver EntradaRetenida): sus bytes se copian AHORA, con el contenedor
    // viejo todavia montado; despues del guardado esa entrada ya no existe
    for (std::set<EntradaRetenida*>::iterator it = gRetenidas.begin(); it != gRetenidas.end(); ++it) {
        EntradaRetenida* r = *it;
        if (!r->bytes.empty() || (!gTexPurgadas.count(r->entrada) && !gArchPurgadas.count(r->entrada))) continue;
        std::vector<unsigned char> d;
        if (W3dContenedorExiste(r->entrada) && w3dFileSystem::ReadFileBytes(r->entrada, d) && !d.empty()) r->bytes.swap(d);
    }
    for (std::set<std::string>::iterator it = gTexPurgadas.begin(); it != gTexPurgadas.end(); ++it)
        esc->Descartar(*it);
    for (std::set<std::string>::iterator it = gArchPurgadas.begin(); it != gArchPurgadas.end(); ++it)
        esc->Descartar(*it);
}

void W3dRecursosVistaGuardado() {
    // lo purgado ya no esta en el archivo nuevo; el listado del contenedor cambio. Lo purgado de AFUERA
    // tampoco se escribio ("externos"): deja de ser de la biblioteca (lo que todavia nombra un objeto sigue
    // marcado y vuelve por su usuario)
    for (std::set<std::string>::iterator it = gTexPurgadas.begin(); it != gTexPurgadas.end(); ++it) gTexExternasBib.erase(*it);
    for (std::set<std::string>::iterator it = gArchPurgadas.begin(); it != gArchPurgadas.end(); ++it) {
        const int tipo = TipoArchivoDeRuta(*it);
        if (tipo < 0 || UbicacionDeRuta(*it) != 1) continue;
        std::vector<RefArchivo> refs; RefsArchivo(tipo, refs);
        bool nombrado = false;
        for (size_t i = 0; i < refs.size() && !nombrado; i++) nombrado = (ClaveTextura(*refs[i].ruta) == *it);
        if (!nombrado) DesmarcarExterna(*it);
    }
    gTexPurgadas.clear();
    gArchPurgadas.clear();
    W3dRecursosVistaInvalidar();
}

// "ext:x" -> la ruta de disco, como la resuelve el lector del proyecto (RutaJson, import_w3d.cpp): relativa a
// la carpeta del .w3d; absoluta tal cual. "" = no es una referencia externa
static std::string RutaExternaAlAbrir(const std::string& j) {
    if (j.size() <= 4 || j.compare(0, 4, "ext:") != 0) return std::string();
    const std::string x = j.substr(4);
    if (x[0] == '/' || (x.size() > 2 && x[1] == ':')) return x;
    return g_w3dDirProyecto.empty() ? x : g_w3dDirProyecto + "/" + x;
}

void W3dRecursosVistaLeerJson(JVal* raiz) {
    gCreadas.clear();
    gCarpetaTex.clear();
    gCarpetaArch.clear();
    gTexExternasBib.clear();
    if (!raiz) return;
    JVal* jc = JHijo(raiz, "carpetas", 5);
    if (jc) {
        // el arbol UNICO (una lista de rutas)
        for (size_t i = 0; i < jc->lista.size(); i++) {
            JVal* e = jc->lista[i];
            if (e && e->tipo == 2) CreadasAgregar(W3dCarpetaNormalizar(e->str));
        }
    } else if ((jc = JHijo(raiz, "carpetas", 4)) != NULL) {
        // MIGRACION: el formato de la fase anterior (un arbol POR TIPO: {"mallas": [...], "materiales": [...]})
        // se junta en el arbol unico. El proximo guardado escribe el formato nuevo.
        for (std::map<std::string, JVal*>::iterator it = jc->obj.begin(); it != jc->obj.end(); ++it) {
            if (!it->second || it->second->tipo != 5) continue;
            for (size_t i = 0; i < it->second->lista.size(); i++) {
                JVal* e = it->second->lista[i];
                if (e && e->tipo == 2) CreadasAgregar(W3dCarpetaNormalizar(e->str));
            }
        }
        w3dLogf("[biblioteca] carpetas por tipo migradas al arbol unico (%d)", (int)gCreadas.size());
    }
    JVal* jt = JHijo(raiz, "texturas", 5);
    if (jt)
        for (size_t i = 0; i < jt->lista.size(); i++) {
            JVal* e = jt->lista[i];
            if (!e || e->tipo != 4) continue;
            const std::string ent = JS(e, "entrada", ""), car = W3dCarpetaNormalizar(JS(e, "carpeta", ""));
            if (!ent.empty() && !car.empty()) gCarpetaTex[ClaveTextura(ent)] = car;
        }
    JVal* ja = JHijo(raiz, "archivos", 5);
    if (ja)
        for (size_t i = 0; i < ja->lista.size(); i++) {
            JVal* e = ja->lista[i];
            if (!e || e->tipo != 4) continue;
            const std::string ent = JS(e, "entrada", ""), car = W3dCarpetaNormalizar(JS(e, "carpeta", ""));
            if (!ent.empty() && !car.empty()) gCarpetaArch[ClaveTextura(ent)] = car;
        }
    // "externos": lo de AFUERA que es de la biblioteca (queda marcado como cualquier referencia "ext:" al
    // abrir: el proximo guardado lo vuelve a escribir afuera), con su carpeta
    JVal* je = JHijo(raiz, "externos", 5);
    if (je)
        for (size_t i = 0; i < je->lista.size(); i++) {
            JVal* e = je->lista[i];
            if (!e || e->tipo != 4) continue;
            const int tipo = W3dVistaDeClave(JS(e, "tipo", ""));
            const std::string ruta = RutaExternaAlAbrir(JS(e, "archivo", ""));
            if (ruta.empty()) continue;
            const bool esTex = (tipo == W3D_VISTA_TEXTURAS && std::string(W3dCategoriaPorExtension(ruta)) == "texturas");
            if (!esTex && (tipo < 0 || TipoArchivoDeRuta(ruta) != tipo)) continue;   // (un tipo que no es de archivo)
            W3dRefExternaMarcar(ruta);
            const std::string k = ClaveTextura(ruta), car = W3dCarpetaNormalizar(JS(e, "carpeta", ""));
            if (esTex) { gTexExternasBib.insert(k); if (!car.empty()) gCarpetaTex[k] = car; }
            else if (!car.empty()) gCarpetaArch[k] = car;
        }
}

void W3dRecursosVistaCerrarProyecto() {
    W3dRecursoDesactivar();
    W3dMiniaturasOlvidar();   // (las miniaturas de la cuadricula son del proyecto que se cierra)
    // un GRUPO de carpetas abierto (el modo mover de la biblioteca que quedo a medias) es del proyecto que se
    // cierra: su foto nombra carpetas y recursos de este. Se tira sin aplicar nada (si no, ninguna operacion de
    // carpetas volvia a tener undo y el proximo Esc del modo mover metia las carpetas de este en el otro)
    delete gGrupoTx; gGrupoTx = NULL;
    gGrupoProf = 0;
    // (los bytes que retenian los pasos de undo son de este .w3d: no pueden reponerse en el proximo)
    for (std::set<EntradaRetenida*>::iterator it = gRetenidas.begin(); it != gRetenidas.end(); ++it)
        std::vector<unsigned char>().swap((*it)->bytes);
    gCreadas.clear();
    gCarpetaTex.clear();
    gCarpetaArch.clear();
    gTexPurgadas.clear();
    gArchPurgadas.clear();
    gTexAntes.clear();
    gTexExternasBib.clear();
    W3dRecursosVistaInvalidar();
}

void W3dRecursosVistaInvalidar() {
    gInfoArch = InfoArchivo();   // (lo leido del archivo del ultimo recurso mirado)
    gMatsIlegibles.clear();
    gScriptsLeidos = false;
    gScriptsTexto.clear();
    gScriptsNombran.clear();
    gEntradasLeidas = false;
    gEntradasTex.clear();
    gEntradasArchLeidas = false;
    gEntradasArch.clear();
}
