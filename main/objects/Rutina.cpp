#include "Rutina.h"
#include "objects/Mesh.h"
#include "objects/Textures.h"
#include "objects/MallaRecurso.h"
#include "objects/MallaFlujos.h"
#include "objects/Culling.h"       // los planos de la camara (paso "Test de visibilidad")
#include "script/W3dScript.h"      // W3dScriptEvento (paso "Llamar Lua")
#include "w3dGraphics.h"
#include "io/JsonW3d.h"            // JVal / JsonNumTexto: los campos del .w3d
#include "animation/Animation.h"   // AnimPaso: los pasos se animan (g_animPasoHook)
#include <map>
#include <stdlib.h>
#include <string.h>

namespace gfx = w3dEngine;

bool (*W3dRutinaValidarHook)(Rutina*) = NULL;
#ifdef W3D_SIN_EDITOR
bool g_w3dSoloJuego = true;     // el juego compilado: siempre solo-juego
#else
bool g_w3dSoloJuego = false;    // el editor: lo prende el modo VERDE+0 (LayoutJuegoPuroToggle)
#endif
unsigned g_w3dConstructorGen = 1;
bool g_w3dLimpiarColorFijo = false;
unsigned g_w3dLimpiezasColor = 0;

// ---------------------------------------------------------------------------
//  la tabla de los pasos: nombre del .w3d, etiqueta de la interfaz, grupo y que campos usa
// ---------------------------------------------------------------------------
struct PasoInfo {
    const char* nombre; const char* etiqueta;
    unsigned char grupo, numeros, ref, entero, opciones;
    bool on; const char* etiquetaOn;
    const char* campos[5];
};
static const PasoInfo kPasos[PasoN] = {
    { "apilar",          "Push matrix",       GrupoTransformar, 0, RefNada,    EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "desapilar",       "Pop matrix",        GrupoTransformar, 0, RefNada,    EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "trasladar",       "Translate",         GrupoTransformar, 3, RefNada,    EnteroNada,   OpcNada,          false, 0, { "X", "Y", "Z" } },
    { "rotar",           "Rotate",            GrupoTransformar, 4, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Angle", "Axis X", "Axis Y", "Axis Z" } },
    { "escalar",         "Scale",             GrupoTransformar, 3, RefNada,    EnteroNada,   OpcNada,          false, 0, { "X", "Y", "Z" } },
    { "matrizObjeto",    "Object matrix",     GrupoTransformar, 0, RefObjeto,  EnteroNada,   OpcMatriz,        false, 0, { 0 } },
    { "array",           "Array on/off",      GrupoDibujar,     0, RefNada,    EnteroNada,   OpcArray,         true,  "On", { 0 } },
    { "punteroVertices", "Vertex pointer",    GrupoDibujar,     0, RefMalla,   EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "punteroNormales", "Normal pointer",    GrupoDibujar,     0, RefMalla,   EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "punteroUV",       "UV pointer",        GrupoDibujar,     0, RefMalla,   EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "punteroColores",  "Color pointer",     GrupoDibujar,     0, RefMalla,   EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "dibujar",         "Draw elements",     GrupoDibujar,     0, RefMalla,   EnteroParte,  OpcPrimitiva,     false, 0, { 0 } },
    { "tamPunto",        "Point size",        GrupoDibujar,     1, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Pixels" } },
    { "anchoLinea",      "Line width",        GrupoDibujar,     1, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Pixels" } },
    { "hijos",           "Draw children",     GrupoDibujar,     0, RefNada,    EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "rutina",          "Call routine",      GrupoDibujar,     0, RefRutina,  EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "textura",         "Texture on/off",    GrupoTextura,     0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "fijarTextura",    "Bind texture",      GrupoTextura,     0, RefTextura, EnteroNada,   OpcNada,          false, 0, { 0 } },
    { "texturaModo",     "Texture mode",      GrupoTextura,     0, RefNada,    EnteroNada,   OpcTexturaModo,   false, 0, { 0 } },
    { "texturaFiltro",   "Texture filter",    GrupoTextura,     0, RefNada,    EnteroNada,   OpcFiltro,        false, 0, { 0 } },
    { "texturaRepetir",  "Texture wrap",      GrupoTextura,     0, RefNada,    EnteroNada,   OpcRepetir,       false, 0, { 0 } },
    { "matrizUV",        "UV offset",         GrupoTextura,     4, RefNada,    EnteroNada,   OpcNada,          false, 0, { "U", "V", "Scale U", "Scale V" } },
    { "matcap",          "Matcap",            GrupoTextura,     0, RefMalla,   EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "nieblaActiva",    "Fog on/off",        GrupoNiebla,      0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "nieblaModo",      "Fog mode",          GrupoNiebla,      0, RefNada,    EnteroNada,   OpcNieblaModo,    false, 0, { 0 } },
    { "nieblaInicio",    "Fog start",         GrupoNiebla,      1, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Start" } },
    { "nieblaFin",       "Fog end",           GrupoNiebla,      1, RefNada,    EnteroNada,   OpcNada,          false, 0, { "End" } },
    { "nieblaDensidad",  "Fog density",       GrupoNiebla,      1, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Density" } },
    { "nieblaColor",     "Fog color",         GrupoNiebla,      4, RefNada,    EnteroNada,   OpcNada,          false, 0, { "R", "G", "B", "A" } },
    { "luz",             "Lighting on/off",   GrupoLuces,       0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "luzActiva",       "Light on/off",      GrupoLuces,       0, RefNada,    EnteroLuz,    OpcNada,          true,  "On", { 0 } },
    { "luzColor",        "Light color",       GrupoLuces,       4, RefNada,    EnteroLuz,    OpcLuzComponente, false, 0, { "R", "G", "B", "A" } },
    { "luzPosicion",     "Light position",    GrupoLuces,       3, RefNada,    EnteroLuz,    OpcLuzTipo,       false, 0, { "X", "Y", "Z" } },
    { "luzAmbiente",     "Ambient light",     GrupoLuces,       4, RefNada,    EnteroNada,   OpcNada,          false, 0, { "R", "G", "B", "A" } },
    { "alfaTest",        "Alpha test on/off", GrupoAlfa,        0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "alfaRef",         "Alpha reference",   GrupoAlfa,        1, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Reference" } },
    { "color",           "Color",             GrupoColor,       4, RefNada,    EnteroNada,   OpcNada,          false, 0, { "R", "G", "B", "A" } },
    { "colorVertice",    "Vertex color",      GrupoColor,       0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "materialColor",   "Material color",    GrupoColor,       4, RefNada,    EnteroNada,   OpcMaterialComp,  false, 0, { "R", "G", "B", "A" } },
    { "brillo",          "Shininess",         GrupoColor,       1, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Shininess" } },
    { "sombreado",       "Smooth shading",    GrupoColor,       0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "testZ",           "Depth test",        GrupoProfundidad, 0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "escribirZ",       "Depth write",       GrupoProfundidad, 0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "funcionZ",        "Depth function",    GrupoProfundidad, 0, RefNada,    EnteroNada,   OpcFuncionZ,      false, 0, { 0 } },
    { "sesgoZ",          "Depth bias",        GrupoProfundidad, 2, RefNada,    EnteroNada,   OpcNada,          true,  "On", { "Factor", "Units" } },
    { "sesgoMetros",     "Depth shift (m)",   GrupoProfundidad, 1, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Meters" } },
    { "rangoZ",          "Depth range",       GrupoProfundidad, 2, RefNada,    EnteroNada,   OpcNada,          false, 0, { "Near", "Far" } },
    { "colorLimpieza",   "Clear color",       GrupoProfundidad, 4, RefNada,    EnteroNada,   OpcNada,          false, 0, { "R", "G", "B", "A" } },
    { "limpiar",         "Clear",             GrupoProfundidad, 0, RefNada,    EnteroBuffers, OpcNada,         false, 0, { 0 } },
    { "mezclaActiva",    "Blending on/off",   GrupoMezcla,      0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "mezclaModo",      "Blend mode",        GrupoMezcla,      0, RefNada,    EnteroNada,   OpcMezclaModo,    false, 0, { 0 } },
    { "caras",           "Face culling",      GrupoCaras,       0, RefNada,    EnteroNada,   OpcNada,          true,  "On", { 0 } },
    { "saltarSi",        "Skip if zero",      GrupoControl,     1, RefNada,    EnteroCuenta, OpcNada,          false, 0, { "Value" } },
    { "saltarOculto",    "Skip if hidden",    GrupoControl,     0, RefObjeto,  EnteroCuenta, OpcNada,          false, 0, { 0 } },
    { "visible",         "Visibility test",   GrupoControl,     2, RefMalla,   EnteroParte,  OpcNada,          false, 0, { "Write to", "Box (memory)" } },
    { "lua",             "Call Lua",          GrupoControl,     0, RefFuncion, EnteroNada,   OpcNada,          false, 0, { 0 } },
};
static const char* const kGrupos[GrupoN] = { "Transform", "Draw", "Texture", "Fog", "Lights", "Alpha test", "Color",
                                             "Depth & clear", "Blending", "Face culling", "Control" };

// las OPCIONES de los desplegables (el indice es el valor de 'modo')
static const char* const kOpcNiebla[]  = { "Linear", "Exponential", "Exponential squared" };
static const char* const kOpcTextura[] = { "Modulate", "Replace", "Normal map (DOT3)", "Alpha only" };
static const char* const kOpcFuncZ[]   = { "Less", "Less or equal", "Equal", "Always" };            // gfx::DepthCmp
static const char* const kOpcMezcla[]  = { "Opaque (one, zero)", "Alpha", "Additive", "Additive (by alpha)", "Multiply",
                                           "Screen", "Premultiplied alpha", "Subtract" };           // gfx::Mezcla
static const char* const kOpcLuzComp[] = { "Ambient", "Diffuse", "Specular" };
static const char* const kOpcLuzTipo[] = { "Point", "Directional" };
static const char* const kOpcPrim[]    = { "Points", "Lines", "Line strip", "Line loop", "Triangles", "Triangle strip",
                                           "Triangle fan", "Quads (simulated)", "Quad strip (simulated)",
                                           "Polygon (simulated)" };                                 // W3dPrimRutina
static const char* const kOpcFiltro[]  = { "Nearest (pixelated)", "Linear" };
static const char* const kOpcRepetir[] = { "Repeat", "Clamp" };
static const char* const kOpcMatComp[] = { "Ambient", "Diffuse", "Specular", "Emission" };          // gfx::MatParam
static const char* const kOpcArray[]   = { "Vertices", "Normals", "UV", "Colors" };                 // W3dArrayRutina
static const char* const kOpcMatriz[]  = { "Multiply", "Load (camera x object)" };
struct OpcInfo { const char* titulo; const char* const* opciones; int n; };
static const OpcInfo kOpc[OpcN] = {
    { "", NULL, 0 },
    { "Mode",      kOpcNiebla,  3 },
    { "Mode",      kOpcTextura, 4 },
    { "Function",  kOpcFuncZ,   4 },
    { "Mode",      kOpcMezcla,  (int)gfx::MezclaCount_ },
    { "Component", kOpcLuzComp, 3 },
    { "Type",      kOpcLuzTipo, 2 },
    { "Primitive", kOpcPrim,    (int)RPrimN },
    { "Filter",    kOpcFiltro,  2 },
    { "Wrap",      kOpcRepetir, 2 },
    { "Component", kOpcMatComp, 4 },
    { "Array",     kOpcArray,   (int)ArrayN },
    { "Mode",      kOpcMatriz,  2 },
};
// la primitiva en el .w3d (por nombre: un numero no dice nada al leer el archivo)
static const char* const kPrimNombre[RPrimN] = { "puntos", "lineas", "tiraLineas", "lazoLineas", "triangulos", "tira",
                                                  "abanico", "quads", "tiraQuads", "poligono" };

const char* W3dPasoNombre(int t)    { return (t >= 0 && t < PasoN) ? kPasos[t].nombre : "?"; }
const char* W3dPasoEtiqueta(int t)  { return (t >= 0 && t < PasoN) ? kPasos[t].etiqueta : "?"; }
int  W3dPasoGrupoDe(int t)          { return (t >= 0 && t < PasoN) ? kPasos[t].grupo : GrupoControl; }
const char* W3dGrupoEtiqueta(int g) { return (g >= 0 && g < GrupoN) ? kGrupos[g] : "?"; }
int  W3dPasoRef(int t)              { return (t >= 0 && t < PasoN) ? kPasos[t].ref : RefNada; }
bool W3dPasoUsaOn(int t)            { return t >= 0 && t < PasoN && kPasos[t].on; }
const char* W3dPasoEtiquetaOn(int t){ return (t >= 0 && t < PasoN && kPasos[t].etiquetaOn) ? kPasos[t].etiquetaOn : "On"; }
int  W3dPasoEntero(int t)           { return (t >= 0 && t < PasoN) ? kPasos[t].entero : EnteroNada; }
int  W3dPasoOpciones(int t)         { return (t >= 0 && t < PasoN) ? kPasos[t].opciones : OpcNada; }
bool W3dPasoEsColor(int t) {
    return t == PasoColor || t == PasoNieblaColor || t == PasoLuzColor || t == PasoLuzAmbiente ||
           t == PasoMaterialColor || t == PasoColorLimpieza;
}
int  W3dOpcionesN(int l)            { return (l > 0 && l < OpcN) ? kOpc[l].n : 0; }
const char* W3dOpcionesTitulo(int l){ return (l > 0 && l < OpcN) ? kOpc[l].titulo : ""; }
const char* W3dOpcionEtiqueta(int l, int i) {
    return (l > 0 && l < OpcN && i >= 0 && i < kOpc[l].n) ? kOpc[l].opciones[i] : "?";
}
const char* W3dPrimitivaNombre(int p) { return (p >= 0 && p < RPrimN) ? kPrimNombre[p] : "triangulos"; }
static int PrimitivaDesdeNombre(const std::string& n) {
    for (int p = 0; p < RPrimN; p++) if (n == kPrimNombre[p]) return p;
    return RPrimTriangulos;
}
int W3dPasoNumeros(const W3dPaso& p) {
    // "Dibujar elementos": manual = [primero, ultimo]; array = [primero, ultimo, "@array"] (el manual se guarda igual)
    if (p.tipo == PasoDibujar) return p.sub == RangoManual ? 2 : (p.sub == RangoArray ? 4 : 0);
    return p.tipo < PasoN ? kPasos[p.tipo].numeros : 0;
}
const char* W3dPasoNumeroNombre(const W3dPaso& p, int i) {
    if (p.tipo == PasoDibujar) return i == 0 ? "First triangle" : (i == 1 ? "Last triangle" : (i == 2 ? "Array" : "Flags"));
    return (p.tipo < PasoN && i >= 0 && i < kPasos[p.tipo].numeros) ? kPasos[p.tipo].campos[i] : "";
}
int W3dPasoDesdeNombre(const std::string& n) {
    for (int t = 0; t < PasoN; t++) if (n == kPasos[t].nombre) return t;
    return -1;
}

W3dPaso W3dPasoNuevo(int tipo) {
    W3dPaso p;
    p.tipo = (unsigned char)tipo;
    switch (tipo) {
    case PasoEscalar:        p.n[0].v = p.n[1].v = p.n[2].v = 1.0f; break;
    case PasoRotar:          p.n[2].v = 1.0f; break;                                  // eje Y
    case PasoColor:          p.n[0].v = p.n[1].v = p.n[2].v = p.n[3].v = 1.0f; break;
    case PasoColorLimpieza:  p.n[3].v = 1.0f; break;                                  // negro
    case PasoLimpiar:        p.entero = LimpiarColor | LimpiarProfundidad; break;
    case PasoDibujar:        p.entero = -1; p.modo = RPrimTriangulos; p.n[1].v = -1.0f; break;   // todas, triangulos
    case PasoNieblaInicio:   p.n[0].v = 50.0f; break;
    case PasoNieblaFin:      p.n[0].v = 300.0f; break;
    case PasoNieblaDensidad: p.n[0].v = 0.01f; break;
    case PasoNieblaColor:    p.n[0].v = p.n[1].v = p.n[2].v = 0.7f; p.n[3].v = 1.0f; break;
    case PasoLuzColor:       p.modo = 1; p.n[0].v = p.n[1].v = p.n[2].v = p.n[3].v = 1.0f; break;   // difuso blanco
    case PasoLuzPosicion:    p.modo = 1; p.n[1].v = 1.0f; break;                      // direccional, desde arriba
    case PasoLuzAmbiente:    p.n[0].v = p.n[1].v = p.n[2].v = 0.2f; p.n[3].v = 1.0f; break;
    case PasoAlfaRef:        p.n[0].v = 0.5f; break;
    case PasoMezclaModo:     p.modo = gfx::MezclaAlpha; break;
    case PasoTexturaFiltro:  p.modo = 1; break;                                       // lineal
    case PasoMaterialColor:  p.modo = 1; p.n[0].v = p.n[1].v = p.n[2].v = p.n[3].v = 1.0f; break;   // difuso blanco
    case PasoBrillo:         p.n[0].v = 12.0f; break;
    case PasoMatrizUV:       p.n[2].v = p.n[3].v = 1.0f; break;                      // identidad
    case PasoRangoZ:         p.n[1].v = 1.0f; break;                                  // 0..1 (el normal)
    case PasoTamPunto:       p.n[0].v = 4.0f; break;
    case PasoAnchoLinea:     p.n[0].v = 1.0f; break;
    case PasoSesgoZ:         p.n[0].v = -1.0f; p.n[1].v = -4.0f; break;              // acerca (calcomanias)
    case PasoSaltarSi: case PasoSaltarOculto: p.entero = 1; break;
    case PasoVisible:        p.entero = -1; break;                                    // la malla entera
    case PasoArray:          p.modo = ArrayNormales; break;
    default: break;
    }
    return p;
}

// ---------------------------------------------------------------------------
//  las MEMORIAS: bloques fijos de 256 floats (un puntero a uno no se invalida nunca)
// ---------------------------------------------------------------------------
static std::map<std::string, float*>& Memorias() { static std::map<std::string, float*> m; return m; }
float* W3dMemoria(const std::string& nombre) {
    std::map<std::string, float*>::iterator it = Memorias().find(nombre);
    if (it != Memorias().end()) return it->second;
    float* b = (float*)calloc(W3D_MEMORIA_TAM, sizeof(float));
    Memorias()[nombre] = b;
    return b;
}
bool W3dMemoriaExiste(const std::string& nombre) { return Memorias().count(nombre) > 0; }
// "@nombre[i]" (sin [i] = el 0): el nombre y el indice; false si esta mal escrito
static bool PartirRef(const std::string& ref, std::string& nombre, int& i) {
    if (ref.size() < 2 || ref[0] != '@') return false;
    size_t c = ref.find('[');
    nombre = ref.substr(1, c == std::string::npos ? std::string::npos : c - 1);
    i = 0;
    if (c != std::string::npos) {
        size_t d = ref.find(']', c);
        if (d == std::string::npos) return false;
        i = atoi(ref.substr(c + 1, d - c - 1).c_str());
    }
    return !nombre.empty() && i >= 0 && i < W3D_MEMORIA_TAM;
}
const float* W3dMemoriaRef(const std::string& ref) {
    std::string nombre; int i;
    if (!PartirRef(ref, nombre, i)) return NULL;
    return W3dMemoria(nombre) + i;
}
int W3dMemoriaIndice(const std::string& ref) {
    std::string nombre; int i;
    return PartirRef(ref, nombre, i) ? i : -1;
}

// la malla 3D de un nombre: un recurso de la biblioteca o el recurso del OBJETO malla con ese nombre
MallaRecurso* W3dRutinaBuscarMalla(Object* desde, const std::string& nombre) {
    if (nombre.empty()) return NULL;
    MallaRecurso* r = W3dMallaRecursoPorNombre(nombre);
    if (r && !r->borrado) return r;
    Object* o = desde ? W3dBuscarNombreDesde(desde, nombre) : NULL;
    if (o && o->getType() == ObjectType::mesh) return ((Mesh*)o)->malla;
    return NULL;
}

// ---------------------------------------------------------------------------
Rutina::Rutina(Object* parent, const std::string& nombre)
    : Object(parent, nombre, Vector3(0, 0, 0)), listaEditada(0), pasoActivo(-1), invalida(false), sucia(true),
      ejecutada(false), constructor(false), genHecha(0), arraysVistos(false), precargada(false), punterosOk(false),
      punterosEdad(0), valOk(false), valSabido(false), valVistos(false), valGen(0), valEdad(0) {
    sinTransformacion = true;
    for (int m = 0; m < ModoN; m++) { usar[m] = -1; conHijos[m] = false; }
    for (int k = 0; k < 4; k++) arraysInicio[k] = -1;
}
Rutina::~Rutina() {
    for (size_t i = 0; i < retenidas.size(); i++) W3dMallaRecursoSoltar(retenidas[i]);
    retenidas.clear();
}

int Rutina::ModoActual() {
    if (w3dRenderWireframe) return ModoAlambre;
    if (w3dRenderSinLuz || w3dRenderNormalColor || w3dRenderAlpha) return ModoZBuffer;
    if (w3dRenderSolido) return ModoSolido;
    if (w3dRenderLuces) return ModoRender;
    return ModoMaterial;
}

int Rutina::ListaActualIndice() {
    int u = usar[ModoActual()];
    return (u < 0 || u >= ModoN) ? (int)ModoDefecto : u;
}
std::vector<W3dPaso>& Rutina::ListaActual() { return listas[ListaActualIndice()]; }

// la textura por su RUTA (la del cache de texturas: termina en el nombre pedido)
static Texture* BuscarTextura(const std::string& ref) {
    if (ref.empty()) return NULL;
    for (size_t i = 0; i < Textures.size(); i++) {
        Texture* t = Textures[i];
        if (!t) continue;
        const std::string& p = t->path;
        if (p == ref || (p.size() > ref.size() && p.compare(p.size() - ref.size(), ref.size(), ref) == 0)) return t;
    }
    return NULL;
}

// resuelve UN paso (nombres -> punteros). 'desde' = donde se buscan los objetos. Las mallas se retienen en
// 'retener' (el que llama suelta las viejas DESPUES: si es la misma, no se descarga para volver a cargarla)
static void ResolverPaso(W3dPaso& p, Object* desde, Object* noSoy, std::vector<MallaRecurso*>* retener) {
    for (int k = 0; k < 5; k++) p.n[k].p = p.n[k].ref.empty() ? NULL : W3dMemoriaRef(p.n[k].ref);
    p.ptr = NULL; p.ptr2 = NULL;
    if (!p.ref2.empty()) p.ptr2 = W3dBuscarNombreDesde(desde, p.ref2);
    const int rt = W3dPasoRef(p.tipo);
    if (rt == RefNada || rt == RefFuncion || p.ref.empty()) return;
    if (rt == RefTextura) {
        // si ningun material la usa todavia, la carga el cache (una vez; la rutina la toma)
        p.ptr = BuscarTextura(p.ref);
        if (!p.ptr) p.ptr = TexturaTomar(p.ref);
        return;
    }
    if (rt == RefMalla) {
        MallaRecurso* r = W3dRutinaBuscarMalla(desde, p.ref);
        if (r && retener) {
            if (!W3dMallaRecursoRetener(r)) r = NULL;
            else retener->push_back(r);
        }
        // (un nombre de OBJETO malla, de los .w3d viejos: queda el del recurso, que es lo que se dibuja)
        if (r && r->nombre != p.ref) p.ref = r->nombre;
        p.ptr = r;
        return;
    }
    Object* o = W3dBuscarNombreDesde(desde, p.ref);
    if (rt == RefRutina) p.ptr = (o && o->getType() == ObjectType::rutina && o != noSoy) ? o : NULL;
    else                 p.ptr = o;
}

static bool MallaTieneArray(const MallaRecurso* r, int k) {
    return k == ArrayVertices ? r->vertex != 0 : k == ArrayNormales ? r->normals != 0 :
           k == ArrayUV ? r->uv != 0 : r->vertexColor != 0;
}
W3dPaso* Rutina::PasoPorId(unsigned id) {
    if (!id) return NULL;
    for (int m = 0; m < ModoN; m++)
        for (size_t i = 0; i < listas[m].size(); i++) if (listas[m][i].id == id) return &listas[m][i];
    return NULL;
}
void Rutina::Resolver() {
    precargada = false;
    punterosOk = false;
    g_w3dRutinasGen++;
    // los pasos NUEVOS (id 0: recien agregados o de un archivo viejo) toman un id libre de la rutina
    {
        unsigned mx = 0;
        for (int m = 0; m < ModoN; m++)
            for (size_t i = 0; i < listas[m].size(); i++) if (listas[m][i].id > mx) mx = listas[m][i].id;
        for (int m = 0; m < ModoN; m++)
            for (size_t i = 0; i < listas[m].size(); i++) if (!listas[m][i].id) listas[m][i].id = (unsigned short)++mx;
    }
    std::vector<MallaRecurso*> nuevas;
    for (int m = 0; m < ModoN; m++) {
        conHijos[m] = false;
        for (size_t i = 0; i < listas[m].size(); i++) {
            W3dPaso& p = listas[m][i];
            if (p.tipo == PasoHijos) conHijos[m] = true;
            ResolverPaso(p, this, this, &nuevas);
        }
        // (version 1) el "Array on" de un puntero viejo: si la malla no tiene ese array, queda apagado (como antes)
        for (size_t i = 0; i + 1 < listas[m].size(); i++) {
            W3dPaso& p = listas[m][i];
            const W3dPaso& q = listas[m][i + 1];
            if (p.tipo != PasoArray || p.sub != 1 || q.tipo != PasoPunteroVertices + p.modo || !q.ptr) continue;
            p.on = MallaTieneArray((const MallaRecurso*)q.ptr, p.modo);
            p.sub = 0;
        }
    }
    for (size_t i = 0; i < retenidas.size(); i++) W3dMallaRecursoSoltar(retenidas[i]);
    retenidas.swap(nuevas);
    sucia = false;
}

// ---------------------------------------------------------------------------
//  EL EJECUTOR. Estado de UNA corrida (la rutina del arbol y lo que llame adentro: subrutinas, hijos, lua)
// ---------------------------------------------------------------------------
static int   gProf = 0;                // profundidad de rutinas corriendo (0 = afuera de toda rutina)
static unsigned gIboActual = 0;        // el IBO bindeado (0xFFFFFFFF = no se sabe)
static bool  gVboEnUso = false;        // algun puntero quedo en un VBO (al terminar se desbindea)
static int   gPlanosEstado = 0;        // 0 sin armar, 1 listos, -1 no cortar (orto / sin vista)
static float gPlanos[24];
static bool  gAlfaOn = false;          // el alpha test es UN estado con dos pasos (on/off y la referencia)
static float gAlfaRef = 0.5f;
static Rutina* gEnCurso = NULL;        // la rutina cuya lista se esta ejecutando (lua: paso())
// "Matriz del objeto" en modo CARGAR: la modelview con la que arranco la corrida (la camara) se lee una vez; cada
// objeto carga camara x objeto (una llamada en vez de apilar + multiplicar + desapilar) y al terminar se repone
static Matrix4 gBaseMV;
static bool gBaseMVLeida = false, gMVCargada = false;
static void ReponerMV() { if (gMVCargada) { gfx::LoadMatrix(gBaseMV.m); gMVCargada = false; } }
// CACHE DE PUNTEROS: la malla a la que apunta cada array (vertices, normales, uv, colores). Apuntar de nuevo al mismo
// array no llama al driver (63 instancias de la misma pieza: sus punteros se ponen una vez). Se olvida al empezar
// cada corrida y despues de lo que puede poner otros punteros (los hijos, lua). Prender o apagar un array NO es
// cosa del puntero: es el paso "Array on/off" (se prende UNA vez y queda; el cache de gfx no repite)
static const MallaRecurso* gPtrMalla[4] = { 0, 0, 0, 0 };
static void OlvidarPunteros() { for (int k = 0; k < 4; k++) gPtrMalla[k] = 0; }

static void AbrirCorrida() {
    if (gProf++ > 0) return;
    gIboActual = 0xFFFFFFFFu; gVboEnUso = false; gPlanosEstado = 0;
    gBaseMVLeida = gMVCargada = false;
    OlvidarPunteros();
}
static void CerrarCorrida() {
    if (--gProf > 0) return;
    if (gVboEnUso) gfx::UnbindVBOs();
    gVboEnUso = false; gIboActual = 0xFFFFFFFFu;
    OlvidarPunteros();
    ReponerMV();
}

// un PUNTERO: el array de esa malla (VBO si lo tiene, si no RAM). Solo apunta (glXxxPointer): el array lo prende o lo
// apaga el paso "Array on/off". Una malla sin ese array no cambia nada (el editor no deja dibujar con ese array
// prendido apuntando ahi)
static void Puntero(int tipo, const W3dPaso& p) {
    MallaRecurso* r = (MallaRecurso*)p.ptr;
    const int k = tipo - PasoPunteroVertices;
    const void* datos = !r ? NULL : tipo == PasoPunteroVertices ? (const void*)r->vertex :
                        tipo == PasoPunteroNormales ? (const void*)r->normals :
                        tipo == PasoPunteroUV ? (const void*)r->uv : (const void*)r->vertexColor;
    if (!datos) return;
    if (gPtrMalla[k] == r) return;   // (ya apunta ahi: ni bind ni puntero)
    gPtrMalla[k] = r;
    unsigned h = 0;
    if (W3dMallaRecursoAsegurarVBO(r))
        h = tipo == PasoPunteroVertices ? r->vboPos : tipo == PasoPunteroNormales ? r->vboNor :
            tipo == PasoPunteroUV ? r->vboUV : r->vboCol;
    if (h) {
        switch (tipo) {
        case PasoPunteroVertices: gfx::VertexVBO(h); break;
        case PasoPunteroNormales: gfx::NormalVBO(h); break;
        case PasoPunteroUV:       gfx::TexCoordVBO(h); break;
        default:                  gfx::ColorVBO(h); break;
        }
        gVboEnUso = true;
    } else {
        if (gVboEnUso) { gfx::UnbindVBOs(); gVboEnUso = false; gIboActual = 0; OlvidarPunteros(); gPtrMalla[k] = r; }
        switch (tipo) {
        case PasoPunteroVertices: gfx::VertexPointer3f(0, r->vertex); break;
        case PasoPunteroNormales: gfx::NormalPointer3b(r->normals); break;
        case PasoPunteroUV:       gfx::TexCoordPointer2f(0, r->uv); break;
        default:                  gfx::ColorPointer4ub(r->vertexColor); break;
        }
    }
}
static const gfx::VArray kArrayGfx[ArrayN] = { gfx::VertexArray, gfx::NormalArray, gfx::TexCoordArray, gfx::ColorArray };

// manda un flujo (del IBO o de RAM), recortado a los indices [desde, desde + cuantos) si cuantos >= 0
static void Mandar(const W3dFlujo& f, int desde, int cuantos) {
    int n = f.cantidad, primero = f.primero;
    const MeshIndex* idx = f.idx;
    if (cuantos >= 0) { n = cuantos; primero += desde; idx += desde; }
    if (n <= 0) return;
    if (f.ibo) {
        if (gIboActual != f.ibo) { gfx::BindIndexVBO(f.ibo); gIboActual = f.ibo; }
        gfx::DrawElementsVBO((gfx::Primitiva)f.primGL, n, primero);
    } else {
        if (gIboActual != 0) { gfx::BindIndexVBO(0); gIboActual = 0; }
        gfx::DrawElements((gfx::Primitiva)f.primGL, n, idx);
    }
}
static void Dibujar(const W3dPaso& p) {
    MallaRecurso* r = (MallaRecurso*)p.ptr;
    W3dMallaRecursoAsegurarVBO(r);   // (el IBO de la malla: los indices de la parte)
    W3dFlujo f;
    if (!W3dMallaFlujo(r, p.entero, p.modo, f)) return;
    const int tris = f.cantidad / 3;
    if (p.sub == RangoManual) {
        const int a = (int)p.n[0].Valor();
        int b = (int)p.n[1].Valor(); if (b < 0) b = tris - 1;
        Mandar(f, a * 3, (b - a + 1) * 3);
    } else if (p.sub == RangoArray) {
        // [cantidad, primero0, ultimo0, primero1, ultimo1...] (ultimo < 0 = el ultimo de la parte). Con BANDERAS (n[3],
        // otra memoria) cada rango lleva la suya: [cantidad, primero, ultimo, bandera, ...] y se dibujan los de bandera
        // prendida. Los seguidos (uno empieza donde termino el anterior) van en UNA llamada
        const float* a = p.n[2].p;
        const float* on = p.n[3].p;
        const int k = (int)a[0], ancho = on ? 3 : 2;
        int x0 = -1, y0 = -1;
        for (int j = 0; j < k; j++) {
            const float* e = a + 1 + ancho * j;
            if (on && on[(int)e[2]] == 0.0f) continue;
            const int x = (int)e[0];
            int y = (int)e[1]; if (y < 0) y = tris - 1;
            if (x0 >= 0 && x == y0 + 1) { y0 = y; continue; }
            if (x0 >= 0) Mandar(f, x0 * 3, (y0 - x0 + 1) * 3);
            x0 = x; y0 = y;
        }
        if (x0 >= 0) Mandar(f, x0 * 3, (y0 - x0 + 1) * 3);
    } else {
        Mandar(f, 0, -1);
    }
}

// la caja de la parte (movida por la matriz de un objeto, si hay) contra los planos de la camara
static bool Visible(const W3dPaso& p) {
    if (gPlanosEstado == 0) gPlanosEstado = W3dFrustumMedidaPlanos(gPlanos) ? 1 : -1;
    if (gPlanosEstado < 0) return true;
    // la caja: la de la parte de la malla o, sin malla, 6 numeros de una memoria (minimo y maximo: las celdas de un mundo
    // unido, que no son partes)
    float mn[3], mx[3];
    const float* c = p.n[1].p;
    if (!p.ptr && c) { for (int k = 0; k < 3; k++) { mn[k] = c[k]; mx[k] = c[3 + k]; } }
    else if (!W3dMallaParteCaja((MallaRecurso*)p.ptr, p.entero, mn, mx)) return true;
    Vector3 a(mn[0], mn[1], mn[2]), b(mx[0], mx[1], mx[2]);
    if (p.ptr2) {
        Matrix4 W; ((Object*)p.ptr2)->GetWorldMatrix(W);
        Vector3 c0(1e30f, 1e30f, 1e30f), c1(-1e30f, -1e30f, -1e30f);
        for (int k = 0; k < 8; k++) {
            Vector3 e = W * Vector3((k & 1) ? mx[0] : mn[0], (k & 2) ? mx[1] : mn[1], (k & 4) ? mx[2] : mn[2]);
            if (e.x < c0.x) c0.x = e.x;
            if (e.y < c0.y) c0.y = e.y;
            if (e.z < c0.z) c0.z = e.z;
            if (e.x > c1.x) c1.x = e.x;
            if (e.y > c1.y) c1.y = e.y;
            if (e.z > c1.z) c1.z = e.z;
        }
        a = c0; b = c1;
    }
    return W3dAabbEnPlanos(gPlanos, a, b);
}

bool g_rutinaPerfil = false;
long g_rutinaPerfilVeces[PasoN], g_rutinaPerfilGL[PasoN];
static int LlamadasGL() {
    return gfx::g_statDrawTris + gfx::g_statTexBinds + gfx::g_statStateChanges + gfx::g_statOtras;
}

// UN paso. 'i' es el indice en la lista (Saltar si lo avanza). 'yo' = la rutina duenia de la lista
static void EjecutarPasoCrudo(W3dPaso& p, size_t& i, Rutina* yo);
static void EjecutarPaso(W3dPaso& p, size_t& i, Rutina* yo) {
    if (!g_rutinaPerfil) { EjecutarPasoCrudo(p, i, yo); return; }
    const int antes = LlamadasGL();
    const bool anida = (p.tipo == PasoRutina || p.tipo == PasoHijos || p.tipo == PasoLua);
    EjecutarPasoCrudo(p, i, yo);
    g_rutinaPerfilVeces[p.tipo]++;
    if (!anida) g_rutinaPerfilGL[p.tipo] += LlamadasGL() - antes;   // (lo de adentro lo cuentan sus propios pasos)
}
static void EjecutarPasoCrudo(W3dPaso& p, size_t& i, Rutina* yo) {
    switch (p.tipo) {
    // ---- transformar ----
    case PasoApilar:    gfx::PushMatrix(); break;
    case PasoDesapilar: gfx::PopMatrix(); break;
    case PasoTrasladar: gfx::Translatef(p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor()); break;
    case PasoRotar:     gfx::Rotatef(p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.n[3].Valor()); break;
    case PasoEscalar:   gfx::Scalef(p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor()); break;
    case PasoMatrizObjeto: {
        Matrix4 w; ((Object*)p.ptr)->GetWorldMatrix(w);
        if (p.modo == 0) { gfx::MultMatrix(w.m); break; }
        if (!gBaseMVLeida) { gfx::GetMatrix(gfx::ModelView, gBaseMV.m); gBaseMVLeida = true; }
        Matrix4 m = gBaseMV * w;
        gfx::LoadMatrix(m.m); gMVCargada = true;
        break;
    }
    // ---- dibujar ----
    case PasoArray: if (p.on) gfx::EnableArray(kArrayGfx[p.modo]); else gfx::DisableArray(kArrayGfx[p.modo]); break;
    case PasoPunteroVertices: case PasoPunteroNormales: case PasoPunteroUV: case PasoPunteroColores:
        Puntero(p.tipo, p); break;
    case PasoDibujar: Dibujar(p); break;
    case PasoTamPunto:   gfx::PointSize(p.n[0].Valor()); break;
    case PasoAnchoLinea: gfx::LineWidth(p.n[0].Valor()); break;
    case PasoHijos:
        if (yo) {
            ReponerMV();   // (los hijos se apilan sobre la camara)
            yo->RenderHijos();
            // los hijos pudieron bindear y desbindear buffers, poner SUS punteros y prender o apagar arrays: no se
            // sabe que quedo (hay que volver a apuntar; el editor pide fijar los arrays antes del proximo draw)
            gIboActual = 0xFFFFFFFFu; gVboEnUso = true;
            OlvidarPunteros();
        }
        break;
    case PasoRutina: {
        Rutina* r = (Rutina*)p.ptr;
        if (r->sucia) r->Resolver();
        Rutina* antes = gEnCurso; gEnCurso = r;
        r->Ejecutar(r->ListaActual());
        gEnCurso = antes;
        break;
    }
    // ---- textura ----
    case PasoTextura:        if (p.on) gfx::Enable(gfx::Texture2D); else gfx::Disable(gfx::Texture2D); break;
    case PasoFijarTextura:   gfx::BindTexture(((Texture*)p.ptr)->iID); break;
    case PasoTexturaModo:
        // (DOT3: el color de vertice es la direccion de la luz, horneada; solo alfa: RGB del color, alfa de la textura)
        if (p.modo == 2) gfx::TexEnvDot3(true); else if (p.modo == 3) gfx::TexEnvAlphaOnly(true); else gfx::TexEnvReplace(p.modo == 1);
        break;
    case PasoTexturaFiltro:  gfx::TexFilter(p.modo == 1); break;
    case PasoTexturaRepetir: gfx::TexWrap(p.modo == 0); break;
    case PasoMatrizUV: gfx::TexMatrixUV(p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.n[3].Valor()); break;
    case PasoMatcap: {
        // las NORMALES de la malla como UV (de RAM: el GL de PC no acepta bytes en un VBO de UV) y la matriz que las
        // lleva al matcap con la modelview de AHORA: va despues de la matriz del objeto, antes de dibujar
        MallaRecurso* r = (MallaRecurso*)p.ptr;
        if (!p.on || !r || !r->normals) { gfx::TexMatrixMatcap(false); break; }
        if (gVboEnUso) { gfx::UnbindVBOs(); gVboEnUso = false; gIboActual = 0; OlvidarPunteros(); }
        gfx::TexCoordPointer3b(r->normals, r->vertexSize);
        gPtrMalla[ArrayUV] = NULL;
        gfx::TexMatrixMatcap(true);
        break;
    }
    // ---- niebla ----
    case PasoNiebla:         if (p.on) gfx::Enable(gfx::Fog); else gfx::Disable(gfx::Fog); break;
    case PasoNieblaModo:     gfx::FogModo(p.modo); break;
    case PasoNieblaInicio:   gfx::FogStart(p.n[0].Valor()); break;
    case PasoNieblaFin:      gfx::FogEnd(p.n[0].Valor()); break;
    case PasoNieblaDensidad: gfx::FogDensidad(p.n[0].Valor()); break;
    case PasoNieblaColor: { float c[4] = { p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.n[3].Valor() }; gfx::FogColor(c); break; }
    // ---- luces ----
    case PasoLuz:  if (p.on) gfx::Enable(gfx::Lighting); else gfx::Disable(gfx::Lighting); break;
    case PasoLuzN: gfx::LightN(p.entero, p.on); break;
    case PasoLuzColor: {
        static const gfx::LightFv kComp[3] = { gfx::LightAmbient, gfx::LightDiffuse, gfx::LightSpecular };
        float c[4] = { p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.n[3].Valor() };
        gfx::LightNfv(p.entero, kComp[p.modo], c);
        break;
    }
    case PasoLuzPosicion: {
        // (en el espacio de la modelview ACTIVA, como glLightfv: w 0 = direccional, la direccion HACIA la luz)
        float v[4] = { p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.modo == 1 ? 0.0f : 1.0f };
        gfx::LightNfv(p.entero, gfx::LightPosition, v);
        break;
    }
    case PasoLuzAmbiente: { float c[4] = { p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.n[3].Valor() }; gfx::LightModelAmbient(c); break; }
    // ---- alpha test ----
    case PasoAlfaTest: gAlfaOn = p.on; gfx::AlphaTest(p.on ? gAlfaRef : 0.0f); break;
    case PasoAlfaRef:  gAlfaRef = p.n[0].Valor(); if (gAlfaOn) gfx::AlphaTest(gAlfaRef); break;
    // ---- color y material ----
    case PasoColor: gfx::Color4f(p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.n[3].Valor()); break;
    case PasoColorVertice: if (p.on) gfx::Enable(gfx::ColorMaterial); else gfx::Disable(gfx::ColorMaterial); break;
    case PasoMaterialColor: {
        float c[4] = { p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.n[3].Valor() };
        gfx::Material((gfx::MatParam)p.modo, c);
        break;
    }
    case PasoBrillo: gfx::MaterialShininess(p.n[0].Valor()); break;
    case PasoSombreado: gfx::SmoothShading(p.on); break;
    // ---- profundidad y buffers ----
    case PasoTestZ:     if (p.on) gfx::Enable(gfx::DepthTest); else gfx::Disable(gfx::DepthTest); break;
    case PasoEscribirZ: gfx::DepthMask(p.on); break;
    case PasoFuncionZ:  gfx::DepthFunc((gfx::DepthCmp)p.modo); break;
    case PasoSesgoZ:
        if (p.on) {
            float u = p.n[1].Valor();
#ifdef W3D_SYMBIAN
            u *= 4.0f;   // z de 16 bits (N95): el mismo sesgo necesita ~4x mas unidades (como el material)
#endif
            gfx::Enable(gfx::PolygonOffsetFill); gfx::PolygonOffset(p.n[0].Valor(), u);
        } else { gfx::PolygonOffset(0.0f, 0.0f); gfx::Disable(gfx::PolygonOffsetFill); }
        break;
    case PasoSesgoMetros: gfx::SesgoMetros(p.n[0].Valor()); break;   // (la proyeccion corrida: estado de gfx)
    case PasoRangoZ: gfx::DepthRange(p.n[0].Valor(), p.n[1].Valor()); break;
    case PasoColorLimpieza:
        if (!g_w3dLimpiarColorFijo) gfx::ClearColor(p.n[0].Valor(), p.n[1].Valor(), p.n[2].Valor(), p.n[3].Valor());
        break;
    case PasoLimpiar: {
        // glClear de lo elegido. Respeta el scissor (en el editor: el rectangulo del viewport). La profundidad se limpia
        // aunque la escritura de z este apagada (glClear la respetaria y no limpiaria nada): queda prendida
        int bits = 0;
        if ((p.entero & LimpiarColor) && !g_w3dLimpiarColorFijo) { bits |= gfx::ColorBuffer; g_w3dLimpiezasColor++; }
        if (p.entero & LimpiarProfundidad) { bits |= gfx::DepthBuffer; gfx::DepthMask(true); }
        if (p.entero & LimpiarStencil) bits |= gfx::StencilBuffer;
        if (bits) gfx::Clear(bits);
        break;
    }
    // ---- mezcla ----
    case PasoMezcla:     if (p.on) gfx::Enable(gfx::Blend); else gfx::Disable(gfx::Blend); break;
    case PasoMezclaModo: gfx::SetMezcla(p.modo); break;
    // ---- sueltos y control ----
    case PasoCaras: if (p.on) gfx::Enable(gfx::CullFace); else gfx::Disable(gfx::CullFace); break;
    case PasoSaltarSi: if (p.n[0].Valor() == 0.0f) i += (size_t)(p.entero > 0 ? p.entero : 0); break;
    case PasoSaltarOculto: {
        const Object* o = (const Object*)p.ptr;
        while (o && o->visible) o = o->Parent;
        if (o) i += (size_t)(p.entero > 0 ? p.entero : 0);
        break;
    }
    case PasoVisible: *(float*)p.n[0].p = Visible(p) ? 1.0f : 0.0f; break;
    case PasoLua:
        // la funcion del script de la RUTINA, o del objeto que eligio el paso
        W3dScriptEvento(p.ptr2 ? (Object*)p.ptr2 : (Object*)yo, p.ref.c_str(), (Object* const*)NULL, 0, (const float*)NULL, 0);
        OlvidarPunteros();   // (paso() puede haber apuntado a otras mallas: igual pasa por este mismo cache)
        break;
    default: break;
    }
}

void Rutina::RenderObject() {
    ejecutada = false;
    if (sucia) Resolver();
    // el EDITOR no ejecuta una rutina mal armada (el nucleo, si: es responsabilidad de quien la armo). Valida con los
    // arrays como estan en este punto del cuadro
    if (W3dRutinaValidarHook) {
        for (int k = 0; k < ArrayN; k++) arraysInicio[k] = (signed char)gfx::ArrayEstado(kArrayGfx[k]);
        arraysVistos = true;
        if (!W3dRutinaValidarHook(this)) return;
    }
    ejecutada = true;
    if (!precargada) {
        // los VBO y los indices de TODAS sus mallas (tambien las de los bloques que hoy se saltean: el humo, los rivales
        // cercanos): subirlos cuando aparecen era un tiron a mitad de la carrera
        precargada = true;
        std::vector<W3dPaso>& L = ListaActual();
        for (size_t i = 0; i < L.size(); i++) {
            if (W3dPasoRef(L[i].tipo) != RefMalla || !L[i].ptr) continue;
            MallaRecurso* m = (MallaRecurso*)L[i].ptr;
            W3dMallaRecursoAsegurarVBO(m);
            W3dFlujo f;
            if (L[i].tipo == PasoDibujar) W3dMallaFlujo(m, L[i].entero, L[i].modo, f);
        }
    }
    AbrirCorrida();
    Rutina* antes = gEnCurso; gEnCurso = this;
    Ejecutar(ListaActual());
    gEnCurso = antes;
    CerrarCorrida();
}

// con "Dibujar hijos" en la lista, los hijos los dibuja ella (ahi); si no corrio (invalida), el recorrido de siempre
bool Rutina::DibujaSusHijos() { return ejecutada && conHijos[ListaActualIndice()]; }

// el CONSTRUCTOR en modo solo-juego: corre UNA vez por generacion; despues no se recorre y se repone el estado que
// dejo (la foto: solo llama al driver por lo que algo le cambio, el HUD 2D por ejemplo)
bool Rutina::SaltarEsteCuadro() {
    if (!constructor || !g_w3dSoloJuego || !gfx::EstadoFotoSoportada()) return false;
    if (genHecha == g_w3dConstructorGen) { gfx::EstadoFotoReponer(); return true; }
    return false;
}
void Rutina::DespuesDeRender() {
    if (!constructor || !g_w3dSoloJuego || !gfx::EstadoFotoSoportada()) return;
    genHecha = g_w3dConstructorGen;
    gfx::EstadoFotoTomar();   // (la ultima rutina constructor saca la foto con TODO lo del constructor)
}

// ---------------------------------------------------------------------------
//  ANIMACION de los pasos (canal AnimPaso, animation/Animation.h): componente = id * 8 + campo
// ---------------------------------------------------------------------------
static void AnimPasoAplicar(Object* o, int comp, float v) {
    W3dPaso* p = ((Rutina*)o)->PasoPorId((unsigned)(comp / AnimPasoCampos));
    if (!p) return;
    const int campo = comp % AnimPasoCampos;
    if (campo == AnimPasoOn) p->on = (v >= 0.5f);
    else if (campo < 5) p->n[campo].v = v;   // (un numero que nombra una MEMORIA la sigue leyendo de ella)
    g_w3dRutinasGen++;
}
float W3dRutinaPasoValor(Rutina* r, int comp) {
    W3dPaso* p = r ? r->PasoPorId((unsigned)(comp / AnimPasoCampos)) : NULL;
    if (!p) return 0.0f;
    const int campo = comp % AnimPasoCampos;
    return campo == AnimPasoOn ? (p->on ? 1.0f : 0.0f) : (campo < 5 ? p->n[campo].v : 0.0f);
}
namespace { struct RegistrarAnimPaso { RegistrarAnimPaso() { g_animPasoHook = AnimPasoAplicar; } } gRegistrarAnimPaso; }

bool W3dHayConstructor() {
    if (!SceneCollection) return false;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::rutina && ((Rutina*)o)->constructor) return true;
    }
    return false;
}
void W3dRutinasPorDefecto(Object* raiz, const std::string& nombreConstructor, const std::string& nombreLimpiar,
                          const float fondo[4]) {
    if (!raiz) return;
    Rutina* c = new Rutina(raiz, nombreConstructor);
    c->constructor = true;
    W3dPaso col = W3dPasoNuevo(PasoColorLimpieza);   // (el color una vez; la limpieza, cada cuadro)
    for (int k = 0; k < 4; k++) col.n[k].v = fondo[k];
    c->listas[Rutina::ModoDefecto].push_back(col);
    Rutina* l = new Rutina(raiz, nombreLimpiar);
    l->listas[Rutina::ModoDefecto].push_back(W3dPasoNuevo(PasoLimpiar));
    // las dos ARRIBA de todo (el constructor primero): el orden del arbol es el de ejecucion
    std::vector<Object*>& h = raiz->Childrens;
    for (size_t i = 0; i < h.size(); i++) if (h[i] == c || h[i] == l) { h.erase(h.begin() + (long)i); i--; }
    h.insert(h.begin(), (Object*)l);
    h.insert(h.begin(), (Object*)c);
    g_w3dConstructorGen++;
}

void Rutina::Ejecutar(std::vector<W3dPaso>& L) {
    const size_t n = L.size();
    for (size_t i = 0; i < n; i++) EjecutarPaso(L[i], i, this);
}

// ---------------------------------------------------------------------------
//  LUA: paso(...) adentro de una funcion del paso "Llamar Lua"
// ---------------------------------------------------------------------------
bool W3dRutinaPasoDesdeLua(W3dPaso& p, std::string& error) {
    if (gProf <= 0 || !gEnCurso) {
        error = "paso() solo se puede usar mientras se dibuja: adentro de una funcion que llama el paso 'Llamar Lua'";
        return false;
    }
    // los nombres, con un cache (una funcion de lua se llama cada cuadro: buscar por nombre cada vez es caro)
    static std::map<std::string, MallaRecurso*> mallas;
    const int rt = W3dPasoRef(p.tipo);
    if (rt == RefMalla) {
        std::map<std::string, MallaRecurso*>::iterator it = mallas.find(p.ref);
        MallaRecurso* r = NULL;
        if (it != mallas.end() && W3dMallaRecursoVivo(it->second)) r = it->second;
        else {
            r = W3dRutinaBuscarMalla(gEnCurso, p.ref);
            if (r && !W3dMallaRecursoRetener(r)) r = NULL;
            if (r) mallas[p.ref] = r;
        }
        if (!r) { error = "no hay una malla '" + p.ref + "'"; return false; }
        for (int k = 0; k < 5; k++) p.n[k].p = p.n[k].ref.empty() ? NULL : W3dMemoriaRef(p.n[k].ref);
        p.ptr = r;
        p.ptr2 = p.ref2.empty() ? NULL : W3dBuscarNombreDesde(gEnCurso, p.ref2);
    } else {
        ResolverPaso(p, gEnCurso, gEnCurso, NULL);
        if (rt != RefNada && rt != RefFuncion && !p.ptr) { error = "no se encontro '" + p.ref + "'"; return false; }
    }
    if (p.tipo == PasoVisible && !p.n[0].p) { error = "visible: el destino tiene que ser una memoria (@nombre[i])"; return false; }
    if (p.tipo == PasoLua && !p.ref2.empty() && !p.ptr2) { error = "lua: no existe el objeto '" + p.ref2 + "'"; return false; }
    if (p.tipo == PasoDibujar && p.sub == RangoArray && !p.n[2].p) { error = "dibujar: el array tiene que ser una memoria"; return false; }
    size_t i = 0;
    EjecutarPaso(p, i, gEnCurso);
    return true;
}

// ---------------------------------------------------------------------------
//  el .w3d
//    "pasos": [ {"paso": "trasladar", "n": [0, "@autos[3]", 1.5]}, {"paso": "dibujar", "ref": "Carroceria"}, ... ]
//    "listas": { "solido": [ ...pasos... ] }    las propias de cada modo (solo si tienen algo o se usan)
//    "usar":   { "solido": "solido", "alambre": "material" }   que lista usa cada modo (falta = la de todos)
//    "constructor": true                         la seccion de arriba del outliner
//    "version": 2                                la de los pasos (sin version = 1: se convierte al leer)
//  Cada paso: "paso" (nombre), y si no valen lo de siempre: "on" (false), "entero", "modo", "ref", "ref2", "n".
//  "Dibujar elementos" guarda la primitiva por NOMBRE ("primitiva": "tira") y el rango ("rango": 1 manual, 2 array).
// ---------------------------------------------------------------------------
// la version de los pasos: 1 = los punteros prendian su array y "limpiar" llevaba el color; 2 = "Array on/off" y
// "Clear color" son pasos aparte (lo de la 1 se convierte al leer: LeerLista)
static const int kVersionPasos = 2;
static const char* const kModos[Rutina::ModoN] = { "defecto", "solido", "material", "render", "alambre", "zbuffer" };
const char* W3dRutinaModoNombre(int m) { return (m >= 0 && m < Rutina::ModoN) ? kModos[m] : "defecto"; }
static int ModoDesdeNombre(const std::string& n) {
    for (int m = 0; m < Rutina::ModoN; m++) if (n == kModos[m]) return m;
    return -1;
}
static void RuSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
static void RuEsc(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const char c = v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += c; }
        else if (c == '\n') s += "\\n";
        else s += c;
    }
    s += '"';
}
static void EscribirLista(std::string& s, int ind, std::vector<W3dPaso>& L,
                          std::string (*emitirTextura)(std::string&, void*), void* ctx) {
    s += "[";
    for (size_t i = 0; i < L.size(); i++) {
        W3dPaso& p = L[i];
        char b[16];
        s += (i ? ",\n" : "\n"); RuSang(s, ind + 1);
        s += "{\"paso\": "; RuEsc(s, W3dPasoNombre(p.tipo));
        if (p.id) { snprintf(b, sizeof b, "%u", (unsigned)p.id); s += ", \"id\": "; s += b; }
        if (!p.on) s += ", \"on\": false";
        if (p.entero != 0) { s += ", \"entero\": "; snprintf(b, sizeof b, "%d", p.entero); s += b; }
        if (p.tipo == PasoDibujar) {
            if (p.modo != RPrimTriangulos) { s += ", \"primitiva\": "; RuEsc(s, W3dPrimitivaNombre(p.modo)); }
            if (p.sub != 0) { s += ", \"rango\": "; snprintf(b, sizeof b, "%d", p.sub); s += b; }
        } else if (p.modo != 0) { s += ", \"modo\": "; snprintf(b, sizeof b, "%d", p.modo); s += b; }
        if (!p.ref.empty()) {
            s += ", \"ref\": ";
            if (emitirTextura && W3dPasoRef(p.tipo) == RefTextura && p.ptr) {
                // la textura entra al .w3d (como la de un material); en memoria queda su ruta nueva
                Texture* tx = (Texture*)p.ptr;
                RuEsc(s, emitirTextura(tx->path, ctx));
                p.ref = tx->path;
            } else RuEsc(s, p.ref);
        }
        if (!p.ref2.empty()) { s += ", \"ref2\": "; RuEsc(s, p.ref2); }
        const int nn = W3dPasoNumeros(p);
        if (nn > 0) {
            s += ", \"n\": [";
            for (int k = 0; k < nn; k++) {
                if (k) s += ", ";
                if (!p.n[k].ref.empty()) RuEsc(s, p.n[k].ref); else s += JsonNumTexto(p.n[k].v);
            }
            s += "]";
        }
        s += "}";
    }
    if (!L.empty()) { s += "\n"; RuSang(s, ind); }
    s += "]";
}
void RutinaEscribirCampos(std::string& s, int ind, Rutina* r, std::string (*emitirTextura)(std::string&, void*), void* ctx) {
    if (r->sucia) r->Resolver();   // (los punteros de las texturas: su ruta en memoria es la que se emite)
    if (r->constructor) { s += ",\n"; RuSang(s, ind); s += "\"constructor\": true"; }
    { char b[32]; snprintf(b, sizeof b, "\"version\": %d", kVersionPasos); s += ",\n"; RuSang(s, ind); s += b; }
    s += ",\n"; RuSang(s, ind); s += "\"pasos\": "; EscribirLista(s, ind, r->listas[Rutina::ModoDefecto], emitirTextura, ctx);
    bool hay = false;
    for (int m = 1; m < Rutina::ModoN; m++) {
        bool usada = false;
        for (int k = 0; k < Rutina::ModoN; k++) if (r->usar[k] == m) usada = true;
        if (r->listas[m].empty() && !usada) continue;
        if (!hay) { s += ",\n"; RuSang(s, ind); s += "\"listas\": {\n"; } else s += ",\n";
        RuSang(s, ind + 1);
        hay = true;
        RuEsc(s, kModos[m]); s += ": "; EscribirLista(s, ind + 1, r->listas[m], emitirTextura, ctx);
    }
    if (hay) { s += "\n"; RuSang(s, ind); s += "}"; }
    bool hayU = false;
    for (int m = 1; m < Rutina::ModoN; m++) {
        if (r->usar[m] < 0) continue;
        if (!hayU) { s += ",\n"; RuSang(s, ind); s += "\"usar\": {"; } else s += ", ";
        hayU = true;
        RuEsc(s, kModos[m]); s += ": "; RuEsc(s, kModos[(int)r->usar[m]]);
    }
    if (hayU) s += "}";
}
// un paso de un tipo con UN numero (copiado de otro): los pasos viejos se parten en estos
static W3dPaso PasoConNumero(int tipo, const W3dNum& n) {
    W3dPaso p = W3dPasoNuevo(tipo);
    p.n[0] = n; p.n[0].p = NULL;
    return p;
}
static W3dPaso PasoPuntero(int tipo, const std::string& malla) {
    W3dPaso p = W3dPasoNuevo(tipo);
    p.ref = malla;
    return p;
}
// (version 1) el "Array on" de un puntero viejo: el puntero prendia su array, salvo que la malla no lo tuviera (lo
// dejaba apagado). Eso se sabe al resolver la malla: 'sub' = 1 lo marca (Rutina::Resolver lo corrige y lo borra)
static W3dPaso PasoArrayV1(int k, bool on, bool segunLaMalla) {
    W3dPaso p = W3dPasoNuevo(PasoArray);
    p.modo = k; p.on = on; p.sub = segunLaMalla ? 1 : 0;
    return p;
}
static void LeerLista(JVal* l, std::vector<W3dPaso>& L, std::string (*refTextura)(const std::string&, void*), void* ctx,
                      int version) {
    L.clear();
    if (!l) return;
    // cada paso del archivo da UNO o MAS pasos (los viejos se parten; uno desconocido, ninguno): un ITEM por paso del
    // archivo. Los "Saltar si" cuentan pasos del archivo: al final se recuentan en pasos nuevos
    std::vector<std::vector<W3dPaso> > items;
    bool hayPunteros = false, hayDibujar = false;
    for (size_t i = 0; i < l->lista.size(); i++) {
        JVal* o = l->lista[i];
        if (!o || o->tipo != 4) continue;
        items.push_back(std::vector<W3dPaso>());
        std::vector<W3dPaso>& it = items.back();
        const std::string nombre = JS(o, "paso", "");
        W3dNum num[5];
        JVal* n = JHijo(o, "n", 5);
        if (n) for (size_t k = 0; k < 5 && k < JFilaLen(n); k++) {
            if (JFilaEsNum(n, k)) num[k].v = JFilaNum(n, k, 0.0f);
            else if (k < n->lista.size() && n->lista[k] && n->lista[k]->tipo == 2) num[k].ref = n->lista[k]->str;
        }
        const bool on = JB(o, "on", true);
        const int entero = JI(o, "entero", 0);
        // ---- los pasos de la primera version (un paso con varios estados) se parten en los estados sueltos ----
        if (nombre == "niebla") {           // on, modo (entero), inicio, fin, densidad, gris, alfa
            W3dPaso a = W3dPasoNuevo(PasoNiebla); a.on = on; it.push_back(a);
            if (!on) continue;
            W3dPaso b = W3dPasoNuevo(PasoNieblaModo); b.modo = entero; it.push_back(b);
            it.push_back(PasoConNumero(PasoNieblaInicio, num[0]));
            it.push_back(PasoConNumero(PasoNieblaFin, num[1]));
            it.push_back(PasoConNumero(PasoNieblaDensidad, num[2]));
            W3dPaso c = W3dPasoNuevo(PasoNieblaColor);
            c.n[0] = c.n[1] = c.n[2] = num[3]; c.n[3] = num[4];
            it.push_back(c);
            continue;
        }
        if (nombre == "profundidad") {      // test on/off + escribe z (entero)
            W3dPaso a = W3dPasoNuevo(PasoTestZ); a.on = on; it.push_back(a);
            W3dPaso b = W3dPasoNuevo(PasoEscribirZ); b.on = (entero != 0); it.push_back(b);
            continue;
        }
        if (nombre == "mezcla") {           // el modo (entero): 0 = apagada
            W3dPaso a = W3dPasoNuevo(PasoMezcla); a.on = (entero != 0); it.push_back(a);
            if (entero != 0) { W3dPaso b = W3dPasoNuevo(PasoMezclaModo); b.modo = entero; it.push_back(b); }
            continue;
        }
        const int t = W3dPasoDesdeNombre(nombre);
        if (t < 0) continue;                         // un paso que esta version no conoce: se saltea
        W3dPaso p = W3dPasoNuevo(t);
        p.id = (unsigned short)JI(o, "id", 0);
        p.on = on;
        p.entero = entero;
        p.ref = JS(o, "ref", "");
        p.ref2 = JS(o, "ref2", "");
        if (t == PasoDibujar) {
            p.modo = PrimitivaDesdeNombre(JS(o, "primitiva", "triangulos"));
            // (la version anterior guardaba el rango en "modo") y "con sus materiales" (on) ya no existe
            p.sub = JI(o, "rango", JI(o, "modo", 0));
            p.on = true;
            hayDibujar = true;
        } else {
            p.modo = JI(o, "modo", 0);
        }
        if (refTextura && W3dPasoRef(t) == RefTextura && !p.ref.empty()) p.ref = refTextura(p.ref, ctx);
        if (n) for (size_t k = 0; k < 5 && k < JFilaLen(n); k++) p.n[k] = num[k];
        if (version < 2 && t == PasoLimpiar) {
            // (version 1) un solo "Clear": on = el color (con sus 4 numeros), entero = la profundidad
            if (on) { W3dPaso c = W3dPasoNuevo(PasoColorLimpieza); for (int k = 0; k < 4; k++) c.n[k] = num[k]; it.push_back(c); }
            W3dPaso q = W3dPasoNuevo(PasoLimpiar);
            q.entero = (on ? (int)LimpiarColor : 0) | (entero ? (int)LimpiarProfundidad : 0);
            it.push_back(q);
            continue;
        }
        if (version < 2 && t >= PasoPunteroVertices && t <= PasoPunteroColores) {
            // (version 1) el puntero prendia o apagaba su array; sin malla SOLO prendia o apagaba
            hayPunteros = true;
            const int k = t - PasoPunteroVertices;
            p.on = true;
            if (p.ref.empty())  it.push_back(PasoArrayV1(k, on, false));
            else if (k == 0)    it.push_back(p);
            else if (!on)       it.push_back(PasoArrayV1(k, false, false));
            else { it.push_back(PasoArrayV1(k, true, true)); it.push_back(p); }
            continue;
        }
        it.push_back(p);
    }
    // una lista VIEJA (de antes de los punteros: "Dibujar malla" ponia todo solo): antes de cada draw van sus
    // cuatro punteros, con su array prendido si la malla lo tiene. Los materiales ya no los pone el draw: se ven con
    // el estado que haya. (antes de CADA draw: un "Saltar si" puede saltear el bloque que los ponia para el anterior)
    if (version < 2 && hayDibujar && !hayPunteros) {
        for (size_t i = 0; i < items.size(); i++) {
            if (items[i].size() != 1 || items[i][0].tipo != PasoDibujar) continue;
            const W3dPaso d = items[i][0];
            std::vector<W3dPaso> v;
            v.push_back(PasoPuntero(PasoPunteroVertices, d.ref));
            for (int k = 1; k < 4; k++) { v.push_back(PasoArrayV1(k, true, true)); v.push_back(PasoPuntero(PasoPunteroVertices + k, d.ref)); }
            v.push_back(d);
            items[i].swap(v);
        }
    }
    // aplanar y recontar los "Saltar si": saltear c pasos del archivo = saltear los pasos nuevos de esos c items
    std::vector<size_t> inicio(items.size() + 1, 0);
    for (size_t i = 0; i < items.size(); i++) inicio[i + 1] = inicio[i] + items[i].size();
    for (size_t i = 0; i < items.size(); i++)
        for (size_t k = 0; k < items[i].size(); k++) {
            W3dPaso q = items[i][k];
            if ((q.tipo == PasoSaltarSi || q.tipo == PasoSaltarOculto) && q.entero > 0) {
                size_t fin = i + (size_t)q.entero;
                if (fin >= items.size()) fin = items.size() - 1;
                q.entero = (int)(inicio[fin + 1] - inicio[i + 1]);
            }
            L.push_back(q);
        }
}
void RutinaLeerCampos(JVal* j, Rutina* r, std::string (*refTextura)(const std::string&, void*), void* ctx) {
    if (!j || !r) return;
    r->constructor = JB(j, "constructor", false);
    if (r->constructor) g_w3dConstructorGen++;   // (un proyecto que se abre: su constructor corre de nuevo)
    const int version = JI(j, "version", 1);     // (sin version: la 1, la de los punteros que prendian su array)
    LeerLista(JHijo(j, "pasos", 5), r->listas[Rutina::ModoDefecto], refTextura, ctx, version);
    JVal* ls = JHijo(j, "listas", 4);
    if (ls) for (std::map<std::string, JVal*>::iterator it = ls->obj.begin(); it != ls->obj.end(); ++it) {
        const int m = ModoDesdeNombre(it->first);
        if (m > 0 && it->second->tipo == 5) LeerLista(it->second, r->listas[m], refTextura, ctx, version);
    }
    JVal* us = JHijo(j, "usar", 4);
    if (us) for (std::map<std::string, JVal*>::iterator it = us->obj.begin(); it != us->obj.end(); ++it) {
        const int m = ModoDesdeNombre(it->first);
        const int u = (it->second->tipo == 2) ? ModoDesdeNombre(it->second->str) : -1;
        if (m > 0) r->usar[m] = (signed char)(u > 0 ? u : -1);
    }
    // (sin transformacion: lo que traiga el archivo no cuenta)
    r->pos = Vector3(0, 0, 0); r->scale = Vector3(1, 1, 1); r->SetRot(Quaternion());
    r->sucia = true;
}
