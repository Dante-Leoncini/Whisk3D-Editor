// ============================================================================
//  TransformRedoPanel.cpp - "Ajustar la ultima operacion" (ver TransformRedoPanel.h). C++03.
//  RE-HACER: el paso de undo de la operacion se deshace y la operacion se vuelve a hacer con los valores del panel
//  (los mismos arranques que G/R/S + los aplicadores EXACTOS: mover = delta de mundo, rotar = grados, escalar =
//  factor). Asi la edicion proporcional, el pivote y el undo son los de siempre, y queda UN solo paso.
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "TransformRedoPanel.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey, EditXform*
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "NumPad.h"
#include "objects/ObjectMode.h"             // BeginTranslate / BeginRotate / BeginScale / SetNumericTransform
#include "objects/Mesh.h"
#include "edit/Proportional.h"
#include "Undo.h"
#include "app/variables.h"
#include <math.h>
#include <stdio.h>

extern bool g_redraw;

bool g_transformRedoAuto = true;
static TransformRedoPanel* gTR = NULL;
static Viewport3D* gTRvp = NULL;
static bool gTRReapplying = false;

// la operacion: que se hizo y sobre que
struct OpTR {
    bool valid, edit, free;
    int type, axis, orient;
    Vector3 world;       // mover: el delta de MUNDO (engine xyz)
    const void* undo;    // su paso de undo (NULL = no dejo paso: el estado es el de antes)
    const void* prev;    // el paso que habia antes de la operacion
};
static OpTR gOp;
static float gTRv[3] = { 0, 0, 0 };   // mover: X/Y/Z en la orientacion; escalar: factores X/Y/Z
static float gTRang = 0.0f;           // rotar: grados
static bool  gTRpropOn = false, gTRpropWith = false;
static float gTRpropRadius = 1.0f;

bool TransformRedoReapplying() { return gTRReapplying; }
bool TransformRedoOpen(const Viewport3D* vp) { return gTR && PopUpActive == gTR && (!vp || vp == gTRvp); }
int  TransformRedoType() { return (gTR && PopUpActive == gTR) ? gOp.type : -1; }
void TransformRedoClose() { if (gTR && PopUpActive == gTR) gTR->Close(); }
static void HookUndo() { if (!gTRReapplying) TransformRedoClose(); }   // Ctrl+Z / Ctrl+Y: el panel ya no vale

// ---------------------------------------------------------------------------
//  ejes (UI: X, Y = Z interno, Z = Y interno) en la orientacion de la operacion
// ---------------------------------------------------------------------------
static Object* GetTarget() { return gOp.edit ? (Object*)g_editMesh : ActiveObj; }
static void AxesOrient(int orient, Vector3 ej[3]) {
    ej[0] = Vector3(1, 0, 0); ej[1] = Vector3(0, 0, 1); ej[2] = Vector3(0, 1, 0);
    Object* o = GetTarget();
    if (orient != LocalOrient || !o) return;
    Matrix4 W; o->GetWorldMatrix(W);
    const int col[3] = { 0, 2, 1 };
    for (int k = 0; k < 3; k++) {
        Vector3 e(W.m[col[k]*4], W.m[col[k]*4+1], W.m[col[k]*4+2]);
        const float l = e.Length(); if (l > 1e-6f) ej[k] = e * (1.0f / l);
    }
}
static void ValuesFromWorld(const Vector3& D, int orient, float v[3]) {
    Vector3 ej[3]; AxesOrient(orient, ej);
    for (int k = 0; k < 3; k++) v[k] = D.Dot(ej[k]);
}
static Vector3 WorldFromValues(const float v[3], int orient) {
    Vector3 ej[3]; AxesOrient(orient, ej);
    return ej[0] * v[0] + ej[1] * v[1] + ej[2] * v[2];
}
static int OrientPanel(int o) { return (o == LocalOrient) ? LocalOrient : GlobalOrient; }

// ---------------------------------------------------------------------------
//  capturar (Viewport3D::Aceptar)
// ---------------------------------------------------------------------------
void TransformRedoCapture(Viewport3D* vp) {
    if (gTRReapplying) return;
    gOp.valid = false;
    if (!g_transformRedoAuto || !vp) return;
    const bool edit = (InteractionMode == EditMode && EditXformActive());
    if (!edit && InteractionMode != ObjectMode) return;            // pose / huesos: no
    if (state != translation && state != rotation && state != EditScale) return;
    if (edit && EditShrinkActive()) return;                         // shrink/fatten: no
    if (!edit && !ActiveObj) return;
    OpTR o; o.valid = true; o.edit = edit; o.free = false; o.type = state; o.axis = axisSelect;
    o.orient = transformOrientation; o.world = Vector3(0, 0, 0); o.undo = NULL; o.prev = UndoLast();
    gOp = o;   // (AxesOrient usa gOp.edit)
    if (state == translation) {
        if (edit) gOp.world = EditXformTransDelta();
        else {
            SaveState* st = NULL;
            for (size_t i = 0; i < objectState.size(); i++) if (objectState[i].obj == ActiveObj) st = &objectState[i];
            if (!st) { gOp.valid = false; return; }
            Matrix4 W; ActiveObj->GetWorldMatrix(W);
            gOp.world = Vector3(W.m[12], W.m[13], W.m[14]) - st->worldPos;
        }
        ValuesFromWorld(gOp.world, OrientPanel(gOp.orient), gTRv);
    } else if (state == rotation) {
        gTRang = gAngleTransform;
        gOp.free = (axisSelect == OrbitalAxis);   // el giro libre no es un angulo sobre un eje: no se re-edita
    } else {
        gTRv[0] = gTRv[1] = gTRv[2] = 1.0f;
        if (edit) {
            const float f = EditXformScaleFactor();
            switch (axisSelect) {
                case X: gTRv[0] = f; break;
                case Y: gTRv[1] = f; break;
                case Z: gTRv[2] = f; break;
                case PlaneX: gTRv[1] = gTRv[2] = f; break;
                case PlaneY: gTRv[0] = gTRv[2] = f; break;
                case PlaneZ: gTRv[0] = gTRv[1] = f; break;
                default: gTRv[0] = gTRv[1] = gTRv[2] = f; break;
            }
        } else {
            SaveState* st = NULL;
            for (size_t i = 0; i < objectState.size(); i++) if (objectState[i].obj == ActiveObj) st = &objectState[i];
            if (!st) { gOp.valid = false; return; }
            const Vector3 s = ActiveObj->scale, s0 = st->scale;
            gTRv[0] = (s0.x != 0.0f) ? s.x / s0.x : 1.0f;
            gTRv[1] = (s0.z != 0.0f) ? s.z / s0.z : 1.0f;   // Y de la UI = z interno
            gTRv[2] = (s0.y != 0.0f) ? s.y / s0.y : 1.0f;
        }
    }
    gTRvp = vp;
}

// ---------------------------------------------------------------------------
//  re-hacer con los valores del panel
// ---------------------------------------------------------------------------
static int ScaleOne(int axis, float f) {
    if (gOp.edit) {
        if (!EditXformStart(EditScale, axis)) return 0;
        axisSelect = axis; EditXformNumValue(f); gTRvp->Accept(); return 1;
    }
    BeginScale(); if (state != EditScale) return 0;
    axisSelect = axis; SetNumericTransform(f); gTRvp->Accept(); return 1;
}
static bool Reapply() {
    if (!gOp.valid || !gTRvp) return false;
    // el historial cambio por otro lado (otra operacion, un undo): el panel ya no sabe que deshacer
    if (UndoLast() != (gOp.undo ? gOp.undo : gOp.prev)) { TransformRedoClose(); return false; }
    gTRReapplying = true;
    if (gOp.undo) UndoUndo();
    transformOrientation = gOp.orient;
    g_prop.on = gTRpropOn; g_prop.connected = gTRpropWith; g_prop.radius = (gTRpropRadius > 1e-4f) ? gTRpropRadius : 1e-4f;
    Viewport3DActive = gTRvp;
    if (gOp.type == translation) {
        const Vector3 D = WorldFromValues(gTRv, OrientPanel(gOp.orient));
        if (gOp.edit) { if (EditXformStart(translation, ViewAxis)) { EditXformTranslationWorld(D); gTRvp->Accept(); } }
        else { BeginTranslate(); if (state == translation) { SetTranslationObjectsWorld(D); gTRvp->Accept(); } }
        gOp.world = D;
    } else if (gOp.type == rotation) {
        if (gOp.edit) { if (EditXformStart(rotation, gOp.axis)) { axisSelect = gOp.axis; EditXformNumValue(gTRang); gTRvp->Accept(); } }
        else { BeginRotate(); if (state == rotation) { axisSelect = gOp.axis; SetNumericTransform(gTRang); gTRvp->Accept(); } }
    } else {
        int n = 0;
        if (fabsf(gTRv[0] - gTRv[1]) < 1e-6f && fabsf(gTRv[1] - gTRv[2]) < 1e-6f) {
            if (fabsf(gTRv[0] - 1.0f) > 1e-6f) n += ScaleOne(XYZ, gTRv[0]);
        } else {
            const int axes[3] = { X, Y, Z };
            for (int k = 0; k < 3; k++) if (fabsf(gTRv[k] - 1.0f) > 1e-6f) n += ScaleOne(axes[k], gTRv[k]);
        }
        if (n > 1) UndoMergeLast(n);   // un solo paso de undo para los tres ejes
    }
    const void* u = UndoLast();
    gOp.undo = (u != gOp.prev) ? u : NULL;
    gTRReapplying = false;
    g_redraw = true;
    return true;
}
static void OnValue() { Reapply(); }
static void OnProp() { if (gTR) gTR->Build(); Reapply(); }
static void Cancel_() {
    if (gOp.valid && gOp.undo && UndoLast() == gOp.undo) { gTRReapplying = true; UndoUndo(); gTRReapplying = false; }
    TransformRedoClose();
    g_redraw = true;
}
static void Accept_() { TransformRedoClose(); }
static void OpenOrient() { if (gTR && !gOp.free) { gTR->listOpen = (gTR->listOpen == 1) ? 0 : 1; gTR->listSel = (OrientPanel(gOp.orient) == LocalOrient) ? 1 : 0; g_redraw = true; } }
static void OpenCurve() { if (gTR) { gTR->listOpen = (gTR->listOpen == 2) ? 0 : 2; gTR->listSel = g_prop.type; g_redraw = true; } }

void TransformRedoAfterConfirm() {
    if (gTRReapplying || !gOp.valid) return;
    const void* u = UndoLast();
    if (!u || u == gOp.prev) { gOp.valid = false; return; }   // no dejo paso (no cambio nada): sin panel
    bool change = true;
    if (gOp.type == translation) change = gOp.world.Length() > 1e-6f;
    else if (gOp.type == rotation) change = gOp.free || fabsf(gTRang) > 1e-4f;
    else change = fabsf(gTRv[0] - 1.0f) > 1e-6f || fabsf(gTRv[1] - 1.0f) > 1e-6f || fabsf(gTRv[2] - 1.0f) > 1e-6f;
    if (!change) { gOp.valid = false; return; }
    gOp.undo = u;
    gTRpropOn = g_prop.on; gTRpropWith = g_prop.connected; gTRpropRadius = g_prop.radius;
    g_undoChangeHook = HookUndo;
    if (gTR) { if (PopUpActive == gTR) PopUpActive = NULL; delete gTR; gTR = NULL; }
    gTR = new TransformRedoPanel();
    PopUpActive = gTR;
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  el panel
// ---------------------------------------------------------------------------
static const char* OrientName(int o) { return (o == LocalOrient) ? "Local" : (o == ViewOrient) ? "View" : (o == NormalOrient) ? "Normal" : "Global"; }
static const char* AxisName(int e) {
    switch (e) { case X: return "X"; case Y: return "Y"; case Z: return "Z"; case OrbitalAxis: return "Free"; default: return "View"; }
}
TransformRedoPanel::TransformRedoPanel()
    : PopUpBase(""), group(NULL), dragField(NULL), dragMoved(false), lastDragMx(0), listOpen(0), listSel(0), list(NULL), frame(NULL) {
    list  = new Card();
    frame = new Card();
    Build();
}
TransformRedoPanel::~TransformRedoPanel() { delete group; delete list; delete frame; }

void TransformRedoPanel::Build() {
    // (nace PLEGADO: solo la flecha y el titulo -ver RedoCollapse en RedoMeshPanel.cpp-; al rearmarse queda como estaba)
    const bool open = group ? group->open : false;
    delete group;
    const char* tit = (gOp.type == translation) ? "Move" : (gOp.type == rotation) ? "Rotate" : "Scale";
    group = new GroupPropertie(T(tit));
    group->valueWidth = 0.55f;
    group->arrow = true;
    group->open = open;
    if (gOp.type == rotation) {
        if (!gOp.free) {
            PropFloat* a = new PropFloat(T("Angle"), "\xC2\xB0");
            a->value = &gTRang; a->SetRange(-36000.0f, 36000.0f); a->fineStep = 1.0f; a->thickStep = 15.0f; a->dragStep = 0.5f;
            a->onChange = OnValue;
            group->properties.push_back(a);
        }
        PropButton* e = new PropButton(T("Axis"));
        e->withLabel = true; e->gray = true; e->button->text = T(AxisName(gOp.axis));
        group->properties.push_back(e);
    } else {
        const char* et[3] = { "X", "Y", "Z" };
        for (int k = 0; k < 3; k++) {
            PropFloat* f = new PropFloat(et[k], (gOp.type == translation) ? "m" : "");
            f->value = &gTRv[k];
            if (gOp.type == translation) { f->SetRange(-1e6f, 1e6f); f->fineStep = 0.1f; f->thickStep = 1.0f; f->dragStep = 0.01f; }
            else { f->SetRange(-1e4f, 1e4f); f->fineStep = 0.05f; f->thickStep = 0.5f; f->dragStep = 0.005f; }
            f->onChange = OnValue;
            group->properties.push_back(f);
        }
    }
    PropButton* o = new PropButton(T("Orientation"));
    o->withLabel = true; o->button->text = T(OrientName(OrientPanel(gOp.orient))); o->action = OpenOrient; o->gray = gOp.free;
    group->properties.push_back(o);
    if (!gOp.free) {
        PropBool* p = new PropBool(T("Proportional Editing"));
        p->value = &gTRpropOn; p->onChange = OnProp;
        group->properties.push_back(p);
        if (gTRpropOn) {
            PropButton* c = new PropButton(T("Falloff"));
            c->withLabel = true; c->button->text = T(ProportionalTypeName(g_prop.type)); c->action = OpenCurve;
            group->properties.push_back(c);
            PropFloat* r = new PropFloat(T("Size"));
            r->value = &gTRpropRadius; r->SetRange(0.001f, 100000.0f); r->fineStep = 0.1f; r->thickStep = 1.0f; r->dragStep = 0.01f;
            r->onChange = OnValue;
            group->properties.push_back(r);
            PropBool* cn = new PropBool(T("Connected"));
            cn->value = &gTRpropWith; cn->onChange = OnValue;
            group->properties.push_back(cn);
        }
    }
    PropButtonRow* b = new PropButtonRow();
    b->Add(T("Cancel"), Cancel_);
    b->Add(T("Accept"), Accept_);
    b->active = 1;
    group->properties.push_back(b);
    group->selectIndex = 0;
    ResizeGroup();
    Relocate();
}
void TransformRedoPanel::ResizeGroup() {
    int w = 220 * GlobalScale;
    if (gTRvp && w > gTRvp->width - gapGS * 4) w = gTRvp->width - gapGS * 4;
    if (w < 120 * GlobalScale) w = 120 * GlobalScale;
    group->Resize(w, 0);
    int height = group->height;
    if (listOpen) { int lx, ly, lw, lh; ListRect(lx, ly, lw, lh); if (ly + lh + borderGS > height) height = ly + lh + borderGS; }
    dz.Measure(height);                      // mas de media pantalla: se ve la mitad y se desplaza
    popUpWindow->Resize(w, dz.visible);
}
void TransformRedoPanel::Relocate() {   // abajo a la izquierda del viewport (donde estaba la barra de herramientas)
    const int height = dz.visible;
    if (!gTRvp) { x = gapGS * 2; y = MenuScreenH - height - gapGS * 2; return; }
    x = gTRvp->x + gapGS * 2;
    y = gTRvp->y + gTRvp->height - height - gapGS * 2;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + height + gapGS * 2)
        y = PopUpActive->y - height - gapGS * 2;   // el teclado numerico abierto encima: el panel sube
    if (y < gTRvp->y + gapGS) y = gTRvp->y + gapGS;
    if (x < gapGS) x = gapGS;
}
int TransformRedoPanel::RowY(int idx) const {
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)group->properties.size(); j++) yRow += group->properties[j]->Resize(group->width);
    return yRow;
}
int TransformRedoPanel::ListCount() const { return (listOpen == 1) ? 2 : (listOpen == 2) ? PropTypes : 0; }
void TransformRedoPanel::ListRect(int& lx, int& ly, int& lw, int& lh) const {
    // la lista se abre HACIA ARRIBA de su fila (el panel esta pegado al borde de abajo)
    const char* which = (listOpen == 1) ? T("Orientation") : T("Falloff");
    int idx = 0;
    for (size_t j = 0; j < group->properties.size(); j++) if (group->properties[j]->name == which) idx = (int)j;
    lx = group->colLabel + 2 * borderGS;
    lw = group->propertiBox->width - bordersGS;
    lh = ListCount() * (RowHeightGS + gapGS) + gapGS;
    ly = RowY(idx) - lh - gapGS;
    if (ly < 0) ly = RowY(idx) + RowHeightGS + gapGS;
}

void TransformRedoPanel::Render() {
    if (!group) return;
    ResizeGroup(); Relocate();
    // la lista hacia arriba puede salirse del rect de la tarjeta: la ventana del popup la abarca
    int top = 0;
    if (listOpen) { int lx, ly, lw, lh; ListRect(lx, ly, lw, lh); if (ly - dz.scroll < top) top = ly - dz.scroll; }
    const int y0 = y; y += top; popUpWindow->Resize(popUpWindow->width, popUpWindow->height - top);
    initView();
    // ARRASTRANDO un valor: la tarjeta translucida (se ve detras el cambio) y la fila que se edita opaca, al final
    const int rowDrag = (dragField && dragMoved) ? group->IndexOf(dragField) : -1;
    if (rowDrag >= 0) w3dEngine::g_alphaColor = kAlphaCardDrag;
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)(-top - dz.scroll), 0);
    group->Render();
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    // el borde VERDE va alrededor de lo que se ve (es la ventana con el foco), aunque el contenido este desplazado
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)dz.scroll, 0);
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    frame->Resize(popUpWindow->width, dz.visible); frame->RenderBorder(false);
    if (dz.Scrollable()) {   // la barrita que dice cuanto falta
        const int height = dz.visible * dz.visible / (dz.total > 0 ? dz.total : 1);
        const int ty = (dz.total - dz.visible > 0) ? (dz.visible - height) * dz.scroll / (dz.total - dz.visible) : 0;
        w3dEngine::Translatef((GLfloat)(popUpWindow->width - 3 * GlobalScale), (GLfloat)ty, 0);
        SetColorID(ColorID::accent, 0.7f);
        list->Resize(2 * GlobalScale, height); list->RenderObject(false);
    }
    w3dEngine::PopMatrix();
    if (listOpen) {
        int lx, ly, lw, lh; ListRect(lx, ly, lw, lh);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
        SetColorID(ColorID::headerColor, 1.0f);
        list->Resize(lw, lh); list->RenderObject(false);
        w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
        list->RenderBorder(false);
        const int n = ListCount();
        for (int i = 0; i < n; i++) {
            const int ry = gapGS + i * (RowHeightGS + gapGS);
            const bool current = (listOpen == 1) ? (i == (OrientPanel(gOp.orient) == LocalOrient ? 1 : 0)) : (i == g_prop.type);
            if (i == listSel) {
                w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)borderGS, (GLfloat)ry, 0);
                SetColorID(ColorID::accent, 0.35f); list->Resize(lw - bordersGS, RowHeightGS); list->RenderObject(false);
                w3dEngine::PopMatrix();
            }
            SetColorID(current ? ColorID::accent : ColorID::white, 1.0f);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry + (RowHeightGS - LetterHeightGS) / 2), 0);
            RenderBitmapText(listOpen == 1 ? T(i == 1 ? "Local" : "Global") : T(ProportionalTypeName(i)), textAlign::left, lw - bordersGS - gapGS);
            w3dEngine::PopMatrix();
        }
        w3dEngine::PopMatrix();
    }
    w3dEngine::PopMatrix();
    w3dEngine::g_alphaColor = 1.0f;
    if (rowDrag >= 0) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(0, (GLfloat)(-top - dz.scroll), 0);   // (la misma base que el grupo)
        group->RenderRow(rowDrag);
        w3dEngine::PopMatrix();
    }
    endView();
    y = y0;
}

static void PickFromList(int which, int i) {
    if (which == 1) { gOp.orient = (i == 1) ? LocalOrient : GlobalOrient; Reapply(); }
    else if (which == 2 && i >= 0 && i < PropTypes) { ProportionalSetType(i); Reapply(); }
    if (gTR) gTR->Build();
}
bool TransformRedoPanel::Click(int mx, int my) {
    const int myL = my - y + dz.scroll, lx = mx - x;
    if (listOpen) {
        int ax, ay, aw, ah; ListRect(ax, ay, aw, ah);
        const int which = listOpen; listOpen = 0;
        if (lx >= ax && lx < ax + aw && myL >= ay && myL < ay + ah) {
            PickFromList(which, (myL - ay - gapGS) / (RowHeightGS + gapGS));
            g_redraw = true;
            return true;
        }
        g_redraw = true;
    }
    if (!Contains(mx, my)) return false;   // afuera: el panel se cierra (acepta) y el click sigue al viewport
    // el TITULO pliega / despliega la tarjeta
    if (myL < borderGS + RowHeightGS + gapGS) { group->open = !group->open; dz.scroll = 0; g_redraw = true; return true; }
    if (!group->open) return true;
    dz.Down(mx, my);
    if (!dz.Scrollable()) { dz.pressed = false; ClickRow(mx, my, false); return true; }   // entra entero: directo
    ClickRow(mx, my, true);   // desplazable: solo se arma el arrastre de un valor; lo demas se resuelve al soltar
    return true;
}
// el click sobre una fila. onlyBuild: un valor numerico queda listo para arrastrar y nada mas (el resto espera a
// saber si el gesto es un toque o un desplazamiento)
void TransformRedoPanel::ClickRow(int mx, int my, bool onlyBuild) {
    const int myL = my - y + dz.scroll, lx = mx - x;
    const int boxStart = group->colLabel + 2 * borderGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h) {
            const PropertyType::Enum type = p->GetType();
            if (type == PropertyType::Float) {
                group->selectIndex = (int)j;
                g_activeTextField = NULL;
                dragField = static_cast<PropFloat*>(p); dragMoved = false; lastDragMx = mx;
                g_redraw = true;
                return;
            }
            if (onlyBuild) return;
            group->selectIndex = (int)j;
            g_activeTextField = NULL;
            if (type == PropertyType::Bool || type == PropertyType::Button) {
                p->EditPropertie();
            } else if (type == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(p);
                const int cw = row->CellWidth(group->width), x0 = boxStart - group->colLabel;
                int cell = -1, vis = 0;
                for (size_t b = 0; b < row->buttons.size(); b++) {
                    if (!row->buttons[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lx >= cx && lx < cx + cw) { cell = (int)b; break; }
                    vis++;
                }
                if (cell >= 0) { row->active = cell; if (row->actions[(size_t)cell]) row->actions[(size_t)cell](); }
                return;
            }
            g_redraw = true;
            return;
        }
        yRow += h;
    }
}
bool TransformRedoPanel::Motion(int mx, int my) {
    // arrastre VERTICAL en un panel desplazable: desplaza (y suelta el valor que estaba por arrastrarse)
    if (!dragMoved && dz.Move(mx, my)) { dragField = NULL; g_redraw = true; return true; }
    if (dz.scrolling) return true;
    if (dragField && dragField->value) {
        // desplazable: el valor recien se arrastra cuando el gesto se va claramente de costado
        if (dz.Scrollable() && !dragMoved) {
            const int threshold = 6 * (GlobalScale > 0 ? GlobalScale : 1);
            if (mx - dz.pressX < threshold && dz.pressX - mx < threshold) return true;
            lastDragMx = mx;
        }
        const int dmx = mx - lastDragMx; lastDragMx = mx;
        if (dmx != 0 || (dz.Scrollable() && !dragMoved)) dragMoved = true;
        if (dmx != 0) { dragField->Set(*dragField->value + dmx * dragField->dragStep); g_redraw = true; }
    }
    return true;
}
void TransformRedoPanel::Release() {
    const bool touch = dz.Up();
    if (dragField) {
        if (!dragMoved && dragField->value) {
            dragField->StartEditText();            // mouse: se escribe el valor exacto
            if (ToolbarUsesTouch()) KeyboardNumberOpen();       // tactil: el teclado numerico en pantalla
        }
        dragField = NULL;
        return;
    }
    if (touch && dz.Scrollable()) ClickRow(dz.pressX, dz.pressY, false);   // fue un toque: el click de verdad
}
void TransformRedoPanel::Wheel(int delta) { dz.Wheel(delta); g_redraw = true; }
bool TransformRedoPanel::Dragging() { return dragField != NULL || dz.pressed; }
bool TransformRedoPanel::Key(int key) {
    if (!group) return true;
    // PLEGADO: OK / derecha lo despliega, C lo cierra; lo demas sigue de largo (se sigue trabajando)
    if (!group->open && !listOpen) {
        if (key == LayoutKey::Enter || key == LayoutKey::Right) { group->open = true; g_redraw = true; return true; }
        if (key == LayoutKey::Cancel) { Close(); return true; }
        return false;
    }
    const int n = (int)group->properties.size();
    if (listOpen) {
        const int nl = ListCount();
        switch (key) {
            case LayoutKey::Up:   if (listSel > 0) listSel--; break;
            case LayoutKey::Down: if (listSel + 1 < nl) listSel++; break;
            case LayoutKey::Enter: { const int c = listOpen; listOpen = 0; PickFromList(c, listSel); break; }
            case LayoutKey::Cancel: listOpen = 0; break;
            default: break;
        }
        g_redraw = true;
        return true;
    }
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (key == LayoutKey::Down) ? +1 : -1;
            int s = group->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (group->properties[s]->Selectable()) break; }
            group->selectIndex = s;
            dz.ToView(RowY(s), group->properties[(size_t)s]->Resize(group->width));   // la fila elegida, a la vista
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (!sel) break;
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value + (key == LayoutKey::Right ? pf->fineStep : -pf->fineStep)); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->active += (key == LayoutKey::Right) ? 1 : -1; row->ActiveClamp(); }
            else if (sel->GetType() == PropertyType::Bool) sel->EditPropertie();
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ActiveClamp();
                if (row->active >= 0 && row->active < (int)row->actions.size() && row->actions[row->active]) row->actions[row->active]();
                return true;
            }
            if (sel && sel->GetType() == PropertyType::Float) static_cast<PropFloat*>(sel)->StartEditText();
            else if (sel) sel->EditPropertie();
            break;
        }
        case LayoutKey::Cancel: Accept_(); return true;   // Esc / Supr / C del telefono: cierra aceptando (solo Cancelar deshace)
    }
    g_redraw = true;
    return true;
}
void TransformRedoPanel::Close() {
    listOpen = 0;
    g_activeTextField = NULL;
    PopUpBase::Close();
    g_redraw = true;
}

// (harness)
bool TransformRedoSet(int field, float value) {
    if (!TransformRedoOpen(NULL)) return false;
    if (gOp.type == rotation) gTRang = value; else if (field >= 0 && field < 3) gTRv[field] = value; else return false;
    return Reapply();
}
void TransformRedoButton(bool accept) { if (accept) Accept_(); else Cancel_(); }
TransformRedoPanel* TransformRedoCurrentPanel() { return (gTR && PopUpActive == gTR) ? gTR : NULL; }
