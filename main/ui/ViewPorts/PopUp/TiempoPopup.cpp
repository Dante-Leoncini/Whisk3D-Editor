// ============================================================================
//  TiempoPopup.cpp - el desplegable del RELOJ del timeline (ver TiempoPopup.h). Misma mecanica que los formularios
//  (Export UV, Nueva textura): filas navegables con el teclado, flechas -1 / +1 en cada numero, arrastre para
//  correrlo y un click (sin arrastrar) para escribirlo. Coordenadas LOCALES al popup en el hit-test.
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "TiempoPopup.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil
#include "NumPad.h"

extern bool g_redraw;

static TiempoPopup* gTP = NULL;

TiempoPopup::TiempoPopup(PropFloat* actual, PropFloat* inicio, PropFloat* fin, PropFloat* fps, int ax, int ay)
    : PopUpBase(T("Timing")), grupo(NULL), dragField(NULL), dragMoved(false), lastDragMx(0), anclaX(ax), anclaY(ay) {
    grupo = new GroupPropertie(T("Timing"));
    grupo->anchoValores = 0.6f;
    grupo->flecha = false;   // desplegable: sin la flecha de plegar
    PropFloat* ps[4] = { actual, inicio, fin, fps };   // el frame ACTUAL arriba de todo (es el que mas se toca)
    for (int i = 0; i < 4; i++) {
        if (!ps[i]) continue;
        ps[i]->centrado = true; ps[i]->flechas = true;
        ps[i]->stepFino = 1.0f; ps[i]->stepGrueso = 1.0f;
        if (ps[i]->dragStep <= 0.0f) ps[i]->dragStep = 0.25f;
        grupo->properties.push_back(ps[i]);
    }
    grupo->selectIndex = 0;
    int w = 150 * GlobalScale;
    if (w > MenuPantallaW - gapGS * 4) w = MenuPantallaW - gapGS * 4;
    grupo->Resize(w, 0);
    popUpWindow->Resize(w, grupo->height);
    Reubicar();
}
TiempoPopup::~TiempoPopup() {
    grupo->properties.clear();   // los PropFloat son del timeline: el grupo no los borra
    delete grupo;
}
void TiempoPopup::Reubicar() {   // debajo del reloj; si no entra, arriba, y siempre dentro de la pantalla
    x = anclaX; y = anclaY;
    if (y + grupo->height > MenuPantallaH - gapGS) y = anclaY - grupo->height - RenglonHeightGS;
    if (x + popUpWindow->width > MenuPantallaW - gapGS) x = MenuPantallaW - gapGS - popUpWindow->width;
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}

void TiempoPopup::Render() {
    if (!grupo) return;
    grupo->Resize(popUpWindow->width, 0);
    popUpWindow->Resize(popUpWindow->width, grupo->height);
    Reubicar();
    initView();
    w3dEngine::PushMatrix();
    grupo->Render();
    w3dEngine::PopMatrix();
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    grupo->card->RenderBorder(false);
    endView();
}

bool TiempoPopup::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera -> el caller lo cierra
    const int myL = my - y, lx = mx - x;
    const int boxStart = grupo->colEtiqueta + 2 * borderGS;
    const int boxW = grupo->propertiBox->width - bordersGS;
    const int aw = RenglonHeightGS;
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (size_t j = 0; j < grupo->properties.size(); j++) {
        PropertieBase* p = grupo->properties[j];
        const int h = p->Resize(grupo->width);
        if (p->Seleccionable() && myL >= yRow && myL < yRow + h && p->GetType() == PropertyType::Float) {
            grupo->selectIndex = (int)j;
            PropFloat* pf = static_cast<PropFloat*>(p);
            if (lx >= boxStart && lx < boxStart + aw)               { pf->Set(*pf->value - 1.0f); g_redraw = true; return true; }
            if (lx >= boxStart + boxW - aw && lx < boxStart + boxW) { pf->Set(*pf->value + 1.0f); g_redraw = true; return true; }
            dragField = pf; dragMoved = false; lastDragMx = mx;
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;   // adentro pero fuera de un campo: consumir
}
bool TiempoPopup::Motion(int mx, int my) {
    (void)my;
    if (dragField && dragField->value) {
        const int dmx = mx - lastDragMx;
        // de a frames ENTEROS: se acumula hasta juntar uno
        const float paso = dragField->dragStep > 0.0f ? dragField->dragStep : 0.25f;
        const int n = (int)((float)dmx * paso);
        if (n != 0) { lastDragMx = mx; dragMoved = true; dragField->Set(*dragField->value + (float)n); g_redraw = true; }
    }
    return true;
}
bool TiempoPopup::Tecla(int tecla) {
    if (!grupo || grupo->properties.empty()) return true;
    const int n = (int)grupo->properties.size();
    if (grupo->selectIndex < 0 || grupo->selectIndex >= n) grupo->selectIndex = 0;
    PropFloat* sel = static_cast<PropFloat*>(grupo->properties[grupo->selectIndex]);
    switch (tecla) {
        case LayoutKey::Up:    grupo->selectIndex = (grupo->selectIndex + n - 1) % n; break;
        case LayoutKey::Down:  grupo->selectIndex = (grupo->selectIndex + 1) % n; break;
        case LayoutKey::Left:  sel->Set(*sel->value - 1.0f); break;
        case LayoutKey::Right: sel->Set(*sel->value + 1.0f); break;
        case LayoutKey::Enter:
            sel->IniciarEdicionTexto();
            #ifndef W3D_SYMBIAN
            if (ToolbarUsaTactil()) NumPadAbrir();
            #endif
            break;
        case LayoutKey::Cancel: Cerrar(); return true;
    }
    g_redraw = true;
    return true;
}
void TiempoPopup::Soltar() {
    if (dragField && !dragMoved && dragField->value) {
        dragField->IniciarEdicionTexto();            // mouse: se escribe directo en el campo
        if (ToolbarUsaTactil()) NumPadAbrir();       // tactil: ademas el teclado en pantalla
    }
    dragField = NULL;
}
bool TiempoPopup::Arrastrando() { return dragField != NULL; }
void TiempoPopup::Cerrar() {
    PopUpBase::Cerrar();
    g_redraw = true;
}

void AbrirTiempoPopup(PropFloat* actual, PropFloat* inicio, PropFloat* fin, PropFloat* fps, int ax, int ay) {
    if (gTP) { if (PopUpActive == gTP) PopUpActive = NULL; delete gTP; gTP = NULL; }
    gTP = new TiempoPopup(actual, inicio, fin, fps, ax, ay);
    PopUpActive = gTP;
    g_redraw = true;
}
bool TiempoPopupAbierto() { return gTP && PopUpActive == gTP; }
// el centro (pantalla) del campo j del reloj abierto y su nombre: la MISMA cuenta de filas que Click. Lo usa el harness
// de capturas para filmar "Fin: 28" con el mouse o el dedo
bool TiempoPopupCampo(int j, int& cx, int& cy, std::string& nombre) {
    if (!TiempoPopupAbierto() || !gTP->grupo || j < 0 || j >= (int)gTP->grupo->properties.size()) return false;
    GroupPropertie* g = gTP->grupo;
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (int k = 0; k < j; k++) yRow += g->properties[k]->Resize(g->width);
    const int h = g->properties[j]->Resize(g->width);
    const int boxStart = g->colEtiqueta + 2 * borderGS, boxW = g->propertiBox->width - bordersGS;
    cx = gTP->x + boxStart + boxW / 2; cy = gTP->y + yRow + h / 2;
    nombre = g->properties[j]->name;
    return true;
}
