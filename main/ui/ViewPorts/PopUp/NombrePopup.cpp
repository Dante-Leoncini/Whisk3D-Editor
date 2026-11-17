// ============================================================================
//  NombrePopup.cpp - la ventanita que pide un nombre (ver el .h). Misma mecanica que "Export UV Layout": una tarjeta
//  con sus filas, coordenadas LOCALES al popup en el hit-test.
// ============================================================================
#include "w3dGraphics.h"
#include "ViewPorts/PopUp/NumPad.h"   // TecladoTextoAbrir / TecladoNumeroAbrir: el teclado en pantalla (Ajustes)
#include "app/W3dTeclado.h"   // W3dTecladoAbrir: el teclado del celular
#include "W3dLang.h"
#include "NombrePopup.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil

extern bool g_redraw;
NombrePopup* nombrePopup = NULL;

static void NPcancelar() { if (nombrePopup) nombrePopup->Cerrar(); }
static void NPaceptar() { if (nombrePopup) nombrePopup->Aceptar(); }

NombrePopup::NombrePopup(const std::string& titulo, const std::string& boton, const std::string& inicial,
                         void (*aceptar)(const std::string&))
    : PopUpBase(titulo), grupo(NULL), pNombre(NULL), pBotones(NULL), alAceptar(aceptar) {
    grupo = new GroupPropertie(titulo);
    grupo->anchoValores = 0.66f;
    grupo->flecha = false;   // ventana flotante: sin la flecha de plegar
    pNombre = new PropText(T("Name"), inicial);
    grupo->properties.push_back(pNombre);
    pBotones = new PropButtonRow();
    pBotones->Agregar(T("Cancel"), NPcancelar);
    pBotones->Agregar(boton, NPaceptar);
    pBotones->activo = 1;
    grupo->properties.push_back(pBotones);
    grupo->selectIndex = 0;
    Reubicar();
}
NombrePopup::~NombrePopup() { delete grupo; }

void NombrePopup::Reubicar() {   // al medio; con el teclado en pantalla abierto ENCIMA, sube
    int w = 240 * GlobalScale;
    const int maxW = MenuPantallaW - gapGS * 4;
    if (w > maxW) w = maxW;
    grupo->Resize(w, 0);
    popUpWindow->Resize(w, grupo->height);
    x = (MenuPantallaW - popUpWindow->width) / 2;
    y = (MenuPantallaH - grupo->height) / 3;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + grupo->height + gapGS * 2)
        y = PopUpActive->y - grupo->height - gapGS * 2;
    if (x < gapGS) x = gapGS;
    if (y < gapGS) y = gapGS;
}

void NombrePopup::Render() {
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

bool NombrePopup::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera -> el caller lo cierra (cancela)
    const int myL = my - y, lx = mx - x;
    const int boxStart = grupo->colEtiqueta + 2 * borderGS;
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (size_t j = 0; j < grupo->properties.size(); j++) {
        PropertieBase* p = grupo->properties[j];
        const int h = p->Resize(grupo->width);
        if (p->Seleccionable() && myL >= yRow && myL < yRow + h) {
            grupo->selectIndex = (int)j;
            if (p == pNombre) {
                pNombre->EditPropertie();
                if (ToolbarUsaTactil()) TecladoTextoAbrir();
            } else if (p == pBotones) {
                // las celdas por geometria (los botones no guardan su rect al dibujarse dentro del popup)
                const int cw = pBotones->AnchoCelda(grupo->width), x0 = boxStart - grupo->colEtiqueta;
                int vis = 0;
                for (size_t b = 0; b < pBotones->botones.size(); b++) {
                    if (!pBotones->botones[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lx >= cx && lx < cx + cw) {
                        pBotones->activo = (int)b;
                        if (pBotones->acciones[b]) pBotones->acciones[b]();
                        return true;
                    }
                    vis++;
                }
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;   // adentro pero fuera de un campo: consumir
}

bool NombrePopup::Tecla(int tecla) {
    switch (tecla) {
        case LayoutKey::Left:
        case LayoutKey::Right:
            pBotones->activo += (tecla == LayoutKey::Right) ? 1 : -1;
            pBotones->ClampActivo();
            break;
        case LayoutKey::Up: grupo->selectIndex = 0; break;
        case LayoutKey::Down: grupo->selectIndex = 1; break;
        case LayoutKey::Enter:
        case LayoutKey::Accept:
            if (grupo->selectIndex == 1 && pBotones->activo == 0) Cerrar();
            else Aceptar();
            return true;
        case LayoutKey::Cancel: Cerrar(); return true;
    }
    g_redraw = true;
    return true;
}

bool NombrePopup::TeclaEnTexto(bool enter) {
    if (g_textFieldActivo != &pNombre->field) return false;
    if (enter) Aceptar(); else Cerrar();
    return true;
}

void NombrePopup::Aceptar() {
    std::string n = pNombre->field.text;
    const size_t a = n.find_first_not_of(" \t");
    n = (a == std::string::npos) ? std::string() : n.substr(a);
    while (!n.empty() && (n[n.size() - 1] == ' ' || n[n.size() - 1] == '\t')) n.erase(n.size() - 1);
    if (n.empty()) { pNombre->error = true; g_redraw = true; return; }
    void (*cb)(const std::string&) = alAceptar;
    Cerrar();
    if (cb) cb(n);
}

void NombrePopup::Cerrar() {
    g_textFieldActivo = NULL;
    PopUpBase::Cerrar();
    g_redraw = true;
}

void AbrirNombrePopup(const std::string& titulo, const std::string& boton, const std::string& inicial,
                      void (*aceptar)(const std::string& nombre)) {
    if (nombrePopup) { if (PopUpActive == nombrePopup) PopUpActive = NULL; delete nombrePopup; nombrePopup = NULL; }
    nombrePopup = new NombrePopup(titulo, boton, inicial, aceptar);
    if (PopUpActive && PopUpActive != nombrePopup) PopUpActive->Cerrar();
    PopUpActive = nombrePopup;
    // el campo ya enfocado: se escribe directo (todo elegido: tipear reemplaza el nombre que se propone)
    TextFieldEnfocar(&nombrePopup->pNombre->field);
    nombrePopup->pNombre->field.SelectAll();
    if (ToolbarUsaTactil()) TecladoTextoAbrir();
    g_redraw = true;
}
