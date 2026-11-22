// ============================================================================
//  PropsBake.cpp — la tarjeta "Bake Lighting" (precalcular la iluminacion) de la pestania Material, debajo de la
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
#include "ViewPorts/PopUp/NewTexturePopup.h"
#include "WhiskUI/widgets/PopupMenu.h"   // los desplegables de la textura y de la capa UV
#include "objects/Mesh.h"
#include "objects/Textures.h"
#include "io/EditedTexture.h"
#include "render/RayTracer.h"
#include "undo/Undo.h"
#include "W3dLang.h"
#include <stdio.h>
#include <map>

extern void Notify(const std::string& text, bool error);
extern void PropsLayoutDirty();
extern void RebindMaterialMeshPart();
extern void (*g_newCreatedTextureHook)(const std::string& path);   // (NewTexturePopup.cpp)
void PropsOpenMenuUnderButton(PopupMenu* menu, Button* button);    // (Properties.cpp: el estandar de los desplegables)

// LA TEXTURA Y LA CAPA UV DE CADA MALLA (por su serial): cada objeto hornea en la SUYA. Antes eran una sola para todos y,
// al pasar a otro objeto, seguia elegida la del anterior: el escenario se horneaba sin querer en la textura del cubo (y sus
// materiales quedaban con las dos capas de luz). La primera vez arranca con la de su luz ya horneada, si tiene
struct PickedBake { Texture* tex; int uv; bool ready; PickedBake() : tex(NULL), uv(-1), ready(false) {} };
static std::map<unsigned int, PickedBake> gBakePerMesh;
static Mesh* ActiveMesh();
static PickedBake& PickedOf(Mesh* m) {
    static PickedBake nothing;
    if (!m) { nothing = PickedBake(); return nothing; }
    PickedBake& e = gBakePerMesh[m->serial];
    if (!e.ready) {
        e.ready = true;
        for (size_t g = 0; g < m->materialsGroup.size() && !e.tex; g++) {
            Material* mat = m->materialsGroup[g].material;
            if (!mat || mat == MaterialDefault) continue;
            for (size_t k = 0; k < mat->layers.size(); k++)
                if (mat->layers[k].baked && mat->layers[k].tex) { e.tex = mat->layers[k].tex; e.uv = mat->layers[k].uvMap; break; }
        }
    }
    return e;
}
static float    gBakeSamples = 32.0f;
static bool     gBakeIndirect = true;   // la luz que rebota (el fondo y lo de alrededor)
static float    gBakeBounces = 3.0f;    // cuantas veces rebota (con mas, la sombra recibe la luz que rebota en lo que la tapa)
static bool     gBakeDenoiser = true;   // la indirecta suavizada (sin granulado)
static Mesh*    gBakeMesh = NULL;   // la malla que se esta horneando
static TextureEditable* gBakeTe = NULL;
static int      gBakeUsedUV = 0;
static float    gBakeProgress = 0.0f;
static PopupMenu* gMenuBakeTex = NULL;
static PopupMenu* gMenuBakeUV = NULL;

static Mesh* ActiveMesh() {
    return (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
}
static bool TextureLive(const Texture* t) {
    if (!t) return false;
    for (size_t i = 0; i < Textures.size(); i++) if (Textures[i] == t) return true;
    return false;
}
static std::string TexName(const Texture* t) {
    if (!t) return std::string(T("Choose a texture"));
    std::string n = t->path;
    const size_t b = n.find_last_of("/\\"); if (b != std::string::npos) n = n.substr(b + 1);
    return n;
}

// ---- la textura ----
static void ActionPickedTex(int id) {
    if (id == 1) {   // "New Texture...": el popup de siempre; al crearla queda elegida
        g_newCreatedTextureHook = NULL;
        struct H { static void Created(const std::string& path) { Texture* t = TextureFind(path); if (t && ActiveMesh()) PickedOf(ActiveMesh()).tex = t; g_redraw = true; } };
        g_newCreatedTextureHook = H::Created;
        OpenNewTexturePopup();
        return;
    }
    const size_t i = (size_t)(id - 2) + 5;
    if (id >= 2 && i < Textures.size() && ActiveMesh()) PickedOf(ActiveMesh()).tex = Textures[i];
    g_redraw = true;
}
static void ActionMenuTex() {
    if (!PropsActive || !PropsActive->propBakeTex) return;
    if (!gMenuBakeTex) { gMenuBakeTex = new PopupMenu(); gMenuBakeTex->action = ActionPickedTex; }
    gMenuBakeTex->Clear();
    gMenuBakeTex->Add(T("New Texture..."), 1, IconType::plus);
    for (size_t i = 5; i < Textures.size(); i++)
        if (Textures[i]) gMenuBakeTex->Add(TexName(Textures[i]), 2 + (int)(i - 5), IconType::texture);
    PropsOpenMenuUnderButton(gMenuBakeTex, PropsActive->propBakeTex->button);
}
// ---- la capa UV ----
static void ActionPickedUV(int id) { if (ActiveMesh()) PickedOf(ActiveMesh()).uv = id; g_redraw = true; }
static void ActionMenuUV() {
    Mesh* m = ActiveMesh();
    if (!m || !PropsActive || !PropsActive->propBakeUV) return;
    if (!gMenuBakeUV) { gMenuBakeUV = new PopupMenu(); gMenuBakeUV->action = ActionPickedUV; }
    gMenuBakeUV->Clear();
    if (m->uvMaps.empty()) gMenuBakeUV->Add("UV", 0, IconType::material);
    for (size_t u = 0; u < m->uvMaps.size(); u++)
        gMenuBakeUV->Add(m->uvMaps[u] ? m->uvMaps[u]->name : std::string("UV"), (int)u, IconType::material);
    PropsOpenMenuUnderButton(gMenuBakeUV, PropsActive->propBakeUV->button);
}
static int UVPicked(const Mesh* m) {
    if (!m) return 0;
    const int uv = PickedOf(const_cast<Mesh*>(m)).uv;
    if (uv >= 0 && uv < (int)m->uvMaps.size()) return uv;
    return m->uvActiveMap >= 0 ? m->uvActiveMap : 0;
}

// ---- hornear ----
// otra malla de la escena (que no es 'm') usa 'mat' en alguna de sus partes
static bool OtherMeshMaterial(Object* o, Material* mat, Mesh* m) {
    if (!o) return false;
    if (o->getType() == ObjectType::mesh && o != (Object*)m) {
        Mesh* me = (Mesh*)o;
        for (size_t g = 0; g < me->materialsGroup.size(); g++) if (me->materialsGroup[g].material == mat) return true;
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) if (OtherMeshMaterial(o->Childrens[i], mat, m)) return true;
    return false;
}
static void Finish() {
    Mesh* m = gBakeMesh;
    TextureEditable* te = gBakeTe;
    gBakeMesh = NULL; gBakeTe = NULL;
    if (!m || !te || !TextureLive(te->tex)) return;
    const std::vector<float>& light = RTBakeLight();
    const std::vector<unsigned char>& cub = RTCoveredBake();
    const int w = te->w, h = te->h;
    if ((int)cub.size() != w * h || (int)te->rgba.size() < w * h * 4) return;
    // los texels de las caras + DOS de margen (el promedio de los vecinos cubiertos): el filtro bilineal lee la orilla
    std::vector<float> L(light);
    std::vector<unsigned char> c(cub);
    // LA MASCARA DEL SOL (al alfa): que fraccion de la luz de cada texel es la directa del sol (RTBakeSun)
    std::vector<float> S(RTBakeSun());
    const bool withSun = (int)S.size() == w * h;
    if (!withSun) S.assign((size_t)w * h, 0.0f);
    for (int pass = 0; pass < 2; pass++) {
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
    const bool picker = RTBakePicker();
    const int mode = picker ? W3D_LAYER_MULTIPLY : W3D_LAYER_MULTIPLY2X;
    UndoTextureStart(te);
    for (int i = 0; i < w * h; i++) {
        if (!c[(size_t)i]) continue;
        for (int k = 0; k < 3; k++) {   // (la mitad: la capa es Multiply 2x, asi entra luz de hasta el doble de blanco)
            const float v = L[(size_t)i * 3 + k] * (picker ? 1.0f : 0.5f);
            te->rgba[(size_t)i * 4 + k] = (unsigned char)(v <= 0.0f ? 0 : v >= 1.0f ? 255 : (int)(v * 255.0f + 0.5f));
        }
        // (el ALFA: la mascara del sol. La capa no lo usa como transparencia: ver TexLayer::maskSun)
        const float sf = S[(size_t)i];
        te->rgba[(size_t)i * 4 + 3] = withSun ? (unsigned char)(sf <= 0.0f ? 0 : sf >= 1.0f ? 255 : (int)(sf * 255.0f + 0.5f)) : 255;
    }
    if (withSun) te->alpha = true;   // (el PNG guarda el alfa: la mascara del sol)
    te->modified = true;
    // (una textura del proyecto viaja con el proximo guardado del .w3d: la luz horneada -y su mascara- no se pierde por
    //  no haber guardado la textura aparte. Una externa se guarda con su boton, como siempre)
    if (!TexEditIsExternal(te->tex)) te->inMemory = true;
    TexEditUpload(te, 0, 0, w, h);
    UndoTextureConfirm(true);
    // UN MATERIAL QUE USAN OTRAS MALLAS: la luz horneada es de ESTA malla (cae por su UV). Puesta en un material compartido,
    // todas las que lo usan la mostraban con sus UV: la luz de otro objeto (el plano nuevo con el material "Default" se
    // veia con la luz del cubo, como una sombra que no era). Esta malla pasa a una COPIA suya del material (con Ctrl+Z)
    UndoGroupStart();
    {
        std::map<Material*, Material*> copies;
        std::vector<std::string> copied;
        for (size_t g = 0; g < m->materialsGroup.size(); g++) {
            Material* mat = m->materialsGroup[g].material;
            if (!mat || mat == MaterialDefault || !OtherMeshMaterial(SceneCollection, mat, m)) continue;
            Material*& c = copies[mat];
            if (!c) {
                extern Material* W3dMaterialCopyWithUndo(const Material*, const std::string&);
                c = W3dMaterialCopyWithUndo(mat, mat->name + " " + m->name);
                // (la copia no se queda con la luz que el compartido tenia de otra malla)
                for (size_t k = c->layers.size(); k-- > 0; )
                    if (c->layers[k].baked) { TextureSetIn(c->layers[k].tex, NULL); c->layers.erase(c->layers.begin() + (long)k); }
                // ...y el compartido suelta la luz de ESTA textura: se la habia dejado un horneado de esta malla (antes de
                // esta copia) y las otras la mostraban con sus UV
                bool has = false;
                for (size_t k = 0; k < mat->layers.size() && !has; k++) has = mat->layers[k].baked && mat->layers[k].tex == te->tex;
                if (has) {
                    UndoMaterialModStart(mat);
                    for (size_t k = mat->layers.size(); k-- > 0; )
                        if (mat->layers[k].baked && mat->layers[k].tex == te->tex) { TextureSetIn(mat->layers[k].tex, NULL); mat->layers.erase(mat->layers.begin() + (long)k); }
                    UndoMaterialModCommit();
                }
                copied.push_back(mat->name);
            }
            UndoCaptureMaterial(m, (int)g);
            m->materialsGroup[g].material = c;
        }
        if (!copied.empty()) {
            std::string l;
            for (size_t i = 0; i < copied.size(); i++) l += (i ? ", " : "") + copied[i];
            Notify(std::string(T("The material is shared with other objects: this one got its own copy")) + " (" + l + ")", false);
        }
    }
    // LA TEXTURA COMO CAPA de los materiales de la malla, y sin la luz de tiempo real
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        Material* mat = m->materialsGroup[g].material;
        if (!mat || mat == MaterialDefault) continue;
        UndoMaterialModStart(mat);
        // (la luz NUEVA reemplaza a la que tenia horneada en otra textura: si no, se multiplicaban las dos)
        for (size_t k = mat->layers.size(); k-- > 0; )
            if (mat->layers[k].baked && mat->layers[k].tex != te->tex) {
                TextureSetIn(mat->layers[k].tex, NULL);   // (suelta su referencia a la textura)
                mat->layers.erase(mat->layers.begin() + (long)k);
            }
        bool has = false;
        for (size_t k = 0; k < mat->layers.size(); k++)
            if (mat->layers[k].tex == te->tex) { mat->layers[k].blend = mode; mat->layers[k].uvMap = gBakeUsedUV;
                                                mat->layers[k].on = true; mat->layers[k].baked = true;
                                                mat->layers[k].maskSun = withSun; has = true; }
        if (!has) {
            TexLayer tl; tl.blend = mode; tl.uvMap = gBakeUsedUV; tl.force = 1.0f; tl.baked = true; tl.maskSun = withSun;
            TextureSetIn(tl.tex, te->tex);
            mat->layers.push_back(tl);
        }
        // (la capa es de LUZ HORNEADA: mientras este prendida el material va sin la luz de tiempo real. Sacarla la devuelve)
        UndoMaterialModCommit();
    }
    UndoGroupEnd();
    RebindMaterialMeshPart();
    RTInvalidate();
    Notify(T("Lighting baked"), false);
    g_redraw = true;
}
static void ActionCancel() {
    if (!RTBakeActive()) return;
    RTBakeCancel(); gBakeMesh = NULL; gBakeTe = NULL; Notify(T("Bake canceled"), true); PropsLayoutDirty(); g_redraw = true;
}
static void ActionBake() {
    if (RTBakeActive()) { ActionCancel(); return; }
    Mesh* m = ActiveMesh();
    if (!m) return;
    Texture* tex = PickedOf(m).tex;
    if (!TextureLive(tex)) { Notify(T("Choose a texture to bake into"), true); return; }
    TextureEditable* te = TexEditGet(tex);
    if (!te || te->w <= 0 || te->h <= 0) { Notify(T("Could not read the image"), true); return; }
    const int uv = UVPicked(m);
    std::string reason;
    if (!RTBakeBegin(m, uv, te->w, te->h, (int)(gBakeSamples + 0.5f), &reason, gBakeIndirect, (int)(gBakeBounces + 0.5f), gBakeDenoiser)) {
        Notify(std::string(T("Not baked")) + ": " + T(reason.c_str()), true);
        return;
    }
    gBakeMesh = m; gBakeTe = te; gBakeUsedUV = uv; gBakeProgress = 0.0f;
    g_redraw = true;
}

// DE UNA SOLA VEZ (el harness): sin la barra, hasta terminar
bool PropsBakeNow(Mesh* m, Texture* t, int uv, int samples, std::string* reason) {
    if (!m || !TextureLive(t)) { if (reason) *reason = "Choose a texture to bake into"; return false; }
    TextureEditable* te = TexEditGet(t);
    if (!te || te->w <= 0 || te->h <= 0) { if (reason) *reason = "Could not read the image"; return false; }
    if (uv < 0) uv = UVPicked(m);
    if (!RTBakeBegin(m, uv, te->w, te->h, samples, reason, gBakeIndirect, (int)(gBakeBounces + 0.5f), gBakeDenoiser)) return false;
    gBakeMesh = m; gBakeTe = te; gBakeUsedUV = uv;
    while (RTBakeActive()) RTBakeStep(1000);
    Finish();
    return true;
}

void PropsBakeBuild(Properties* p) {
    if (!p) return;
    p->propBake = new GroupPropertie(T("Bake Lighting"));
    p->propBakeTex = new PropButton(T("Texture"), IconType::texture);
    p->propBakeTex->withLabel = true;
    p->propBakeTex->button->dropdown = true;
    p->propBakeTex->action = ActionMenuTex;
    p->propBake->properties.push_back(p->propBakeTex);
    p->propBakeNoticeTex = new PropLabel("", true);
    p->propBakeNoticeTex->hidden = true;
    p->propBake->properties.push_back(p->propBakeNoticeTex);
    p->propBakeUV = new PropButton(T("UV"), IconType::material);
    p->propBakeUV->withLabel = true;
    p->propBakeUV->button->dropdown = true;
    p->propBakeUV->action = ActionMenuUV;
    p->propBake->properties.push_back(p->propBakeUV);
    p->propBakeSamples = new PropFloat(T("Samples"));
    p->propBakeSamples->SetRange(1.0f, 1024.0f);
    p->propBakeSamples->integer = true;
    p->propBakeSamples->fineStep = 8.0f; p->propBakeSamples->thickStep = 32.0f; p->propBakeSamples->dragStep = 0.5f;
    p->propBakeIndirect = new PropBool(T("Indirect light"));
    p->propBake->properties.push_back(p->propBakeIndirect);
    p->propBake->properties.push_back(p->propBakeSamples);
    p->propBakeBounces = new PropFloat(T("Bounces"));
    p->propBakeBounces->SetRange(1.0f, 8.0f);
    p->propBakeBounces->integer = true;
    p->propBakeBounces->fineStep = 1.0f; p->propBakeBounces->thickStep = 1.0f; p->propBakeBounces->dragStep = 0.05f;
    p->propBake->properties.push_back(p->propBakeBounces);
    p->propBakeDenoiser = new PropBool(T("Denoiser"));
    p->propBake->properties.push_back(p->propBakeDenoiser);
    p->propBakeButton = new PropButton(T("Bake"), IconType::sphere);
    p->propBakeButton->action = ActionBake;
    p->propBake->properties.push_back(p->propBakeButton);
    p->propBakeBar = new PropButton("0%");   // (la barra del progreso, solo mientras hornea)
    p->propBakeBar->button->bar = 0.0f;
    p->propBake->properties.push_back(p->propBakeBar);
    // CANCELAR (la cruz), abajo de la barra: solo mientras hornea (el boton Bake se esconde)
    p->propBakeCancel = new PropButton(T("Cancel"), IconType::close);
    p->propBakeCancel->action = ActionCancel;
    p->propBakeCancel->hidden = true;
    p->propBake->properties.push_back(p->propBakeCancel);
    p->propBakeNotice = new PropLabel(T("Bakes the light of the static objects into the texture, through the UV of the mesh"), true);
    p->propBake->properties.push_back(p->propBakeNotice);
    p->GroupProperties.push_back(p->propBake);
}

// las OTRAS mallas de la escena que ya tienen la luz horneada en 't' (una capa de luz de sus materiales): hornear 'm' en la
// misma textura pisa lo que se cruce con sus UV
static void MeshesWithLightIn(Object* o, Mesh* m, Texture* t, std::vector<std::string>& out) {
    if (!o || out.size() >= 4) return;
    if (o->getType() == ObjectType::mesh && o != (Object*)m) {
        Mesh* me  = (Mesh*)o;
        bool uses = false;
        for (size_t g = 0; g < me->materialsGroup.size() && !uses; g++) {
            Material* mat = me->materialsGroup[g].material;
            if (!mat) continue;
            for (size_t k = 0; k < mat->layers.size() && !uses; k++) uses = mat->layers[k].baked && mat->layers[k].tex == t;
        }
        if (uses) out.push_back(o->name);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) MeshesWithLightIn(o->Childrens[i], m, t, out);
}
void PropsBakeUpdate(Properties* p, bool visible) {
    if (!p || !p->propBake) return;
    // EL TRABAJO avanza una vez por cuadro (aunque haya varios paneles): ~25 ms por cuadro, el editor sigue andando
    static unsigned int last = 0;
    extern unsigned int w3dGetTicks();
    if (RTBakeActive() && w3dGetTicks() - last >= 10) {
        last = w3dGetTicks();
        gBakeProgress = RTBakeStep(25);
        if (!RTBakeActive()) Finish();
        g_redraw = true;
    }
    Mesh* m = visible ? ActiveMesh() : NULL;
    p->propBake->visible = (m != NULL);
    if (!m) return;
    PickedBake& el = PickedOf(m);
    if (!TextureLive(el.tex)) el.tex = NULL;
    const bool working = RTBakeActive();
    // EL AVISO: esa textura ya tiene la luz de otro objeto
    if (p->propBakeNoticeTex) {
        std::string av;
        if (el.tex && !working) {
            std::vector<std::string> other;
            MeshesWithLightIn(SceneCollection, m, el.tex, other);
            for (size_t i = 0; i < other.size(); i++) av += (i ? ", " : "") + other[i];
            if (!av.empty()) av = std::string(T("This texture already has the baked light of")) + ": " + av + ". " +
                                  T("Baking here overwrites where their UVs overlap");
        }
        if (p->propBakeNoticeTex->name != av || p->propBakeNoticeTex->hidden != av.empty()) {
            p->propBakeNoticeTex->name = av; p->propBakeNoticeTex->hidden = av.empty(); PropsLayoutDirty();
        }
    }
    std::string t = TexName(el.tex);
    if (p->propBakeTex->button->text != t) { p->propBakeTex->button->text = t; PropsLayoutDirty(); }
    const int uv = UVPicked(m);
    std::string tu = (uv >= 0 && uv < (int)m->uvMaps.size() && m->uvMaps[(size_t)uv]) ? m->uvMaps[(size_t)uv]->name : std::string("UV");
    if (p->propBakeUV->button->text != tu) p->propBakeUV->button->text = tu;
    p->propBakeIndirect->value = working ? NULL : &gBakeIndirect;
    p->propBakeSamples->value = (working || !gBakeIndirect) ? NULL : &gBakeSamples;
    p->propBakeBounces->value = (working || !gBakeIndirect) ? NULL : &gBakeBounces;
    p->propBakeDenoiser->value = (working || !gBakeIndirect) ? NULL : &gBakeDenoiser;
    // MIENTRAS HORNEA: sin la textura ni la capa UV (no se cambian a mitad de camino) ni el boton Bake; la barra y abajo
    // Cancelar
    {
        bool change = false;
        if (p->propBakeTex->hidden != working) { p->propBakeTex->hidden = working; change = true; }
        if (p->propBakeUV->hidden != working) { p->propBakeUV->hidden = working; change = true; }
        if (p->propBakeButton->hidden != working) { p->propBakeButton->hidden = working; change = true; }
        if (p->propBakeBar->hidden != !working) { p->propBakeBar->hidden = !working; change = true; }
        if (p->propBakeCancel && p->propBakeCancel->hidden != !working) { p->propBakeCancel->hidden = !working; change = true; }
        if (change) PropsLayoutDirty();
    }
    if (working) {
        char b[32]; snprintf(b, sizeof(b), "%d%%", (int)(gBakeProgress * 100.0f));
        p->propBakeBar->button->text = b;
        p->propBakeBar->button->bar = gBakeProgress;
    }
    p->propBakeNotice->hidden = working;
}
