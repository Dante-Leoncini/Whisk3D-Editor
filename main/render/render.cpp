#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render.h"
#include "w3dTexture.h"     // LoadTexture: el icono del parlante
#include <string>
#include "objects/Light.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/core/UI.h" // GlobalScale (point sprites relativos)
#include "WhiskUI/draw/icons.h" // IconSizeGS: el parlante del 3D, del tamano del icono del panel
#include "ui/W3dColors.h" // W3dColores: colores del editor (ejes de transformacion)
#include "objects/Mesh.h"          // Mesh + g_meshOverlayHook (hook de overlays)
#include "objects/RenderColors.h"  // gRenderColors / RC_selActive (color del contorno)
#include "render/OpcionesRender.h" // g_mostrarOverlays (master de overlays)
#ifndef W3D_SYMBIAN
#include "GeometriaUI/GeometriaUI.h" // LineaLightVertex/LineaEdge (gizmo de la luz; PC)
#include "variables.h"               // showOverlayGlobal
#include "W3dRaices.h"               // las otras escenas abiertas: sus iconos tambien
#include "script/W3dScript.h"        // (jugando no: el Play dibuja su base y sus capas)
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
        m->RenderEditOverlay();   // (el X-Ray de la jaula ya lo maneja EditMesh::Render)
    } else if (m->select && w3dVerSeleccion) {   // "Ver seleccion" OFF (menu Select): sin contorno
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
// ADEMAS una linea local (0,0,0 -> 0,-3,0) que quedaba mal/redundante -> se quito. Sin registrar
// el hook, Light::RenderObject no dibuja ninguna linea extra (g_lightOverlayHook queda NULL).
#endif // !W3D_SYMBIAN

// Definiciones de funciones

void RenderLinkLines(Object* obj){
    // espacio MUNDO con GetGlobalPosition (el viejo trasladaba (x,z,y)
    // sin la rotacion del padre: la linea quedaba mal al emparentar)
    static GLfloat pool[16][6]; // pool rotativo (captura tardia del driver)
    static int slot = 0;
    for (size_t c = 0; c < obj->Childrens.size(); c++) {
        Object* objChild = obj->Childrens[c];
        if (!objChild->visible || !objChild->showRelantionshipsLines ) continue;
        if (objChild->getType() == ObjectType::ui || objChild->getType() == ObjectType::texto2d ||
            objChild->getType() == ObjectType::imagen2d ||
            objChild->getType() == ObjectType::rect2d ||
            objChild->getType() == ObjectType::cont2d ||
            objChild->getType() == ObjectType::slice9 ||
            objChild->getType() == ObjectType::boton2d ||
            objChild->getType() == ObjectType::expandir2d ||
            objChild->getType() == ObjectType::video2d) continue; // sin linea de parent para la UI 2D
        // Culling se comporta como Collection: es un contenedor ESTRUCTURAL (agrupa 100s de props para
        // cullear), no una relacion padre-hijo real -> no dibuja lineas a sus hijos (sino satura el editor).
        if (obj->getType()!= ObjectType::collection && obj->getType() != ObjectType::baseObject &&
            obj->getType() != ObjectType::culling){
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

void DibujarOrigen(Object* obj){
    if (!obj->visible) return;
    if (obj->getType() == ObjectType::ui) return;   // la UI no vive en el 3D: ni ella ni sus hijos 2D

    w3dEngine::PushMatrix();

    Matrix4 local;                 // LOCAL, no obj->M: ver el comentario de 'M' en Objects.h
    obj->GetMatrix(local);
    w3dEngine::MultMatrix(local.m);

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
// VISTA LOCAL ("/"): lo que no esta aislado tampoco muestra su icono ni sus lineas (Objects.cpp: g_localView*)
#include <set>
extern bool g_localViewActivo;
extern const std::set<Object*>* g_localViewVisibles;
static bool FueraDeVistaLocal(Object* o) { return g_localViewActivo && g_localViewVisibles && !g_localViewVisibles->count(o); }   // (los padres de paso tampoco)

// tambien oculta la linea de la lampara al piso)
static bool VisibleEnArbol(Object* o) {
    while (o) {
        if (!o->visible) return false;
        o = o->Parent;
    }
    return true;
}

// LA GUIA de una luz, con el color que ya puso el que llama:
//   SOL: una linea hacia donde alumbra HASTA EL PISO (la altura 0), SIEMPRE (seleccionado o no); si no apunta al
//        piso, 10 m. FOCO: su cono (8 aristas y el circulo del borde) del largo de su distancia. PUNTUAL: la esfera
//        de su distancia (tres circulos). El cono y la esfera, solo con la luz seleccionada ('sel').
// Pool propio rotativo (como el de las lineas: el driver puede leer tarde)
static void DibujarGuiaLuz(Light* L, bool sel) {
    const bool foco = !L->direccional && L->spotCutoff < 179.5f;
    const bool punto = !L->direccional && !foco;
    if (!L->direccional && !sel) return;
    if (punto && L->distancia <= 0.0f) return;   // (sin limite: no hay esfera que mostrar)
    static GLfloat pool[8][192 * 3];
    static int slot = 0;
    GLfloat* v = pool[slot]; slot = (slot + 1) % 8;
    Matrix4 W; L->GetWorldMatrix(W);
    const Vector3 p(W.m[12], W.m[13], W.m[14]);
    Vector3 ex(W.m[0], W.m[1], W.m[2]), ey(W.m[4], W.m[5], W.m[6]), ez(W.m[8], W.m[9], W.m[10]);
    ex = ex.Normalized(); ey = ey.Normalized(); ez = ez.Normalized();
    int n = 0;
    if (L->direccional) {   // SOL: alumbra hacia su -Y local, hasta cortar el piso
        float largo = 10.0f;
        if (ey.y > 0.02f && p.y > 0.0f) { largo = p.y / ey.y; if (largo > 1000.0f) largo = 1000.0f; }
        v[0] = p.x; v[1] = p.y; v[2] = p.z;
        v[3] = p.x - ey.x * largo; v[4] = p.y - ey.y * largo; v[5] = p.z - ey.z * largo;
        n = 2;
    } else if (punto) {     // PUNTUAL: la esfera de su distancia (en el mundo: no la deforma la escala del objeto)
        const float r = L->distancia;
        const int N = 32;
        for (int pl = 0; pl < 3; pl++)
            for (int i = 0; i < N; i++) {
                const float a0 = i * (6.2831853f / N), a1 = (i + 1) * (6.2831853f / N);
                const float c0 = cosf(a0) * r, s0 = sinf(a0) * r, c1 = cosf(a1) * r, s1 = sinf(a1) * r;
                GLfloat* q = &v[n * 3];
                if (pl == 0)      { q[0] = p.x + c0; q[1] = p.y + s0; q[2] = p.z; q[3] = p.x + c1; q[4] = p.y + s1; q[5] = p.z; }
                else if (pl == 1) { q[0] = p.x + c0; q[1] = p.y; q[2] = p.z + s0; q[3] = p.x + c1; q[4] = p.y; q[5] = p.z + s1; }
                else              { q[0] = p.x; q[1] = p.y + c0; q[2] = p.z + s0; q[3] = p.x; q[4] = p.y + c1; q[5] = p.z + s1; }
                n += 2;
            }
    } else {       // FOCO: hacia su -Z local, el cono de 'spotCutoff' grados
        // el largo del cono: su DISTANCIA en metros (hasta donde alumbra); sin limite (0), un cono corto de 2 m
        float largo = L->distancia > 0.0f ? L->distancia : 2.0f; if (largo < 0.05f) largo = 0.05f; if (largo > 1000.0f) largo = 1000.0f;
        float ang = L->spotCutoff; if (ang > 85.0f) ang = 85.0f;
        const float r = largo * tanf(ang * 3.14159265f / 180.0f);
        const Vector3 c(p.x - ez.x * largo, p.y - ez.y * largo, p.z - ez.z * largo);
        for (int i = 0; i < 24; i++) {
            const float a0 = i * (6.2831853f / 24.0f), a1 = (i + 1) * (6.2831853f / 24.0f);
            const float c0 = cosf(a0) * r, s0 = sinf(a0) * r, c1 = cosf(a1) * r, s1 = sinf(a1) * r;
            GLfloat* q = &v[n * 3];
            q[0] = c.x + ex.x * c0 + ey.x * s0; q[1] = c.y + ex.y * c0 + ey.y * s0; q[2] = c.z + ex.z * c0 + ey.z * s0;
            q[3] = c.x + ex.x * c1 + ey.x * s1; q[4] = c.y + ex.y * c1 + ey.y * s1; q[5] = c.z + ex.z * c1 + ey.z * s1;
            n += 2;
            if ((i % 3) == 0) {   // (8 aristas del vertice al borde)
                GLfloat* e = &v[n * 3];
                e[0] = p.x; e[1] = p.y; e[2] = p.z; e[3] = q[0]; e[4] = q[1]; e[5] = q[2];
                n += 2;
            }
        }
    }
    w3dEngine::VertexPointer3f(0, v);
    w3dEngine::DrawLines(n);
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
        if (!VisibleEnArbol(Lights[l]) || FueraDeVistaLocal(Lights[l])) continue;
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
        DibujarGuiaLuz(Lights[l], Lights[l]->select);   // (el cono y la esfera, solo de la SELECCIONADA; la linea del sol, siempre)
    }
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
}

void DibujarIcono3D(Object* obj){
    if (!obj->visible || FueraDeVistaLocal(obj)) return;

    w3dEngine::PushMatrix();
    Matrix4 local;                 // LOCAL, no obj->M: ver el comentario de 'M' en Objects.h
    obj->GetMatrix(local);
    w3dEngine::MultMatrix(local.m);

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

// el PARLANTE de los objetos Sonido: el mismo punto con icono que la lampara de las luces (parlante.png del skin)
static void DibujarIconoSonido3D(Object* obj){
    if (!obj->visible || FueraDeVistaLocal(obj)) return;
    w3dEngine::PushMatrix();
    Matrix4 local; obj->GetMatrix(local);
    w3dEngine::MultMatrix(local.m);
    if (obj->getType() == ObjectType::sonido){
        if (ObjActivo == obj && obj->select)  w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        else if (obj->select)                 w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        else                                  w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::grisUI)]);
        w3dEngine::DrawPoints(1);
    }
    for (size_t c = 0; c < obj->Childrens.size(); c++) DibujarIconoSonido3D(obj->Childrens[c]);
    w3dEngine::PopMatrix();
}
static unsigned int TexParlante(){
    static bool probado = false; static unsigned int id = 0;
    if (!probado){
        probado = true;
        extern std::string g_skinDir;
        if (!w3dEngine::LoadTexture((g_skinDir + "parlante.png").c_str(), id)) id = 0;
    }
    return id;
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

    // (las OTRAS escenas abiertas en el editor tambien: se ven y se eligen junto a la activa)
    std::vector<Object*> raices(1, (Object*)SceneCollection);
    if (!W3dScriptHayPartida()) {
        std::vector<Object*> otras;
        W3dRaicesOtrasVisibles(otras);
        raices.insert(raices.end(), otras.begin(), otras.end());
    }
    for (size_t r = 0; r < raices.size(); r++) DibujarIcono3D(raices[r]);
    // los SONIDOS (overlay "Sounds"): su parlante
    { extern bool g_showSonidos;
      const unsigned int tp = g_showSonidos ? TexParlante() : 0;
      if (tp){
          // el MISMO tamano y la misma nitidez que el icono del panel: el icono (10 de los 16 del png) a IconSizeGS
          w3dEngine::BindTexture(tp);
#ifndef W3D_SYMBIAN
          w3dEngine::TexFilter(false);
#endif
          w3dEngine::PointSize((float)IconSizeGS * 16.0f / 10.0f);
          for (size_t r = 0; r < raices.size(); r++) DibujarIconoSonido3D(raices[r]);
      } }

    w3dEngine::PointSpriteCoordReplace(false);
    w3dEngine::Disable(w3dEngine::PointSprite);
    w3dEngine::DepthMask(true);
}