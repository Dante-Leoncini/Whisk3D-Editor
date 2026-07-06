#include "ViewPorts/UVEditor.h"
#include "objects/Mesh.h"          // Mesh, MaterialGroup, Material, Texture
#include "objects/EditMesh.h"      // EditMesh (seleccion de caras para el Sync Selection)
#include "objects/Textures.h"      // Textures[] (atlas de iconos = Textures[0])
#include "w3dGraphics.h"           // w3dEngine (abstraccion grafica)
#include "WhiskUI/glesdraw.h"      // W3dPantallaAlto + helpers de dibujo
#include "WhiskUI/colores.h"       // ListaColores / ColorID
#include "WhiskUI/icons.h"         // IconType (botones-icono SelMode / Pivot)
#include "ViewPorts/Properties.h"  // PropsActivo (parte activa seleccionada)
#include "WhiskUI/Propieties/PropList.h" // PropListMeshParts
#include "render/OpcionesRender.h" // g_redraw (render event-driven)
#include "w3dTexture.h"             // w3dEngine::TextureSize (aspect ratio de la textura del UV editor)
#include "PopUp/PopUpBase.h"       // PopUpActive (file browser / popups modales tienen prioridad)
#include <vector>
#include <math.h> // atan2f/cosf/sinf/sqrtf (transform 2D)

namespace gfx = w3dEngine;

UVEditor::UVEditor() {
    zoom = 1.0f;
    panX = 0.0f; panY = 0.0f;
    syncSelection = true; // por defecto: ves todo el UV (con la seleccion resaltada)
    repeatTexture = false;
    mostrarChromeUV = false; // off por defecto: no gasta CPU dibujando el overlay (pedido Dante)
    lastMx = 0; lastMy = 0;
    uvXform = 0; uvXPivotU = uvXPivotV = uvXStartU = uvXStartV = 0.0f;
    uvCursorU = 0.5f; uvCursorV = 0.5f; // cursor 2D al centro por defecto
    uvSelMode = SelVertex;              // modo de seleccion propio del UV (default vertices)
    BarCrear();
    // botones de barra (ademas del icono [0]); los rutea LayoutClickBarraUV:
    BarButtons.push_back(new Button("View"));                         // [1] checkboxes (Sync/Repeat)
    BarButtons.push_back(new Button("", (int)IconType::selVertex));   // [2] SelMode UV (SOLO icono)
    BarButtons[2]->desplegable = true;
    BarButtons.push_back(new Button("", (int)IconType::pivotMedian)); // [3] Pivot (SOLO icono, = el 3D)
    BarButtons[3]->desplegable = true;
    BarButtons.push_back(new Button("Snap"));                        // [4] snap cursor<->seleccion
    BarButtons[4]->desplegable = true;
}

// modo de seleccion EFECTIVO del editor UV: en sync sigue al 3D; si no, el propio.
int UVEditor::ModoUV() const { return syncSelection ? EditSelectMode : uvSelMode; }

UVEditor::~UVEditor() {}

void UVEditor::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH); // el borde sigue el tamano (como Outliner/Properties)
}

// la mesh part SELECCIONADA en el panel de propiedades activo; si no hay, la 0.
static int UVParteActiva(Mesh* m) {
    if (PropsActivo && PropsActivo->propMeshParts &&
        !PropsActivo->propMeshParts->properties.empty()) {
        PropListMeshParts* lst =
            static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
        if (lst && lst->mesh == m && lst->selectIndex >= 0 &&
            lst->selectIndex < (int)m->materialsGroup.size())
            return lst->selectIndex;
    }
    return 0;
}

// un punto (u,v) del espacio UV -> pixel del viewport. V=0 va ARRIBA: la convencion del engine
// es V=0 = arriba de la imagen (stb top-first + el importador OBJ hace 1-v), asi la textura
// se ve DERECHA (no dada vuelta) y el wireframe queda alineado.
// aspecto de la textura activa (1,1 = cuadrada). Lo setea ParamsUV/Render segun la textura de la parte
// activa -> la 0..1 UV se dibuja como RECTANGULO con la proporcion real (ej 32x128 = alto), no siempre cuadrada.
static float g_uvAspU = 1.0f, g_uvAspV = 1.0f;
static inline void UVtoScreen(float u, float v, float cx, float cy, float s,
                              float& sx, float& sy) {
    sx = cx + (u - 0.5f) * s * g_uvAspU;
    sy = cy + (v - 0.5f) * s * g_uvAspV;
}
// calcula el aspecto (g_uvAspU/V) de la textura de la mesh part activa de m. mayor lado = 1.0.
static void CalcAspectoUV(class Mesh* m, int part) {
    g_uvAspU = g_uvAspV = 1.0f;
    if (!m) return;
    Material* mat = (part >= 0 && part < (int)m->materialsGroup.size()) ? m->materialsGroup[part].material : NULL;
    int tw = 0, th = 0;
    if (mat && mat->texture && mat->texture->iID && w3dEngine::TextureSize(mat->texture->iID, tw, th) && tw > 0 && th > 0) {
        if (tw >= th) g_uvAspV = (float)th / (float)tw; // ancha -> achica el alto
        else          g_uvAspU = (float)tw / (float)th; // alta  -> achica el ancho
    }
}

// distancia^2 de un punto al segmento ab (en pixeles) — para el pick de arista.
static inline float DistPtSeg2(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay; float l2 = dx*dx + dy*dy;
    float t = (l2 > 1e-6f) ? ((px-ax)*dx + (py-ay)*dy) / l2 : 0.0f;
    if (t < 0) t = 0; if (t > 1) t = 1;
    float qx = ax + t*dx, qy = ay + t*dy, ex = px - qx, ey = py - qy;
    return ex*ex + ey*ey;
}

// punto dentro del poligono (ray casting) — para el pick de cara (quads/ngons).
static bool PointInPoly(float px, float py, const float* pts, int n) {
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float xi = pts[i*2], yi = pts[i*2+1], xj = pts[j*2], yj = pts[j*2+1];
        if (((yi > py) != (yj > py)) && (px < (xj-xi)*(py-yi)/(yj-yi) + xi)) in = !in;
    }
    return in;
}

void UVEditor::Render() {
    const int glY = W3dPantallaAlto - y - height;

    // fondo
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    const float* bg = ListaColores[static_cast<int>(ColorID::background)];
    gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);

    gfx::Viewport(x, glY, width, height);
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Blend);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);

    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    const float cx = width * 0.5f + panX;
    const float cy = top + ch * 0.5f + panY;
    float baseSize = (float)(width < ch ? width : ch) * 0.8f;
    float s = baseSize * zoom; if (s < 1.0f) s = 1.0f;

    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh)
                  ? (Mesh*)ObjActivo : NULL;
    CalcAspectoUV(m, m ? UVParteActiva(m) : -1); // aspect ratio de la textura activa (antes de cualquier UVtoScreen)
    // botones SelMode/Pivot/Snap solo en Edit Mode sobre esta malla; iconos = modo/pivot actual
    const bool enEditUV = (m && (Object*)m == g_editMesh);
    if (BarButtons.size() > 4) {
        BarButtons[2]->visible = enEditUV; BarButtons[3]->visible = enEditUV; BarButtons[4]->visible = enEditUV;
        int mUV = ModoUV();
        BarButtons[2]->icon = (mUV == SelEdge) ? (int)IconType::selEdge :
                              (mUV == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
        BarButtons[3]->icon = (g_transformPivot == PivotCursor3D)   ? (int)IconType::pivotCursor :
                              (g_transformPivot == PivotIndividual) ? (int)IconType::pivotIndividual :
                              (g_transformPivot == PivotActive)     ? (int)IconType::pivotActive :
                                                                      (int)IconType::pivotMedian;
    }

    // contorno del cuadrado UV (0..1): referencia de los limites de la imagen
    {
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
        float c0x,c0y,c1x,c1y,c2x,c2y,c3x,c3y;
        UVtoScreen(0,0, cx,cy,s, c0x,c0y); UVtoScreen(1,0, cx,cy,s, c1x,c1y);
        UVtoScreen(1,1, cx,cy,s, c2x,c2y); UVtoScreen(0,1, cx,cy,s, c3x,c3y);
        float sq[16] = { c0x,c0y, c1x,c1y,  c1x,c1y, c2x,c2y,
                         c2x,c2y, c3x,c3y,  c3x,c3y, c0x,c0y };
        const float* gris = ListaColores[static_cast<int>(ColorID::grisUI)];
        gfx::Color4f(gris[0], gris[1], gris[2], 1.0f);
        gfx::LineWidth(1.0f);
        gfx::VertexPointer2f(0, sq);
        gfx::DrawLines(8);
    }

    if (m) {
        const int part = UVParteActiva(m);
        Material* mat = (part < (int)m->materialsGroup.size())
                            ? m->materialsGroup[part].material : NULL;

        // --- la textura de la parte activa, centrada ---
        if (mat && mat->texture && mat->texture->iID) {
            gfx::Enable(gfx::Texture2D);
            gfx::EnableArray(gfx::TexCoordArray);
            gfx::BindTexture(mat->texture->iID);
            gfx::TexWrap(repeatTexture);
            gfx::TexFilter(mat->filtrado);
            gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
            const float lo = repeatTexture ? -3.0f : 0.0f;
            const float hi = repeatTexture ?  4.0f : 1.0f;
            float aX,aY,bX,bY,cX,cY,dX,dY;
            UVtoScreen(lo,lo, cx,cy,s, aX,aY); UVtoScreen(hi,lo, cx,cy,s, bX,bY);
            UVtoScreen(hi,hi, cx,cy,s, cX,cY); UVtoScreen(lo,hi, cx,cy,s, dX,dY);
            float P[12] = { aX,aY, bX,bY, cX,cY,  aX,aY, cX,cY, dX,dY };
            float T[12] = { lo,lo, hi,lo, hi,hi,  lo,lo, hi,hi, lo,hi };
            gfx::VertexPointer2f(0, P);
            gfx::TexCoordPointer2f(0, T);
            gfx::DrawTrianglesArray(6);
            gfx::Disable(gfx::Texture2D);
            gfx::DisableArray(gfx::TexCoordArray);
        }

        // --- el wireframe de las UV encima: caras LOGICAS (quads/ngons), SIN triangular ---
        // SYNC SELECTION: ON -> dibuja TODO; las caras seleccionadas en 3D van verde (accent) y el
        // resto gris. OFF -> SOLO las caras seleccionadas. En Object Mode (sin seleccion) = todo verde.
        if (m->uv && !m->faces3d.empty()) {
            const int nVerts = m->vertexSize;
            const bool enEdit = ((Object*)m == g_editMesh);
            std::vector<unsigned char> sel3d(m->faces3d.size(), 0);
            if (enEdit) { m->EnsureEdit();
                if (m->edit) for (size_t f = 0; f < m->edit->faceSel.size(); f++)
                    if (m->edit->faceSel[f] && f < m->edit->faceSrc.size()) {
                        int f3 = m->edit->faceSrc[f]; if (f3>=0 && f3<(int)m->faces3d.size()) sel3d[f3]=1;
                    }
            }
            std::vector<float> Lsel, Luns;          // ARISTAS: cara sel en 3D (accent) / no (gris)
            std::vector<float> Psel, Puns;          // PUNTOS del sub-modo (vertex/face)
            std::vector<float> Lwhite, Pwhite;      // SELECCION del editor UV (blanco): aristas / puntos
            const bool haySelUV = enEdit && (int)m->uvSelVert.size() == nVerts;
            const int modoUV = ModoUV();            // vertex/edge/face efectivo (propio o sincronizado)
            for (size_t f = 0; f < m->faces3d.size(); f++) {
                const std::vector<int>& id = m->faces3d[f].idx;
                const int nc = (int)id.size();
                if (nc < 2) continue;
                // en Object Mode todo cuenta como "seleccionado" (no hay sub-seleccion)
                const bool selFace = enEdit ? (f < sel3d.size() && sel3d[f]) : true;
                if (!syncSelection && !selFace) continue; // OFF: solo las seleccionadas
                std::vector<float>& L = selFace ? Lsel : Luns;
                std::vector<float>& P = selFace ? Psel : Puns;
                float fcu = 0, fcv = 0; int fn = 0;     // para el centro de la cara (modo face)
                bool faceAllSel = haySelUV;             // modo face: TODOS los verts UV seleccionados?
                for (int c = 0; c < nc; c++) {            // arista c -> c+1 (cierra el poligono)
                    int ka = id[c], kb = id[(c+1) % nc];
                    if (ka < 0 || ka >= nVerts || kb < 0 || kb >= nVerts) { faceAllSel = false; continue; }
                    float ax,ay,bx,by;
                    UVtoScreen(m->uv[ka*2], m->uv[ka*2+1], cx,cy,s, ax,ay); L.push_back(ax); L.push_back(ay);
                    UVtoScreen(m->uv[kb*2], m->uv[kb*2+1], cx,cy,s, bx,by); L.push_back(bx); L.push_back(by);
                    // sub-modo VERTEX: un punto en cada UV-vert (blanco si esta seleccionado en el editor UV)
                    if (enEdit && modoUV == SelVertex) {
                        P.push_back(ax); P.push_back(ay);
                        if (haySelUV && m->uvSelVert[ka]) { Pwhite.push_back(ax); Pwhite.push_back(ay); }
                    } else if (enEdit && modoUV == SelEdge) {
                        // sub-modo EDGE: arista blanca si sus 2 extremos UV estan seleccionados
                        if (haySelUV && m->uvSelVert[ka] && m->uvSelVert[kb]) {
                            Lwhite.push_back(ax); Lwhite.push_back(ay); Lwhite.push_back(bx); Lwhite.push_back(by);
                        }
                    }
                    if (haySelUV && !m->uvSelVert[ka]) faceAllSel = false;
                    fcu += m->uv[ka*2]; fcv += m->uv[ka*2+1]; fn++;
                }
                // sub-modo FACE: un punto en el centro UV de la cara (blanco si TODA la cara esta seleccionada)
                if (enEdit && modoUV == SelFace && fn > 0) {
                    float vx,vy; UVtoScreen(fcu/fn, fcv/fn, cx,cy,s, vx,vy);
                    P.push_back(vx); P.push_back(vy);
                    if (faceAllSel) { Pwhite.push_back(vx); Pwhite.push_back(vy); }
                }
            }
            const float* gr = ListaColores[static_cast<int>(ColorID::grisUI)];
            const float* ac = ListaColores[static_cast<int>(ColorID::accent)];
            gfx::LineWidth(1.0f);
            if (!Luns.empty()) { gfx::Color4f(gr[0],gr[1],gr[2],1.0f); gfx::VertexPointer2f(0,&Luns[0]); gfx::DrawLines((int)(Luns.size()/2)); }
            if (!Lsel.empty()) { gfx::Color4f(ac[0],ac[1],ac[2],1.0f); gfx::VertexPointer2f(0,&Lsel[0]); gfx::DrawLines((int)(Lsel.size()/2)); }
            // SELECCION del editor UV en BLANCO, encima de todo (aristas mas gruesas)
            if (!Lwhite.empty()) { gfx::LineWidth(2.0f); gfx::Color4f(1,1,1,1); gfx::VertexPointer2f(0,&Lwhite[0]); gfx::DrawLines((int)(Lwhite.size()/2)); gfx::LineWidth(1.0f); }
            // los puntos del sub-modo (vertex/face) ENCIMA de las aristas; los blancos arriba de todo
            if (!Puns.empty() || !Psel.empty() || !Pwhite.empty()) {
                gfx::PointSize((float)GlobalScale * 3.0f);
                if (!Puns.empty()) { gfx::Color4f(gr[0],gr[1],gr[2],1.0f); gfx::VertexPointer2f(0,&Puns[0]); gfx::DrawPoints((int)(Puns.size()/2)); }
                if (!Psel.empty()) { gfx::Color4f(ac[0],ac[1],ac[2],1.0f); gfx::VertexPointer2f(0,&Psel[0]); gfx::DrawPoints((int)(Psel.size()/2)); }
                if (!Pwhite.empty()) { gfx::PointSize((float)GlobalScale * 4.0f); gfx::Color4f(1,1,1,1); gfx::VertexPointer2f(0,&Pwhite[0]); gfx::DrawPoints((int)(Pwhite.size()/2)); }
            }
        }

        // --- OVERLAY del CHROME UV (equirect) en TIEMPO REAL ---
        // El reflejo recalcula chromeExpUV al orbitar (cache del 3D); aca lo dibujamos como wireframe CYAN.
        // Se ve morphear en vivo el mapeo del reflejo. off = 0 CPU extra (no calcula nada de mas). Demo/debug.
        if (mostrarChromeUV && m->chromeExpUV && m->chromeExpCount >= 3) {
            gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
            std::vector<float> CL;
            for (int t = 0; t + 2 < m->chromeExpCount; t += 3) {
                float xx[3], yy[3];
                for (int k = 0; k < 3; k++)
                    UVtoScreen(m->chromeExpUV[(t+k)*2], m->chromeExpUV[(t+k)*2+1], cx,cy,s, xx[k], yy[k]);
                for (int e = 0; e < 3; e++) { int a = e, b = (e+1)%3;
                    CL.push_back(xx[a]); CL.push_back(yy[a]); CL.push_back(xx[b]); CL.push_back(yy[b]); }
            }
            if (!CL.empty()) {
                gfx::LineWidth(1.0f); gfx::Color4f(0.2f, 0.9f, 0.95f, 1.0f); // cyan
                gfx::VertexPointer2f(0, &CL[0]);
                gfx::DrawLines((int)(CL.size()/2));
            }
        }
    }

    // CURSOR 2D (en el espacio UV): cruz roja con halo blanco (estilo cursor 3D). Pivot opcional + snap.
    if (enEditUV) {
        float ccx, ccy; UVtoScreen(uvCursorU, uvCursorV, cx,cy,s, ccx,ccy);
        float r = (float)GlobalScale * 3.0f;
        float cross[8] = { ccx-r,ccy, ccx+r,ccy,  ccx,ccy-r, ccx,ccy+r };
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        gfx::VertexPointer2f(0, cross);
        gfx::LineWidth(3.0f); gfx::Color4f(1,1,1,1); gfx::DrawLines(4);            // halo blanco
        gfx::LineWidth(1.0f); gfx::Color4f(0.9f,0.15f,0.15f,1.0f); gfx::DrawLines(4); // cruz roja
    }

    gfx::Disable(gfx::ScissorTest);
    // RenderBar y DibujarBordes dibujan quads/bordes TEXTURIZADOS (iconos del atlas):
    // hay que dejarles el estado prendido (el wireframe de arriba lo apago) Y re-bindear el
    // ATLAS de iconos (Textures[0]) -> sino usan la textura del mesh que quedo bindeada y la
    // barra/borde salen con la textura del modelo encima (bug reportado por Dante).
    gfx::Enable(gfx::Texture2D);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::EnableArray(gfx::VertexArray);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    RenderBar();
    DibujarBordes(this); // borde del viewport (verde si es el activo)
}

void UVEditor::event_mouse_motion(int mx, int my) {
    // si hay un popup modal (file browser, etc.) el UV editor CEDE el input: no transforma
    // ni panea (sino un transform activo seguia robando el mouse con el browser abierto).
    if (PopUpActive) { lastMx = mx; lastMy = my; return; }
    if (uvXform) {                         // transform en curso: mover la seleccion en vivo
        Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (m) {
            float cx,cy,s; ParamsUV(cx,cy,s);
            float cu = ((float)(mx - x) - cx) / s + 0.5f;
            float cv = ((float)(my - y) - cy) / s + 0.5f;
            AplicarXform(m, cu, cv);
        }
    } else if (middleMouseDown) {          // boton del medio: paneo
        panX += (float)(mx - lastMx);
        panY += (float)(my - lastMy);
        g_redraw = true;
    }
    lastMx = mx; lastMy = my;
}

// parametros del mapeo UV->pantalla (identicos al Render/PickUV): centro + escala.
void UVEditor::ParamsUV(float& cx, float& cy, float& s) const {
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    cx = width * 0.5f + panX;
    cy = top + ch * 0.5f + panY;
    float baseSize = (float)(width < ch ? width : ch) * 0.8f;
    s = baseSize * zoom; if (s < 1.0f) s = 1.0f;
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    CalcAspectoUV(m, m ? UVParteActiva(m) : -1); // mismo aspecto que el Render -> el pick coincide
}

// snapshot de la seleccion + pivot (centroide) + mouse inicial. modo: 1=mover 2=rotar 3=escalar.
void UVEditor::IniciarXform(Mesh* m, int modo) {
    if (!m || !m->uv) return;
    const int nV = m->vertexSize;
    if ((int)m->uvSelVert.size() != nV) return;
    uvXIdx.clear(); uvXOrig.clear();
    double su = 0, sv = 0;
    for (int i = 0; i < nV; i++) if (m->uvSelVert[i]) {
        uvXIdx.push_back(i);
        uvXOrig.push_back(m->uv[i*2]); uvXOrig.push_back(m->uv[i*2+1]);
        su += m->uv[i*2]; sv += m->uv[i*2+1];
    }
    if (uvXIdx.empty()) return; // nada seleccionado -> no arranca
    // PIVOT segun el menu (mismo g_transformPivot que el 3D): 3D Cursor = el cursor 2D;
    // el resto (Median / Individual / Active) = centroide de la seleccion (en 2D no hay activo).
    if (g_transformPivot == PivotCursor3D) { uvXPivotU = uvCursorU; uvXPivotV = uvCursorV; }
    else { uvXPivotU = (float)(su / uvXIdx.size()); uvXPivotV = (float)(sv / uvXIdx.size()); }
    float cx,cy,s; ParamsUV(cx,cy,s);
    uvXStartU = ((float)(lastMx - x) - cx) / s + 0.5f; // mouse actual = arranque (sin salto)
    uvXStartV = ((float)(lastMy - y) - cy) / s + 0.5f;
    uvXform = modo;
    g_redraw = true;
}

// aplica el transform en vivo desde la base ORIGINAL (no acumula): mover/rotar/escalar respecto al pivot.
void UVEditor::AplicarXform(Mesh* m, float curU, float curV) {
    if (!uvXform || uvXIdx.empty() || !m || !m->uv) return;
    if (uvXform == 1) {                    // MOVER
        float dU = curU - uvXStartU, dV = curV - uvXStartV;
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            m->uv[k*2]   = uvXOrig[i*2]   + dU;
            m->uv[k*2+1] = uvXOrig[i*2+1] + dV; }
    } else if (uvXform == 2) {             // ROTAR alrededor del pivot
        float a0 = atan2f(uvXStartV - uvXPivotV, uvXStartU - uvXPivotU);
        float a1 = atan2f(curV - uvXPivotV, curU - uvXPivotU);
        float da = a1 - a0, c = cosf(da), s2 = sinf(da);
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            float ru = uvXOrig[i*2] - uvXPivotU, rv = uvXOrig[i*2+1] - uvXPivotV;
            m->uv[k*2]   = uvXPivotU + ru*c - rv*s2;
            m->uv[k*2+1] = uvXPivotV + ru*s2 + rv*c; }
    } else if (uvXform == 3) {             // ESCALAR desde el pivot
        float d0 = sqrtf((uvXStartU-uvXPivotU)*(uvXStartU-uvXPivotU) + (uvXStartV-uvXPivotV)*(uvXStartV-uvXPivotV));
        float d1 = sqrtf((curU-uvXPivotU)*(curU-uvXPivotU) + (curV-uvXPivotV)*(curV-uvXPivotV));
        float f = (d0 > 1e-5f) ? d1 / d0 : 1.0f;
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            m->uv[k*2]   = uvXPivotU + (uvXOrig[i*2]   - uvXPivotU) * f;
            m->uv[k*2+1] = uvXPivotV + (uvXOrig[i*2+1] - uvXPivotV) * f; }
    }
    g_redraw = true;
}

void UVEditor::ConfirmarXform() {          // deja el cambio aplicado
    uvXform = 0; uvXIdx.clear(); uvXOrig.clear(); g_redraw = true;
}

void UVEditor::CancelarXform(Mesh* m) {    // restaura los uv originales
    if (uvXform && m && m->uv)
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            if (k >= 0 && k < m->vertexSize) { m->uv[k*2] = uvXOrig[i*2]; m->uv[k*2+1] = uvXOrig[i*2+1]; } }
    uvXform = 0; uvXIdx.clear(); uvXOrig.clear(); g_redraw = true;
}

// paneo de la vista UV (compartido PC/Symbian). dx>0 mueve el contenido a la derecha (revela la izquierda).
void UVEditor::Panear(float dx, float dy) { panX += dx; panY += dy; g_redraw = true; }

// TOUCH: 1 dedo. Sobre la barra superior = scroll horizontal; sino = panear la vista UV.
bool UVEditor::event_finger_scroll(int px, int py, int dx, int dy){
    if (BarScrollHorizontal(px, py, dx)) return true;
    Panear((float)dx, (float)dy);
    return true;
}
// TOUCH: 2 dedos = zoom (pinch) + paneo del centroide.
void UVEditor::event_finger_gesture(float zoomDelta, float panDx, float panDy){
    if (zoomDelta > 1.0f)       ZoomCentro(1);
    else if (zoomDelta < -1.0f) ZoomCentro(-1);
    if (panDx != 0.0f || panDy != 0.0f) Panear(panDx, panDy);
}

// zoom CENTRADO en el viewport (sin cursor; para el teclado 0+arriba/abajo de Symbian). Es el zoom de la rueda
// con el "cursor" en el centro: ahi (curX-uvCx) = -panX -> panX queda *= f (el centro de pantalla no se mueve).
void UVEditor::ZoomCentro(int dir) {
    float f = (dir > 0) ? 1.05f : (1.0f / 1.05f); // suave por frame (la rueda usa 1.1 por notch)
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    f = nz / zoom;                 // factor real tras el clamp
    zoom = nz; panX *= f; panY *= f;
    g_redraw = true;
}

#ifndef W3D_SYMBIAN
// teclas del editor UV: flechas = paneo (cualquier modo); G/R/S inician mover/rotar/escalar; ESC/ENTER transform.
void UVEditor::event_key_down(SDL_Event &e) {
    if (PopUpActive) return;                    // un popup modal abierto (file browser) tiene prioridad
    SDL_Keycode k = e.key.keysym.sym;
    // PANEO de la vista con las flechas (en cualquier modo): la flecha revela ese lado
    const float pp = (float)GlobalScale * 16.0f;
    if (k == SDLK_LEFT)  { Panear(+pp, 0); return; }
    if (k == SDLK_RIGHT) { Panear(-pp, 0); return; }
    if (k == SDLK_UP)    { Panear(0, +pp); return; }
    if (k == SDLK_DOWN)  { Panear(0, -pp); return; }
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || (Object*)m != g_editMesh) return; // el resto (G/R/S) solo en Edit Mode sobre esta malla
    if (uvXform) {                          // dentro de un transform: confirmar/cancelar
        if (k == SDLK_ESCAPE) CancelarXform(m);
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) ConfirmarXform();
        return;
    }
    if (k == SDLK_g) IniciarXform(m, 1);    // G = mover  (Symbian 1)
    else if (k == SDLK_r) IniciarXform(m, 2); // R = rotar  (Symbian 2)
    else if (k == SDLK_s) IniciarXform(m, 3); // S = escalar (Symbian 3)
}

void UVEditor::event_mouse_wheel(SDL_Event &e) {
    if (PopUpActive) return;                    // dejar que el popup (file browser) maneje la rueda
    { int mx, my; SDL_GetMouseState(&mx, &my);
      if (BarScrollHorizontal(mx, my, (int)(e.wheel.y * 40))) return; } // sobre la barra -> scroll horizontal
    float f = (e.wheel.y > 0) ? 1.1f : (1.0f / 1.1f);
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    f = nz / zoom;                         // factor real tras el clamp
    // zoom HACIA EL CURSOR: el punto UV bajo el mouse queda fijo
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    float uvCx = width * 0.5f + panX;
    float uvCy = top + ch * 0.5f + panY;
    float curX = (float)(lastMx - x);      // cursor en coords LOCALES del viewport
    float curY = (float)(lastMy - y);
    panX += (curX - uvCx) * (1.0f - f);
    panY += (curY - uvCy) * (1.0f - f);
    zoom = nz;
    g_redraw = true;
}

// al soltar el mouse: liberar el "click sostenido" para que viewPortActive vuelva a seguir
// al mouse (sino el borde verde queda clavado aca y no se pueden mover los splitters).
void UVEditor::mouse_button_up(SDL_Event &e) {
    (void)e;
    ViewPortClickDown = false;
    g_redraw = true;
}
#endif

// click izquierdo en el editor UV = seleccionar el sub-elemento (vertex/edge/face) bajo el cursor.
void UVEditor::button_left() {
#ifdef W3D_SYMBIAN
    return; // Symbian: el pick por OK/lapiz se cablea aparte (llama directo a PickUV)
#else
    if (PopUpActive) return;                    // un popup modal abierto (file browser) tiene prioridad
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || !m->uv || (Object*)m != g_editMesh) return; // solo en Edit Mode sobre esta malla
    if (uvXform) { ConfirmarXform(); return; } // si hay transform en curso, el click lo CONFIRMA
    const Uint8* ks = SDL_GetKeyboardState(NULL);
    bool add = ks && (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]); // shift = sumar/toggle
    PickUV(m, lastMx - x, lastMy - y, add); // lastMx/My son GLOBALES; - origen del viewport = local
    g_redraw = true;
#endif
}

// click derecho: cancela el transform en curso; si no hay, COLOCA el cursor 2D en el mouse.
void UVEditor::button_right() {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (uvXform) { if (m) CancelarXform(m); return; }
    float cx,cy,s; ParamsUV(cx,cy,s);
    uvCursorU = ((float)(lastMx - x) - cx) / s + 0.5f;
    uvCursorV = ((float)(lastMy - y) - cy) / s + 0.5f;
    g_redraw = true;
}

// --- SNAP (menu Snap del UV editor) ---
void UVEditor::SnapCursorToSel() {         // cursor 2D -> centro de la seleccion
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || !m->uv) return;
    const int nV = m->vertexSize;
    if ((int)m->uvSelVert.size() != nV) return;
    double su = 0, sv = 0; int n = 0;
    for (int i = 0; i < nV; i++) if (m->uvSelVert[i]) { su += m->uv[i*2]; sv += m->uv[i*2+1]; n++; }
    if (n > 0) { uvCursorU = (float)(su/n); uvCursorV = (float)(sv/n); g_redraw = true; }
}

void UVEditor::SnapSelToCursor() {         // mueve la seleccion para que su centro caiga en el cursor
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || !m->uv) return;
    const int nV = m->vertexSize;
    if ((int)m->uvSelVert.size() != nV) return;
    double su = 0, sv = 0; int n = 0;
    for (int i = 0; i < nV; i++) if (m->uvSelVert[i]) { su += m->uv[i*2]; sv += m->uv[i*2+1]; n++; }
    if (n == 0) return;
    float dU = uvCursorU - (float)(su/n), dV = uvCursorV - (float)(sv/n);
    for (int i = 0; i < nV; i++) if (m->uvSelVert[i]) { m->uv[i*2] += dU; m->uv[i*2+1] += dV; }
    g_redraw = true;
}

void UVEditor::CursorToCenter() { uvCursorU = 0.5f; uvCursorV = 0.5f; g_redraw = true; }

// pick: encuentra el elemento mas cercano (segun EditSelectMode) y actualiza Mesh::uvSelVert.
void UVEditor::PickUV(Mesh* m, int lx, int ly, bool add) {
    if (!m || !m->uv) return;
    const int nV = m->vertexSize;
    if (nV <= 0) return;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign(nV, 0);

    // mismos parametros de transform que el Render (cx,cy,s)
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    const float cx = width * 0.5f + panX;
    const float cy = top + ch * 0.5f + panY;
    float baseSize = (float)(width < ch ? width : ch) * 0.8f;
    float s = baseSize * zoom; if (s < 1.0f) s = 1.0f;

    // caras visibles (= las que dibuja el Render segun syncSelection)
    const bool enEdit = ((Object*)m == g_editMesh);
    std::vector<unsigned char> sel3d(m->faces3d.size(), 0);
    if (enEdit) { m->EnsureEdit();
        if (m->edit) for (size_t f = 0; f < m->edit->faceSel.size(); f++)
            if (m->edit->faceSel[f] && f < m->edit->faceSrc.size()) {
                int f3 = m->edit->faceSrc[f]; if (f3 >= 0 && f3 < (int)m->faces3d.size()) sel3d[f3] = 1;
            }
    }

    const int modoUV = ModoUV();                       // vertex/edge/face efectivo
    const float clx = (float)lx, cly = (float)ly;
    const float rad = (float)GlobalScale * 5.0f;       // radio de pick (px)
    int hitVert = -1; float bestVD = rad*rad;
    int hitEA = -1, hitEB = -1; float bestED = rad*rad;
    int hitFace = -1;

    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        const int nc = (int)id.size();
        if (nc < 2) continue;
        const bool selFace = enEdit ? (f < sel3d.size() && sel3d[f]) : true;
        if (!syncSelection && !selFace) continue;
        if (modoUV == SelVertex) {
            for (int c = 0; c < nc; c++) { int ka = id[c]; if (ka < 0 || ka >= nV) continue;
                float sx,sy; UVtoScreen(m->uv[ka*2], m->uv[ka*2+1], cx,cy,s, sx,sy);
                float d = (sx-clx)*(sx-clx) + (sy-cly)*(sy-cly);
                if (d < bestVD) { bestVD = d; hitVert = ka; } }
        } else if (modoUV == SelEdge) {
            for (int c = 0; c < nc; c++) { int ka = id[c], kb = id[(c+1)%nc]; if (ka<0||ka>=nV||kb<0||kb>=nV) continue;
                float ax,ay,bx,by; UVtoScreen(m->uv[ka*2],m->uv[ka*2+1],cx,cy,s,ax,ay);
                UVtoScreen(m->uv[kb*2],m->uv[kb*2+1],cx,cy,s,bx,by);
                float d = DistPtSeg2(clx,cly, ax,ay, bx,by);
                if (d < bestED) { bestED = d; hitEA = ka; hitEB = kb; } }
        } else { // SelFace: click DENTRO del poligono (el ultimo que contenga = el de mas "encima")
            std::vector<float> poly; bool ok = true;
            for (int c = 0; c < nc; c++) { int ka = id[c]; if (ka<0||ka>=nV) { ok=false; break; }
                float sx,sy; UVtoScreen(m->uv[ka*2],m->uv[ka*2+1],cx,cy,s,sx,sy); poly.push_back(sx); poly.push_back(sy); }
            if (ok && PointInPoly(clx,cly, &poly[0], nc)) hitFace = (int)f;
        }
    }

    if (!add) m->uvSelVert.assign(nV, 0); // sin shift: reemplaza
    if (modoUV == SelVertex && hitVert >= 0) {
        m->uvSelVert[hitVert] = add ? (m->uvSelVert[hitVert] ? 0 : 1) : 1;
    } else if (modoUV == SelEdge && hitEA >= 0) {
        unsigned char nv = add ? ((m->uvSelVert[hitEA] && m->uvSelVert[hitEB]) ? 0 : 1) : 1;
        m->uvSelVert[hitEA] = nv; m->uvSelVert[hitEB] = nv;
    } else if (modoUV == SelFace && hitFace >= 0) {
        const std::vector<int>& id = m->faces3d[hitFace].idx;
        bool allSel = true;
        for (size_t c = 0; c < id.size(); c++) { int k = id[c]; if (k>=0 && k<nV && !m->uvSelVert[k]) { allSel = false; break; } }
        unsigned char nv = add ? (allSel ? 0 : 1) : 1;
        for (size_t c = 0; c < id.size(); c++) { int k = id[c]; if (k>=0 && k<nV) m->uvSelVert[k] = nv; }
    }
}
