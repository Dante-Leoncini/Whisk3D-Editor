// ============================================================================
//  TimelineRedoPanel.cpp - "Ajustar la ultima operacion" del timeline (ver TimelineRedoPanel.h). C++03.
//  La interaccion (arrastrar un valor, tocarlo para tipearlo, desplazarse, plegar) es la de TransformRedoPanel; lo que
//  cambia es que se hizo y como se re-hace (TimelineRedoLeer / TimelineRedoReaplicar, en Timeline.cpp).
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "TimelineRedoPanel.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/Timeline.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil
#include "NumPad.h"
#include "Undo.h"
#include <math.h>

extern bool g_redraw;

static TimelineRedoPanel* gTL = NULL;
static TimelineRedoOp gTLop;
static float gTLv[2] = { 0, 0 };   // los dos valores editables (mover: frames/valor; rotar: angulo; escalar: X/Y)

bool TimelineRedoPanelAbierto() { return gTL && PopUpActive == gTL; }
void TimelineRedoCerrarPanel() { if (TimelineRedoPanelAbierto()) gTL->Cerrar(); }
static void HookUndo() { if (!TimelineRedoReaplicando()) { TimelineRedoOlvidar(); TimelineRedoCerrarPanel(); } }   // Ctrl+Z / Ctrl+Y

static void ValoresAOp() {
    if (gTLop.modo == 0) { gTLop.frames = gTLv[0]; gTLop.valor = gTLv[1]; }
    else if (gTLop.modo == 1) gTLop.angulo = gTLv[0];
    else { gTLop.escX = gTLv[0]; gTLop.escY = gTLop.curvas ? gTLv[1] : 1.0f; }
}
static void OnValor() { ValoresAOp(); TimelineRedoReaplicar(gTLop); }
static void Cancelar_() { TimelineRedoCancelar(); TimelineRedoCerrarPanel(); g_redraw = true; }
static void Aceptar_() { TimelineRedoOlvidar(); TimelineRedoCerrarPanel(); }

void TimelineRedoAbrir() {
    if (!TimelineRedoLeer(gTLop)) return;
    if (gTLop.modo == 0) { gTLv[0] = gTLop.frames; gTLv[1] = gTLop.valor; }
    else if (gTLop.modo == 1) { gTLv[0] = gTLop.angulo; gTLv[1] = 0.0f; }
    else { gTLv[0] = gTLop.escX; gTLv[1] = gTLop.escY; }
    g_undoCambioHook = HookUndo;
    if (gTL) { if (PopUpActive == gTL) PopUpActive = NULL; delete gTL; gTL = NULL; }
    gTL = new TimelineRedoPanel();
    PopUpActive = gTL;
    g_redraw = true;
}

static const char* NombrePivote(int p) {
    switch (p) { case 1: return "Current Frame"; case 2: return "Median Point"; case 3: return "Active Keyframe"; default: return "Center"; }
}

TimelineRedoPanel::TimelineRedoPanel()
    : PopUpBase(""), grupo(NULL), dragField(NULL), dragMoved(false), lastDragMx(0), marco(NULL) {
    marco = new Card();
    Armar();
}
TimelineRedoPanel::~TimelineRedoPanel() { delete grupo; delete marco; }

void TimelineRedoPanel::Armar() {
    // nace PLEGADO (solo la flecha y el titulo, como la del 3D): no tapa el timeline si no se lo necesita
    const bool abierto = grupo ? grupo->open : false;
    delete grupo;
    const char* tit = (gTLop.modo == 1) ? "Rotate" : (gTLop.modo == 2) ? "Scale" : "Move";
    grupo = new GroupPropertie(T(tit));
    grupo->anchoValores = 0.55f;
    grupo->flecha = true;
    grupo->open = abierto;
    if (gTLop.modo == 0) {
        PropFloat* f = new PropFloat(T("Frames"));
        f->value = &gTLv[0]; f->SetRango(-1e6f, 1e6f); f->stepFino = 1.0f; f->stepGrueso = 10.0f; f->dragStep = 0.2f;
        f->onChange = OnValor;
        grupo->properties.push_back(f);
        if (gTLop.curvas) {
            PropFloat* v = new PropFloat(T("Value"));
            v->value = &gTLv[1]; v->SetRango(-1e6f, 1e6f); v->stepFino = 0.1f; v->stepGrueso = 1.0f; v->dragStep = 0.01f;
            v->onChange = OnValor;
            grupo->properties.push_back(v);
        }
    } else if (gTLop.modo == 1) {
        PropFloat* a = new PropFloat(T("Angle"), "\xC2\xB0");
        a->value = &gTLv[0]; a->SetRango(-36000.0f, 36000.0f); a->stepFino = 1.0f; a->stepGrueso = 15.0f; a->dragStep = 0.5f;
        a->onChange = OnValor;
        grupo->properties.push_back(a);
    } else {
        // X = el TIEMPO, Y = el VALOR (solo en curvas: en el dope sheet no hay eje de valor)
        for (int k = 0; k < (gTLop.curvas ? 2 : 1); k++) {
            PropFloat* f = new PropFloat(k == 0 ? "X" : "Y");
            f->value = &gTLv[k]; f->SetRango(-1e4f, 1e4f); f->stepFino = 0.05f; f->stepGrueso = 0.5f; f->dragStep = 0.005f;
            f->onChange = OnValor;
            grupo->properties.push_back(f);
        }
    }
    PropButton* p = new PropButton(T("Pivot"));
    p->conLabel = true; p->gris = true; p->button->text = T(NombrePivote(gTLop.pivote));
    grupo->properties.push_back(p);
    PropButtonRow* b = new PropButtonRow();
    b->Agregar(T("Cancel"), Cancelar_);
    b->Agregar(T("Accept"), Aceptar_);
    b->activo = 1;
    grupo->properties.push_back(b);
    grupo->selectIndex = 0;
    ResizeGrupo();
    Reubicar();
}
void TimelineRedoPanel::ResizeGrupo() {
    Timeline* tl = TimelineRedoVista();
    int w = 200 * GlobalScale;
    if (tl && w > tl->width - gapGS * 4) w = tl->width - gapGS * 4;
    if (w < 120 * GlobalScale) w = 120 * GlobalScale;
    grupo->Resize(w, 0);
    dz.Medir(grupo->height);
    popUpWindow->Resize(w, dz.visible);
}
void TimelineRedoPanel::Reubicar() {   // abajo a la izquierda del timeline, encima de su barra de herramientas
    const int alto = dz.visible;
    Timeline* tl = TimelineRedoVista();
    if (!tl) { x = gapGS * 2; y = MenuPantallaH - alto - gapGS * 2; return; }
    x = tl->x + gapGS * 2;
    y = tl->y + tl->height - TimelineToolbarAlto(tl) - alto - gapGS * 2;
    if (y < gapGS) y = gapGS;   // (un timeline mas bajo que la tarjeta: sube por encima de el)
    if (x < gapGS) x = gapGS;
}
int TimelineRedoPanel::FilaY(int idx) const {
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)grupo->properties.size(); j++) yRow += grupo->properties[j]->Resize(grupo->width);
    return yRow;
}

void TimelineRedoPanel::Render() {
    if (!grupo) return;
    ResizeGrupo(); Reubicar();
    initView();
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)(-dz.scroll), 0);
    grupo->Render();
    w3dEngine::Translatef(0, (GLfloat)dz.scroll, 0);
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    marco->Resize(popUpWindow->width, dz.visible); marco->RenderBorder(false);
    if (dz.Desplazable()) {
        const int alto = dz.visible * dz.visible / (dz.total > 0 ? dz.total : 1);
        const int ty = (dz.total - dz.visible > 0) ? (dz.visible - alto) * dz.scroll / (dz.total - dz.visible) : 0;
        w3dEngine::Translatef((GLfloat)(popUpWindow->width - 3 * GlobalScale), (GLfloat)ty, 0);
        SetColorID(ColorID::accent, 0.7f);
        marco->Resize(2 * GlobalScale, alto); marco->RenderObject(false);
    }
    w3dEngine::PopMatrix();
    endView();
}

bool TimelineRedoPanel::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera: el panel se cierra (acepta) y el click sigue al timeline
    const int myL = my - y + dz.scroll;
    if (myL < borderGS + RenglonHeightGS + gapGS) { grupo->open = !grupo->open; dz.scroll = 0; g_redraw = true; return true; }
    if (!grupo->open) return true;
    dz.Abajo(mx, my);
    if (!dz.Desplazable()) { dz.presionado = false; ClickFila(mx, my, false); return true; }
    ClickFila(mx, my, true);
    return true;
}
void TimelineRedoPanel::ClickFila(int mx, int my, bool soloArmar) {
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
            if (tipo == PropertyType::ButtonRow) {
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
bool TimelineRedoPanel::Motion(int mx, int my) {
    if (!dragMoved && dz.Mover(mx, my)) { dragField = NULL; g_redraw = true; return true; }
    if (dz.desplazando) return true;
    if (dragField && dragField->value) {
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
void TimelineRedoPanel::Soltar() {
    const bool toque = dz.Arriba();
    if (dragField) {
        if (!dragMoved && dragField->value) {
            dragField->IniciarEdicionTexto();            // mouse: se escribe el valor exacto
            if (ToolbarUsaTactil()) NumPadAbrir();       // tactil: el teclado numerico en pantalla
        }
        dragField = NULL;
        return;
    }
    if (toque && dz.Desplazable()) ClickFila(dz.pressX, dz.pressY, false);
}
void TimelineRedoPanel::Wheel(int delta) { dz.Rueda(delta); g_redraw = true; }
bool TimelineRedoPanel::Arrastrando() { return dragField != NULL || dz.presionado; }
bool TimelineRedoPanel::Tecla(int tecla) {
    if (!grupo) return true;
    if (!grupo->open) {
        if (tecla == LayoutKey::Enter || tecla == LayoutKey::Right) { grupo->open = true; g_redraw = true; return true; }
        if (tecla == LayoutKey::Cancel) { Aceptar_(); return true; }
        return false;   // plegada: las demas teclas siguen de largo (se sigue trabajando)
    }
    const int n = (int)grupo->properties.size();
    switch (tecla) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (tecla == LayoutKey::Down) ? +1 : -1;
            int s = grupo->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (grupo->properties[s]->Seleccionable()) break; }
            grupo->selectIndex = s;
            dz.AVista(FilaY(s), grupo->properties[(size_t)s]->Resize(grupo->width));
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[grupo->selectIndex] : NULL;
            if (!sel) break;
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value + (tecla == LayoutKey::Right ? pf->stepFino : -pf->stepFino)); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->activo += (tecla == LayoutKey::Right) ? 1 : -1; row->ClampActivo(); }
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
            break;
        }
        case LayoutKey::Cancel: Aceptar_(); return true;   // Esc: cierra aceptando (solo Cancelar deshace)
    }
    g_redraw = true;
    return true;
}
void TimelineRedoPanel::Cerrar() {
    g_textFieldActivo = NULL;
    PopUpBase::Cerrar();
    g_redraw = true;
}

// (harness)
bool TimelineRedoFijar(int campo, float valor) {
    if (!TimelineRedoPanelAbierto() || campo < 0 || campo > 1) return false;
    gTLv[campo] = valor;
    ValoresAOp();
    return TimelineRedoReaplicar(gTLop);
}
void TimelineRedoBoton(bool aceptar) { if (aceptar) Aceptar_(); else Cancelar_(); }
