// ============================================================================
//  W3dTestsCalls.cpp — comandos de harness de MENOS LLAMADAS GL POR CUADRO (graphics/w3dGraphics.h): la pila
//  PEREZOSA de la modelview (g_matricesLazy) y el CACHE DE PUNTEROS y binds de buffers (g_pointersCache). Los
//  despacha W3dTestsResourcesCmd (test/W3dTestsResources.cpp). La prueba: tools/pruebas/prueba_menos_llamadas.w3s.
//
//  Comandos:
//    perezosas on|off | perezosas es 0|1        la pila perezosa / su assert
//    punteroscache on|off | punteroscache es 0|1  el cache de punteros y binds / su assert
//    menosllamadas on|off                        las dos juntas (el A/B: off = como antes)
//    llamadasarmar
//        agrega a la escena (escenadefecto) lo que ejercita los dos caches: jerarquias de vacios con transform,
//        mallas con VBO y de RAM (reflejo matcap y equirect, normal map, capa, generada), mezcla de texturas,
//        lineas de material, una calcomania, mallas afuera de la camara (autocull), dos esqueletos con su malla
//        skinneada (uno se posa entre cuadros: "animando"), una luz puntual y una spot con padres rotados, un
//        espejo con recorte, un Recorte con su camara, un Culling, particulas (billboard y lineas) y una UI 2D
//        con un rectangulo rotado
//    llamadaspose <rig> <grados>
//        posa a mano el segundo hueso del esqueleto (la pose cambia: el proximo cuadro la malla va "animando")
//    llamadascuadro <clave> [vp]
//        UN cuadro y lo anota con la clave: el hash de los pixeles y las llamadas GL (total, por categoria,
//        draws, binds, estados, otras). Sin 'vp' el cuadro COMPLETO (escena + HUD + editor, toda la ventana);
//        con 'vp' solo el pase del viewport 3D activo (sus pixeles)
//    llamadasigual <a> <b>                       los dos cuadros tienen la MISMA imagen
//    llamadasdistinto <a> <b>                    (el control) dos cuadros con imagen distinta
//    llamadasbajan <antes> <despues> <minimo>    'despues' hace al menos <minimo> llamadas GL menos, con los
//                                                mismos draws
// ============================================================================
#include "test/W3dTestsResources.h"
#include "graphics/w3dTexture.h"     // SavePNG (W3D_FRAME_PNG)
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "objects/Materials.h"
#include "objects/Light.h"
#include "objects/Collection.h"
#include "objects/Mirror.h"
#include "objects/ClipRegion.h"
#include "objects/Culling.h"
#include "objects/Camera.h"
#include "objects/Particles.h"
#include "WhiskUI/objects/UI.h"
#include "WhiskUI/objects/Rect2D.h"
#include "edit/Modifier.h"
#include "edit/WeightPaint.h"            // WeightPaintEnsureMap
#include "animation/SkeletalAnimation.h" // EvaluatePoseSkeleton / PrepareAuthoredSkin
#include "animation/Animation.h"         // CurrentFrame
#include "io/EditedTexture.h"            // TexEditCreate: una textura en memoria
#include "graphics/w3dGraphics.h"
#include "ViewPorts/ViewPorts.h"         // rootViewport
#include "ViewPorts/ViewPort3D.h"        // Viewport3D
#include "ViewPorts/LayoutInput.h"       // LayoutRenderMenu
#include "WhiskUI/draw/glesdraw.h"       // W3dScreenHeight
#include <map>
#include <vector>
#include <stdio.h>
#include <string.h>

extern Viewport3D* Viewport3DActive;
extern void LoadPendingTextures();
extern int MenuScreenW, MenuScreenH;
void W3dParticlesTick(float dt);

namespace {

namespace gfx = w3dEngine;

unsigned Hash(const unsigned char* p, size_t n) {   // FNV-1a de 32 bits
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

// ---- las opciones ----
bool CmdOption(const std::string& cmd, std::istringstream& ss, std::string& err) {
    std::string q; ss >> q;
    bool* v = cmd == "lazy" ? &gfx::g_matricesLazy : &gfx::g_pointersCache;
    if (q == "es") {
        int e = -1; ss >> e;
        printf("      [%s] %s\n", cmd.c_str(), *v ? "on" : "off");
        if ((*v ? 1 : 0) != e) { err = cmd + " es: la opcion esta " + (*v ? "on" : "off"); return false; }
        return true;
    }
    if (q != "on" && q != "off") { err = cmd + ": uso: " + cmd + " on|off|es 0|1"; return false; }
    const bool on = (q == "on");
    if (cmd == "lazy" || cmd == "fewercalls") {
        if (!on) gfx::MatricesSync();   // (lo que quedo perezoso, a GL)
        gfx::g_matricesLazy = on;
    }
    if (cmd == "pointerscache" || cmd == "fewercalls") {
        gfx::g_pointersCache = on;
        if (!on) gfx::UnbindVBOs();             // (apagado de verdad: el camino viejo supone 0 entre mallas)
    }
    printf("      [%s] perezosas=%d punteroscache=%d\n", cmd.c_str(), gfx::g_matricesLazy ? 1 : 0, gfx::g_pointersCache ? 1 : 0);
    return true;
}

// ---- la escena ----
Mesh* MakeMesh(MeshType::Enum t, Object* parent, const char* name, const Vector3& pos, const Vector3& rot, float esc) {
    Mesh* m = (Mesh*)NewMesh(MeshType(t), parent, false);
    m->SetNameObj(name);
    m->pos = pos;
    m->SetRotEuler(rot);
    m->scale = Vector3(esc, esc, esc);
    return m;
}
Empty* IsEmpty(Object* parent, const char* name, const Vector3& pos, const Vector3& rot, const Vector3& esc) {
    Empty* e = new Empty(parent, pos);
    e->SetNameObj(name);
    e->SetRotEuler(rot);
    e->scale = esc;
    return e;
}
Material* Mat(const char* name, float r, float g, float b) {
    Material* mt = new Material(name);
    mt->diffuse[0] = r; mt->diffuse[1] = g; mt->diffuse[2] = b;
    mt->ambient[0] = r * 0.3f; mt->ambient[1] = g * 0.3f; mt->ambient[2] = b * 0.3f;
    return mt;
}
void Paint(Mesh* m, Material* mt) {
    for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = mt;
}
// un esqueleto de dos huesos y una malla (cilindro) skinneada con el: la mitad de arriba sigue al segundo hueso
Armature* Character(Object* parent, const char* rig, const char* mesh, const Vector3& pos, Material* mt) {
    Armature* a = new Armature(parent, pos);
    a->name = rig;
    { W3dBone r; r.name = "raiz";  r.parent = -1; r.head = Vector3(0, 0, 0); r.tail = Vector3(0, 1, 0); a->bones.push_back(r); }
    { W3dBone c; c.name = "brazo"; c.parent = 0;  c.head = Vector3(0, 1, 0); c.tail = Vector3(0, 2, 0); a->bones.push_back(c); }
    PrepareAuthoredSkin(a);
    Mesh* s = (Mesh*)NewMesh(MeshType::cylinder, parent, false);
    s->SetNameObj(mesh);
    s->pos = pos;
    s->scale = Vector3(0.4f, 0.8f, 0.4f);
    s->GenerateRender();
    WeightPaintEnsureMap(s);
    VertexGroup* vr = new VertexGroup("raiz");
    VertexGroup* vb = new VertexGroup("brazo");
    std::vector<char> done((size_t)s->vertexSize, 0);
    for (int i = 0; i < s->vertexSize; i++) {
        const int cp = s->vertCtrlPoint[i];
        if (done[(size_t)cp]) continue;
        done[(size_t)cp] = 1;
        if (s->vertex[i * 3 + 1] > 0.0f) { vb->verts.push_back(cp); vb->weights.push_back(1.0f); }
        else { vr->verts.push_back(cp); vr->weights.push_back(0.7f); }
    }
    s->vertexGroups.push_back(vr); s->vertexGroups.push_back(vb);
    Modifier* md = new Modifier(ModifierType::Armature, ModifierTypeName(ModifierType::Armature));
    md->target = (Object*)a; s->modifiers.push_back(md);
    s->skinArmature = a;
    s->lastSkinFrame = -999999;
    Paint(s, mt);
    return a;
}
void Pose(Armature* a, float degrees) {
    EvaluatePoseSkeleton(a, CurrentFrame);
    a->bones[1].poseR = Vector3(0, 0, degrees);
    a->poseDirty = true;
    EvaluatePoseSkeleton(a, CurrentFrame);
}

bool CmdBuild(std::string& err) {
    if (!SceneCollection) { err = "callsbuild: no hay escena"; return false; }
    Object* root = CollectionActive ? (Object*)CollectionActive : (Object*)SceneCollection;
    // una textura en memoria (tablero) y una segunda (franjas): las del reflejo, el normal map, la capa y la mezcla
    std::vector<unsigned char> px(64 * 64 * 4), px2(64 * 64 * 4);
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            unsigned char* p = &px[(size_t)(y * 64 + x) * 4];
            const bool c = ((x / 8) + (y / 8)) & 1;
            p[0] = c ? 230 : 40; p[1] = c ? 200 : 90; p[2] = c ? 60 : 160; p[3] = 255;
            unsigned char* q = &px2[(size_t)(y * 64 + x) * 4];
            q[0] = (unsigned char)(128 + 100 * ((y / 4) & 1)); q[1] = 128; q[2] = 255; q[3] = 255;
        }
    Texture* tex = TexEditCreate("llam_tablero", px, 64, 64, false);
    Texture* tex2 = TexEditCreate("llam_stripes", px2, 64, 64, false);
    if (!tex || !tex2) { err = "callsbuild: no se pudieron crear las texturas"; return false; }
    // la camara de la escena por defecto, mas atras (que entre todo), y su cubo chico y a un costado (la rutina
    // dibuja su malla)
    if (CameraActive) CameraActive->pos = CameraActive->pos * 1.7f;
    Object* cube = FindObjectByName(SceneCollection, "Cubo");
    if (cube) { cube->pos = Vector3(-0.2f, -0.6f, -1.4f); cube->scale = Vector3(0.25f, 0.25f, 0.25f); }

    // 1) JERARQUIA: vacios con traslacion, rotacion y escala, y mallas con VBO adentro (una afuera de la camara)
    Empty* j0 = IsEmpty(root, "LlamJer", Vector3(0.8f, 0.0f, -0.6f), Vector3(0, 25, 0), Vector3(1.0f, 1.0f, 1.0f));
    Empty* j1 = IsEmpty(j0, "LlamJer.Brazo", Vector3(0.0f, 0.6f, 0.0f), Vector3(15, 0, 30), Vector3(0.8f, 0.8f, 0.8f));
    Material* red = Mat("LlamRojo", 0.9f, 0.2f, 0.2f);
    Material* green = Mat("LlamVerde", 0.2f, 0.8f, 0.3f);
    Paint(MakeMesh(MeshType::cube, j0, "LlamCuboPadre", Vector3(0, 0, 0), Vector3(0, 0, 0), 0.3f), red);
    Paint(MakeMesh(MeshType::UVsphere, j1, "LlamEsferaHija", Vector3(0.4f, 0.0f, 0.0f), Vector3(0, 45, 0), 0.25f), green);
    Paint(MakeMesh(MeshType::cube, j1, "LlamCuboFuera", Vector3(0.0f, 0.0f, 90.0f), Vector3(0, 0, 0), 0.5f), green);   // autocull
    Paint(MakeMesh(MeshType::cube, root, "LlamCuboLejos", Vector3(-80.0f, 10.0f, 60.0f), Vector3(0, 0, 0), 1.0f), red);

    // 2) MATERIALES que van de RAM (reflejo matcap / equirect, normal map, capa) y los que siguen en VBO con algo
    //    mas (mezcla de texturas en la unidad 1, lineas del material)
    Material* matcap = Mat("LlamMatcap", 1, 1, 1);
    matcap->texture = tex; matcap->chrome = true; matcap->reflectMode = 0;
    Material* equi = Mat("LlamEquirect", 1, 1, 1);
    equi->texture = tex; equi->chrome = true; equi->reflectMode = 2;
    Material* nmap = Mat("LlamNormalMap", 0.9f, 0.9f, 0.9f);
    nmap->texture = tex; nmap->normalMap = true; nmap->normalTexture = tex2;
    Material* layer = Mat("LlamCapa", 1, 1, 1);
    layer->texture = tex;
    { TexLayer c; c.tex = tex2; c.on = true; c.blend = 2; layer->layers.push_back(c); }
    Material* mix = Mat("LlamMezcla", 1, 1, 1);
    mix->texture = tex; mix->textureB = tex2; mix->mixB = 0.5f;
    Material* lines = Mat("LlamLineas", 0.3f, 0.6f, 1.0f);
    lines->lines = true; lines->thicknessLine = 2.0f;
    Empty* row = IsEmpty(root, "LlamFila", Vector3(-1.2f, 0.2f, 0.9f), Vector3(0, -20, 0), Vector3(1, 1, 1));
    Paint(MakeMesh(MeshType::UVsphere, row, "LlamMatcap", Vector3(0.0f, 0.0f, 0.0f), Vector3(0, 0, 0), 0.25f), matcap);
    Paint(MakeMesh(MeshType::UVsphere, row, "LlamEquirect", Vector3(0.6f, 0.0f, 0.0f), Vector3(0, 30, 0), 0.25f), equi);
    Paint(MakeMesh(MeshType::cube, row, "LlamNormalMap", Vector3(1.2f, 0.0f, 0.0f), Vector3(20, 0, 0), 0.22f), nmap);
    Paint(MakeMesh(MeshType::cube, row, "LlamCapa", Vector3(0.0f, 0.6f, 0.0f), Vector3(0, 40, 0), 0.22f), layer);
    Paint(MakeMesh(MeshType::cube, row, "LlamMezcla", Vector3(0.6f, 0.6f, 0.0f), Vector3(0, 0, 10), 0.22f), mix);
    Paint(MakeMesh(MeshType::cube, row, "LlamLineas", Vector3(1.2f, 0.6f, 0.0f), Vector3(10, 10, 0), 0.22f), lines);
    Paint(MakeMesh(MeshType::cube, row, "LlamGenerada", Vector3(0.0f, -0.6f, 0.0f), Vector3(0, 0, 0), 0.15f), green);   // (modadd en el .w3s)

    // 3) una CALCOMANIA (se dibuja en el pase diferido, con su matriz armada aparte)
    Material* decal = Mat("LlamDecal", 0.1f, 0.1f, 0.1f);
    decal->order_pass = 1; decal->depth_write = false; decal->transparent = true; decal->diffuse[3] = 0.6f;
    Paint(MakeMesh(MeshType::plane, root, "LlamSombra", Vector3(0.8f, -0.95f, -0.6f), Vector3(0, 0, 0), 0.5f), decal);

    // 4) dos ESQUELETOS con su malla skinneada, colgados de un vacio rotado: uno se posa entre cuadros
    Empty* pj = IsEmpty(root, "LlamPersonajes", Vector3(0.2f, -0.9f, 1.2f), Vector3(0, 35, 0), Vector3(1, 1, 1));
    Material* skin = Mat("LlamPiel", 0.9f, 0.7f, 0.5f);
    Armature* still = Character(pj, "LlamRigQuieto", "LlamPersonajeQuieto", Vector3(-0.5f, 0, 0), skin);
    Armature* animates = Character(pj, "LlamRigAnima", "LlamPersonajeAnima", Vector3(0.5f, 0, 0), skin);
    Pose(still, 25.0f);
    Pose(animates, 10.0f);

    // 5) LUCES: una puntual y una spot, con padres rotados (la posicion y la direccion se llevan con la modelview)
    Empty* lp = IsEmpty(root, "LlamLuzPadre", Vector3(0.5f, 1.5f, 0.5f), Vector3(30, 60, 0), Vector3(1, 1, 1));
    Light* point = Light::Create(lp, 0.5f, 0.0f, 0.0f);
    if (point) { point->SetNameObj("LlamPuntual"); point->SetDiffuse(1.0f, 0.6f, 0.4f); }
    Empty* sp = IsEmpty(root, "LlamSpotPadre", Vector3(-0.5f, 2.0f, -0.5f), Vector3(-70, 20, 10), Vector3(1, 1, 1));
    Light* spot = Light::Create(sp, 0.0f, 0.0f, 0.0f);
    if (spot) { spot->SetNameObj("LlamSpot"); spot->SetDiffuse(0.4f, 0.6f, 1.0f); spot->spotCutoff = 35.0f; spot->spotExponent = 4.0f; }

    // 6) un ESPEJO horizontal con recorte (estencil o planos de recorte) que refleja un cubo
    Material* water = Mat("LlamAgua", 0.2f, 0.3f, 0.8f);
    water->transparent = true; water->diffuse[3] = 0.5f;
    Paint(MakeMesh(MeshType::cube, root, "LlamEspejado", Vector3(-0.9f, -0.3f, -0.9f), Vector3(0, 30, 0), 0.2f), red);
    Mirror* mi = new Mirror(root, Vector3(0.0f, -1.2f, 0.0f), false, true, false);
    mi->SetNameObj("LlamEspejo");
    mi->SetTarget("LlamEspejado");
    mi->ReloadTarget(mi);
    mi->noDepth = true;
    mi->usesLimits = true; mi->limU0 = -1.6f; mi->limU1 = -0.2f; mi->limV0 = -1.6f; mi->limV1 = -0.2f;
    Paint(MakeMesh(MeshType::plane, root, "LlamLamina", Vector3(-0.9f, -0.6f, -0.9f), Vector3(0, 0, 0), 0.7f), water);

    // 7) un RECORTE con su propia camara (otra proyeccion y otro viewport en medio del recorrido) y un CULLING
    Camera* cr = new Camera(root, Vector3(0.0f, 0.5f, 3.0f), Vector3(-10.0f, 0.0f, 0.0f));
    cr->SetNameObj("LlamCamRecorte");
    ClipRegion* rc = new ClipRegion(root);
    rc->SetNameObj("LlamRecorte");
    rc->x = 0.32f; rc->y = 0.3f; rc->width = 0.3f; rc->height = 0.3f; rc->camera = "LlamCamRecorte";
    rc->background = true; rc->color[0] = 0.1f; rc->color[1] = 0.1f; rc->color[2] = 0.2f; rc->color[3] = 1.0f;
    Paint(MakeMesh(MeshType::UVsphere, rc, "LlamEnRecorte", Vector3(0, 0, 0), Vector3(0, 0, 0), 0.6f), green);
    Culling* cu = new Culling(root);
    cu->SetNameObj("LlamCulling");
    Paint(MakeMesh(MeshType::cube, cu, "LlamCullAdentro", Vector3(-0.4f, 0.9f, -0.2f), Vector3(0, 10, 0), 0.15f), red);
    Paint(MakeMesh(MeshType::cube, cu, "LlamCullAfuera", Vector3(0.0f, 0.0f, -70.0f), Vector3(0, 0, 0), 0.5f), red);

    // 8) PARTICULAS: billboards con textura (del disco, al lado de la prueba) y una estela de lineas
    Particles* pa = new Particles(root, Vector3(1.0f, 0.3f, 0.6f));
    pa->SetNameObj("LlamParticulas");
    pa->texture = "mezcla_a.png"; pa->count = 40.0f; pa->life = 3.0f; pa->size = 0.2f; pa->vel = 0.6f; pa->gravity = 0.0f;
    Particles* pl = new Particles(root, Vector3(-1.0f, 0.3f, 0.6f));
    pl->SetNameObj("LlamEstela");
    pl->shape = w3dEngine::ShapeLine; pl->count = 20.0f; pl->life = 3.0f; pl->vel = 0.8f; pl->gravity = 0.0f;
    for (int i = 0; i < 20; i++) W3dParticlesTick(1.0f / 30.0f);

    // 9) UI 2D con un rectangulo rotado (el HUD del juego)
    UI* ui = new UI(NULL, Vector3(0, 0, 0));
    ui->SetNameObj("LlamHUD");
    Rect2D* r1 = new Rect2D(ui, Vector3(0.0f, 0.0f, 0.0f));
    r1->SetNameObj("LlamFranja");
    r1->width = 0.3f; r1->height = 0.08f; r1->rot2d = 20.0f;
    r1->color[0] = 1.0f; r1->color[1] = 0.8f; r1->color[2] = 0.1f; r1->color[3] = 0.9f;

    // 10) vacios de los que una RUTINA toma la matriz (los pasos van en el .w3s)
    IsEmpty(root, "LlamAnclaRut", Vector3(-0.3f, 1.2f, 0.4f), Vector3(10, 50, 0), Vector3(0.25f, 0.25f, 0.25f));
    IsEmpty(root, "LlamAnclaRut2", Vector3(0.6f, 1.3f, -0.3f), Vector3(0, 0, 25), Vector3(0.2f, 0.3f, 0.2f));

    DeselectAll(); ActiveObj = NULL;
    printf("      [llamadasarmar] jerarquias, mallas VBO/RAM, esqueletos, luces, espejo, recorte, culling, particulas "
           "(%d + %d vivas) y UI 2D\n", (int)pa->sys.parts.size(), (int)pl->sys.parts.size());
    return true;
}

bool CmdPose(std::istringstream& ss, std::string& err) {
    std::string n; float g = 0.0f; ss >> n >> g;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || o->getType() != ObjectType::armature) { err = "callspose: no hay un esqueleto '" + n + "'"; return false; }
    Pose((Armature*)o, g);
    return true;
}

// ---- los cuadros ----
enum { CTotal = 0, CScene, CParticles, CUI, CEditor, CDraws, CBinds, CStates, COther, CN };
const char* const kNames[CN] = { "total", "scene", "particles", "ui", "editor", "draws", "binds", "states", "other" };
struct Frame { unsigned hash; long n[CN]; int w, h; };
std::map<std::string, Frame> gFrames;

bool CmdFrame(std::istringstream& ss, std::string& err) {
    std::string key, mode; ss >> key >> mode;
    if (key.empty()) { err = "callsframe: falta la clave"; return false; }
    if (!rootViewport) { err = "callsframe: no hay layout de viewports"; return false; }
    LoadPendingTextures();
    Frame c;
    std::vector<unsigned char> px;
    if (mode == "vp") {
        rootViewport->Render();   // (puebla Viewport3DActive)
        Viewport3D* vp = Viewport3DActive;
        if (!vp || vp->width < 1 || vp->height < 1) { err = "callsframe: no hay viewport 3D"; return false; }
        gfx::StatsFrame();
        const int other0 = gfx::g_otherStat;
        vp->Render();
        gfx::StatsFrame();
        c.n[COther] = gfx::g_otherStat - other0;
        gfx::Finish();
        c.w = vp->width; c.h = vp->height;
        px.assign((size_t)c.w * c.h * 4, 0);
        gfx::ReadPixelsRGBA(vp->x, W3dScreenHeight - vp->y - vp->height, c.w, c.h, &px[0]);
    } else {
        // el cuadro COMPLETO, como statsvivo: el arbol de viewports y los menus encima
        gfx::StatsFrame();
        const int other0 = gfx::g_otherStat;
        rootViewport->Render();
        LayoutRenderMenu(MenuScreenW, MenuScreenH);
        gfx::StatsFrame();
        c.n[COther] = gfx::g_otherStat - other0;
        gfx::Finish();
        c.w = MenuScreenW; c.h = W3dScreenHeight;
        if (c.w < 1 || c.h < 1) { err = "callsframe: pantalla sin tamano"; return false; }
        px.assign((size_t)c.w * c.h * 4, 0);
        gfx::ReadPixelsRGBA(0, 0, c.w, c.h, &px[0]);
    }
    const gfx::FrameStats& s = gfx::g_statFrame;
    c.n[CTotal] = s.total;
    c.n[CScene] = s.calls[gfx::StatCatScene]; c.n[CParticles] = s.calls[gfx::StatCatParticles];
    c.n[CUI] = s.calls[gfx::StatCatUI]; c.n[CEditor] = s.calls[gfx::StatCatEditor];
    c.n[CDraws] = s.draws; c.n[CBinds] = s.binds; c.n[CStates] = s.states;
    c.hash = Hash(&px[0], px.size());
    // (para ver la diferencia: W3D_FRAME_PNG=<carpeta> guarda cada cuadro como <clave>.png)
    if (getenv("W3D_FRAME_PNG")) gfx::SavePNG((std::string(getenv("W3D_FRAME_PNG")) + "/" + key + ".png").c_str(), &px[0], c.w, c.h, false);
    long distinct = 0;   // (que el cuadro dibujo algo: no comparar dos pantallas vacias)
    for (size_t i = 4; i < px.size(); i += 4)
        if (px[i] != px[0] || px[i + 1] != px[1] || px[i + 2] != px[2]) distinct++;
    gFrames[key] = c;
    printf("      [llamadascuadro] %s: %dx%d hash=%08x px_no_fondo=%ld", key.c_str(), c.w, c.h, c.hash, distinct);
    for (int i = 0; i < CN; i++) printf(" %s=%ld", kNames[i], c.n[i]);
    printf("\n");
    if (distinct == 0) { err = "callsframe: el cuadro salio vacio"; return false; }
    return true;
}

bool Find(const std::string& k, Frame& c, std::string& err, const char* cmd) {
    std::map<std::string, Frame>::iterator it = gFrames.find(k);
    if (it == gFrames.end()) { err = std::string(cmd) + ": no hay un cuadro '" + k + "'"; return false; }
    c = it->second;
    return true;
}

bool CmdEqual(std::istringstream& ss, std::string& err) {
    std::string k1, k2; ss >> k1 >> k2;
    Frame a, b;
    if (!Find(k1, a, err, "callsequal") || !Find(k2, b, err, "callsequal")) return false;
    if (a.w != b.w || a.h != b.h || a.hash != b.hash) {
        char t[200]; snprintf(t, sizeof t, "callsequal: la imagen de '%s' (%08x) no es la de '%s' (%08x)", k1.c_str(), a.hash, k2.c_str(), b.hash);
        err = t; return false;
    }
    printf("      [llamadasigual] '%s' y '%s': la misma imagen (%08x)\n", k1.c_str(), k2.c_str(), a.hash);
    return true;
}

// (el control: lo que cambio entre dos cuadros -una pose- se ve)
bool CmdDistinct(std::istringstream& ss, std::string& err) {
    std::string k1, k2; ss >> k1 >> k2;
    Frame a, b;
    if (!Find(k1, a, err, "callsdistinct") || !Find(k2, b, err, "callsdistinct")) return false;
    if (a.w == b.w && a.h == b.h && a.hash == b.hash) { err = "callsdistinct: '" + k1 + "' y '" + k2 + "' son la misma imagen"; return false; }
    printf("      [llamadasdistinto] '%s' (%08x) y '%s' (%08x) son distintas\n", k1.c_str(), a.hash, k2.c_str(), b.hash);
    return true;
}

bool CmdLower(std::istringstream& ss, std::string& err) {
    std::string k1, k2; long min = 1; ss >> k1 >> k2 >> min;
    Frame a, b;
    if (!Find(k1, a, err, "callsdrop") || !Find(k2, b, err, "callsdrop")) return false;
    printf("      [llamadasbajan] %s -> %s:", k1.c_str(), k2.c_str());
    for (int i = 0; i < CN; i++) printf(" %s %ld->%ld", kNames[i], a.n[i], b.n[i]);
    printf(" (ahorro %ld)\n", a.n[CTotal] - b.n[CTotal]);
    if (a.n[CDraws] != b.n[CDraws]) {
        char t[160]; snprintf(t, sizeof t, "callsdrop: los draws cambiaron (%ld -> %ld)", a.n[CDraws], b.n[CDraws]);
        err = t; return false;
    }
    if (a.n[CTotal] - b.n[CTotal] < min) {
        char t[160]; snprintf(t, sizeof t, "callsdrop: bajaron %ld llamadas y el minimo era %ld", a.n[CTotal] - b.n[CTotal], min);
        err = t; return false;
    }
    return true;
}

} // namespace

// el despachador de esta area (lo llama W3dTestsResourcesCmd)
bool W3dTestsCallsCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "lazy" || cmd == "pointerscache" || cmd == "fewercalls") return CmdOption(cmd, ss, err);
    if (cmd == "callsbuild")  return CmdBuild(err);
    if (cmd == "callspose")   return CmdPose(ss, err);
    if (cmd == "callsframe") return CmdFrame(ss, err);
    if (cmd == "callsequal")  return CmdEqual(ss, err);
    if (cmd == "callsdistinct") return CmdDistinct(ss, err);
    if (cmd == "callsdrop")  return CmdLower(ss, err);
    handled = false;
    return false;
}
