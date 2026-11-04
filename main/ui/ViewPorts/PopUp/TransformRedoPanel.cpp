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
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey, EditXform*
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil
#include "NumPad.h"
#include "objects/ObjectMode.h"             // SetPosicion / SetRotacion / SetEscala / SetTransformNumerico
#include "objects/Mesh.h"
#include "edit/Proporcional.h"
#include "Undo.h"
#include "app/variables.h"
#include <math.h>
#include <stdio.h>

extern bool g_redraw;

bool g_transformRedoAuto = true;
static TransformRedoPanel* gTR = NULL;
static Viewport3D* gTRvp = NULL;
static bool gTRreaplicando = false;

// la operacion: que se hizo y sobre que
struct OpTR {
    bool valida, edit, libre;
    int tipo, eje, orient;
    Vector3 mundo;        // mover: el delta de MUNDO (engine xyz)
    const void* undo;     // su paso de undo (NULL = no dejo paso: el estado es el de antes)
    const void* antes;    // el paso que habia antes de la operacion
};
static OpTR gOp;
static float gTRv[3] = { 0, 0, 0 };   // mover: X/Y/Z en la orientacion; escalar: factores X/Y/Z
static float gTRang = 0.0f;           // rotar: grados
static bool  gTRpropOn = false, gTRpropCon = false;
static float gTRpropRadio = 1.0f;

bool TransformRedoReaplicando() { return gTRreaplicando; }
bool TransformRedoAbierto(const Viewport3D* vp) { return gTR && PopUpActive == gTR && (!vp || vp == gTRvp); }
int  TransformRedoTipo() { return (gTR && PopUpActive == gTR) ? gOp.tipo : -1; }
void TransformRedoCerrar() { if (gTR && PopUpActive == gTR) gTR->Cerrar(); }
static void HookUndo() { if (!gTRreaplicando) TransformRedoCerrar(); }   // Ctrl+Z / Ctrl+Y: el panel ya no vale

// ---------------------------------------------------------------------------
//  ejes (UI: X, Y = Z interno, Z = Y interno) en la orientacion de la operacion
// ---------------------------------------------------------------------------
static Object* Objetivo() { return gOp.edit ? (Object*)g_editMesh : ObjActivo; }
static void EjesOrient(int orient, Vector3 ej[3]) {
    ej[0] = Vector3(1, 0, 0); ej[1] = Vector3(0, 0, 1); ej[2] = Vector3(0, 1, 0);
    Object* o = Objetivo();
    if (orient != LocalOrient || !o) return;
    Matrix4 W; o->GetWorldMatrix(W);
    const int col[3] = { 0, 2, 1 };
    for (int k = 0; k < 3; k++) {
        Vector3 e(W.m[col[k]*4], W.m[col[k]*4+1], W.m[col[k]*4+2]);
        const float l = e.Length(); if (l > 1e-6f) ej[k] = e * (1.0f / l);
    }
}
static void ValoresDesdeMundo(const Vector3& D, int orient, float v[3]) {
    Vector3 ej[3]; EjesOrient(orient, ej);
    for (int k = 0; k < 3; k++) v[k] = D.Dot(ej[k]);
}
static Vector3 MundoDesdeValores(const float v[3], int orient) {
    Vector3 ej[3]; EjesOrient(orient, ej);
    return ej[0] * v[0] + ej[1] * v[1] + ej[2] * v[2];
}
static int OrientPanel(int o) { return (o == LocalOrient) ? LocalOrient : GlobalOrient; }

// ---------------------------------------------------------------------------
//  capturar (Viewport3D::Aceptar)
// ---------------------------------------------------------------------------
void TransformRedoCapturar(Viewport3D* vp) {
    if (gTRreaplicando) return;
    gOp.valida = false;
    if (!g_transformRedoAuto || !vp) return;
    const bool edit = (InteractionMode == EditMode && EditXformActivo());
    if (!edit && InteractionMode != ObjectMode) return;            // pose / huesos: no
    if (estado != translacion && estado != rotacion && estado != EditScale) return;
    if (edit && EditShrinkActivo()) return;                         // shrink/fatten: no
    if (!edit && !ObjActivo) return;
    OpTR o; o.valida = true; o.edit = edit; o.libre = false; o.tipo = estado; o.eje = axisSelect;
    o.orient = transformOrientation; o.mundo = Vector3(0, 0, 0); o.undo = NULL; o.antes = UndoUltimo();
    gOp = o;   // (EjesOrient usa gOp.edit)
    if (estado == translacion) {
        if (edit) gOp.mundo = EditXformTransDelta();
        else {
            SaveState* st = NULL;
            for (size_t i = 0; i < estadoObjetos.size(); i++) if (estadoObjetos[i].obj == ObjActivo) st = &estadoObjetos[i];
            if (!st) { gOp.valida = false; return; }
            Matrix4 W; ObjActivo->GetWorldMatrix(W);
            gOp.mundo = Vector3(W.m[12], W.m[13], W.m[14]) - st->worldPos;
        }
        ValoresDesdeMundo(gOp.mundo, OrientPanel(gOp.orient), gTRv);
    } else if (estado == rotacion) {
        gTRang = gAnguloTransform;
        gOp.libre = (axisSelect == OrbitalAxis);   // el giro libre no es un angulo sobre un eje: no se re-edita
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
            for (size_t i = 0; i < estadoObjetos.size(); i++) if (estadoObjetos[i].obj == ObjActivo) st = &estadoObjetos[i];
            if (!st) { gOp.valida = false; return; }
            const Vector3 s = ObjActivo->scale, s0 = st->scale;
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
static int EscalarUno(int eje, float f) {
    if (gOp.edit) {
        if (!EditXformStart(EditScale, eje)) return 0;
        axisSelect = eje; EditXformNumValor(f); gTRvp->Aceptar(); return 1;
    }
    SetEscala(); if (estado != EditScale) return 0;
    axisSelect = eje; SetTransformNumerico(f); gTRvp->Aceptar(); return 1;
}
static bool Reaplicar() {
    if (!gOp.valida || !gTRvp) return false;
    // el historial cambio por otro lado (otra operacion, un undo): el panel ya no sabe que deshacer
    if (UndoUltimo() != (gOp.undo ? gOp.undo : gOp.antes)) { TransformRedoCerrar(); return false; }
    gTRreaplicando = true;
    if (gOp.undo) UndoDeshacer();
    transformOrientation = gOp.orient;
    g_prop.on = gTRpropOn; g_prop.conectado = gTRpropCon; g_prop.radio = (gTRpropRadio > 1e-4f) ? gTRpropRadio : 1e-4f;
    Viewport3DActive = gTRvp;
    if (gOp.tipo == translacion) {
        const Vector3 D = MundoDesdeValores(gTRv, OrientPanel(gOp.orient));
        if (gOp.edit) { if (EditXformStart(translacion, ViewAxis)) { EditXformTraslacionMundo(D); gTRvp->Aceptar(); } }
        else { SetPosicion(); if (estado == translacion) { SetTranslacionObjetosMundo(D); gTRvp->Aceptar(); } }
        gOp.mundo = D;
    } else if (gOp.tipo == rotacion) {
        if (gOp.edit) { if (EditXformStart(rotacion, gOp.eje)) { axisSelect = gOp.eje; EditXformNumValor(gTRang); gTRvp->Aceptar(); } }
        else { SetRotacion(); if (estado == rotacion) { axisSelect = gOp.eje; SetTransformNumerico(gTRang); gTRvp->Aceptar(); } }
    } else {
        int n = 0;
        if (fabsf(gTRv[0] - gTRv[1]) < 1e-6f && fabsf(gTRv[1] - gTRv[2]) < 1e-6f) {
            if (fabsf(gTRv[0] - 1.0f) > 1e-6f) n += EscalarUno(XYZ, gTRv[0]);
        } else {
            const int ejes[3] = { X, Y, Z };
            for (int k = 0; k < 3; k++) if (fabsf(gTRv[k] - 1.0f) > 1e-6f) n += EscalarUno(ejes[k], gTRv[k]);
        }
        if (n > 1) UndoFundirUltimos(n);   // un solo paso de undo para los tres ejes
    }
    const void* u = UndoUltimo();
    gOp.undo = (u != gOp.antes) ? u : NULL;
    gTRreaplicando = false;
    g_redraw = true;
    return true;
}
static void OnValor() { Reaplicar(); }
static void OnProp() { if (gTR) gTR->Armar(); Reaplicar(); }
static void Cancelar_() {
    if (gOp.valida && gOp.undo && UndoUltimo() == gOp.undo) { gTRreaplicando = true; UndoDeshacer(); gTRreaplicando = false; }
    TransformRedoCerrar();
    g_redraw = true;
}
static void Aceptar_() { TransformRedoCerrar(); }
static void AbrirOrient() { if (gTR && !gOp.libre) { gTR->listaAbierta = (gTR->listaAbierta == 1) ? 0 : 1; gTR->listaSel = (OrientPanel(gOp.orient) == LocalOrient) ? 1 : 0; g_redraw = true; } }
static void AbrirCurva() { if (gTR) { gTR->listaAbierta = (gTR->listaAbierta == 2) ? 0 : 2; gTR->listaSel = g_prop.tipo; g_redraw = true; } }

void TransformRedoTrasConfirmar() {
    if (gTRreaplicando || !gOp.valida) return;
    const void* u = UndoUltimo();
    if (!u || u == gOp.antes) { gOp.valida = false; return; }   // no dejo paso (no cambio nada): sin panel
    bool cambio = true;
    if (gOp.tipo == translacion) cambio = gOp.mundo.Length() > 1e-6f;
    else if (gOp.tipo == rotacion) cambio = gOp.libre || fabsf(gTRang) > 1e-4f;
    else cambio = fabsf(gTRv[0] - 1.0f) > 1e-6f || fabsf(gTRv[1] - 1.0f) > 1e-6f || fabsf(gTRv[2] - 1.0f) > 1e-6f;
    if (!cambio) { gOp.valida = false; return; }
    gOp.undo = u;
    gTRpropOn = g_prop.on; gTRpropCon = g_prop.conectado; gTRpropRadio = g_prop.radio;
    g_undoCambioHook = HookUndo;
    if (gTR) { if (PopUpActive == gTR) PopUpActive = NULL; delete gTR; gTR = NULL; }
    gTR = new TransformRedoPanel();
    PopUpActive = gTR;
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  el panel
// ---------------------------------------------------------------------------
static const char* NombreOrient(int o) { return (o == LocalOrient) ? "Local" : (o == ViewOrient) ? "View" : (o == NormalOrient) ? "Normal" : "Global"; }
static const char* NombreEje(int e) {
    switch (e) { case X: return "X"; case Y: return "Y"; case Z: return "Z"; case OrbitalAxis: return "Free"; default: return "View"; }
}
TransformRedoPanel::TransformRedoPanel()
    : PopUpBase(""), grupo(NULL), dragField(NULL), dragMoved(false), lastDragMx(0), listaAbierta(0), listaSel(0), lista(NULL), marco(NULL) {
    lista = new Card();
    marco = new Card();
    Armar();
}
TransformRedoPanel::~TransformRedoPanel() { delete grupo; delete lista; delete marco; }

void TransformRedoPanel::Armar() {
    // (nace PLEGADO: solo la flecha y el titulo -ver RedoPlegar en RedoMeshPanel.cpp-; al rearmarse queda como estaba)
    const bool abierto = grupo ? grupo->open : false;
    delete grupo;
    const char* tit = (gOp.tipo == translacion) ? "Move" : (gOp.tipo == rotacion) ? "Rotate" : "Scale";
    grupo = new GroupPropertie(T(tit));
    grupo->anchoValores = 0.55f;
    grupo->flecha = true;
    grupo->open = abierto;
    if (gOp.tipo == rotacion) {
        if (!gOp.libre) {
            PropFloat* a = new PropFloat(T("Angle"), "\xC2\xB0");
            a->value = &gTRang; a->SetRango(-36000.0f, 36000.0f); a->stepFino = 1.0f; a->stepGrueso = 15.0f; a->dragStep = 0.5f;
            a->onChange = OnValor;
            grupo->properties.push_back(a);
        }
        PropButton* e = new PropButton(T("Axis"));
        e->conLabel = true; e->gris = true; e->button->text = T(NombreEje(gOp.eje));
        grupo->properties.push_back(e);
    } else {
        const char* et[3] = { "X", "Y", "Z" };
        for (int k = 0; k < 3; k++) {
            PropFloat* f = new PropFloat(et[k], (gOp.tipo == translacion) ? "m" : "");
            f->value = &gTRv[k];
            if (gOp.tipo == translacion) { f->SetRango(-1e6f, 1e6f); f->stepFino = 0.1f; f->stepGrueso = 1.0f; f->dragStep = 0.01f; }
            else { f->SetRango(-1e4f, 1e4f); f->stepFino = 0.05f; f->stepGrueso = 0.5f; f->dragStep = 0.005f; }
            f->onChange = OnValor;
            grupo->properties.push_back(f);
        }
    }
    PropButton* o = new PropButton(T("Orientation"));
    o->conLabel = true; o->button->text = T(NombreOrient(OrientPanel(gOp.orient))); o->action = AbrirOrient; o->gris = gOp.libre;
    grupo->properties.push_back(o);
    if (!gOp.libre) {
        PropBool* p = new PropBool(T("Proportional Editing"));
        p->value = &gTRpropOn; p->onChange = OnProp;
        grupo->properties.push_back(p);
        if (gTRpropOn) {
            PropButton* c = new PropButton(T("Falloff"));
            c->conLabel = true; c->button->text = T(ProporcionalTipoNombre(g_prop.tipo)); c->action = AbrirCurva;
            grupo->properties.push_back(c);
            PropFloat* r = new PropFloat(T("Size"));
            r->value = &gTRpropRadio; r->SetRango(0.001f, 100000.0f); r->stepFino = 0.1f; r->stepGrueso = 1.0f; r->dragStep = 0.01f;
            r->onChange = OnValor;
            grupo->properties.push_back(r);
            PropBool* cn = new PropBool(T("Connected"));
            cn->value = &gTRpropCon; cn->onChange = OnValor;
            grupo->properties.push_back(cn);
        }
    }
    PropButtonRow* b = new PropButtonRow();
    b->Agregar(T("Cancel"), Cancelar_);
    b->Agregar(T("Accept"), Aceptar_);
    b->activo = 1;
    grupo->properties.push_back(b);
    grupo->selectIndex = 0;
    ResizeGrupo();
    Reubicar();
}
void TransformRedoPanel::ResizeGrupo() {
    int w = 220 * GlobalScale;
    if (gTRvp && w > gTRvp->width - gapGS * 4) w = gTRvp->width - gapGS * 4;
    if (w < 120 * GlobalScale) w = 120 * GlobalScale;
    grupo->Resize(w, 0);
    int alto = grupo->height;
    if (listaAbierta) { int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh); if (ly + lh + borderGS > alto) alto = ly + lh + borderGS; }
    dz.Medir(alto);                      // mas de media pantalla: se ve la mitad y se desplaza
    popUpWindow->Resize(w, dz.visible);
}
void TransformRedoPanel::Reubicar() {   // abajo a la izquierda del viewport (donde estaba la barra de herramientas)
    const int alto = dz.visible;
    if (!gTRvp) { x = gapGS * 2; y = MenuPantallaH - alto - gapGS * 2; return; }
    x = gTRvp->x + gapGS * 2;
    y = gTRvp->y + gTRvp->height - alto - gapGS * 2;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + alto + gapGS * 2)
        y = PopUpActive->y - alto - gapGS * 2;   // el teclado numerico abierto encima: el panel sube
    if (y < gTRvp->y + gapGS) y = gTRvp->y + gapGS;
    if (x < gapGS) x = gapGS;
}
int TransformRedoPanel::FilaY(int idx) const {
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)grupo->properties.size(); j++) yRow += grupo->properties[j]->Resize(grupo->width);
    return yRow;
}
int TransformRedoPanel::ListaCantidad() const { return (listaAbierta == 1) ? 2 : (listaAbierta == 2) ? PropTipos : 0; }
void TransformRedoPanel::ListaRect(int& lx, int& ly, int& lw, int& lh) const {
    // la lista se abre HACIA ARRIBA de su fila (el panel esta pegado al borde de abajo)
    const char* cual = (listaAbierta == 1) ? T("Orientation") : T("Falloff");
    int idx = 0;
    for (size_t j = 0; j < grupo->properties.size(); j++) if (grupo->properties[j]->name == cual) idx = (int)j;
    lx = grupo->colEtiqueta + 2 * borderGS;
    lw = grupo->propertiBox->width - bordersGS;
    lh = ListaCantidad() * (RenglonHeightGS + gapGS) + gapGS;
    ly = FilaY(idx) - lh - gapGS;
    if (ly < 0) ly = FilaY(idx) + RenglonHeightGS + gapGS;
}

void TransformRedoPanel::Render() {
    if (!grupo) return;
    ResizeGrupo(); Reubicar();
    // la lista hacia arriba puede salirse del rect de la tarjeta: la ventana del popup la abarca
    int top = 0;
    if (listaAbierta) { int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh); if (ly - dz.scroll < top) top = ly - dz.scroll; }
    const int y0 = y; y += top; popUpWindow->Resize(popUpWindow->width, popUpWindow->height - top);
    initView();
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)(-top - dz.scroll), 0);
    grupo->Render();
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    // el borde VERDE va alrededor de lo que se ve (es la ventana con el foco), aunque el contenido este desplazado
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)dz.scroll, 0);
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    marco->Resize(popUpWindow->width, dz.visible); marco->RenderBorder(false);
    if (dz.Desplazable()) {   // la barrita que dice cuanto falta
        const int alto = dz.visible * dz.visible / (dz.total > 0 ? dz.total : 1);
        const int ty = (dz.total - dz.visible > 0) ? (dz.visible - alto) * dz.scroll / (dz.total - dz.visible) : 0;
        w3dEngine::Translatef((GLfloat)(popUpWindow->width - 3 * GlobalScale), (GLfloat)ty, 0);
        SetColorID(ColorID::accent, 0.7f);
        lista->Resize(2 * GlobalScale, alto); lista->RenderObject(false);
    }
    w3dEngine::PopMatrix();
    if (listaAbierta) {
        int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
        SetColorID(ColorID::headerColor, 1.0f);
        lista->Resize(lw, lh); lista->RenderObject(false);
        w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
        lista->RenderBorder(false);
        const int n = ListaCantidad();
        for (int i = 0; i < n; i++) {
            const int ry = gapGS + i * (RenglonHeightGS + gapGS);
            const bool actual = (listaAbierta == 1) ? (i == (OrientPanel(gOp.orient) == LocalOrient ? 1 : 0)) : (i == g_prop.tipo);
            if (i == listaSel) {
                w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)borderGS, (GLfloat)ry, 0);
                SetColorID(ColorID::accent, 0.35f); lista->Resize(lw - bordersGS, RenglonHeightGS); lista->RenderObject(false);
                w3dEngine::PopMatrix();
            }
            SetColorID(actual ? ColorID::accent : ColorID::blanco, 1.0f);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry + (RenglonHeightGS - LetterHeightGS) / 2), 0);
            RenderBitmapText(listaAbierta == 1 ? T(i == 1 ? "Local" : "Global") : T(ProporcionalTipoNombre(i)), textAlign::left, lw - bordersGS - gapGS);
            w3dEngine::PopMatrix();
        }
        w3dEngine::PopMatrix();
    }
    w3dEngine::PopMatrix();
    endView();
    y = y0;
}

static void ElegirDeLista(int cual, int i) {
    if (cual == 1) { gOp.orient = (i == 1) ? LocalOrient : GlobalOrient; Reaplicar(); }
    else if (cual == 2 && i >= 0 && i < PropTipos) { ProporcionalSetTipo(i); Reaplicar(); }
    if (gTR) gTR->Armar();
}
bool TransformRedoPanel::Click(int mx, int my) {
    const int myL = my - y + dz.scroll, lx = mx - x;
    if (listaAbierta) {
        int ax, ay, aw, ah; ListaRect(ax, ay, aw, ah);
        const int cual = listaAbierta; listaAbierta = 0;
        if (lx >= ax && lx < ax + aw && myL >= ay && myL < ay + ah) {
            ElegirDeLista(cual, (myL - ay - gapGS) / (RenglonHeightGS + gapGS));
            g_redraw = true;
            return true;
        }
        g_redraw = true;
    }
    if (!Contains(mx, my)) return false;   // afuera: el panel se cierra (acepta) y el click sigue al viewport
    // el TITULO pliega / despliega la tarjeta
    if (myL < borderGS + RenglonHeightGS + gapGS) { grupo->open = !grupo->open; dz.scroll = 0; g_redraw = true; return true; }
    if (!grupo->open) return true;
    dz.Abajo(mx, my);
    if (!dz.Desplazable()) { dz.presionado = false; ClickFila(mx, my, false); return true; }   // entra entero: directo
    ClickFila(mx, my, true);   // desplazable: solo se arma el arrastre de un valor; lo demas se resuelve al soltar
    return true;
}
// el click sobre una fila. soloArmar: un valor numerico queda listo para arrastrar y nada mas (el resto espera a
// saber si el gesto es un toque o un desplazamiento)
void TransformRedoPanel::ClickFila(int mx, int my, bool soloArmar) {
    const int myL = my - y + dz.scroll, lx = mx - x;
    const int boxStart = grupo->colEtiqueta + 2 * borderGS;
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (size_t j = 0; j < grupo->properties.size(); j++) {
        PropertieBase* p = grupo->properties[j];
        const int h = p->Resize(grupo->width);
        if (p->Seleccionable() && myL >= yRow && myL < yRow + h) {
            const PropertyType::Enum tipo = p->GetType();
            if (tipo == PropertyType::Float) {
                grupo->selectIndex = (int)j;
                g_textFieldActivo = NULL;
                dragField = static_cast<PropFloat*>(p); dragMoved = false; lastDragMx = mx;
                g_redraw = true;
                return;
            }
            if (soloArmar) return;
            grupo->selectIndex = (int)j;
            g_textFieldActivo = NULL;
            if (tipo == PropertyType::Bool || tipo == PropertyType::Button) {
                p->EditPropertie();
            } else if (tipo == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(p);
                const int cw = row->AnchoCelda(grupo->width), x0 = boxStart - grupo->colEtiqueta;
                int celda = -1, vis = 0;
                for (size_t b = 0; b < row->botones.size(); b++) {
                    if (!row->botones[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lx >= cx && lx < cx + cw) { celda = (int)b; break; }
                    vis++;
                }
                if (celda >= 0) { row->activo = celda; if (row->acciones[(size_t)celda]) row->acciones[(size_t)celda](); }
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
    if (!dragMoved && dz.Mover(mx, my)) { dragField = NULL; g_redraw = true; return true; }
    if (dz.desplazando) return true;
    if (dragField && dragField->value) {
        // desplazable: el valor recien se arrastra cuando el gesto se va claramente de costado
        if (dz.Desplazable() && !dragMoved) {
            const int umbral = 6 * (GlobalScale > 0 ? GlobalScale : 1);
            if (mx - dz.pressX < umbral && dz.pressX - mx < umbral) return true;
            lastDragMx = mx;
        }
        const int dmx = mx - lastDragMx; lastDragMx = mx;
        if (dmx != 0 || (dz.Desplazable() && !dragMoved)) dragMoved = true;
        if (dmx != 0) { dragField->Set(*dragField->value + dmx * dragField->dragStep); g_redraw = true; }
    }
    return true;
}
void TransformRedoPanel::Soltar() {
    const bool toque = dz.Arriba();
    if (dragField) {
        if (!dragMoved && dragField->value) {
            dragField->IniciarEdicionTexto();            // mouse: se escribe el valor exacto
            if (ToolbarUsaTactil()) NumPadAbrir();       // tactil: el teclado numerico en pantalla
        }
        dragField = NULL;
        return;
    }
    if (toque && dz.Desplazable()) ClickFila(dz.pressX, dz.pressY, false);   // fue un toque: el click de verdad
}
void TransformRedoPanel::Wheel(int delta) { dz.Rueda(delta); g_redraw = true; }
bool TransformRedoPanel::Arrastrando() { return dragField != NULL || dz.presionado; }
bool TransformRedoPanel::Tecla(int tecla) {
    if (!grupo) return true;
    // PLEGADO: OK / derecha lo despliega, C lo cierra; lo demas sigue de largo (se sigue trabajando)
    if (!grupo->open && !listaAbierta) {
        if (tecla == LayoutKey::Enter || tecla == LayoutKey::Right) { grupo->open = true; g_redraw = true; return true; }
        if (tecla == LayoutKey::Cancel) { Cerrar(); return true; }
        return false;
    }
    const int n = (int)grupo->properties.size();
    if (listaAbierta) {
        const int nl = ListaCantidad();
        switch (tecla) {
            case LayoutKey::Up:   if (listaSel > 0) listaSel--; break;
            case LayoutKey::Down: if (listaSel + 1 < nl) listaSel++; break;
            case LayoutKey::Enter: { const int c = listaAbierta; listaAbierta = 0; ElegirDeLista(c, listaSel); break; }
            case LayoutKey::Cancel: listaAbierta = 0; break;
            default: break;
        }
        g_redraw = true;
        return true;
    }
    switch (tecla) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (tecla == LayoutKey::Down) ? +1 : -1;
            int s = grupo->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (grupo->properties[s]->Seleccionable()) break; }
            grupo->selectIndex = s;
            dz.AVista(FilaY(s), grupo->properties[(size_t)s]->Resize(grupo->width));   // la fila elegida, a la vista
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[grupo->selectIndex] : NULL;
            if (!sel) break;
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value + (tecla == LayoutKey::Right ? pf->stepFino : -pf->stepFino)); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->activo += (tecla == LayoutKey::Right) ? 1 : -1; row->ClampActivo(); }
            else if (sel->GetType() == PropertyType::Bool) sel->EditPropertie();
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[grupo->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ClampActivo();
                if (row->activo >= 0 && row->activo < (int)row->acciones.size() && row->acciones[row->activo]) row->acciones[row->activo]();
                return true;
            }
            if (sel && sel->GetType() == PropertyType::Float) static_cast<PropFloat*>(sel)->IniciarEdicionTexto();
            else if (sel) sel->EditPropertie();
            break;
        }
        case LayoutKey::Cancel: Aceptar_(); return true;   // Esc / Supr / C del telefono: cierra aceptando (solo Cancelar deshace)
    }
    g_redraw = true;
    return true;
}
void TransformRedoPanel::Cerrar() {
    listaAbierta = 0;
    g_textFieldActivo = NULL;
    PopUpBase::Cerrar();
    g_redraw = true;
}

// (harness)
bool TransformRedoFijar(int campo, float valor) {
    if (!TransformRedoAbierto(NULL)) return false;
    if (gOp.tipo == rotacion) gTRang = valor; else if (campo >= 0 && campo < 3) gTRv[campo] = valor; else return false;
    return Reaplicar();
}
void TransformRedoBoton(bool aceptar) { if (aceptar) Aceptar_(); else Cancelar_(); }
TransformRedoPanel* TransformRedoPanelActual() { return (gTR && PopUpActive == gTR) ? gTR : NULL; }
