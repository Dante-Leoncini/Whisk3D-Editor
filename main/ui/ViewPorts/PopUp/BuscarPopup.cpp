// ============================================================================
//  BuscarPopup.cpp - Buscar / Reemplazar del editor de scripts (ver BuscarPopup.h). Coordenadas LOCALES al panel en
//  el hit-test, con el desplazamiento de PanelDesplazable cuando no entra en la mitad de la pantalla.
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "BuscarPopup.h"
#include "ViewPorts/IDE.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropLabel.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/widgets/TextField.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil
#include "W3dAviso.h"                       // Notificar
#ifndef W3D_SYMBIAN
#include "app/W3dTeclado.h"                 // W3dTecladoAbrir
#endif
#include <stdio.h>

extern bool g_redraw;

static BuscarPopup* gBP = NULL;
static std::string gTexto, gTextoPrevio, gReemplazo;
static bool gMayus = false, gPalabra = false;
static int gAnclaLin = 0, gAnclaCol = 0;   // desde donde se busca mientras se tipea (el cursor al abrir)
static IdeBusqueda gActiva;

static std::string Num(int n) { char b[16]; snprintf(b, sizeof(b), "%d", n); return b; }

static void AccAnterior()  { if (gBP && gBP->ide) { gBP->Sincronizar(); gBP->ide->IrACoincidencia(gTexto, gMayus, gPalabra, -1); } }
static void AccSiguiente() { if (gBP && gBP->ide) { gBP->Sincronizar(); gBP->ide->IrACoincidencia(gTexto, gMayus, gPalabra, +1); } }
static void AccReemplazar() {
    if (!gBP || !gBP->ide || gTexto.empty()) return;
    gBP->Sincronizar();
    IDE* ide = gBP->ide;
    // la seleccion ES una coincidencia: se reemplaza (un paso de deshacer) y se pasa a la siguiente; si no, primero se va a una
    if (ide->SeleccionEsCoincidencia(gTexto, gMayus)) ide->InsertarTexto(gReemplazo);
    ide->IrACoincidencia(gTexto, gMayus, gPalabra, +1);
}
static void AccReemplazarTodo() {
    if (!gBP || !gBP->ide || gTexto.empty()) return;
    gBP->Sincronizar();
    const int n = gBP->ide->ReemplazarTodas(gTexto, gReemplazo, gMayus, gPalabra);
    Notificar(Num(n) + " " + T("replacements"), false);
}
static void AccCerrar() { if (gBP) gBP->Cerrar(); }

BuscarPopup::BuscarPopup(IDE* ide_, bool reemplazar_)
    : PopUpBase(""), ide(ide_), reemplazar(reemplazar_), grupo(NULL), pBuscar(NULL), pReemplazo(NULL), pCuenta(NULL), pNav(NULL),
      barra(NULL), buscarEnfocado(false) {
    barra = new Card();
    Armar();
}
BuscarPopup::~BuscarPopup() { delete grupo; delete barra; }

void BuscarPopup::Armar() {
    delete grupo;
    grupo = new GroupPropertie(T(reemplazar ? "Find and Replace" : "Find"));
    grupo->anchoValores = 0.62f;
    grupo->flecha = false;
    pBuscar = new PropText(T("Find"), gTexto);
    grupo->properties.push_back(pBuscar);
    pCuenta = new PropLabel(" ");
    grupo->properties.push_back(pCuenta);
    PropBool* m = new PropBool(T("Match case"));
    m->value = &gMayus;
    grupo->properties.push_back(m);
    PropBool* w = new PropBool(T("Whole word"));
    w->value = &gPalabra;
    grupo->properties.push_back(w);
    pNav = new PropButtonRow();
    pNav->Agregar(T("Previous"), AccAnterior);
    pNav->Agregar(T("Next"), AccSiguiente);
    pNav->activo = 1;
    grupo->properties.push_back(pNav);
    pReemplazo = NULL;
    if (reemplazar) {
        pReemplazo = new PropText(T("Replace"), gReemplazo);
        grupo->properties.push_back(pReemplazo);
        PropButtonRow* r = new PropButtonRow();
        r->Agregar(T("Replace"), AccReemplazar);
        r->Agregar(T("Replace all"), AccReemplazarTodo);
        grupo->properties.push_back(r);
    }
    PropButtonRow* c = new PropButtonRow();
    c->Agregar(T("Close"), AccCerrar);
    grupo->properties.push_back(c);
    grupo->selectIndex = 0;
    Reubicar();
}

// arriba a la derecha del IDE, debajo de su barra (donde lo esperan los editores de codigo)
void BuscarPopup::Reubicar() {
    int w = 240 * GlobalScale;
    if (ide && w > ide->width - gapGS * 4) w = ide->width - gapGS * 4;
    if (w < 140 * GlobalScale) w = 140 * GlobalScale;
    grupo->Resize(w, 0);
    dz.Medir(grupo->height);
    popUpWindow->Resize(w, dz.visible);
    if (ide) {
        x = ide->x + ide->width - w - gapGS * 2;
        y = ide->y + ide->BarTopOffset() + gapGS;
    } else { x = MenuPantallaW - w - gapGS * 2; y = gapGS * 2; }
    if (x < gapGS) x = gapGS;
    if (y + dz.visible > MenuPantallaH - gapGS) y = MenuPantallaH - gapGS - dz.visible;
    if (y < gapGS) y = gapGS;
}
int BuscarPopup::FilaY(int idx) const {
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)grupo->properties.size(); j++) yRow += grupo->properties[j]->Resize(grupo->width);
    return yRow;
}

void BuscarPopup::Sincronizar() {
    if (!ide) return;
    gTexto = pBuscar->field.text;
    if (pReemplazo) gReemplazo = pReemplazo->field.text;
    // mientras se tipea: la primera coincidencia desde donde estaba el cursor al abrir
    if (gTexto != gTextoPrevio) {
        gTextoPrevio = gTexto;
        ide->CursorA(gAnclaLin, gAnclaCol, false);
        if (!gTexto.empty()) ide->IrACoincidencia(gTexto, gMayus, gPalabra, +1);
    }
    // el campo Buscar solto el foco (Enter): el proximo Enter es "Siguiente"
    const bool enfocado = (g_textFieldActivo == &pBuscar->field);
    if (buscarEnfocado && !enfocado) {
        for (size_t j = 0; j < grupo->properties.size(); j++) if (grupo->properties[j] == pNav) grupo->selectIndex = (int)j;
        pNav->activo = 1;
    }
    buscarEnfocado = enfocado;
    // "3 de 7"
    std::vector<int> L, C;
    ide->Coincidencias(gTexto, gMayus, gPalabra, &L, &C);
    std::string cuenta;
    if (gTexto.empty()) cuenta = " ";
    else if (L.empty()) cuenta = T("No results");
    else {
        int actual = 0;
        if (ide->haySel) {
            int a, b, c, d; ide->NormalizarSel(&a, &b, &c, &d);
            for (size_t i = 0; i < L.size(); i++) if (L[i] == a && C[i] == b) actual = (int)i + 1;
        }
        cuenta = actual ? Num(actual) + " " + T("of") + " " + Num((int)L.size()) : Num((int)L.size()) + " " + T("results");
    }
    pCuenta->name = cuenta;
    gActiva.ide = ide; gActiva.texto = gTexto; gActiva.mayus = gMayus; gActiva.palabra = gPalabra;
}

void BuscarPopup::Render() {
    if (!grupo) return;
    Sincronizar();
    Reubicar();
    initView();
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)(-dz.scroll), 0);
    grupo->Render();
    w3dEngine::PopMatrix();
    // borde VERDE: es la ventana con el foco
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    popUpWindow->RenderBorder(false);
    if (dz.Desplazable()) {   // la barrita que dice cuanto falta
        const int alto = dz.visible * dz.visible / (dz.total > 0 ? dz.total : 1);
        const int ty = (dz.total - dz.visible > 0) ? (dz.visible - alto) * dz.scroll / (dz.total - dz.visible) : 0;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(popUpWindow->width - 3 * GlobalScale), (GLfloat)ty, 0);
        SetColorID(ColorID::accent, 0.7f);
        barra->Resize(2 * GlobalScale, alto); barra->RenderObject(false);
        w3dEngine::PopMatrix();
    }
    endView();
}

void BuscarPopup::ClickEnFila(int mx, int my) {
    const int myL = my - y + dz.scroll, lx = mx - x;
    const int boxStart = grupo->colEtiqueta + 2 * borderGS;
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (size_t j = 0; j < grupo->properties.size(); j++) {
        PropertieBase* p = grupo->properties[j];
        const int h = p->Resize(grupo->width);
        if (p->Seleccionable() && myL >= yRow && myL < yRow + h) {
            grupo->selectIndex = (int)j;
            const PropertyType::Enum tipo = p->GetType();
            if (tipo == PropertyType::Text) {
                g_textFieldActivo = NULL;
                p->EditPropertie();
#ifndef W3D_SYMBIAN
                if (ToolbarUsaTactil()) W3dTecladoAbrir();
#endif
            } else if (tipo == PropertyType::Bool || tipo == PropertyType::Button) {
                g_textFieldActivo = NULL;
                p->EditPropertie();
            } else if (tipo == PropertyType::ButtonRow) {
                g_textFieldActivo = NULL;
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
            }
            g_redraw = true;
            return;
        }
        yRow += h;
    }
}
bool BuscarPopup::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera: NO se cierra (no es modal); el click sigue al IDE
    dz.Abajo(mx, my);
    if (!dz.Desplazable()) { dz.presionado = false; ClickEnFila(mx, my); }   // entra entero: click directo
    return true;
}
bool BuscarPopup::Motion(int mx, int my) { if (dz.Mover(mx, my)) g_redraw = true; return true; }
void BuscarPopup::Soltar() { if (dz.Arriba()) ClickEnFila(dz.pressX, dz.pressY); }
bool BuscarPopup::Arrastrando() { return dz.presionado; }
void BuscarPopup::Wheel(int delta) { dz.Rueda(delta); g_redraw = true; }

bool BuscarPopup::Tecla(int tecla) {
    if (!grupo || grupo->properties.empty()) return true;
    const int n = (int)grupo->properties.size();
    PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[(size_t)grupo->selectIndex] : NULL;
    switch (tecla) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (tecla == LayoutKey::Down) ? +1 : -1;
            int s = grupo->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (grupo->properties[(size_t)s]->Seleccionable()) break; }
            grupo->selectIndex = s;
            dz.AVista(FilaY(s), grupo->properties[(size_t)s]->Resize(grupo->width));
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right:
            if (sel && sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->activo += (tecla == LayoutKey::Right) ? 1 : -1; row->ClampActivo(); }
            else if (sel && sel->GetType() == PropertyType::Bool) sel->EditPropertie();
            break;
        case LayoutKey::Enter:
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ClampActivo();
                if (row->activo >= 0 && row->activo < (int)row->acciones.size() && row->acciones[(size_t)row->activo]) row->acciones[(size_t)row->activo]();
                return true;
            }
            if (sel) { g_textFieldActivo = NULL; sel->EditPropertie(); }   // OK: edita el campo SIN cerrar el panel
            break;
        case LayoutKey::Cancel: Cerrar(); return true;   // Esc / Supr / C: se cierra
        default: break;
    }
    g_redraw = true;
    return true;
}
void BuscarPopup::Cerrar() {
    if (g_textFieldActivo == &pBuscar->field || (pReemplazo && g_textFieldActivo == &pReemplazo->field)) g_textFieldActivo = NULL;
    PopUpBase::Cerrar();
    g_redraw = true;
}

void IDEAbrirBuscar(IDE* ide, bool reemplazar) {
    if (!ide) return;
    // lo seleccionado (si es de una linea) arranca como texto a buscar, como en los editores de codigo
    if (ide->haySel && ide->selLin == ide->curLin) gTexto = ide->TextoSeleccionado();
    int a = ide->curLin, b = ide->curCol;
    if (ide->haySel) { int c, d; ide->NormalizarSel(&a, &b, &c, &d); }
    gAnclaLin = a; gAnclaCol = b;
    gTextoPrevio = gTexto;   // abrir no mueve el cursor: recien cambia al tipear
    if (gBP) { if (PopUpActive == gBP) PopUpActive = NULL; delete gBP; gBP = NULL; }
    gBP = new BuscarPopup(ide, reemplazar);
    PopUpActive = gBP;
    // el foco al campo Buscar, con todo seleccionado: lo que se tipea lo reemplaza
    gBP->pBuscar->EditPropertie();
    gBP->pBuscar->field.SelectAll();
    gBP->buscarEnfocado = true;
#ifndef W3D_SYMBIAN
    if (ToolbarUsaTactil()) W3dTecladoAbrir();
#endif
    g_redraw = true;
}
bool IDEBuscarAbierto() { return gBP && PopUpActive == gBP; }
const IdeBusqueda* IDEBusquedaActiva() {
    if (!IDEBuscarAbierto() || gActiva.texto.empty()) return NULL;
    return &gActiva;
}
void IDEBuscarFijar(const std::string& texto, const std::string& reemplazo, int mayus, int palabra) {
    if (!gBP) return;
    if (mayus >= 0) gMayus = (mayus != 0);
    if (palabra >= 0) gPalabra = (palabra != 0);
    gBP->pBuscar->field.SetText(texto);
    if (gBP->pReemplazo) gBP->pReemplazo->field.SetText(reemplazo);
    g_textFieldActivo = NULL;
    gBP->Sincronizar();
}
void IDEBuscarBoton(int cual) {
    switch (cual) {
        case 0: AccAnterior(); break;
        case 1: AccSiguiente(); break;
        case 2: AccReemplazar(); break;
        case 3: AccReemplazarTodo(); break;
        case 4: AccCerrar(); break;
    }
    if (gBP && PopUpActive == gBP) gBP->Sincronizar();
}
std::string IDEBuscarCuenta() { return (gBP && gBP->pCuenta) ? gBP->pCuenta->name : std::string(); }
