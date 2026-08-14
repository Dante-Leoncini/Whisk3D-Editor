// ============================================================================
//  W3dContenedor.cpp — ver W3dContenedor.h.
// ============================================================================
#include "io/W3dContenedor.h"
#include "io/W3dZip.h"
#include "io/W3dAlmacen.h"
#include "io/UI2DFormato.h"   // W3dRutaBajoCarpeta: LA regla de "esta ruta cuelga de esta carpeta"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
    #include <direct.h>
    #define W3D_MKDIR(p) _mkdir(p)
#else
    #include <sys/stat.h>
    #define W3D_MKDIR(p) mkdir(p, 0755)
#endif

// ---------------------------------------------------------------------------
//  CRC32 (el mismo polinomio del zip): la HUELLA con la que se deduplica por
//  contenido. El del W3dZip.cpp es static; aca hay una copia chica en vez de
//  exportar simbolos nuevos del Core.
// ---------------------------------------------------------------------------
static unsigned gTabla[256];
static bool     gTablaLista = false;
static void TablaCrc() {
    if (gTablaLista) return;
    for (unsigned i = 0; i < 256; i++) {
        unsigned c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        gTabla[i] = c;
    }
    gTablaLista = true;
}
static unsigned Crc32(const unsigned char* d, size_t n) {
    TablaCrc();
    unsigned c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = gTabla[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// ===========================================================================
//  ENTRADAS DE SERVICIO (mimetype / LEEME.txt / proyecto.json / EXTERNOS.txt)
// ===========================================================================
// 36 bytes EXACTOS, sin salto de linea final: es lo que queda en el offset 38
// del zip, que es donde file(1) y xdg-mime buscan la firma de un OpenDocument.
const char* const kW3dMimetype = "application/vnd.whisk3d.proyecto+zip";

bool W3dEsEntradaDeServicio(const std::string& n) {
    return n == "mimetype" || n == "LEEME.txt" ||
           n == "proyecto.json" || n == "EXTERNOS.txt";
}

// TEXTO FIJO: ni fecha, ni usuario, ni rutas de la maquina. Cualquier campo
// volatil aca rompe el round-trip byte a byte del guardado (y con el, el diff
// en git, el dedup entre versiones y los tests).
std::string W3dContenedorLeeme() {
    std::string s;
    s += "Esto es un proyecto de Whisk3D.\n";
    s += "\n";
    s += "El archivo .w3d es un ZIP comun: lo estas mirando con un descompresor y\n";
    s += "eso es a proposito. Adentro esta el proyecto ENTERO y nada mas.\n";
    s += "\n";
    s += "  mimetype        el tipo de archivo. Va primero y sin comprimir (igual\n";
    s += "                  que un .odt de OpenDocument). No lo edites.\n";
    s += "  LEEME.txt       este texto. Se regenera solo en cada guardado.\n";
    s += "  proyecto.json   EL PROYECTO: objetos, escenas, materiales, animaciones.\n";
    s += "                  Es texto y se puede leer y editar a mano.\n";
    s += "  escenas/        las escenas de interfaz (.w3dui)\n";
    s += "  mallas/         la geometria de los objetos 3D\n";
    s += "  animaciones/    los keyframes de vertices (binario)\n";
    s += "  scripts/        el codigo Lua\n";
    s += "  texturas/  fuentes/  sonidos/  videos/    los assets, por tipo\n";
    s += "  modelos/        los .obj y .fbx originales que importaste\n";
    s += "  proyecto/       el icono del juego y su arte fuente\n";
    s += "  extra/          lo que el editor no reconocio\n";
    s += "  EXTERNOS.txt    aparece SOLO si el proyecto usa archivos de afuera\n";
    s += "\n";
    s += "Cada carpeta dice QUE TIPO de cosa hay adentro, no de quien es: una\n";
    s += "textura usada por tres escenas es UNA sola entrada.\n";
    s += "\n";
    s += "Si agregas archivos tuyos aca adentro, Whisk3D los deja donde estan (no\n";
    s += "los borra al guardar), salvo en mallas/ y animaciones/, que son del\n";
    s += "editor y ahi lo que no se usa se descarta.\n";
    return s;
}

// ===========================================================================
//  MONTAJE
// ===========================================================================
// ---------------------------------------------------------------------------
//  EL ALMACEN MONTADO = el zip del disco MAS las entradas editadas en caliente.
//
//  Sin esta capa, "guardar el .lua que estas editando" no tenia a donde ir: el
//  IDE hacia fopen("scripts/menu.lua") y eso crea un archivo RELATIVO AL
//  DIRECTORIO ACTUAL (una carpeta scripts/ al lado del binario del editor), o sea
//  que el cambio no llegaba nunca al proyecto y encima ensuciaba el disco.
//  Con el overlay, el zip sigue siendo de solo lectura y lo editado se sirve de
//  memoria: todo el editor lo ve por ReadFileBytes sin cambiar un solo call site.
// ---------------------------------------------------------------------------
class AlmacenContenedor : public W3dAlmacen {
public:
    virtual bool Abrir(const std::string& ruta) { return zip.Abrir(ruta); }
    virtual bool Existe(const std::string& e) const {
        return editadas.find(e) != editadas.end() || zip.Existe(e);
    }
    virtual bool Leer(const std::string& e, std::vector<unsigned char>& out) {
        std::map<std::string, std::vector<unsigned char> >::const_iterator it = editadas.find(e);
        if (it != editadas.end()) { out = it->second; return true; }
        return zip.Leer(e, out);
    }
    virtual void Listar(std::vector<std::string>& out) const {
        zip.Listar(out);
        for (std::map<std::string, std::vector<unsigned char> >::const_iterator it = editadas.begin();
             it != editadas.end(); ++it) {
            bool ya = false;
            for (size_t i = 0; i < out.size() && !ya; i++) if (out[i] == it->first) ya = true;
            if (!ya) out.push_back(it->first);
        }
    }
    virtual unsigned Tam(const std::string& e) const {
        std::map<std::string, std::vector<unsigned char> >::const_iterator it = editadas.find(e);
        if (it != editadas.end()) return (unsigned)it->second.size();
        return zip.Tam(e);
    }
    virtual void Cerrar() { zip.Cerrar(); editadas.clear(); }

    AlmacenZip zip;
    std::map<std::string, std::vector<unsigned char> > editadas;
};

static AlmacenContenedor* gAlmacen = NULL;
static std::string  gRutaMontada;

bool W3dContenedorHayMontado() { return gAlmacen != NULL; }
W3dZipLector* W3dContenedorLector() { return gAlmacen ? &gAlmacen->zip.Lector() : NULL; }

bool W3dContenedorEscribirEntrada(const std::string& entrada, const void* datos, size_t tam) {
    if (!gAlmacen || !W3dEsNombreDeEntrada(entrada)) return false;
    std::vector<unsigned char>& d = gAlmacen->editadas[entrada];
    d.assign((const unsigned char*)datos, (const unsigned char*)datos + tam);
    w3dLogf("[W3D] entrada editada en memoria: %s (%u bytes; se hornea al guardar el proyecto)",
            entrada.c_str(), (unsigned)tam);
    return true;
}
bool W3dContenedorEntradaEditada(const std::string& entrada) {
    return gAlmacen && gAlmacen->editadas.find(entrada) != gAlmacen->editadas.end();
}
bool W3dContenedorLeerEditada(const std::string& entrada, std::vector<unsigned char>& out) {
    if (!gAlmacen) return false;
    std::map<std::string, std::vector<unsigned char> >::const_iterator it =
        gAlmacen->editadas.find(entrada);
    if (it == gAlmacen->editadas.end()) return false;
    out = it->second;
    return true;
}
size_t W3dContenedorEditadasCantidad() { return gAlmacen ? gAlmacen->editadas.size() : 0; }

void W3dContenedorListarCarpeta(const std::string& carpeta, std::vector<std::string>& out) {
    out.clear();
    if (!gAlmacen) return;
    std::vector<std::string> todas;
    gAlmacen->Listar(todas);
    for (size_t i = 0; i < todas.size(); i++)
        if (todas[i].size() > carpeta.size() &&
            todas[i].compare(0, carpeta.size(), carpeta) == 0)
            out.push_back(todas[i]);
    // ORDEN ALFABETICO: la lista que ve el usuario no puede depender del orden del zip
    for (size_t i = 0; i + 1 < out.size(); i++)
        for (size_t k = i + 1; k < out.size(); k++)
            if (out[k] < out[i]) { std::string t = out[i]; out[i] = out[k]; out[k] = t; }
}

// crea la ruta de carpetas completa ("a/b/c"): mkdir por segmento (existente = ok)
static void MkdirRec(const std::string& dir) {
    for (size_t i = 1; i < dir.size(); i++)
        if (dir[i] == '/' || dir[i] == '\\') W3D_MKDIR(dir.substr(0, i).c_str());
    W3D_MKDIR(dir.c_str());
}

bool W3dContenedorExtraer(const std::string& entrada, const std::string& rutaDisco) {
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(entrada, datos)) {
        w3dLogfE("[W3D] extraer: no pude leer la entrada %s", entrada.c_str());
        return false;
    }
    // la carpeta del destino ("<dir>/texturas") se crea sola: el arbol de adentro
    // del .w3d se reproduce tal cual afuera
    { size_t sb = rutaDisco.find_last_of("/\\");
      if (sb != std::string::npos && sb > 0) MkdirRec(rutaDisco.substr(0, sb)); }
    // ATOMICO igual que todo lo que escribe este editor: primero el temporal
    // completo, despues UN rename. Un fallo a mitad no deja un archivo mocho.
    std::string tmp = rutaDisco + ".w3dtmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { w3dLogfE("[W3D] extraer: no pude escribir %s", tmp.c_str()); return false; }
    size_t esc = datos.empty() ? 0 : fwrite(&datos[0], 1, datos.size(), f);
    bool ok = (fclose(f) == 0) && esc == datos.size();
    if (!ok) { remove(tmp.c_str()); return false; }
    remove(rutaDisco.c_str());   // en Windows rename no pisa; en POSIX es inofensivo
    if (rename(tmp.c_str(), rutaDisco.c_str()) != 0) { remove(tmp.c_str()); return false; }
    w3dLogf("[W3D] extraido %s -> %s", entrada.c_str(), rutaDisco.c_str());
    return true;
}

void W3dContenedorDesmontar() {
    if (!gAlmacen) return;
    W3dAlmacenDesmontar();
    delete gAlmacen;
    gAlmacen = NULL;
    gRutaMontada.clear();
}

bool W3dContenedorMontar(const std::string& rutaW3d) {
    W3dContenedorDesmontar();
    AlmacenContenedor* a = new AlmacenContenedor();
    if (!a->Abrir(rutaW3d)) { delete a; return false; }
    // EL QUE MANDA para decidir si es un proyecto es proyecto.json, NO el
    // mimetype: un .w3d escrito por el editor de antes del estilo OpenDocument
    // no lo tiene, y tiene que abrir igual (se migra en el primer guardado).
    if (!a->Existe("proyecto.json")) {
        w3dLogfW("[W3D] %s es un zip pero no trae proyecto.json (no es un contenedor v4)", rutaW3d.c_str());
        delete a;
        return false;
    }
    {
        std::vector<std::string> ents;
        a->zip.Lector().Listar(ents);
        if (ents.empty() || ents[0] != "mimetype") {
            w3dLogf("[W3D] %s no trae la entrada 'mimetype' primera (formato anterior): "
                    "se migra al primer guardado", rutaW3d.c_str());
        } else {
            std::vector<unsigned char> mt;
            if (a->zip.Lector().Leer("mimetype", mt)) {
                std::string s(mt.begin(), mt.end());
                if (s != kW3dMimetype)
                    w3dLogfW("[W3D] %s dice ser '%s' y no '%s': se abre igual",
                             rutaW3d.c_str(), s.c_str(), kW3dMimetype);
            }
        }
    }
    gAlmacen = a;
    gRutaMontada = rutaW3d;
    W3dAlmacenMontar(a);
    w3dLogf("[W3D] contenedor montado: %s", rutaW3d.c_str());
    return true;
}

bool W3dContenedorEs(const std::string& ruta) {
    if (gAlmacen && gRutaMontada == ruta) return true;
    W3dZipLector z;
    if (!z.Abrir(ruta)) return false;
    bool si = z.Existe("proyecto.json");
    z.Cerrar();
    return si;
}

// ---------------------------------------------------------------------------
//  IMPORTAR: la copia adentro pasa AHORA, no al guardar (ver W3dContenedor.h)
// ---------------------------------------------------------------------------
std::string W3dImportarAsset(const std::string& rutaDisco, const char* categoria) {
    if (rutaDisco.empty()) return rutaDisco;
    if (!gAlmacen) return rutaDisco;                       // proyecto v3: sigue suelto
    if (W3dEsNombreDeEntrada(rutaDisco)) return rutaDisco; // ya esta adentro
    if (W3dRefEsExterna(rutaDisco)) return rutaDisco;      // el usuario la quiso afuera
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(rutaDisco, datos)) {
        // NO se inventa nada: si no se puede leer, la ref queda como estaba y el
        // guardado la va a registrar como externa que FALTA (con aviso).
        w3dLogfW("[W3D] importar: no pude leer %s (queda como referencia externa)", rutaDisco.c_str());
        return rutaDisco;
    }
    if (!categoria || !*categoria) categoria = W3dCategoriaPorExtension(rutaDisco);
    const unsigned crc = datos.empty() ? 0u : Crc32(&datos[0], datos.size());
    const unsigned tam = (unsigned)datos.size();
    // DEDUP POR CONTENIDO: la misma imagen importada dos veces es UNA entrada
    // (misma regla que el guardado; si no, cada "Cargar textura" engordaba el .w3d)
    {
        std::vector<std::string> todas;
        gAlmacen->Listar(todas);
        for (size_t i = 0; i < todas.size(); i++) {
            if (gAlmacen->Tam(todas[i]) != tam) continue;
            std::vector<unsigned char> otro;
            if (!gAlmacen->Leer(todas[i], otro) || otro.size() != datos.size()) continue;
            unsigned c2 = otro.empty() ? 0u : Crc32(&otro[0], otro.size());
            if (c2 == crc && otro == datos) return todas[i];
        }
    }
    // nombre de entrada libre ("texturas/piso.png", "texturas/piso-2.png"...)
    std::string base = rutaDisco, ext;
    { size_t s = base.find_last_of("/\\"); if (s != std::string::npos) base = base.substr(s + 1); }
    size_t p = base.find_last_of('.');
    if (p != std::string::npos && p > 0) { ext = base.substr(p); base = base.substr(0, p); }
    std::string raiz = W3dSlugEntrada(base);
    std::string sufijo = ext.empty() ? std::string() : W3dSlugEntrada(ext);
    if (!sufijo.empty() && sufijo[0] != '.') sufijo = "." + sufijo;
    std::string cand = std::string(categoria) + "/" + raiz + sufijo;
    for (int n = 2; gAlmacen->Existe(cand); n++) {
        char s[16]; snprintf(s, sizeof(s), "-%d", n);
        cand = std::string(categoria) + "/" + raiz + s + sufijo;
    }
    gAlmacen->editadas[cand] = datos;
    w3dLogf("[W3D] importado %s -> %s (%u bytes, adentro del proyecto)",
            rutaDisco.c_str(), cand.c_str(), tam);
    return cand;
}

// ===========================================================================
//  REFERENCIAS EXTERNAS
// ===========================================================================
static std::set<std::string> gExternas;

// TEST del harness (ver W3dContenedor.h)
bool g_w3dContOlvidarUna = false;

void W3dRefExternaMarcar(const std::string& r) { if (!r.empty()) gExternas.insert(r); }
bool W3dRefEsExterna(const std::string& r)     { return !r.empty() && gExternas.count(r) != 0; }
void W3dRefExternasLimpiar()                   { gExternas.clear(); }
void W3dRefExternasListar(std::vector<std::string>* out) {
    if (!out) return;
    for (std::set<std::string>::const_iterator it = gExternas.begin(); it != gExternas.end(); ++it)
        out->push_back(*it);
}

// ===========================================================================
//  NOMBRES DE ENTRADA
// ===========================================================================
static const char* const kCarpetas[] = {
    "escenas/", "scripts/", "texturas/", "fuentes/", "sonidos/", "videos/",
    "mallas/", "animaciones/", "modelos/", "proyecto/", "extra/", 0
};

bool W3dEsNombreDeEntrada(const std::string& r) {
    if (r.empty() || r[0] == '/' || r[0] == '\\') return false;
    if (r.size() > 1 && r[1] == ':') return false;                 // "C:\..." o "ext:..."
    if (r.compare(0, 13, "proyecto.json") == 0 && r.size() == 13) return true;
    for (int i = 0; kCarpetas[i]; i++) {
        size_t n = strlen(kCarpetas[i]);
        if (r.size() > n && r.compare(0, n, kCarpetas[i]) == 0) return true;
    }
    return false;
}

// translitera un byte UTF-8 conocido a ASCII. Devuelve cuantos bytes consumio
// (0 = no es una secuencia que conozcamos).
static int Translitera(const std::string& s, size_t i, char* out) {
    unsigned char a = (unsigned char)s[i];
    if (a < 0x80) { *out = (char)a; return 1; }
    if (i + 1 >= s.size()) return 0;
    unsigned char b = (unsigned char)s[i + 1];
    unsigned cp = 0;
    if ((a & 0xE0) == 0xC0) cp = ((unsigned)(a & 0x1F) << 6) | (b & 0x3F);
    else return 0;   // 3+ bytes: no hay tabla, cae al '_'
    switch (cp) {
        case 0xE1: case 0xE0: case 0xE4: case 0xE2: case 0xC1: case 0xC0: case 0xC4: case 0xC2: *out='a'; return 2;
        case 0xE9: case 0xE8: case 0xEB: case 0xEA: case 0xC9: case 0xC8: case 0xCB: case 0xCA: *out='e'; return 2;
        case 0xED: case 0xEC: case 0xEF: case 0xEE: case 0xCD: case 0xCC: case 0xCF: case 0xCE: *out='i'; return 2;
        case 0xF3: case 0xF2: case 0xF6: case 0xF4: case 0xD3: case 0xD2: case 0xD6: case 0xD4: *out='o'; return 2;
        case 0xFA: case 0xF9: case 0xFC: case 0xFB: case 0xDA: case 0xD9: case 0xDC: case 0xDB: *out='u'; return 2;
        case 0xF1: case 0xD1: *out='n'; return 2;
        case 0xE7: case 0xC7: *out='c'; return 2;
        case 0xBF: case 0xA1: *out=0;   return 2;   // '¿' y '¡' se van
    }
    return 0;
}

std::string W3dSlugEntrada(const std::string& nombre) {
    std::string s;
    for (size_t i = 0; i < nombre.size(); ) {
        char c = 0;
        int usados = Translitera(nombre, i, &c);
        if (usados == 0) { s += '_'; i++; continue; }
        i += usados;
        if (c == 0) continue;                       // caracter que se descarta
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '-' || c == '_';
        s += ok ? c : '_';
    }
    // colapsar '_' repetidos
    std::string t;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '_' && !t.empty() && t[t.size() - 1] == '_') continue;
        t += s[i];
    }
    // sacar '.', '_' y '-' de los bordes
    size_t ini = 0, fin = t.size();
    while (ini < fin && (t[ini] == '.' || t[ini] == '_' || t[ini] == '-')) ini++;
    while (fin > ini && (t[fin-1] == '.' || t[fin-1] == '_' || t[fin-1] == '-')) fin--;
    t = t.substr(ini, fin - ini);
    if (t.empty()) t = "sin_nombre";
    // nombres reservados de Windows (con o sin extension)
    {
        std::string raiz = t;
        size_t p = raiz.find('.');
        if (p != std::string::npos) raiz = raiz.substr(0, p);
        static const char* const res[] = { "con","prn","aux","nul",
            "com1","com2","com3","com4","com5","com6","com7","com8","com9",
            "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9", 0 };
        for (int i = 0; res[i]; i++) if (raiz == res[i]) { t = "_" + t; break; }
    }
    if (t.size() > 60) t = t.substr(0, 60);
    return t;
}

const char* W3dCategoriaPorExtension(const std::string& r) {
    size_t p = r.find_last_of('.');
    if (p == std::string::npos) return "extra";
    std::string e = r.substr(p + 1);
    for (size_t i = 0; i < e.size(); i++) if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] + 32);
    if (e == "w3dui") return "escenas";
    if (e == "lua" || e == "luac") return "scripts";
    if (e == "png" || e == "jpg" || e == "jpeg" || e == "webp" || e == "gif" ||
        e == "svg" || e == "bmp" || e == "tga") return "texturas";
    if (e == "ttf" || e == "otf" || e == "w3dfnt") return "fuentes";
    if (e == "ogg" || e == "wav" || e == "mp3") return "sonidos";
    if (e == "mp4" || e == "webm" || e == "mov" || e == "avi") return "videos";
    // "w3dm" es el formato de geometria PROPIO (texto por lineas); glb/gltf
    // quedan por los proyectos guardados antes de que existiera
    if (e == "w3dm" || e == "glb" || e == "gltf" || e == "w3dmesh") return "mallas";
    if (e == "bin" || e == "w3danim") return "animaciones";
    if (e == "obj" || e == "wobj" || e == "fbx" || e == "mtl") return "modelos";
    return "extra";
}

// ===========================================================================
//  ESCRITOR
// ===========================================================================
static std::string CarpetaDe(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : r.substr(0, s);
}
static std::string BaseDe(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? r : r.substr(s + 1);
}
static bool EsAbsoluta(const std::string& r) {
    return !r.empty() && (r[0] == '/' || r[0] == '\\' || (r.size() > 2 && r[1] == ':'));
}

W3dContenedorEscritor::W3dContenedorEscritor() : viejo(NULL), contadorTmp(0) {}
W3dContenedorEscritor::~W3dContenedorEscritor() { LimpiarTemporales(); }

void W3dContenedorEscritor::Iniciar(const std::string& rutaFinal, W3dZipLector* v) {
    rutaDestino = rutaFinal;
    dirDestino  = CarpetaDe(rutaFinal);
    viejo       = v;
    contadorTmp = 0;
    pend.clear(); porNombre.clear(); porHuella.clear();
    yaResueltas.clear(); externos.clear(); olvidadas.clear();
}

std::string W3dContenedorEscritor::RutaTemporal(const char* etiqueta) {
    char b[64];
    snprintf(b, sizeof(b), ".%s%d.w3dtmp", etiqueta ? etiqueta : "t", contadorTmp++);
    return rutaDestino + b;
}

bool W3dContenedorEscritor::Tiene(const std::string& nombre) const {
    return porNombre.find(nombre) != porNombre.end();
}

// "texturas/pausa.png" libre, o "texturas/pausa-2.png" si ese nombre ya lo ocupa
// OTRO contenido (el sufijo es deliberadamente distinto del ".001" del editor:
// si ves "-2" sabes que es una desambiguacion de ARCHIVO, no un nombre de objeto)
std::string W3dContenedorEscritor::NombreLibre(const char* categoria, const std::string& base) {
    std::string b = base;
    std::string ext;
    size_t p = b.find_last_of('.');
    if (p != std::string::npos && p > 0) { ext = b.substr(p); b = b.substr(0, p); }
    std::string raiz = W3dSlugEntrada(b);
    std::string sufijo = ext.empty() ? std::string() : W3dSlugEntrada(ext);
    if (!sufijo.empty() && sufijo[0] != '.') sufijo = "." + sufijo;
    std::string cand = std::string(categoria) + "/" + raiz + sufijo;
    for (int n = 2; Tiene(cand); n++) {
        char s[16]; snprintf(s, sizeof(s), "-%d", n);
        cand = std::string(categoria) + "/" + raiz + s + sufijo;
    }
    return cand;
}

// ---------------------------------------------------------------------------
//  DEDUP POR CONTENIDO DE VERDAD (no por huella)
//
//  La huella "crc:tam" es un INDICE para no comparar contra todas las entradas;
//  la decision la toma la comparacion BYTE A BYTE. CRC32 son 32 bits y ademas es
//  LINEAL: fabricar dos archivos distintos del mismo tamano con el mismo CRC es
//  aritmetica de secundaria, no suerte. Con el dedup por huella sola, esos dos
//  archivos colapsaban en UNO y el proyecto abria con una textura donde iba otra
//  (o con el .lua de otra escena). El otro camino del mismo repo ya lo hacia
//  bien: W3dImportarAsset compara los bytes.
// ---------------------------------------------------------------------------
bool W3dContenedorEscritor::BytesDeEntrada(size_t idx, std::vector<unsigned char>& out) const {
    out.clear();
    if (idx >= pend.size()) return false;
    const W3dEntradaPend& e = pend[idx];
    if (e.origen == 0) { out = e.bytes; return true; }
    if (e.origen == 1) return w3dFileSystem::ReadFileBytes(e.fuente, out);
    if (e.origen == 2 && viejo) return viejo->Leer(e.fuente.empty() ? e.nombre : e.fuente, out);
    return false;
}

size_t W3dContenedorEscritor::BuscarPorContenido(const std::string& huella,
                                                 const std::vector<unsigned char>& datos) const {
    std::map<std::string, std::vector<size_t> >::const_iterator it = porHuella.find(huella);
    if (it == porHuella.end()) return (size_t)-1;
    for (size_t k = 0; k < it->second.size(); k++) {
        size_t idx = it->second[k];
        if (idx >= pend.size() || pend[idx].tam != (unsigned)datos.size()) continue;
        std::vector<unsigned char> otros;
        if (!BytesDeEntrada(idx, otros)) continue;   // no se pudo releer: NO se aliasa
        if (otros == datos) return idx;
    }
    return (size_t)-1;
}

void W3dContenedorEscritor::RegistrarExterno(const std::string& ref, const std::string& quien, bool existe) {
    for (size_t i = 0; i < externos.size(); i++)
        if (externos[i].ref == ref) return;
    W3dExterno e; e.ref = ref; e.quien = quien; e.existe = existe;
    externos.push_back(e);
}

// ver el comentario del .h: una referencia que es el PREFIJO de una secuencia no se
// puede comprobar con FileExists, y el que sabe cual es el archivo real lo dice aca.
void W3dContenedorEscritor::ExternoCorregirExiste(const std::string& ref, bool existe) {
    for (size_t i = 0; i < externos.size(); i++)
        if (externos[i].ref == ref) { externos[i].existe = existe; return; }
}

std::string W3dContenedorEscritor::Ingerir(std::string& ruta, const char* categoria,
                                           const std::string& quien) {
    if (ruta.empty()) return ruta;
    std::map<std::string, std::string>::iterator ya = yaResueltas.find(ruta);
    if (ya != yaResueltas.end()) {
        std::string escrito = ya->second;
        // la ruta EN MEMORIA queda en el nombre de entrada solo si es interna
        if (escrito.compare(0, 4, "ext:") != 0) ruta = escrito;
        return escrito;
    }
    const std::string original = ruta;
    if (!categoria || !*categoria) categoria = W3dCategoriaPorExtension(ruta);

    // --- 1) EXTERNA DELIBERADA: la ref vino con "ext:" y asi se re-escribe ---
    if (W3dRefEsExterna(ruta)) {
        std::string rel = ruta, tmp;
        if (W3dRutaBajoCarpeta(ruta, dirDestino, tmp)) rel = tmp;
        std::string escrito = "ext:" + rel;
        bool hay = w3dFileSystem::FileExists(ruta);
        RegistrarExterno(escrito, quien, hay);
        yaResueltas[original] = escrito;
        return escrito;
    }

    // --- 1.2) IDEMPOTENCIA: LA MISMA RUTA, YA REESCRITA, LLEGANDO OTRA VEZ ---
    //  Ingerir() deja 'ruta' con el NOMBRE DE ENTRADA, y ese std::string puede
    //  estar COMPARTIDO entre varios duenos: el cache de texturas por ruta le da
    //  UN solo Texture* a todos los materiales que nombran el mismo png, asi que
    //  un .mtl con dos materiales y el mismo `map_Kd` (caballero / caballero-
    //  sombra) manda la MISMA cadena dos veces: la primera vuelve como
    //  "texturas/caballero.png"... y la segunda ENTRA con ese valor.
    //  Sin esto: 'yaResueltas' se busca por la ruta ORIGINAL y falla, no hay
    //  contenedor viejo donde mirar (primer guardado desde el .w3d de texto),
    //  ReadFileBytes no encuentra "texturas/caballero.png" en disco y la textura
    //  quedaba anotada como EXTERNA QUE FALTA. Resultado: del SEGUNDO material
    //  en adelante se perdia la textura (7 materiales en el caso medido).
    //  El nombre de entrada YA ingerido se resuelve a si mismo -- pero SOLO si de
    //  verdad es el mismo archivo: un nombre generado ("texturas/caballero.png")
    //  puede coincidir con un archivo REAL del disco que no tiene nada que ver, y
    //  ese caso tiene que seguir por el camino normal (NombreLibre le da "-2").
    if (Tiene(ruta)) {
        std::vector<unsigned char> enDisco, enEntrada;
        bool hayEnDisco = w3dFileSystem::ReadFileBytes(ruta, enDisco);
        if (!hayEnDisco ||
            (BytesDeEntrada(porNombre[ruta], enEntrada) && enEntrada == enDisco)) {
            yaResueltas[original] = ruta;
            return ruta;
        }
    }

    // --- 1.5) ENTRADA EDITADA EN CALIENTE (el IDE guardo este .lua): van los
    //          bytes NUEVOS. Si esto faltara, el reenvasado byte a byte del paso
    //          siguiente copiaria los del zip VIEJO y guardar el proyecto tiraria
    //          silenciosamente lo que el usuario acaba de escribir.
    if (W3dEsNombreDeEntrada(ruta) && W3dContenedorEntradaEditada(ruta)) {
        if (!Tiene(ruta)) {
            std::vector<unsigned char> d;
            W3dContenedorLeerEditada(ruta, d);
            W3dEntradaPend e;
            e.nombre = ruta; e.origen = 0; e.bytes = d;
            e.tam = (unsigned)d.size();
            e.crc = d.empty() ? 0u : Crc32(&d[0], d.size());
            porNombre[e.nombre] = pend.size();
            char h[40]; snprintf(h, sizeof(h), "%08x:%u", e.crc, e.tam);
            porHuella[h].push_back(pend.size());
            pend.push_back(e);
        }
        yaResueltas[original] = ruta;
        return ruta;
    }

    // --- 2) YA ES UNA ENTRADA del contenedor viejo: se reenvasa tal cual ---
    if (W3dEsNombreDeEntrada(ruta) && viejo && viejo->Existe(ruta)) {
        // TEST: "olvidarse" de esta entrada (el JSON la va a nombrar igual). Lo tiene
        // que atajar Verificar() ANTES de renombrar nada.
        if (g_w3dContOlvidarUna) {
            g_w3dContOlvidarUna = false;
            olvidadas.insert(ruta);   // ...y que tampoco la salve la preservacion
            yaResueltas[original] = ruta;
            return ruta;
        }
        if (!Tiene(ruta)) {
            W3dEntradaPend e;
            e.nombre = ruta; e.origen = 2; e.fuente = ruta;
            e.tam = viejo->Tam(ruta);
            std::vector<unsigned char> crudo; unsigned met = 0, crc = 0, olen = 0;
            if (viejo->LeerCrudo(ruta, crudo, &met, &crc, &olen)) { e.crc = crc; e.tam = olen; }
            porNombre[e.nombre] = pend.size();
            char h[40]; snprintf(h, sizeof(h), "%08x:%u", e.crc, e.tam);
            porHuella[h].push_back(pend.size());
            pend.push_back(e);
        }
        yaResueltas[original] = ruta;
        return ruta;
    }

    // --- 2.6) HERMANO DERIVADO de una entrada, clasificado en OTRA carpeta ---
    // El guardado de una fuenteBitmap deriva el .json de glifos del nombre del
    // png ("texturas/x.json"), pero el contenedor clasifica los .json bajo
    // "extra/": el nombre derivado no existe ni como entrada ni en disco y el
    // re-guardado lo anotaba como referencia externa FALTA -> el round-trip
    // dejaba de ser byte a byte. Misma tolerancia que el LECTOR (Fuente2D.cpp,
    // busca "extra/<base>"): se reenvasa ESA entrada bajo su nombre real.
    if (W3dEsNombreDeEntrada(ruta) && viejo && !viejo->Existe(ruta)) {
        std::string alt = "extra/" + BaseDe(ruta);
        if (alt != ruta && viejo->Existe(alt)) {
            if (!Tiene(alt)) {
                W3dEntradaPend e;
                e.nombre = alt; e.origen = 2; e.fuente = alt;
                e.tam = viejo->Tam(alt);
                std::vector<unsigned char> crudo; unsigned met = 0, crc2 = 0, olen = 0;
                if (viejo->LeerCrudo(alt, crudo, &met, &crc2, &olen)) { e.crc = crc2; e.tam = olen; }
                porNombre[e.nombre] = pend.size();
                char h2[40]; snprintf(h2, sizeof(h2), "%08x:%u", e.crc, e.tam);
                porHuella[h2].push_back(pend.size());
                pend.push_back(e);
            }
            yaResueltas[original] = alt;
            return alt;
        }
    }

    // --- 3) ARCHIVO DE DISCO (o entrada del contenedor montado): se INGIERE ---
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(ruta, datos)) {
        // referencia ROTA: NUNCA se borra (un disco desmontado o un pendrive
        // sacado no puede convertirse en perdida de configuracion). Queda como
        // externa que FALTA y se avisa.
        std::string rel = ruta, tmp;
        if (W3dRutaBajoCarpeta(ruta, dirDestino, tmp)) rel = tmp;
        std::string escrito = "ext:" + rel;
        RegistrarExterno(escrito, quien, false);
        W3dRefExternaMarcar(ruta);
        yaResueltas[original] = escrito;
        w3dLogfW("[W3D] no pude leer %s (%s): queda como referencia externa", ruta.c_str(), quien.c_str());
        return escrito;
    }
    unsigned crc = datos.empty() ? 0u : Crc32(&datos[0], datos.size());
    unsigned tam = (unsigned)datos.size();
    char h[40]; snprintf(h, sizeof(h), "%08x:%u", crc, tam);
    // DEDUP POR CONTENIDO: el mismo archivo referenciado desde tres escenas es
    // UNA entrada (y sonidos.lua no tiene que "pertenecerle" a nadie).
    // "Por CONTENIDO" es literal: se comparan LOS BYTES. La huella crc:tam solo
    // elige contra quien comparar (ver BuscarPorContenido).
    {
        size_t dup = BuscarPorContenido(h, datos);
        if (dup != (size_t)-1) {
            std::string nombre = pend[dup].nombre;
            yaResueltas[original] = nombre;
            ruta = nombre;
            return nombre;
        }
    }
    // TEST: "olvidarse" de esta entrada (el JSON la va a nombrar igual). Lo tiene
    // que atajar Verificar() ANTES de renombrar nada.
    if (g_w3dContOlvidarUna) {
        g_w3dContOlvidarUna = false;
        std::string nombre = NombreLibre(categoria, BaseDe(ruta));
        olvidadas.insert(nombre);
        yaResueltas[original] = nombre;
        ruta = nombre;
        return nombre;
    }
    W3dEntradaPend e;
    e.nombre = NombreLibre(categoria, BaseDe(ruta));
    e.origen = 1;
    e.fuente = ruta;
    e.temporal = false;
    e.crc = crc; e.tam = tam;
    porNombre[e.nombre] = pend.size();
    porHuella[h].push_back(pend.size());
    pend.push_back(e);
    yaResueltas[original] = e.nombre;
    ruta = e.nombre;
    return e.nombre;
}

bool W3dContenedorEscritor::AgregarBytes(const std::string& nombre, const std::string& texto,
                                         bool dedup) {
    if (Tiene(nombre)) {
        w3dLogfE("[W3D] entrada duplicada en el contenedor: %s", nombre.c_str());
        return false;
    }
    W3dEntradaPend e;
    e.nombre = nombre; e.origen = 0;
    e.bytes.assign(texto.begin(), texto.end());
    e.tam = (unsigned)e.bytes.size();
    e.crc = e.bytes.empty() ? 0u : Crc32(&e.bytes[0], e.bytes.size());
    porNombre[nombre] = pend.size();
    if (dedup) {
        char h[40]; snprintf(h, sizeof(h), "%08x:%u", e.crc, e.tam);
        porHuella[h].push_back(pend.size());
    }
    pend.push_back(e);
    return true;
}

// ---------------------------------------------------------------------------
//  ESTILO OPENDOCUMENT: "mimetype" (que Escribir() manda PRIMERA) y "LEEME.txt".
//  Las dos se REGENERAN en cada guardado y NINGUNA entra al indice por contenido
//  (ver el 'dedup' de AgregarBytes): son servicio, no datos del proyecto.
// ---------------------------------------------------------------------------
void W3dContenedorEscritor::EscribirCabeceraOdf() {
    if (!Tiene("mimetype"))  AgregarBytes("mimetype", kW3dMimetype, false);
    if (!Tiene("LEEME.txt")) AgregarBytes("LEEME.txt", W3dContenedorLeeme(), false);
}

bool W3dContenedorEscritor::AgregarTemporal(const std::string& nombre, const std::string& rutaTmp) {
    if (Tiene(nombre)) {
        w3dLogfE("[W3D] entrada duplicada en el contenedor: %s", nombre.c_str());
        return false;
    }
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(rutaTmp, datos)) {
        w3dLogfE("[W3D] no pude releer el temporal %s", rutaTmp.c_str());
        return false;
    }
    W3dEntradaPend e;
    e.nombre = nombre; e.origen = 1; e.fuente = rutaTmp; e.temporal = true;
    e.tam = (unsigned)datos.size();
    e.crc = datos.empty() ? 0u : Crc32(&datos[0], datos.size());
    porNombre[nombre] = pend.size();
    char h[40]; snprintf(h, sizeof(h), "%08x:%u", e.crc, e.tam);
    porHuella[h].push_back(pend.size());
    pend.push_back(e);
    return true;
}

// ---------------------------------------------------------------------------
//  VERIFICACION: se recorre el TEXTO de cada entrada generada (proyecto.json y
//  los .w3dui) buscando strings que arranquen con una carpeta reservada, o sea
//  referencias INTERNAS, y se exige que cada una tenga su entrada.
//
//  Es a proposito un escaneo de strings y no un parseo del esquema: un campo de
//  ruta NUEVO (que manana alguien agregue en el .w3dui) queda cubierto igual, y
//  esta verificacion existe justamente para atajar lo que el esquema no sabe.
// ---------------------------------------------------------------------------
static void RefsDelTexto(const std::vector<unsigned char>& b, std::set<std::string>& out) {
    std::string s;
    bool dentro = false;
    for (size_t i = 0; i < b.size(); i++) {
        char c = (char)b[i];
        if (!dentro) { if (c == '"') { dentro = true; s.clear(); } continue; }
        if (c == '\\') { if (i + 1 < b.size()) { s += (char)b[i + 1]; i++; } continue; }
        if (c == '"') {
            dentro = false;
            if (W3dEsNombreDeEntrada(s)) out.insert(s);
            continue;
        }
        s += c;
    }
}

bool W3dContenedorEscritor::Verificar(std::string& falta) {
    std::set<std::string> refs;      // de lo que GENERO este guardado -> abortar
    std::set<std::string> refsViejas;// de lo que se PRESERVO del zip viejo -> avisar
    for (size_t i = 0; i < pend.size(); i++) {
        const W3dEntradaPend& e = pend[i];
        // solo lo GENERADO por este guardado describe referencias: el JSON raiz
        // y las escenas. Un png no referencia nada.
        bool esJson = (e.nombre == "proyecto.json");
        bool esUi   = (e.nombre.size() > 6 && e.nombre.compare(e.nombre.size() - 6, 6, ".w3dui") == 0);
        if (!esJson && !esUi) continue;
        if (e.origen == 0) { RefsDelTexto(e.bytes, refs); continue; }
        std::vector<unsigned char> datos;
        if (e.origen == 1 && w3dFileSystem::ReadFileBytes(e.fuente, datos)) RefsDelTexto(datos, refs);
        // ORIGEN 2 = una escena que venia del .w3d viejo y que este guardado
        // PRESERVA sin haberla cargado (PreservarPasajeras). Antes no se escaneaba
        // NUNCA, o sea que era el unico .w3dui del archivo que podia quedar
        // apuntando a una entrada que ya no esta. Se escanea, pero NO aborta el
        // guardado: no la generamos nosotros y una escena vieja a medias del
        // usuario no puede dejarlo sin poder guardar. Sale por el log y sigue.
        if (e.origen == 2 && viejo && viejo->Leer(e.fuente.empty() ? e.nombre : e.fuente, datos))
            RefsDelTexto(datos, refsViejas);
    }
    for (std::set<std::string>::iterator it = refs.begin(); it != refs.end(); ++it)
        if (!Tiene(*it)) { falta = *it; return false; }
    for (std::set<std::string>::iterator it = refsViejas.begin(); it != refsViejas.end(); ++it)
        if (!Tiene(*it))
            w3dLogfW("[W3D] una escena PRESERVADA del .w3d anterior nombra '%s', que ya no esta adentro "
                     "(no la genero este guardado: no aborto, pero queda dicho)", it->c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  LIMITE CONOCIDO Y MEDIDO DE Verificar(): SOLO SE ESCANEA proyecto.json Y LOS
//  .w3dui. NINGUN .lua se mira.
//
//  O sea que un asset nombrado UNICAMENTE desde un script -"texturas/logo.png"
//  armado en Lua, o un require de otro .lua- no se copia adentro ni se reporta:
//  para el guardado ese archivo no existe. No es un descuido, es una decision:
//  un .lua puede construir el nombre en tiempo de ejecucion (".."), asi que un
//  escaneo de strings daria falsos negativos igual, y los falsos POSITIVOS
//  (cualquier string del script que PAREZCA un nombre de entrada) abortarian el
//  guardado del usuario por un comentario. La regla real es la otra: para que un
//  asset viaje adentro tiene que estar referenciado desde la ESCENA o el JSON
//  (que es lo que el editor sabe ingerir), y el IDE importa los assets al
//  agregarlos, no al nombrarlos.
// ---------------------------------------------------------------------------

void W3dContenedorEscritor::PreservarPasajeras() {
    if (!viejo) return;
    std::vector<std::string> todas;
    viejo->Listar(todas);
    for (size_t i = 0; i < todas.size(); i++) {
        const std::string& n = todas[i];
        if (Tiene(n)) continue;
        if (olvidadas.count(n)) continue;                 // hook de test
        // las entradas de SERVICIO las escribe este guardado (mimetype y
        // LEEME.txt se regeneran; preservar el LEEME viejo congelaria un texto
        // que ya no describe el archivo)
        if (W3dEsEntradaDeServicio(n)) continue;
        // mallas/ y animaciones/ son EXCLUSIVAS del editor: lo que no se
        // referencia ahi son huerfanos de un guardado anterior y se descartan
        if (n.compare(0, 7, "mallas/") == 0) continue;
        if (n.compare(0, 12, "animaciones/") == 0) continue;
        // EDITADA EN CALIENTE y que nadie referencia (un .lua suelto que el usuario
        // abrio y guardo desde el IDE): se preserva lo NUEVO, no lo del zip viejo.
        if (W3dContenedorEntradaEditada(n)) {
            std::vector<unsigned char> d;
            W3dContenedorLeerEditada(n, d);
            W3dEntradaPend ed;
            ed.nombre = n; ed.origen = 0; ed.bytes = d;
            ed.tam = (unsigned)d.size();
            ed.crc = d.empty() ? 0u : Crc32(&d[0], d.size());
            porNombre[n] = pend.size();
            pend.push_back(ed);
            continue;
        }
        W3dEntradaPend e;
        e.nombre = n; e.origen = 2; e.fuente = n;
        std::vector<unsigned char> crudo; unsigned met = 0, crc = 0, olen = 0;
        if (!viejo->LeerCrudo(n, crudo, &met, &crc, &olen)) continue;
        e.crc = crc; e.tam = olen;
        porNombre[n] = pend.size();
        pend.push_back(e);
    }
}

size_t W3dContenedorEscritor::CantidadExternosQueFaltan() const {
    size_t n = 0;
    for (size_t i = 0; i < externos.size(); i++) if (!externos[i].existe) n++;
    return n;
}

void W3dContenedorEscritor::EscribirExternos() {
    if (externos.empty()) return;
    // ORDEN ALFABETICO: el archivo tiene que ser determinista igual que el zip
    for (size_t i = 0; i + 1 < externos.size(); i++)
        for (size_t k = i + 1; k < externos.size(); k++)
            if (externos[k].ref < externos[i].ref) {
                W3dExterno t = externos[i]; externos[i] = externos[k]; externos[k] = t;
            }
    std::string s;
    s += "# Archivos que este proyecto usa pero que NO estan adentro del .w3d.\n";
    s += "# Si mandas este .w3d a otra maquina, estos archivos NO viajan.\n";
    for (size_t i = 0; i < externos.size(); i++) {
        s += externos[i].ref;
        s += "  | ";
        s += externos[i].quien.empty() ? std::string("(sin dueno)") : externos[i].quien;
        s += "  | ";
        s += externos[i].existe ? "EXISTE" : "FALTA";
        s += "\n";
    }
    AgregarBytes("EXTERNOS.txt", s);
}

bool W3dContenedorEscritor::Escribir(const std::string& rutaTmpZip) {
    // ORDEN DETERMINISTA (estilo OpenDocument): "mimetype" PRIMERA, despues
    // proyecto.json, despues todo lo demas ALFABETICO. Un hexdump de los
    // primeros 74 bytes dice:
    //   PK\3\4 ......... mimetype application/vnd.whisk3d.proyecto+zip
    // porque el local header del writer mide 30 bytes fijos (campo extra en
    // cero), asi que el contenido del mimetype cae justo en el offset 38.
    // Junto con la fecha fija y el metodo STORE, dos guardados sin cambios dan
    // BYTES IDENTICOS.
    static const char* const kPrimeras[] = { "mimetype", "proyecto.json", 0 };
    std::vector<size_t> orden;
    for (int p = 0; kPrimeras[p]; p++)
        for (size_t i = 0; i < pend.size(); i++)
            if (pend[i].nombre == kPrimeras[p]) orden.push_back(i);
    std::vector<std::string> nombres;
    for (size_t i = 0; i < pend.size(); i++) {
        bool fija = false;
        for (int p = 0; kPrimeras[p]; p++) if (pend[i].nombre == kPrimeras[p]) fija = true;
        if (!fija) nombres.push_back(pend[i].nombre);
    }
    for (size_t i = 0; i + 1 < nombres.size(); i++)
        for (size_t k = i + 1; k < nombres.size(); k++)
            if (nombres[k] < nombres[i]) { std::string t = nombres[i]; nombres[i] = nombres[k]; nombres[k] = t; }
    for (size_t i = 0; i < nombres.size(); i++) orden.push_back(porNombre[nombres[i]]);

    W3dZipWriter w;
    if (!w.AbrirEscritura(rutaTmpZip)) return false;
    for (size_t i = 0; i < orden.size(); i++) {
        const W3dEntradaPend& e = pend[orden[i]];
        bool ok = true;
        if (e.origen == 0) {
            ok = w.Escribir(e.nombre, e.bytes.empty() ? (const void*)"" : (const void*)&e.bytes[0],
                            e.bytes.size());
        } else if (e.origen == 1) {
            std::vector<unsigned char> datos;
            if (!w3dFileSystem::ReadFileBytes(e.fuente, datos)) {
                w3dLogfE("[W3D] no pude releer %s para meterlo en el .w3d", e.fuente.c_str());
                ok = false;
            } else {
                ok = w.Escribir(e.nombre, datos.empty() ? (const void*)"" : (const void*)&datos[0],
                                datos.size());
            }
        } else {
            ok = viejo && w.CopiarDe(*viejo, e.fuente);
        }
        if (!ok) { w.Abortar(); return false; }
    }
    return w.Cerrar();
}

void W3dContenedorEscritor::LimpiarTemporales() {
    for (size_t i = 0; i < pend.size(); i++)
        if (pend[i].origen == 1 && pend[i].temporal) remove(pend[i].fuente.c_str());
}
