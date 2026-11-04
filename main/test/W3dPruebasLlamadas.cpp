// ============================================================================
//  W3dPruebasLlamadas.cpp — comandos de harness de MENOS LLAMADAS GL POR CUADRO (gfx/w3dGraphics.h): la pila
//  PEREZOSA de la modelview (g_matricesPerezosas) y el CACHE DE PUNTEROS y binds de buffers (g_punterosCache). Los
//  despacha W3dPruebasRecursosCmd (test/W3dPruebasRecursos.cpp). La prueba: tools/pruebas/prueba_menos_llamadas.w3s.
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
#include "test/W3dPruebasRecursos.h"
#include "gfx/w3dTexture.h"     // SavePNG (W3D_CUADRO_PNG)
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "objects/Materials.h"
#include "objects/Light.h"
#include "objects/Collection.h"
#include "objects/Mirror.h"
#include "objects/Recorte.h"
#include "objects/Culling.h"
#include "objects/Camera.h"
#include "objects/Particulas.h"
#include "objects/UI.h"
#include "objects/Rect2D.h"
#include "edit/Modifier.h"
#include "edit/WeightPaint.h"            // WeightPaintAsegurarMapa
#include "animation/SkeletalAnimation.h" // EvaluarPoseEsqueleto / PrepararSkinAutorado
#include "animation/Animation.h"         // CurrentFrame
#include "io/TexturaEditada.h"           // TexEditCrear: una textura en memoria
#include "gfx/w3dGraphics.h"
#include "ViewPorts/ViewPorts.h"         // rootViewport
#include "ViewPorts/ViewPort3D.h"        // Viewport3D
#include "ViewPorts/LayoutInput.h"       // LayoutRenderMenu
#include "WhiskUI/draw/glesdraw.h"       // W3dPantallaAlto
#include <map>
#include <vector>
#include <stdio.h>
#include <string.h>

extern Viewport3D* Viewport3DActive;
extern void CargarTexturasPendientes();
extern int MenuPantallaW, MenuPantallaH;
void W3dParticulasTick(float dt);

namespace {

namespace gfx = w3dEngine;

unsigned Hash(const unsigned char* p, size_t n) {   // FNV-1a de 32 bits
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

// ---- las opciones ----
bool CmdOpcion(const std::string& cmd, std::istringstream& ss, std::string& err) {
    std::string q; ss >> q;
    bool* v = cmd == "perezosas" ? &gfx::g_matricesPerezosas : &gfx::g_punterosCache;
    if (q == "es") {
        int e = -1; ss >> e;
        printf("      [%s] %s\n", cmd.c_str(), *v ? "prendida" : "apagada");
        if ((*v ? 1 : 0) != e) { err = cmd + " es: la opcion esta " + (*v ? "prendida" : "apagada"); return false; }
        return true;
    }
    if (q != "on" && q != "off") { err = cmd + ": uso: " + cmd + " on|off|es 0|1"; return false; }
    const bool on = (q == "on");
    if (cmd == "perezosas" || cmd == "menosllamadas") {
        if (!on) gfx::MatricesSincronizar();   // (lo que quedo perezoso, a GL)
        gfx::g_matricesPerezosas = on;
    }
    if (cmd == "punteroscache" || cmd == "menosllamadas") {
        gfx::g_punterosCache = on;
        if (!on) gfx::UnbindVBOs();             // (apagado de verdad: el camino viejo supone 0 entre mallas)
    }
    printf("      [%s] perezosas=%d punteroscache=%d\n", cmd.c_str(), gfx::g_matricesPerezosas ? 1 : 0, gfx::g_punterosCache ? 1 : 0);
    return true;
}

// ---- la escena ----
Mesh* Malla(MeshType::Enum t, Object* padre, const char* nombre, const Vector3& pos, const Vector3& rot, float esc) {
    Mesh* m = (Mesh*)NewMesh(MeshType(t), padre, false);
    m->SetNameObj(nombre);
    m->pos = pos;
    m->SetRotEuler(rot);
    m->scale = Vector3(esc, esc, esc);
    return m;
}
Empty* Vacio(Object* padre, const char* nombre, const Vector3& pos, const Vector3& rot, const Vector3& esc) {
    Empty* e = new Empty(padre, pos);
    e->SetNameObj(nombre);
    e->SetRotEuler(rot);
    e->scale = esc;
    return e;
}
Material* Mat(const char* nombre, float r, float g, float b) {
    Material* mt = new Material(nombre);
    mt->diffuse[0] = r; mt->diffuse[1] = g; mt->diffuse[2] = b;
    mt->ambient[0] = r * 0.3f; mt->ambient[1] = g * 0.3f; mt->ambient[2] = b * 0.3f;
    return mt;
}
void Pintar(Mesh* m, Material* mt) {
    for (size_t g = 0; g < m->materialsGroup.size(); g++) m->materialsGroup[g].material = mt;
}
// un esqueleto de dos huesos y una malla (cilindro) skinneada con el: la mitad de arriba sigue al segundo hueso
Armature* Personaje(Object* padre, const char* rig, const char* malla, const Vector3& pos, Material* mt) {
    Armature* a = new Armature(padre, pos);
    a->name = rig;
    { W3dBone r; r.name = "raiz";  r.parent = -1; r.head = Vector3(0, 0, 0); r.tail = Vector3(0, 1, 0); a->bones.push_back(r); }
    { W3dBone c; c.name = "brazo"; c.parent = 0;  c.head = Vector3(0, 1, 0); c.tail = Vector3(0, 2, 0); a->bones.push_back(c); }
    PrepararSkinAutorado(a);
    Mesh* s = (Mesh*)NewMesh(MeshType::cylinder, padre, false);
    s->SetNameObj(malla);
    s->pos = pos;
    s->scale = Vector3(0.4f, 0.8f, 0.4f);
    s->GenerarRender();
    WeightPaintAsegurarMapa(s);
    VertexGroup* vr = new VertexGroup("raiz");
    VertexGroup* vb = new VertexGroup("brazo");
    std::vector<char> hecho((size_t)s->vertexSize, 0);
    for (int i = 0; i < s->vertexSize; i++) {
        const int cp = s->vertCtrlPoint[i];
        if (hecho[(size_t)cp]) continue;
        hecho[(size_t)cp] = 1;
        if (s->vertex[i * 3 + 1] > 0.0f) { vb->verts.push_back(cp); vb->pesos.push_back(1.0f); }
        else { vr->verts.push_back(cp); vr->pesos.push_back(0.7f); }
    }
    s->vertexGroups.push_back(vr); s->vertexGroups.push_back(vb);
    Modifier* md = new Modifier(ModifierType::Armature, NombreTipoModificador(ModifierType::Armature));
    md->target = (Object*)a; s->modificadores.push_back(md);
    s->skinArmature = a;
    s->lastSkinFrame = -999999;
    Pintar(s, mt);
    return a;
}
void Posar(Armature* a, float grados) {
    EvaluarPoseEsqueleto(a, CurrentFrame);
    a->bones[1].poseR = Vector3(0, 0, grados);
    a->poseDirty = true;
    EvaluarPoseEsqueleto(a, CurrentFrame);
}

bool CmdArmar(std::string& err) {
    if (!SceneCollection) { err = "llamadasarmar: no hay escena"; return false; }
    Object* raiz = CollectionActive ? (Object*)CollectionActive : (Object*)SceneCollection;
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
    Texture* tex = TexEditCrear("llam_tablero", px, 64, 64, false);
    Texture* tex2 = TexEditCrear("llam_franjas", px2, 64, 64, false);
    if (!tex || !tex2) { err = "llamadasarmar: no se pudieron crear las texturas"; return false; }
    // la camara de la escena por defecto, mas atras (que entre todo), y su cubo chico y a un costado (la rutina
    // dibuja su malla)
    if (CameraActive) CameraActive->pos = CameraActive->pos * 1.7f;
    Object* cubo = FindObjectByName(SceneCollection, "Cubo");
    if (cubo) { cubo->pos = Vector3(-0.2f, -0.6f, -1.4f); cubo->scale = Vector3(0.25f, 0.25f, 0.25f); }

    // 1) JERARQUIA: vacios con traslacion, rotacion y escala, y mallas con VBO adentro (una afuera de la camara)
    Empty* j0 = Vacio(raiz, "LlamJer", Vector3(0.8f, 0.0f, -0.6f), Vector3(0, 25, 0), Vector3(1.0f, 1.0f, 1.0f));
    Empty* j1 = Vacio(j0, "LlamJer.Brazo", Vector3(0.0f, 0.6f, 0.0f), Vector3(15, 0, 30), Vector3(0.8f, 0.8f, 0.8f));
    Material* rojo = Mat("LlamRojo", 0.9f, 0.2f, 0.2f);
    Material* verde = Mat("LlamVerde", 0.2f, 0.8f, 0.3f);
    Pintar(Malla(MeshType::cube, j0, "LlamCuboPadre", Vector3(0, 0, 0), Vector3(0, 0, 0), 0.3f), rojo);
    Pintar(Malla(MeshType::UVsphere, j1, "LlamEsferaHija", Vector3(0.4f, 0.0f, 0.0f), Vector3(0, 45, 0), 0.25f), verde);
    Pintar(Malla(MeshType::cube, j1, "LlamCuboFuera", Vector3(0.0f, 0.0f, 90.0f), Vector3(0, 0, 0), 0.5f), verde);   // autocull
    Pintar(Malla(MeshType::cube, raiz, "LlamCuboLejos", Vector3(-80.0f, 10.0f, 60.0f), Vector3(0, 0, 0), 1.0f), rojo);

    // 2) MATERIALES que van de RAM (reflejo matcap / equirect, normal map, capa) y los que siguen en VBO con algo
    //    mas (mezcla de texturas en la unidad 1, lineas del material)
    Material* matcap = Mat("LlamMatcap", 1, 1, 1);
    matcap->texture = tex; matcap->chrome = true; matcap->reflectMode = 0;
    Material* equi = Mat("LlamEquirect", 1, 1, 1);
    equi->texture = tex; equi->chrome = true; equi->reflectMode = 2;
    Material* nmap = Mat("LlamNormalMap", 0.9f, 0.9f, 0.9f);
    nmap->texture = tex; nmap->normalMap = true; nmap->normalTexture = tex2;
    Material* capa = Mat("LlamCapa", 1, 1, 1);
    capa->texture = tex;
    { TexLayer c; c.tex = tex2; c.on = true; c.blend = 2; capa->capas.push_back(c); }
    Material* mezcla = Mat("LlamMezcla", 1, 1, 1);
    mezcla->texture = tex; mezcla->texturaB = tex2; mezcla->mezclaB = 0.5f;
    Material* lineas = Mat("LlamLineas", 0.3f, 0.6f, 1.0f);
    lineas->lineas = true; lineas->grosorLinea = 2.0f;
    Empty* fila = Vacio(raiz, "LlamFila", Vector3(-1.2f, 0.2f, 0.9f), Vector3(0, -20, 0), Vector3(1, 1, 1));
    Pintar(Malla(MeshType::UVsphere, fila, "LlamMatcap", Vector3(0.0f, 0.0f, 0.0f), Vector3(0, 0, 0), 0.25f), matcap);
    Pintar(Malla(MeshType::UVsphere, fila, "LlamEquirect", Vector3(0.6f, 0.0f, 0.0f), Vector3(0, 30, 0), 0.25f), equi);
    Pintar(Malla(MeshType::cube, fila, "LlamNormalMap", Vector3(1.2f, 0.0f, 0.0f), Vector3(20, 0, 0), 0.22f), nmap);
    Pintar(Malla(MeshType::cube, fila, "LlamCapa", Vector3(0.0f, 0.6f, 0.0f), Vector3(0, 40, 0), 0.22f), capa);
    Pintar(Malla(MeshType::cube, fila, "LlamMezcla", Vector3(0.6f, 0.6f, 0.0f), Vector3(0, 0, 10), 0.22f), mezcla);
    Pintar(Malla(MeshType::cube, fila, "LlamLineas", Vector3(1.2f, 0.6f, 0.0f), Vector3(10, 10, 0), 0.22f), lineas);
    Pintar(Malla(MeshType::cube, fila, "LlamGenerada", Vector3(0.0f, -0.6f, 0.0f), Vector3(0, 0, 0), 0.15f), verde);   // (modadd en el .w3s)

    // 3) una CALCOMANIA (se dibuja en el pase diferido, con su matriz armada aparte)
    Material* decal = Mat("LlamDecal", 0.1f, 0.1f, 0.1f);
    decal->orden_pasada = 1; decal->depth_write = false; decal->transparent = true; decal->diffuse[3] = 0.6f;
    Pintar(Malla(MeshType::plane, raiz, "LlamSombra", Vector3(0.8f, -0.95f, -0.6f), Vector3(0, 0, 0), 0.5f), decal);

    // 4) dos ESQUELETOS con su malla skinneada, colgados de un vacio rotado: uno se posa entre cuadros
    Empty* pj = Vacio(raiz, "LlamPersonajes", Vector3(0.2f, -0.9f, 1.2f), Vector3(0, 35, 0), Vector3(1, 1, 1));
    Material* piel = Mat("LlamPiel", 0.9f, 0.7f, 0.5f);
    Armature* quieto = Personaje(pj, "LlamRigQuieto", "LlamPersonajeQuieto", Vector3(-0.5f, 0, 0), piel);
    Armature* anima = Personaje(pj, "LlamRigAnima", "LlamPersonajeAnima", Vector3(0.5f, 0, 0), piel);
    Posar(quieto, 25.0f);
    Posar(anima, 10.0f);

    // 5) LUCES: una puntual y una spot, con padres rotados (la posicion y la direccion se llevan con la modelview)
    Empty* lp = Vacio(raiz, "LlamLuzPadre", Vector3(0.5f, 1.5f, 0.5f), Vector3(30, 60, 0), Vector3(1, 1, 1));
    Light* puntual = Light::Create(lp, 0.5f, 0.0f, 0.0f);
    if (puntual) { puntual->SetNameObj("LlamPuntual"); puntual->SetDiffuse(1.0f, 0.6f, 0.4f); }
    Empty* sp = Vacio(raiz, "LlamSpotPadre", Vector3(-0.5f, 2.0f, -0.5f), Vector3(-70, 20, 10), Vector3(1, 1, 1));
    Light* spot = Light::Create(sp, 0.0f, 0.0f, 0.0f);
    if (spot) { spot->SetNameObj("LlamSpot"); spot->SetDiffuse(0.4f, 0.6f, 1.0f); spot->spotCutoff = 35.0f; spot->spotExponent = 4.0f; }

    // 6) un ESPEJO horizontal con recorte (estencil o planos de recorte) que refleja un cubo
    Material* agua = Mat("LlamAgua", 0.2f, 0.3f, 0.8f);
    agua->transparent = true; agua->diffuse[3] = 0.5f;
    Pintar(Malla(MeshType::cube, raiz, "LlamEspejado", Vector3(-0.9f, -0.3f, -0.9f), Vector3(0, 30, 0), 0.2f), rojo);
    Mirror* mi = new Mirror(raiz, Vector3(0.0f, -1.2f, 0.0f), false, true, false);
    mi->SetNameObj("LlamEspejo");
    mi->SetTarget("LlamEspejado");
    mi->ReloadTarget(mi);
    mi->sinProfundidad = true;
    mi->usaLimites = true; mi->limU0 = -1.6f; mi->limU1 = -0.2f; mi->limV0 = -1.6f; mi->limV1 = -0.2f;
    Pintar(Malla(MeshType::plane, raiz, "LlamLamina", Vector3(-0.9f, -0.6f, -0.9f), Vector3(0, 0, 0), 0.7f), agua);

    // 7) un RECORTE con su propia camara (otra proyeccion y otro viewport en medio del recorrido) y un CULLING
    Camera* cr = new Camera(raiz, Vector3(0.0f, 0.5f, 3.0f), Vector3(-10.0f, 0.0f, 0.0f));
    cr->SetNameObj("LlamCamRecorte");
    Recorte* rc = new Recorte(raiz);
    rc->SetNameObj("LlamRecorte");
    rc->x = 0.32f; rc->y = 0.3f; rc->ancho = 0.3f; rc->alto = 0.3f; rc->camara = "LlamCamRecorte";
    rc->fondo = true; rc->color[0] = 0.1f; rc->color[1] = 0.1f; rc->color[2] = 0.2f; rc->color[3] = 1.0f;
    Pintar(Malla(MeshType::UVsphere, rc, "LlamEnRecorte", Vector3(0, 0, 0), Vector3(0, 0, 0), 0.6f), verde);
    Culling* cu = new Culling(raiz);
    cu->SetNameObj("LlamCulling");
    Pintar(Malla(MeshType::cube, cu, "LlamCullAdentro", Vector3(-0.4f, 0.9f, -0.2f), Vector3(0, 10, 0), 0.15f), rojo);
    Pintar(Malla(MeshType::cube, cu, "LlamCullAfuera", Vector3(0.0f, 0.0f, -70.0f), Vector3(0, 0, 0), 0.5f), rojo);

    // 8) PARTICULAS: billboards con textura (del disco, al lado de la prueba) y una estela de lineas
    Particulas* pa = new Particulas(raiz, Vector3(1.0f, 0.3f, 0.6f));
    pa->SetNameObj("LlamParticulas");
    pa->textura = "mezcla_a.png"; pa->cantidad = 40.0f; pa->vida = 3.0f; pa->tam = 0.2f; pa->vel = 0.6f; pa->gravedad = 0.0f;
    Particulas* pl = new Particulas(raiz, Vector3(-1.0f, 0.3f, 0.6f));
    pl->SetNameObj("LlamEstela");
    pl->forma = w3dEngine::FormaLinea; pl->cantidad = 20.0f; pl->vida = 3.0f; pl->vel = 0.8f; pl->gravedad = 0.0f;
    for (int i = 0; i < 20; i++) W3dParticulasTick(1.0f / 30.0f);

    // 9) UI 2D con un rectangulo rotado (el HUD del juego)
    UI* ui = new UI(NULL, Vector3(0, 0, 0));
    ui->SetNameObj("LlamHUD");
    Rect2D* r1 = new Rect2D(ui, Vector3(0.0f, 0.0f, 0.0f));
    r1->SetNameObj("LlamFranja");
    r1->ancho = 0.3f; r1->alto = 0.08f; r1->rot2d = 20.0f;
    r1->color[0] = 1.0f; r1->color[1] = 0.8f; r1->color[2] = 0.1f; r1->color[3] = 0.9f;

    // 10) vacios de los que una RUTINA toma la matriz (los pasos van en el .w3s)
    Vacio(raiz, "LlamAnclaRut", Vector3(-0.3f, 1.2f, 0.4f), Vector3(10, 50, 0), Vector3(0.25f, 0.25f, 0.25f));
    Vacio(raiz, "LlamAnclaRut2", Vector3(0.6f, 1.3f, -0.3f), Vector3(0, 0, 25), Vector3(0.2f, 0.3f, 0.2f));

    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [llamadasarmar] jerarquias, mallas VBO/RAM, esqueletos, luces, espejo, recorte, culling, particulas "
           "(%d + %d vivas) y UI 2D\n", (int)pa->sys.parts.size(), (int)pl->sys.parts.size());
    return true;
}

bool CmdPose(std::istringstream& ss, std::string& err) {
    std::string n; float g = 0.0f; ss >> n >> g;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
    if (!o || o->getType() != ObjectType::armature) { err = "llamadaspose: no hay un esqueleto '" + n + "'"; return false; }
    Posar((Armature*)o, g);
    return true;
}

// ---- los cuadros ----
enum { CTotal = 0, CEscena, CParticulas, CUI, CEditor, CDraws, CBinds, CEstados, COtras, CN };
const char* const kNombres[CN] = { "total", "escena", "particulas", "ui", "editor", "draws", "binds", "estados", "otras" };
struct Cuadro { unsigned hash; long n[CN]; int w, h; };
std::map<std::string, Cuadro> gCuadros;

bool CmdCuadro(std::istringstream& ss, std::string& err) {
    std::string clave, modo; ss >> clave >> modo;
    if (clave.empty()) { err = "llamadascuadro: falta la clave"; return false; }
    if (!rootViewport) { err = "llamadascuadro: no hay layout de viewports"; return false; }
    CargarTexturasPendientes();
    Cuadro c;
    std::vector<unsigned char> px;
    if (modo == "vp") {
        rootViewport->Render();   // (puebla Viewport3DActive)
        Viewport3D* vp = Viewport3DActive;
        if (!vp || vp->width < 1 || vp->height < 1) { err = "llamadascuadro: no hay viewport 3D"; return false; }
        gfx::StatsCuadro();
        const int otras0 = gfx::g_statOtras;
        vp->Render();
        gfx::StatsCuadro();
        c.n[COtras] = gfx::g_statOtras - otras0;
        gfx::Finish();
        c.w = vp->width; c.h = vp->height;
        px.assign((size_t)c.w * c.h * 4, 0);
        gfx::ReadPixelsRGBA(vp->x, W3dPantallaAlto - vp->y - vp->height, c.w, c.h, &px[0]);
    } else {
        // el cuadro COMPLETO, como statsvivo: el arbol de viewports y los menus encima
        gfx::StatsCuadro();
        const int otras0 = gfx::g_statOtras;
        rootViewport->Render();
        LayoutRenderMenu(MenuPantallaW, MenuPantallaH);
        gfx::StatsCuadro();
        c.n[COtras] = gfx::g_statOtras - otras0;
        gfx::Finish();
        c.w = MenuPantallaW; c.h = W3dPantallaAlto;
        if (c.w < 1 || c.h < 1) { err = "llamadascuadro: pantalla sin tamano"; return false; }
        px.assign((size_t)c.w * c.h * 4, 0);
        gfx::ReadPixelsRGBA(0, 0, c.w, c.h, &px[0]);
    }
    const gfx::StatsDeCuadro& s = gfx::g_statCuadro;
    c.n[CTotal] = s.total;
    c.n[CEscena] = s.llamadas[gfx::StatCatEscena]; c.n[CParticulas] = s.llamadas[gfx::StatCatParticulas];
    c.n[CUI] = s.llamadas[gfx::StatCatUI]; c.n[CEditor] = s.llamadas[gfx::StatCatEditor];
    c.n[CDraws] = s.draws; c.n[CBinds] = s.binds; c.n[CEstados] = s.estados;
    c.hash = Hash(&px[0], px.size());
    // (para ver la diferencia: W3D_CUADRO_PNG=<carpeta> guarda cada cuadro como <clave>.png)
    if (getenv("W3D_CUADRO_PNG")) gfx::SavePNG((std::string(getenv("W3D_CUADRO_PNG")) + "/" + clave + ".png").c_str(), &px[0], c.w, c.h, false);
    long distintos = 0;   // (que el cuadro dibujo algo: no comparar dos pantallas vacias)
    for (size_t i = 4; i < px.size(); i += 4)
        if (px[i] != px[0] || px[i + 1] != px[1] || px[i + 2] != px[2]) distintos++;
    gCuadros[clave] = c;
    printf("      [llamadascuadro] %s: %dx%d hash=%08x px_no_fondo=%ld", clave.c_str(), c.w, c.h, c.hash, distintos);
    for (int i = 0; i < CN; i++) printf(" %s=%ld", kNombres[i], c.n[i]);
    printf("\n");
    if (distintos == 0) { err = "llamadascuadro: el cuadro salio vacio"; return false; }
    return true;
}

bool Buscar(const std::string& k, Cuadro& c, std::string& err, const char* cmd) {
    std::map<std::string, Cuadro>::iterator it = gCuadros.find(k);
    if (it == gCuadros.end()) { err = std::string(cmd) + ": no hay un cuadro '" + k + "'"; return false; }
    c = it->second;
    return true;
}

bool CmdIgual(std::istringstream& ss, std::string& err) {
    std::string k1, k2; ss >> k1 >> k2;
    Cuadro a, b;
    if (!Buscar(k1, a, err, "llamadasigual") || !Buscar(k2, b, err, "llamadasigual")) return false;
    if (a.w != b.w || a.h != b.h || a.hash != b.hash) {
        char t[200]; snprintf(t, sizeof t, "llamadasigual: la imagen de '%s' (%08x) no es la de '%s' (%08x)", k1.c_str(), a.hash, k2.c_str(), b.hash);
        err = t; return false;
    }
    printf("      [llamadasigual] '%s' y '%s': la misma imagen (%08x)\n", k1.c_str(), k2.c_str(), a.hash);
    return true;
}

// (el control: lo que cambio entre dos cuadros -una pose- se ve)
bool CmdDistinto(std::istringstream& ss, std::string& err) {
    std::string k1, k2; ss >> k1 >> k2;
    Cuadro a, b;
    if (!Buscar(k1, a, err, "llamadasdistinto") || !Buscar(k2, b, err, "llamadasdistinto")) return false;
    if (a.w == b.w && a.h == b.h && a.hash == b.hash) { err = "llamadasdistinto: '" + k1 + "' y '" + k2 + "' son la misma imagen"; return false; }
    printf("      [llamadasdistinto] '%s' (%08x) y '%s' (%08x) son distintas\n", k1.c_str(), a.hash, k2.c_str(), b.hash);
    return true;
}

bool CmdBajan(std::istringstream& ss, std::string& err) {
    std::string k1, k2; long minimo = 1; ss >> k1 >> k2 >> minimo;
    Cuadro a, b;
    if (!Buscar(k1, a, err, "llamadasbajan") || !Buscar(k2, b, err, "llamadasbajan")) return false;
    printf("      [llamadasbajan] %s -> %s:", k1.c_str(), k2.c_str());
    for (int i = 0; i < CN; i++) printf(" %s %ld->%ld", kNombres[i], a.n[i], b.n[i]);
    printf(" (ahorro %ld)\n", a.n[CTotal] - b.n[CTotal]);
    if (a.n[CDraws] != b.n[CDraws]) {
        char t[160]; snprintf(t, sizeof t, "llamadasbajan: los draws cambiaron (%ld -> %ld)", a.n[CDraws], b.n[CDraws]);
        err = t; return false;
    }
    if (a.n[CTotal] - b.n[CTotal] < minimo) {
        char t[160]; snprintf(t, sizeof t, "llamadasbajan: bajaron %ld llamadas y el minimo era %ld", a.n[CTotal] - b.n[CTotal], minimo);
        err = t; return false;
    }
    return true;
}

} // namespace

// el despachador de esta area (lo llama W3dPruebasRecursosCmd)
bool W3dPruebasLlamadasCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "perezosas" || cmd == "punteroscache" || cmd == "menosllamadas") return CmdOpcion(cmd, ss, err);
    if (cmd == "llamadasarmar")  return CmdArmar(err);
    if (cmd == "llamadaspose")   return CmdPose(ss, err);
    if (cmd == "llamadascuadro") return CmdCuadro(ss, err);
    if (cmd == "llamadasigual")  return CmdIgual(ss, err);
    if (cmd == "llamadasdistinto") return CmdDistinto(ss, err);
    if (cmd == "llamadasbajan")  return CmdBajan(ss, err);
    manejado = false;
    return false;
}
