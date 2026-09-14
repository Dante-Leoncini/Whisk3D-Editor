// ============================================================================
//  NuevaTexturaPopup.cpp - el formulario de "Nueva textura" (ver NuevaTexturaPopup.h).
//  Misma tarjeta de propiedades que el panel de Add (RedoMeshPanel): filas navegables con el
//  teclado, tap/click en cada campo, arrastre en los numericos y tap para escribir el valor.
//  OJO con las coordenadas: el popup dibuja con su propio viewport (initView), asi que los
//  botones guardan sx/sy LOCALES al popup; el hit-test se hace con (mx - x, my - y).
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "NuevaTexturaPopup.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropColor.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil
#include "ColorPicker.h"
#include "NumPad.h"
#include "io/TexturaGenerada.h"
#include "W3dAviso.h"                       // Notificar
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>                   // SDL_StartTextInput (teclado del celular para el nombre)
#endif

extern bool g_redraw;

static NuevaTexturaPopup* gNT = NULL;
static float gNTancho = 256.0f, gNTalto = 256.0f;
static float gNTcolor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
static bool  gNTalpha = true;
static int   gNTtipo = TexGenUVGrid;

static void NTcerrar() { if (gNT) gNT->Cerrar(); }
static void NTcancelar() { NTcerrar(); }
static void NTcrear();
static void NTabrirLista() { if (gNT) gNT->AbrirLista(); }
static void NTsetTipo(int tipo) {
    if (tipo < 0 || tipo >= TexGenTipos) return;
    gNTtipo = tipo;
    if (gNT && gNT->pTipo) gNT->pTipo->button->text = T(TexGenTipoNombre(gNTtipo));
    g_redraw = true;
}

NuevaTexturaPopup::NuevaTexturaPopup()
    : PopUpBase(T("New Texture")), grupo(NULL), pNombre(NULL), pAncho(NULL), pAlto(NULL), pColor(NULL), pAlpha(NULL),
      pTipo(NULL), pBotones(NULL), dragField(NULL), dragMoved(false), lastDragMx(0),
      pickerAbierto(false), listaAbierta(false), listaSel(0), lista(NULL) {
    grupo = new GroupPropertie(T("New Texture"));
    grupo->anchoValores = 0.55f;
    grupo->flecha = false;   // ventana flotante: sin la flecha de plegar
    pNombre = new PropText(T("Name"), "Textura");
    grupo->properties.push_back(pNombre);
    pAncho = new PropFloat(T("Width"), "px");
    pAncho->value = &gNTancho; pAncho->entero = true; pAncho->centrado = true; pAncho->flechas = true;
    pAncho->stepFino = 1.0f; pAncho->stepGrueso = 1.0f; pAncho->dragStep = 1.0f; pAncho->SetRango(8.0f, 4096.0f);
    grupo->properties.push_back(pAncho);
    pAlto = new PropFloat(T("Height"), "px");
    pAlto->value = &gNTalto; pAlto->entero = true; pAlto->centrado = true; pAlto->flechas = true;
    pAlto->stepFino = 1.0f; pAlto->stepGrueso = 1.0f; pAlto->dragStep = 1.0f; pAlto->SetRango(8.0f, 4096.0f);
    grupo->properties.push_back(pAlto);
    pColor = new PropColor(T("Color"));
    pColor->value = gNTcolor;
    grupo->properties.push_back(pColor);
    pAlpha = new PropBool(T("Alpha"));
    pAlpha->value = &gNTalpha;
    grupo->properties.push_back(pAlpha);
    pTipo = new PropButton(T("Type"));
    pTipo->conLabel = true; pTipo->button->text = T(TexGenTipoNombre(gNTtipo)); pTipo->action = NTabrirLista;
    grupo->properties.push_back(pTipo);
    pBotones = new PropButtonRow();
    pBotones->Agregar(T("Cancel"), NTcancelar);
    pBotones->Agregar(T("Create"), NTcrear);
    pBotones->activo = 1;
    grupo->properties.push_back(pBotones);
    grupo->selectIndex = 0;
    lista = new Card();
    ResizeGrupo();
    Reubicar();
}
NuevaTexturaPopup::~NuevaTexturaPopup() { delete grupo; delete lista; }

void NuevaTexturaPopup::ResizeGrupo() {
    int w = 240 * GlobalScale;
    int maxW = MenuPantallaW - gapGS * 4;
    if (w > maxW) w = maxW;
    grupo->Resize(w, 0);
    int alto = grupo->height;
    if (listaAbierta) {   // la ventana (y su scissor) tiene que abarcar el desplegable, que cuelga por debajo de la tarjeta
        int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh);
        if (ly + lh + borderGS > alto) alto = ly + lh + borderGS;
    }
    popUpWindow->Resize(w, alto);
}
void NuevaTexturaPopup::Reubicar() {   // centrado en la pantalla (por la tarjeta: el desplegable no la mueve)
    x = (MenuPantallaW - popUpWindow->width) / 2;
    y = (MenuPantallaH - grupo->height) / 2;
    // con el teclado numerico (u otro popup) abierto ENCIMA, el formulario se corre hacia arriba para no quedar tapado
    if (PopUpActive && PopUpActive != this && PopUpActive != colorPicker && PopUpActive->y < y + grupo->height + gapGS * 2)
        y = PopUpActive->y - grupo->height - gapGS * 2;
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}
// y local del renglon idx (misma cuenta que el hit-test: titulo + filas anteriores)
int NuevaTexturaPopup::FilaY(int idx) const {
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)grupo->properties.size(); j++) yRow += grupo->properties[j]->Resize(grupo->width);
    return yRow;
}
// el desplegable del tipo: pegado debajo de la fila Type, del ancho de la caja de valores
void NuevaTexturaPopup::ListaRect(int& lx, int& ly, int& lw, int& lh) const {
    int idx = 0; for (size_t j = 0; j < grupo->properties.size(); j++) if (grupo->properties[j] == pTipo) idx = (int)j;
    lx = grupo->colEtiqueta + 2 * borderGS;
    lw = grupo->propertiBox->width - bordersGS;
    ly = FilaY(idx) + RenglonHeightGS + gapGS;
    lh = TexGenTipos * (RenglonHeightGS + gapGS) + gapGS;
}
void NuevaTexturaPopup::AbrirLista() { listaAbierta = !listaAbierta; listaSel = gNTtipo; g_redraw = true; }

void NuevaTexturaPopup::Render() {
    if (!grupo) return;
    ResizeGrupo(); Reubicar();
    initView();
    w3dEngine::PushMatrix();
    grupo->Render();
    w3dEngine::PopMatrix();
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    grupo->card->RenderBorder(false);
    if (listaAbierta) {   // el desplegable del tipo, encima de lo que haya debajo
        int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
        SetColorID(ColorID::headerColor, 1.0f);
        lista->Resize(lw, lh); lista->RenderObject(false);
        w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
        lista->RenderBorder(false);
        for (int t = 0; t < TexGenTipos; t++) {
            const int ry = gapGS + t * (RenglonHeightGS + gapGS);
            if (t == listaSel) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)borderGS, (GLfloat)ry, 0);
                SetColorID(ColorID::accent, 0.35f);
                lista->Resize(lw - bordersGS, RenglonHeightGS); lista->RenderObject(false);
                w3dEngine::PopMatrix();
            }
            SetColorID((t == gNTtipo) ? ColorID::accent : ColorID::blanco, 1.0f);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry + (RenglonHeightGS - LetterHeightGS) / 2), 0);
            RenderBitmapText(T(TexGenTipoNombre(t)), textAlign::left, lw - bordersGS - gapGS);
            w3dEngine::PopMatrix();
        }
        w3dEngine::PopMatrix();
    }
    endView();
    if (pickerAbierto && colorPicker) colorPicker->Render();   // el selector de color, encima del formulario
}

bool NuevaTexturaPopup::Click(int mx, int my) {
    // el selector de color abierto se queda con TODOS los clicks: adentro los maneja el, afuera lo cierra
    if (pickerAbierto && colorPicker) {
        if (!colorPicker->Click(mx, my)) colorPicker->Cerrar();
        if (!colorPicker->target) pickerAbierto = false;   // Cerrar() suelta el target: ya no esta
        g_redraw = true;
        return true;
    }
    const int myL = my - y, lx = mx - x;
    if (listaAbierta) {   // el desplegable del tipo: elegir o cerrarlo
        int ax, ay, aw, ah; ListaRect(ax, ay, aw, ah);
        listaAbierta = false;
        if (lx >= ax && lx < ax + aw && myL >= ay && myL < ay + ah) {
            const int t = (myL - ay - gapGS) / (RenglonHeightGS + gapGS);
            NTsetTipo(t);
            return true;
        }
        g_redraw = true;
    }
    if (!Contains(mx, my)) return false;   // afuera -> el caller lo cierra (cancela)
    const int boxStart = grupo->colEtiqueta + 2 * borderGS;
    const int boxW = grupo->propertiBox->width - bordersGS;
    const int aw = RenglonHeightGS;
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (size_t j = 0; j < grupo->properties.size(); j++) {
        PropertieBase* p = grupo->properties[j];
        const int h = p->Resize(grupo->width);
        if (p->Seleccionable() && myL >= yRow && myL < yRow + h) {
            grupo->selectIndex = (int)j;
            const PropertyType::Enum tipo = p->GetType();
            if (tipo != PropertyType::Text) g_textFieldActivo = NULL;   // otro control: el nombre deja de editarse (queda lo escrito)
            if (tipo == PropertyType::Float) {
                PropFloat* pf = static_cast<PropFloat*>(p);
                // las flechas del tamano van por POTENCIAS DE 2 (8, 16, ... 4096): lo que se usa en texturas
                if (lx >= boxStart && lx < boxStart + aw) { pf->Set(*pf->value * 0.5f); g_redraw = true; return true; }
                if (lx >= boxStart + boxW - aw && lx < boxStart + boxW) { pf->Set(*pf->value * 2.0f); g_redraw = true; return true; }
                dragField = pf; dragMoved = false; lastDragMx = mx;
            } else if (tipo == PropertyType::Bool || tipo == PropertyType::Button) {
                p->EditPropertie();   // el boton "Type" abre el desplegable (NTabrirLista)
            } else if (tipo == PropertyType::Text) {
                g_textFieldActivo = NULL;
                p->EditPropertie();
                #ifndef W3D_SYMBIAN
                if (ToolbarUsaTactil()) SDL_StartTextInput();
                #endif
            } else if (tipo == PropertyType::Color) {
                if (!colorPicker) colorPicker = new ColorPicker();
                // a la derecha del formulario si entra, sino encima. Abrir() lo hace popup activo: se
                // vuelve a poner ESTE, que le reenvia los eventos mientras esta abierto
                int px = x + popUpWindow->width + gapGS * 2;
                if (px + 200 * GlobalScale > MenuPantallaW) px = x + gapGS * 4;
                colorPicker->Abrir(gNTcolor, px, y);
                PopUpActive = this;
                pickerAbierto = true;
            } else if (tipo == PropertyType::ButtonRow) {
                // los botones no guardan su rect al dibujarse dentro del popup: se calcula la celda por geometria
                // (misma cuenta que PropButtonRow::RenderPropertiBox: arrancan en el borde izquierdo del cuerpo)
                PropButtonRow* row = static_cast<PropButtonRow*>(p);
                const int cw = row->AnchoCelda(grupo->width), x0 = boxStart - grupo->colEtiqueta;
                int celda = -1; int vis = 0;
                for (size_t b = 0; b < row->botones.size(); b++) {
                    if (!row->botones[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lx >= cx && lx < cx + cw) { celda = (int)b; break; }
                    vis++;
                }
                if (celda >= 0) { row->activo = celda; if (row->acciones[(size_t)celda]) row->acciones[(size_t)celda](); }
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;   // adentro pero fuera de un campo: consumir
}
bool NuevaTexturaPopup::Motion(int mx, int my) {
    if (pickerAbierto && colorPicker) { colorPicker->Motion(mx, my); return true; }
    if (dragField && dragField->value) {
        const int dmx = mx - lastDragMx; lastDragMx = mx;
        if (dmx != 0) dragMoved = true;
        dragField->Set(*dragField->value + dmx * dragField->dragStep);
        g_redraw = true;
    }
    return true;
}
bool NuevaTexturaPopup::Tecla(int tecla) {
    if (pickerAbierto && colorPicker) {
        colorPicker->Tecla(tecla);
        if (!colorPicker->target) pickerAbierto = false;
        g_redraw = true;
        return true;
    }
    if (!grupo || grupo->properties.empty()) return true;
    const int n = (int)grupo->properties.size();
    if (listaAbierta) {   // navegar el desplegable del tipo
        switch (tecla) {
            case LayoutKey::Up:   listaSel = (listaSel + TexGenTipos - 1) % TexGenTipos; break;
            case LayoutKey::Down: listaSel = (listaSel + 1) % TexGenTipos; break;
            case LayoutKey::Enter: NTsetTipo(listaSel); listaAbierta = false; break;
            case LayoutKey::Cancel: listaAbierta = false; break;
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
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            if (grupo->selectIndex < 0) grupo->selectIndex = 0;
            PropertieBase* sel = (grupo->selectIndex < n) ? grupo->properties[grupo->selectIndex] : NULL;
            if (!sel) break;
            const PropertyType::Enum tipo = sel->GetType();
            if (tipo == PropertyType::Bool) sel->EditPropertie();
            else if (tipo == PropertyType::Button) NTsetTipo((gNTtipo + (tecla == LayoutKey::Right ? 1 : TexGenTipos - 1)) % TexGenTipos);
            else if (tipo == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value * (tecla == LayoutKey::Right ? 2.0f : 0.5f)); }
            else if (tipo == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->activo += (tecla == LayoutKey::Right) ? 1 : -1; row->ClampActivo(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[grupo->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ClampActivo();
                if (row->activo >= 0 && row->activo < (int)row->acciones.size() && row->acciones[row->activo]) row->acciones[row->activo]();
            } else if (sel && (sel->GetType() == PropertyType::Text || sel->GetType() == PropertyType::Bool || sel->GetType() == PropertyType::Button)) {
                sel->EditPropertie();
            } else if (sel && sel->GetType() == PropertyType::Color) {
                Click(x + grupo->colEtiqueta + 3 * borderGS, y + FilaY(grupo->selectIndex) + 1);   // como un click en la fila: abre el selector
            } else NTcrear();   // OK sobre cualquier otro campo: crear
            break;
        }
        case LayoutKey::Cancel: NTcerrar(); break;
    }
    g_redraw = true;
    return true;
}
bool NuevaTexturaPopup::TeclaRepeat(int tecla) {
    if (pickerAbierto && colorPicker) return colorPicker->TeclaRepeat(tecla);
    return true;
}
void NuevaTexturaPopup::Wheel(int delta) { if (pickerAbierto && colorPicker) colorPicker->Wheel(delta); }
void NuevaTexturaPopup::Soltar() {
    if (pickerAbierto && colorPicker) { colorPicker->Soltar(); return; }
    if (dragField && !dragMoved && dragField->value) {
        dragField->IniciarEdicionTexto();            // mouse: se escribe directo en el campo
        if (ToolbarUsaTactil()) NumPadAbrir();       // tactil: ademas el teclado en pantalla (se abre abajo; el popup sube)
    }
    dragField = NULL;
}
bool NuevaTexturaPopup::Arrastrando() { return (pickerAbierto && colorPicker) ? colorPicker->Arrastrando() : (dragField != NULL); }
void NuevaTexturaPopup::Cerrar() {
    if (pickerAbierto && colorPicker) { colorPicker->Cerrar(); pickerAbierto = false; }
    listaAbierta = false;
    g_textFieldActivo = NULL;
    PopUpBase::Cerrar();
    g_redraw = true;
}

static void NTcrear() {
    if (!gNT) return;
    std::string nombre = gNT->pNombre ? gNT->pNombre->field.text : std::string("Textura");
    if (nombre.find_first_not_of(" \t") == std::string::npos) { if (gNT->pNombre) gNT->pNombre->error = true; g_redraw = true; return; }
    const int w = (int)(gNTancho + 0.5f), h = (int)(gNTalto + 0.5f);
    std::string msg;
    const std::string ruta = W3dCrearTexturaProyecto(nombre, gNTtipo, w, h, gNTcolor, gNTalpha, msg);
    if (ruta.empty()) { Notificar(T(msg.c_str()), true); return; }
    Notificar(std::string(T("New Texture")) + ": " + nombre, false);
    NTcerrar();
}

void AbrirNuevaTexturaPopup() {
    if (gNT) { if (PopUpActive == gNT) PopUpActive = NULL; delete gNT; gNT = NULL; }
    gNT = new NuevaTexturaPopup();
    PopUpActive = gNT;
    g_redraw = true;
}
