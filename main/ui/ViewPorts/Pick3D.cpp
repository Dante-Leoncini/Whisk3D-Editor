// ===================================================================================================
//  PICK / SELECCION 3D: pick por color + pick de malla + Select Linked / Loop Select (guiados) + Loop Cut.
//  Extraido de LayoutInput.cpp (Fase 2 del reorg). Ver Pick3D.h.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
#include "ViewPorts/Notificaciones.h" // toasts (extraido a su propio archivo)
#include "ViewPorts/NumInput.h" // entrada numerica/formulas (extraido a su propio archivo)
#include "ViewPorts/Parent.h" // emparentar/desemparentar (extraido a su propio archivo)
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/UVEditor.h"
#include "ViewPorts/Timeline.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h" // el velo del modo foco
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Materials.h" // Material (mat->texture) para el dropdown "Texture" del UV editor
#include "objects/Textures.h"  // Texture (path) para las etiquetas del dropdown
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertarKeyframeEsqueleto (Pose Mode: Insert Keyframe)
#include "objects/Instance.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "objects/Primitivas.h"
#include "objects/Textures.h"
#include "variables.h"
#include "render/OpcionesRender.h" // g_fpsActual
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/theme/colores.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
#include "ViewPorts/Pick3D.h"

// ====================================================================
// pick 3D por color (antes vivia en w3dnewscene.cpp, solo Symbian)
// ====================================================================

static int pickCounter = 0;
static Object* pickFound = NULL;

// tipos seleccionables por click: malla (su geometria) o un objeto con icono
// (camara, luz, empty, instancia) -> un punto del tamaño del icono en su origen
static bool PickSeleccionable(Object* obj) {
    if (!obj->visible) return false;
    ObjectType t = obj->getType();
    // si el overlay de ese TIPO esta oculto (submenu "Objects"), tampoco se puede pickear (no se ve -> no se clickea).
    extern bool g_showLights, g_showCamera, g_showEmpty;
    if (t == ObjectType::light  && !g_showLights) return false;
    if (t == ObjectType::camera && !g_showCamera) return false;
    if (t == ObjectType::empty  && !g_showEmpty)  return false;
    return t == ObjectType::mesh || t == ObjectType::camera ||
           t == ObjectType::light || t == ObjectType::empty ||
           t == ObjectType::instance || t == ObjectType::armature;
}
// pick en 2 pasadas: primero NO-armatures (con z-buffer), despues armatures (XRAY, sin z, encima de todo)
// para que el esqueleto se pickee EXACTAMENTE como se ve. 'armaturePass' filtra que se pinta/cuenta en cada pasada.
static bool PickEnPasada(Object* obj, bool armaturePass) {
    if (!PickSeleccionable(obj)) return false;
    return (obj->getType() == ObjectType::armature) == armaturePass;
}

static void PickPaint(Object* obj, bool armaturePass) {
    if (!obj) return;
    // IGUAL que Object::Render: se aplica la matriz LOCAL y los hijos se pintan DENTRO de ella (acumulando el
    // transform del padre). Antes el push/pop envolvia solo a este objeto y los hijos se pintaban en el ORIGEN del
    // mundo -> un objeto EMPARENTADO se pickeaba en el lugar equivocado (bug Dante: no se podia clickear un hijo).
    w3dEngine::PushMatrix();
    Matrix4 M;
    obj->GetMatrix(M);
    w3dEngine::MultMatrix(M.m);
    if (PickEnPasada(obj, armaturePass)) {
        pickCounter++;
        int id = pickCounter; // 1..N
        w3dEngine::Color4f(((id & 0x1F)) / 31.0f, ((id >> 5) & 0x3F) / 63.0f, 0.0f, 1.0f);
        if (obj->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)obj;
            // El pick tiene que pintar EXACTAMENTE lo que dibuja Mesh::Render (solid), sino el click no coincide.
            // PRIORIDAD (igual que Render): (1) malla GENERADA por modificador (subdiv/screw) -> es lo que se VE
            // (sin esto un Screw sin caras no se podia clickear); (2) malla DEFORMADA por esqueleto (skinVertex) ->
            // el click sigue la animacion (sino cae en el bind: el drama de LISA); (3) el bind crudo.
            if (m->genValido && m->genVertex && m->genFaces && m->genFacesSize > 0) {
                w3dEngine::VertexPointer3f(0, m->genVertex);
                w3dEngine::DrawTriangles(m->genFacesSize, m->genFaces);
            } else if (m->skinArmature && m->faces && m->facesSize > 0) {
                extern void SkinearMesh(Mesh*); SkinearMesh(m); // asegura skinVertex al frame actual (cacheado)
                const GLfloat* pb = m->skinVertex ? m->skinVertex : m->vertex; // guard: si no hay skinVertex, bind
                if (pb) { w3dEngine::VertexPointer3f(0, pb); w3dEngine::DrawTriangles(m->facesSize, m->faces); }
            } else if (m->vertex && m->faces) {
                w3dEngine::VertexPointer3f(0, m->vertex);
                w3dEngine::DrawTriangles(m->facesSize, m->faces);
            }
        } else if (obj->getType() == ObjectType::armature) {
            // XRAY: los HUESOS como lineas GRUESAS, sin z-test (se pickean como se ven, encima de todo)
            Armature* arm = (Armature*)obj;
            if (!arm->bones.empty()) {
                std::vector<GLfloat> buf; buf.reserve(arm->bones.size() * 6);
                for (size_t bi = 0; bi < arm->bones.size(); bi++) {
                    const W3dBone& b = arm->bones[bi]; // pose animada (la setea el render); en rest = head/tail
                    buf.push_back(b.poseHead.x); buf.push_back(b.poseHead.y); buf.push_back(b.poseHead.z);
                    buf.push_back(b.poseTail.x); buf.push_back(b.poseTail.y); buf.push_back(b.poseTail.z);
                }
                w3dEngine::Disable(w3dEngine::DepthTest);      // xray (esta pasada es toda de armatures)
                w3dEngine::LineWidth(9.0f * (float)GlobalScale); // grueso: facil de clickear
                w3dEngine::VertexPointer3f(0, &buf[0]);
                w3dEngine::DrawLines((int)(buf.size() / 3));
                w3dEngine::LineWidth(1.0f);
            }
        } else {
            // icono (camara/luz/empty/instancia): un punto clickeable en el
            // origen, del MISMO tamaño que el icono 3D (16 * GlobalScale)
            static const GLfloat origen[3] = { 0.0f, 0.0f, 0.0f };
            w3dEngine::PointSize(16.0f * GlobalScale);
            w3dEngine::VertexPointer3f(0, origen);
            w3dEngine::DrawPoints(1);
        }
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        PickPaint(obj->Childrens[i], armaturePass);
    }
    w3dEngine::PopMatrix();
}

static void PickResolve(Object* obj, int target, bool armaturePass) {
    if (!obj || pickFound) return;
    if (PickEnPasada(obj, armaturePass)) {
        pickCounter++;
        if (pickCounter == target) { pickFound = obj; return; }
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        PickResolve(obj->Childrens[i], target, armaturePass);
    }
}

// hubo input TACTIL en la sesion? (para agrandar el pick de vertices en pantallas tactiles)
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controles.cpp
static bool PickEsTactil(){ return g_lastFingerTicks != 0; }
#else
static bool PickEsTactil(){ return false; }
#endif

// EDIT MODE: pick de VERTICE o ARISTA por color-ID (segun EditSelectMode). Dibuja
// los sub-elementos con un color = (id+1) en R5G6B5 (tolera framebuffer 16-bit:
// hasta 65535), lee el pixel bajo el click y resuelve. Sin shift = solo ese; con
// shift = toggle. Usa la EditMesh (datos de edicion separados de la malla de render).
// Pickea por color-ID el sub-elemento bajo (mx,my) en el MODO dado (SelVertex/Edge/
// Face, NO necesariamente el modo activo: el loop-select pickea un BORDE en modo cara).
// Devuelve el indice 0-based o -1. NO modifica la seleccion.
static int EditPickIndex(int modo, int mx, int my, int vx, int vy, int vw, int vh, int screenH) {
    Mesh* m = (Mesh*)g_editMesh;
    if (!m) return -1;
    m->EnsureEdit();
    EditMesh* e = m->edit;
    if (!e || e->pos.empty()) return -1;
    const bool edgeMode = (modo == SelEdge);
    const bool faceMode = (modo == SelFace);
    const int N = faceMode ? e->NumFaces() : edgeMode ? e->NumEdges() : e->NumVerts();
    if (N <= 0) return -1;

    // misma proyeccion + camara que el viewport real
    int glY = screenH - vy - vh;
    w3dEngine::Viewport(vx, glY, vw, vh);
    w3dEngine::Scissor(vx, glY, vw, vh);
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    float pkNear = Viewport3DActive ? Viewport3DActive->nearClip : 0.01f;
    float pkFar  = Viewport3DActive ? Viewport3DActive->farClip  : 1000.0f;
    float pkAspect = (float)vw / (float)vh;
    if (Viewport3DActive && Viewport3DActive->orthographic) {
        // MISMA proyeccion ortografica que Render() (size = orbitDistance*tan(fov/2)); antes pickeaba SIEMPRE con
        // perspectiva -> en orto el click caia corrido. Ahora el color-id usa el mismo volumen que se ve.
        float size = Viewport3DActive->orbitDistance * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
        if (size < 0.001f) size = 0.001f;
        w3dEngine::Ortho(-size * pkAspect, size * pkAspect, -size, size, pkNear, Viewport3DActive->orbitDistance + pkFar);
    } else {
        w3dEngine::Perspective(fovDeg, pkAspect, pkNear, pkFar);
    }
    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    if (Viewport3DActive) Viewport3DActive->UpdateViewOrbit();
    // matriz de MUNDO del objeto: la MISMA que usa el foco y el render. Antes el
    // pick rearmaba la cadena a mano con la condicion "o->Parent", que salteaba la
    // matriz del PROPIO objeto si era top-level (Parent NULL: el ctor lo agrega a
    // SceneCollection->Childrens pero NO le setea Parent) y la del padre top -> a
    // escala!=1 o emparentado, el pick caia corrido (a escala 1 ~= identidad y zafaba).
    {
        Matrix4 W; m->GetWorldMatrix(W);
        w3dEngine::MultMatrix(W.m);
    }

    w3dEngine::Disable(w3dEngine::Dither);
    // MSAA *mezcla* los colores-ID en los bordes (4 samples promediados) -> el color
    // leido decodifica a un indice CUALQUIERA (lejano) -> el pick "agarra otra cosa de
    // la pantalla", impredecible. Apagarlo deja cada pixel cubierto con el ID solido.
    w3dEngine::Disable(w3dEngine::Multisample);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::ColorArray);
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::ClearColor(0, 0, 0, 1);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    // OCLUSION: dibujar la malla RELLENA solo en el depth (sin color), un toque
    // atras (polygon offset), para que los sub-elementos de ATRAS -tapados por la
    // malla en el render- NO sean clickeables. Solo se pickea lo que se VE adelante.
    // (En PC el depth-test de los puntos ya lo resolvia; en Symbian no, y se
    // seleccionaba un elemento de atras que caia cerca en pantalla.)
    // X-RAY (solo verts/aristas, NO caras): se SALTEA la oclusion -> los verts/bordes de atras tambien se pickean.
    const bool xrayPick = g_xray && !faceMode;
    if (!xrayPick && m->vertex && m->faces && m->facesSize >= 3) {
        w3dEngine::ColorMask(false, false, false, false);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::Enable(w3dEngine::PolygonOffsetFill);
        w3dEngine::PolygonOffset(1.0f, 1.0f);
        w3dEngine::VertexPointer3f(0, m->vertex);
        w3dEngine::DrawTriangles(m->facesSize, m->faces);
        w3dEngine::Disable(w3dEngine::PolygonOffsetFill);
        w3dEngine::ColorMask(true, true, true, true);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
    }

    if (faceMode) {
        // caras: triangulos EXPANDIDOS con color-ID por cara (vertices propios por
        // cara: un vertice compartido no puede tener 2 colores de cara distintos)
        std::vector<GLfloat> tp; std::vector<GLubyte> tc;
        for (int f = 0; f < N; f++) {
            int id = f + 1;
            GLubyte r=(GLubyte)((id&0x1F)<<3), g=(GLubyte)(((id>>5)&0x3F)<<2), b=(GLubyte)(((id>>11)&0x1F)<<3);
            const std::vector<int>& p = e->faces[(size_t)f];
            for (size_t t = 1; t + 1 < p.size(); t++) {
                int tri[3] = { p[0], p[t], p[t+1] };
                for (int j = 0; j < 3; j++) {
                    int v = tri[j];
                    tp.push_back(e->pos[v*3]); tp.push_back(e->pos[v*3+1]); tp.push_back(e->pos[v*3+2]);
                    tc.push_back(r); tc.push_back(g); tc.push_back(b); tc.push_back(255);
                }
            }
        }
        if (!tp.empty()) {
            w3dEngine::VertexPointer3f(0, &tp[0]);
            w3dEngine::ColorPointer4ub(&tc[0]);
            w3dEngine::DrawTrianglesArray((int)(tp.size() / 3));
        }
    } else if (edgeMode) {
        // aristas como lineas GORDAS con color-ID por arista (los 2 vertices iguales)
        const int NV = (int)(e->linePos.size() / 3); // = N*2
        std::vector<GLubyte> idcol((size_t)NV * 4);
        for (int eg = 0; eg < N; eg++) {
            int id = eg + 1;
            GLubyte r=(GLubyte)((id&0x1F)<<3), g=(GLubyte)(((id>>5)&0x3F)<<2), b=(GLubyte)(((id>>11)&0x1F)<<3);
            for (int s = 0; s < 2; s++) { int v = eg*2+s; idcol[v*4]=r; idcol[v*4+1]=g; idcol[v*4+2]=b; idcol[v*4+3]=255; }
        }
        w3dEngine::VertexPointer3f(0, &e->linePos[0]);
        w3dEngine::ColorPointer4ub(&idcol[0]);
        w3dEngine::LineWidth(9.0f); // gorda: facil de clickear
        w3dEngine::DrawLines(NV);
        w3dEngine::LineWidth(1.0f);
    } else {
        std::vector<GLubyte> idcol((size_t)N * 4);
        for (int k = 0; k < N; k++) {
            int id = k + 1;
            idcol[k*4]   = (GLubyte)((id & 0x1F) << 3);
            idcol[k*4+1] = (GLubyte)(((id >> 5) & 0x3F) << 2);
            idcol[k*4+2] = (GLubyte)(((id >> 11) & 0x1F) << 3);
            idcol[k*4+3] = 255;
        }
        w3dEngine::VertexPointer3f(0, &e->pos[0]);
        w3dEngine::ColorPointer4ub(&idcol[0]);
        // TOUCH: punto de pick un poco mas grande (los vertices son puntitos, dificiles con el dedo), pero
        // SIN exagerar: puntos muy grandes se solapan en mallas densas y sesgan el voto. La tolerancia real
        // la da el area de lectura ampliada (abajo). Con mouse queda igual (22).
        w3dEngine::PointSize(PickEsTactil() ? 28.0f : 22.0f);
        w3dEngine::DrawPoints(N);
        w3dEngine::PointSize(1.0f);
    }

    // leer un AREA chica (no 1 pixel) y tomar el ID mas cercano al centro: tolera
    // que el cursor virtual del telefono caiga un par de pixeles corrido + el
    // clamping del tamano de punto/linea del driver. Clampeado al viewport.
#ifdef __EMSCRIPTEN__
    // WebGL clampea glLineWidth a 1px (las aristas del pick se dibujan de 1px en vez de 9) y limita
    // el tamano de punto -> leemos un AREA MAS GRANDE para pescar aristas/caras finas bajo el click.
    int RAD = 7;
#else
    int RAD = 3;
#endif
    // VERTICE + TOUCH: area de lectura GRANDE -> el vertice mas cercano al dedo gana (el voto por
    // pixeles favorece al que tiene el punto mas centrado). Aristas/caras y mouse quedan igual.
    if (!faceMode && !edgeMode && PickEsTactil()) RAD = 16;
    const int WD = 2 * RAD + 1;
    int cxw = mx, cyw = screenH - 1 - my; // centro en coords de ventana (GL)
    int x0 = cxw - RAD, y0 = cyw - RAD;
    if (x0 < vx) x0 = vx; if (x0 + WD > vx + vw) x0 = vx + vw - WD;
    if (y0 < glY) y0 = glY; if (y0 + WD > glY + vh) y0 = glY + vh - WD;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    GLubyte area[33 * 33 * 4]; // dimensionado para el WD mas grande (vertice+touch: 33); otros modos usan menos
    w3dEngine::ReadPixelsRGBA(x0, y0, WD, WD, area);
    w3dEngine::Enable(w3dEngine::Dither);
    w3dEngine::Enable(w3dEngine::Multisample); // restaurar el MSAA para el render normal
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray); // baseline de la UI

    int ccx = cxw - x0, ccy = cyw - y0; // donde cae el centro real dentro del area
    // MSAA-robusto: NO tomamos el pixel valido mas cercano al centro (que puede ser BASURA de un borde que el
    // antialiasing mezclo -> decodifica a un id CUALQUIERA y selecciona un vert lejano en cualquier direccion).
    // En su lugar VOTAMOS: cada id suma 1 por pixel. Un vert real cubre un BLOQUE solido; un artefacto de MSAA es
    // un pixel suelto -> pierde. Desempate por cercania al centro (para elegir el vert justo bajo el click).
    int id = 0;
    {
        std::map<int,int> cnt; std::map<int,long> nearest;
        for (int dy = 0; dy < WD; dy++) for (int dx = 0; dx < WD; dx++) {
            GLubyte* px = &area[(dy*WD + dx) * 4];
            int cand = (px[0] >> 3) | ((px[1] >> 2) << 5) | ((px[2] >> 3) << 11);
            if (cand <= 0 || cand > N) continue;
            long ex = dx - ccx, ey = dy - ccy, d = ex*ex + ey*ey;
            cnt[cand]++;
            std::map<int,long>::iterator itn = nearest.find(cand);
            if (itn == nearest.end() || d < itn->second) nearest[cand] = d;
        }
        int bestCnt = 0; long bestNear = 1L << 30;
        for (std::map<int,int>::iterator it = cnt.begin(); it != cnt.end(); ++it) {
            long nr = nearest[it->first];
            if (it->second > bestCnt || (it->second == bestCnt && nr < bestNear)) { bestCnt = it->second; bestNear = nr; id = it->first; }
        }
    }
    int k = id - 1;
    return (k >= 0 && k < N) ? k : -1;
}

// click normal de seleccion: pickea en el modo activo y togglea (sin shift = solo ese).
static bool EditPickVert(int mx, int my, int vx, int vy, int vw, int vh, int screenH) {
    Mesh* m = (Mesh*)g_editMesh;
    if (!m) return true;
    m->EnsureEdit();
    EditMesh* e = m->edit;
    if (!e) return true;
    int k = EditPickIndex(EditSelectMode, mx, my, vx, vy, vw, vh, screenH);
    if (k >= 0) {
        if (EditSelectMode == SelFace)      e->TogglearFace(k, !LShiftPressed);
        else if (EditSelectMode == SelEdge) e->TogglearEdge(k, !LShiftPressed);
        else                                e->TogglearVert(k, !LShiftPressed);
    } else if (!LShiftPressed) {
        e->SeleccionarTodo(false); // click al vacio sin shift: deseleccionar todo
    }
    return true; // el frame siguiente redibuja la escena real
}

// ===== Navegacion de seleccion por TECLADO en Edit Mode (lapiz + flechas de Symbian) =====
// Opera sobre vert/edge/face segun EditSelectMode, tocando el array de seleccion + activeIdx;
// Recolorear() refresca el visual (incluye el relleno de caras). Devuelve false si no aplica.
static unsigned char* EditSelArray(EditMesh* e, int& N){
    if (EditSelectMode == SelFace){ N=(int)e->faceSel.size(); return e->faceSel.empty()?NULL:&e->faceSel[0]; }
    if (EditSelectMode == SelEdge){ N=(int)e->edgeSel.size(); return e->edgeSel.empty()?NULL:&e->edgeSel[0]; }
    N=(int)e->vertSel.size(); return e->vertSel.empty()?NULL:&e->vertSel[0];
}
static EditMesh* EditSelMesh(){
    if (InteractionMode != EditMode || !g_editMesh) return NULL;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); return m->edit;
}

// ===== Navegacion de seleccion de OBJETOS (Object Mode) =====
// Espejo EXACTO de la de Edit Mode, pero sobre los objetos de la escena (mismo lapiz +
// flechas de Symbian). El "activo" es ObjActivo; el "array" es la lista plana de objetos
// (sin colecciones) en orden de outliner. Seleccionar()/Deseleccionar() mantienen select +
// ObjSelects + ObjActivo. NO tocar la seleccion durante un transform (estado != editNavegacion,
// como DeseleccionarTodo: estadoObjetos apunta a los seleccionados y crashea si cambia).
static void ObjSelRecolectar(Object* nodo, std::vector<Object*>& out){
    if (!nodo) return;
    for (size_t i=0;i<nodo->Childrens.size();i++){
        Object* h = nodo->Childrens[i];
        if (!h) continue;
        if (h->getType() != ObjectType::collection) out.push_back(h); // los objetos, no colecciones
        ObjSelRecolectar(h, out);                                     // recursivo (sub-colecciones)
    }
}
static bool ObjSelLista(std::vector<Object*>& v){
    if (!SceneCollection) return false;
    ObjSelRecolectar(SceneCollection, v);
    return !v.empty();
}
static int ObjSelActivoIdx(const std::vector<Object*>& v){
    for (size_t i=0;i<v.size();i++) if (v[i]==ObjActivo) return (int)i;
    return -1;
}
static bool ObjSelAvanzar(int paso, bool extender){
    if (estado != editNavegacion) return true;        // no durante un transform
    std::vector<Object*> v; if (!ObjSelLista(v)) return true;
    int N=(int)v.size();
    int act = ObjSelActivoIdx(v);
    if (act<0){ act=0; for (int i=0;i<N;i++) if (v[i]->select){ act=i; break; } } // sin activo: 1er sel (o 0)
    if (!extender) v[act]->Deseleccionar();           // deselecciona SOLO el activo (el resto queda)
    int next = act + paso; if (next<0) next=N-1; if (next>=N) next=0;
    v[next]->Seleccionar();                            // select=true + ObjActivo=v[next]
    g_redraw = true;
    return true;
}
static bool ObjSelTodoToggle(){
    if (estado != editNavegacion) return true;
    std::vector<Object*> v; if (!ObjSelLista(v)) return true;
    int N=(int)v.size();
    bool todo = true; for (int i=0;i<N;i++) if (!v[i]->select){ todo=false; break; }
    if (todo){ for (int i=0;i<N;i++) v[i]->Deseleccionar(); ObjActivo = NULL; }
    else     { for (int i=0;i<N;i++) v[i]->Seleccionar(); ObjActivo = v[0]; }
    g_redraw = true;
    return true;
}
static bool ObjSelToggleActual(){
    if (estado != editNavegacion) return true;
    std::vector<Object*> v; if (!ObjSelLista(v)) return true;
    int N=(int)v.size();
    int act = ObjSelActivoIdx(v);
    if (act<0){ act=0; for (int i=0;i<N;i++) if (v[i]->select){ act=i; break; } if (!v[act]->select) return true; }
    if (v[act]->select){ v[act]->Deseleccionar(); ObjActivo = NULL; }
    else                 v[act]->Seleccionar();        // Seleccionar setea ObjActivo
    g_redraw = true;
    return true;
}

// lapiz solo (extender=false): DESELECCIONA el activo + selecciona el siguiente (el resto
// queda). lapiz+flecha (extender=true): mantiene todo + agrega el siguiente/anterior. El
// nuevo siempre queda ACTIVO. paso = +1 (siguiente) / -1 (anterior).
// Mode-aware: en Object Mode delega a la version de objetos (mismo comportamiento exacto).
// ---- POSE MODE: el lapiz/las flechas ciclan HUESOS, no objetos. (Estaba: "si es Edit Mode, sub-elementos; si
//      no, OBJETOS" -> en Pose caia en objetos y el shift te deseleccionaba el armature y saltaba al siguiente
//      objeto, que es lo ultimo que queres mientras posas.)
static Armature* SelArmActiva(){
    return (InteractionMode == PoseMode && ObjActivo && ObjActivo->getType() == ObjectType::armature)
           ? (Armature*)ObjActivo : NULL;
}
static bool PoseSelAvanzar(Armature* a, int paso, bool extender){
    int N = (int)a->bones.size(); if (N <= 0) return true;
    int act = a->boneActivo;
    if (act < 0 || act >= N){ act = 0; for (int i=0;i<N;i++) if (a->bones[i].select){ act=i; break; } }
    if (!extender) a->bones[act].select = false;   // deselecciona el activo
    int next = act + paso;
    if (next < 0) next = N-1;
    if (next >= N) next = 0;
    a->bones[next].select = true;                  // selecciona el siguiente (si ya estaba, queda)
    a->boneActivo = next;
    g_redraw = true;
    return true;
}
static bool PoseSelTodoToggle(Armature* a){
    int N = (int)a->bones.size(); if (N <= 0) return true;
    bool todo = true; for (int i=0;i<N;i++) if (!a->bones[i].select){ todo=false; break; }
    for (int i=0;i<N;i++) a->bones[i].select = todo ? false : true;
    a->boneActivo = todo ? -1 : 0;
    g_redraw = true;
    return true;
}

bool EditSelAvanzar(int paso, bool extender){
    Armature* pa = SelArmActiva(); if (pa) return PoseSelAvanzar(pa, paso, extender);
    if (InteractionMode != EditMode) return ObjSelAvanzar(paso, extender);
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    int act = e->activeIdx;
    if (act<0 || act>=N){ act=0; for (int i=0;i<N;i++) if (sel[i]){ act=i; break; } } // sin activo: 1er sel (o 0)
    if (!extender) sel[act] = 0;          // deselecciona el activo
    int next = act + paso;
    if (next < 0) next = N-1;
    if (next >= N) next = 0;
    sel[next] = 1;                        // selecciona el siguiente (si ya estaba, queda)
    e->activeIdx = next;
    e->Recolorear(); g_redraw = true;
    return true;
}
// lapiz+arriba: si TODO esta seleccionado -> nada; sino -> todo.
bool EditSelTodoToggle(){
    Armature* pa = SelArmActiva(); if (pa) return PoseSelTodoToggle(pa);
    if (InteractionMode != EditMode) return ObjSelTodoToggle();
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    bool todo = true; for (int i=0;i<N;i++) if (!sel[i]){ todo=false; break; }
    for (int i=0;i<N;i++) sel[i] = todo ? 0 : 1;
    e->activeIdx = todo ? -1 : 0;
    e->Recolorear(); g_redraw = true;
    return true;
}
// lapiz+abajo: togglea el indice ACTIVO. Si lo deselecciona, pierde el activo.
bool EditSelToggleActual(){
    if (InteractionMode != EditMode) return ObjSelToggleActual();
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    int act = e->activeIdx;
    if (act<0 || act>=N){ act=0; for (int i=0;i<N;i++) if (sel[i]){ act=i; break; } if (act<0) return true; }
    sel[act] = sel[act] ? 0 : 1;
    e->activeIdx = sel[act] ? act : -1;
    e->Recolorear(); g_redraw = true;
    return true;
}

// L: Select Linked — selecciona la ISLA (componente conexa) bajo el mouse, en el
// modo activo. Sin shift reemplaza; con shift agrega. Usa el rect del viewport activo.
void LayoutSelectLinked(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh || !Viewport3DActive) return;
    Mesh* m = (Mesh*)g_editMesh;
    m->EnsureEdit();
    if (!m->edit) return;
    UndoCapturarSeleccionEdit(m); // Ctrl+Z: Select Linked va a cambiar la seleccion
    int vx = Viewport3DActive->x, vy = Viewport3DActive->y;
    int vw = Viewport3DActive->width, vh = Viewport3DActive->height;
    int k = EditPickIndex(EditSelectMode, mx, my, vx, vy, vw, vh, W3dPantallaAlto);
    if (k >= 0) { m->edit->SeleccionarLinked(k, EditSelectMode, !LShiftPressed); g_redraw = true; }
}

// ===== LOOP SELECT desde el sub-elemento ACTIVO (menu Select / Symbian lapiz+OK) =====
// Modo BORDE: el borde activo da la direccion (sin modal). Modo CARA: una cara tiene 2 sentidos,
// asi que arranca un modal de orientacion: las flechas alternan, OK/click/enter acepta. La
// seleccion resaltada ES el preview (no hace falta geometria aparte).
static bool gLoopSelOrient = false;
static int  gLoopSelFace   = -1;
static int  gLoopSelDir    = 0;   // 0 / 1 (faceEdges[face][0] o [1])
bool LoopSelOrientando(){ return gLoopSelOrient; }

void LayoutLoopSelectGuiado(); // definida mas abajo (modo guiado: pide click cuando no hay elemento activo)
void LayoutLoopSelectActivo(int tipo){
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    EditMesh* e=m->edit;
    if (tipo == 2){                                   // FACE LOOP desde la cara activa
        int act = e->activeIdx;
        if (act<0) for (size_t f=0;f<e->faceSel.size();f++) if (e->faceSel[f]){ act=(int)f; break; }
        if (act<0){ LayoutLoopSelectGuiado(); return; }  // SIN cara activa -> modo guiado (pedi click o cancela)
        if (act>=(int)e->faceEdges.size() || e->faceEdges[act].size()<2) return;
        UndoCapturarSeleccionEdit(m); // Ctrl+Z: el loop select (+ refinar direccion con flechas) = 1 sola accion
        gLoopSelFace = act; gLoopSelDir = 0; gLoopSelOrient = true;
        e->SeleccionarLoopFace(e->faceEdges[act][0], true);
        NotificarHint("Loop Select: arrows = direction, Enter to confirm");
    } else {                                          // EDGE LOOP / RING desde el borde activo
        int act = e->activeIdx;
        if (act<0) for (size_t k=0;k<e->edgeSel.size();k++) if (e->edgeSel[k]){ act=(int)k; break; }
        if (act<0){ LayoutLoopSelectGuiado(); return; }  // SIN borde activo -> modo guiado
        UndoCapturarSeleccionEdit(m);
        if (tipo==0) e->SeleccionarLoopEdge(act, true);
        else         e->SeleccionarRingEdge(act, true);
    }
    g_redraw = true;
}
void LoopSelTecla(int dir){
    if (!gLoopSelOrient || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; if (!m->edit) return;
    EditMesh* e=m->edit;
    if (gLoopSelFace<0 || gLoopSelFace>=(int)e->faceEdges.size() || e->faceEdges[gLoopSelFace].size()<2) return;
    gLoopSelDir = (dir==1 || dir==3) ? 1 : 0;
    e->SeleccionarLoopFace(e->faceEdges[gLoopSelFace][gLoopSelDir], true);
    g_redraw = true;
}
void LoopSelConfirm(){ gLoopSelOrient = false; NotificarHintClear(); g_redraw = true; }

// Loop select en una POSICION (cursor): pickea el borde bajo (mx,my) y aplica el loop del modo
// (cara -> Face Loop ; borde -> Edge Loop). El cursor da la direccion, asi que NO hay modal.
// Lo usa el mouse virtual de Symbian (lapiz + OK). Devuelve false si no pickeo nada.
bool LayoutLoopSelectEnPos(int mx, int my, int vx, int vy, int vw, int vh, int screenH){
    if (InteractionMode != EditMode || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return false;
    int eg = EditPickIndex(SelEdge, mx, my, vx, vy, vw, vh, screenH); // siempre pickea un BORDE de referencia
    if (eg < 0) return false;
    UndoCapturarSeleccionEdit(m); // Ctrl+Z: loop select por cursor (Symbian) va a cambiar la seleccion
    EditMesh* ed = m->edit;
    if (EditSelectMode == SelFace)      ed->SeleccionarLoopFace(eg, true);
    else if (EditSelectMode == SelEdge) ed->SeleccionarLoopEdge(eg, true);
    else                                ed->SeleccionarLoopEdgeVerts(eg, true); // VERTICE: verts del loop, sin acumular edgeSel
    g_redraw = true;
    return true;
}

// ===== PICK SHORTEST PATH guiado desde el menu Select (2 clicks con cartel-tutorial) =====
// Al activarlo desde el menu sale un hint "click the first <elem>"; el 1er click fija el activo
// y el cartel pasa a "...the second <elem>"; el 2do click hace el shortest path (con/sin fill).
// El click se intercepta en ScenePick3D, asi anda IGUAL en PC (mouse) y Symbian (mouse virtual + OK).
static bool gPathGuided = false;
static bool gPathFill   = false;
static int  gPathStep   = 0;   // 0 = espera el primero, 1 = espera el segundo
bool PickPathGuiado(){ return gPathGuided; }

static const char* PathElemNombre(){
    if (EditSelectMode == SelFace) return "face";
    if (EditSelectMode == SelEdge) return "edge";
    return "vertex";
}
void LayoutPickPathIniciar(bool fill){
    if (InteractionMode != EditMode || !g_editMesh) return;
    gPathGuided = true; gPathFill = fill; gPathStep = 0;
    NotificarHint(std::string("Pick Shortest Path: click the first ") + PathElemNombre());
    g_redraw = true;
}
void LayoutPickPathCancelar(){
    if (!gPathGuided) return;
    gPathGuided = false; NotificarHintClear(); g_redraw = true;
}
// click durante el modo guiado: true si lo consumio (ScenePick3D NO sigue con el pick normal).
static bool PickPathClick(int mx,int my,int vx,int vy,int vw,int vh,int screenH){
    if (!gPathGuided || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit){ gPathGuided=false; NotificarHintClear(); return true; }
    int k = EditPickIndex(EditSelectMode, mx,my,vx,vy,vw,vh,screenH);
    if (k < 0) return true;   // click al vacio: ignora (sigue esperando el mismo paso)
    if (gPathStep == 0){
        if (EditSelectMode==SelFace)      m->edit->TogglearFace(k, true);  // selecciona el primero (activo)
        else if (EditSelectMode==SelEdge) m->edit->TogglearEdge(k, true);
        else                              m->edit->TogglearVert(k, true);
        gPathStep = 1;
        NotificarHint(std::string("Now click the second ") + PathElemNombre());
    } else {
        m->edit->SeleccionarShortestPath(k, gPathFill);
        gPathGuided = false;
        NotificarHintClear();
        Notificar(gPathFill ? "Shortest path: region filled" : "Shortest path selected", false);
    }
    g_redraw = true;
    return true;
}

// ===== modo guiado de 1 CLICK (Select Linked desde el menu; Loop Select sin elemento activo) =====
// Sale un hint pidiendo click; el click hace la operacion y termina (1 solo click, vs los 2 del shortest path).
// El click se intercepta en ScenePick3D -> anda IGUAL en PC (mouse) y Symbian (mouse virtual + OK).
static int gGuiadoOp = 0; // 0 = ninguno, 1 = Select Linked, 2 = Loop Select
bool GuiadoUnClickActivo(){ return gGuiadoOp != 0; }
void LayoutGuiadoCancelar(){ if (!gGuiadoOp) return; gGuiadoOp = 0; NotificarHintClear(); g_redraw = true; }
static const char* GuiadoElemNombre(){ return (EditSelectMode==SelFace)?"face":(EditSelectMode==SelEdge)?"edge":"vertex"; }
void LayoutSelectLinkedGuiado(){
    if (InteractionMode != EditMode || !g_editMesh) return;
    gGuiadoOp = 1;
    NotificarHint(std::string("Select Linked: click the ") + GuiadoElemNombre() + " you want to select (Esc to cancel)");
    g_redraw = true;
}
void LayoutLoopSelectGuiado(){ // fallback de Loop Select cuando no hay elemento activo (pide click)
    if (InteractionMode != EditMode || !g_editMesh) return;
    gGuiadoOp = 2;
    NotificarHint(std::string("Loop Select: click an ") + ((EditSelectMode==SelFace)?"edge of a face":"edge") + " (Esc to cancel)");
    g_redraw = true;
}
// click durante el modo guiado: true si lo consumio (ScenePick3D NO sigue con el pick normal).
static bool GuiadoUnClickClick(int mx,int my,int vx,int vy,int vw,int vh,int screenH){
    if (!gGuiadoOp || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit){ gGuiadoOp=0; NotificarHintClear(); return true; }
    if (gGuiadoOp == 1){ // Select Linked: la isla conexa que contiene al elemento clickeado
        int k = EditPickIndex(EditSelectMode, mx,my,vx,vy,vw,vh,screenH);
        if (k < 0) return true; // click al vacio: sigue esperando
        UndoCapturarSeleccionEdit(m);
        m->edit->SeleccionarLinked(k, EditSelectMode, !LShiftPressed);
    } else { // Loop Select en la posicion clickeada (cara->Face Loop, borde->Edge Loop; el cursor da la direccion)
        if (!LayoutLoopSelectEnPos(mx,my,vx,vy,vw,vh,screenH)) return true; // click al vacio: sigue esperando
    }
    gGuiadoOp = 0; NotificarHintClear(); g_redraw = true;
    return true;
}

// ====================================================================
//  LOOP CUT AND SLIDE (Ctrl+R) — herramienta modal
// ====================================================================
//  Fase PREVIEW: hover -> dibuja el corte que se generaria (gLoopCutSegs); rueda = mas
//  cortes. Click izq -> aplica el corte (snapshot) y entra en SLIDE: mover el mouse
//  ajusta el factor (re-corta desde el snapshot). Click izq confirma; click der deja el
//  factor en 0 (centro) y confirma. Al confirmar sale el panel redo (cortes + factor).
//  Esc cancela. Compartido 4 OS (cada plataforma rutea sus eventos).
static bool gLoopCutOn = false;
static bool gLoopCutSlide = false;
static int  gLoopCutCortes = 1;
static float gLoopCutFactor = 0.0f; // -1..1 (0 = centro)
static int  gLoopCutEdge = -1;      // arista (edit mesh) bajo el mouse = start del loop
static int  gLoopCutSlideX0 = 0;    // X del mouse al empezar el slide
static int  gLoopCutSlideW = 800;   // ancho del viewport (escala del slide)
static std::vector<float> gLoopCutSegs; // preview (segmentos locales) -> lo dibuja el viewport
bool gLoopCutEsPunto = false; // true = el corte es de UN borde suelto (perfil): el preview son PUNTOS, no lineas.
                              // Lo setea Mesh::LoopCutPreview (MeshEdit) segun si hay anillo de quads o no.
// fase ORIENTACION: solo al arrancar el loop cut desde un QUAD activo (menu). Antes de
// elegir los cortes, las flechas eligen cual de las 2 direcciones del quad cortar.
static bool gLoopCutOrientando = false;
static int  gLoopCutOrient = 0;           // 0 / 1
static int  gLoopCutOrientEdge[2] = {-1,-1}; // los 2 bordes perpendiculares del quad
bool LoopCutOrientando(){ return gLoopCutOrientando; }
// PICKER de direccion (click/tactil): en la fase de orientacion se dibujan los DOS cortes posibles
// (amarillo 0.5) + 4 PUNTOS grandes amarillos (centro de cada borde del quad). Tocar/clickear un punto
// elige la direccion de una. 2 puntos = una direccion, los otros 2 = la otra. gLoopCutSegs2 = el 2do corte.
static std::vector<float> gLoopCutSegs2;      // preview del corte de la OTRA direccion (solo en orientacion)
static float gLoopCutOrientPt[4][3];          // 4 midpoints (LOCAL) de los bordes del quad
static int   gLoopCutOrientPtDir[4] = {0,0,0,0}; // a que direccion (0/1) pertenece cada punto
static int   gLoopCutOrientNPts = 0;          // 4 cuando el picker esta activo
static void LoopCutAplicarYPanel(); // (def mas abajo) aplica el corte + abre el panel de edicion
// en tactil no hay rueda ni flechas -> el loop cut se ajusta desde el panel redo (cortes + slide)
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controles.cpp: hubo input tactil en la sesion
static bool LoopCutTactil(){ return g_lastFingerTicks != 0; }
#else
static bool LoopCutTactil(){ return false; } // Symbian: va a teclado (flechas + Enter)
#endif

// snapshot de la geometria PRE-corte (para re-cortar en slide / redo)
struct LCSnap { std::vector<GLfloat> vp; std::vector<GLbyte> vn; std::vector<GLfloat> vu;
                std::vector<GLubyte> vc; int vsz; std::vector<MeshFace> f3d;
                std::vector<MaterialGroup> mg; std::vector<int> le, lv; bool tiene; }; // le/lv = looseEdges/looseVerts
static LCSnap gLCSnap;

static void LCGuardar(Mesh* m){
    gLCSnap.vsz = m->vertexSize;
    gLCSnap.vp.assign(m->vertex, m->vertex + m->vertexSize*3);
    if (m->normals)     gLCSnap.vn.assign(m->normals, m->normals + m->vertexSize*3); else gLCSnap.vn.clear();
    if (m->uv)          gLCSnap.vu.assign(m->uv, m->uv + m->vertexSize*2); else gLCSnap.vu.clear();
    if (m->vertexColor) gLCSnap.vc.assign(m->vertexColor, m->vertexColor + m->vertexSize*4); else gLCSnap.vc.clear();
    gLCSnap.f3d = m->faces3d; gLCSnap.mg = m->materialsGroup;
    gLCSnap.le = m->looseEdges; gLCSnap.lv = m->looseVerts; // sin esto el corte de un borde SUELTO (perfil) perdia las lineas al re-cortar
    gLCSnap.tiene = true;
}
static void LCRestaurar(Mesh* m){
    if (!gLCSnap.tiene) return;
    m->vertexSize = gLCSnap.vsz;
    delete[] m->vertex; m->vertex = new GLfloat[gLCSnap.vsz*3];
    for (int i=0;i<gLCSnap.vsz*3;i++) m->vertex[i]=gLCSnap.vp[i];
    if (!gLCSnap.vn.empty()){ delete[] m->normals; m->normals=new GLbyte[gLCSnap.vsz*3]; for (int i=0;i<gLCSnap.vsz*3;i++) m->normals[i]=gLCSnap.vn[i]; }
    if (!gLCSnap.vu.empty()){ delete[] m->uv; m->uv=new GLfloat[gLCSnap.vsz*2]; for (int i=0;i<gLCSnap.vsz*2;i++) m->uv[i]=gLCSnap.vu[i]; }
    if (!gLCSnap.vc.empty()){ delete[] m->vertexColor; m->vertexColor=new GLubyte[gLCSnap.vsz*4]; for (int i=0;i<gLCSnap.vsz*4;i++) m->vertexColor[i]=gLCSnap.vc[i]; }
    m->faces3d = gLCSnap.f3d; m->materialsGroup = gLCSnap.mg;
    m->looseEdges = gLCSnap.le; m->looseVerts = gLCSnap.lv; // restaurar el perfil suelto (sino desaparece al re-cortar)
    std::vector<GLushort> tris;
    for (size_t f=0;f<m->faces3d.size();f++){ const std::vector<int>& idx=m->faces3d[f].idx;
        for (size_t k=1;k+1<idx.size();k++){ tris.push_back((GLushort)idx[0]);tris.push_back((GLushort)idx[k]);tris.push_back((GLushort)idx[k+1]); } }
    m->facesSize=(int)tris.size(); delete[] m->faces; m->faces=new MeshIndex[m->facesSize>0?m->facesSize:1];
    for (int i=0;i<m->facesSize;i++) m->faces[i]=tris[i];
    m->CalcularBordes(); m->RecalcularNormales();
}

bool LoopCutActivo(){ return gLoopCutOn; }

static void LoopCutActualizarPreview(int mx, int my){
    gLoopCutSegs.clear(); gLoopCutEdge = -1;
    if (!Viewport3DActive || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    int eg = EditPickIndex(SelEdge, mx, my, Viewport3DActive->x, Viewport3DActive->y,
                           Viewport3DActive->width, Viewport3DActive->height, W3dPantallaAlto);
    gLoopCutEdge = eg;
    if (eg>=0) m->LoopCutPreview(eg, gLoopCutCortes, 0.0f, gLoopCutSegs);
}

// cartel-tutorial del loop cut, segun la fase (orientacion / cortes / slide). Se cierra al confirmar/cancelar.
static void LoopCutHint(){
    if (!gLoopCutOn){ NotificarHintClear(); return; }
    if (gLoopCutOrientando) NotificarHint("Loop Cut: tap a yellow dot to pick the cut direction (or arrows + Enter)");
    else if (gLoopCutSlide) NotificarHint("Loop Cut: move = slide, Enter to confirm, Esc = center");
    else                    NotificarHint("Loop Cut: scroll = number of cuts, Enter to confirm");
}

void LoopCutIniciar(int mx, int my){
    if (InteractionMode!=EditMode || !g_editMesh){ Notificar(T("Loop Cut: enter Edit Mode first"), true); return; }
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrientando=false; gLoopCutCortes=1; gLoopCutFactor=0.0f; gLCSnap.tiene=false;
    LoopCutActualizarPreview(mx,my);
    g_redraw=true;
    LoopCutHint();
}

// setup del loop cut sobre un BORDE conocido (direccion automatica). En tactil aplica+panel;
// en PC/Symbian sigue el flujo clasico (rueda = cortes, click aplica + slide).
static void LoopCutIniciarBorde(Mesh* m, int edgeIdx){
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrientando=false;
    gLoopCutCortes=1; gLoopCutFactor=0.0f; gLCSnap.tiene=false;
    gLoopCutEdge = edgeIdx;
    gLoopCutSegs.clear(); m->LoopCutPreview(gLoopCutEdge, gLoopCutCortes, 0.0f, gLoopCutSegs);
    gLoopCutSegs2.clear(); gLoopCutOrientNPts=0;
    g_redraw=true;
    if (LoopCutTactil()){ LoopCutAplicarYPanel(); return; }
    LoopCutHint();
}

// setup del loop cut sobre un QUAD conocido: fase ORIENTACION (2 cortes en 0.5 + 4 puntos para
// elegir direccion con un click/toque; en PC/Symbian tambien flechas + Enter).
static void LoopCutIniciarQuad(Mesh* m, EditMesh* e, int faceIdx){
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrientando=true;
    gLoopCutCortes=1; gLoopCutFactor=0.0f; gLCSnap.tiene=false;
    gLoopCutOrient=0;
    gLoopCutOrientEdge[0] = e->faceEdges[faceIdx][0]; // las 2 direcciones perpendiculares
    gLoopCutOrientEdge[1] = e->faceEdges[faceIdx][1];
    gLoopCutEdge = gLoopCutOrientEdge[0];
    // preview de LAS DOS direcciones (se dibujan ambas en 0.5)
    gLoopCutSegs.clear();  m->LoopCutPreview(gLoopCutOrientEdge[0], gLoopCutCortes, 0.0f, gLoopCutSegs);
    gLoopCutSegs2.clear(); m->LoopCutPreview(gLoopCutOrientEdge[1], gLoopCutCortes, 0.0f, gLoopCutSegs2);
    // 4 PUNTOS = midpoint de cada borde del quad. Bordes 0,2 -> direccion 0; bordes 1,3 -> direccion 1.
    gLoopCutOrientNPts = 0;
    for (int k=0; k<4 && k<(int)e->faceEdges[faceIdx].size(); k++){
        int eg = e->faceEdges[faceIdx][k];
        if (eg<0 || eg*2+1 >= (int)e->lineIdx.size()) continue;
        int a = e->lineIdx[eg*2], b = e->lineIdx[eg*2+1];
        int n = gLoopCutOrientNPts;
        gLoopCutOrientPt[n][0] = (e->pos[a*3+0]+e->pos[b*3+0])*0.5f;
        gLoopCutOrientPt[n][1] = (e->pos[a*3+1]+e->pos[b*3+1])*0.5f;
        gLoopCutOrientPt[n][2] = (e->pos[a*3+2]+e->pos[b*3+2])*0.5f;
        gLoopCutOrientPtDir[n] = (k % 2 == 0) ? 0 : 1; // bordes opuestos 0/2 y 1/3
        gLoopCutOrientNPts++;
    }
    g_redraw=true;
    LoopCutHint();
}

// Loop Cut desde el menu Edge/Face/Vertex, sobre el elemento ACTIVO / la SELECCION (sin hover):
//  - modo BORDE: el borde activo -> el corte cruza ese borde directo (direccion obvia).
//  - modo CARA:  el quad activo  -> fase ORIENTACION (elegir 1 de las 2 direcciones). SOLO quads.
//  - modo VERTICE: por la seleccion. 2 verts = un borde (direccion obvia); 4 verts = un quad
//                  (elegir direccion). Cualquier otra cantidad (3, sueltos, etc.) = invalido.
void LayoutLoopCutDesdeActivo(){
    if (InteractionMode!=EditMode || !g_editMesh){ Notificar(T("Loop Cut: enter Edit Mode first"), true); return; }
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    EditMesh* e = m->edit;
    int act = e->activeIdx;
    if (EditSelectMode == SelEdge){
        if (act < 0 || act >= e->NumEdges()){ Notificar(T("Loop Cut: no active edge (click an edge first)"), true); return; }
        LoopCutIniciarBorde(m, act);
    } else if (EditSelectMode == SelFace){
        if (act < 0 || act >= (int)e->faces.size()){ Notificar(T("Loop Cut: no active face (click a face first)"), true); return; }
        if (e->faces[act].size() != 4){ Notificar(T("Loop Cut: the active face is not a quad"), true); return; } // SOLO quads
        if (act >= (int)e->faceEdges.size() || e->faceEdges[act].size() < 2){ Notificar(T("Loop Cut: the active face has no edges"), true); return; }
        LoopCutIniciarQuad(m, e, act);
    } else { // SelVertex: decide por la SELECCION
        std::vector<int> sel;
        for (int k=0; k<(int)e->vertSel.size(); k++) if (e->vertSel[k]) sel.push_back(k);
        if (sel.size() == 2){
            // 2 verts: el borde que los une -> direccion obvia
            int eg = -1;
            for (int i=0; i<e->NumEdges(); i++){
                int a=e->lineIdx[i*2], b=e->lineIdx[i*2+1];
                if ((a==sel[0]&&b==sel[1]) || (a==sel[1]&&b==sel[0])){ eg=i; break; }
            }
            if (eg<0){ Notificar(T("Loop Cut: the 2 selected vertices are not an edge"), true); return; }
            LoopCutIniciarBorde(m, eg);
        } else if (sel.size() == 4){
            // 4 verts: la cara (quad) cuyos 4 vertices son EXACTAMENTE los seleccionados -> elegir direccion
            int f = -1;
            for (int i=0; i<(int)e->faces.size(); i++){
                if (e->faces[i].size()!=4) continue;
                bool todos=true;
                for (int j=0;j<4;j++){ if (!e->vertSel[e->faces[i][j]]){ todos=false; break; } }
                if (todos){ f=i; break; }
            }
            if (f<0 || f>=(int)e->faceEdges.size() || e->faceEdges[f].size()<2){ Notificar(T("Loop Cut: select the 4 vertices of one quad"), true); return; }
            LoopCutIniciarQuad(m, e, f);
        } else {
            Notificar(T("Loop Cut: select an edge (2 verts) or a quad (4 verts)"), true); // 3 o algo raro = invalido
        }
    }
}

static void LoopCutConfirmar(){
    gLoopCutOn=false; gLoopCutSlide=false; gLoopCutSegs.clear(); gLoopCutSegs2.clear(); gLoopCutOrientNPts=0;
    NotificarHintClear();
    AbrirRedoLoopCutPanel((Mesh*)g_editMesh); // panel redo (cortes + factor); usa el snapshot
    g_redraw=true;
}

// aplica el corte con los defaults (1 corte, factor 0) y abre el panel redo para editar cortes/slide.
// Es el camino de UN CLICK/TOUCH: como no hay flechas ni rueda en tactil, se ajusta desde el panel.
static void LoopCutAplicarYPanel(){
    if (gLoopCutEdge<0 || !g_editMesh){ LoopCutCancelar(); return; }
    Mesh* m=(Mesh*)g_editMesh;
    UndoCapturarMallaGeo(m);           // Ctrl+Z: 1 sola captura, PRE-corte
    LCGuardar(m);
    m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, 0.0f);
    LoopCutConfirmar();                // cierra el modal + abre el panel (cortes + factor)
}

// ENTER en la fase de orientacion (teclado PC/Symbian): confirma la direccion elegida con las
// flechas y sigue al flujo clasico (preview con rueda -> click aplica+slide). El PICKER de puntos
// (click/tactil) NO pasa por aca: ese va directo a aplicar + panel.
void LoopCutOrientConfirmarTeclado(){
    if (!gLoopCutOrientando) return;
    gLoopCutOrientando=false; gLoopCutOrientNPts=0; gLoopCutSegs2.clear();
    gLoopCutSegs.clear();
    if (g_editMesh) ((Mesh*)g_editMesh)->LoopCutPreview(gLoopCutEdge, gLoopCutCortes, 0.0f, gLoopCutSegs);
    g_redraw=true;
    LoopCutHint(); // ayuda de la fase de cortes
}

void LoopCutMotion(int mx, int my){
    if (!gLoopCutOn) return;
    if (gLoopCutOrientando) return; // orientacion: los 2 previews son fijos (no re-pickear por hover)
    if (!gLoopCutSlide){ LoopCutActualizarPreview(mx,my); g_redraw=true; }
    else {
        float f = (float)(mx - gLoopCutSlideX0) / (float)(gLoopCutSlideW*0.4f);
        if (f>1.0f) f=1.0f; if (f<-1.0f) f=-1.0f;
        gLoopCutFactor=f;
        Mesh* m=(Mesh*)g_editMesh; LCRestaurar(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, f);
        g_redraw=true;
    }
}

void LoopCutWheel(int dir){
    if (!gLoopCutOn || gLoopCutSlide || gLoopCutOrientando) return; // rueda = cortes (fase preview)
    gLoopCutCortes += dir; if (gLoopCutCortes<1) gLoopCutCortes=1; if (gLoopCutCortes>32) gLoopCutCortes=32;
    gLoopCutSegs.clear();
    if (gLoopCutEdge>=0 && g_editMesh) ((Mesh*)g_editMesh)->LoopCutPreview(gLoopCutEdge, gLoopCutCortes, 0.0f, gLoopCutSegs);
    g_redraw=true;
}

// FLECHAS durante el modal (PC y Symbian). dir: 0=izq 1=der 2=arriba 3=abajo.
//  orientacion (quad): izq/arriba = direccion 1, der/abajo = direccion 2.
//  cortes (preview):   arriba/derecha = +1, abajo/izquierda = -1.
//  slide:              izquierda/derecha mueve el factor.
void LoopCutTecla(int dir){
    if (!gLoopCutOn || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh;
    if (gLoopCutOrientando){
        gLoopCutOrient = (dir==1 || dir==3) ? 1 : 0;
        gLoopCutEdge = gLoopCutOrientEdge[gLoopCutOrient];
        gLoopCutSegs.clear(); m->LoopCutPreview(gLoopCutEdge, gLoopCutCortes, 0.0f, gLoopCutSegs);
        g_redraw=true;
        return;
    }
    if (!gLoopCutSlide){
        LoopCutWheel((dir==2 || dir==1) ? +1 : -1); // arriba/derecha +1 corte; abajo/izq -1
        return;
    }
    float paso = 0.08f; // slide con flechas: izquierda/derecha
    if (dir==0) gLoopCutFactor -= paso;
    else if (dir==1) gLoopCutFactor += paso;
    else return;
    if (gLoopCutFactor>1.0f) gLoopCutFactor=1.0f;
    if (gLoopCutFactor<-1.0f) gLoopCutFactor=-1.0f;
    LCRestaurar(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, gLoopCutFactor);
    g_redraw=true;
}

// click IZQUIERDO durante el loop cut: en preview aplica+entra al slide; en slide confirma
void LoopCutClickIzq(int mx, int my){
    if (!gLoopCutOn) return;
    if (gLoopCutOrientando){
        // PICKER: elegir la direccion tocando/clickeando el PUNTO mas cercano (uno de los 4
        // midpoints del quad). Con un solo click sacamos la direccion; despues se aplica el
        // corte y se abre el panel (cortes + slide) porque en tactil no hay flechas ni rueda.
        int best = -1; float bestD = 1e18f;
        if (Viewport3DActive && g_editMesh && gLoopCutOrientNPts > 0){
            Mesh* m=(Mesh*)g_editMesh; Matrix4 W; m->GetWorldMatrix(W);
            float lmx = (float)mx - (float)Viewport3DActive->x;
            float lmy = (float)my - (float)Viewport3DActive->y;
            for (int i=0;i<gLoopCutOrientNPts;i++){
                Vector3 wp = W * Vector3(gLoopCutOrientPt[i][0], gLoopCutOrientPt[i][1], gLoopCutOrientPt[i][2]);
                float sx=0, sy=0;
                if (!Viewport3DActive->ProyectarPunto(wp, sx, sy)) continue;
                float dx=sx-lmx, dy=sy-lmy; float d=dx*dx+dy*dy;
                if (d < bestD){ bestD = d; best = i; }
            }
        }
        if (best >= 0){
            gLoopCutOrient = gLoopCutOrientPtDir[best];
            gLoopCutEdge   = gLoopCutOrientEdge[gLoopCutOrient];
            gLoopCutOrientando=false;
            LoopCutAplicarYPanel();   // aplica + abre el panel de edicion (cortes/slide)
        } else {
            // sin puntos proyectables: fallback al confirm clasico (sigue al preview)
            LoopCutOrientConfirmarTeclado();
        }
        g_redraw=true;
        return;
    }
    if (!gLoopCutSlide){
        if (gLoopCutEdge<0 || !g_editMesh){ LoopCutCancelar(); return; }
        Mesh* m=(Mesh*)g_editMesh;
        UndoCapturarMallaGeo(m); // Ctrl+Z: 1 sola captura, PRE-corte (el slide/redo re-cortan sin recapturar)
        LCGuardar(m);
        m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, 0.0f);
        gLoopCutSlide=true; gLoopCutFactor=0.0f; gLoopCutSlideX0=mx;
        gLoopCutSlideW = Viewport3DActive ? Viewport3DActive->width : 800;
        gLoopCutSegs.clear();
        g_redraw=true;
        LoopCutHint();            // ayuda de la fase de slide
    } else LoopCutConfirmar();
}

// click DERECHO: en slide deja el factor en 0 (centro) + confirma; en preview cancela
void LoopCutClickDer(){
    if (!gLoopCutOn) return;
    if (!gLoopCutSlide){ LoopCutCancelar(); return; }
    gLoopCutFactor=0.0f;
    Mesh* m=(Mesh*)g_editMesh; LCRestaurar(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCortes, 0.0f);
    LoopCutConfirmar();
}

void LoopCutCancelar(){
    if (!gLoopCutOn) return;
    if (gLoopCutSlide && gLCSnap.tiene){ LCRestaurar((Mesh*)g_editMesh); gLCSnap.tiene=false; }
    gLoopCutOn=false; gLoopCutSlide=false; gLoopCutSegs.clear();
    NotificarHintClear();
    g_redraw=true;
}

// la usa el panel redo: re-corta desde el snapshot con nuevos parametros
void LoopCutRedoAplicar(int cortes, float factor){
    if (!gLCSnap.tiene || gLoopCutEdge<0 || !g_editMesh) return;
    if (cortes<1) cortes=1;
    gLoopCutCortes=cortes; gLoopCutFactor=factor;
    Mesh* m=(Mesh*)g_editMesh; LCRestaurar(m); m->LoopCutEdit(gLoopCutEdge, cortes, factor);
    g_redraw=true;
}
int   LoopCutGetCortes(){ return gLoopCutCortes; }
float LoopCutGetFactor(){ return gLoopCutFactor; }

// dibuja el preview del corte (segmentos locales) con la matriz de mundo del objeto.
// Lo llama el viewport 3D despues de renderizar la escena.
void LoopCutRenderPreview(){
    if (!gLoopCutOn || !g_editMesh) return;
    if (gLoopCutSegs.empty() && !gLoopCutOrientando) return;
    Mesh* m=(Mesh*)g_editMesh;
    Matrix4 W; m->GetWorldMatrix(W);
    w3dEngine::PushMatrix(); w3dEngine::MultMatrix(W.m);
    w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Texture2D); w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::DisableArray(w3dEngine::NormalArray); w3dEngine::DisableArray(w3dEngine::ColorArray); w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::EnableArray(w3dEngine::VertexArray);

    if (gLoopCutOrientando){
        // FASE ORIENTACION (quad): los DOS cortes posibles en amarillo 0.5 + 4 PUNTOS grandes 100% opacos.
        w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 0.5f); // amarillo semitransparente
        w3dEngine::LineWidth(3.0f);
        if (!gLoopCutSegs.empty()){  w3dEngine::VertexPointer3f(0, &gLoopCutSegs[0]);  w3dEngine::DrawLines((int)(gLoopCutSegs.size()/3)); }
        if (!gLoopCutSegs2.empty()){ w3dEngine::VertexPointer3f(0, &gLoopCutSegs2[0]); w3dEngine::DrawLines((int)(gLoopCutSegs2.size()/3)); }
        w3dEngine::LineWidth(1.0f);
        if (gLoopCutOrientNPts > 0){ // los puntos que se tocan para elegir la direccion
            w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 1.0f); // amarillo opaco
            w3dEngine::PointSize(22.0f);
            w3dEngine::VertexPointer3f(0, &gLoopCutOrientPt[0][0]);
            w3dEngine::DrawPoints(gLoopCutOrientNPts);
            w3dEngine::PointSize(1.0f);
        }
    } else {
        w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 1.0f); // amarillo (como Blender)
        w3dEngine::VertexPointer3f(0, &gLoopCutSegs[0]);
        if (gLoopCutEsPunto){ // corte de un BORDE SUELTO: el corte es un PUNTO en la arista, no una linea
            w3dEngine::PointSize(9.0f);
            w3dEngine::DrawPoints((int)(gLoopCutSegs.size()/3));
            w3dEngine::PointSize(1.0f);
        } else {
            w3dEngine::LineWidth(3.0f);
            w3dEngine::DrawLines((int)(gLoopCutSegs.size()/3));
            w3dEngine::LineWidth(1.0f);
        }
    }
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::PopMatrix();
}

bool ScenePick3D(int mx, int my, int vx, int vy, int vw, int vh, int screenH) {
    if (!SceneCollection) return false;
    // Ctrl+Z: en Object Mode el click va a cambiar la seleccion -> guardar la previa.
    // (la seleccion de sub-elementos en Edit Mode es 2da tanda)
    if (InteractionMode != EditMode) UndoCapturarSeleccion();
    // POSE MODE: el click selecciona un HUESO del armature activo (no un objeto de la escena). Shift = agrega/saca.
    if (InteractionMode == PoseMode && ObjActivo && ObjActivo->getType() == ObjectType::armature && Viewport3DActive) {
        Armature* arm = (Armature*)ObjActivo;
        int b = Viewport3DActive->PickBone(arm, (float)(mx - vx), (float)(my - vy));
        if (!LShiftPressed) for (size_t i = 0; i < arm->bones.size(); i++) arm->bones[i].select = false;
        if (b >= 0) {
            bool nuevo = !(LShiftPressed && arm->bones[b].select); // shift sobre uno ya seleccionado -> lo saca
            arm->bones[b].select = nuevo;
            arm->boneActivo = nuevo ? b : -1;
        } else if (!LShiftPressed) arm->boneActivo = -1;
        g_redraw = true;
        return true;
    }
    // en Edit Mode el click selecciona sub-elementos (no objetos)
    if (InteractionMode == EditMode && g_editMesh) {
        UndoCapturarSeleccionEdit((Mesh*)g_editMesh); // Ctrl+Z: el click va a cambiar la seleccion de sub-elementos
        // modo guiado "Pick Shortest Path" (desde el menu): el click elige 1ro/2do elemento
        if (PickPathClick(mx, my, vx, vy, vw, vh, screenH)) return true;
        if (GuiadoUnClickClick(mx, my, vx, vy, vw, vh, screenH)) return true; // Select Linked / Loop Select guiados (1 click)
        // Loop Select por Alt+Click (el borde de referencia es el que esta bajo el cursor):
        //   Shift+Alt+Click -> en CUALQUIER modo (CARA=Face Loop, BORDE/VERTICE=Edge Loop). El Shift ademas AGREGA.
        //   Ctrl+Alt+Click (modo BORDE) -> Edge Loop tambien (compat con el atajo viejo; sin shift = reemplaza).
        // (Fix Dante: antes Shift+Alt solo andaba en modo cara; en borde/vertice no seleccionaba el loop.)
        if (LAltPressed && (LShiftPressed || (LCtrlPressed && EditSelectMode == SelEdge))) {
            Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
            if (m->edit) {
                int eg = EditPickIndex(SelEdge, mx, my, vx, vy, vw, vh, screenH); // siempre pickea un BORDE de referencia
                if (eg >= 0) {
                    bool soloEste = !LShiftPressed; // Shift = agrega a la seleccion; sino reemplaza
                    if (EditSelectMode == SelFace)      m->edit->SeleccionarLoopFace(eg, soloEste);
                    else if (EditSelectMode == SelEdge) m->edit->SeleccionarLoopEdge(eg, soloEste);
                    else                                m->edit->SeleccionarLoopEdgeVerts(eg, true); // VERTICE: REEMPLAZA (Shift es el atajo, no un "add"; ademas no acumula el edgeSel)
                }
            }
            return true;
        }
        // Ctrl+Click (sin Alt) = Pick Shortest Path desde el ACTIVO hasta el clickeado.
        // +Shift = Fill Region (rellena la region en vez de un solo caminito). Agrega.
        if (LCtrlPressed && !LAltPressed) {
            Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
            if (m->edit) {
                int k = EditPickIndex(EditSelectMode, mx, my, vx, vy, vw, vh, screenH);
                if (k >= 0) m->edit->SeleccionarShortestPath(k, LShiftPressed);
            }
            return true;
        }
        return EditPickVert(mx, my, vx, vy, vw, vh, screenH);
    }

    // misma proyeccion+camara que el Viewport3D REAL (el pick tiene que
    // ver EXACTAMENTE lo que se dibuja)
    {
        int glY = screenH - vy - vh;
        w3dEngine::Viewport(vx, glY, vw, vh);
        w3dEngine::Scissor(vx, glY, vw, vh);
        w3dEngine::Enable(w3dEngine::ScissorTest);
        w3dEngine::MatrixMode(w3dEngine::Projection);
        w3dEngine::LoadIdentity();
        float pkNear = Viewport3DActive ? Viewport3DActive->nearClip : 0.01f;
        float pkFar  = Viewport3DActive ? Viewport3DActive->farClip  : 1000.0f;
        float pkAspect = (float)vw / (float)vh;
        if (Viewport3DActive && Viewport3DActive->orthographic) {
            // orto: mismo volumen que Render() (sino el pick de objetos cae corrido al zoomear en ortografica)
            float size = Viewport3DActive->orbitDistance * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
            if (size < 0.001f) size = 0.001f;
            w3dEngine::Ortho(-size * pkAspect, size * pkAspect, -size, size, pkNear, Viewport3DActive->orbitDistance + pkFar);
        } else {
            w3dEngine::Perspective(fovDeg, pkAspect, pkNear, pkFar);
        }
        w3dEngine::MatrixMode(w3dEngine::ModelView);
        w3dEngine::LoadIdentity();
        if (Viewport3DActive) Viewport3DActive->UpdateViewOrbit();
    }
    w3dEngine::Disable(w3dEngine::Dither);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::ClearColor(0, 0, 0, 1);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    pickCounter = 0;
    PickPaint(SceneCollection, false); // 1ra pasada: NO-armatures (con z-buffer)
    PickPaint(SceneCollection, true);  // 2da pasada: armatures XRAY (sin z, encima) -> se pickean como se ven
    w3dEngine::Enable(w3dEngine::DepthTest); // restaurar (la pasada xray lo dejo off)

    GLubyte pix[4] = {0, 0, 0, 0};
    w3dEngine::ReadPixelsRGBA(mx, screenH - 1 - my, 1, 1, pix);
    w3dEngine::Enable(w3dEngine::Dither);
    w3dEngine::Disable(w3dEngine::ScissorTest);

    w3dEngine::EnableArray(w3dEngine::TexCoordArray); // baseline de la UI

    int id = ((pix[1] >> 2) << 5) | (pix[0] >> 3);

    if (!LShiftPressed) {
        DeseleccionarTodo();
    }
    if (id > 0) {
        pickCounter = 0;
        pickFound = NULL;
        PickResolve(SceneCollection, id, false);              // 1ra pasada: NO-armatures
        if (!pickFound) PickResolve(SceneCollection, id, true); // 2da pasada: armatures (continua el contador)
        if (pickFound) {
            if (LShiftPressed && pickFound->select) {
                pickFound->select = false; // shift: sacar de la seleccion
            } else {
                pickFound->Seleccionar();
            }
        }
    }
    return true; // el frame siguiente redibuja la escena real
}

