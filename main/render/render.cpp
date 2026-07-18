#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render.h"
#include "objects/Light.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/core/UI.h" // GlobalScale (point sprites relativos)
#include "ui/W3dColors.h" // W3dColores: colores del editor (ejes de transformacion)
#include "objects/Mesh.h"          // Mesh + g_meshOverlayHook (hook de overlays)
#include "objects/RenderColors.h"  // gRenderColors / RC_selActive (color del contorno)
#include "render/OpcionesRender.h" // g_mostrarOverlays (master de overlays)
#ifndef W3D_SYMBIAN
#include "GeometriaUI/GeometriaUI.h" // LineaLightVertex/LineaEdge (gizmo de la luz; PC)
#include "variables.h"               // showOverlayGlobal
#endif

// angulo (rad) en el vertice p del triangulo p-q-r (pondera la normal de cara en el vertex-normal)
static float AnguloEnVertice(const float* p, const float* q, const float* r) {
    float e1x=q[0]-p[0], e1y=q[1]-p[1], e1z=q[2]-p[2];
    float e2x=r[0]-p[0], e2y=r[1]-p[1], e2z=r[2]-p[2];
    float l1=sqrtf(e1x*e1x+e1y*e1y+e1z*e1z), l2=sqrtf(e2x*e2x+e2y*e2y+e2z*e2z);
    if (l1<1e-6f || l2<1e-6f) return 0.0f;
    float d=(e1x*e2x+e1y*e2y+e1z*e2z)/(l1*l2);
    if (d>1.0f) d=1.0f; if (d<-1.0f) d=-1.0f;
    return acosf(d);
}

// dibuja los pares de puntos de 'buf' como GL_LINES con el color de la paleta
static void DibujarLineasNormales(std::vector<GLfloat>& buf, int colorId) {
    if (buf.empty()) return;
    namespace gfx = w3dEngine;
    const float* c = gRenderColors[colorId];
    gfx::Color4f(c[0], c[1], c[2], 1.0f);
    gfx::VertexPointer3f(0, &buf[0]);
    gfx::DrawLines((int)(buf.size() / 3));
}

// PRECALCULA las lineas de los 3 overlays de normales en sus buffers (cacheado; se rehace solo
// si cambia la geometria o el largo L del slider). Antes era Mesh::CalcularOverlayNormales (core).
static void CalcularOverlayNormales(Mesh* m) {
    m->overlayLcache = OverlayNormalSize;
    const float L = OverlayNormalSize;
    const int nTri = m->facesSize / 3;
    const int nV = m->vertexSize;
    m->normFaceBuf.clear(); m->normCustomBuf.clear(); m->normVertBuf.clear();
    if (!m->vertex || !m->faces) return;

    // FACE (cian): UNA normal por CARA (ngon/quad/tri de faces3d, o por triangulo si no hay)
    if (!m->faces3d.empty()) {
        for (size_t fi = 0; fi < m->faces3d.size(); fi++) {
            const std::vector<int>& ring = m->faces3d[fi].idx;
            int nc = (int)ring.size(); if (nc < 3) continue;
            float cx=0,cy=0,cz=0, nx=0,ny=0,nz=0;
            for (int k=0;k<nc;k++){ GLfloat* p=&m->vertex[ring[k]*3]; cx+=p[0]; cy+=p[1]; cz+=p[2]; }
            cx/=(float)nc; cy/=(float)nc; cz/=(float)nc;
            for (int k=0;k<nc;k++){ // normal Newell del poligono
                GLfloat* a=&m->vertex[ring[k]*3]; GLfloat* b=&m->vertex[ring[(k+1)%nc]*3];
                nx+=(a[1]-b[1])*(a[2]+b[2]); ny+=(a[2]-b[2])*(a[0]+b[0]); nz+=(a[0]-b[0])*(a[1]+b[1]);
            }
            float ln=sqrtf(nx*nx+ny*ny+nz*nz); if (ln<1e-6f) continue; nx/=ln; ny/=ln; nz/=ln;
            m->normFaceBuf.push_back(cx); m->normFaceBuf.push_back(cy); m->normFaceBuf.push_back(cz);
            m->normFaceBuf.push_back(cx+nx*L); m->normFaceBuf.push_back(cy+ny*L); m->normFaceBuf.push_back(cz+nz*L);
        }
    } else {
        for (int t = 0; t < nTri; t++) {
            int i0=m->faces[t*3], i1=m->faces[t*3+1], i2=m->faces[t*3+2];
            GLfloat* p0=&m->vertex[i0*3]; GLfloat* p1=&m->vertex[i1*3]; GLfloat* p2=&m->vertex[i2*3];
            float ax=p1[0]-p0[0], ay=p1[1]-p0[1], az=p1[2]-p0[2];
            float bx=p2[0]-p0[0], by=p2[1]-p0[1], bz=p2[2]-p0[2];
            float nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
            float ln=sqrtf(nx*nx+ny*ny+nz*nz); if (ln<1e-6f) continue; nx/=ln; ny/=ln; nz/=ln;
            float cx=(p0[0]+p1[0]+p2[0])/3.0f, cy=(p0[1]+p1[1]+p2[1])/3.0f, cz=(p0[2]+p1[2]+p2[2])/3.0f;
            m->normFaceBuf.push_back(cx); m->normFaceBuf.push_back(cy); m->normFaceBuf.push_back(cz);
            m->normFaceBuf.push_back(cx+nx*L); m->normFaceBuf.push_back(cy+ny*L); m->normFaceBuf.push_back(cz+nz*L);
        }
    }

    // CUSTOM (magenta): la normal guardada en cada vertice (normals[])
    if (m->normals) {
        for (int i = 0; i < nV; i++) {
            GLfloat* p=&m->vertex[i*3];
            float nx=m->normals[i*3]/127.0f, ny=m->normals[i*3+1]/127.0f, nz=m->normals[i*3+2]/127.0f;
            float ln=sqrtf(nx*nx+ny*ny+nz*nz); if (ln<1e-6f) continue; nx/=ln; ny/=ln; nz/=ln;
            m->normCustomBuf.push_back(p[0]); m->normCustomBuf.push_back(p[1]); m->normCustomBuf.push_back(p[2]);
            m->normCustomBuf.push_back(p[0]+nx*L); m->normCustomBuf.push_back(p[1]+ny*L); m->normCustomBuf.push_back(p[2]+nz*L);
        }
    }

    // VERTEX (amarillo): promedio ponderado por angulo de las normales de cara, agrupado por POSICION
    {
        std::vector<float> acc(nV*3, 0.0f);
        for (int t = 0; t < nTri; t++) {
            int i0=m->faces[t*3], i1=m->faces[t*3+1], i2=m->faces[t*3+2];
            GLfloat* p0=&m->vertex[i0*3]; GLfloat* p1=&m->vertex[i1*3]; GLfloat* p2=&m->vertex[i2*3];
            float ax=p1[0]-p0[0], ay=p1[1]-p0[1], az=p1[2]-p0[2];
            float bx=p2[0]-p0[0], by=p2[1]-p0[1], bz=p2[2]-p0[2];
            float nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
            float ln=sqrtf(nx*nx+ny*ny+nz*nz); if (ln<1e-6f) continue; nx/=ln; ny/=ln; nz/=ln;
            float w0=AnguloEnVertice(p0,p1,p2), w1=AnguloEnVertice(p1,p0,p2), w2=AnguloEnVertice(p2,p0,p1);
            acc[i0*3]+=nx*w0; acc[i0*3+1]+=ny*w0; acc[i0*3+2]+=nz*w0;
            acc[i1*3]+=nx*w1; acc[i1*3+1]+=ny*w1; acc[i1*3+2]+=nz*w1;
            acc[i2*3]+=nx*w2; acc[i2*3+1]+=ny*w2; acc[i2*3+2]+=nz*w2;
        }
        const bool usarRep = ((int)m->posRep.size() == nV);
        std::vector<float> sum(nV*3, 0.0f);
        for (int i = 0; i < nV; i++) {
            int r = usarRep ? m->posRep[i] : i;
            sum[r*3]+=acc[i*3]; sum[r*3+1]+=acc[i*3+1]; sum[r*3+2]+=acc[i*3+2];
        }
        for (int i = 0; i < nV; i++) {
            int r = usarRep ? m->posRep[i] : i;
            if (r != i) continue; // una sola linea por representante de posicion
            float sx=sum[i*3], sy=sum[i*3+1], sz=sum[i*3+2];
            float ln=sqrtf(sx*sx+sy*sy+sz*sz); if (ln<1e-6f) continue; sx/=ln; sy/=ln; sz/=ln;
            GLfloat* pi=&m->vertex[i*3];
            m->normVertBuf.push_back(pi[0]); m->normVertBuf.push_back(pi[1]); m->normVertBuf.push_back(pi[2]);
            m->normVertBuf.push_back(pi[0]+sx*L); m->normVertBuf.push_back(pi[1]+sy*L); m->normVertBuf.push_back(pi[2]+sz*L);
        }
    }
}

// overlay de normales (vertex=amarillo / custom=magenta / face=cian). Antes era Mesh::RenderNormales (core).
static void RenderNormales(Mesh* m) {
    if (!OverlayVertexNormal && !OverlayCustomNormal && !OverlayFaceNormal) return;
    if (!m->vertex || !m->faces) return;
    namespace gfx = w3dEngine;
    if (m->overlayLcache != OverlayNormalSize) CalcularOverlayNormales(m);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::TexCoordArray);
    if (OverlayFaceNormal)   DibujarLineasNormales(m->normFaceBuf,   RC_normalFace);
    if (OverlayCustomNormal) DibujarLineasNormales(m->normCustomBuf, RC_normalCustom);
    if (OverlayVertexNormal) DibujarLineasNormales(m->normVertBuf,   RC_normalVert);
    gfx::VertexPointer3f(0, m->vertex); // no dejar el puntero en un buffer temporal
    gfx::Invalidate();
}

// El Core Mesh::RenderObject llama a este hook tras dibujar cada malla. Aca (editor) se decide
// y dibuja el contorno de seleccion / normales / overlay de edit. Se registra solo al arrancar
// (struct de abajo); en una app/juego sin editor el hook queda NULL y no se dibuja nada.
static void MeshOverlayHook(Mesh* m) {
    if (!g_mostrarOverlays) return;
    if ((Object*)m == g_editMesh) {
        m->RenderEditOverlay();
    } else if (m->select) {
        int cid = ((Object*)m == ObjActivo) ? RC_selActive : RC_selInactive;
        const float* col = gRenderColors[cid];
        m->RenderBordes(col, 3.0f, true); // pushBack=true: el contorno se empuja ATRAS (DepthRange), sin adelantar las caras
        RenderNormales(m);
    } else if (m->facesSize == 0 && !(m->genValido && m->genVertex && m->genFaces)) {
        // malla SIN caras propias NI malla generada (un wireframe suelto, no un perfil de modificador): dibuja sus
        // bordes como OVERLAY del editor asi no queda invisible en modo objeto (sino no hay forma de saber que hay
        // algo). Es overlay -> NO sale en el render final. Los verts sueltos no se dibujan (ok, alcanza con lineas).
        // Con malla generada (ej. Screw) NO se dibuja el perfil: en modo objeto solo se ve el resultado (como Blender).
        m->RenderBordes(gRenderColors[RC_wireframe], 1.5f, false);
    }
}
struct MeshOverlayHookReg { MeshOverlayHookReg() { g_meshOverlayHook = MeshOverlayHook; } };
static MeshOverlayHookReg g_meshOverlayHookReg;

#ifndef W3D_SYMBIAN
// NOTA: la luz se representa con (a) su ICONO (RenderIcons3D/DibujarIcono3D) y (b) la LINEA al piso
// (RenderLightLines), ambos con color de seleccion y gateados por el toggle "Lights". El viejo hook dibujaba
// ADEMAS una linea local (0,0,0 -> 0,-3,0) que quedaba mal/redundante -> se quito (pedido Dante). Sin registrar
// el hook, Light::RenderObject no dibuja ninguna linea extra (g_lightOverlayHook queda NULL).
#endif // !W3D_SYMBIAN

// Definiciones de funciones

void DrawnLines(int LineWidth, int cantidad, GLshort* vertexlines, GLushort* lineasIndices){
    W3dDrawLinesS(LineWidth, cantidad, vertexlines, lineasIndices);
}

void DrawnLines(int LineWidth, int cantidad, const GLshort* vertexlines, const GLushort* lineasIndices) {
    W3dDrawLinesS(LineWidth, cantidad, vertexlines, lineasIndices);
}

void RenderLinkLines(Object* obj){
    // espacio MUNDO con GetGlobalPosition (el viejo trasladaba (x,z,y)
    // sin la rotacion del padre: la linea quedaba mal al emparentar)
    static GLfloat pool[16][6]; // pool rotativo (captura tardia del driver)
    static int slot = 0;
    for (size_t c = 0; c < obj->Childrens.size(); c++) {
        Object* objChild = obj->Childrens[c];
        if (!objChild->visible || !objChild->showRelantionshipsLines ) continue;
        if (obj->getType()!= ObjectType::collection && obj->getType() != ObjectType::baseObject){
            Vector3 a = obj->GetGlobalPosition();
            Vector3 b = objChild->GetGlobalPosition();
            GLfloat* v = pool[slot];
            slot = (slot + 1) % 16;
            v[0] = a.x; v[1] = a.y; v[2] = a.z;
            v[3] = b.x; v[4] = b.y; v[5] = b.z;

            Vector3 diff = b - a;
            GLfloat distancia = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

            lineUV[3] = distancia*8;
            W3dDrawLinesF(v, LineaEdge, LineaEdgeSize);
        }
        RenderLinkLines(objChild);
    }
}

void DrawTransformAxis() {
    w3dEngine::PushMatrix();
    if (InteractionMode == ObjectMode){
        w3dEngine::Translatef(TransformPivotPoint.x/65000,
                     TransformPivotPoint.y/65000,
                     TransformPivotPoint.z/65000);
    }

    switch (axisSelect) {
        case X:
            w3dSetColor(W3dColores[W3dColor_ColorTransformX]);
            w3dEngine::DrawLinesIndexed(2, EjeRojo);
            break;
        case Y:
            w3dSetColor(W3dColores[W3dColor_ColorTransformY]);
            w3dEngine::DrawLinesIndexed(2, EjeVerde);
            break;
        case Z:
            w3dSetColor(W3dColores[W3dColor_ColorTransformZ]);
            w3dEngine::DrawLinesIndexed(2, EjeAzul);
            break;
        case XYZ:
            w3dSetColor(W3dColores[W3dColor_ColorTransformX]);
            w3dEngine::DrawLinesIndexed(2, EjeRojo);
            w3dSetColor(W3dColores[W3dColor_ColorTransformY]);
            w3dEngine::DrawLinesIndexed(2, EjeVerde);
            w3dSetColor(W3dColores[W3dColor_ColorTransformZ]);
            w3dEngine::DrawLinesIndexed(2, EjeAzul);
            break;
        // PLANOS (Shift+eje): dibujan SOLO los dos ejes del plano (el excluido NO)
        case PlaneX: // excluye X -> Y, Z
            w3dSetColor(W3dColores[W3dColor_ColorTransformY]);
            w3dEngine::DrawLinesIndexed(2, EjeVerde);
            w3dSetColor(W3dColores[W3dColor_ColorTransformZ]);
            w3dEngine::DrawLinesIndexed(2, EjeAzul);
            break;
        case PlaneY: // excluye Y -> X, Z
            w3dSetColor(W3dColores[W3dColor_ColorTransformX]);
            w3dEngine::DrawLinesIndexed(2, EjeRojo);
            w3dSetColor(W3dColores[W3dColor_ColorTransformZ]);
            w3dEngine::DrawLinesIndexed(2, EjeAzul);
            break;
        case PlaneZ: // excluye Z -> X, Y
            w3dSetColor(W3dColores[W3dColor_ColorTransformX]);
            w3dEngine::DrawLinesIndexed(2, EjeRojo);
            w3dSetColor(W3dColores[W3dColor_ColorTransformY]);
            w3dEngine::DrawLinesIndexed(2, EjeVerde);
            break;
    }
    w3dEngine::PopMatrix();
}

bool RenderAxisTransform(Object* obj) {
    bool found = false;
    w3dEngine::PushMatrix();

    obj->GetMatrix(obj->M);
    w3dEngine::MultMatrix(obj->M.m);

    if (obj == ObjActivo) {
        if (estado == rotacion || estado == EditScale){
            /*w3dEngine::Rotatef(obj->rotX, 1, 0, 0);
            w3dEngine::Rotatef(obj->rotZ, 0, 1, 0);
            w3dEngine::Rotatef(obj->rotY, 0, 0, 1);*/
        }
        if (estado == translacion || estado == rotacion || estado == EditScale){
            DrawTransformAxis();
        }
        found = true;
    }
    else if (!obj->Childrens.empty()){
        /*w3dEngine::Rotatef(obj->rotX, 1, 0, 0);
        w3dEngine::Rotatef(obj->rotZ, 0, 1, 0);
        w3dEngine::Rotatef(obj->rotY, 0, 0, 1);*/

        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            if (RenderAxisTransform(obj->Childrens[c])){
                found = true;
                break;
            }
        }
    }
    w3dEngine::PopMatrix();
    return found;
}

void DibujarOrigen(Object* obj){
    if (!obj->visible) return;

    w3dEngine::PushMatrix();

    obj->GetMatrix(obj->M);
    w3dEngine::MultMatrix(obj->M.m);

    if (obj->select || obj == ObjActivo){
        if (obj == ObjActivo){
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        }
        else {
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        }
        w3dEngine::DrawPoints(1);
    }

    if (!obj->Childrens.empty()){
        /*w3dEngine::Rotatef(obj->rotX, 1, 0, 0);
        w3dEngine::Rotatef(obj->rotZ, 0, 1, 0);
        w3dEngine::Rotatef(obj->rotY, 0, 0, 1);*/
        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            DibujarOrigen(obj->Childrens[c]);
        }
    }
    w3dEngine::PopMatrix();
}

void RenderOrigins(){
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(Textures[1]->iID);
#ifndef W3D_SYMBIAN
    // pixel perfect: sin filtrado (se veia borroso al escalar)
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    w3dEngine::VertexPointer3s(0, pointVertex);
    w3dEngine::PointSpriteCoordReplace(true);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::PointSprite);
    w3dEngine::PointSize(8 * GlobalScale); // origen: 8 relativo a la UI
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        DibujarOrigen(SceneCollection->Childrens[c]);
    }
    w3dEngine::PointSpriteCoordReplace(false);
    w3dEngine::Disable(w3dEngine::PointSprite);
}

// visible considerando TODOS los ancestros (ocultar la coleccion
// tambien oculta la linea de la lampara al piso)
static bool VisibleEnArbol(Object* o) {
    while (o) {
        if (!o->visible) return false;
        o = o->Parent;
    }
    return true;
}

void RenderLightLines(){
    // pool rotativo: nada de arrays de stack (captura tardia del driver)
    static GLfloat pool[16][6];
    static int slot = 0;
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::LineWidth(1);
    for (size_t l = 0; l < Lights.size(); l++) {
        if (!VisibleEnArbol(Lights[l])) continue;
        // mismo esquema de color que el icono de la luz
        if (Lights[l] == (Light*)ObjActivo && Lights[l]->select)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        else if (Lights[l]->select)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        else
            SetColorID(ColorID::grisUI, 0.7f);
        Vector3 p = Lights[l]->GetGlobalPosition();
        GLfloat* v = pool[slot];
        slot = (slot + 1) % 16;
        v[0] = p.x; v[1] = p.y; v[2] = p.z;
        v[3] = p.x; v[4] = 0.0f; v[5] = p.z;
        w3dEngine::VertexPointer3f(0, v);
        w3dEngine::DrawLines(2);
    }
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
}

void DibujarIcono3D(Object* obj){
    if (!obj->visible) return;

    w3dEngine::PushMatrix();
    obj->GetMatrix(obj->M);
    w3dEngine::MultMatrix(obj->M.m);

    extern bool g_showLights; // toggle "Lights" del submenu "Objects": oculta tambien el icono de la luz
    if (obj->getType() == ObjectType::light && g_showLights){
        if (ObjActivo == obj && obj->select)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        else if (obj->select)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        else
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::grisUI)]);
        w3dEngine::DrawPoints(1);
    }

    if (!obj->Childrens.empty()){
        /*w3dEngine::Rotatef(obj->rotX, 1, 0, 0);
        w3dEngine::Rotatef(obj->rotZ, 0, 1, 0);
        w3dEngine::Rotatef(obj->rotY, 0, 0, 1);*/
        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            DibujarIcono3D(obj->Childrens[c]);
        }
    }
    w3dEngine::PopMatrix();
}

void RenderIcons3D(){
    w3dEngine::DepthMask(false);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(Textures[4]->iID);
#ifndef W3D_SYMBIAN
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    w3dEngine::PointSpriteCoordReplace(true);
    w3dEngine::VertexPointer3s(0, pointVertex);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::PointSprite);
    w3dEngine::PointSize(16 * GlobalScale); // iconos de 16 relativos a la UI

    DibujarIcono3D(SceneCollection);

    w3dEngine::PointSpriteCoordReplace(false);
    w3dEngine::Disable(w3dEngine::PointSprite);
    w3dEngine::DepthMask(true);
}

void RenderVK(){
    // por implementar
}