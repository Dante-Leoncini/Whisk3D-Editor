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
#include <map>

extern void Notificar(const std::string& texto, bool error);
extern void PropsLayoutSucio();
extern void RebindMaterialMeshPart();
extern void (*g_nuevaTexturaCreadaHook)(const std::string& ruta);   // (NuevaTexturaPopup.cpp)
void PropsAbrirMenuBajoBoton(PopupMenu* menu, Button* boton);       // (Properties.cpp: el estandar de los desplegables)

// LA TEXTURA Y LA CAPA UV DE CADA MALLA (por su serial): cada objeto hornea en la SUYA. Antes eran una sola para todos y,
// al pasar a otro objeto, seguia elegida la del anterior: el escenario se horneaba sin querer en la textura del cubo (y sus
// materiales quedaban con las dos capas de luz). La primera vez arranca con la de su luz ya horneada, si tiene
struct HornElegido { Texture* tex; int uv; bool listo; HornElegido() : tex(NULL), uv(-1), listo(false) {} };
static std::map<unsigned int, HornElegido> gHornPorMalla;
static Mesh* MallaActiva();
static HornElegido& ElegidoDe(Mesh* m) {
    static HornElegido nada;
    if (!m) { nada = HornElegido(); return nada; }
    HornElegido& e = gHornPorMalla[m->serial];
    if (!e.listo) {
        e.listo = true;
        for (size_t g = 0; g < m->materialsGroup.size() && !e.tex; g++) {
            Material* mat = m->materialsGroup[g].material;
            if (!mat || mat == MaterialDefecto) continue;
            for (size_t k = 0; k < mat->capas.size(); k++)
                if (mat->capas[k].horneada && mat->capas[k].tex) { e.tex = mat->capas[k].tex; e.uv = mat->capas[k].uvMapa; break; }
        }
    }
    return e;
}
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
        struct H { static void Creada(const std::string& ruta) { Texture* t = TexturaBuscar(ruta); if (t && MallaActiva()) ElegidoDe(MallaActiva()).tex = t; g_redraw = true; } };
        g_nuevaTexturaCreadaHook = H::Creada;
        AbrirNuevaTexturaPopup();
        return;
    }
    const size_t i = (size_t)(id - 2) + 5;
    if (id >= 2 && i < Textures.size() && MallaActiva()) ElegidoDe(MallaActiva()).tex = Textures[i];
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
static void AccionUVElegida(int id) { if (MallaActiva()) ElegidoDe(MallaActiva()).uv = id; g_redraw = true; }
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
    const int uv = ElegidoDe(const_cast<Mesh*>(m)).uv;
    if (uv >= 0 && uv < (int)m->uvMaps.size()) return uv;
    return m->uvMapActivo >= 0 ? m->uvMapActivo : 0;
}

// ---- hornear ----
// otra malla de la escena (que no es 'm') usa 'mat' en alguna de sus partes
static bool MaterialDeOtraMalla(Object* o, Material* mat, Mesh* m) {
    if (!o) return false;
    if (o->getType() == ObjectType::mesh && o != (Object*)m) {
        Mesh* me = (Mesh*)o;
        for (size_t g = 0; g < me->materialsGroup.size(); g++) if (me->materialsGroup[g].material == mat) return true;
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) if (MaterialDeOtraMalla(o->Childrens[i], mat, m)) return true;
    return false;
}
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
    // LA MASCARA DEL SOL (al alfa): que fraccion de la luz de cada texel es la directa del sol (RTHorneadoSol)
    std::vector<float> S(RTHorneadoSol());
    const bool conSol = (int)S.size() == w * h;
    if (!conSol) S.assign((size_t)w * h, 0.0f);
    for (int pasada = 0; pasada < 2; pasada++) {
        std::vector<float> L2(L); std::vector<unsigned char> c2(c); std::vector<float> S2(S);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const int id = y * w + x;
                if (c[(size_t)id]) continue;
                float acc[3] = { 0, 0, 0 }; float accS = 0.0f; int n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h || !c[(size_t)(yy * w + xx)]) continue;
                        const float* q = &L[(size_t)(yy * w + xx) * 3];
                        acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]; accS += S[(size_t)(yy * w + xx)]; n++;
                    }
                if (!n) continue;
                for (int k = 0; k < 3; k++) L2[(size_t)id * 3 + k] = acc[k] / (float)n;
                S2[(size_t)id] = accS / (float)n;
                c2[(size_t)id] = 1;
            }
        L.swap(L2); c.swap(c2); S.swap(S2);
    }
    // UN CAPTADOR DE SOMBRAS: la textura es la sombra (blanco = nada), para una capa Multiply comun
    const bool captador = RTHorneadoCaptador();
    const int modo = captador ? W3D_CAPA_MULTIPLY : W3D_CAPA_MULTIPLY2X;
    UndoTexturaIniciar(te);
    for (int i = 0; i < w * h; i++) {
        if (!c[(size_t)i]) continue;
        for (int k = 0; k < 3; k++) {   // (la mitad: la capa es Multiply 2x, asi entra luz de hasta el doble de blanco)
            const float v = L[(size_t)i * 3 + k] * (captador ? 1.0f : 0.5f);
            te->rgba[(size_t)i * 4 + k] = (unsigned char)(v <= 0.0f ? 0 : v >= 1.0f ? 255 : (int)(v * 255.0f + 0.5f));
        }
        // (el ALFA: la mascara del sol. La capa no lo usa como transparencia: ver TexLayer::mascaraSol)
        const float sf = S[(size_t)i];
        te->rgba[(size_t)i * 4 + 3] = conSol ? (unsigned char)(sf <= 0.0f ? 0 : sf >= 1.0f ? 255 : (int)(sf * 255.0f + 0.5f)) : 255;
    }
    if (conSol) te->alpha = true;   // (el PNG guarda el alfa: la mascara del sol)
    te->modificada = true;
    // (una textura del proyecto viaja con el proximo guardado del .w3d: la luz horneada -y su mascara- no se pierde por
    //  no haber guardado la textura aparte. Una externa se guarda con su boton, como siempre)
    if (!TexEditEsExterna(te->tex)) te->enMemoria = true;
    TexEditSubir(te, 0, 0, w, h);
    UndoTexturaConfirmar(true);
    // UN MATERIAL QUE USAN OTRAS MALLAS: la luz horneada es de ESTA malla (cae por su UV). Puesta en un material compartido,
    // todas las que lo usan la mostraban con sus UV: la luz de otro objeto (el plano nuevo con el material "Default" se
    // veia con la luz del cubo, como una sombra que no era). Esta malla pasa a una COPIA suya del material (con Ctrl+Z)
    UndoGrupoIniciar();
    {
        std::map<Material*, Material*> copias;
        std::vector<std::string> copiados;
        for (size_t g = 0; g < m->materialsGroup.size(); g++) {
            Material* mat = m->materialsGroup[g].material;
            if (!mat || mat == MaterialDefecto || !MaterialDeOtraMalla(SceneCollection, mat, m)) continue;
            Material*& c = copias[mat];
            if (!c) {
                extern Material* W3dMaterialCopiaConUndo(const Material*, const std::string&);
                c = W3dMaterialCopiaConUndo(mat, mat->name + " " + m->name);
                // (la copia no se queda con la luz que el compartido tenia de otra malla)
                for (size_t k = c->capas.size(); k-- > 0; )
                    if (c->capas[k].horneada) { TexturaPonerEn(c->capas[k].tex, NULL); c->capas.erase(c->capas.begin() + (long)k); }
                // ...y el compartido suelta la luz de ESTA textura: se la habia dejado un horneado de esta malla (antes de
                // esta copia) y las otras la mostraban con sus UV
                bool tiene = false;
                for (size_t k = 0; k < mat->capas.size() && !tiene; k++) tiene = mat->capas[k].horneada && mat->capas[k].tex == te->tex;
                if (tiene) {
                    UndoMaterialModIniciar(mat);
                    for (size_t k = mat->capas.size(); k-- > 0; )
                        if (mat->capas[k].horneada && mat->capas[k].tex == te->tex) { TexturaPonerEn(mat->capas[k].tex, NULL); mat->capas.erase(mat->capas.begin() + (long)k); }
                    UndoMaterialModCommit();
                }
                copiados.push_back(mat->name);
            }
            UndoCapturarMaterial(m, (int)g);
            m->materialsGroup[g].material = c;
        }
        if (!copiados.empty()) {
            std::string l;
            for (size_t i = 0; i < copiados.size(); i++) l += (i ? ", " : "") + copiados[i];
            Notificar(std::string(T("The material is shared with other objects: this one got its own copy")) + " (" + l + ")", false);
        }
    }
    // LA TEXTURA COMO CAPA de los materiales de la malla, y sin la luz de tiempo real
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        Material* mat = m->materialsGroup[g].material;
        if (!mat || mat == MaterialDefecto) continue;
        UndoMaterialModIniciar(mat);
        // (la luz NUEVA reemplaza a la que tenia horneada en otra textura: si no, se multiplicaban las dos)
        for (size_t k = mat->capas.size(); k-- > 0; )
            if (mat->capas[k].horneada && mat->capas[k].tex != te->tex) {
                TexturaPonerEn(mat->capas[k].tex, NULL);   // (suelta su referencia a la textura)
                mat->capas.erase(mat->capas.begin() + (long)k);
            }
        bool hay = false;
        for (size_t k = 0; k < mat->capas.size(); k++)
            if (mat->capas[k].tex == te->tex) { mat->capas[k].blend = modo; mat->capas[k].uvMapa = gHornUVUsada;
                                                mat->capas[k].on = true; mat->capas[k].horneada = true;
                                                mat->capas[k].mascaraSol = conSol; hay = true; }
        if (!hay) {
            TexLayer tl; tl.blend = modo; tl.uvMapa = gHornUVUsada; tl.fuerza = 1.0f; tl.horneada = true; tl.mascaraSol = conSol;
            TexturaPonerEn(tl.tex, te->tex);
            mat->capas.push_back(tl);
        }
        // (la capa es de LUZ HORNEADA: mientras este prendida el material va sin la luz de tiempo real. Sacarla la devuelve)
        UndoMaterialModCommit();
    }
    UndoGrupoFin();
    RebindMaterialMeshPart();
    RTInvalidar();
    Notificar(T("Lighting baked"), false);
    g_redraw = true;
}
static void AccionCancelar() {
    if (!RTHorneadoActivo()) return;
    RTHorneadoCancelar(); gHornMalla = NULL; gHornTe = NULL; Notificar(T("Bake canceled"), true); PropsLayoutSucio(); g_redraw = true;
}
static void AccionHornear() {
    if (RTHorneadoActivo()) { AccionCancelar(); return; }
    Mesh* m = MallaActiva();
    if (!m) return;
    Texture* tex = ElegidoDe(m).tex;
    if (!TexturaViva(tex)) { Notificar(T("Choose a texture to bake into"), true); return; }
    TexturaEditable* te = TexEditObtener(tex);
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
    p->propHornAvisoTex = new PropLabel("", true);
    p->propHornAvisoTex->oculto = true;
    p->propHorneado->properties.push_back(p->propHornAvisoTex);
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
    // CANCELAR (la cruz), abajo de la barra: solo mientras hornea (el boton Bake se esconde)
    p->propHornCancelar = new PropButton(T("Cancel"), IconType::cerrar);
    p->propHornCancelar->action = AccionCancelar;
    p->propHornCancelar->oculto = true;
    p->propHorneado->properties.push_back(p->propHornCancelar);
    p->propHornAviso = new PropLabel(T("Bakes the light of the static objects into the texture, through the UV of the mesh"), true);
    p->propHorneado->properties.push_back(p->propHornAviso);
    p->GroupProperties.push_back(p->propHorneado);
}

// las OTRAS mallas de la escena que ya tienen la luz horneada en 't' (una capa de luz de sus materiales): hornear 'm' en la
// misma textura pisa lo que se cruce con sus UV
static void MallasConLuzEn(Object* o, Mesh* m, Texture* t, std::vector<std::string>& out) {
    if (!o || out.size() >= 4) return;
    if (o->getType() == ObjectType::mesh && o != (Object*)m) {
        Mesh* me = (Mesh*)o;
        bool usa = false;
        for (size_t g = 0; g < me->materialsGroup.size() && !usa; g++) {
            Material* mat = me->materialsGroup[g].material;
            if (!mat) continue;
            for (size_t k = 0; k < mat->capas.size() && !usa; k++) usa = mat->capas[k].horneada && mat->capas[k].tex == t;
        }
        if (usa) out.push_back(o->name);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) MallasConLuzEn(o->Childrens[i], m, t, out);
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
    HornElegido& el = ElegidoDe(m);
    if (!TexturaViva(el.tex)) el.tex = NULL;
    const bool trabajando = RTHorneadoActivo();
    // EL AVISO: esa textura ya tiene la luz de otro objeto
    if (p->propHornAvisoTex) {
        std::string av;
        if (el.tex && !trabajando) {
            std::vector<std::string> otras;
            MallasConLuzEn(SceneCollection, m, el.tex, otras);
            for (size_t i = 0; i < otras.size(); i++) av += (i ? ", " : "") + otras[i];
            if (!av.empty()) av = std::string(T("This texture already has the baked light of")) + ": " + av + ". " +
                                  T("Baking here overwrites where their UVs overlap");
        }
        if (p->propHornAvisoTex->name != av || p->propHornAvisoTex->oculto != av.empty()) {
            p->propHornAvisoTex->name = av; p->propHornAvisoTex->oculto = av.empty(); PropsLayoutSucio();
        }
    }
    std::string t = NombreTex(el.tex);
    if (p->propHornTex->button->text != t) { p->propHornTex->button->text = t; PropsLayoutSucio(); }
    const int uv = UVElegida(m);
    std::string tu = (uv >= 0 && uv < (int)m->uvMaps.size() && m->uvMaps[(size_t)uv]) ? m->uvMaps[(size_t)uv]->nombre : std::string("UV");
    if (p->propHornUV->button->text != tu) p->propHornUV->button->text = tu;
    p->propHornIndirecta->value = trabajando ? NULL : &gHornIndirecta;
    p->propHornMuestras->value = (trabajando || !gHornIndirecta) ? NULL : &gHornMuestras;
    p->propHornRebotes->value = (trabajando || !gHornIndirecta) ? NULL : &gHornRebotes;
    p->propHornDenoiser->value = (trabajando || !gHornIndirecta) ? NULL : &gHornDenoiser;
    // MIENTRAS HORNEA: sin la textura ni la capa UV (no se cambian a mitad de camino) ni el boton Bake; la barra y abajo
    // Cancelar
    {
        bool cambio = false;
        if (p->propHornTex->oculto != trabajando) { p->propHornTex->oculto = trabajando; cambio = true; }
        if (p->propHornUV->oculto != trabajando) { p->propHornUV->oculto = trabajando; cambio = true; }
        if (p->propHornBoton->oculto != trabajando) { p->propHornBoton->oculto = trabajando; cambio = true; }
        if (p->propHornBarra->oculto != !trabajando) { p->propHornBarra->oculto = !trabajando; cambio = true; }
        if (p->propHornCancelar && p->propHornCancelar->oculto != !trabajando) { p->propHornCancelar->oculto = !trabajando; cambio = true; }
        if (cambio) PropsLayoutSucio();
    }
    if (trabajando) {
        char b[32]; snprintf(b, sizeof(b), "%d%%", (int)(gHornProgreso * 100.0f));
        p->propHornBarra->button->text = b;
        p->propHornBarra->button->barra = gHornProgreso;
    }
    p->propHornAviso->oculto = trabajando;
}
