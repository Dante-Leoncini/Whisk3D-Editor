// ===================================================================================================
//  PICK / SELECCION 3D: pick por color + pick de malla + Select Linked / Loop Select (guiados) + Loop Cut.
//  Extraido de LayoutInput.cpp (Fase 2 del reorg). Ver Pick3D.h.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmPopup.h" // OpenConfirmDelete (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "edit/MeshEdit.h"   // W3dTriangulateFace
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
#include "ViewPorts/Notifications.h" // toasts (extraido a su propio archivo)
#include "ViewPorts/NumInput.h" // entrada numerica/formulas (extraido a su propio archivo)
#include "ViewPorts/Parent.h" // emparentar/desemparentar (extraido a su propio archivo)
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h" // el velo del modo foco
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h" // W3dMeshFacesView: la seleccion por caja de una malla de un recurso
#include "objects/Materials.h" // Material (mat->texture) para el dropdown "Texture" del UV editor
#include "objects/Textures.h"  // Texture (path) para las etiquetas del dropdown
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertKeyframeSkeleton (Pose Mode: Insert Keyframe)
#include "animation/VertexAnimation.h"   // VtxAnimSnap: los frames de la vertex anim viajan con el snapshot del loop cut
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "edit/BoneEdit.h" // Edit Mode de ARMATURE: click = seleccionar hueso + drag de head/tail (Fase 3)
#include "objects/Primitives.h"
#include "variables.h"
#include "render/RenderOptions.h" // g_currentFps
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/theme/colors.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
#include "ViewPorts/Pick3D.h"
#include "edit/BoxSelect.h"   // la caja compartida + sus predicados
#include "objects/Hitbox.h"   // el pick de un hitbox es su ALAMBRE (HitboxEdges)
#include "objects/LightProbe.h"   // la sonda de luz se elige por su icono (LightProbeDraws)
#include "W3dRoots.h"          // las otras escenas abiertas: el click tambien las elige
#include "io/EditorRoots.h"    // W3dActivateRootOf: lo elegido en otra escena abierta la activa
#include "script/W3dScript.h"  // (jugando no: el Play dibuja su base y sus capas)

// ====================================================================
// pick 3D por color (antes vivia en w3dnewscene.cpp, solo Symbian)
// ====================================================================

static int pickCounter = 0;
static Object* pickFound = NULL;

// tipos seleccionables por click: malla (su geometria) o un objeto con icono
// (camara, luz, empty, instancia) -> un punto del tamaño del icono en su origen
static bool PickSelectable(Object* obj) {
    if (!obj->visible) return false;
    ObjectType t = obj->getType();
    // si el overlay de ese TIPO esta oculto (submenu "Objects"), tampoco se puede pickear (no se ve -> no se clickea).
    extern bool g_showLights, g_showCamera, g_showEmpty;
    if (t == ObjectType::light  && !g_showLights) return false;
    if (t == ObjectType::camera && !g_showCamera) return false;
    if (t == ObjectType::empty  && !g_showEmpty)  return false;
    { extern bool g_showSounds; if (t == ObjectType::sound && !g_showSounds) return false; }   // (overlay "Sounds")
    if (t == ObjectType::hitbox && !HitboxDraws()) return false;   // la MISMA regla que su dibujo
    if ((t == ObjectType::lightProbe || t == ObjectType::reflectionProbe) && !LightProbeDraws()) return false;
    return t == ObjectType::mesh || t == ObjectType::camera ||
           t == ObjectType::light || t == ObjectType::empty ||
           t == ObjectType::instance || t == ObjectType::armature ||
           t == ObjectType::hitbox || t == ObjectType::sound || t == ObjectType::lightProbe || t == ObjectType::reflectionProbe;
}
// pick en 2 pasadas: primero NO-armatures (con z-buffer), despues armatures (XRAY, sin z, encima de todo)
// para que el esqueleto se pickee EXACTAMENTE como se ve. 'armaturePass' filtra que se pinta/cuenta en cada pasada.
static bool PickInPass(Object* obj, bool armaturePass) {
    if (!PickSelectable(obj)) return false;
    return (obj->getType() == ObjectType::armature) == armaturePass;
}

static void PickPaint(Object* obj, bool armaturePass) {
    if (!obj) return;
    // IGUAL que Object::Render: se aplica la matriz LOCAL y los hijos se pintan DENTRO de ella (acumulando el
    // transform del padre). Antes el push/pop envolvia solo a este objeto y los hijos se pintaban en el ORIGEN del
    // mundo -> un objeto EMPARENTADO se pickeaba en el lugar equivocado (no se podia clickear un hijo).
    w3dEngine::PushMatrix();
    Matrix4 M;
    obj->GetMatrix(M);
    w3dEngine::MultMatrix(M.m);
    if (PickInPass(obj, armaturePass)) {
        pickCounter++;
        int id = pickCounter; // 1..N
        w3dEngine::Color4f(((id & 0x1F)) / 31.0f, ((id >> 5) & 0x3F) / 63.0f, 0.0f, 1.0f);
        if (obj->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)obj;
            // El pick tiene que pintar EXACTAMENTE lo que dibuja Mesh::Render (solid), sino el click no coincide.
            // PRIORIDAD (igual que Render): (1) malla GENERADA por modificador (subdiv/screw) -> es lo que se VE
            // (sin esto un Screw sin caras no se podia clickear); (2) malla DEFORMADA por esqueleto (skinVertex) ->
            // el click sigue la animacion (sino cae en el bind: el drama de LISA); (3) el bind crudo.
            if (m->genValid && m->genVertex && m->genFaces && m->genFacesSize > 0) {
                w3dEngine::VertexPointer3f(0, m->genVertex);
                w3dEngine::DrawTriangles(m->genFacesSize, m->genFaces);
            } else if (m->skinArmature && m->faces && m->facesSize > 0) {
                extern void SkinMesh(Mesh*); SkinMesh(m); // asegura skinVertex al frame actual (cacheado)
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
        } else if (obj->getType() == ObjectType::hitbox) {
            // las ARISTAS de la caja, gruesas: se clickea el alambre que se ve y no el volumen (un
            // hitbox que envuelve una puerta no le tapa el click a la puerta)
            float lines[72];
            HitboxEdges((W3dHitboxBase*)obj, lines);
            w3dEngine::LineWidth(5.0f * (float)GlobalScale);
            w3dEngine::VertexPointer3f(0, lines);
            w3dEngine::DrawLines(24);
            w3dEngine::LineWidth(1.0f);
        } else if (obj->getType() == ObjectType::empty && ((Empty*)obj)->shape == EmptyImage) {
            // LA REFERENCIA: la IMAGEN entera se clickea (no solo su origen), de los dos lados
            float c[12]; ((Empty*)obj)->ImageCorners(c);
            static const MeshIndex idx[6] = { 0, 1, 2, 2, 1, 3 };
            const GLboolean cull = w3dEngine::IsEnabled(w3dEngine::CullFace);
            w3dEngine::Disable(w3dEngine::CullFace);
            w3dEngine::VertexPointer3f(0, c);
            w3dEngine::DrawTriangles(6, idx);
            if (cull) w3dEngine::Enable(w3dEngine::CullFace);
        } else {
            // icono (camara/luz/empty/instancia): un punto clickeable en el
            // origen, del MISMO tamaño que el icono 3D (16 * GlobalScale)
            static const GLfloat origin[3] = { 0.0f, 0.0f, 0.0f };
            w3dEngine::PointSize(16.0f * GlobalScale);
            w3dEngine::VertexPointer3f(0, origin);
            w3dEngine::DrawPoints(1);
        }
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        PickPaint(obj->Childrens[i], armaturePass);
    }
    w3dEngine::PopMatrix();
}

static void PickResolves(Object* obj, int target, bool armaturePass) {
    if (!obj || pickFound) return;
    if (PickInPass(obj, armaturePass)) {
        pickCounter++;
        if (pickCounter == target) { pickFound = obj; return; }
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        PickResolves(obj->Childrens[i], target, armaturePass);
    }
}

// hubo input TACTIL en la sesion? (para agrandar el pick de vertices en pantallas tactiles)
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controls.cpp
static bool PickIsTouch(){ return g_lastFingerTicks != 0; }
#else
static bool PickIsTouch(){ return false; }
#endif

// EDIT MODE: pick de VERTICE o ARISTA por color-ID (segun EditSelectMode). Dibuja
// los sub-elementos con un color = (id+1) en R5G6B5 (tolera framebuffer 16-bit:
// hasta 65535), lee el pixel bajo el click y resuelve. Sin shift = solo ese; con
// shift = toggle. Usa la EditMesh (datos de edicion separados de la malla de render).
// Pickea por color-ID el sub-elemento bajo (mx,my) en el MODO dado (SelVertex/Edge/
// Face, NO necesariamente el modo activo: el loop-select pickea un BORDE en modo cara).
// Devuelve el indice 0-based o -1. NO modifica la seleccion.
// X-RAY + CARAS: el pick por color-ID no sirve. Con X-Ray las caras de atras se ven, pero el
// id-buffer sigue teniendo el z-test: clickeabas una cara del fondo y agarrabas la de adelante.
// La regla que corresponde es la que se VE: gana el PUNTITO (el centro de cara que dibuja la
// jaula) MAS CERCA del mouse, sin importar la profundidad -- que es exactamente lo que el ojo
// esta apuntando. Sin X-Ray no cambia nada: ahi el id-buffer es lo correcto (tapa lo de atras).
static int EditPickFaceXRay(Mesh* m, EditMesh* e, int mx, int my) {
    if (!Viewport3DActive || !e || e->faceCenter.empty()) return -1;
    Viewport3DActive->BindView();
    Matrix4 W; m->GetWorldMatrix(W);
    const int nF = e->NumFaces();
    const float radius = 40.0f * (float)GlobalScale;   // hasta donde se considera "apuntado"
    int best = -1; float bestD2 = radius * radius;
    for (int f = 0; f < nF && (size_t)(f*3+2) < e->faceCenter.size(); f++) {
        float sx, sy;
        Vector3 w = W * Vector3(e->faceCenter[f*3], e->faceCenter[f*3+1], e->faceCenter[f*3+2]);
        if (!Viewport3DActive->ProjectPoint(w, sx, sy)) continue;
        const float dx = sx - (float)mx, dy = sy - (float)my;
        const float d2 = dx*dx + dy*dy;
        if (d2 < bestD2) { bestD2 = d2; best = f; }
    }
    return best;
}

static int EditPickIndex(int mode, int mx, int my, int vx, int vy, int vw, int vh, int screenH) {
    Mesh* m = (Mesh*)g_editMesh;
    if (!m) return -1;
    m->EnsureEdit();
    EditMesh* e = m->edit;
    if (!e || e->pos.empty()) return -1;
    if (g_xray && mode == SelFace) return EditPickFaceXRay(m, e, mx, my);
    const bool edgeMode = (mode == SelEdge);
    const bool faceMode = (mode == SelFace);
    const int N = faceMode ? e->NumFaces() : edgeMode ? e->NumEdges() : e->NumVerts();
    if (N <= 0) return -1;

    // misma proyeccion + camara que el viewport real
    int glY = screenH - vy - vh;
    w3dEngine::Viewport(vx, glY, vw, vh);
    w3dEngine::Scissor(vx, glY, vw, vh);
    w3dEngine::Enable(w3dEngine::ScissorTest);
    // LA proyeccion del viewport (la misma funcion que usa su Render): mirando por la camara, la lente de la CAMARA con
    // su marco. Antes se armaba aca con la del visor: desde la camara el click caia en otro lado (una luz no se podia
    // elegir; un cubo grande en el centro, de casualidad si)
    if (Viewport3DActive) Viewport3DActive->LoadProjectionView();
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
    if (!faceMode && !edgeMode && PickIsTouch()) RAD = 16;
    const int WD = 2 * RAD + 1;
    int cxw = mx, cyw = screenH - 1 - my; // centro en coords de ventana (GL)
    int x0 = cxw - RAD, y0 = cyw - RAD;
    if (x0 < vx) x0 = vx; if (x0 + WD > vx + vw) x0 = vx + vw - WD;
    if (y0 < glY) y0 = glY; if (y0 + WD > glY + vh) y0 = glY + vh - WD;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    w3dEngine::ReadPixelsBegin(x0, y0, WD, WD);   // (antes de dibujar: la Dreamcast dibuja la pasada por software)

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
        // (con caras OCULTAS -H- tapan solo las que se ven)
        if (e->numHidden > 0) { if (!e->visTris.empty()) w3dEngine::DrawTriangles((int)e->visTris.size(), &e->visTris[0]); }
        else w3dEngine::DrawTriangles(m->facesSize, m->faces);
        w3dEngine::Disable(w3dEngine::PolygonOffsetFill);
        w3dEngine::ColorMask(true, true, true, true);
        w3dEngine::EnableArray(w3dEngine::ColorArray);
    }
#if defined(W3D_SYMBIAN) && defined(W3D_GLES2)
    // el N8 (VideoCore III, z de 16 bits) no aplica bien el polygon offset de la oclusion: el vertice/arista quedaba
    // TAPADO por su propia cara y no se podia clickear. El mismo arreglo que la jaula (EditMesh::Render): los empates
    // pasan y un sesgo chico hacia la camara
    w3dEngine::DepthFunc(w3dEngine::DepthLEqual);
    w3dEngine::BiasMeters(-0.02f);
#endif

    if (faceMode) {
        // caras: triangulos EXPANDIDOS con color-ID por cara (vertices propios por
        // cara: un vertice compartido no puede tener 2 colores de cara distintos)
        std::vector<GLfloat> tp; std::vector<GLubyte> tc;
        for (int f = 0; f < N; f++) {
            int id = f + 1;
            GLubyte r=(GLubyte)((id&0x1F)<<3), g=(GLubyte)(((id>>5)&0x3F)<<2), b=(GLubyte)(((id>>11)&0x1F)<<3);
            const IntRows::Row p = e->faces[(size_t)f];
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
        w3dEngine::PointSize(PickIsTouch() ? 28.0f : 22.0f);
        w3dEngine::DrawPoints(N);
        w3dEngine::PointSize(1.0f);
    }

    GLubyte area[33 * 33 * 4]; // dimensionado para el WD mas grande (vertice+touch: 33); otros modos usan menos
    w3dEngine::ReadPixelsRGBA(x0, y0, WD, WD, area);
#if defined(W3D_SYMBIAN) && defined(W3D_GLES2)
    w3dEngine::BiasMeters(0.0f);
    w3dEngine::DepthFunc(w3dEngine::DepthLess);
#endif
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
            if (e->Hidden(mode, cand - 1)) continue;   // (lo oculto con H no se elige)
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
        if (EditSelectMode == SelFace)      e->ToggleFace(k, !LShiftPressed);
        else if (EditSelectMode == SelEdge) e->ToggleEdge(k, !LShiftPressed);
        else                                e->ToggleVert(k, !LShiftPressed);
    } else if (!LShiftPressed) {
        e->SelectAll(false); // click al vacio sin shift: deseleccionar todo
    }
    return true; // el frame siguiente redibuja la escena real
}

// ===== Navegacion de seleccion por TECLADO en Edit Mode (lapiz + flechas de Symbian) =====
// Opera sobre vert/edge/face segun EditSelectMode, tocando el array de seleccion + activeIdx;
// Recolor() refresca el visual (incluye el relleno de caras). Devuelve false si no aplica.
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
// flechas de Symbian). El "activo" es ActiveObj; el "array" es la lista plana de objetos
// (sin colecciones) en orden de outliner. Select()/Deselect() mantienen select +
// ObjSelects + ActiveObj. NO tocar la seleccion durante un transform (estado != editNavigation,
// como DeselectAll: objectState apunta a los seleccionados y crashea si cambia).
static void ObjSelCollect(Object* node, std::vector<Object*>& out){
    if (!node) return;
    for (size_t i=0;i<node->Childrens.size();i++){
        Object* h = node->Childrens[i];
        if (!h) continue;
        if (h->getType() != ObjectType::collection) out.push_back(h); // los objetos, no colecciones
        ObjSelCollect(h, out);                                        // recursivo (sub-colecciones)
    }
}
static bool ObjSelList(std::vector<Object*>& v){
    if (!SceneCollection) return false;
    ObjSelCollect(SceneCollection, v);
    return !v.empty();
}
static int ActiveObjSelIdx(const std::vector<Object*>& v){
    for (size_t i=0;i<v.size();i++) if (v[i]==ActiveObj) return (int)i;
    return -1;
}
static bool ObjSelAdvance(int step, bool extend){
    if (state != editNavigation) return true;        // no durante un transform
    std::vector<Object*> v; if (!ObjSelList(v)) return true;
    int N=(int)v.size();
    int act = ActiveObjSelIdx(v);
    if (act<0){ act=0; for (int i=0;i<N;i++) if (v[i]->select){ act=i; break; } } // sin activo: 1er sel (o 0)
    if (!extend) v[act]->Deselect();           // deselecciona SOLO el activo (el resto queda)
    int next = act + step; if (next<0) next=N-1; if (next>=N) next=0;
    v[next]->Select();                            // select=true + ActiveObj=v[next]
    g_redraw = true;
    return true;
}
static bool ObjSelAllToggle(){
    if (state != editNavigation) return true;
    std::vector<Object*> v; if (!ObjSelList(v)) return true;
    int N=(int)v.size();
    bool all = true; for (int i=0;i<N;i++) if (!v[i]->select){ all=false; break; }
    if (all){ for (int i=0;i<N;i++) v[i]->Deselect(); ActiveObj = NULL; }
    else     { for (int i=0;i<N;i++) v[i]->Select(); ActiveObj = v[0]; }
    g_redraw = true;
    return true;
}
static bool ObjSelToggleCurrent(){
    if (state != editNavigation) return true;
    std::vector<Object*> v; if (!ObjSelList(v)) return true;
    int N=(int)v.size();
    int act = ActiveObjSelIdx(v);
    if (act<0){ act=0; for (int i=0;i<N;i++) if (v[i]->select){ act=i; break; } if (!v[act]->select) return true; }
    if (v[act]->select){ v[act]->Deselect(); ActiveObj = NULL; }
    else                 v[act]->Select();        // Seleccionar setea ActiveObj
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
static Armature* ActiveSelArm(){
    return (InteractionMode == PoseMode && ActiveObj && ActiveObj->getType() == ObjectType::armature)
           ? (Armature*)ActiveObj : NULL;
}
static bool PoseSelAdvance(Armature* a, int step, bool extend){
    int N = (int)a->bones.size(); if (N <= 0) return true;
    int act = a->activeBone;
    if (act < 0 || act >= N){ act = 0; for (int i=0;i<N;i++) if (a->bones[i].select){ act=i; break; } }
    if (!extend) a->bones[act].select = false;   // deselecciona el activo
    int next = act + step;
    if (next < 0) next = N-1;
    if (next >= N) next = 0;
    a->bones[next].select = true;                  // selecciona el siguiente (si ya estaba, queda)
    a->activeBone = next;
    g_redraw = true;
    return true;
}
static bool PoseSelAllToggle(Armature* a){
    int N = (int)a->bones.size(); if (N <= 0) return true;
    bool all = true; for (int i=0;i<N;i++) if (!a->bones[i].select){ all=false; break; }
    for (int i=0;i<N;i++) a->bones[i].select = all ? false : true;
    a->activeBone = all ? -1 : 0;
    g_redraw = true;
    return true;
}

bool EditSelAdvance(int step, bool extend){
    Armature* pa = ActiveSelArm(); if (pa) return PoseSelAdvance(pa, step, extend);
    if (InteractionMode != EditMode) return ObjSelAdvance(step, extend);
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    int act = e->activeIdx;
    if (act<0 || act>=N){ act=0; for (int i=0;i<N;i++) if (sel[i]){ act=i; break; } } // sin activo: 1er sel (o 0)
    if (!extend) sel[act] = 0;          // deselecciona el activo
    int next = act + step;
    if (next < 0) next = N-1;
    if (next >= N) next = 0;
    sel[next] = 1;                        // selecciona el siguiente (si ya estaba, queda)
    e->activeIdx = next;
    e->Recolor(); g_redraw = true;
    return true;
}
// lapiz+arriba: si TODO esta seleccionado -> nada; sino -> todo.
bool EditSelAllToggle(){
    Armature* pa = ActiveSelArm(); if (pa) return PoseSelAllToggle(pa);
    if (InteractionMode != EditMode) return ObjSelAllToggle();
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    bool all = true; for (int i=0;i<N;i++) if (!sel[i]){ all=false; break; }
    for (int i=0;i<N;i++) sel[i] = all ? 0 : 1;
    e->activeIdx = all ? -1 : 0;
    e->Recolor(); g_redraw = true;
    return true;
}
// lapiz+abajo: togglea el indice ACTIVO. Si lo deselecciona, pierde el activo.
bool EditSelToggleCurrent(){
    if (InteractionMode != EditMode) return ObjSelToggleCurrent();
    EditMesh* e = EditSelMesh(); if (!e) return false;
    int N=0; unsigned char* sel = EditSelArray(e, N);
    if (!sel || N<=0) return true;
    int act = e->activeIdx;
    if (act<0 || act>=N){ act=0; for (int i=0;i<N;i++) if (sel[i]){ act=i; break; } if (act<0) return true; }
    sel[act] = sel[act] ? 0 : 1;
    e->activeIdx = sel[act] ? act : -1;
    e->Recolor(); g_redraw = true;
    return true;
}

// L: Select Linked — selecciona la ISLA (componente conexa) bajo el mouse, en el
// modo activo. Sin shift reemplaza; con shift agrega. Usa el rect del viewport activo.
void LayoutSelectLinked(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh || !Viewport3DActive) return;
    Mesh* m = (Mesh*)g_editMesh;
    m->EnsureEdit();
    if (!m->edit) return;
    UndoCaptureSelectionEdit(m); // Ctrl+Z: Select Linked va a cambiar la seleccion
    int vx = Viewport3DActive->x, vy = Viewport3DActive->y;
    int vw = Viewport3DActive->width, vh = Viewport3DActive->height;
    int k = EditPickIndex(EditSelectMode, mx, my, vx, vy, vw, vh, W3dScreenHeight);
    // SUMA a lo que ya estaba elegido (pedido del duenio, 2026-11-27: "no deselecciones lo que ya este seleccionado")
    if (k >= 0) { m->edit->SelectLinked(k, EditSelectMode, false); g_redraw = true; }
}

// ===== LOOP SELECT desde el sub-elemento ACTIVO (menu Select / Symbian lapiz+OK) =====
// Modo BORDE: el borde activo da la direccion (sin modal). Modo CARA: una cara tiene 2 sentidos,
// asi que arranca un modal de orientacion: las flechas alternan, OK/click/enter acepta. La
// seleccion resaltada ES el preview (no hace falta geometria aparte).
static bool gLoopSelOrient = false;
static int  gLoopSelFace   = -1;
static int  gLoopSelDir    = 0;   // 0 / 1 (faceEdges[face][0] o [1])
bool LoopSelOrienting(){ return gLoopSelOrient; }

void LayoutLoopSelectGuided(); // definida mas abajo (modo guiado: pide click cuando no hay elemento activo)
void LayoutLoopSelectActive(int type){
    if (InteractionMode != EditMode || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    EditMesh* e=m->edit;
    if (type == 2){                                   // FACE LOOP desde la cara activa
        int act = e->activeIdx;
        if (act<0) for (size_t f=0;f<e->faceSel.size();f++) if (e->faceSel[f]){ act=(int)f; break; }
        if (act<0){ LayoutLoopSelectGuided(); return; }  // SIN cara activa -> modo guiado (pedi click o cancela)
        if (act>=(int)e->faceEdges.size() || e->faceEdges[act].size()<2) return;
        UndoCaptureSelectionEdit(m); // Ctrl+Z: el loop select (+ refinar direccion con flechas) = 1 sola accion
        gLoopSelFace = act; gLoopSelDir = 0; gLoopSelOrient = true;
        e->SelectLoopFace(e->faceEdges[act][0], true);
        NotifyHint(T("Loop Select: arrows = direction, Enter to confirm"));
    } else {                                          // EDGE LOOP / RING desde el borde activo
        int act = e->activeIdx;
        if (act<0) for (size_t k=0;k<e->edgeSel.size();k++) if (e->edgeSel[k]){ act=(int)k; break; }
        if (act<0){ LayoutLoopSelectGuided(); return; }  // SIN borde activo -> modo guiado
        UndoCaptureSelectionEdit(m);
        if (type==0) e->SelectLoopEdge(act, true);
        else         e->SelectRingEdge(act, true);
    }
    g_redraw = true;
}
void LoopSelKey(int dir){
    if (!gLoopSelOrient || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; if (!m->edit) return;
    EditMesh* e=m->edit;
    if (gLoopSelFace<0 || gLoopSelFace>=(int)e->faceEdges.size() || e->faceEdges[gLoopSelFace].size()<2) return;
    gLoopSelDir = (dir==1 || dir==3) ? 1 : 0;
    e->SelectLoopFace(e->faceEdges[gLoopSelFace][gLoopSelDir], true);
    g_redraw = true;
}
void LoopSelConfirm(){ gLoopSelOrient = false; NotifyHintClear(); g_redraw = true; }

// Loop select en una POSICION (cursor): pickea el borde bajo (mx,my) y aplica el loop del modo
// (cara -> Face Loop ; borde -> Edge Loop). El cursor da la direccion, asi que NO hay modal.
// Lo usa el mouse virtual de Symbian (lapiz + OK). Devuelve false si no pickeo nada.
bool LayoutLoopSelectInPos(int mx, int my, int vx, int vy, int vw, int vh, int screenH){
    if (InteractionMode != EditMode || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return false;
    int eg = EditPickIndex(SelEdge, mx, my, vx, vy, vw, vh, screenH); // siempre pickea un BORDE de referencia
    if (eg < 0) return false;
    UndoCaptureSelectionEdit(m); // Ctrl+Z: loop select por cursor (Symbian) va a cambiar la seleccion
    EditMesh* ed = m->edit;
    if (EditSelectMode == SelFace)      ed->SelectLoopFace(eg, true);
    else if (EditSelectMode == SelEdge) ed->SelectLoopEdge(eg, true);
    else                                ed->SelectLoopEdgeVerts(eg, true); // VERTICE: verts del loop, sin acumular edgeSel
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
bool PickPathGuided(){ return gPathGuided; }

static const char* PathElemName(){
    if (EditSelectMode == SelFace) return "face";
    if (EditSelectMode == SelEdge) return "edge";
    return "vertex";
}
void LayoutPickPathStart(bool fill){
    if (InteractionMode != EditMode || !g_editMesh) return;
    gPathGuided = true; gPathFill = fill; gPathStep = 0;
    NotifyHint(std::string(T("Pick Shortest Path: click the first")) + " " + T(PathElemName()));
    g_redraw = true;
}
void LayoutPickPathCancel(){
    if (!gPathGuided) return;
    gPathGuided = false; NotifyHintClear(); g_redraw = true;
}
// click durante el modo guiado: true si lo consumio (ScenePick3D NO sigue con el pick normal).
static bool PickPathClick(int mx,int my,int vx,int vy,int vw,int vh,int screenH){
    if (!gPathGuided || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit){ gPathGuided=false; NotifyHintClear(); return true; }
    int k = EditPickIndex(EditSelectMode, mx,my,vx,vy,vw,vh,screenH);
    if (k < 0) return true;   // click al vacio: ignora (sigue esperando el mismo paso)
    if (gPathStep == 0){
        if (EditSelectMode==SelFace)      m->edit->ToggleFace(k, true);  // selecciona el primero (activo)
        else if (EditSelectMode==SelEdge) m->edit->ToggleEdge(k, true);
        else                              m->edit->ToggleVert(k, true);
        gPathStep = 1;
        NotifyHint(std::string(T("Now click the second")) + " " + T(PathElemName()));
    } else {
        m->edit->SelectShortestPath(k, gPathFill);
        gPathGuided = false;
        NotifyHintClear();
        Notify(T(gPathFill ? "Shortest path: region filled" : "Shortest path selected"), false);
    }
    g_redraw = true;
    return true;
}

// ============================================================================
//  BOX SELECT aplicado al viewport 3D. La caja y sus predicados son compartidos
//  (edit/BoxSelect.h); aca se decide QUE entra, que es lo unico propio del 3D.
//
//  LA REGLA, por tipo de elemento (pedido del dueno):
//    VERDE (left->right): entra lo que esta ENTERO adentro. Un pixel afuera y no entra.
//    AZUL  (right->left): alcanza con ROZAR.
//  Un VERTICE es un punto: esta o no esta, y ahi los dos colores dan lo mismo.
//  Una ARISTA o una CARA tienen extension: en azul basta que la caja cruce la
//  arista (aunque las dos puntas queden afuera), en verde tienen que entrar todas
//  sus puntas. Un OBJETO entra en verde solo si TODOS sus vertices proyectados
//  caen adentro -- y como el rect es convexo, eso es exactamente "ni un pixel
//  afuera": si las puntas de cada triangulo estan adentro, el triangulo tambien.
// ============================================================================
static bool BoxObjectEnters(Viewport3D* vp, Object* o, int x0, int y0, int x1, int y1, bool touch) {
    if (o->getType() != ObjectType::mesh) {
        // sin geometria (camara, luz, empty, hueso): su origen es todo lo que hay
        float sx, sy;
        if (!vp->ProjectPoint(o->GetGlobalPosition(), sx, sy)) return false;
        return BoxPoint(x0, y0, x1, y1, sx - vp->x, sy - vp->y);
    }
    Mesh* m = (Mesh*)o;
    if (m->vertexSize <= 0 || !m->vertex) {
        float sx, sy;
        if (!vp->ProjectPoint(o->GetGlobalPosition(), sx, sy)) return false;
        return BoxPoint(x0, y0, x1, y1, sx - vp->x, sy - vp->y);
    }
    Matrix4 W; m->GetWorldMatrix(W);
    const GLfloat* pos = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex; // la pose visible
    static std::vector<float> px, py; static std::vector<char> vis;
    px.assign((size_t)m->vertexSize, 0.0f); py.assign((size_t)m->vertexSize, 0.0f);
    vis.assign((size_t)m->vertexSize, 0);
    bool anyInside = false, allInside = true;
    for (int i = 0; i < m->vertexSize; i++) {
        float sx, sy;
        Vector3 w = W * Vector3(pos[i*3], pos[i*3+1], pos[i*3+2]);
        if (!vp->ProjectPoint(w, sx, sy)) { allInside = false; continue; } // detras de camara
        px[(size_t)i] = sx - vp->x; py[(size_t)i] = sy - vp->y; vis[(size_t)i] = 1;
        if (BoxPoint(x0, y0, x1, y1, px[(size_t)i], py[(size_t)i])) anyInside = true;
        else allInside = false;
    }
    if (!touch) return allInside;      // VERDE: entero adentro
    if (anyInside) return true;        // AZUL: con una punta adentro ya esta
    // ...o que la caja CRUCE alguna arista, o que haya quedado ENTERA adentro de una cara
    // (ahi no toca ningun vertice ni cruza nada, pero esta encima del objeto igual)
    const float cxBox = (float)(x0 + x1) * 0.5f, cyBox = (float)(y0 + y1) * 0.5f;
    static std::vector<float> polX, polY;
    // las caras de la malla, o las COMPARTIDAS de su recurso si su edicion sigue pendiente
    // (objects/MeshResource.h): la misma prueba que con la malla editada, sin cargar su edicion
    const std::vector<MeshFace>* pF = W3dMeshFacesView(m);
    // sin caras (escenario cerrado a edicion, o una topologia que no se pudo leer): alcanzan los
    // triangulos del render (mismo resultado: la caja cruza o cae en una cara)
    if (!pF && m->faces && m->facesSize >= 3) {
        for (int t = 0; t + 2 < m->facesSize; t += 3) {
            const int tri[3] = { (int)m->faces[t], (int)m->faces[t + 1], (int)m->faces[t + 2] };
            polX.clear(); polY.clear();
            bool faceVisible = true;
            for (int k = 0; k < 3; k++) {
                const int a = tri[k], b = tri[(k + 1) % 3];
                if (a < 0 || b < 0 || a >= m->vertexSize || b >= m->vertexSize) { faceVisible = false; continue; }
                if (!vis[(size_t)a] || !vis[(size_t)b]) { faceVisible = false; continue; }
                if (BoxSegment(x0, y0, x1, y1, px[(size_t)a], py[(size_t)a], px[(size_t)b], py[(size_t)b]))
                    return true;
                polX.push_back(px[(size_t)a]); polY.push_back(py[(size_t)a]);
            }
            if (faceVisible && BoxPointInPolygon(&polX[0], &polY[0], (int)polX.size(), cxBox, cyBox))
                return true;
        }
        return false;
    }
    if (!pF) return false;
    const std::vector<MeshFace>& F = *pF;
    for (size_t f = 0; f < F.size(); f++) {
        const std::vector<int>& id = F[f].idx;
        polX.clear(); polY.clear();
        bool faceVisible = true;
        for (size_t k = 0; k < id.size(); k++) {
            const int a = id[k], b = id[(k + 1) % id.size()];
            if (a < 0 || b < 0 || a >= m->vertexSize || b >= m->vertexSize) { faceVisible = false; continue; }
            if (!vis[(size_t)a] || !vis[(size_t)b]) { faceVisible = false; continue; }
            if (BoxSegment(x0, y0, x1, y1, px[(size_t)a], py[(size_t)a], px[(size_t)b], py[(size_t)b]))
                return true;
            polX.push_back(px[(size_t)a]); polY.push_back(py[(size_t)a]);
        }
        if (faceVisible && BoxPointInPolygon(&polX[0], &polY[0], (int)polX.size(), cxBox, cyBox))
            return true;
    }
    return false;
}

static void BoxCollect(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* c = o->Childrens[i];
        if (!c) continue;
        if (c->getType() != ObjectType::collection && c->visible) out.push_back(c);
        BoxCollect(c, out);   // las colecciones no se seleccionan, pero sus hijos si
    }
}

void BoxSelectApply3D(int x0, int y0, int x1, int y1, bool touch, bool add) {
    Viewport3D* vp = Viewport3DActive;
    if (!vp) return;
    vp->BindView();                 // proyectar con ESTA vista (multi-viewport)
    // el rect llega en pantalla; ProjectPoint devuelve pantalla tambien, pero los
    // predicados trabajan en LOCALES del viewport (como el resto de los overlays)
    const int lx0 = x0 - vp->x, ly0 = y0 - vp->y, lx1 = x1 - vp->x, ly1 = y1 - vp->y;

    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        EditMesh* e = m->edit;
        if (!e || e->pos.empty()) return;
        UndoCaptureSelectionEdit(m);            // Ctrl+Z, igual que All/None/Invert
        Matrix4 W; m->GetWorldMatrix(W);
        const int nV = e->NumVerts();
        static std::vector<float> px, py; static std::vector<char> vis;
        px.assign((size_t)nV, 0.0f); py.assign((size_t)nV, 0.0f); vis.assign((size_t)nV, 0);
        for (int k = 0; k < nV; k++) {
            float sx, sy;
            Vector3 w = W * Vector3(e->pos[k*3], e->pos[k*3+1], e->pos[k*3+2]);
            if (!vp->ProjectPoint(w, sx, sy)) continue;
            px[(size_t)k] = sx - vp->x; py[(size_t)k] = sy - vp->y; vis[(size_t)k] = 1;
        }
        // ---- QUE SE VE: sin X-Ray la caja agarra SOLO lo que esta de cara a la camara ----
        // El criterio es el AREA CON SIGNO del poligono proyectado: si da vuelta al otro lado,
        // esa cara mira para atras y lo que hay ahi no se ve. De ahi sale la visibilidad de sus
        // aristas y vertices (un vertice se ve si alguna de sus caras se ve). Con X-Ray prendido
        // no se filtra nada: justamente el modo esta para agarrar lo de atras.
        // (una malla SIN caras -los puntos y manijas de una curva, un path- no tiene nada que la tape a si misma:
        // con el filtro ningun vertice "se veia" y la caja no agarraba nada)
        const bool filterHidden = !g_xray && e->NumFaces() > 0;   // el X-Ray del menu Overlays (global, ya existia)
        std::vector<char> vertVis, edgeVis, faceVis;
        if (filterHidden) {
            vertVis.assign((size_t)nV, 0);
            edgeVis.assign((size_t)e->NumEdges(), 0);
            faceVis.assign((size_t)e->NumFaces(), 0);
            for (int f = 0; f < e->NumFaces(); f++) {
                const IntRows::Row id = e->faces[(size_t)f];
                if (id.size() < 3) continue;
                float area = 0.0f;
                bool allVis = true;
                for (size_t k = 0; k < id.size(); k++) {
                    const int a = id[k], b = id[(k + 1) % id.size()];
                    if (a < 0 || b < 0 || a >= nV || b >= nV || !vis[(size_t)a] || !vis[(size_t)b]) { allVis = false; break; }
                    area += px[(size_t)a] * py[(size_t)b] - px[(size_t)b] * py[(size_t)a];
                }
                if (!allVis || area >= 0.0f) continue;   // mira para el otro lado (o no proyecta)
                faceVis[(size_t)f] = 1;
                for (size_t k = 0; k < id.size(); k++)
                    if (id[k] >= 0 && id[k] < nV) vertVis[(size_t)id[k]] = 1;
                if (f < (int)e->faceEdges.size())
                    for (size_t k = 0; k < e->faceEdges[(size_t)f].size(); k++) {
                        const int ei = e->faceEdges[(size_t)f][k];
                        if (ei >= 0 && ei < (int)edgeVis.size()) edgeVis[(size_t)ei] = 1;
                    }
            }
        }
        if (!add) e->SelectAll(false);   // sin Shift la caja REEMPLAZA la seleccion
        if (EditSelectMode == SelVertex) {
            // un punto esta o no esta: verde y azul dan lo mismo
            for (int k = 0; k < nV; k++) {
                if (!vis[(size_t)k]) continue;
                if (filterHidden && !vertVis[(size_t)k]) continue;   // esta atras: no se ve
                if (BoxPoint(lx0, ly0, lx1, ly1, px[(size_t)k], py[(size_t)k]))
                    e->vertSel[(size_t)k] = 1;
            }
        } else if (EditSelectMode == SelEdge) {
            const int nE = e->NumEdges();
            for (int i = 0; i < nE; i++) {
                const int a = e->lineIdx[(size_t)i*2], b = e->lineIdx[(size_t)i*2+1];
                if (a < 0 || b < 0 || a >= nV || b >= nV) continue;
                const bool va = vis[(size_t)a] != 0, vb = vis[(size_t)b] != 0;
                bool enters;
                if (!touch) {   // VERDE: las DOS puntas adentro
                    enters = va && vb &&
                            BoxPoint(lx0, ly0, lx1, ly1, px[(size_t)a], py[(size_t)a]) &&
                            BoxPoint(lx0, ly0, lx1, ly1, px[(size_t)b], py[(size_t)b]);
                } else {        // AZUL: que la caja toque la arista en cualquier punto
                    enters = va && vb &&
                            BoxSegment(lx0, ly0, lx1, ly1, px[(size_t)a], py[(size_t)a],
                                        px[(size_t)b], py[(size_t)b]);
                }
                if (enters && (!filterHidden || edgeVis[(size_t)i])) e->edgeSel[(size_t)i] = 1;
            }
        } else { // SelFace
            const int nF = e->NumFaces();
            // X-RAY: la cara se agarra por su PUNTITO (el centro), no por su superficie. Es lo
            // mismo que el click: con X-Ray lo que se ve y se apunta son los puntitos, asi que
            // una caja que pasa por encima de la cara pero no de su centro NO la agarra.
            if (g_xray && !e->faceCenter.empty()) {
                for (int f = 0; f < nF && (size_t)(f*3+2) < e->faceCenter.size(); f++) {
                    float sx, sy;
                    Vector3 w = W * Vector3(e->faceCenter[f*3], e->faceCenter[f*3+1], e->faceCenter[f*3+2]);
                    if (!vp->ProjectPoint(w, sx, sy)) continue;
                    if (BoxPoint(lx0, ly0, lx1, ly1, sx - vp->x, sy - vp->y)) e->faceSel[(size_t)f] = 1;
                }
                e->Recolor();
                g_redraw = true;
                return;
            }
            for (int f = 0; f < nF; f++) {
                const IntRows::Row id = e->faces[(size_t)f];
                if (id.empty()) continue;
                bool all = true, any = false, crosses = false;
                for (size_t k = 0; k < id.size(); k++) {
                    const int a = id[k];
                    if (a < 0 || a >= nV || !vis[(size_t)a]) { all = false; continue; }
                    if (BoxPoint(lx0, ly0, lx1, ly1, px[(size_t)a], py[(size_t)a])) any = true;
                    else all = false;
                    const int b = id[(k + 1) % id.size()];
                    if (b >= 0 && b < nV && vis[(size_t)b] &&
                        BoxSegment(lx0, ly0, lx1, ly1, px[(size_t)a], py[(size_t)a],
                                    px[(size_t)b], py[(size_t)b])) crosses = true;
                }
                bool inside = false;   // la caja ENTERA adentro de esta cara
                if (touch && !any && !crosses) {
                    static std::vector<float> polX, polY;
                    polX.clear(); polY.clear();
                    bool okPol = true;
                    for (size_t k = 0; k < id.size(); k++) {
                        const int a = id[k];
                        if (a < 0 || a >= nV || !vis[(size_t)a]) { okPol = false; break; }
                        polX.push_back(px[(size_t)a]); polY.push_back(py[(size_t)a]);
                    }
                    if (okPol && polX.size() >= 3)
                        inside = BoxPointInPolygon(&polX[0], &polY[0], (int)polX.size(),
                                                     (float)(lx0 + lx1) * 0.5f, (float)(ly0 + ly1) * 0.5f);
                }
                const bool enters = touch ? (any || crosses || inside) : all;
                if (enters && (!filterHidden || faceVis[(size_t)f])) e->faceSel[(size_t)f] = 1;
            }
        }
        e->Recolor();
        g_redraw = true;
        return;
    }

    // ---- OBJECT MODE ----
    std::vector<Object*> objs;
    BoxCollect(SceneCollection, objs);
    if (!add) DeselectAll();
    for (size_t i = 0; i < objs.size(); i++)
        if (BoxObjectEnters(vp, objs[i], lx0, ly0, lx1, ly1, touch)) objs[i]->Select();
    g_redraw = true;
}

// ===== modo guiado de 1 CLICK (Select Linked desde el menu; Loop Select sin elemento activo) =====
// Sale un hint pidiendo click; el click hace la operacion y termina (1 solo click, vs los 2 del shortest path).
// El click se intercepta en ScenePick3D -> anda IGUAL en PC (mouse) y Symbian (mouse virtual + OK).
static int gGuidedOp = 0; // 0 = ninguno, 1 = Select Linked, 2 = Loop Select
bool GuidedUnClickActive(){ return gGuidedOp != 0; }
void LayoutGuidedCancel(){ if (!gGuidedOp) return; gGuidedOp = 0; NotifyHintClear(); g_redraw = true; }
static const char* GuidedElemName(){ return (EditSelectMode==SelFace)?"face":(EditSelectMode==SelEdge)?"edge":"vertex"; }
void LayoutSelectLinkedGuided(){
    if (InteractionMode != EditMode || !g_editMesh) return;
    gGuidedOp = 1;
    NotifyHint(std::string(T("Select Linked: click the")) + " " + T(GuidedElemName()) + " " + T("you want to select (Esc to cancel)"));
    g_redraw = true;
}
void LayoutLoopSelectGuided(){ // fallback de Loop Select cuando no hay elemento activo (pide click)
    if (InteractionMode != EditMode || !g_editMesh) return;
    gGuidedOp = 2;
    NotifyHint(std::string(T("Loop Select: click an")) + " " + T((EditSelectMode==SelFace)?"edge of a face":"edge") + " " + T("(Esc to cancel)"));
    g_redraw = true;
}
// click durante el modo guiado: true si lo consumio (ScenePick3D NO sigue con el pick normal).
static bool GuidedUnClickClick(int mx,int my,int vx,int vy,int vw,int vh,int screenH){
    if (!gGuidedOp || !g_editMesh) return false;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit){ gGuidedOp=0; NotifyHintClear(); return true; }
    if (gGuidedOp == 1){ // Select Linked: la isla conexa que contiene al elemento clickeado
        int k = EditPickIndex(EditSelectMode, mx,my,vx,vy,vw,vh,screenH);
        if (k < 0) return true; // click al vacio: sigue esperando
        UndoCaptureSelectionEdit(m);
        m->edit->SelectLinked(k, EditSelectMode, false);   // (suma a lo elegido, como la L)
    } else { // Loop Select en la posicion clickeada (face->Face Loop, border->Edge Loop; el cursor da la direccion)
        if (!LayoutLoopSelectInPos(mx,my,vx,vy,vw,vh,screenH)) return true; // click al vacio: sigue esperando
    }
    gGuidedOp = 0; NotifyHintClear(); g_redraw = true;
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
static int  gLoopCutCuts = 1;
static float gLoopCutFactor = 0.0f; // -1..1 (0 = centro)
static int  gLoopCutEdge = -1;      // arista (edit mesh) bajo el mouse = start del loop
static int  gLoopCutSlideX0 = 0;    // X del mouse al empezar el slide
static int  gLoopCutSlideW = 800;   // ancho del viewport (escala del slide)
static std::vector<float> gLoopCutSegs; // preview (segmentos locales) -> lo dibuja el viewport
bool gLoopCutIsPoint = false; // true = el corte es de UN borde suelto (perfil): el preview son PUNTOS, no lineas.
                              // Lo setea Mesh::LoopCutPreview (MeshEdit) segun si hay anillo de quads o no.
// fase ORIENTACION: solo al arrancar el loop cut desde un QUAD activo (menu). Antes de
// elegir los cortes, las flechas eligen cual de las 2 direcciones del quad cortar.
static bool gLoopCutOrienting = false;
static int  gLoopCutOrient = 0;           // 0 / 1
static int  gLoopCutOrientEdge[2] = {-1,-1}; // los 2 bordes perpendiculares del quad
bool LoopCutOrienting(){ return gLoopCutOrienting; }
// PICKER de direccion (click/tactil): en la fase de orientacion se dibujan los DOS cortes posibles
// (amarillo 0.5) + 4 PUNTOS grandes amarillos (centro de cada borde del quad). Tocar/clickear un punto
// elige la direccion de una. 2 puntos = una direccion, los otros 2 = la otra. gLoopCutSegs2 = el 2do corte.
static std::vector<float> gLoopCutSegs2;      // preview del corte de la OTRA direccion (solo en orientacion)
static float gLoopCutOrientPt[4][3];          // 4 midpoints (LOCAL) de los bordes del quad
static int   gLoopCutOrientPtDir[4] = {0,0,0,0}; // a que direccion (0/1) pertenece cada punto
static int   gLoopCutOrientNPts = 0;          // 4 cuando el picker esta activo
static void LoopCutApplyYPanel(); // (def mas abajo) aplica el corte + abre el panel de edicion
// en tactil no hay rueda ni flechas -> el loop cut se ajusta desde el panel redo (cortes + slide)
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controls.cpp: hubo input tactil en la sesion
static bool LoopCutTouch(){ return g_lastFingerTicks != 0; }
#else
static bool LoopCutTouch(){ return false; } // Symbian: va a teclado (flechas + Enter)
#endif

// snapshot de la geometria PRE-corte (para re-cortar en slide / redo)
// 'vp' es W3dPosVerts (Mesh.h): las posiciones Y el estado de pose (Mesh::posedByAnim) van
// juntos. Con un vector pelado, re-cortar (slide/redo) devolvia el vertex[] POSADO con el flag
// apagado y el ComputeBorders de abajo podaba TODAS las marcas contra la pose.
struct LCSnap { W3dPosVerts vp; std::vector<GLbyte> vn; std::vector<GLfloat> vu;
                std::vector<GLubyte> vc; int vsz; std::vector<MeshFace> f3d;
                std::vector<MaterialGroup> mg; std::vector<int> le, lv; bool has; // le/lv = looseEdges/looseVerts
                VtxAnimSnap vanims; }; // frames de las vertex anims PRE-corte (ver LCRestore)
static LCSnap gLCSnap;
// OPCIONES del panel redo: las 4 aristas del primer quad del loop (candidatas: por cual arista
// entra el corte -> 2 direcciones), cual esta elegida, y si los uv/color se interpolan
static int  gLoopCutCand[4] = {-1,-1,-1,-1};
static int  gLoopCutCandN = 0, gLoopCutCandSel = 0;
static bool gLoopCutCorrectUV = true;
// Los indices de arista del edit mesh NO sobreviven a un rebuild (LCRestore -> ComputeBorders reagrupa
// posRep y el edit se rearma): la arista de entrada y las candidatas se guardan por POSICION de sus dos
// puntas y se vuelven a resolver contra el edit mesh vivo antes de cada re-corte (LCReanchor).
static Vector3 gLoopCutEdgePos[2];
static Vector3 gLoopCutCandPos[4][2];
static int LCFindEdge(EditMesh* e, const Vector3& a, const Vector3& b){
    if (!e) return -1;
    for (int eg=0; eg<e->NumEdges(); eg++){ const int u=e->lineIdx[(size_t)eg*2], v=e->lineIdx[(size_t)eg*2+1];
        const Vector3 pu(e->pos[(size_t)u*3], e->pos[(size_t)u*3+1], e->pos[(size_t)u*3+2]), pv(e->pos[(size_t)v*3], e->pos[(size_t)v*3+1], e->pos[(size_t)v*3+2]);
        if (((pu-a).LengthSq() < 1e-8f && (pv-b).LengthSq() < 1e-8f) || ((pu-b).LengthSq() < 1e-8f && (pv-a).LengthSq() < 1e-8f)) return eg; }
    return -1;
}
static Vector3 LCPosVert(EditMesh* e, int k){ return Vector3(e->pos[(size_t)k*3], e->pos[(size_t)k*3+1], e->pos[(size_t)k*3+2]); }
static void LCReanchor(Mesh* m){   // tras LCRestore: indices frescos para la arista de entrada y las candidatas
    m->EnsureEdit(); EditMesh* e = m->edit; if (!e) return;
    const int eg = LCFindEdge(e, gLoopCutEdgePos[0], gLoopCutEdgePos[1]); if (eg >= 0) gLoopCutEdge = eg;
    for (int i=0;i<gLoopCutCandN;i++){ const int c = LCFindEdge(e, gLoopCutCandPos[i][0], gLoopCutCandPos[i][1]); if (c >= 0) gLoopCutCand[i] = c; }
}

static void LCSave(Mesh* m){
    gLCSnap.vsz = m->vertexSize;
    gLCSnap.vp.Capture(m);   // posiciones + estado de pose (la unica puerta; ver Mesh.h)
    if (m->normals)     gLCSnap.vn.assign(m->normals, m->normals + m->vertexSize*3); else gLCSnap.vn.clear();
    if (m->uv)          gLCSnap.vu.assign(m->uv, m->uv + m->vertexSize*2); else gLCSnap.vu.clear();
    if (m->vertexColor) gLCSnap.vc.assign(m->vertexColor, m->vertexColor + m->vertexSize*4); else gLCSnap.vc.clear();
    gLCSnap.f3d = m->faces3d; gLCSnap.mg = m->materialsGroup;
    gLCSnap.le = m->looseEdges; gLCSnap.lv = m->looseVerts; // sin esto el corte de un borde SUELTO (perfil) perdia las lineas al re-cortar
    // FRAMES de las vertex anims PRE-corte: el primer LoopCutEdit los remapea al layout
    // NUEVO; re-cortar (slide/redo) restaura la geometria VIEJA, asi que hay que devolver
    // tambien los frames VIEJOS o el proximo remap los lee desalineados y el cubo animado
    // se rompia por completo (vertex anim + loop cut).
    VertexAnimSnapshot(m, gLCSnap.vanims);
    gLCSnap.has = true;
    // candidatas (sobre la malla PRE-corte, que es la que se re-corta): las aristas del primer quad del loop
    gLoopCutCandN = 0; gLoopCutCandSel = 0;
    { std::vector<int> rEg, rA, rB, lf; bool closed = false;   // "cerrado", no "cerr": en Symbian cerr es un macro de STLport
      m->EnsureEdit(); EditMesh* e = m->edit;
      if (e && gLoopCutEdge >= 0 && gLoopCutEdge < e->NumEdges()) { gLoopCutEdgePos[0] = LCPosVert(e, e->lineIdx[(size_t)gLoopCutEdge*2]); gLoopCutEdgePos[1] = LCPosVert(e, e->lineIdx[(size_t)gLoopCutEdge*2+1]); }
      if (e && gLoopCutEdge >= 0 && m->LoopCutTraversal(gLoopCutEdge, rEg, rA, rB, lf, closed) && !lf.empty() && lf[0] < (int)e->faceEdges.size()) {
          const IntRows::Row fe = e->faceEdges[lf[0]];
          for (size_t k = 0; k < fe.size() && gLoopCutCandN < 4; k++) { const int c = fe[k]; if (c < 0 || c >= e->NumEdges()) continue;
              gLoopCutCand[gLoopCutCandN] = c; gLoopCutCandPos[gLoopCutCandN][0] = LCPosVert(e, e->lineIdx[(size_t)c*2]); gLoopCutCandPos[gLoopCutCandN][1] = LCPosVert(e, e->lineIdx[(size_t)c*2+1]);
              if (c == gLoopCutEdge) gLoopCutCandSel = gLoopCutCandN; gLoopCutCandN++; } } }
}
static void LCRestore(Mesh* m){
    if (!gLCSnap.has) return;
    VertexAnimRestoreSnap(m, gLCSnap.vanims); // los keyframes vuelven al layout PRE-corte (junto con la geometria)
    // si el snapshot tiene la MISMA cantidad de verts (re-cortar sin cambiar la topologia) esto
    // es un MOVE y las claves de sharp/seam hay que re-anclarlas; si cambia, es un REBUILD y el
    // guard no hace nada (no hay correspondencia por indice). Ver W3dMoveVerts.
    // OJO CON EL SCOPE: el guard tiene que CERRARSE ANTES del ComputeBorders de abajo
    // (que ademas de los bordes PODA las marcas que ya no son arista). Con el guard
    // abierto la poda se saltea, y ese salteo es la red de seguridad, no el plan.
    { W3dMoveVerts mv(m);
    m->vertexSize = gLCSnap.vsz;
    gLCSnap.vp.WriteIn(m);   // vertex[] Y posedByAnim, juntos
    if (!gLCSnap.vn.empty()){ delete[] m->normals; m->normals=new GLbyte[gLCSnap.vsz*3]; for (int i=0;i<gLCSnap.vsz*3;i++) m->normals[i]=gLCSnap.vn[i]; }
    if (!gLCSnap.vu.empty()){ delete[] m->uv; m->uv=new GLfloat[gLCSnap.vsz*2]; for (int i=0;i<gLCSnap.vsz*2;i++) m->uv[i]=gLCSnap.vu[i]; }
    if (!gLCSnap.vc.empty()){ delete[] m->vertexColor; m->vertexColor=new GLubyte[gLCSnap.vsz*4]; for (int i=0;i<gLCSnap.vsz*4;i++) m->vertexColor[i]=gLCSnap.vc[i]; }
    m->faces3d = gLCSnap.f3d; m->materialsGroup = gLCSnap.mg;
    m->looseEdges = gLCSnap.le; m->looseVerts = gLCSnap.lv; // restaurar el perfil suelto (sino desaparece al re-cortar)
    std::vector<MeshIndex> tris;
    for (size_t f=0;f<m->faces3d.size();f++) W3dTriangulateFace(m->vertex, m->faces3d[f].idx, tris);
    m->facesSize=(int)tris.size(); delete[] m->faces; m->faces=new MeshIndex[m->facesSize>0?m->facesSize:1];
    for (int i=0;i<m->facesSize;i++) m->faces[i]=tris[i];
    } // <- aca se re-ancla sharp/seam (recien despues se puede podar)
    m->ComputeBorders(); m->RecalculateNormals();
}

bool LoopCutActive(){ return gLoopCutOn; }

static void LoopCutUpdatePreview(int mx, int my){
    gLoopCutSegs.clear(); gLoopCutEdge = -1;
    if (!Viewport3DActive || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    int eg = EditPickIndex(SelEdge, mx, my, Viewport3DActive->x, Viewport3DActive->y,
                           Viewport3DActive->width, Viewport3DActive->height, W3dScreenHeight);
    gLoopCutEdge = eg;
    if (eg>=0) m->LoopCutPreview(eg, gLoopCutCuts, 0.0f, gLoopCutSegs);
}

// cartel-tutorial del loop cut, segun la fase (orientacion / cortes / slide). Se cierra al confirmar/cancelar.
static void LoopCutHint(){
    if (!gLoopCutOn){ NotifyHintClear(); return; }
    if (gLoopCutOrienting) NotifyHint(T("Loop Cut: tap a yellow dot to pick the cut direction (or arrows + Enter)"));
    else if (gLoopCutSlide) NotifyHint(T("Loop Cut: move = slide, Enter to confirm, Esc = center"));
    else                    NotifyHint(T("Loop Cut: scroll = number of cuts, Enter to confirm"));
}

void LoopCutStart(int mx, int my){
    if (InteractionMode!=EditMode || !g_editMesh){ Notify(T("Loop Cut: enter Edit Mode first"), true); return; }
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrienting=false; gLoopCutCuts=1; gLoopCutFactor=0.0f; gLCSnap.has=false;
    LoopCutUpdatePreview(mx,my);
    g_redraw=true;
    LoopCutHint();
}

// setup del loop cut sobre un BORDE conocido (direccion automatica). En tactil aplica+panel;
// en PC/Symbian sigue el flujo clasico (rueda = cortes, click aplica + slide).
static void LoopCutStartBorder(Mesh* m, int edgeIdx){
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrienting=false;
    gLoopCutCuts=1; gLoopCutFactor=0.0f; gLCSnap.has=false;
    gLoopCutEdge = edgeIdx;
    gLoopCutSegs.clear(); m->LoopCutPreview(gLoopCutEdge, gLoopCutCuts, 0.0f, gLoopCutSegs);
    gLoopCutSegs2.clear(); gLoopCutOrientNPts=0;
    g_redraw=true;
    if (LoopCutTouch()){ LoopCutApplyYPanel(); return; }
    LoopCutHint();
}

// setup del loop cut sobre un QUAD conocido: fase ORIENTACION (2 cortes en 0.5 + 4 puntos para
// elegir direccion con un click/toque; en PC/Symbian tambien flechas + Enter).
static void LoopCutStartQuad(Mesh* m, EditMesh* e, int faceIdx){
    gLoopCutOn=true; gLoopCutSlide=false; gLoopCutOrienting=true;
    gLoopCutCuts=1; gLoopCutFactor=0.0f; gLCSnap.has=false;
    gLoopCutOrient=0;
    gLoopCutOrientEdge[0] = e->faceEdges[faceIdx][0]; // las 2 direcciones perpendiculares
    gLoopCutOrientEdge[1] = e->faceEdges[faceIdx][1];
    gLoopCutEdge = gLoopCutOrientEdge[0];
    // preview de LAS DOS direcciones (se dibujan ambas en 0.5)
    gLoopCutSegs.clear();  m->LoopCutPreview(gLoopCutOrientEdge[0], gLoopCutCuts, 0.0f, gLoopCutSegs);
    gLoopCutSegs2.clear(); m->LoopCutPreview(gLoopCutOrientEdge[1], gLoopCutCuts, 0.0f, gLoopCutSegs2);
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
void LayoutLoopCutFromActive(){
    if (InteractionMode!=EditMode || !g_editMesh){ Notify(T("Loop Cut: enter Edit Mode first"), true); return; }
    Mesh* m=(Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return;
    EditMesh* e = m->edit;
    int act = e->activeIdx;
    if (EditSelectMode == SelEdge){
        if (act < 0 || act >= e->NumEdges()){ Notify(T("Loop Cut: no active edge (click an edge first)"), true); return; }
        LoopCutStartBorder(m, act);
    } else if (EditSelectMode == SelFace){
        if (act < 0 || act >= (int)e->faces.size()){ Notify(T("Loop Cut: no active face (click a face first)"), true); return; }
        if (e->faces[act].size() != 4){ Notify(T("Loop Cut: the active face is not a quad"), true); return; } // SOLO quads
        if (act >= (int)e->faceEdges.size() || e->faceEdges[act].size() < 2){ Notify(T("Loop Cut: the active face has no edges"), true); return; }
        LoopCutStartQuad(m, e, act);
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
            if (eg<0){ Notify(T("Loop Cut: the 2 selected vertices are not an edge"), true); return; }
            LoopCutStartBorder(m, eg);
        } else if (sel.size() == 4){
            // 4 verts: la cara (quad) cuyos 4 vertices son EXACTAMENTE los seleccionados -> elegir direccion
            int f = -1;
            for (int i=0; i<(int)e->faces.size(); i++){
                if (e->faces[i].size()!=4) continue;
                bool all=true;
                for (int j=0;j<4;j++){ if (!e->vertSel[e->faces[i][j]]){ all=false; break; } }
                if (all){ f=i; break; }
            }
            if (f<0 || f>=(int)e->faceEdges.size() || e->faceEdges[f].size()<2){ Notify(T("Loop Cut: select the 4 vertices of one quad"), true); return; }
            LoopCutStartQuad(m, e, f);
        } else {
            Notify(T("Loop Cut: select an edge (2 verts) or a quad (4 verts)"), true); // 3 o algo raro = invalido
        }
    }
}

static void LoopCutConfirm(){
    gLoopCutOn=false; gLoopCutSlide=false; gLoopCutSegs.clear(); gLoopCutSegs2.clear(); gLoopCutOrientNPts=0;
    NotifyHintClear();
    OpenRedoLoopCutPanel((Mesh*)g_editMesh); // panel redo (cortes + factor); usa el snapshot
    g_redraw=true;
}

// aplica el corte con los defaults (1 corte, factor 0) y abre el panel redo para editar cortes/slide.
// Es el camino de UN CLICK/TOUCH: como no hay flechas ni rueda en tactil, se ajusta desde el panel.
static void LoopCutApplyYPanel(){
    if (gLoopCutEdge<0 || !g_editMesh){ LoopCutCancel(); return; }
    Mesh* m=(Mesh*)g_editMesh;
    UndoCaptureMeshGeo(m);           // Ctrl+Z: 1 sola captura, PRE-corte
    LCSave(m);
    m->LoopCutEdit(gLoopCutEdge, gLoopCutCuts, 0.0f);
    LoopCutConfirm();                // cierra el modal + abre el panel (cortes + factor)
}

// ENTER en la fase de orientacion (teclado PC/Symbian): confirma la direccion elegida con las
// flechas y sigue al flujo clasico (preview con rueda -> click aplica+slide). El PICKER de puntos
// (click/tactil) NO pasa por aca: ese va directo a aplicar + panel.
void LoopCutOrientConfirmKeyboard(){
    if (!gLoopCutOrienting) return;
    gLoopCutOrienting=false; gLoopCutOrientNPts=0; gLoopCutSegs2.clear();
    gLoopCutSegs.clear();
    if (g_editMesh) ((Mesh*)g_editMesh)->LoopCutPreview(gLoopCutEdge, gLoopCutCuts, 0.0f, gLoopCutSegs);
    g_redraw=true;
    LoopCutHint(); // ayuda de la fase de cortes
}

// OK del TELEFONO (sin mouse) en la fase de orientacion: la direccion elegida con las flechas, aplicada de una + el panel
// (cortes/slide), como el toque de un punto. Antes el OK tomaba el punto mas cercano al cursor ESCONDIDO.
void LoopCutOrientApplyPicked(){
    if (!gLoopCutOrienting) return;
    gLoopCutOrienting=false; gLoopCutOrientNPts=0; gLoopCutSegs2.clear();
    gLoopCutEdge = gLoopCutOrientEdge[gLoopCutOrient];
    LoopCutApplyYPanel();
    g_redraw=true;
}

void LoopCutMotion(int mx, int my){
    if (!gLoopCutOn) return;
    if (!g_editMesh){ LoopCutCancel(); return; } // se salio de Edit Mode con el corte activo
    if (gLoopCutOrienting) return; // orientacion: los 2 previews son fijos (no re-pickear por hover)
    if (!gLoopCutSlide){ LoopCutUpdatePreview(mx,my); g_redraw=true; }
    else {
        float f = (float)(mx - gLoopCutSlideX0) / (float)(gLoopCutSlideW*0.4f);
        if (f>1.0f) f=1.0f; if (f<-1.0f) f=-1.0f;
        gLoopCutFactor=f;
        Mesh* m=(Mesh*)g_editMesh; LCRestore(m); LCReanchor(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCuts, f, gLoopCutCorrectUV);
        g_redraw=true;
    }
}

void LoopCutWheel(int dir){
    if (!gLoopCutOn || gLoopCutSlide || gLoopCutOrienting) return; // rueda = cortes (fase preview)
    gLoopCutCuts += dir; if (gLoopCutCuts<1) gLoopCutCuts=1; if (gLoopCutCuts>32) gLoopCutCuts=32;
    gLoopCutSegs.clear();
    if (gLoopCutEdge>=0 && g_editMesh) ((Mesh*)g_editMesh)->LoopCutPreview(gLoopCutEdge, gLoopCutCuts, 0.0f, gLoopCutSegs);
    g_redraw=true;
}

// FLECHAS durante el modal (PC y Symbian). dir: 0=izq 1=der 2=arriba 3=abajo.
//  orientacion (quad): izq/arriba = direccion 1, der/abajo = direccion 2.
//  cortes (preview):   arriba/derecha = +1, abajo/izquierda = -1.
//  slide:              izquierda/derecha mueve el factor.
void LoopCutKey(int dir){
    if (!gLoopCutOn || !g_editMesh) return;
    Mesh* m=(Mesh*)g_editMesh;
    if (gLoopCutOrienting){
        gLoopCutOrient = (dir==1 || dir==3) ? 1 : 0;
        gLoopCutEdge = gLoopCutOrientEdge[gLoopCutOrient];
        // gLoopCutSegs = la elegida, gLoopCutSegs2 = la OTRA (antes Segs2 quedaba fija en la direccion 1: elegida la 0
        // se veian las dos -una cruz- y elegida la 1, las dos encimadas)
        gLoopCutSegs.clear();  m->LoopCutPreview(gLoopCutEdge, gLoopCutCuts, 0.0f, gLoopCutSegs);
        gLoopCutSegs2.clear(); m->LoopCutPreview(gLoopCutOrientEdge[1 - gLoopCutOrient], gLoopCutCuts, 0.0f, gLoopCutSegs2);
        g_redraw=true;
        return;
    }
    if (!gLoopCutSlide){
        LoopCutWheel((dir==2 || dir==1) ? +1 : -1); // arriba/derecha +1 corte; abajo/izq -1
        return;
    }
    float step = 0.08f; // slide con flechas: izquierda/derecha
    if (dir==0) gLoopCutFactor -= step;
    else if (dir==1) gLoopCutFactor += step;
    else return;
    if (gLoopCutFactor>1.0f) gLoopCutFactor=1.0f;
    if (gLoopCutFactor<-1.0f) gLoopCutFactor=-1.0f;
    LCRestore(m); LCReanchor(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCuts, gLoopCutFactor, gLoopCutCorrectUV);
    g_redraw=true;
}

// click IZQUIERDO durante el loop cut: en preview aplica+entra al slide; en slide confirma
void LoopCutClickLeft(int mx, int my){
    if (!gLoopCutOn) return;
    if (gLoopCutOrienting){
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
                if (!Viewport3DActive->ProjectPoint(wp, sx, sy)) continue;
                float dx=sx-lmx, dy=sy-lmy; float d=dx*dx+dy*dy;
                if (d < bestD){ bestD = d; best = i; }
            }
        }
        if (best >= 0){
            gLoopCutOrient = gLoopCutOrientPtDir[best];
            gLoopCutEdge   = gLoopCutOrientEdge[gLoopCutOrient];
            gLoopCutOrienting=false;
            LoopCutApplyYPanel();   // aplica + abre el panel de edicion (cortes/slide)
        } else {
            // sin puntos proyectables: fallback al confirm clasico (sigue al preview)
            LoopCutOrientConfirmKeyboard();
        }
        g_redraw=true;
        return;
    }
    if (!gLoopCutSlide){
        if (gLoopCutEdge<0 || !g_editMesh){ LoopCutCancel(); return; }
        Mesh* m=(Mesh*)g_editMesh;
        UndoCaptureMeshGeo(m); // Ctrl+Z: 1 sola captura, PRE-corte (el slide/redo re-cortan sin recapturar)
        LCSave(m);
        m->LoopCutEdit(gLoopCutEdge, gLoopCutCuts, 0.0f);
        gLoopCutSlide=true; gLoopCutFactor=0.0f; gLoopCutSlideX0=mx;
        gLoopCutSlideW = Viewport3DActive ? Viewport3DActive->width : 800;
        gLoopCutSegs.clear();
        g_redraw=true;
        LoopCutHint();            // ayuda de la fase de slide
    } else LoopCutConfirm();
}

// click DERECHO: en slide deja el factor en 0 (centro) + confirma; en preview cancela
void LoopCutClickRight(){
    if (!gLoopCutOn) return;
    if (!gLoopCutSlide || !g_editMesh){ LoopCutCancel(); return; } // sin Edit Mode no hay malla que cortar
    gLoopCutFactor=0.0f;
    Mesh* m=(Mesh*)g_editMesh; LCRestore(m); LCReanchor(m); m->LoopCutEdit(gLoopCutEdge, gLoopCutCuts, 0.0f, gLoopCutCorrectUV);
    LoopCutConfirm();
}

void LoopCutCancel(){
    if (!gLoopCutOn) return;
    if (gLoopCutSlide && gLCSnap.has){ LCRestore((Mesh*)g_editMesh); gLCSnap.has=false; }
    // mismo reset COMPLETO que LoopCutConfirm: sin apagar la fase de orientacion,
    // el toque de 1 dedo y el Enter seguian desviandose a un loop cut ya cancelado
    gLoopCutOn=false; gLoopCutSlide=false; gLoopCutOrienting=false;
    gLoopCutSegs.clear(); gLoopCutSegs2.clear(); gLoopCutOrientNPts=0;
    NotifyHintClear();
    g_redraw=true;
}

// la usa el panel redo: re-corta desde el snapshot con nuevos parametros
void LoopCutRedoApply(int cuts, float factor, bool correctUV, int edgeSel){
    if (!gLCSnap.has || gLoopCutEdge<0 || !g_editMesh) return;
    if (cuts<1) cuts=1;
    gLoopCutCuts=cuts; gLoopCutFactor=factor; gLoopCutCorrectUV=correctUV;
    Mesh* m=(Mesh*)g_editMesh; LCRestore(m); LCReanchor(m);
    if (edgeSel >= 0 && edgeSel < gLoopCutCandN) { gLoopCutCandSel = edgeSel; gLoopCutEdge = gLoopCutCand[edgeSel]; gLoopCutEdgePos[0] = gLoopCutCandPos[edgeSel][0]; gLoopCutEdgePos[1] = gLoopCutCandPos[edgeSel][1]; }
    m->LoopCutEdit(gLoopCutEdge, cuts, factor, correctUV);
    g_redraw=true;
}
void LoopCutRedoApply(int cuts, float factor){ LoopCutRedoApply(cuts, factor, gLoopCutCorrectUV, gLoopCutCandSel); }
int   LoopCutGetCuts(){ return gLoopCutCuts; }
float LoopCutGetFactor(){ return gLoopCutFactor; }
bool  LoopCutGetCorrectUV(){ return gLoopCutCorrectUV; }
int   LoopCutGetCandN(){ return gLoopCutCandN; }
int   LoopCutGetCandSel(){ return gLoopCutCandSel; }
int   LoopCutGetEdge(){ return gLoopCutEdge; }
int   LoopCutGetCand(int i){ return (i >= 0 && i < gLoopCutCandN) ? gLoopCutCand[i] : -1; }

// dibuja el preview del corte (segmentos locales) con la matriz de mundo del objeto.
// Lo llama el viewport 3D despues de renderizar la escena.
void LoopCutRenderPreview(){
    if (!gLoopCutOn || !g_editMesh) return;
    if (gLoopCutSegs.empty() && !gLoopCutOrienting) return;
    Mesh* m=(Mesh*)g_editMesh;
    Matrix4 W; m->GetWorldMatrix(W);
    w3dEngine::PushMatrix(); w3dEngine::MultMatrix(W.m);
    w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Texture2D); w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::DisableArray(w3dEngine::NormalArray); w3dEngine::DisableArray(w3dEngine::ColorArray); w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::EnableArray(w3dEngine::VertexArray);

    if (gLoopCutOrienting){
        // FASE ORIENTACION (quad): + 4 PUNTOS grandes 100% opacos. Con el DEDO, los DOS cortes posibles en amarillo
        // 0.5 (se toca el punto de la que se quiere). Con TECLADO (flechas + Enter) UNA sola linea: la elegida.
        w3dEngine::LineWidth(3.0f);
        if (LoopCutTouch()){
            w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 0.5f); // amarillo semitransparente
            if (!gLoopCutSegs2.empty()){ w3dEngine::VertexPointer3f(0, &gLoopCutSegs2[0]); w3dEngine::DrawLines((int)(gLoopCutSegs2.size()/3)); }
        } else {
            w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 1.0f); // la elegida, opaca
        }
        if (!gLoopCutSegs.empty()){  w3dEngine::VertexPointer3f(0, &gLoopCutSegs[0]);  w3dEngine::DrawLines((int)(gLoopCutSegs.size()/3)); }
        w3dEngine::LineWidth(1.0f);
        if (gLoopCutOrientNPts > 0){ // los puntos que se tocan para elegir la direccion
            w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 1.0f); // amarillo opaco
            w3dEngine::PointSize(22.0f);
            w3dEngine::VertexPointer3f(0, &gLoopCutOrientPt[0][0]);
            w3dEngine::DrawPoints(gLoopCutOrientNPts);
            w3dEngine::PointSize(1.0f);
        }
    } else {
        w3dEngine::Color4f(1.0f, 0.95f, 0.2f, 1.0f); // amarillo
        w3dEngine::VertexPointer3f(0, &gLoopCutSegs[0]);
        if (gLoopCutIsPoint){ // corte de un BORDE SUELTO: el corte es un PUNTO en la arista, no una linea
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
    // WEIGHT PAINT: el click sobre el 3D PINTA (Viewport3D::button_left arranca el trazo); NO
    // re-pickea objetos (cambiar de malla mientras pintas seria un accidente) ni ensucia el undo.
    // Para cambiar de objeto en este modo esta el Outliner.
    if (InteractionMode == WeightPaint) return true;
    // EDIT MODE de ARMATURE (Fase 3): el click selecciona un HUESO (head/tail crudos) y arranca el
    // drag de su extremo (head/tail/cuerpo). Todo vive en BoneEdit.cpp; shift agrega a la seleccion.
    if (InteractionMode == EditMode && ActiveObj && ActiveObj->getType() == ObjectType::armature && Viewport3DActive)
        return BoneEditClick3D(Viewport3DActive, mx, my, vx, vy, LShiftPressed);
    // Ctrl+Z: en Object Mode el click va a cambiar la seleccion -> guardar la previa.
    // (la seleccion de sub-elementos en Edit Mode es 2da tanda)
    if (InteractionMode != EditMode) UndoCaptureSelection();
    // POSE MODE: el click selecciona un HUESO del armature activo (no un objeto de la escena). Shift = agrega/saca.
    if (InteractionMode == PoseMode && ActiveObj && ActiveObj->getType() == ObjectType::armature && Viewport3DActive) {
        Armature* arm = (Armature*)ActiveObj;
        int b = Viewport3DActive->PickBone(arm, (float)(mx - vx), (float)(my - vy));
        if (!LShiftPressed) for (size_t i = 0; i < arm->bones.size(); i++) arm->bones[i].select = false;
        if (b >= 0) {
            bool newValue = !(LShiftPressed && arm->bones[b].select); // shift sobre uno ya seleccionado -> lo saca
            arm->bones[b].select = newValue;
            arm->activeBone = newValue ? b : -1;
        } else if (!LShiftPressed) arm->activeBone = -1;
        g_redraw = true;
        return true;
    }
    // en Edit Mode el click selecciona sub-elementos (no objetos)
    if (InteractionMode == EditMode && g_editMesh) {
        UndoCaptureSelectionEdit((Mesh*)g_editMesh); // Ctrl+Z: el click va a cambiar la seleccion de sub-elementos
        // modo guiado "Pick Shortest Path" (desde el menu): el click elige 1ro/2do elemento
        if (PickPathClick(mx, my, vx, vy, vw, vh, screenH)) return true;
        if (GuidedUnClickClick(mx, my, vx, vy, vw, vh, screenH)) return true; // Select Linked / Loop Select guiados (1 click)
        // Loop Select por Alt+Click (el borde de referencia es el que esta bajo el cursor):
        //   Shift+Alt+Click -> en CUALQUIER modo (CARA=Face Loop, BORDE/VERTICE=Edge Loop). El Shift ademas AGREGA.
        //   Ctrl+Alt+Click (modo BORDE) -> Edge Loop tambien (compat con el atajo viejo; sin shift = reemplaza).
        // (antes Shift+Alt solo andaba en modo cara; en borde/vertice no seleccionaba el loop.)
        if (LAltPressed && (LShiftPressed || (LCtrlPressed && EditSelectMode == SelEdge))) {
            Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
            if (m->edit) {
                int eg = EditPickIndex(SelEdge, mx, my, vx, vy, vw, vh, screenH); // siempre pickea un BORDE de referencia
                if (eg >= 0) {
                    bool onlyThis = !LShiftPressed; // Shift = agrega a la seleccion; sino reemplaza
                    if (EditSelectMode == SelFace)      m->edit->SelectLoopFace(eg, onlyThis);
                    else if (EditSelectMode == SelEdge) m->edit->SelectLoopEdge(eg, onlyThis);
                    // VERTICE: igual que en borde, Shift AGREGA el loop a los verts que ya estaban (varios loops seguidos,
                    // como en Blender). Antes reemplazaba y cada Shift+Alt+Click borraba lo anterior. El edgeSel intermedio
                    // igual arranca limpio en cada llamada (SelectLoopEdgeVerts): no se acumulan loops viejos
                    else                                m->edit->SelectLoopEdgeVerts(eg, onlyThis);
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
                if (k >= 0) m->edit->SelectShortestPath(k, LShiftPressed);
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
        // LA proyeccion del viewport (la de su Render: mirando por la camara, la de la CAMARA; ver EditPickIndex)
        if (Viewport3DActive) Viewport3DActive->LoadProjectionView();
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
    w3dEngine::ReadPixelsBegin(mx, screenH - 1 - my, 1, 1);   // (antes de dibujar: ver EditPickIndex)
    w3dEngine::ClearColor(0, 0, 0, 1);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    // LAS RAICES que se ven: la activa y las otras escenas abiertas (EscenaRender las dibuja despues de la activa). Una
    // oculta con su ojo no se pinta (sus objetos no se ven)
    std::vector<Object*> roots;
    if (SceneCollection->visible) roots.push_back(SceneCollection);
    if (!W3dScriptHasSession()) {
        std::vector<Object*> other;
        W3dOtherVisibleRoots(other);
        roots.insert(roots.end(), other.begin(), other.end());
    }
    pickCounter = 0;
    for (size_t r = 0; r < roots.size(); r++) PickPaint(roots[r], false); // 1ra pasada: NO-armatures (con z-buffer)
    for (size_t r = 0; r < roots.size(); r++) PickPaint(roots[r], true);  // 2da: armatures XRAY (sin z, encima)
    w3dEngine::Enable(w3dEngine::DepthTest); // restaurar (la pasada xray lo dejo off)

    GLubyte pix[4] = {0, 0, 0, 0};
    w3dEngine::ReadPixelsRGBA(mx, screenH - 1 - my, 1, 1, pix);
    w3dEngine::Enable(w3dEngine::Dither);
    w3dEngine::Disable(w3dEngine::ScissorTest);

    w3dEngine::EnableArray(w3dEngine::TexCoordArray); // baseline de la UI

    int id = ((pix[1] >> 2) << 5) | (pix[0] >> 3);

    pickFound = NULL;
    if (id > 0) {
        pickCounter = 0;
        for (size_t r = 0; r < roots.size() && !pickFound; r++) PickResolves(roots[r], id, false);   // 1ra pasada
        for (size_t r = 0; r < roots.size() && !pickFound; r++) PickResolves(roots[r], id, true);    // 2da (sigue la cuenta)
    }
    // un objeto de OTRA escena abierta: su escena pasa a ser la activa y el click elige ahi
    if (pickFound) {
        Object* root = pickFound;
        while (root->Parent) root = root->Parent;
        if (root != SceneCollection && !W3dActivateRootOf(root)) return true;
    }
    if (!LShiftPressed) {
        DeselectAll();
    }
    {
        if (pickFound) {
            if (LShiftPressed && pickFound->select) {
                pickFound->select = false; // shift: sacar de la seleccion
            } else {
                pickFound->Select();
            }
        }
    }
    return true; // el frame siguiente redibuja la escena real
}


// ============================================================================
//  INSET FACES: el MODAL (I en Edit Mode / menu Face) y el panel redo. La op de malla es Mesh::InsetEdit
//  (edit/MeshEdit.cpp). Como Blender: acercar el mouse al centro de la seleccion = mas grosor; con Ctrl el
//  mouse maneja la profundidad; I = Individual, B = Boundary, O = Outset; click / Enter confirma, click
//  derecho / Esc cancela. En vivo: cada movimiento vuelve a la malla de ANTES (el snapshot) y re-inseta.
//  Al confirmar recien se toma el undo (de la malla de antes) y se abre el panel "Inset Faces".
// ============================================================================
struct InsetSnap { W3dPosVerts vp; std::vector<GLbyte> vn; std::vector<GLfloat> vu; std::vector<GLubyte> vc; int vsz;
                   std::vector<MeshFace> f3d; std::vector<MaterialGroup> mg; std::vector<int> le, lv, rep;
                   std::vector<std::vector<GLfloat> > uvs; std::vector<std::vector<GLubyte> > cols; std::vector<GLbyte> cn;
                   std::set<std::string> sharp, seam; VtxAnimSnap vanims; bool has;
                   InsetSnap() : vsz(0), has(false) {} };
static InsetSnap gInSnap;
static Mesh* gInMesh = NULL;              // la malla del snapshot (si cambia, el snapshot no sirve)
static bool gInsetOn = false;
static std::vector<char> gInsetSel;        // las faces3d seleccionadas (sobre la malla del snapshot)
static std::vector<Vector3> gInsetCenters; // los centros de esas caras: la seleccion vuelve al cancelar
static InsetOptions gInsetOp;
static float gInCx = 0, gInCy = 0;         // el centro de la seleccion en pantalla
static float gInPxByU = 100.0f;            // pixeles por unidad (local) alrededor de ese centro
static float gInDistBase = 0, gInValueBase = 0;
static bool  gInProf = false;              // el mouse maneja la profundidad (Ctrl)
static bool  gInDepthDrag = false;         // ...o el boton "Profundidad" de la barra (tactil, sin Ctrl)
// el valor SIN redondear (el mouse / las flechas lo mueven): con el IMAN prendido lo que se aplica es este valor llevado
// al paso redondo mas cercano (ModalSnap); sin esto un paso chico de flecha nunca llegaba al proximo
static float gInRawThick = 0.0f, gInRawDepth = 0.0f;

// EL IMAN en insertar caras / bevel: no hay geometria a la que pegarse (el valor es un ancho, no una posicion), asi que
// el iman lo lleva a pasos REDONDOS, como la grilla: 0.1 / 1 / 10... segun el zoom (cada paso entre 3 y 30 px de
// pantalla). En Percent, de a 1 / 10 %. Sin iman, el valor tal cual
static float ModalSnap(float v, float pxByU, bool percent) {
    if (!g_snap.enabled || pxByU <= 0.0f) return v;
    float unitPx = percent ? pxByU / 100.0f : pxByU;   // (pixeles por unidad del valor)
    if (unitPx <= 0.0f) return v;
    float step = 1.0f;   // (sin pow/log10: el exe del N95 no importa nada nuevo de la libm)
    for (int i = 0; i < 12 && step * unitPx > 30.0f; i++) step *= 0.1f;
    for (int i = 0; i < 12 && step * unitPx < 3.0f; i++) step *= 10.0f;
    const float q = v / step;
    return (float)(int)(q + (q >= 0.0f ? 0.5f : -0.5f)) * step;
}
static void InsetSetRaw() {
    gInsetOp.thickness = ModalSnap(gInRawThick, gInPxByU, false); if (gInsetOp.thickness < 0.0f) gInsetOp.thickness = 0.0f;
    gInsetOp.depth = ModalSnap(gInRawDepth, gInPxByU, false);
}

static void InsetSave(Mesh* m) {
    InsetSnap& s = gInSnap;
    s.vsz = m->vertexSize; s.vp.Capture(m);
    if (m->normals)     s.vn.assign(m->normals, m->normals + m->vertexSize*3); else s.vn.clear();
    if (m->uv)          s.vu.assign(m->uv, m->uv + m->vertexSize*2); else s.vu.clear();
    if (m->vertexColor) s.vc.assign(m->vertexColor, m->vertexColor + m->vertexSize*4); else s.vc.clear();
    s.f3d = m->faces3d; s.mg = m->materialsGroup; s.le = m->looseEdges; s.lv = m->looseVerts; s.rep = m->posRep;
    s.uvs.clear(); for (size_t k = 0; k < m->uvMaps.size(); k++) s.uvs.push_back(m->uvMaps[k]->uv);
    s.cols.clear(); for (size_t k = 0; k < m->colorLayers.size(); k++) s.cols.push_back(m->colorLayers[k]->color);
    s.cn = m->cornerNormal; s.sharp = m->sharpEdges; s.seam = m->seamEdges;
    VertexAnimSnapshot(m, s.vanims);
    s.has = true; gInMesh = m;
}
// vuelve la malla a como estaba al arrancar. 'regenerar' = dejarla entera (render + edicion); sin eso queda lista
// solo para que InsetEdit la vuelva a insetar (que regenera al final)
static void InsetRestore(Mesh* m, bool regenerate) {
    InsetSnap& s = gInSnap;
    if (!s.has || m != gInMesh) return;
    VertexAnimRestoreSnap(m, s.vanims);
    m->vertexSize = s.vsz; s.vp.WriteIn(m);
    delete[] m->normals; m->normals = NULL; if (!s.vn.empty()) { m->normals = new GLbyte[s.vsz*3]; for (int i = 0; i < s.vsz*3; i++) m->normals[i] = s.vn[i]; }
    delete[] m->uv; m->uv = NULL; if (!s.vu.empty()) { m->uv = new GLfloat[s.vsz*2]; for (int i = 0; i < s.vsz*2; i++) m->uv[i] = s.vu[i]; }
    delete[] m->vertexColor; m->vertexColor = NULL; if (!s.vc.empty()) { m->vertexColor = new GLubyte[s.vsz*4]; for (int i = 0; i < s.vsz*4; i++) m->vertexColor[i] = s.vc[i]; }
    m->faces3d = s.f3d; m->materialsGroup = s.mg; m->looseEdges = s.le; m->looseVerts = s.lv; m->posRep = s.rep;
    for (size_t k = 0; k < m->uvMaps.size() && k < s.uvs.size(); k++) m->uvMaps[k]->uv = s.uvs[k];
    for (size_t k = 0; k < m->colorLayers.size() && k < s.cols.size(); k++) m->colorLayers[k]->color = s.cols[k];
    m->cornerNormal = s.cn; m->sharpEdges = s.sharp; m->seamEdges = s.seam;
    if (regenerate) m->GenerateRender();
}

bool InsetActive() { return gInsetOn; }
bool& InsetDepthDrag() { return gInDepthDrag; }
InsetOptions& InsetCurrentOptions() { return gInsetOp; }
void InsetScreenCenter(float& x, float& y, float& pxByU) { x = gInCx; y = gInCy; pxByU = gInPxByU; }

static void InsetHint() {
    if (!gInsetOn) { NotifyHintClear(); return; }
    NotifyHint(T("Inset: move toward the center = thickness, Ctrl = depth, I/B/O = individual/boundary/outset, click to confirm"));
}

// re-inseta desde el snapshot con las opciones de ahora (el modal y el panel redo)
static void InsetApply() {
    Mesh* m = (Mesh*)g_editMesh;
    if (!m || m != gInMesh || !gInSnap.has) return;
    InsetRestore(m, false);
    m->InsetEdit(gInsetOp, gInsetSel);
    g_redraw = true;
}
void InsetRedoApply() { InsetApply(); }

void InsetStart(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh) { Notify(T("Inset: enter Edit Mode first"), true); return; }
    if (gInsetOn || LoopCutActive()) return;
    Mesh* m = (Mesh*)g_editMesh;
    std::vector<char> sel;
    if (!m->SelectedFaces3d(sel)) { Notify(T("Inset: select faces first"), true); g_redraw = true; return; }
    RedoMeshPanelClose();   // (otro panel redo trabaja sobre su propio snapshot)
    // el centro de la seleccion y la escala de la pantalla alrededor de el
    Vector3 c(0, 0, 0); int n = 0; gInsetCenters.clear();
    for (size_t f = 0; f < sel.size(); f++) { if (!sel[f]) continue;
        const std::vector<int>& idx = m->faces3d[f].idx; Vector3 cf(0, 0, 0);
        for (size_t k = 0; k < idx.size(); k++) cf = cf + Vector3(m->vertex[idx[k]*3], m->vertex[idx[k]*3+1], m->vertex[idx[k]*3+2]);
        cf = cf * (1.0f / (float)idx.size()); gInsetCenters.push_back(cf); c = c + cf; n++; }
    c = c * (1.0f / (float)n);
    gInCx = (float)mx - 200.0f; gInCy = (float)my; gInPxByU = 100.0f;   // (sin vista: un centro 200 px a la izquierda)
    if (Viewport3DActive) {
        Viewport3DActive->BindView();
        Matrix4 W; m->GetWorldMatrix(W);
        float sx, sy;
        if (Viewport3DActive->ProjectPoint(W * c, sx, sy)) {
            gInCx = sx; gInCy = sy;
            const Vector3 axes[3] = { Vector3(1,0,0), Vector3(0,1,0), Vector3(0,0,1) };
            float best = 0.0f;
            for (int k = 0; k < 3; k++) { float ax, ay;
                if (!Viewport3DActive->ProjectPoint(W * (c + axes[k]), ax, ay)) continue;
                const float d = sqrtf((ax-sx)*(ax-sx) + (ay-sy)*(ay-sy)); if (d > best) best = d; }
            if (best > 1.0f) gInPxByU = best;
        }
    }
    InsetSave(m);
    gInsetSel = sel;
    gInsetOp.thickness = 0.0f; gInsetOp.depth = 0.0f;   // (las tildes quedan como se usaron la ultima vez)
    gInRawThick = 0.0f; gInRawDepth = 0.0f;
    gInProf = false; gInDepthDrag = false;
    NumInputReset();
    float d = sqrtf(((float)mx-gInCx)*((float)mx-gInCx) + ((float)my-gInCy)*((float)my-gInCy));
    gInDistBase = d < 10.0f ? 10.0f : d; gInValueBase = 0.0f;
    gInsetOn = true;
    InsetHint();
    g_redraw = true;
}

// el mouse (o el dedo) arranca desde (mx, my) con el valor que ya tenia: al cambiar lo que maneja (Ctrl) o al apoyar
// el dedo (sin esto el valor saltaba a la distancia del dedo al centro)
void InsetGrab(int mx, int my) {
    if (!gInsetOn) return;
    gInProf = LCtrlPressed || gInDepthDrag;
    gInDistBase = sqrtf(((float)mx-gInCx)*((float)mx-gInCx) + ((float)my-gInCy)*((float)my-gInCy));
    gInValueBase = gInProf ? gInsetOp.depth : gInsetOp.thickness;
}

void InsetMotion(int mx, int my) {
    if (!gInsetOn || NumInputActive()) return;   // (con un valor tipeado manda el numero)
    const float d = sqrtf(((float)mx-gInCx)*((float)mx-gInCx) + ((float)my-gInCy)*((float)my-gInCy));
    if ((LCtrlPressed || gInDepthDrag) != gInProf) InsetGrab(mx, my);   // cambia lo que maneja el mouse
    const float v = gInValueBase + (gInDistBase - d) / gInPxByU;   // acercarse al centro = mas
    if (gInProf) gInRawDepth = v;
    else         gInRawThick = v > 0.0f ? v : 0.0f;
    InsetSetRaw();
    InsetApply();
}

// el valor TIPEADO (la barra de estado, el teclado numerico, el lapiz de Symbian): el grosor, o la profundidad
// si es lo que maneja el mouse
void InsetNumber() {
    float v = 0.0f;
    if (!gInsetOn || !NumInputValue(v)) return;
    if (gInProf) gInsetOp.depth = gInRawDepth = v;   // (tipeado: el numero exacto, sin iman)
    else         gInsetOp.thickness = gInRawThick = v > 0.0f ? v : 0.0f;
    InsetApply();
}

void InsetCancel() {
    if (!gInsetOn) return;
    gInsetOn = false;
    NumInputReset();
    Mesh* m = (Mesh*)g_editMesh;
    if (m && m == gInMesh) { InsetRestore(m, true); m->SelectFacesByCenter(gInsetCenters); }
    gInSnap.has = false;
    InsetHint();
    g_redraw = true;
}

void InsetConfirm() {
    if (!gInsetOn) return;
    gInsetOn = false;
    NumInputReset();
    InsetHint();
    Mesh* m = (Mesh*)g_editMesh;
    if (!m || m != gInMesh) { gInSnap.has = false; return; }
    // el undo guarda la malla de ANTES: se vuelve a ella, se captura y se re-inseta (el snapshot queda para el panel)
    InsetRestore(m, true); m->SelectFacesByCenter(gInsetCenters);
    UndoCaptureMeshGeo(m);
    m->InsetEdit(gInsetOp, gInsetSel);
    OpenRedoInsetPanel(m);
    g_redraw = true;
}

// teclas del modal: true = la uso
bool InsetKey(int key) {
    if (!gInsetOn) return false;
    switch (key) {
        case W3dK_RETURN: case W3dK_KP_ENTER: InsetConfirm(); return true;
        case W3dK_ESCAPE: case W3dK_BACKSPACE: InsetCancel(); return true;
        case W3dK_I: gInsetOp.individual = !gInsetOp.individual; InsetApply(); return true;
        case W3dK_B: gInsetOp.border = !gInsetOp.border; InsetApply(); return true;
        case W3dK_O: gInsetOp.outside = !gInsetOp.outside; InsetApply(); return true;
        // las FLECHAS agrandan / achican lo que maneja el mouse (el teclado del telefono, sin mouse): de a 10 px
        case W3dK_UP: case W3dK_RIGHT: case W3dK_DOWN: case W3dK_LEFT: {
            const float step = ((key == W3dK_UP || key == W3dK_RIGHT) ? 10.0f : -10.0f) / gInPxByU;
            NumInputReset();
            if (gInProf) gInRawDepth = gInsetOp.depth + step;
            else { gInRawThick = gInsetOp.thickness + step; if (gInRawThick < 0.0f) gInRawThick = 0.0f; }
            // con el iman, al menos UN paso redondo por pulsacion
            if (g_snap.enabled) { float& r = gInProf ? gInRawDepth : gInRawThick; const float cur = gInProf ? gInsetOp.depth : gInsetOp.thickness;
                for (int i = 0; i < 1000 && ModalSnap(r, gInPxByU, false) == cur; i++) r += step; }
            InsetSetRaw();
            InsetApply(); return true;
        }
    }
    return true;   // (el resto no hace nada mientras dura: que no se escape a los atajos del viewport)
}


// ============================================================================
//  BEVEL: el MODAL (Ctrl+B en Edit Mode / menus Edge y Vertex) y el panel redo. La op de malla es Mesh::BevelEdit
//  (edit/MeshEdit.cpp). Como Blender: ALEJAR el mouse del centro de la seleccion = mas ancho; la rueda (o arriba /
//  abajo, Re Pag / Av Pag) = mas o menos segmentos; V = vertices / aristas; click / Enter confirma, click derecho /
//  Esc cancela. En vivo: cada cambio vuelve a la malla de ANTES (el mismo snapshot del inset) y re-bisela. Al
//  confirmar se toma el undo (de la malla de antes) y se abre el panel "Bevel".
// ============================================================================
static bool gBevelOn = false;
static std::vector<char> gBvSelRep;                       // la seleccion de ANTES (sobre la malla del snapshot)
static std::vector<std::pair<int,int> > gBvEdges;
static std::vector<Vector3> gBvCenters;                   // los centros de las caras elegidas: vuelven al cancelar
static BevelOptions gBevelOp;
static float gBvCx = 0, gBvCy = 0, gBvPxByU = 100.0f, gBvDistBase = 0, gBvValueBase = 0;
static bool gBvHasSnap = false;
static float gBvRawWidth = 0.0f;   // el ancho sin redondear (ver gInRawThick)
static void BevelSetRaw() {
    if (gBvRawWidth < 0.0f) gBvRawWidth = 0.0f;
    gBevelOp.width = ModalSnap(gBvRawWidth, gBvPxByU, gBevelOp.widthType == 3);
    if (gBevelOp.width < 0.0f) gBevelOp.width = 0.0f;
}

bool BevelActive() { return gBevelOn; }
BevelOptions& BevelCurrentOptions() { return gBevelOp; }
void BevelScreenCenter(float& x, float& y, float& pxByU) { x = gBvCx; y = gBvCy; pxByU = gBvPxByU; }

static void BevelHint() {
    if (!gBevelOn) { NotifyHintClear(); return; }
    NotifyHint(T("Bevel: move away from the center = width, wheel / up-down = segments, V = vertices/edges, click to confirm"));
}
static void BevelApply() {
    Mesh* m = (Mesh*)g_editMesh;
    if (!m || m != gInMesh || !gInSnap.has || !gBvHasSnap) return;
    InsetRestore(m, false);
    if (!m->BevelEdit(gBevelOp, gBvSelRep, gBvEdges)) m->GenerateRender();   // (ancho 0: la malla de antes, entera)
    g_redraw = true;
}
void BevelRedoApply() { BevelApply(); }

void BevelStart(int mx, int my, int affects) {
    if (InteractionMode != EditMode || !g_editMesh) { Notify(T("Bevel: enter Edit Mode first"), true); return; }
    if (gBevelOn || gInsetOn || LoopCutActive()) return;
    Mesh* m = (Mesh*)g_editMesh;
    std::vector<char> selRep; std::vector<std::pair<int,int> > edges;
    if (!m->BevelSelection(selRep, edges) || (affects == 1 && edges.empty())) {
        Notify(T(affects == 1 ? "Bevel: select edges first" : "Bevel: select vertices first"), true); g_redraw = true; return; }
    RedoMeshPanelClose();
    // el centro de lo elegido y la escala de la pantalla alrededor de el
    Vector3 c(0, 0, 0); int n = 0;
    for (int i = 0; i < m->vertexSize && i < (int)selRep.size(); i++) if (selRep[i]) {
        c = c + Vector3(m->vertex[i*3], m->vertex[i*3+1], m->vertex[i*3+2]); n++; }
    if (n > 0) c = c * (1.0f / (float)n);
    gBvCenters.clear();
    { std::vector<char> sf; if (m->SelectedFaces3d(sf)) for (size_t f = 0; f < sf.size(); f++) { if (!sf[f]) continue;
        const std::vector<int>& idx = m->faces3d[f].idx; Vector3 cf(0, 0, 0);
        for (size_t k = 0; k < idx.size(); k++) cf = cf + Vector3(m->vertex[idx[k]*3], m->vertex[idx[k]*3+1], m->vertex[idx[k]*3+2]);
        gBvCenters.push_back(cf * (1.0f / (float)idx.size())); } }
    gBvCx = (float)mx - 100.0f; gBvCy = (float)my; gBvPxByU = 100.0f;
    if (Viewport3DActive) {
        Viewport3DActive->BindView();
        Matrix4 W; m->GetWorldMatrix(W);
        float sx, sy;
        if (Viewport3DActive->ProjectPoint(W * c, sx, sy)) {
            gBvCx = sx; gBvCy = sy;
            const Vector3 axes[3] = { Vector3(1,0,0), Vector3(0,1,0), Vector3(0,0,1) };
            float best = 0.0f;
            for (int k = 0; k < 3; k++) { float ax, ay;
                if (!Viewport3DActive->ProjectPoint(W * (c + axes[k]), ax, ay)) continue;
                const float d = sqrtf((ax-sx)*(ax-sx) + (ay-sy)*(ay-sy)); if (d > best) best = d; }
            if (best > 1.0f) gBvPxByU = best;
        }
    }
    InsetSave(m);   // (el mismo snapshot que el inset: geometria + capas + marcas)
    gBvHasSnap = true;
    gBvSelRep = selRep; gBvEdges = edges;
    gBevelOp.affects = affects; gBevelOp.width = 0.0f;   // (el resto queda como se uso la ultima vez)
    gBvRawWidth = 0.0f;
    const float d = sqrtf(((float)mx-gBvCx)*((float)mx-gBvCx) + ((float)my-gBvCy)*((float)my-gBvCy));
    gBvDistBase = d; gBvValueBase = 0.0f;
    NumInputReset();
    gBevelOn = true;
    BevelHint();
    g_redraw = true;
}

// el mouse (o el dedo) arranca desde (mx, my) con el ancho que ya tenia (al apoyar el dedo: sin esto saltaba)
void BevelGrab(int mx, int my) {
    if (!gBevelOn) return;
    gBvDistBase = sqrtf(((float)mx-gBvCx)*((float)mx-gBvCx) + ((float)my-gBvCy)*((float)my-gBvCy));
    gBvValueBase = gBevelOp.width;
}
void BevelMotion(int mx, int my) {
    if (!gBevelOn || NumInputActive()) return;   // (con un valor tipeado manda el numero)
    const float d = sqrtf(((float)mx-gBvCx)*((float)mx-gBvCx) + ((float)my-gBvCy)*((float)my-gBvCy));
    float v = (d - gBvDistBase) / gBvPxByU;   // alejarse del centro = mas ancho
    if (gBevelOp.widthType == 3) v *= 100.0f;   // (Percent: en por ciento)
    v += gBvValueBase;
    gBvRawWidth = v > 0.0f ? v : 0.0f;
    BevelSetRaw();
    BevelApply();
}
// el valor TIPEADO (la barra de estado, el teclado numerico, el lapiz de Symbian): el ancho
void BevelNumber() {
    float v = 0.0f;
    if (!gBevelOn || !NumInputValue(v)) return;
    gBevelOp.width = gBvRawWidth = v > 0.0f ? v : 0.0f;   // (tipeado: el numero exacto, sin iman)
    BevelApply();
}
void BevelWheel(int dir) {
    if (!gBevelOn) return;
    gBevelOp.segments += dir > 0 ? 1 : -1;
    if (gBevelOp.segments < 1) gBevelOp.segments = 1; if (gBevelOp.segments > 100) gBevelOp.segments = 100;
    BevelApply();
}

void BevelCancel() {
    if (!gBevelOn) return;
    gBevelOn = false;
    NumInputReset();
    Mesh* m = (Mesh*)g_editMesh;
    if (m && m == gInMesh) { InsetRestore(m, true); if (!gBvCenters.empty()) m->SelectFacesByCenter(gBvCenters); }
    gInSnap.has = false; gBvHasSnap = false;
    BevelHint();
    g_redraw = true;
}

void BevelConfirm() {
    if (!gBevelOn) return;
    gBevelOn = false;
    NumInputReset();
    BevelHint();
    Mesh* m = (Mesh*)g_editMesh;
    if (!m || m != gInMesh) { gInSnap.has = false; gBvHasSnap = false; return; }
    // el undo guarda la malla de ANTES: se vuelve a ella, se captura y se re-bisela (el snapshot queda para el panel)
    InsetRestore(m, true);
    UndoCaptureMeshGeo(m);
    if (!m->BevelEdit(gBevelOp, gBvSelRep, gBvEdges)) m->GenerateRender();
    OpenRedoBevelPanel(m);
    g_redraw = true;
}

// LAS FLECHAS MANTENIDAS del telefono (ApplyArrows3D, con aceleracion): 'px' pixeles de pantalla (signo = agrandar /
// achicar) sobre lo que maneja el modal: el ancho del bevel, o el grosor / la profundidad del inset. Va sobre el valor
// SIN redondear: con el iman, el aplicado salta de paso redondo en paso redondo mientras se mantiene
void MeshModalArrowPx(float px) {
    if (gBevelOn) {
        float step = px / gBvPxByU; if (gBevelOp.widthType == 3) step *= 100.0f;
        NumInputReset();
        gBvRawWidth += step; BevelSetRaw(); BevelApply();
    } else if (gInsetOn) {
        const float step = px / gInPxByU;
        NumInputReset();
        if (gInProf) gInRawDepth += step; else { gInRawThick += step; if (gInRawThick < 0.0f) gInRawThick = 0.0f; }
        InsetSetRaw(); InsetApply();
    }
}

bool BevelKey(int key) {
    if (!gBevelOn) return false;
    switch (key) {
        case W3dK_RETURN: case W3dK_KP_ENTER: BevelConfirm(); return true;
        case W3dK_ESCAPE: case W3dK_BACKSPACE: BevelCancel(); return true;
        case W3dK_UP: case W3dK_PGUP:   BevelWheel(+1); return true;
        case W3dK_DOWN: case W3dK_PGDN: BevelWheel(-1); return true;
        // izquierda / derecha achican / agrandan el ANCHO (el teclado del telefono, sin mouse): de a 10 px
        case W3dK_RIGHT: case W3dK_LEFT: {
            float step = (key == W3dK_RIGHT ? 10.0f : -10.0f) / gBvPxByU;
            if (gBevelOp.widthType == 3) step *= 100.0f;
            NumInputReset();
            gBvRawWidth = gBevelOp.width + step;
            if (g_snap.enabled) { const float cur = gBevelOp.width;   // con el iman, al menos UN paso redondo por pulsacion
                for (int i = 0; i < 1000 && gBvRawWidth > 0.0f && ModalSnap(gBvRawWidth, gBvPxByU, gBevelOp.widthType == 3) == cur; i++) gBvRawWidth += step; }
            BevelSetRaw();
            BevelApply(); return true;
        }
        case W3dK_V: gBevelOp.affects = gBevelOp.affects ? 0 : 1; BevelApply(); return true;
    }
    return true;   // (el resto no hace nada mientras dura)
}

// =====================================================================================================================
//  EL CUCHILLO (K en Edit Mode): el modal. La op es Mesh::KnifeCutEdit (MeshEdit.cpp). Pasando el mouse por la malla se
//  engancha a un VERTICE (recuadro verde, como el snap), a una ARISTA (que se pinta de amarillo: el corte va a caer justo
//  sobre ella) o queda en la CARA, tambien en el medio. Click = un punto; Enter/Espacio = cortar; Esc/click der =
//  cancelar; Backspace = saca el ultimo punto; Z = cortar tambien lo de atras (lo que no se ve)
// =====================================================================================================================
static bool gKnifeOn = false, gKnifeThrough = false;
static std::vector<KnifePoint> gKnifePts;          // los clicks (LOCAL de la malla)
static KnifePoint gKnifeHover; static bool gKnifeHoverOk = false;
static float gKnifeHoverSx = 0.0f, gKnifeHoverSy = 0.0f;   // el enganche en la pantalla (coords del viewport)
static Vector3 gKnifeEdgeA, gKnifeEdgeB;           // la arista enganchada (LOCAL), para pintarla de amarillo
static std::vector<GLfloat> gKnifeCrossFixed, gKnifeCrossHover;   // los cruces (LOCAL): donde van a nacer vertices
bool KnifeActive() { return gKnifeOn; }
bool KnifeThrough() { return gKnifeThrough; }
int  KnifePoints() { return (int)gKnifePts.size(); }
int  KnifeHoverKind() { return gKnifeHoverOk ? gKnifeHover.kind : -1; }

// el ojo y hacia donde mira, en el espacio LOCAL de la malla
static bool KnifeView(Mesh* m, Vector3& eye, Vector3& fwd, bool& ortho, Matrix4& W, Matrix4& Wi) {
    if (!Viewport3DActive || !m) return false;
    m->GetWorldMatrix(W);
    if (!Matrix4::InvertAffine(W, Wi)) return false;
    const Viewport3D::Lens L = Viewport3DActive->CurrentLens();
    eye = Wi * L.pos;
    fwd = (Wi * (L.pos + L.fwd)) - eye;
    const float l = fwd.Length(); if (l < 1e-12f) return false;
    fwd = fwd * (1.0f / l); ortho = L.ortho;
    return true;
}
// el rayo de mundo del pixel (lx,ly) del viewport (la inversa de ProjectPoint, como el del gizmo)
static bool KnifePixelRay(Viewport3D* vp, float lx, float ly, Vector3& o, Vector3& d) {
    if (vp->width <= 0 || vp->height <= 0) return false;
    const Viewport3D::Lens L = vp->CurrentLens();
    const float z = (L.zoom > 1e-4f) ? L.zoom : 1.0f;
    const float u = ((lx / (float)vp->width * 2.0f - 1.0f) - L.panX) / z * L.sx;
    const float v = ((1.0f - ly / (float)vp->height * 2.0f) - L.panY) / z * L.sy;
    if (L.ortho) { o = L.pos + L.right * u + L.arr * v; d = L.fwd; }
    else { o = L.pos; d = (L.fwd + L.right * u + L.arr * v).Normalized(); }
    return true;
}
// lo que hay bajo el mouse (mx,my: coords de ventana): vertice, arista o cara
static bool KnifePick(int mx, int my, KnifePoint& out, float& sx, float& sy) {
    Viewport3D* vp = Viewport3DActive; Mesh* m = (Mesh*)g_editMesh;
    if (!vp || !m || !m->vertex) return false;
    vp->BindView();
    Vector3 eye, fwd; bool ortho; Matrix4 W, Wi;
    if (!KnifeView(m, eye, fwd, ortho, W, Wi)) return false;
    const float lx = (float)(mx - vp->x), ly = (float)(my - vp->y);
    Vector3 o, d; if (!KnifePixelRay(vp, lx, ly, o, d)) return false;
    const Vector3 oL = Wi * o; Vector3 dL = (Wi * (o + d)) - oL; dL = dL.Normalized();
    Vector3 hit; int face = -1;
    const bool onFace = m->KnifeRayFace(oL, dL, hit, face);
    // las caras donde buscar vertices y aristas cerca del mouse: la de abajo del mouse o, si el mouse esta afuera de la
    // malla (en una SILUETA, como el borde de un cubo visto de frente), todas, pero solo lo que se VE
    std::vector<int> cand;
    if (onFace) cand.push_back(face);
    else for (size_t f = 0; f < m->faces3d.size(); f++) cand.push_back((int)f);
    const bool hasRep = ((int)m->posRep.size() == m->vertexSize);
    extern bool ToolbarUsesTouch();
    const float R = (float)((ToolbarUsesTouch() ? 22 : 11) * (GlobalScale > 0 ? GlobalScale : 1));
    struct Vis { static bool Of(Mesh* m, const Vector3& P, const Vector3& eye, const Vector3& fwd, bool ortho) {
        Vector3 o, d; float tP;
        if (ortho) { tP = 1e4f; o = P - fwd * tP; d = fwd; }
        else { d = P - eye; tP = d.Length(); if (tP < 1e-9f) return true; d = d * (1.0f / tP); o = eye; }
        Vector3 h; int f; if (!m->KnifeRayFace(o, d, h, f)) return true;
        return (h - o).Length() >= tP * (1.0f - 1e-3f) - 1e-4f; } };
    // 1) un VERTICE cerca del mouse (en pantalla)
    int bestV = -1; float bestD = R * R;
    for (size_t ci = 0; ci < cand.size(); ci++) {
        const std::vector<int>& idx = m->faces3d[(size_t)cand[ci]].idx;
        for (size_t c = 0; c < idx.size(); c++) {
            const int g = idx[c]; if (g < 0 || g >= m->vertexSize) continue;
            const Vector3 P(m->vertex[g*3], m->vertex[g*3+1], m->vertex[g*3+2]);
            float px, py; if (!vp->ProjectPoint(W * P, px, py)) continue;
            const float dd = (px - lx) * (px - lx) + (py - ly) * (py - ly);
            if (dd < bestD && (onFace || Vis::Of(m, P, eye, fwd, ortho))) { bestD = dd; bestV = g; }
        }
    }
    if (bestV >= 0) {
        out = KnifePoint(); out.kind = KnifePoint::Vertex; out.a = hasRep ? m->posRep[(size_t)bestV] : bestV;
        out.p = Vector3(m->vertex[out.a*3], m->vertex[out.a*3+1], m->vertex[out.a*3+2]);
    } else {
        // 2) una ARISTA cerca del mouse: el punto de la arista mas cercano al rayo (en 3D: la perspectiva no lo corre)
        int bestG0 = -1, bestG1 = -1; bestD = R * R * 0.5f; float bestT = 0.5f;
        for (size_t ci = 0; ci < cand.size(); ci++) {
            const std::vector<int>& idx = m->faces3d[(size_t)cand[ci]].idx; const int n = (int)idx.size();
            for (int c = 0; c < n; c++) {
                const int g0 = idx[(size_t)c], g1 = idx[(size_t)((c + 1) % n)];
                if (g0 < 0 || g0 >= m->vertexSize || g1 < 0 || g1 >= m->vertexSize) continue;
                const Vector3 A(m->vertex[g0*3], m->vertex[g0*3+1], m->vertex[g0*3+2]), B(m->vertex[g1*3], m->vertex[g1*3+1], m->vertex[g1*3+2]);
                float ax, ay, bx, by;
                if (!vp->ProjectPoint(W * A, ax, ay) || !vp->ProjectPoint(W * B, bx, by)) continue;
                const float ex = bx - ax, ey = by - ay, l2 = ex * ex + ey * ey; if (l2 < 1e-6f) continue;
                float s2 = ((lx - ax) * ex + (ly - ay) * ey) / l2; if (s2 < 0) s2 = 0; if (s2 > 1) s2 = 1;
                const float qx = ax + ex * s2 - lx, qy = ay + ey * s2 - ly, dd = qx * qx + qy * qy;
                if (dd >= bestD) continue;
                const Vector3 u = B - A, w0 = A - oL;
                const float a = u.Dot(u), b = u.Dot(dL), d2 = u.Dot(w0), e = dL.Dot(w0), den = a - b * b;
                float t = (den > 1e-12f) ? (b * e - d2) / den : s2; if (t < 0) t = 0; if (t > 1) t = 1;
                if (!onFace && !Vis::Of(m, A + u * t, eye, fwd, ortho)) continue;
                bestD = dd; bestG0 = g0; bestG1 = g1; bestT = t;
            }
        }
        if (bestG0 >= 0) {
            const Vector3 A(m->vertex[bestG0*3], m->vertex[bestG0*3+1], m->vertex[bestG0*3+2]), B(m->vertex[bestG1*3], m->vertex[bestG1*3+1], m->vertex[bestG1*3+2]);
            out = KnifePoint(); out.kind = KnifePoint::Edge;
            out.a = hasRep ? m->posRep[(size_t)bestG0] : bestG0; out.b = hasRep ? m->posRep[(size_t)bestG1] : bestG1; out.t = bestT;
            out.p = A + (B - A) * bestT;
            gKnifeEdgeA = A; gKnifeEdgeB = B;
        } else if (onFace) {
            // 3) la CARA, donde la toca el rayo
            out = KnifePoint(); out.kind = KnifePoint::Face; out.face = face; out.p = hit;
        } else return false;
    }
    if (!vp->ProjectPoint(W * out.p, sx, sy)) { sx = lx; sy = ly; }
    return true;
}
static void KnifeCrossesOf(Mesh* m, const std::vector<KnifePoint>& seq, size_t from, std::vector<GLfloat>& out) {
    out.clear();
    Vector3 eye, fwd; bool ortho; Matrix4 W, Wi;
    if (!KnifeView(m, eye, fwd, ortho, W, Wi)) return;
    for (size_t i = from; i + 1 < seq.size(); i++) {
        std::vector<KnifePoint> cr; m->KnifeCrossings(seq[i], seq[i + 1], eye, fwd, ortho, gKnifeThrough, cr);
        for (size_t j = 0; j < cr.size(); j++) { out.push_back(cr[j].p.x); out.push_back(cr[j].p.y); out.push_back(cr[j].p.z); }
    }
}
static void KnifeHint() {
    if (!gKnifeOn) { NotifyHintClear(); return; }
    NotifyHint(gKnifeThrough ? T("Knife (cutting through): click = point, Enter = cut, Esc = cancel, Backspace = undo point, Z = only what you see")
                             : T("Knife: click = point, Enter = cut, Esc = cancel, Backspace = undo point, Z = cut through"));
}
static void KnifeRefreshHover(int mx, int my) {
    gKnifeHoverOk = KnifePick(mx, my, gKnifeHover, gKnifeHoverSx, gKnifeHoverSy);
    gKnifeCrossHover.clear();
    if (gKnifeHoverOk && !gKnifePts.empty() && g_editMesh) {
        std::vector<KnifePoint> seq; seq.push_back(gKnifePts.back()); seq.push_back(gKnifeHover);
        KnifeCrossesOf((Mesh*)g_editMesh, seq, 0, gKnifeCrossHover);
    }
}
void KnifeCancel() {
    if (!gKnifeOn) return;
    gKnifeOn = false; gKnifePts.clear(); gKnifeHoverOk = false; gKnifeCrossFixed.clear(); gKnifeCrossHover.clear();
    NotifyHintClear(); g_redraw = true;
}
void KnifeStart(int mx, int my) {
    if (InteractionMode != EditMode || !g_editMesh) { Notify(T("Knife: enter Edit Mode first"), true); return; }
    gKnifeOn = true; gKnifeThrough = false; gKnifePts.clear(); gKnifeCrossFixed.clear();   // (cada K arranca viendo solo lo de adelante)
    KnifeRefreshHover(mx, my);
    KnifeHint(); g_redraw = true;
}
void KnifeMotion(int mx, int my) {
    if (!gKnifeOn) return;
    if (!g_editMesh || InteractionMode != EditMode) { KnifeCancel(); return; }
    KnifeRefreshHover(mx, my);
    g_redraw = true;
}
void KnifeClickLeft(int mx, int my) {
    if (!gKnifeOn) return;
    KnifeRefreshHover(mx, my);
    if (!gKnifeHoverOk) return;   // (afuera de la malla: nada)
    if (!gKnifePts.empty() && (gKnifePts.back().p - gKnifeHover.p).Length() < 1e-6f) return;
    gKnifePts.push_back(gKnifeHover);
    KnifeCrossesOf((Mesh*)g_editMesh, gKnifePts, 0, gKnifeCrossFixed);
    gKnifeCrossHover.clear();
    g_redraw = true;
}
void KnifeBack() {
    if (!gKnifeOn || gKnifePts.empty()) return;
    gKnifePts.pop_back();
    if (g_editMesh) KnifeCrossesOf((Mesh*)g_editMesh, gKnifePts, 0, gKnifeCrossFixed);
    gKnifeCrossHover.clear(); g_redraw = true;
}
void KnifeToggleThrough() {
    if (!gKnifeOn) return;
    gKnifeThrough = !gKnifeThrough;
    if (g_editMesh) KnifeCrossesOf((Mesh*)g_editMesh, gKnifePts, 0, gKnifeCrossFixed);
    gKnifeCrossHover.clear(); KnifeHint(); g_redraw = true;
}
void KnifeConfirm() {
    if (!gKnifeOn) return;
    Mesh* m = (Mesh*)g_editMesh;
    bool cut = false;
    if (m && gKnifePts.size() >= 2) {
        Vector3 eye, fwd; bool ortho; Matrix4 W, Wi;
        if (KnifeView(m, eye, fwd, ortho, W, Wi)) cut = m->KnifeCutEdit(gKnifePts, eye, fwd, ortho, gKnifeThrough);
    }
    const bool hadPoints = gKnifePts.size() >= 2;
    KnifeCancel();
    if (hadPoints && !cut) Notify(T("Knife: the cut does not cross any face"), true);
}
// el corte encima de la escena (LOCAL de la malla): las lineas, los puntos, los cruces y la arista enganchada
void KnifeRenderPreview() {
    if (!gKnifeOn || !g_editMesh) return;
    Mesh* m = (Mesh*)g_editMesh;
    Matrix4 W; m->GetWorldMatrix(W);
    namespace gfx = w3dEngine;
    gfx::PushMatrix(); gfx::MultMatrix(W.m);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::DepthTest);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray); gfx::DisableArray(gfx::TexCoordArray);
    gfx::EnableArray(gfx::VertexArray);
    const float gs = (float)(GlobalScale > 0 ? GlobalScale : 1);
    // la ARISTA enganchada, en amarillo: el corte va a caer sobre ella
    if (gKnifeHoverOk && gKnifeHover.kind == KnifePoint::Edge) {
        const GLfloat e[6] = { gKnifeEdgeA.x, gKnifeEdgeA.y, gKnifeEdgeA.z, gKnifeEdgeB.x, gKnifeEdgeB.y, gKnifeEdgeB.z };
        gfx::Color4f(1.0f, 0.9f, 0.1f, 1.0f); gfx::LineWidth(3.0f * gs);
        gfx::VertexPointer3f(0, e); gfx::DrawLines(2);
    }
    // el corte: los puntos puestos y el tramo hasta el mouse
    std::vector<GLfloat> L;
    for (size_t i = 0; i < gKnifePts.size(); i++) { L.push_back(gKnifePts[i].p.x); L.push_back(gKnifePts[i].p.y); L.push_back(gKnifePts[i].p.z); }
    const size_t nFixed = gKnifePts.size();
    if (gKnifeHoverOk && !gKnifePts.empty()) { L.push_back(gKnifeHover.p.x); L.push_back(gKnifeHover.p.y); L.push_back(gKnifeHover.p.z); }
    if (L.size() >= 6) {
        std::vector<GLfloat> seg;
        for (size_t i = 0; i + 5 < L.size(); i += 3) for (int q = 0; q < 6; q++) seg.push_back(L[i + (size_t)q]);
        gfx::Color4f(0.2f, 1.0f, 0.35f, 1.0f); gfx::LineWidth(2.0f * gs);
        gfx::VertexPointer3f(0, &seg[0]); gfx::DrawLines((int)(seg.size() / 3));
    }
    gfx::LineWidth(1.0f);
    // donde van a nacer los vertices (cruces): rojo; los clicks: verde
    gfx::PointSize(6.0f * gs);
    gfx::Color4f(1.0f, 0.3f, 0.25f, 1.0f);
    if (!gKnifeCrossFixed.empty()) { gfx::VertexPointer3f(0, &gKnifeCrossFixed[0]); gfx::DrawPoints((int)(gKnifeCrossFixed.size() / 3)); }
    if (!gKnifeCrossHover.empty()) { gfx::VertexPointer3f(0, &gKnifeCrossHover[0]); gfx::DrawPoints((int)(gKnifeCrossHover.size() / 3)); }
    if (nFixed > 0) { gfx::Color4f(0.2f, 1.0f, 0.35f, 1.0f); gfx::PointSize(8.0f * gs); gfx::VertexPointer3f(0, &L[0]); gfx::DrawPoints((int)nFixed); }
    gfx::PointSize(1.0f);
    gfx::Enable(gfx::DepthTest);
    gfx::PopMatrix();
}
// el ENGANCHE bajo el mouse, en la pantalla: el recuadro verde del snap en un vertice; un rombo en una arista; un punto
// en la cara
void KnifeRenderOverlay(Viewport3D* vp) {
    if (!gKnifeOn || !gKnifeHoverOk || !vp || vp != Viewport3DActive) return;
    namespace gfx = w3dEngine;
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, vp->width, vp->height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);
    extern bool ToolbarUsesTouch();
    const int gs = GlobalScale > 0 ? GlobalScale : 1;
    const float h = (float)((ToolbarUsesTouch() ? 9 : 5) * gs), x = gKnifeHoverSx, y = gKnifeHoverSy;
    if (gKnifeHover.kind == KnifePoint::Vertex) gfx::Color4f(0.15f, 1.0f, 0.2f, 1.0f);
    else if (gKnifeHover.kind == KnifePoint::Edge) gfx::Color4f(1.0f, 0.9f, 0.1f, 1.0f);
    else gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    for (int k = 0; k < gs; k++) {
        const float r = h + (float)k;
        if (gKnifeHover.kind == KnifePoint::Vertex) {
            const GLfloat b[] = { x-r,y-r, x+r,y-r,  x+r,y-r, x+r,y+r,  x+r,y+r, x-r,y+r,  x-r,y+r, x-r,y-r };
            gfx::VertexPointer2f(0, b); gfx::DrawLines(8);
        } else if (gKnifeHover.kind == KnifePoint::Edge) {
            const GLfloat b[] = { x,y-r, x+r,y,  x+r,y, x,y+r,  x,y+r, x-r,y,  x-r,y, x,y-r };
            gfx::VertexPointer2f(0, b); gfx::DrawLines(8);
        } else {
            const float q = r * 0.5f;
            const GLfloat b[] = { x-q,y, x+q,y,  x,y-q, x,y+q };
            gfx::VertexPointer2f(0, b); gfx::DrawLines(4);
        }
    }
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
    gfx::Invalidate();
}
