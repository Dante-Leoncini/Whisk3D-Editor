// ============================================================================
//  PropsHorneado.cpp — la tarjeta "Bake Lighting" (precalcular la iluminacion) de la pestania Material, debajo de la
//  tarjeta Material. Hornea la luz de la malla ACTIVA (las lamparas con sus sombras + la luz indirecta, solo con lo
//  estatico: render/RayTracer.h) en una TEXTURA, por su capa UV:
//    - Texture: la textura donde se escribe (una cargada, o "New Texture..." con su definicion: el popup de siempre)
//    - UV: la capa UV de la malla que dice donde cae cada cara en la textura
//    - Samples: los rayos de luz indirecta por texel (mas = menos ruido, mas lento)
//    - Bake: escribe SOLO los texels de las caras de la malla (y dos de margen alrededor, para que el filtro no lea lo
//      de afuera en la orilla); el resto de la textura queda como estaba. Se puede deshacer (Ctrl+Z)
//  Al terminar, la textura queda como CAPA "Multiply 2x" de cada material de la malla (con esa capa UV) y esos
//  materiales sin la luz de tiempo real (la luz ya esta en la textura: si no, se sumaria dos veces). Mientras trabaja,
//  la barra muestra el progreso y el boton cancela.
// ============================================================================
#include "ViewPorts/Properties.h"
#include "ViewPorts/PopUp/NuevaTexturaPopup.h"
#include "WhiskUI/widgets/PopupMenu.h"   // los desplegables de la textura y de la capa UV
#include "objects/Mesh.h"
#include "objects/Textures.h"
#include "io/TexturaEditada.h"
#include "render/RayTracer.h"
#include "undo/Undo.h"
#include "W3dLang.h"
#include <stdio.h>

extern void Notificar(const std::string& texto, bool error);
extern void PropsLayoutSucio();
extern void RebindMaterialMeshPart();
extern void (*g_nuevaTexturaCreadaHook)(const std::string& ruta);   // (NuevaTexturaPopup.cpp)
void PropsAbrirMenuBajoBoton(PopupMenu* menu, Button* boton);       // (Properties.cpp: el estandar de los desplegables)

static Texture* gHornTex = NULL;     // la textura elegida (se valida contra Textures en cada uso)
static int      gHornUV = -1;        // la capa UV (-1 = la activa de la malla)
static float    gHornMuestras = 32.0f;
static bool     gHornIndirecta = true;   // la luz que rebota (el fondo y lo de alrededor)
static float    gHornRebotes = 3.0f;     // cuantas veces rebota (con mas, la sombra recibe la luz que rebota en lo que la tapa)
static bool     gHornDenoiser = true;    // la indirecta suavizada (sin granulado)
static Mesh*    gHornMalla = NULL;   // la malla que se esta horneando
static TexturaEditable* gHornTe = NULL;
static int      gHornUVUsada = 0;
static float    gHornProgreso = 0.0f;
static PopupMenu* gMenuHornTex = NULL;
static PopupMenu* gMenuHornUV = NULL;

static Mesh* MallaActiva() {
    return (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
}
static bool TexturaViva(const Texture* t) {
    if (!t) return false;
    for (size_t i = 0; i < Textures.size(); i++) if (Textures[i] == t) return true;
    return false;
}
static std::string NombreTex(const Texture* t) {
    if (!t) return std::string(T("Choose a texture"));
    std::string n = t->path;
    const size_t b = n.find_last_of("/\\"); if (b != std::string::npos) n = n.substr(b + 1);
    return n;
}

// ---- la textura ----
static void AccionTexElegida(int id) {
    if (id == 1) {   // "New Texture...": el popup de siempre; al crearla queda elegida
        g_nuevaTexturaCreadaHook = NULL;
        struct H { static void Creada(const std::string& ruta) { Texture* t = TexturaBuscar(ruta); if (t) gHornTex = t; g_redraw = true; } };
        g_nuevaTexturaCreadaHook = H::Creada;
        AbrirNuevaTexturaPopup();
        return;
    }
    const size_t i = (size_t)(id - 2) + 5;
    if (id >= 2 && i < Textures.size()) gHornTex = Textures[i];
    g_redraw = true;
}
static void AccionMenuTex() {
    if (!PropsActivo || !PropsActivo->propHornTex) return;
    if (!gMenuHornTex) { gMenuHornTex = new PopupMenu(); gMenuHornTex->action = AccionTexElegida; }
    gMenuHornTex->Limpiar();
    gMenuHornTex->Agregar(T("New Texture..."), 1, IconType::mas);
    for (size_t i = 5; i < Textures.size(); i++)
        if (Textures[i]) gMenuHornTex->Agregar(NombreTex(Textures[i]), 2 + (int)(i - 5), IconType::textura);
    PropsAbrirMenuBajoBoton(gMenuHornTex, PropsActivo->propHornTex->button);
}
// ---- la capa UV ----
static void AccionUVElegida(int id) { gHornUV = id; g_redraw = true; }
static void AccionMenuUV() {
    Mesh* m = MallaActiva();
    if (!m || !PropsActivo || !PropsActivo->propHornUV) return;
    if (!gMenuHornUV) { gMenuHornUV = new PopupMenu(); gMenuHornUV->action = AccionUVElegida; }
    gMenuHornUV->Limpiar();
    if (m->uvMaps.empty()) gMenuHornUV->Agregar("UV", 0, IconType::material);
    for (size_t u = 0; u < m->uvMaps.size(); u++)
        gMenuHornUV->Agregar(m->uvMaps[u] ? m->uvMaps[u]->nombre : std::string("UV"), (int)u, IconType::material);
    PropsAbrirMenuBajoBoton(gMenuHornUV, PropsActivo->propHornUV->button);
}
static int UVElegida(const Mesh* m) {
    if (!m) return 0;
    if (gHornUV >= 0 && gHornUV < (int)m->uvMaps.size()) return gHornUV;
    return m->uvMapActivo >= 0 ? m->uvMapActivo : 0;
}

// ---- hornear ----
static void Terminar() {
    Mesh* m = gHornMalla;
    TexturaEditable* te = gHornTe;
    gHornMalla = NULL; gHornTe = NULL;
    if (!m || !te || !TexturaViva(te->tex)) return;
    const std::vector<float>& luz = RTHorneadoLuz();
    const std::vector<unsigned char>& cub = RTHorneadoCubierto();
    const int w = te->w, h = te->h;
    if ((int)cub.size() != w * h || (int)te->rgba.size() < w * h * 4) return;
    // los texels de las caras + DOS de margen (el promedio de los vecinos cubiertos): el filtro bilineal lee la orilla
    std::vector<float> L(luz);
    std::vector<unsigned char> c(cub);
    for (int pasada = 0; pasada < 2; pasada++) {
        std::vector<float> L2(L); std::vector<unsigned char> c2(c);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const int id = y * w + x;
                if (c[(size_t)id]) continue;
                float acc[3] = { 0, 0, 0 }; int n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h || !c[(size_t)(yy * w + xx)]) continue;
                        const float* q = &L[(size_t)(yy * w + xx) * 3];
                        acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]; n++;
                    }
                if (!n) continue;
                for (int k = 0; k < 3; k++) L2[(size_t)id * 3 + k] = acc[k] / (float)n;
                c2[(size_t)id] = 1;
            }
        L.swap(L2); c.swap(c2);
    }
    UndoTexturaIniciar(te);
    for (int i = 0; i < w * h; i++) {
        if (!c[(size_t)i]) continue;
        for (int k = 0; k < 3; k++) {   // (la mitad: la capa es Multiply 2x, asi entra luz de hasta el doble de blanco)
            const float v = L[(size_t)i * 3 + k] * 0.5f;
            te->rgba[(size_t)i * 4 + k] = (unsigned char)(v <= 0.0f ? 0 : v >= 1.0f ? 255 : (int)(v * 255.0f + 0.5f));
        }
        te->rgba[(size_t)i * 4 + 3] = 255;
    }
    te->modificada = true;
    TexEditSubir(te, 0, 0, w, h);
    UndoTexturaConfirmar(true);
    // LA TEXTURA COMO CAPA de los materiales de la malla, y sin la luz de tiempo real
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        Material* mat = m->materialsGroup[g].material;
        if (!mat || mat == MaterialDefecto) continue;
        UndoMaterialModIniciar(mat);
        bool hay = false;
        for (size_t k = 0; k < mat->capas.size(); k++)
            if (mat->capas[k].tex == te->tex) { mat->capas[k].blend = W3D_CAPA_MULTIPLY2X; mat->capas[k].uvMapa = gHornUVUsada;
                                                mat->capas[k].on = true; mat->capas[k].horneada = true; hay = true; }
        if (!hay) {
            TexLayer tl; tl.blend = W3D_CAPA_MULTIPLY2X; tl.uvMapa = gHornUVUsada; tl.fuerza = 1.0f; tl.horneada = true;
            TexturaPonerEn(tl.tex, te->tex);
            mat->capas.push_back(tl);
        }
        // (la capa es de LUZ HORNEADA: mientras este prendida el material va sin la luz de tiempo real. Sacarla la devuelve)
        UndoMaterialModCommit();
    }
    RebindMaterialMeshPart();
    RTInvalidar();
    Notificar(T("Lighting baked"), false);
    g_redraw = true;
}
static void AccionHornear() {
    if (RTHorneadoActivo()) { RTHorneadoCancelar(); gHornMalla = NULL; gHornTe = NULL; Notificar(T("Bake canceled"), true); g_redraw = true; return; }
    Mesh* m = MallaActiva();
    if (!m) return;
    if (!TexturaViva(gHornTex)) { Notificar(T("Choose a texture to bake into"), true); return; }
    TexturaEditable* te = TexEditObtener(gHornTex);
    if (!te || te->w <= 0 || te->h <= 0) { Notificar(T("Could not read the image"), true); return; }
    const int uv = UVElegida(m);
    std::string motivo;
    if (!RTHorneadoEmpezar(m, uv, te->w, te->h, (int)(gHornMuestras + 0.5f), &motivo, gHornIndirecta, (int)(gHornRebotes + 0.5f), gHornDenoiser)) {
        Notificar(std::string(T("Not baked")) + ": " + T(motivo.c_str()), true);
        return;
    }
    gHornMalla = m; gHornTe = te; gHornUVUsada = uv; gHornProgreso = 0.0f;
    g_redraw = true;
}

// DE UNA SOLA VEZ (el harness): sin la barra, hasta terminar
bool PropsHorneadoAhora(Mesh* m, Texture* t, int uv, int muestras, std::string* motivo) {
    if (!m || !TexturaViva(t)) { if (motivo) *motivo = "Choose a texture to bake into"; return false; }
    TexturaEditable* te = TexEditObtener(t);
    if (!te || te->w <= 0 || te->h <= 0) { if (motivo) *motivo = "Could not read the image"; return false; }
    if (uv < 0) uv = UVElegida(m);
    if (!RTHorneadoEmpezar(m, uv, te->w, te->h, muestras, motivo, gHornIndirecta, (int)(gHornRebotes + 0.5f), gHornDenoiser)) return false;
    gHornMalla = m; gHornTe = te; gHornUVUsada = uv;
    while (RTHorneadoActivo()) RTHorneadoPaso(1000);
    Terminar();
    return true;
}

void PropsHorneadoConstruir(Properties* p) {
    if (!p) return;
    p->propHorneado = new GroupPropertie(T("Bake Lighting"));
    p->propHornTex = new PropButton(T("Texture"), IconType::textura);
    p->propHornTex->conLabel = true;
    p->propHornTex->button->desplegable = true;
    p->propHornTex->action = AccionMenuTex;
    p->propHorneado->properties.push_back(p->propHornTex);
    p->propHornUV = new PropButton(T("UV"), IconType::material);
    p->propHornUV->conLabel = true;
    p->propHornUV->button->desplegable = true;
    p->propHornUV->action = AccionMenuUV;
    p->propHorneado->properties.push_back(p->propHornUV);
    p->propHornMuestras = new PropFloat(T("Samples"));
    p->propHornMuestras->SetRango(1.0f, 1024.0f);
    p->propHornMuestras->entero = true;
    p->propHornMuestras->stepFino = 8.0f; p->propHornMuestras->stepGrueso = 32.0f; p->propHornMuestras->dragStep = 0.5f;
    p->propHornIndirecta = new PropBool(T("Indirect light"));
    p->propHorneado->properties.push_back(p->propHornIndirecta);
    p->propHorneado->properties.push_back(p->propHornMuestras);
    p->propHornRebotes = new PropFloat(T("Bounces"));
    p->propHornRebotes->SetRango(1.0f, 8.0f);
    p->propHornRebotes->entero = true;
    p->propHornRebotes->stepFino = 1.0f; p->propHornRebotes->stepGrueso = 1.0f; p->propHornRebotes->dragStep = 0.05f;
    p->propHorneado->properties.push_back(p->propHornRebotes);
    p->propHornDenoiser = new PropBool(T("Denoiser"));
    p->propHorneado->properties.push_back(p->propHornDenoiser);
    p->propHornBoton = new PropButton(T("Bake"), IconType::esfera);
    p->propHornBoton->action = AccionHornear;
    p->propHorneado->properties.push_back(p->propHornBoton);
    p->propHornBarra = new PropButton("0%");   // (la barra del progreso, solo mientras hornea)
    p->propHornBarra->button->barra = 0.0f;
    p->propHorneado->properties.push_back(p->propHornBarra);
    p->propHornAviso = new PropLabel(T("Bakes the light of the static objects into the texture, through the UV of the mesh"), true);
    p->propHorneado->properties.push_back(p->propHornAviso);
    p->GroupProperties.push_back(p->propHorneado);
}

void PropsHorneadoActualizar(Properties* p, bool visible) {
    if (!p || !p->propHorneado) return;
    // EL TRABAJO avanza una vez por cuadro (aunque haya varios paneles): ~25 ms por cuadro, el editor sigue andando
    static unsigned int ultimo = 0;
    extern unsigned int w3dGetTicks();
    if (RTHorneadoActivo() && w3dGetTicks() - ultimo >= 10) {
        ultimo = w3dGetTicks();
        gHornProgreso = RTHorneadoPaso(25);
        if (!RTHorneadoActivo()) Terminar();
        g_redraw = true;
    }
    Mesh* m = visible ? MallaActiva() : NULL;
    p->propHorneado->visible = (m != NULL);
    if (!m) return;
    if (!TexturaViva(gHornTex)) gHornTex = NULL;
    const bool trabajando = RTHorneadoActivo();
    std::string t = NombreTex(gHornTex);
    if (p->propHornTex->button->text != t) { p->propHornTex->button->text = t; PropsLayoutSucio(); }
    const int uv = UVElegida(m);
    std::string tu = (uv >= 0 && uv < (int)m->uvMaps.size() && m->uvMaps[(size_t)uv]) ? m->uvMaps[(size_t)uv]->nombre : std::string("UV");
    if (p->propHornUV->button->text != tu) p->propHornUV->button->text = tu;
    p->propHornIndirecta->value = trabajando ? NULL : &gHornIndirecta;
    p->propHornMuestras->value = (trabajando || !gHornIndirecta) ? NULL : &gHornMuestras;
    p->propHornRebotes->value = (trabajando || !gHornIndirecta) ? NULL : &gHornRebotes;
    p->propHornDenoiser->value = (trabajando || !gHornIndirecta) ? NULL : &gHornDenoiser;
    const std::string tb = trabajando ? std::string(T("Cancel")) : std::string(T("Bake"));
    if (p->propHornBoton->button->text != tb) { p->propHornBoton->button->text = tb; PropsLayoutSucio(); }
    p->propHornBarra->oculto = !trabajando;
    if (trabajando) {
        char b[32]; snprintf(b, sizeof(b), "%d%%", (int)(gHornProgreso * 100.0f));
        p->propHornBarra->button->text = b;
        p->propHornBarra->button->barra = gHornProgreso;
    }
    p->propHornAviso->oculto = trabajando;
}
