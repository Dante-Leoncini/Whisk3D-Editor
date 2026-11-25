#include "W3dLang.h"   // T(): los avisos salen en el idioma del editor
#include "import_obj.h"
#include "render/RenderOptions.h"   // RenderType / g_redraw: son del editor
#include "w3dVersion.h" // W3dVersion() para el header del .obj exportado
#include <sstream>
#include <stdlib.h>
#include <algorithm>
#include <set>
#include <map>
#include "objects/ObjectMode.h" // RotGlobalOf (normales a mundo en el export)
#include "ViewPorts/PopUp/ProgressPopup.h" // barra de progreso (export/import, clave en el N95 lento)
#include "ViewPorts/LayoutInput.h"          // Notificar (toast de exito/error)
#include "w3dlog.h" // diagnostico del import (clave en N95: ubicar donde crashea)
#include "w3dFilesystem.h" // lectura de archivos UNIFICADA del Core (un solo camino por OS)
#include <cstdio> // sprintf para FloatStr
#include <cerrno>  // errno del ofstream fallido (diagnostico del export en Android)
#include <cstring> // strerror
#include "graphics/w3dGraphics.h" // gfx::MixAdd: la palabra ADITIVO del .mtl
#include "graphics/w3dTexture.h"  // TexAlphaDiscard: la luz horneada con la mascara del sol, opaca en el pipeline fijo
#include "io/W3dMeshBin.h"        // W3dMeshBinMaterializeEdit: las caras pendientes de una malla de un recurso
#include "objects/MeshResource.h" // W3dMeshReleaseEdit: y vuelven a quedar pendientes al terminar
#include "io/W3dWeb.h"            // W3dDownloadIfWeb: en el navegador las texturas del export bajan a la PC

// formateo de float SIN operator<< ni %f: ambos ROTOS en STLport/Symbian (el export escribia basura tipo
// 2.31e-307 Y era lentisimo -> el N95 se "colgaba"). Replica de RenderBitmapFloat: entero.fraccion con %d.
static std::string FloatStr(float v) {
    char buf[40];
    int neg = (v < 0.0f) ? 1 : 0;
    float av = neg ? -v : v;
    int ent = (int)av;
    int frac = (int)((av - (float)ent) * 1000000.0f + 0.5f); // 6 decimales
    if (frac >= 1000000) { ent++; frac -= 1000000; }
    sprintf(buf, "%s%d.%06d", neg ? "-" : "", ent, frac);
    return std::string(buf);
}

// int -> string (para los indices de las caras del export). SSO (sin heap para enteros chicos).
static std::string IntStr(int n) {
    char buf[16]; sprintf(buf, "%d", n); return std::string(buf);
}

// ===== APPEND directo al buffer del export, SIN sprintf (era el cuello: ~2.6M sprintf para 200k verts = ~7s) ni
// std::string temporal. Extraen los digitos a mano. AppendFloat replica EXACTO el formato de FloatStr (signo +
// entero + '.' + 6 decimales redondeados) para que el round-trip no cambie. =====
static void AppendInt(std::string& out, int v) {
    if (v < 0) { out += '-'; v = -v; }
    char tmp[12]; int i = 0;
    if (v == 0) tmp[i++] = '0';
    else while (v > 0) { tmp[i++] = (char)('0' + v % 10); v /= 10; }
    while (i > 0) out += tmp[--i]; // los digitos salieron al reves
}
static void AppendFloat(std::string& out, float v) {
    int neg = (v < 0.0f) ? 1 : 0;
    float av = neg ? -v : v;
    int ent = (int)av;
    int frac = (int)((av - (float)ent) * 1000000.0f + 0.5f); // 6 decimales, redondeado
    if (frac >= 1000000) { ent++; frac -= 1000000; }
    if (neg) out += '-';
    AppendInt(out, ent);  // ent >= 0 aca
    out += '.';
    char fd[6]; // fraccion: 6 digitos con ceros a la izquierda
    for (int k = 5; k >= 0; k--) { fd[k] = (char)('0' + frac % 10); frac /= 10; }
    out.append(fd, 6);
}

bool VertexKey::operator==(const VertexKey &other) const {
    return pos == other.pos && normal == other.normal
           && uv == other.uv && color == other.color && uv2 == other.uv2;
}

bool VertexKey::operator<(const VertexKey &other) const {
    if (pos != other.pos) return pos < other.pos;
    if (normal != other.normal) return normal < other.normal;
    if (uv != other.uv) return uv < other.uv;
    if (color != other.color) return color < other.color;
    return uv2 < other.uv2;
}

#ifndef W3D_SYMBIAN
size_t std::hash<VertexKey>::operator()(const VertexKey &k) const {
    return ((size_t)k.pos * 73856093) ^ ((size_t)k.normal * 19349663)
           ^ ((size_t)k.uv * 83492791) ^ ((size_t)k.color * 49979693) ^ ((size_t)(k.uv2 + 1) * 15485863);
}
#endif

#ifdef W3D_SYMBIAN
typedef std::map<VertexKey, MeshIndex> TVertexMap;
typedef std::map<std::string, int> TPosMap;            // STLport: sin unordered_map (export en N95 = modelos chicos)
#else
typedef std::unordered_map<VertexKey, MeshIndex> TVertexMap;
typedef std::unordered_map<std::string, int> TPosMap;  // HASH: dedup de posiciones del export O(n) en vez de O(n log n)
#endif

// directorio de un path (reemplaza a std::filesystem: portable a C++03)
static std::string DirOf(const std::string& path) {
    size_t i = path.find_last_of("/\\");
    return (i == std::string::npos) ? std::string("") : path.substr(0, i + 1);
}

// saturar [0..1] -> byte (era una lambda: C++03)
static unsigned char ObjSaturate(double v) {
    double n = v * 255.0;
    if (n < 0) n = 0;
    if (n > 255.0) n = 255.0;
    return (unsigned char)n;
}

// normal [-1..1] -> GLbyte (era una lambda: C++03)
static signed char ObjConvNormal(double v) {
    v = ((v + 1.0) / 2.0) * 255.0 - 128.0;
    if (v > 127) v = 127;
    if (v < -128) v = -128;
    return (signed char)v;
}

// ===== CARGA DIFERIDA DE TEXTURAS (el import de modelos grandes tardaba ~17s SOLO decodificando los PNG del
// MTL -upper 24MB, lower 18MB...-). Ahora el import NO decodifica: ENCOLA (material, ruta) y el modelo aparece
// enseguida (gris). El loop principal (PC y N95) llama LoadPendingTextures() 1 vez por frame -> decodifica +
// sube UNA textura por frame (hilo principal, sin threads) y la asigna; las texturas "aparecen" solas. =====
struct PendingTex { Material* mat; std::string path; };
static std::vector<PendingTex> g_pendingTex;
static size_t g_texPendIdx = 0;

void EnqueueTexture(Material* mat, const std::string& path) { // (la usa tambien el importador FBX)
    PendingTex tp; tp.mat = mat; tp.path = path; g_pendingTex.push_back(tp);
}

// la textura que 'mat' tiene ENCOLADA y todavia no se decodifico ("" si no hay). La necesita el
// importador glTF para comparar dos materiales homonimos: recien importado, mat->texture sigue
// NULL (la carga es diferida) y sin esto dos materiales con texturas DISTINTAS parecian iguales.
// TEXTURAS DORMIDAS (ver import_obj.h): las de un material que al abrir no usaba ninguna malla. Su
// textura BASE cuenta como "encolada" (PendingTextureOf / RefDe la devuelven): para el guardado, el
// outliner y el rename es la misma ruta que espera su turno, solo que no se carga hasta despertar.
static std::vector<AsleepTex> g_asleepTex;

std::string PendingTextureOf(const Material* mat) {
    for (size_t i = g_texPendIdx; i < g_pendingTex.size(); i++)
        if (g_pendingTex[i].mat == mat) return g_pendingTex[i].path;
    const AsleepTex* d = AsleepTexturesOf(mat);
    return (d && !d->base.empty()) ? d->base : std::string();
}

// LA MISMA ENTRADA, PERO ESCRIBIBLE. El guardado no solo LEE la ruta: la INGIERE al
// contenedor y la reemplaza por el nombre de entrada ("textures/piso.png"). Con la
// version de arriba (que devuelve una COPIA) esa reescritura se perdia y la cola se
// quedaba con la ruta de DISCO: al reabrir, W3dSameMaterial comparaba "/home/.../
// piso.png" contra "textures/piso.png", no matcheaban, y CADA material importado
// volvia duplicado como ".001". Pasaba siempre que se guardara antes de que la carga
// diferida vaciara la cola (headless, y cualquier guardado inmediato al importar).
std::string* PendingTextureRefOf(const Material* mat) {
    for (size_t i = g_texPendIdx; i < g_pendingTex.size(); i++)
        if (g_pendingTex[i].mat == mat) return &g_pendingTex[i].path;
    AsleepTex* d = AsleepTexturesOf(mat);
    return (d && !d->base.empty()) ? &d->base : NULL;
}

void TexturesSleep(const AsleepTex& d) {
    bool anyMap = false;
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) if (!d.maps[k].empty()) anyMap = true;
    if (!d.mat || (d.base.empty() && d.normal.empty() && d.texB.empty() && d.layers.empty() && !anyMap)) return;
    AsleepTex* already = AsleepTexturesOf(d.mat);
    if (already) *already = d; else g_asleepTex.push_back(d);
}
AsleepTex* AsleepTexturesOf(const Material* mat) {
    if (!mat) return NULL;
    for (size_t i = 0; i < g_asleepTex.size(); i++) if (g_asleepTex[i].mat == mat) return &g_asleepTex[i];
    return NULL;
}
int AsleepTexturesCount() { return (int)g_asleepTex.size(); }

// DESPIERTA una: la base a la cola diferida (1 por cuadro, como al abrir) y el normal map y las capas
// ya. Lo que el material haya recibido mientras dormia (una textura asignada a mano) MANDA: la dormida
// no la pisa. Las capas dormidas eran las primeras del material: van adelante.
static void WakeOne(const AsleepTex& d) {
    Material* m = d.mat;
    if (!d.base.empty() && !m->texture && PendingTextureRefOf(m) == NULL) {
        // YA EN MEMORIA (el streaming la trajo antes de generar, u otro material la usa): se pone ahora, sin esperar
        // su turno en la cola (no toca el disco ni la GPU: es la referencia de la ranura). Como la cola, la prende
        if (TextureFind(d.base)) { TextureSetIn(m->texture, TextureFind(d.base)); m->textureOn = true; }
        else EnqueueTexture(m, d.base);
    }
    if (!d.normal.empty() && !m->normalTexture) {
        m->normalTexture = TextureTake(d.normal);
        if (!m->normalTexture) w3dLogfW("[W3D] no pude cargar la textura '%s'", d.normal.c_str());
    }
    if (!d.texB.empty() && !m->textureB) {   // la textura B (mezcla de texturas): como el normal map
        m->textureB = TextureTake(d.texB);
        if (!m->textureB) w3dLogfW("[W3D] no pude cargar la textura '%s'", d.texB.c_str());
    }
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++)   // los MAPAS (emision, rugosidad, metalico, alfa): como el normal
        if (!d.maps[k].empty() && !m->mapTex[k]) {
            m->mapTex[k] = TextureTake(d.maps[k]);
            if (!m->mapTex[k]) w3dLogfW("[W3D] no pude cargar la textura '%s'", d.maps[k].c_str());
        }
    std::vector<TexLayer> layers;
    for (size_t c = 0; c < d.layers.size(); c++) {
        // (la LUZ HORNEADA con la mascara del sol en el alfa: donde no hay sombras por shader -el pipeline fijo: N95,
        //  PSP, 3DS, Dreamcast- el alfa no sirve y la textura sube OPACA, en 16 bits como siempre)
        const bool noAlpha = d.layers[c].maskSun && !w3dEngine::ShadowsSupported();
        if (noAlpha) w3dEngine::TexAlphaDiscard(true);
        Texture* t = TextureTake(d.layers[c].texture);
        if (noAlpha) w3dEngine::TexAlphaDiscard(false);
        if (!t) { w3dLogfW("[W3D] no pude cargar la textura '%s'", d.layers[c].texture.c_str()); continue; }
        TexLayer tl; tl.tex = t; tl.blend = d.layers[c].mix; tl.uvMap = d.layers[c].uv; tl.on = d.layers[c].on;
        tl.force = d.layers[c].force; tl.baked = d.layers[c].baked; tl.maskSun = d.layers[c].maskSun;
        layers.push_back(tl);
    }
    if (!layers.empty()) m->layers.insert(m->layers.begin(), layers.begin(), layers.end());
    w3dLogf("[W3D] material '%s': una malla lo usa, sus texturas se cargan", m->name.c_str());
}
// los materiales que dibuja alguna malla de la escena (partes y lo generado por los modificadores)
static void CollectMaterialsUsed(Object* o, std::set<const Material*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::mesh) {
            const Mesh* m = (const Mesh*)h;
            for (size_t g = 0; g < m->materialsGroup.size(); g++) out.insert(m->materialsGroup[g].material);
            for (size_t g = 0; g < m->genMaterialsGroup.size(); g++) out.insert(m->genMaterialsGroup[g].material);
        }
        CollectMaterialsUsed(h, out);
    }
}
bool TexturesSleepMaterial(Material* m) {
    if (!m || m == MaterialDefault || AsleepTexturesOf(m)) return false;
    // un material ANIMADO: sus cuadros pasan por la ranura base sin referencia propia (son del material animado)
    for (size_t i = 0; i < AnimatedMaterials.size(); i++)
        if (AnimatedMaterials[i])
            for (size_t t = 0; t < AnimatedMaterials[i]->targets.size(); t++)
                if (AnimatedMaterials[i]->targets[t] == m) return false;
    AsleepTex d;
    d.mat = m;
    if (m->texture) d.base = m->texture->path;
    // (la base que esperaba en la cola diferida tambien duerme: su entrada se anula -sin material no se carga ni la
    //  nombra el guardado- y su ruta queda con las dormidas)
    for (size_t i = g_texPendIdx; i < g_pendingTex.size(); i++)
        if (g_pendingTex[i].mat == m) {
            if (d.base.empty()) d.base = g_pendingTex[i].path;
            g_pendingTex[i].mat = NULL;
        }
    if (m->normalTexture) d.normal = m->normalTexture->path;
    if (m->textureB) d.texB = m->textureB->path;
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) if (m->mapTex[k]) d.maps[k] = m->mapTex[k]->path;
    // las capas duermen TODAS o ninguna (despiertan adelante de las que tenga: el orden se conserva)
    bool layersAll = !m->layers.empty();
    for (size_t c = 0; c < m->layers.size(); c++) if (!m->layers[c].tex) layersAll = false;
    std::vector<Texture*> loose;
    if (layersAll) {
        for (size_t c = 0; c < m->layers.size(); c++) {
            TexAsleepLayer cd;
            cd.texture = m->layers[c].tex->path; cd.mix = m->layers[c].blend; cd.uv = m->layers[c].uvMap; cd.on = m->layers[c].on;
            cd.force = m->layers[c].force; cd.baked = m->layers[c].baked;
            d.layers.push_back(cd);
            loose.push_back(m->layers[c].tex);
        }
        m->layers.clear();
    }
    bool maps = false;
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) if (!d.maps[k].empty()) maps = true;
    if (d.base.empty() && d.normal.empty() && d.texB.empty() && d.layers.empty() && !maps) return false;
    loose.push_back(m->texture);
    loose.push_back(m->normalTexture);
    loose.push_back(m->textureB);
    m->texture = NULL;
    m->normalTexture = NULL;
    m->textureB = NULL;
    // los mapas (emision...) tambien: se anotaban pero seguian en la ranura, asi que no salian de la GPU y al despertar
    // WakeOne los salteaba
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) { loose.push_back(m->mapTex[k]); m->mapTex[k] = NULL; }
    TexturesSleep(d);
    // las ranuras sueltan su referencia (la textura que ningun otro material retiene se libera YA, tambien en el editor)
    for (size_t i = 0; i < loose.size(); i++) TextureReleaseAsleep(loose[i]);
    return true;
}
int TexturesWakeUsed() {
    if (g_asleepTex.empty() || !SceneCollection) return 0;
    std::set<const Material*> used;
    CollectMaterialsUsed(SceneCollection, used);
    std::vector<AsleepTex> wake;
    for (size_t i = 0; i < g_asleepTex.size(); ) {
        if (used.count(g_asleepTex[i].mat)) {
            wake.push_back(g_asleepTex[i]);
            g_asleepTex.erase(g_asleepTex.begin() + (long)i);
        } else i++;
    }
    for (size_t i = 0; i < wake.size(); i++) WakeOne(wake[i]);
    return (int)wake.size();
}
bool TexturesWake(Material* mat) {
    for (size_t i = 0; i < g_asleepTex.size(); i++) {
        if (g_asleepTex[i].mat != mat) continue;
        const AsleepTex d = g_asleepTex[i];
        g_asleepTex.erase(g_asleepTex.begin() + (long)i);
        WakeOne(d);
        return true;
    }
    return false;
}
// una malla que PASA a usar un material dormido: se revisa la escena cuando nacieron objetos (un
// duplicado, un anexo del streaming) y, en el editor, cada tanto (asignarle un material a una malla que
// ya existe no crea objetos). Sin dormidas no cuesta nada; con, un recorrido cada 16 cuadros.
static void CheckAsleep() {
    if (g_asleepTex.empty()) return;
    static unsigned int seen = 0;
    static int frames = 0;
    frames++;
    const unsigned int spawned = W3dObjectsSpawned();
    bool check = (spawned != seen && frames >= 4);
#ifndef W3D_NO_EDITOR
    // editando (un material asignado a mano no hace nacer nada) cada 16 cuadros; jugando, como el juego compilado:
    // recorrer todos los materiales de la escena cada 16 cuadros era un tiron periodico en el N95
    { extern bool W3dGameRunning(); if (frames >= 16 && !W3dGameRunning()) check = true; }
#endif
    if (!check) return;
    seen = spawned;
    frames = 0;
    TexturesWakeUsed();
}

// una textura de la cola (sin la revision de las dormidas: CargarTodas la hace una vez antes)
static void LoadPendingOne() {
    if (g_texPendIdx >= g_pendingTex.size()) {
        if (!g_pendingTex.empty()) { g_pendingTex.clear(); g_texPendIdx = 0; } // termino -> liberar la cola
        return;
    }
    extern bool g_redraw;
    PendingTex& tp = g_pendingTex[g_texPendIdx++];
    // el material pudo borrarse mientras tanto -> cargar solo si sigue vivo en Materials (sino quedaria colgado)
    bool live = false;
    for (size_t i = 0; i < Materials.size(); i++) if (Materials[i] == tp.mat) { live = true; break; }
    if (live) {
        // CACHE POR RUTA (Textures.h): si esa MISMA ruta ya esta cargada devuelve
        // la que hay y solo suma una referencia. Antes esto hacia `new Texture()`
        // + LoadTexture a ciegas: 28 trozos de escenario medidos, todos con el
        // mismo atlas subian 28 copias de 1 MB a la GPU (y las 43 cajas, 43).
        // Si el material YA tenia una textura (reimportar encima), se suelta la
        // vieja: sin eso el refcount nunca baja y no se libera nada.
        Texture* prev = tp.mat->texture;
        Texture* tex = TextureTake(tp.path);
        if (tex) {
            tp.mat->texture = tex;
            tp.mat->textureOn = true;
            if (prev && prev != tex) TextureRelease(prev);
        }
    }
    g_redraw = true; // redibujar con la textura recien cargada (las texturas "aparecen")
}

void LoadPendingTextures() {
    CheckAsleep();
    LoadPendingOne();
}

// cuantas quedan en la cola sin decodificar. Lo mira el PLAY: no se puede
// arrancar el juego con la mitad de las texturas en gris (reporte del dueno).
// Las DORMIDAS no cuentan: son de materiales que nadie dibuja.
int PendingTextures() {
    return (g_texPendIdx < g_pendingTex.size())
         ? (int)(g_pendingTex.size() - g_texPendIdx) : 0;
}

// los binds de lua texturasPendientes() / cargarTexturas() (script/W3dScript.h: W3dTexturesLuaHook)
extern int (*W3dTexturesLuaHook)(int load);
namespace {
int TexturesLua(int load) {
    if (load) LoadAllPendingTextures();
    return PendingTextures();
}
struct RegisterTexturesLua { RegisterTexturesLua() { W3dTexturesLuaHook = TexturesLua; } } gRegisterTexturesLua;
}

// tira la cola entera SIN cargar nada (cierre de proyecto): las entradas
// apuntan a materiales del proyecto que se esta cerrando. Las dormidas tambien.
void ForgetPendingTextures() {
    g_pendingTex.clear();
    g_texPendIdx = 0;
    g_asleepTex.clear();
}

// vacia la cola ENTERA de una (la usa el export: la textura tiene que estar
// cargada YA para poder escribirla; antes se simulaba con 100000 llamadas).
// Antes despierta las dormidas que alguna malla ya usa (esas tambien se dibujan).
void LoadAllPendingTextures() {
    TexturesWakeUsed();
    while (g_texPendIdx < g_pendingTex.size()) LoadPendingOne();
}

void Wavefront::Reset() {
    vertex.clear();
    vertexColor.clear();
    cornerColors.clear();
    normals.clear();
    uv.clear();
    uv2.clear();
    faces.clear();
    looseEdges.clear();
    materialsGroup.clear();
    facesSize = 0;
    facesCount = 0;
}

void Wavefront::ConvertToES1(Mesh* TempMesh, int* accumulatedVertices, int* accumulatedNormals, int* accumulatedUVs,
                             std::vector<int>* vertToCP) {
    (void)accumulatedVertices; (void)accumulatedNormals; (void)accumulatedUVs; // ya se restaron al parsear
    if (vertToCP) vertToCP->clear();
    std::vector<GLfloat> newVertices;
    std::vector<GLubyte> newColors;
    std::vector<GLbyte> newNormals;
    std::vector<GLfloat> newUVs;
    std::vector<GLfloat> newUVs2;   // 2da capa UV por render-vert -> TempMesh->uvExtra[1]
    std::vector<MeshIndex> newFaces;
    const bool hasUV2 = !uv2.empty();

    TVertexMap vertexMap;
    std::vector<int> posToMesh((size_t)(vertex.size()/3), -1); // posicion OBJ -> primer vertice de malla (para 'l')

    // un OBJ puede venir SIN normales o SIN UVs ("f v", "f v//vn", "f v/vt"):
    // los indices que faltan llegan en -1 y se completan con 0
    bool hasNormals = !normals.empty();
    bool hasUVs = !uv.empty();

    // grupos de material recomputados sobre la triangulacion REAL (un quad son 2
    // triangulos, un ngon N-2): antes se asumia 1 triangulo por cara y los
    // quads/ngones renderizaban a medias.
    TempMesh->materialsGroup.clear();
    int currentMaterial = -1;

    for (size_t i = 0; i < faces.size(); i++) {
        // cambio de material (start = indice de CARA donde arranca el material)
        for (size_t m = 0; m < materialsGroup.size(); m++) {
            if ((int)i == materialsGroup[m].start) {
                MaterialGroup mg;
                mg.material = materialsGroup[m].material;
                // el nombre de la PARTE que trajo el importador manda (el .glb lo guarda en los
                // extras de la primitiva); recien si no vino se rearma del material. Pisarlo
                // siempre perdia los nombres del usuario y repetia el del material.
                mg.name = !materialsGroup[m].name.empty() ? materialsGroup[m].name
                        : (materialsGroup[m].material ? materialsGroup[m].material->name : std::string("Mesh"));
                mg.start = (int)(newFaces.size() / 3);
                mg.startDrawn = (int)newFaces.size();
                mg.count = 0;
                mg.indicesDrawnCount = 0;
                TempMesh->materialsGroup.push_back(mg);
                currentMaterial = (int)TempMesh->materialsGroup.size() - 1;
            }
        }

        Face &f = faces[i];
        if (f.corners.size() < 3) continue;

        // dedup de CADA esquina de la cara (una vez) -> indices del vertex buffer
        std::vector<MeshIndex> faceIdx;
        for (size_t c = 0; c < f.corners.size(); c++) {
            FaceCorner& fc = f.corners[c];
            // color: por ESQUINA (fc.color>=0 -> cornerColors) o por VERTICE (vertexColor[fc.vertex]).
            // el key del color usa un indice NEGATIVO para el por-esquina (no colisiona con el por-vertice).
            int colorKey = (fc.color >= 0) ? -(fc.color + 1) : fc.vertex;
            VertexKey key = {fc.vertex, fc.normal, fc.uv, colorKey, hasUV2 ? fc.uv2 : -1};
            TVertexMap::iterator it = vertexMap.find(key);
            MeshIndex idx;
            if (it != vertexMap.end()) {
                idx = it->second;
            } else {
                idx = (MeshIndex)(newVertices.size() / 3);
                vertexMap[key] = idx;
                if (vertToCP) vertToCP->push_back(fc.vertex); // este vertice de render sale del control-point fc.vertex
                for (int v = 0; v < 3; v++) {
                    size_t vi = (size_t)fc.vertex * 3 + v;
                    newVertices.push_back(vi < vertex.size() ? vertex[vi] : 0.0f);
                    if (hasNormals) {
                        size_t ni = (size_t)(fc.normal < 0 ? 0 : fc.normal) * 3 + v;
                        newNormals.push_back(ni < normals.size() ? normals[ni] : (GLbyte)0);
                    }
                }
                for (int v = 0; v < 4; v++) {
                    if (fc.color >= 0) { size_t ci = (size_t)fc.color * 4 + v;
                        newColors.push_back(ci < cornerColors.size() ? cornerColors[ci] : (GLubyte)255); }
                    else { size_t ci = (size_t)fc.vertex * 4 + v;
                        newColors.push_back(ci < vertexColor.size() ? vertexColor[ci] : (GLubyte)255); }
                }
                if (hasUVs) {
                    for (int u = 0; u < 2; u++) {
                        size_t ui = (size_t)(fc.uv < 0 ? 0 : fc.uv) * 2 + u;
                        newUVs.push_back(ui < uv.size() ? uv[ui] : 0.0f);
                    }
                }
                if (hasUV2) {
                    for (int u = 0; u < 2; u++) {
                        size_t ui = (size_t)fc.uv2 * 2 + u;
                        newUVs2.push_back((fc.uv2 >= 0 && ui < uv2.size()) ? uv2[ui] : 0.0f);
                    }
                }
            }
            faceIdx.push_back(idx);
            if (fc.vertex>=0 && fc.vertex<(int)posToMesh.size() && posToMesh[fc.vertex]<0) posToMesh[fc.vertex]=idx; // 1er vert de esa pos (para 'l')
        }

        // triangulacion en abanico (tri=1, quad=2, ngon=N-2)
        int trisFace = 0;
        for (size_t t = 1; t + 1 < faceIdx.size(); t++) {
            newFaces.push_back(faceIdx[0]);
            newFaces.push_back(faceIdx[t]);
            newFaces.push_back(faceIdx[t+1]);
            trisFace++;
        }
        if (currentMaterial >= 0) {
            TempMesh->materialsGroup[currentMaterial].count += trisFace;
            TempMesh->materialsGroup[currentMaterial].indicesDrawnCount += trisFace * 3;
        }

        // CARA LOGICA: preserva el quad/ngon (overlay = 1 normal por cara; edicion)
        MeshFace mf;
        // TAG del mesh part: sin esto toda cara importada quedaba con mat=0 (default) y los materiales/meshparts
        // se COLAPSABAN a uno solo apenas se rebuildeaba faces[] desde faces3d (edit / Ctrl+J / re-export). Los rangos
        // de render (materialsGroup) ya se arman bien arriba; faltaba propagar el material a la cara logica.
        mf.mat = (currentMaterial >= 0) ? currentMaterial : 0;
        for (size_t k = 0; k < faceIdx.size(); k++) mf.idx.push_back((int)faceIdx[k]);
        TempMesh->faces3d.push_back(mf);
    }

    // BORDES SUELTOS (lineas 'l'): cada posicion -> vertice de malla. Si la posicion NO la usa ninguna cara,
    // se crea un vertice nuevo (perfil suelto: Screw / planos). Preserva las aristas para el editor.
    if (!looseEdges.empty()) {
        const int nPos = (int)(vertex.size()/3);
        for (size_t i = 0; i + 1 < looseEdges.size(); i += 2) {
            int ab[2] = { looseEdges[i], looseEdges[i+1] }, mm[2] = { -1, -1 };
            for (int s = 0; s < 2; s++) {
                int pos = ab[s];
                if (pos < 0 || pos >= nPos) continue;
                if (posToMesh[pos] >= 0) { mm[s] = posToMesh[pos]; continue; }
                int newValue = (int)(newVertices.size()/3); // posicion no usada por caras -> vert nuevo
                for (int v = 0; v < 3; v++) newVertices.push_back(vertex[(size_t)pos*3+v]);
                if (hasNormals) { newNormals.push_back(0); newNormals.push_back(127); newNormals.push_back(0); }
                for (int v = 0; v < 4; v++) { size_t ci=(size_t)pos*4+v; newColors.push_back(ci<vertexColor.size()?vertexColor[ci]:(GLubyte)255); }
                if (hasUVs) { newUVs.push_back(0.0f); newUVs.push_back(0.0f); }
                if (hasUV2) { newUVs2.push_back(0.0f); newUVs2.push_back(0.0f); }
                posToMesh[pos] = newValue; mm[s] = newValue;
            }
            if (mm[0]>=0 && mm[1]>=0 && mm[0]!=mm[1]) { TempMesh->looseEdges.push_back(mm[0]); TempMesh->looseEdges.push_back(mm[1]); }
        }
    }

    // Asignar a TempMesh (vertexSize = cantidad de VERTICES, no de floats: asi lo
    // usan las primitivas, ObjectMode (duplicar) y el overlay de normales)
    TempMesh->vertexSize = (int)(newVertices.size() / 3);
    TempMesh->vertex = new GLfloat[newVertices.size()];
    std::copy(newVertices.begin(), newVertices.end(), TempMesh->vertex);

    if (newNormals.empty()) {
        TempMesh->normals = NULL; // OBJ sin "vn": que el render lo sepa
    } else {
        TempMesh->normals = new GLbyte[newNormals.size()];
        std::copy(newNormals.begin(), newNormals.end(), TempMesh->normals);
    }

    TempMesh->vertexColor = new GLubyte[newColors.size()];
    std::copy(newColors.begin(), newColors.end(), TempMesh->vertexColor);

    if (newUVs.empty()) {
        TempMesh->uv = NULL; // OBJ sin "vt": el render usa el dummy
    } else {
        TempMesh->uv = new GLfloat[newUVs.size()];
        std::copy(newUVs.begin(), newUVs.end(), TempMesh->uv);
    }
    // 2da capa UV (TEXCOORD_1): al render como uvExtra[1]; PopulateLayers la vuelve una capa editable "UV2"
    TempMesh->uvExtra.clear();
    if (hasUV2 && !newUVs2.empty()) { TempMesh->uvExtra.resize(2); TempMesh->uvExtra[1] = newUVs2; }

    TempMesh->facesSize = (int)newFaces.size();
    TempMesh->faces = new MeshIndex[newFaces.size() ? newFaces.size() : 1];
    std::copy(newFaces.begin(), newFaces.end(), TempMesh->faces);

    // sin materiales: un grupo unico que dibuja todo
    if (TempMesh->materialsGroup.empty()) {
        MaterialGroup mg;
        mg.start = 0;
        mg.count = (int)(newFaces.size() / 3);
        mg.startDrawn = 0;
        mg.indicesDrawnCount = TempMesh->facesSize;
        mg.material = 0;
        TempMesh->materialsGroup.push_back(mg);
    }

    Reset();
}

// Detecta si un mesh IMPORTADO (con normales) tiene shading SMOOTH o FLAT, para setear meshSmooth. Sin esto el flag
// quedaba flat y, al mover un vertice, GenerateRender recalculaba TODA la malla en flat aunque hubiera venido smooth.
// Heuristica: agrupa los render-verts por POSICION (posRep, ya listo tras ComputeBorders) y ve si las normales dentro
// de cada grupo COINCIDEN. Un smooth comparte la normal (los splits que quedan son por costura de UV); un flat tiene
// una normal por CARA (normales distintas en la misma posicion). Empate o mayoria que coincide -> smooth.
bool MeshShadingImportedIsSmooth(Mesh* m) {
    if (!m || !m->normals || m->vertexSize <= 0 || (int)m->posRep.size() != m->vertexSize) return true;
    int coincide = 0, differs = 0;
    std::vector<int> ref((size_t)m->vertexSize, -1); // representante de posicion -> primer render-vert (para comparar)
    for (int i = 0; i < m->vertexSize; i++) {
        int r = m->posRep[i];
        if (r < 0 || r >= m->vertexSize) continue;
        if (ref[r] < 0) { ref[r] = i; continue; } // 1er vert del grupo de posicion
        int j = ref[r];
        float ax=m->normals[i*3], ay=m->normals[i*3+1], az=m->normals[i*3+2];
        float bx=m->normals[j*3], by=m->normals[j*3+1], bz=m->normals[j*3+2];
        float dot = ax*bx + ay*by + az*bz;
        float la = ax*ax+ay*ay+az*az, lb = bx*bx+by*by+bz*bz;
        // ~<32 grados = "misma" normal (dot>0 y dot^2 > 0.72*la*lb; sin sqrt, GLbyte escala ~127)
        if (la > 1.0f && lb > 1.0f && dot > 0.0f && dot*dot > 0.72f * la * lb) coincide++; else differs++;
    }
    return differs <= coincide;
}

void Wavefront::ConvertToES1_NoMerge(Mesh* TempMesh) {

    // =========================================================
    // 1) COPIAR VERTICES Y COLORES TAL CUAL
    // =========================================================

    TempMesh->vertexSize = (int)(vertex.size() / 3); // cantidad de VERTICES (no floats)
    TempMesh->vertex = new GLfloat[vertex.size()];
    std::copy(vertex.begin(), vertex.end(), TempMesh->vertex);

    TempMesh->vertexColor = new GLubyte[vertexColor.size()];
    std::copy(vertexColor.begin(), vertexColor.end(), TempMesh->vertexColor);

    size_t vertCount = vertex.size() / 3;

    // =========================================================
    // 2) NORMALES Y UV INICIALMENTE EN CERO
    // =========================================================

    TempMesh->normals = new GLbyte[vertCount * 3];
    memset(TempMesh->normals, 0, vertCount * 3);

    TempMesh->uv = new GLfloat[vertCount * 2];
    memset(TempMesh->uv, 0, sizeof(GLfloat) * vertCount * 2);

    // =========================================================
    // 3) CARAS + MATERIALES + PISAR NORMAL / UV
    // =========================================================

    std::vector<MeshIndex> newFaces;
    TempMesh->materialsGroup.clear();

    int currentMaterial = -1;

    for (size_t i = 0; i < faces.size(); i++) {

        // --- cambio de material ---
        for (size_t m = 0; m < materialsGroup.size(); m++) {
            if ((int)i == materialsGroup[m].start) {

                MaterialGroup mg;
                mg.material = materialsGroup[m].material;
                mg.name = !materialsGroup[m].name.empty() ? materialsGroup[m].name
                        : (mg.material ? mg.material->name : std::string("Mesh")); // (como la variante con merge)
                mg.start = newFaces.size() / 3;
                mg.count = 0;
                mg.startDrawn = mg.start * 3;
                mg.indicesDrawnCount = 0;

                TempMesh->materialsGroup.push_back(mg);
                currentMaterial = (int)TempMesh->materialsGroup.size() - 1;
            }
        }

        Face& f = faces[i];
        if (f.corners.size() < 3) continue;

        for (size_t t = 1; t < f.corners.size() - 1; t++) {

            FaceCorner corners[3] = {
                f.corners[0],
                f.corners[t],
                f.corners[t + 1]
            };

            for (int c = 0; c < 3; c++) {

                const FaceCorner& fc = corners[c];
                int v = fc.vertex;
                // indices del archivo SIN validar escribian fuera de rango (la
                // variante con merge clampa TODOS los indices): mismo criterio aca
                if (v < 0 || v >= (int)vertCount) v = 0;

                // -------- PISAR NORMAL --------
                if (fc.normal >= 0 && (size_t)fc.normal * 3 + 2 < normals.size()) {
                    TempMesh->normals[v * 3 + 0] = normals[fc.normal * 3 + 0];
                    TempMesh->normals[v * 3 + 1] = normals[fc.normal * 3 + 1];
                    TempMesh->normals[v * 3 + 2] = normals[fc.normal * 3 + 2];
                }

                // -------- PISAR UV --------
                if (fc.uv >= 0 && (size_t)fc.uv * 2 + 1 < uv.size()) {
                    TempMesh->uv[v * 2 + 0] = uv[fc.uv * 2 + 0];
                    TempMesh->uv[v * 2 + 1] = uv[fc.uv * 2 + 1];
                }

                newFaces.push_back((MeshIndex)v);
            }

            if (currentMaterial >= 0) {
                TempMesh->materialsGroup[currentMaterial].count++;
                TempMesh->materialsGroup[currentMaterial].indicesDrawnCount += 3;
            }
        }

        // CARA LOGICA (faces3d): igual que la variante con merge. Sin esto la malla
        // noMerge quedaba SIN caras logicas: el Edit Mode no la veia y el export
        // OBJ escribia v/vn/vt con CERO lineas 'f'.
        {
            MeshFace mf;
            mf.mat = (currentMaterial >= 0) ? currentMaterial : 0;
            for (size_t k = 0; k < f.corners.size(); k++) {
                int vk = f.corners[k].vertex;
                if (vk < 0 || vk >= (int)vertCount) vk = 0;
                mf.idx.push_back(vk);
            }
            TempMesh->faces3d.push_back(mf);
        }
    }

    // =========================================================
    // 4) COPIAR ÍNDICES
    // =========================================================

    TempMesh->facesSize = newFaces.size();
    TempMesh->faces = new MeshIndex[newFaces.size()];
    std::copy(newFaces.begin(), newFaces.end(), TempMesh->faces);

    // BORDES SUELTOS (lineas 'l'): sin merge los verts van 1:1, asi que las posiciones = indices de malla.
    int nvNM = (int)(vertex.size()/3);
    for (size_t i = 0; i + 1 < looseEdges.size(); i += 2) {
        int a = looseEdges[i], b = looseEdges[i+1];
        if (a>=0 && a<nvNM && b>=0 && b<nvNM && a!=b) { TempMesh->looseEdges.push_back(a); TempMesh->looseEdges.push_back(b); }
    }

    // =========================================================
    // 5) DEBUG
    // =========================================================

#ifndef W3D_SYMBIAN
    std::cout << "Este objeto tenia "
              << TempMesh->materialsGroup.size()
              << " materiales\n\n";
#endif

    Reset();
}

// extraer nombre base del filename (sin path ni extensión)
std::string ExtractBaseName(const std::string& filepath) {
    // quitar ruta
    size_t pos = filepath.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? filepath : filepath.substr(pos + 1);
    // quitar extensión (la última '.')
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

// prefijo de linea (sobre const char*: las lineas del OBJ son punteros a un buffer, sin std::string por linea)
static bool LineBegins(const char* s, const char* p) {
    while (*p) { if (*s != *p) return false; s++; p++; }
    return true;
}

bool ReadOBJ(const std::vector<const char*>& lines,
             size_t& idx,
             const std::string& filename,
             int* accumulatedVertices,
             int* accumulatedNormals,
             int* accumulatedUVs,
             int* accumulatedColors,
             bool NoMerge)
{
    Mesh* mesh = new Mesh(CollectionActive, Vector3(0, 0, 0));
    mesh->SetNameObj(ExtractBaseName(filename)); // nombre UNICO: importar dos veces el mismo .obj no repite

    Wavefront Wobj;
    Wobj.Reset();

    bool NameFound = false;
    bool hasMoreObjects = false;
    bool HasVertexColor = false;

    int accumulatedNextVertices = 0;
    int accumulatedNextNormals  = 0;
    int accumulatedUVsNext = 0;
    int accumulatedNextColors = 0;

    while (idx < lines.size()) {
        if ((idx & 4095) == 0 && !lines.empty()) ProgressUpdate((float)idx / (float)lines.size()); // barra (cada 4096 lineas; ya throttlea a 1.5%)
        const char* line = lines[idx];

        if (LineBegins(line, "o ")) {
            if (!NameFound) {
                NameFound = true;
                std::string name(line + 2);
                while (!name.empty() && (name[name.size()-1]==' ' || name[name.size()-1]=='	'))
                    name.erase(name.size()-1);
                if (!name.empty()) mesh->SetNameObj(name); // nombre UNICO (el 'o' del .obj puede chocar)
                idx++;
            } else {
                // proximo objeto: NO consumir su 'o' (lo lee el siguiente ReadOBJ)
                hasMoreObjects = true;
                break;
            }
        }
        // PARSEO MANUAL (strtod/strtol sobre el c_str): NADA de istringstream ni substr -> ~5-10x mas rapido en
        // modelos grandes (200k+ verts). El istringstream construia un stream + locale POR LINEA (~660k veces).
        else if (LineBegins(line, "v ")) {
            const char* p = line + 2; char* e;
            double val[7]; int n = 0;
            while (n < 7) { double d = strtod(p, &e); if (e == p) break; val[n++] = d; p = e; }
            if (n >= 3) {
                Wobj.vertex.push_back((GLfloat)val[0]);
                Wobj.vertex.push_back((GLfloat)val[1]);
                Wobj.vertex.push_back((GLfloat)val[2]);
                if (n >= 6) { HasVertexColor = true; // v x y z r g b [a] -> vertex color
                    Wobj.vertexColor.push_back(ObjSaturate(val[3]));
                    Wobj.vertexColor.push_back(ObjSaturate(val[4]));
                    Wobj.vertexColor.push_back(ObjSaturate(val[5]));
                    Wobj.vertexColor.push_back(ObjSaturate(n >= 7 ? val[6] : 1.0));
                } else { // sin color -> blanco
                    Wobj.vertexColor.push_back(255); Wobj.vertexColor.push_back(255);
                    Wobj.vertexColor.push_back(255); Wobj.vertexColor.push_back(255);
                }
                accumulatedNextVertices++;
            }
            idx++;
        }
        else if (LineBegins(line, "vn ")) {
            const char* p = line + 3; char* e;
            double nx = strtod(p, &e); p = e;
            double ny = strtod(p, &e); p = e;
            double nz = strtod(p, &e);
            Wobj.normals.push_back(ObjConvNormal(nx));
            Wobj.normals.push_back(ObjConvNormal(ny));
            Wobj.normals.push_back(ObjConvNormal(nz));
            accumulatedNextNormals++;
            idx++;
        }
        else if (LineBegins(line, "vt ")) {
            const char* p = line + 3; char* e;
            double u = strtod(p, &e); p = e;
            double v = strtod(p, &e);
            Wobj.uv.push_back((float)u);
            Wobj.uv.push_back(1.0f - (float)v);
            accumulatedUVsNext++;
            idx++;
        }
        else if (LineBegins(line, "vc ")) { // COLOR POR ESQUINA (extension Whisk3D)
            const char* p = line + 3; char* e;
            double c[4]; int n = 0;
            while (n < 4) { double d = strtod(p, &e); if (e == p) break; c[n++] = d; p = e; }
            Wobj.cornerColors.push_back(ObjSaturate(n > 0 ? c[0] : 1.0));
            Wobj.cornerColors.push_back(ObjSaturate(n > 1 ? c[1] : 1.0));
            Wobj.cornerColors.push_back(ObjSaturate(n > 2 ? c[2] : 1.0));
            Wobj.cornerColors.push_back(ObjSaturate(n > 3 ? c[3] : 1.0));
            HasVertexColor = true;
            accumulatedNextColors++;
            idx++;
        }
        else if (LineBegins(line, "f ")) {
            const char* p = line + 2;
            Face newFace;
            while (*p) {
                while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') p++; // separa los corners por espacio
                if (!*p) break;
                FaceCorner fc;
                int parts[4]; bool has[4];
                for (int i = 0; i < 4; i++) { parts[i] = 0; has[i] = false; }
                // corner = v[/vt[/vn[/vc]]] (campos vacios en "v//vn")
                const char* start = p;
                for (int pi = 0; pi < 4; pi++) {
                    if ((*p>='0'&&*p<='9') || *p=='-') {
                        char* e; const long n = strtol(p, &e, 10);
                        if (e != p) { parts[pi] = (int)n; has[pi] = true; p = e; }
                    }
                    if (*p == '/') p++; else break; // proximo campo, o fin del corner
                }
                // (algo que no es un corner -"f 1 2 x", un "-" suelto-: antes 'p' no avanzaba y se juntaban corners
                //  para siempre hasta quedarse sin memoria. Se saltea hasta el proximo espacio)
                if (p == start || !has[0]) {
                    while (*p && *p!=' ' && *p!='\t' && *p!='\r' && *p!='\n') p++;
                    continue;
                }
                // indices GLOBALES del .obj -> LOCALES (resto lo acumulado antes)
                fc.vertex = parts[0] - 1 - *accumulatedVertices;
                fc.uv     = has[1] ? parts[1] - 1 - *accumulatedUVs     : -1;
                fc.normal = has[2] ? parts[2] - 1 - *accumulatedNormals : -1;
                fc.color  = has[3] ? parts[3] - 1 - *accumulatedColors : -1; // color por esquina
                newFace.corners.push_back(fc);
            }
            Wobj.faces.push_back(newFace);
            if (!Wobj.materialsGroup.empty()) {
                MaterialGroup& mg = Wobj.materialsGroup.back();
                mg.count++;
                mg.indicesDrawnCount += 3;
                if (mg.count == 1) mg.startDrawn = (Wobj.faces.size() - 1) * 3;
            }
            idx++;
        }
        else if (LineBegins(line, "l ")) {
            // BORDES SUELTOS: 'l v1 v2 v3 ...' es una POLILINEA -> aristas (v1,v2),(v2,v3),... Indices de POSICION
            // globales (1-based) -> locales. Solo posiciones (l no lleva vt/vn). Sirven de perfil (Screw) / planos.
            const char* p = line + 2;
            int prev = -1;
            while (*p) {
                while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') p++;
                if (!*p) break;
                if (!((*p>='0'&&*p<='9') || *p=='-')) { p++; continue; }
                char* e; long gv = strtol(p, &e, 10);
                if (e == p) { p++; continue; }   // (un "-" suelto: sin esto no avanzaba nunca)
                p = e;
                while (*p=='/' || (*p>='0'&&*p<='9')) p++; // por si viniera 'l v/vt' (raro): ignoramos vt
                int loc = (int)gv - 1 - *accumulatedVertices;
                if (prev >= 0 && prev != loc) { Wobj.looseEdges.push_back(prev); Wobj.looseEdges.push_back(loc); }
                prev = loc;
            }
            idx++;
        }
        else if (LineBegins(line, "usemtl ")) {
            std::string matName(line + 7);
            W3dTrimName(matName);   // CRLF de Windows: sin esto el material no matchea el del newmtl del .mtl
            Material* materialPointer = FindMaterialByName(matName);
            if (!materialPointer) materialPointer = new Material(matName, false, HasVertexColor);
            MaterialGroup mg;
            mg.name = materialPointer->name;
            mg.start = Wobj.faces.size();
            mg.startDrawn = Wobj.faces.size() * 3;
            mg.count = 0;
            mg.indicesDrawnCount = 0;
            mg.material = materialPointer;
            Wobj.materialsGroup.push_back(mg);
            idx++;
        }
        else {
            idx++;
        }
    }

#ifdef W3D_SYMBIAN
    // N95 = GLES 1.1: el index buffer es de 16 bits (max 65535 vertices por malla; glDrawElements solo soporta
    // GL_UNSIGNED_SHORT). Si el OBJ pasa ese limite -> CARTEL + NO importar (sino los indices se truncan = geometria
    // rota, y ademas armar 200k+ verts revienta la RAM del N95). En PC/Android/WebGL/N8 esto NO corre: MeshIndex es de
    // 32 bits y entra de una (ver crossplatform.h).
    if ((int)(Wobj.vertex.size() / 3) > W3D_MAX_INDEX16) {
        Notify(T("Model too big for the N95: more than 65535 vertices (16-bit limit)"), true);
        delete mesh;   // borra la malla recien creada (vacia; el dtor de Object la detacha del padre)
        return false;  // no importar este objeto
    }
#endif

    if (NoMerge){
        Wobj.ConvertToES1_NoMerge(mesh);
    } else {
        Wobj.ConvertToES1(mesh, accumulatedVertices, accumulatedNormals, accumulatedUVs);
    }

    mesh->ComputeBorders(); // bordes unicos para contorno de seleccion / wireframe

    // El OBJ no traia normales (vn) -> CALCULARLAS smooth (como Blender), sino la malla sale NEGRA/sin luz.
    // posRep ya esta listo de ComputeBorders; RecalculateNormals promedia por posicion (smooth).
    if (!mesh->normals && mesh->vertexSize > 0) {
        mesh->normals = new GLbyte[mesh->vertexSize * 3];
        mesh->meshSmooth = true;
        mesh->RecalculateNormals();
    } else if (mesh->normals && mesh->vertexSize > 0) {
        // CON normales (vn): detectar smooth/flat para que al mover un vertice NO se recalcule todo flat
        mesh->meshSmooth = MeshShadingImportedIsSmooth(mesh);
    }

    mesh->OptimizeCacheRender(); // reordena el index buffer para el cache de vertices del GPU (perf en tile-based / N95)

    *accumulatedVertices += accumulatedNextVertices;
    *accumulatedNormals += accumulatedNextNormals;
    *accumulatedUVs += accumulatedUVsNext;
    *accumulatedColors += accumulatedNextColors;
    return hasMoreObjects;
}

// saca \r, \n, espacios y tabs del FINAL de un nombre. CLAVE: un .obj/.mtl escrito en Windows (Python en modo
// texto, o cualquier editor con CRLF) deja el nombre de material con un '\r' INVISIBLE ("crash_escenario_alpha\r"),
// que no matchea "crash_escenario_alpha" en FindMaterialByName -> se creaba un MATERIAL NUEVO por cada .obj
// (el "millon de materiales" + malla sin textura porque el usemtl y el newmtl caian en materiales distintos).
void W3dTrimName(std::string& s) {
    while (!s.empty()) {
        char c = s[s.size() - 1];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') s.erase(s.size() - 1);
        else break;
    }
}

bool ReadMTL(const std::string& filepath, int loadedObjects) {
    // POR LA ABSTRACCION DEL CORE (ReadTextFile), no con ifstream: un .mtl puede
    // venir del pak embebido o de ADENTRO DEL APK, donde no es un archivo real y
    // el ifstream falla siempre. En Android eso dejaba sin materiales a todos los
    // .obj del proyecto, en silencio.
    bool okMtl = false;
    const std::string dataMtl = w3dFileSystem::ReadTextFile(filepath, &okMtl);
    if (!okMtl) {
#ifndef W3D_SYMBIAN
        std::cerr << "Error al abrir: " << filepath << std::endl;
#endif
        return false;
    }
    std::istringstream file(dataMtl);

    std::string line;
    Material* mat = NULL;
    bool HasTexturesToLoad = false;

    while (std::getline(file, line)) {
        if (line.rfind("newmtl ", 0) == 0) {
            std::string matName = line.substr(7);
            W3dTrimName(matName);   // CRLF de Windows: sin esto "X\r" del newmtl no matcheaba el "X" del usemtl
            mat = FindMaterialByName(matName);

            if (!mat) {
                // (el ctor de Material YA se registra en Materials: un push_back
                // aca lo metia DOS veces y el dropdown lo listaba duplicado)
                mat = new Material(matName);
            }
        }
        else if (mat) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "Ns") {
                float ns; iss >> ns;
                float value = ns / 1000.0f;
                mat->specular[0] = mat->specular[1] = mat->specular[2] = mat->specular[3] = value;
            }
            else if (prefix == "Kd") {
                iss >> mat->diffuse[0] >> mat->diffuse[1] >> mat->diffuse[2];
            }
            else if (prefix == "Ke") {
                iss >> mat->emission[0] >> mat->emission[1] >> mat->emission[2];
            }
            else if (prefix == "d") {
                float d; iss >> d;
                mat->diffuse[3] = d;
                mat->transparent = (d < 1.0f);
            }
            else if (prefix == "map_Kd") {
                std::string texfile;
                iss >> texfile;
                //std::string absPath = getParentPath(filepath) + "/" + texfile;
                // JoinPath NORMALIZA la ruta: '\'->'/' y COLAPSA el '..' (ej: models/frutas/../../textures/
                // frutas.png -> textures/frutas.png). En PC el fopen resuelve los '..' al abrir; en SYMBIAN RFs
                // los rechaza (KErrBadName) -> sin esto la textura de la fruta/cajas no cargaba en el N95.
                std::string absPath = w3dFileSystem::JoinPath(DirOf(filepath), texfile);

                // CARGA DIFERIDA: no decodificar aca (bloquea el import ~17s con los PNG grandes). Encolar -> el loop
                // principal la carga en los frames siguientes (el modelo aparece gris y las texturas entran solas).
                EnqueueTexture(mat, absPath);
            }
            //sprite animado. esto no es un estandar en MTL. es solo para whisk3D
#ifndef W3D_SYMBIAN // sprites animados del MTL: pendiente en Symbian
            else if (prefix == "map_Kd_ANIM") {
                AnimatedMaterial* NewAM = new AnimatedMaterial;
                NewAM->targets.push_back(mat);
                std::string extension;
                int totalFrames = 0;
                int zeros = 0;
                int speed = 1;

                // Leer primeros parámetros
                iss >> extension >> totalFrames >> zeros >> speed;

                // Leer el resto de la línea crudo
                std::string rest;
                std::getline(iss, rest);

                // Quitar espacios al inicio
                rest.erase(0, rest.find_first_not_of(" \t"));

                // Buscar comillas simples o dobles
                char quote = 0;
                if (!rest.empty() && (rest[0] == '"' || rest[0] == '\'')) {
                    quote = rest[0];
                } else {
#ifndef W3D_SYMBIAN
                    std::cerr << "ERROR: map_Kd_ANIM debe usar comillas para la ruta base.\n";
#endif
                    delete NewAM;   // recien se adopta en AnimatedMaterials (abajo): sin esto se fugaba
                    return false;
                }

                // Buscar la última comilla del MISMO tipo
                size_t end = rest.find_last_of(quote);
                if (end == std::string::npos || end == 0) {
#ifndef W3D_SYMBIAN
                    std::cerr << "ERROR: comillas mal formadas en map_Kd_ANIM\n";
#endif
                    delete NewAM;
                    return false;
                }

                // Extraer ruta sin comillas
                std::string baseURL = rest.substr(1, end - 1);

                // Normalizar slashes
                std::replace(baseURL.begin(), baseURL.end(), '\\', '/');

                // Generar frames
                for (int i = 1; i <= totalFrames; i++) {
                    NewAM->frameDurations.push_back(speed);

                    std::ostringstream num;
                    num << std::setw(zeros) << std::setfill('0') << i;

                    std::string filename = baseURL + num.str() + "." + extension;

                    // MISMA normalizacion que map_Kd (linea ~819): JoinPath colapsa '..' y separadores, asi la
                    // clave del cache de texturas coincide con la de un map_Kd al mismo PNG -> 1 solo upload.
                    std::string absPath = w3dFileSystem::JoinPath(DirOf(filepath), filename);

                    // por el CACHE por ruta: dos tiras animadas que compartan
                    // frames (o el mismo .mtl leido dos veces) suben la imagen
                    // una sola vez.
                    Texture* newTex = TextureTake(absPath);
                    if (newTex) {
                        NewAM->frameTextures.push_back(newTex);
                    }
                    else {
#ifndef W3D_SYMBIAN
                        std::cerr << "Error cargando textura ANIM: " << absPath << "\n";
#endif
                    }
                }

                AnimatedMaterials.push_back(NewAM);

#ifndef W3D_SYMBIAN
                std::cout << "Animación cargada con "
                        << NewAM->frameTextures.size()
                        << " frames para material " << mat->name << "\n";
#endif
            }
#endif // !W3D_SYMBIAN
            else if (prefix == "BackfaceCullingOff") {
                mat->culling = false;
            }
            else if (prefix == "GL_DEPTH_TEST_OFF") {
                mat->depth_test = false;
            }
            else if (prefix == "BACKGROUND") {
                mat->background = true;   // cielo: ultimo de los opacos, clavado al plano lejano
            }
            else if (prefix == "NoLight") {
                mat->lighting = false;
            }
            else if (prefix == "CLAMP_TO_EDGE") {
                mat->repeat = false;
            }
            //por defecto es lineal
#ifndef W3D_SYMBIAN
            else if (prefix == "PIXELATED") {
                mat->interpolation = closest;
            }
#endif
            else if (prefix == "map_d" || prefix == "alpha") {
                mat->transparent = true;
            }
            // ADITIVO: mezcla GL_ONE/GL_ONE (gfx::MixAdd). Aclara y el NEGRO queda invisible.
            // Es lo que hace el PSX con el torbellino del giro, los rayos, el halo y las chispas:
            // emularlo con alpha comun oscurece el fondo donde la textura es negra. Implica
            // transparent (sin blend habilitado la mezcla no existe).
            else if (prefix == "ADDITIVE" || prefix == "additive") {
                mat->transparent = true;
                mat->mix = w3dEngine::MixAdd;
            }
            // DECAL: calcomania pegada a una superficie que ya escribio z (sombras, blobs,
            // manchas). NO escribe z + glPolygonOffset hacia el ojo + se dibuja despues de los
            // opacos de la misma malla. Argumento opcional = unidades del offset (default -4;
            // en el N95, con z de 16 bits, el render lo multiplica x4 solo).
            else if (prefix == "decal" || prefix == "DECAL") {
                float units = 0.0f;
                if (!(iss >> units)) units = 0.0f;   // 0 = el default de la receta (-4)
                W3dMaterialApplyDecal(mat, true, units);   // LA receta, en Materials.cpp
            }
            // LINEAS: dibujar las ARISTAS de la malla con este material (ademas del
            // relleno; en una tira degenerada, el CAMINO de la polilinea). Argumento
            // opcional = grosor en px de glLineWidth (default 1).
            else if (prefix == "lines" || prefix == "LINES") {
                float thickness = 0.0f;
                mat->lines = true;
                if ((iss >> thickness) && thickness >= 1.0f) mat->thicknessLine = thickness;
            }
        }
    }

    if (HasTexturesToLoad) {
#ifndef W3D_SYMBIAN
        std::cout << "Se encontraron texturas para cargar." << std::endl;
#endif
    }

    return true;
}

// ============================================================================
//  EXPORTAR OBJ + MTL (con normales, vertex color, UV, texturas y los extras de
//  material de Whisk3D: BackfaceCullingOff / NoLight / etc.). Mismo formato que
//  lee el importador. Coordenadas en MUNDO (preserva el layout de la escena).
// ============================================================================
static void CollectMeshesExport(Object* o, bool selectedOnly, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* c = o->Childrens[i];
        if (c->getType() == ObjectType::mesh && (!selectedOnly || c->select))
            out.push_back((Mesh*)c);
        CollectMeshesExport(c, selectedOnly, out);
    }
}

// basename de una textura (sin carpeta): fallback cuando no se puede armar una ruta relativa
static std::string BaseNameTex(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return (s == std::string::npos) ? p : p.substr(s + 1);
}
static bool PathIsAbsolute(const std::string& p) {
    if (p.empty()) return false;
    if (p[0] == '/' || p[0] == '\\') return true;
    if (p.size() >= 2 && p[1] == ':') return true; // C:\ ...
    return false;
}
static void SplitPath(const std::string& p, std::vector<std::string>& out) {
    std::string cur;
    for (size_t i = 0; i < p.size(); i++) {
        char c = p[i];
        if (c == '/' || c == '\\') { if (!cur.empty()) { out.push_back(cur); cur.clear(); } }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
}
// ruta de la TEXTURA (archivo) relativa al DIRECTORIO del export, para que el .mtl sea portable (OBJ+MTL+textura se
// mueven juntos). Ej: export a /a/b/, textura en /a/b/tex/x.png -> "tex/x.png"; en /a/c/x.png -> "../c/x.png".
// Si no son comparables (raiz distinta, o la textura ya es relativa/asset) cae al basename o a la ruta tal cual.
static std::string RelativeTexturePath(const std::string& dir, const std::string& target) {
    if (!PathIsAbsolute(target)) return target;   // ya es relativa (o asset del APK): dejar como esta
    if (!PathIsAbsolute(dir))    return BaseNameTex(target);
    std::vector<std::string> a, b; SplitPath(dir, a); SplitPath(target, b);
    size_t common = 0;
    while (common < a.size() && common + 1 < b.size() && a[common] == b[common]) common++;
    if (common == 0) return BaseNameTex(target); // sin prefijo comun (raiz/unidad distinta)
    std::string rel;
    for (size_t i = common; i < a.size(); i++) rel += "../";
    for (size_t i = common; i < b.size(); i++) { rel += b[i]; if (i + 1 < b.size()) rel += "/"; }
    return rel.empty() ? BaseNameTex(target) : rel;
}

static void WriteMaterialMTL(std::ofstream& mtl, Material* mat, const std::string& exportDir) {
    if (!mat) return;
    mtl << "newmtl " << mat->name << "\n";
    mtl << "Kd " << FloatStr(mat->diffuse[0]) << " " << FloatStr(mat->diffuse[1]) << " " << FloatStr(mat->diffuse[2]) << "\n";
    mtl << "Ke " << FloatStr(mat->emission[0]) << " " << FloatStr(mat->emission[1]) << " " << FloatStr(mat->emission[2]) << "\n";
    mtl << "Ns " << FloatStr(mat->specular[0] * 1000.0f) << "\n";
    mtl << "d " << FloatStr(mat->diffuse[3]) << "\n";
    if (mat->texture && !mat->texture->path.empty())
#ifdef __EMSCRIPTEN__
        mtl << "map_Kd " << BaseNameTex(mat->texture->path) << "\n";   // navegador: la textura bajo AL LADO (ver ExportOBJ)
#else
        mtl << "map_Kd " << RelativeTexturePath(exportDir, mat->texture->path) << "\n";
#endif
    // extras propios de Whisk3D (los lee ReadMTL)
    if (!mat->culling)    mtl << "BackfaceCullingOff\n";
    if (!mat->depth_test) mtl << "GL_DEPTH_TEST_OFF\n";
    if (mat->background) mtl << "FONDO\n";
    if (!mat->lighting)   mtl << "NoLight\n";
    if (!mat->repeat)     mtl << "CLAMP_TO_EDGE\n";
    // DECAL / ADITIVO (round-trip del import de arriba). El 'decal' ya implica NoLight,
    // pero escribir los dos no molesta: ReadMTL los aplica en cualquier orden.
    if (!mat->depth_write && mat->order_pass == 1) mtl << "decal " << FloatStr(mat->depth_bias) << "\n";
    if (mat->mix == w3dEngine::MixAdd)         mtl << "ADITIVO\n";
    else if (mat->transparent)                       mtl << "alpha\n";
    if (mat->lines)                                 mtl << "lines " << FloatStr(mat->thicknessLine) << "\n";
#ifndef W3D_SYMBIAN
    if (mat->interpolation == closest) mtl << "PIXELATED\n";
#endif
    mtl << "\n";
}

// LA EDICION PRESTADA para exportar: las mallas de un RECURSO compartido tienen sus caras
// PENDIENTES (objects/MeshResource.h) y el OBJ se escribe por caras (faces3d). Se leen antes de
// escribir y, al terminar, las que se leyeron solo para esto vuelven a quedar pendientes (el
// export no deja la memoria de mil arboles cargada). false = alguna no se pudo leer: exportarla
// sin caras seria perder su geometria sin avisar.
static bool EditToExport(const std::vector<Mesh*>& meshes, std::vector<Mesh*>& borrowed) {
    borrowed.clear();
    for (size_t i = 0; i < meshes.size(); i++) {
        Mesh* m = meshes[i];
        if (m->pendingEdit.empty()) continue;
        const bool ofResource = m->mesh != NULL;
        if (!W3dMeshBinMaterializeEdit(m)) {
            // (el que llama avisa que el OBJ no se guardo; el motivo queda en el log)
            w3dLogfE("ExportOBJ: no pude leer las caras de '%s': no exporto", m->name.c_str());
            return false;
        }
        if (ofResource) borrowed.push_back(m);
    }
    return true;
}
static void ReleaseBorrowed(std::vector<Mesh*>& borrowed) {
    for (size_t i = 0; i < borrowed.size(); i++) W3dMeshReleaseEdit(borrowed[i]);
    borrowed.clear();
}
namespace {
struct ReleaseBorrowedOnExit {   // (cualquier return de ExportOBJ las devuelve a pendientes)
    std::vector<Mesh*>& v;
    ReleaseBorrowedOnExit(std::vector<Mesh*>& p) : v(p) {}
    ~ReleaseBorrowedOnExit() { ReleaseBorrowed(v); }
};
}

bool ExportOBJ(const std::string& filepath, bool selectedOnly, bool applyModifiers, bool applyTransforms) {
    std::vector<Mesh*> meshes;
    CollectMeshesExport(SceneCollection, selectedOnly, meshes);
    if (meshes.empty()) { w3dLogfE("ExportOBJ: NO hay meshes para exportar"); return false; }
    // las caras de las mallas de un recurso (pendientes) se leen ANTES de escribir nada
    std::vector<Mesh*> borrowed;
    ReleaseBorrowedOnExit release(borrowed);
    if (!EditToExport(meshes, borrowed)) return false;

    errno = 0;
    std::ofstream obj(filepath.c_str());
    if (!obj.is_open()) { w3dLogfE("ExportOBJ: no se pudo abrir '%s' errno=%d (%s)", filepath.c_str(), errno, strerror(errno)); return false; }

    ProgressStart(T("Exporting model...")); // barra de progreso (no-op sin GUI; clave en el N95)

    std::string base    = ExtractBaseName(filepath);
    std::string mtlName = base + ".mtl";
    // SE ARMA TODO EN UN BUFFER `out` y se escribe de UNA al final (con obj.write). Antes eran millones de `obj <<`
    // (cada uno una llamada al stream + formato): lentisimo en Debug. FloatStr/IntStr (sprintf+SSO) -> append.
    std::string out;
#ifdef W3D_SYMBIAN
    out.reserve(256 * 1024);      // N95: export de modelos chicos (el guard del import rechaza >65535 verts) + poca RAM
#else
    out.reserve(8 * 1024 * 1024); // PC/Android/WebGL: modelos grandes -> evita reallocs del buffer de 20-40MB
#endif
    out += "# creado con Whisk3D "; out += W3dVersion(); out += "\n"; // version (fecha de build) para identificar la captura
    out += "mtllib "; out += mtlName; out += "\n";

    int vOff = 0, vtOff = 0, vnOff = 0, vcOff = 0; // offsets 1-based acumulados (vc = color por esquina)
    std::set<Material*> usedMats;

    for (size_t mi = 0; mi < meshes.size(); mi++) {
        ProgressUpdate((float)mi / (float)meshes.size()); // avance de la barra (por malla)
        Mesh* m = meshes[mi];
        if (!m->vertex || m->vertexSize <= 0) continue;

        // FUENTE de geometria: con applyModifiers y stack presente -> malla GENERADA (mirror, etc.); sino la editable.
        // applyTransforms -> posiciones/normales a MUNDO; sino en LOCAL. AMBOS afectan SOLO el archivo, no la escena.
        bool useGen = applyModifiers && !m->modifiers.empty();
        if (useGen) m->GenerateMeshModified(); // refresca (en Object mode = todos los modificadores de viewport)
        useGen = useGen && m->genValid && m->genVertex && m->genFaces && m->genVertexSize > 0;
        const GLfloat* Vp = useGen ? m->genVertex     : m->vertex;
        const GLbyte*  Vn = useGen ? m->genNormals    : m->normals;
        const GLfloat* Vu = useGen ? m->genUV         : m->uv;
        const GLubyte* Vc = useGen ? m->genColor      : m->vertexColor;
        const int nV      = useGen ? m->genVertexSize : m->vertexSize;
        const std::vector<MaterialGroup>& mgrp = useGen ? m->genMaterialsGroup : m->materialsGroup;

        // caras (indices en Vp) + su mesh part (indice en mgrp). gen = triangulos por grupo; editable = faces3d (quads/ngons).
        std::vector<std::vector<int> > faceList; std::vector<int> faceMat;
        if (useGen) {
            for (size_t g=0; g<mgrp.size(); g++){ const MaterialGroup& G=mgrp[g];
                for (int k=G.startDrawn; k+2 < G.startDrawn+G.indicesDrawnCount; k+=3){
                    std::vector<int> t(3); t[0]=m->genFaces[k]; t[1]=m->genFaces[k+1]; t[2]=m->genFaces[k+2];
                    faceList.push_back(t); faceMat.push_back((int)g); } }
        } else {
            for (size_t f=0; f<m->faces3d.size(); f++){ if (m->faces3d[f].idx.size()<3) continue;
                faceList.push_back(m->faces3d[f].idx); int mt=m->faces3d[f].mat; if (mt<0||mt>=(int)mgrp.size()) mt=0; faceMat.push_back(mt); } }

        Quaternion R = RotGlobalOf(m); // para llevar las normales a mundo (solo si applyTransforms)
        const bool hasN  = (Vn != NULL);
        const bool hasUV = (Vu != NULL);
        const bool hasCol= (Vc != NULL);

        out += "o "; out += (m->name.empty() ? std::string("Mesh") : m->name); out += "\n";

        // DEDUP de POSICIONES: los render verts COINCIDENTES comparten el indice 'v' (igual que el
        // editor, donde mover una esquina mueve los 3 verts asociados). Asi Blender los ve UNIDOS y
        // no "flotando". vt/vn quedan POR ESQUINA (cada render vert) -> 'f v/vt/vn' comparte la 'v'.
        TPosMap posMap;
        std::vector<int> vOf(nV);       // vOf[i] = indice (0-based, local) de la posicion del vert i
        std::vector<int> repOf;         // repOf[g] = primer render vert del grupo g (color/posicion)
        for (int i = 0; i < nV; i++) {
            std::string key((const char*)&Vp[i*3], 12); // 3 floats = 12 bytes (match EXACTO)
            TPosMap::iterator it = posMap.find(key);
            if (it == posMap.end()) { int g = (int)repOf.size(); posMap[key] = g; vOf[i] = g; repOf.push_back(i); }
            else vOf[i] = it->second;
        }
        const int nPos = (int)repOf.size();

        // COLOR per-corner? AUTO-DETECT: si dos render verts en la MISMA posicion tienen colores
        // DISTINTOS -> el color es por esquina (vc + f .../vc). Si todos los del grupo son iguales ->
        // por vertice (color en 'v', Blender). Asi el round-trip no depende de la capa (que puede no
        // existir aun tras importar). El flag ColorLayer::perVertex es para EDITAR (pestaña Vertices).
        bool colCorner = false;
        if (hasCol) for (int i = 0; i < nV && !colCorner; i++) {
            int rep = repOf[vOf[i]];
            if (Vc[i*4]   != Vc[rep*4]   || Vc[i*4+1] != Vc[rep*4+1] ||
                Vc[i*4+2] != Vc[rep*4+2] || Vc[i*4+3] != Vc[rep*4+3])
                colCorner = true;
        }

        // v: uno por POSICION unica + color representativo del grupo (per-vertex, Blender). applyTransforms -> a MUNDO.
        for (int g = 0; g < nPos; g++) {
            int rep = repOf[g];
            Vector3 lp(Vp[rep*3], Vp[rep*3+1], Vp[rep*3+2]);
            // BASE: los vertices que van al archivo no pueden depender de la vista que se dibujo
            // ultimo. Ademas es lo COHERENTE con las normales, que ya salen por RotGlobalOf (los
            // campos, o sea la base): con LocalToWorld efectiva, un billboard rotaba las posiciones
            // y no las normales.
            Vector3 wp = applyTransforms ? m->LocalToWorldBase(lp) : lp;
            out += "v "; AppendFloat(out, wp.x); out += ' '; AppendFloat(out, wp.y); out += ' '; AppendFloat(out, wp.z);
            // color en 'v' SOLO si la capa es por-vertice (Blender lo lee); por-corner va en 'vc'
            if (hasCol && !colCorner) {
                out += ' '; AppendFloat(out, Vc[rep*4]/255.0f); out += ' '; AppendFloat(out, Vc[rep*4+1]/255.0f);
                out += ' '; AppendFloat(out, Vc[rep*4+2]/255.0f); out += ' '; AppendFloat(out, Vc[rep*4+3]/255.0f);
            }
            out += '\n';
        }
        if (hasN) for (int i = 0; i < nV; i++) {
            Vector3 ln0(Vn[i*3]/127.0f, Vn[i*3+1]/127.0f, Vn[i*3+2]/127.0f);
            Vector3 wn = applyTransforms ? (R * ln0) : ln0;
            float ln = sqrtf(wn.x*wn.x+wn.y*wn.y+wn.z*wn.z); if (ln>1e-6f) wn=wn*(1.0f/ln);
            out += "vn "; AppendFloat(out, wn.x); out += ' '; AppendFloat(out, wn.y); out += ' '; AppendFloat(out, wn.z); out += '\n';
        }
        if (hasUV) for (int i = 0; i < nV; i++) { // el importador hace 1-v: lo deshago
            out += "vt "; AppendFloat(out, Vu[i*2]); out += ' '; AppendFloat(out, 1.0f - Vu[i*2+1]); out += '\n';
        }
        // COLOR POR ESQUINA (capa por-corner): una linea 'vc r g b a' por render vert (esquina).
        if (colCorner) for (int i = 0; i < nV; i++) {
            out += "vc "; AppendFloat(out, Vc[i*4]/255.0f); out += ' '; AppendFloat(out, Vc[i*4+1]/255.0f);
            out += ' '; AppendFloat(out, Vc[i*4+2]/255.0f); out += ' '; AppendFloat(out, Vc[i*4+3]/255.0f); out += '\n';
        }

        // caras (faceList: quads/ngons de faces3d o triangulos del gen) con su material (por cara = faceMat).
        // f POSICION_COMPARTIDA/vt/vn (vt y vn por esquina = render vert)
        int curMat = -1;
        for (size_t f = 0; f < faceList.size(); f++) {
            if (!faceList.empty()) // barra GRANULAR (antes saltaba 0->100): avance por cara dentro de la malla
                ProgressUpdate(((float)mi + (float)f / (float)faceList.size()) / (float)meshes.size());
            const std::vector<int>& idx = faceList[f];
            if (idx.size() < 3) continue;
            int mt = faceMat[f];
            if (mt != curMat) { curMat = mt; // usemtl al cambiar de mesh part
                if (mt>=0 && mt<(int)mgrp.size() && mgrp[mt].material) {
                    out += "usemtl "; out += mgrp[mt].material->name; out += "\n";
                    usedMats.insert(mgrp[mt].material);
                }
            }
            out += "f";
            for (size_t c = 0; c < idx.size(); c++) {
                int rv = idx[c];
                out += ' '; AppendInt(out, vOff + vOf[rv] + 1); // POSICION compartida (1-based global)
                if (hasUV || hasN || colCorner) {   // f v/vt/vn[/vc] (vc = color por esquina, opcional)
                    out += '/'; if (hasUV) AppendInt(out, vtOff + rv + 1);
                    if (hasN || colCorner) { out += '/'; if (hasN) AppendInt(out, vnOff + rv + 1); }
                    if (colCorner) { out += '/'; AppendInt(out, vcOff + rv + 1); }
                }
            }
            out += '\n';
        }
        // BORDES SUELTOS: aristas SIN cara -> lineas 'l a b' (Blender las lee/escribe asi). Sirven de PERFIL
        // (Screw) o para dibujar planos. Usan la POSICION compartida (vOf), igual que las caras. Solo sin gen
        // (con modificadores aplicados el perfil ya paso a ser superficie).
        if (!useGen) {
            for (size_t i = 0; i + 1 < m->looseEdges.size(); i += 2) {
                int a = m->looseEdges[i], b = m->looseEdges[i+1];
                if (a<0||a>=nV||b<0||b>=nV) continue;
                out += "l "; AppendInt(out, vOff + vOf[a] + 1); out += ' '; AppendInt(out, vOff + vOf[b] + 1); out += '\n';
            }
        }
        vOff += nPos; if (hasN) vnOff += nV; if (hasUV) vtOff += nV; if (colCorner) vcOff += nV;
    }
    obj.write(out.data(), (std::streamsize)out.size()); // UNA sola escritura de todo el OBJ
    obj.close();

    // MTL al lado del OBJ
    std::string mtlPath = DirOf(filepath) + mtlName;
    std::ofstream mtl(mtlPath.c_str());
    if (mtl.is_open()) {
        mtl.precision(7);
        mtl << "# creado con Whisk3D\n\n";
        std::string exportDir = DirOf(filepath); // el .mtl y el .obj estan aca -> las texturas se referencian relativas a esto
#ifdef __EMSCRIPTEN__
        // navegador: el .obj y el .mtl bajan a la carpeta de descargas de la PC, sin las carpetas del disco virtual.
        // Cada textura se copia al lado del export y baja tambien (una vez cada una): el .mtl la nombra por basename.
        std::set<std::string> texDescents;
        for (std::set<Material*>::iterator it = usedMats.begin(); it != usedMats.end(); ++it) {
            Material* mt = *it;
            if (!mt || !mt->texture || mt->texture->path.empty()) continue;
            const std::string& orig = mt->texture->path;
            const std::string dest = exportDir + BaseNameTex(orig);
            if (!texDescents.insert(dest).second) continue;
            if (dest != orig) {
                std::vector<unsigned char> bytes;
                if (!w3dFileSystem::ReadFileBytes(orig, bytes) || bytes.empty()) { w3dLogfW("ExportOBJ: no pude leer la textura %s", orig.c_str()); continue; }
                std::ofstream tf(dest.c_str(), std::ios::binary);
                if (!tf.is_open()) { w3dLogfW("ExportOBJ: no pude copiar la textura a %s", dest.c_str()); continue; }
                tf.write((const char*)&bytes[0], (std::streamsize)bytes.size()); tf.close();
            }
            W3dDownloadIfWeb(dest);
        }
#endif
        for (std::set<Material*>::iterator it = usedMats.begin(); it != usedMats.end(); ++it)
            WriteMaterialMTL(mtl, *it, exportDir);
        mtl.close();
    }
    ProgressEnd();
    return true;
}

bool ImportOBJ(const std::string& filepath, bool NoMerge = false) {
    w3dLogf("ImportOBJ: INICIO '%s' (len=%d)", filepath.c_str(), (int)filepath.size());
    if (filepath.size() < 4 || filepath.substr(filepath.size() - 4) != ".obj") {
        w3dLog("ImportOBJ: rechazado (no termina en .obj)");
#ifndef W3D_SYMBIAN
        std::cerr << "Error: El archivo seleccionado no tiene la extension .obj" << std::endl;
#endif
        return false;
    }

    // Leemos el archivo ENTERO a UN buffer y tokenizamos en punteros const char* (un solo alloc, sin un std::string
    // por linea -> antes eran ~663k allocs/2.7s para 200k verts). fileData vive hasta el final del parseo (lines
    // apunta adentro). BINARY: que el tokenizador vea el \r crudo y lo limpie el (no doble traduccion del runtime).
    // Leemos el archivo ENTERO por la ABSTRACCION del Core (w3dFileSystem::ReadTextFile):
    // UN solo camino para todas las plataformas (en Android resuelve solo si es asset del
    // APK o archivo real de /storage). fileData vive hasta el final del parseo (lines apunta adentro).
    bool _okRead = false;
    std::string fileData = w3dFileSystem::ReadTextFile(filepath, &_okRead);
    w3dLogf("ImportOBJ: open ok=%d", (int)_okRead);
    if (!_okRead) {
#ifndef W3D_SYMBIAN
        std::cerr << "Error al abrir: " << filepath << std::endl;
#endif
        return false;
    }

    // tokenizar en lugar: cada '\n' (y el final) -> '\0'; el \r de Windows tambien -> '\0'. lines[i] = puntero al
    // inicio de cada linea dentro de fileData.
    std::vector<const char*> lines;
    if (!fileData.empty()) {
        char* base = &fileData[0];
        size_t n = fileData.size();
        lines.reserve(n / 16 + 16); // estimacion grosera (~16 bytes/linea) para evitar reallocs del vector
        size_t start = 0;
        for (size_t i = 0; i <= n; i++) {
            if (i == n || base[i] == '\n') {
                if (i > start && base[i-1] == '\r') base[i-1] = '\0'; // strip \r (Windows)
                if (i < n) base[i] = '\0';                            // terminar la linea (el final ya tiene el \0 del std::string)
                lines.push_back(base + start);
                start = i + 1;
            }
        }
    }

    w3dLogf("ImportOBJ: '%s' -> %d lineas", filepath.c_str(), (int)lines.size());
    ProgressStart(T("Importing model...")); // barra de progreso (clave en el N95: importar tarda)

    size_t idx = 0;
    int loadedObjects = 0;
    int accumulatedVertices = 0;
    int accumulatedNormals  = 0;
    int accumulatedUVs = 0;
    int accumulatedColors = 0;

    // cada ReadOBJ crea UN objeto (avanza idx) y devuelve si hay mas
    bool hasMore = true;
    while (idx < lines.size() && hasMore) {
        hasMore = ReadOBJ(lines, idx, filepath, &accumulatedVertices, &accumulatedNormals, &accumulatedUVs, &accumulatedColors, NoMerge);
        loadedObjects++;
        w3dLogf("ImportOBJ: obj %d parseado (idx=%d v=%d)", loadedObjects, (int)idx, accumulatedVertices);
    }
    w3dLogf("ImportOBJ: parse OK (%d objetos, %d verts)", loadedObjects, accumulatedVertices);

    // Archivo .mtl asociado
    std::string mtlFile = filepath.substr(0, filepath.size() - 4) + ".mtl";
    // la EXISTENCIA tambien por el Core (pak / APK / contenedor), no con ifstream
    bool mtlExists = w3dFileSystem::FileExists(mtlFile);
    w3dLogf("ImportOBJ: mtl '%s' exists=%d", mtlFile.c_str(), (int)mtlExists);
    if (mtlExists) {
        if (!ReadMTL(mtlFile, loadedObjects)) {
#ifndef W3D_SYMBIAN
            std::cerr << "Error al leer el archivo .mtl" << std::endl;
#endif
        }
    } else {
#ifndef W3D_SYMBIAN
        std::cerr << "El archivo .mtl no existe" << std::endl;
#endif
    }

    w3dLog("ImportOBJ: FIN OK (si crashea despues de esto, es el render de la malla importada)");
    ProgressEnd();
    Notify(T("OBJ imported successfully!"), false); // toast verde de exito
    return true;
}
