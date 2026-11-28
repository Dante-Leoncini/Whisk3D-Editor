#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "RedoMeshPanel.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "objects/Mesh.h"          // Mesh + Regenerar + MeshType
#include "ViewPorts/LayoutInput.h" // LayoutKey
#include "ViewPorts/ViewPort3D.h"  // Viewport3DActive (rect del viewport que crea)
#include "ViewPorts/LayoutTree.h"  // aviso de destruccion de viewports (vpCreator)
#include "WhiskUI/widgets/PopupMenu.h"          // MenuScreenW / MenuScreenH (tamano ventana)
#include "NumPad.h"                     // teclado numerico (editar el valor por texto/formula)
#include "ViewPorts/TransformUI.h"     // ToolbarUsesTouch: el teclado en pantalla solo con el dedo
#include "edit/MeshEdit.h"              // InsetOptions (el panel "Inset Faces")
#include "ViewPorts/Pick3D.h"           // InsetRedoApply / InsetCurrentOptions

// la malla que el panel esta editando + un espejo float de los vertices (los
// PropFloat trabajan con float*; meshVerts es int). onChange unico para todos.
static Mesh* gRedoMesh = NULL;
static float gRedoVertsF = 8.0f;   // circulo: vertices / esfera: segments
static float gRedoVerts2F = 8.0f;  // esfera: rings
static bool  gRedoInside = false;  // Recalculate Normals: tilde "Inside"
static float gRedoCutsF   = 1.0f;  // Loop Cut: Number of Cuts (espejo float)
static float gRedoFactorF = 0.0f;  // Loop Cut: Factor (-1..1)
static RedoMeshPanel* gRedoPanel = NULL;

// ============================================================================
//  EL PANEL NO PUEDE SOBREVIVIR A SU MALLA
//
//  Este panel es el caso mas crudo de la familia: no solo se guarda la malla (gRedoMesh), sino
//  que sus campos apuntan a MIEMBROS de ella (PropFloat::value = &m->meshSize, &m->meshDepth,
//  PropBool::value = &m->meshSmooth...) y NO son solo de lectura: el arrastre los ESCRIBE
//  (Motion -> dragField->Set(*dragField->value + ...)). Si la malla se borra con el panel
//  abierto, arrastrar escribe en memoria liberada; y con la direccion reciclada por la malla
//  siguiente, escribe el tamano/profundidad de OTRA malla y la regenera, en silencio.
//  El aviso de destruccion de Object (W3dUnlinkRegister, Objects.h) cierra el panel: es
//  lo unico correcto, porque los punteros de los campos se arman en el constructor y no hay
//  forma de "revalidarlos" uno por uno.
// ============================================================================
static void RedoMeshUnlink(Object* deleted){
    if (!deleted || (Object*)gRedoMesh != deleted) return;
    gRedoMesh = NULL;                                  // el onChange comun ya no toca nada
    if (gRedoPanel){                                   // ...y el panel se va con ella
        if (PopUpActive == gRedoPanel) PopUpActive = NULL;
        delete gRedoPanel; gRedoPanel = NULL;          // su dtor solo libera widgets: no lee la malla
    }
}
struct RedoMeshHook { RedoMeshHook(){ W3dUnlinkRegister(RedoMeshUnlink); } };
static RedoMeshHook g_redoMeshHook;
// idem el VIEWPORT 3D que creo la forma: de el se lee el rect para ubicar el panel (vpCreator) y
// si muere (cambiar el tipo del viewport, abrir un proyecto: LayoutTree.h) el panel se cierra,
// igual que con su malla. g_redoVpCreator es el mismo puntero que el vpCreator del panel abierto.
static Viewport3D* g_redoVpCreator = NULL;
static void RedoMeshForgetViewport(ViewportBase* vp){
    if (!gRedoPanel || !g_redoVpCreator || (ViewportBase*)g_redoVpCreator != vp) return;
    g_redoVpCreator = NULL;
    RedoMeshPanelClose();   // el mismo cierre que el undo: panel + su malla
}
struct RedoMeshHookVp { RedoMeshHookVp(){ ViewportForgetRegister(RedoMeshForgetViewport); } };
static RedoMeshHookVp g_redoMeshHookVp;

static void RedoOnChange(){
    if (!gRedoMesh) return;
    int v = (int)(gRedoVertsF + 0.5f);   if (v < 3) v = 3;
    int v2 = (int)(gRedoVerts2F + 0.5f); if (v2 < 2) v2 = 2;
    gRedoMesh->meshVerts = v;
    gRedoMesh->meshVerts2 = v2;
    gRedoMesh->Regenerate(); // reposiciona (tamano) o recrea (vertices) en vivo
}

// Recalculate Normals: al togglear "Inside" re-corre el recalculo. Es determinista
// (orienta segun el centro), asi que togglear da vuelta limpio las caras objetivo.
static void RedoNormalsOnChange(){
    if (!gRedoMesh) return;
    gRedoMesh->RecalculateOrientationEdit(gRedoInside);
}

// Loop Cut: cambiar cortes o factor -> re-corta desde el snapshot (en el modulo de loop cut)
static bool  gRedoLCCorrectUV = true;   // Correct UVs (por defecto SI)
static float gRedoLCEdgeF = 1.0f;       // arista de entrada elegida, 1..N (N = aristas del primer quad)
static void RedoLoopCutOnChange(){
    int cuts = (int)(gRedoCutsF + 0.5f); if (cuts < 1) cuts = 1;
    LoopCutRedoApply(cuts, gRedoFactorF, gRedoLCCorrectUV, (int)(gRedoLCEdgeF + 0.5f) - 1);
}

// BEVEL: las opciones de "elegir uno" van como numeros (PropFloat con nombres): estos los reflejan y vuelven
static float gBvF[10];   // 0 afecta, 1 tipo de ancho, 2 segmentos, 3 material, 4 inglete afuera, 5 adentro, 6 interseccion,
                         // 7 fuerza de cara, 8 tipo de perfil
static void BevelPanelOnChange() {
    BevelOptions& o = BevelCurrentOptions();
    o.affects = (int)(gBvF[0] + 0.5f); o.widthType = (int)(gBvF[1] + 0.5f);
    o.segments = (int)(gBvF[2] + 0.5f); if (o.segments < 1) o.segments = 1;
    o.material = (int)floorf(gBvF[3] + 0.5f);
    o.miterOutside = (int)(gBvF[4] + 0.5f); o.miterInside = (int)(gBvF[5] + 0.5f);
    o.intersection = (int)(gBvF[6] + 0.5f); o.forceFace = (int)(gBvF[7] + 0.5f); o.profileType = (int)(gBvF[8] + 0.5f);
    BevelRedoApply();
}

RedoMeshPanel::RedoMeshPanel(Mesh* m, int mode)
    : PopUpBase("Add"), group(NULL), dragField(NULL), dragMoved(false), lastDragMx(0) {
    gRedoMesh = m;

    if (mode == 4) { // Bevel: lo mismo que el panel de Blender. Cada cambio re-bisela desde la malla de antes
        BevelOptions& o = BevelCurrentOptions();
        gBvF[0] = (float)o.affects; gBvF[1] = (float)o.widthType; gBvF[2] = (float)o.segments; gBvF[3] = (float)o.material;
        gBvF[4] = (float)o.miterOutside; gBvF[5] = (float)o.miterInside; gBvF[6] = (float)o.intersection;
        gBvF[7] = (float)o.forceFace; gBvF[8] = (float)o.profileType;
        group = new GroupPropertie(T("Bevel"));
        group->valueWidth = 0.5f;
        struct List { static PropFloat* New(const char* name, float* v, const char* const* ops, int n) {
            PropFloat* p = new PropFloat(T(name)); p->value = v; p->integer = true; p->arrows = true; p->centered = true;
            p->options = ops; p->nOptions = n; p->fineStep = 1.0f; p->thickStep = 1.0f; p->dragStep = 0.05f;
            p->SetRange(0.0f, (float)(n - 1)); p->onChange = BevelPanelOnChange; return p; } };
        static const char* const kAffects[2] = { "Vertices", "Edges" };
        static const char* const kType[5] = { "Offset", "Width", "Depth", "Percent", "Absolute" };
        static const char* const kInter[2] = { "Grid Fill", "Cutoff" };
        static const char* const kForce[4] = { "None", "New", "Affected", "All" };
        group->properties.push_back(List::New("Affect", &gBvF[0], kAffects, 2));
        group->properties.push_back(List::New("Width Type", &gBvF[1], kType, 5));
        PropFloat* width = new PropFloat(T("Width"));
        width->value = &o.width; width->fineStep = 0.01f; width->thickStep = 0.1f; width->dragStep = 0.005f;
        width->SetRange(0.0f, 100000.0f); width->onChange = BevelRedoApply; width->centered = true;
        group->properties.push_back(width);
        PropFloat* segs = new PropFloat(T("Segments"));
        segs->value = &gBvF[2]; segs->integer = true; segs->arrows = true; segs->centered = true;
        segs->fineStep = 1.0f; segs->thickStep = 1.0f; segs->dragStep = 0.1f; segs->SetRange(1.0f, 100.0f); segs->onChange = BevelPanelOnChange;
        group->properties.push_back(segs);
        PropFloat* profile = new PropFloat(T("Profile Shape"));
        profile->value = &o.profile; profile->fineStep = 0.01f; profile->thickStep = 0.1f; profile->dragStep = 0.005f;
        profile->SetRange(0.0f, 1.0f); profile->onChange = BevelRedoApply; profile->centered = true;
        group->properties.push_back(profile);
        PropFloat* mat = new PropFloat(T("Material Index"));
        mat->value = &gBvF[3]; mat->integer = false; mat->centered = true;
        mat->fineStep = 1.0f; mat->thickStep = 1.0f; mat->dragStep = 0.1f; mat->SetRange(-1.0f, 1000.0f); mat->onChange = BevelPanelOnChange;
        group->properties.push_back(mat);
        struct Check { const char* name; bool* value; };
        const Check checks[5] = { { "Harden Normals", &o.harden }, { "Clamp Overlap", &o.limitOverlap },
                                  { "Loop Slide", &o.slide }, { "Mark Seams", &o.markSeams }, { "Mark Sharp", &o.markSharp } };
        for (int i = 0; i < 5; i++) {
            PropBool* b = new PropBool(T(checks[i].name)); b->value = checks[i].value; b->onChange = BevelRedoApply;
            group->properties.push_back(b);
        }
        group->properties.push_back(List::New("Intersection Type", &gBvF[6], kInter, 2));
        group->properties.push_back(List::New("Face Strength", &gBvF[7], kForce, 4));
        group->selectIndex = 0;
        vpCreator = Viewport3DActive; g_redoVpCreator = vpCreator;
        ResizeGroup();
        Relocate();
        return;
    }

    if (mode == 3) { // Inset Faces: lo mismo que el panel de Blender. Cada cambio re-inseta desde la malla de antes
        InsetOptions& o = InsetCurrentOptions();
        group = new GroupPropertie(T("Inset Faces"));
        group->valueWidth = 0.5f;
        struct Check { const char* name; bool* value; };
        const Check up[4] = { { "Boundary", &o.border }, { "Offset Even", &o.even },
                                  { "Offset Relative", &o.relative }, { "Edge Rail", &o.rail } };
        for (int i = 0; i < 4; i++) {
            PropBool* b = new PropBool(T(up[i].name)); b->value = up[i].value; b->onChange = InsetRedoApply;
            group->properties.push_back(b);
        }
        PropFloat* thickness = new PropFloat(T("Thickness"));
        thickness->value = &o.thickness; thickness->fineStep = 0.01f; thickness->thickStep = 0.1f; thickness->dragStep = 0.005f;
        thickness->SetRange(0.0f, 1000.0f); thickness->onChange = InsetRedoApply; thickness->centered = true;
        group->properties.push_back(thickness);
        PropFloat* prof = new PropFloat(T("Depth"));
        prof->value = &o.depth; prof->fineStep = 0.01f; prof->thickStep = 0.1f; prof->dragStep = 0.005f;
        prof->SetRange(-1000.0f, 1000.0f); prof->onChange = InsetRedoApply; prof->centered = true;
        group->properties.push_back(prof);
        const Check down[4] = { { "Outset", &o.outside }, { "Select Outer", &o.selOutside },
                                 { "Individual", &o.individual }, { "Interpolate", &o.interpolate } };
        for (int i = 0; i < 4; i++) {
            PropBool* b = new PropBool(T(down[i].name)); b->value = down[i].value; b->onChange = InsetRedoApply;
            group->properties.push_back(b);
        }
        group->selectIndex = 0;
        vpCreator = Viewport3DActive; g_redoVpCreator = vpCreator;
        ResizeGroup();
        Relocate();
        return;
    }

    if (mode == 1) { // Recalculate Normals: una tarjeta con la tilde "Inside"
        group = new GroupPropertie(T("Recalculate Normals"));
        group->valueWidth = 0.5f;
        gRedoInside = false;
        PropBool* inside = new PropBool(T("Inside"));
        inside->value = &gRedoInside; inside->onChange = RedoNormalsOnChange;
        group->properties.push_back(inside);
        group->selectIndex = 0;
        vpCreator = Viewport3DActive; g_redoVpCreator = vpCreator;
        ResizeGroup();
        Relocate();
        return;
    }

    if (mode == 2) { // Loop Cut and Slide: Number of Cuts + Factor
        group = new GroupPropertie(T("Loop Cut and Slide"));
        group->valueWidth = 0.5f;
        gRedoCutsF   = (float)LoopCutGetCuts();
        gRedoFactorF = LoopCutGetFactor();
        PropFloat* cuts = new PropFloat(T("Number of Cuts"));
        cuts->value = &gRedoCutsF;
        cuts->fineStep = 1.0f; cuts->thickStep = 1.0f; cuts->dragStep = 0.1f;
        cuts->SetRange(1.0f, 32.0f); cuts->onChange = RedoLoopCutOnChange;
        cuts->integer = true; cuts->centered = true; cuts->arrows = true;
        group->properties.push_back(cuts);
        PropFloat* fac = new PropFloat(T("Factor"));
        fac->value = &gRedoFactorF;
        fac->fineStep = 0.05f; fac->thickStep = 0.05f; fac->dragStep = 0.01f;
        fac->SetRange(-1.0f, 1.0f); fac->onChange = RedoLoopCutOnChange;
        fac->centered = true;
        group->properties.push_back(fac);
        // Correct UVs: los corners nuevos interpolan uv/color a lo largo de la arista (tildado por defecto)
        gRedoLCCorrectUV = LoopCutGetCorrectUV();
        PropBool* cuv = new PropBool(T("Correct UVs"));
        cuv->value = &gRedoLCCorrectUV; cuv->onChange = RedoLoopCutOnChange;
        group->properties.push_back(cuv);
        // Edge: por cual de las aristas del primer quad entra el corte (cambia la direccion del loop)
        if (LoopCutGetCandN() > 1) {
            gRedoLCEdgeF = (float)(LoopCutGetCandSel() + 1);
            PropFloat* edge = new PropFloat(T("Edge"));
            edge->value = &gRedoLCEdgeF;
            edge->fineStep = 1.0f; edge->thickStep = 1.0f; edge->dragStep = 0.1f;
            edge->SetRange(1.0f, (float)LoopCutGetCandN()); edge->onChange = RedoLoopCutOnChange;
            edge->integer = true; edge->centered = true; edge->arrows = true;
            group->properties.push_back(edge);
        }
        group->selectIndex = 0;
        vpCreator = Viewport3DActive; g_redoVpCreator = vpCreator;
        ResizeGroup();
        Relocate();
        return;
    }

    gRedoVertsF = (float)m->meshVerts;
    gRedoVerts2F = (float)m->meshVerts2;

    const char* title = (m->meshType == (int)MeshType::cube)     ? "Add Cube"
                       : (m->meshType == (int)MeshType::plane)    ? "Add Plane"
                       : (m->meshType == (int)MeshType::UVsphere) ? "Add UV Sphere"
                       : (m->meshType == (int)MeshType::cone)     ? "Add Cone"
                       : (m->meshType == (int)MeshType::cylinder) ? "Add Cylinder"
                                                                  : "Add Circle";
    group = new GroupPropertie(T(title));
    group->valueWidth = 0.5f;

    if (m->meshType == (int)MeshType::circle){
        PropFloat* radius = new PropFloat(T("Radius"));
        radius->value = &m->meshSize;
        radius->fineStep = 0.05f; radius->thickStep = 0.05f; // flechas: +-0.05 (fino)
        radius->dragStep = 0.02f;
        radius->SetRange(0.01f, 1000.0f); radius->onChange = RedoOnChange;
        radius->centered = true; // valor centrado (float: misma logica, se ve bien)
        group->properties.push_back(radius);

        PropFloat* verts = new PropFloat(T("Vertices"));
        verts->value = &gRedoVertsF;
        verts->fineStep = 1.0f; verts->thickStep = 1.0f; // flechas/teclas: +-1
        verts->dragStep = 0.1f; // LENTO: ~10px por vertice (que no explote)
        verts->SetRange(3.0f, 256.0f); verts->onChange = RedoOnChange;
        verts->integer = true; verts->centered = true; verts->arrows = true;
        group->properties.push_back(verts);
    } else if (m->meshType == (int)MeshType::UVsphere){
        PropFloat* segs = new PropFloat(T("Segments"));
        segs->value = &gRedoVertsF; // segments (longitud)
        segs->fineStep = 1.0f; segs->thickStep = 1.0f;
        segs->dragStep = 0.1f; // LENTO (que no explote)
        segs->SetRange(3.0f, 64.0f); segs->onChange = RedoOnChange; // 64 max (flat no pasa el limite de indices)
        segs->integer = true; segs->centered = true; segs->arrows = true;
        group->properties.push_back(segs);

        PropFloat* rings = new PropFloat(T("Rings"));
        rings->value = &gRedoVerts2F; // rings (latitud)
        rings->fineStep = 1.0f; rings->thickStep = 1.0f;
        rings->dragStep = 0.1f;
        rings->SetRange(2.0f, 64.0f); rings->onChange = RedoOnChange;
        rings->integer = true; rings->centered = true; rings->arrows = true;
        group->properties.push_back(rings);

        PropFloat* radius = new PropFloat(T("Radius"));
        radius->value = &m->meshSize;
        radius->fineStep = 0.05f; radius->thickStep = 0.05f;
        radius->dragStep = 0.02f;
        radius->SetRange(0.01f, 1000.0f); radius->onChange = RedoOnChange;
        radius->centered = true;
        group->properties.push_back(radius);

        PropBool* smooth = new PropBool(T("Shade Smooth"));
        smooth->value = &m->meshSmooth; smooth->onChange = RedoOnChange;
        group->properties.push_back(smooth);
    } else if (m->meshType == (int)MeshType::cone){
        PropFloat* r1 = new PropFloat(T("Radius 1")); // base
        r1->value = &m->meshSize;
        r1->fineStep = 0.05f; r1->thickStep = 0.05f; r1->dragStep = 0.02f;
        r1->SetRange(0.0f, 1000.0f); r1->onChange = RedoOnChange;
        r1->centered = true;
        group->properties.push_back(r1);

        PropFloat* r2 = new PropFloat(T("Radius 2")); // punta (0 = puntiagudo)
        r2->value = &m->meshSize2;
        r2->fineStep = 0.05f; r2->thickStep = 0.05f; r2->dragStep = 0.02f;
        r2->SetRange(0.0f, 1000.0f); r2->onChange = RedoOnChange;
        r2->centered = true;
        group->properties.push_back(r2);

        PropFloat* depth = new PropFloat(T("Depth")); // altura
        depth->value = &m->meshDepth;
        depth->fineStep = 0.05f; depth->thickStep = 0.05f; depth->dragStep = 0.02f;
        depth->SetRange(0.01f, 1000.0f); depth->onChange = RedoOnChange;
        depth->centered = true;
        group->properties.push_back(depth);

        PropFloat* verts = new PropFloat(T("Vertices"));
        verts->value = &gRedoVertsF;
        verts->fineStep = 1.0f; verts->thickStep = 1.0f; verts->dragStep = 0.1f;
        verts->SetRange(3.0f, 256.0f); verts->onChange = RedoOnChange;
        verts->integer = true; verts->centered = true; verts->arrows = true;
        group->properties.push_back(verts);

        PropBool* smooth = new PropBool(T("Shade Smooth"));
        smooth->value = &m->meshSmooth;
        smooth->onChange = RedoOnChange; // regenera con/sin normales compartidas
        group->properties.push_back(smooth);
    } else if (m->meshType == (int)MeshType::cylinder){
        PropFloat* radius = new PropFloat(T("Radius")); // un solo radio
        radius->value = &m->meshSize;
        radius->fineStep = 0.05f; radius->thickStep = 0.05f; radius->dragStep = 0.02f;
        radius->SetRange(0.01f, 1000.0f); radius->onChange = RedoOnChange;
        radius->centered = true;
        group->properties.push_back(radius);

        PropFloat* depth = new PropFloat(T("Depth"));
        depth->value = &m->meshDepth;
        depth->fineStep = 0.05f; depth->thickStep = 0.05f; depth->dragStep = 0.02f;
        depth->SetRange(0.01f, 1000.0f); depth->onChange = RedoOnChange;
        depth->centered = true;
        group->properties.push_back(depth);

        PropFloat* verts = new PropFloat(T("Vertices"));
        verts->value = &gRedoVertsF;
        verts->fineStep = 1.0f; verts->thickStep = 1.0f; verts->dragStep = 0.1f;
        verts->SetRange(3.0f, 256.0f); verts->onChange = RedoOnChange;
        verts->integer = true; verts->centered = true; verts->arrows = true;
        group->properties.push_back(verts);

        PropBool* smooth = new PropBool(T("Shade Smooth"));
        smooth->value = &m->meshSmooth; smooth->onChange = RedoOnChange;
        group->properties.push_back(smooth);
    } else if (m->meshType == (int)MeshType::cube){
        PropFloat* size = new PropFloat(T("Size"));
        size->value = &m->meshSize;
        size->fineStep = 0.05f; size->thickStep = 0.05f; size->dragStep = 0.02f;
        size->SetRange(0.01f, 1000.0f); size->onChange = RedoOnChange;
        size->centered = true;
        group->properties.push_back(size);

        PropBool* smooth = new PropBool(T("Shade Smooth"));
        smooth->value = &m->meshSmooth; smooth->onChange = RedoOnChange;
        group->properties.push_back(smooth);
    } else {
        PropFloat* size = new PropFloat(T("Size"));
        size->value = &m->meshSize;
        size->fineStep = 0.05f; size->thickStep = 0.05f; // flechas: +-0.05 (fino)
        size->dragStep = 0.02f;
        size->SetRange(0.01f, 1000.0f); size->onChange = RedoOnChange;
        size->centered = true;
        group->properties.push_back(size);
    }

    group->selectIndex = 0; // abrir con el primer campo ya seleccionado

    // recordar el viewport3D que creo la forma; sus bounds se leen en cada
    // Render (en el constructor todavia valen 0, antes del primer layout)
    vpCreator = Viewport3DActive; g_redoVpCreator = vpCreator;
    ResizeGroup();
    Relocate();
}

void RedoMeshPanel::Relocate(){
    int vx, vy, vh;
    if (vpCreator){ vx = vpCreator->x; vy = vpCreator->y; vh = vpCreator->height; }
    else { vx = 0; vy = 0; vh = MenuScreenH; }
    int margin = gapGS * 2;
    x = vx + margin;
    y = vy + vh - popUpWindow->height - margin; // pegado al borde inferior-IZQ
    // NUNCA salirse de la pantalla (en vertical el viewport3D es angosto y el panel sobresalia a la
    // derecha / abajo). Se reencuadra dentro de [margen, Pantalla-margen].
    if (x + popUpWindow->width  + margin > MenuScreenW) x = MenuScreenW - popUpWindow->width  - margin;
    if (y + popUpWindow->height + margin > MenuScreenH) y = MenuScreenH - popUpWindow->height - margin;
    if (x < margin) x = margin;
    if (y < margin) y = margin;
}

RedoMeshPanel::~RedoMeshPanel(){
    delete group;
}

void RedoMeshPanel::ResizeGroup(){
    int w = 200 * GlobalScale;
    int maxW = MenuScreenW - gapGS * 4; // que quepa en pantalla (en vertical el ancho es chico)
    if (w > maxW) w = maxW;             // al achicarse, la tarjeta trunca los labels sola (RenderBitmapText)
    group->Resize(w, 0);
    popUpWindow->Resize(w, group->height);
}

void RedoMeshPanel::Render(){
    if (!group) return;
    ResizeGroup();
    Relocate(); // por si el viewport cambio de tamano
    // cada popup arma su propio ortho/viewport/scissor local (patron FileBrowser).
    // group->Render() dibuja en coords locales: (0,0) = esquina sup-izq del panel.
    initView();
    // ARRASTRANDO un valor: la tarjeta translucida (se ve detras el cambio) y la fila que se edita opaca, al final
    const int rowDrag = (dragField && dragMoved) ? group->IndexOf(dragField) : -1;
    if (rowDrag >= 0) w3dEngine::g_alphaColor = kAlphaCardDrag;
    w3dEngine::PushMatrix();
    group->Render();
    w3dEngine::PopMatrix();
    // borde VERDE = panel activo (mismo accent que el viewport activo)
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    group->card->RenderBorder(false);
    w3dEngine::g_alphaColor = 1.0f;
    if (rowDrag >= 0) group->RenderRow(rowDrag);
    endView();
}

bool RedoMeshPanel::Click(int mx, int my){
    if (!Contains(mx, my)) return false; // afuera -> el caller cierra el panel
    int myL = my - y; // a coordenadas locales del panel
    int lx  = mx - x;
    // el TITULO pliega / despliega la tarjeta
    if (myL < borderGS + RowHeightGS + gapGS) { group->open = !group->open; g_redraw = true; return true; }
    if (!group->open) return true;
    // rect de la columna de valores (igual que el render: titulo + box)
    int boxStart = group->colLabel + 2 * borderGS;
    int boxW = group->propertiBox->width - bordersGS;
    int aw = RowHeightGS; // ancho de cada flecha < >
    // mismo recorrido que ClickIn: titulo + cada fila mide lo que da su Resize
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++){
        int h = group->properties[j]->Resize(group->width);
        if (group->properties[j]->Selectable() && myL >= yRow && myL < yRow + h){
            group->selectIndex = (int)j;
            if (group->properties[j]->GetType() == PropertyType::Float){
                PropFloat* pf = static_cast<PropFloat*>(group->properties[j]);
                if (pf->arrows){
                    // click en la flecha izquierda/derecha: -1 / +1 (sin arrastre)
                    if (lx >= boxStart && lx < boxStart + aw){ pf->button_left();  return true; }
                    if (lx >= boxStart + boxW - aw && lx < boxStart + boxW){ pf->button_right(); return true; }
                }
                dragField = pf; dragMoved = false; lastDragMx = mx; // resto del box: ARRASTRAR (mueve) o TAP (abre teclado al soltar)
            } else if (group->properties[j]->GetType() == PropertyType::Bool){
                group->properties[j]->EditPropertie(); // togglea + onChange (Regenerar)
            }
            return true;
        }
        yRow += h;
    }
    return true; // dentro del panel pero no en un campo: consumir (no cerrar)
}

bool RedoMeshPanel::Motion(int mx, int my){
    // el popup es modal: el viewport NO warpea el mouse mientras esta abierto,
    // asi que el delta crudo (mx - lastDragMx) es seguro.
    if (dragField && dragField->value){
        int dmx = mx - lastDragMx;
        lastDragMx = mx;
        if (dmx != 0) dragMoved = true; // hubo arrastre real -> al soltar NO abrir el teclado
        dragField->Set(*dragField->value + dmx * dragField->dragStep);
    }
    return true;
}

bool RedoMeshPanel::Key(int key){
    if (!group || group->properties.empty()) return true;
    // PLEGADO: OK / derecha lo despliega, C lo cierra; lo demas sigue de largo (se sigue trabajando)
    if (!group->open) {
        if (key == LayoutKey::Enter || key == LayoutKey::Right) { group->open = true; g_redraw = true; return true; }
        if (key == LayoutKey::Cancel) { Close(); return true; }
        return false;
    }
    int n = (int)group->properties.size();
    switch (key){
        // arriba/abajo: navegar entre campos con WRAP (sin los estados -1/-2 de
        // NextSelect/PrevSelect, que son para navegar entre VARIOS grupos)
        case LayoutKey::Up:
        case LayoutKey::Down: {
            int dir = (key == LayoutKey::Down) ? +1 : -1;
            int s = group->selectIndex;
            if (s < 0) s = (dir > 0) ? -1 : 0; // arrancar antes/en el 0
            for (int k = 0; k < n; k++){
                s += dir;
                if (s < 0)  s = n - 1;
                if (s >= n) s = 0;
                if (group->properties[s]->Selectable() &&
                    group->properties[s]->GetType() != PropertyType::Gap) break;
            }
            group->selectIndex = s;
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            if (group->selectIndex < 0) group->selectIndex = 0;
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n)
                                 ? group->properties[group->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::Bool) sel->EditPropertie(); // togglea
            else if (key == LayoutKey::Left) group->button_left();  // -paso
            else group->button_right();                             // +paso
            break;
        }
        case LayoutKey::Cancel: Close(); break; // Escape / soft-izq: cerrar
        case LayoutKey::Enter:  Close(); break; // Enter / OK: confirmar y cerrar
    }
    return true;
}

void RedoMeshPanel::Release(){
    // TAP sobre el value box (sin arrastrar) -> la edicion del valor por texto/formula. Con el MOUSE alcanza el campo (el
    // teclado fisico); con el DEDO ademas el TECLADO NUMERICO en pantalla, que restaura ESTE panel al cerrar (prevPopup)
    // y su onChange re-corta en vivo. (Antes lo abria siempre: en la PC salia el teclado tactil con un click de mouse)
    if (dragField && !dragMoved && dragField->value){
        dragField->StartEditText();
        if (ToolbarUsesTouch()) KeyboardNumberOpen();
    }
    dragField = NULL;
}

// el panel sigue vivo? (para el test 'uipunteros': despues de borrar SU malla tiene que dar
// false, sino quedan colgados gRedoMesh y los PropFloat::value que apuntan adentro de ella)
// undo/redo: el panel trabaja sobre un snapshot PRE-operacion que ya no corresponde a la malla -> se cierra
// LOS PANELES DE "AJUSTAR LA ULTIMA OPERACION" NACEN PLEGADOS: solo la flecha y el titulo de la operacion. Lo normal
// es que ya haya quedado bien y se quiera seguir trabajando (en el N95 un panel grande desplegado de golpe interrumpia
// todo); se despliega con un click en el titulo u OK. Plegado no se queda con las teclas ni con el mouse de afuera
void RedoMeshPanel::Collapse() { if (group) { group->open = false; group->arrow = true; } }
static void RedoCollapse(RedoMeshPanel* p) { if (p) p->Collapse(); }
void RedoMeshPanelClose(){
    if (!gRedoPanel) return;
    if (PopUpActive == gRedoPanel) PopUpActive = NULL;
    delete gRedoPanel; gRedoPanel = NULL;
    gRedoMesh = NULL;
}
bool RedoMeshPanelActive(){ return gRedoPanel != NULL || gRedoMesh != NULL; }

void OpenRedoMeshPanel(Mesh* m){
    if (!m) return;
    if (gRedoPanel){ delete gRedoPanel; gRedoPanel = NULL; } // reemplaza el anterior
    gRedoPanel = new RedoMeshPanel(m);
    RedoCollapse(gRedoPanel);
    PopUpActive = gRedoPanel;
}

void OpenRedoNormalsPanel(Mesh* m){
    if (!m) return;
    if (gRedoPanel){ delete gRedoPanel; gRedoPanel = NULL; }
    gRedoPanel = new RedoMeshPanel(m, 1); // modo 1 = Recalculate Normals
    RedoCollapse(gRedoPanel);
    PopUpActive = gRedoPanel;
}

void OpenRedoBevelPanel(Mesh* m){
    if (!m) return;
    if (gRedoPanel){ delete gRedoPanel; gRedoPanel = NULL; }
    gRedoPanel = new RedoMeshPanel(m, 4); // modo 4 = Bevel
    RedoCollapse(gRedoPanel);
    PopUpActive = gRedoPanel;
}

void OpenRedoInsetPanel(Mesh* m){
    if (!m) return;
    if (gRedoPanel){ delete gRedoPanel; gRedoPanel = NULL; }
    gRedoPanel = new RedoMeshPanel(m, 3); // modo 3 = Inset Faces
    RedoCollapse(gRedoPanel);
    PopUpActive = gRedoPanel;
}

void OpenRedoLoopCutPanel(Mesh* m){
    if (!m) return;
    if (gRedoPanel){ delete gRedoPanel; gRedoPanel = NULL; }
    gRedoPanel = new RedoMeshPanel(m, 2); // modo 2 = Loop Cut and Slide
    RedoCollapse(gRedoPanel);
    PopUpActive = gRedoPanel;
}
