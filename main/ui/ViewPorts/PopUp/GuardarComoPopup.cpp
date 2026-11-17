// ============================================================================
//  GuardarComoPopup.cpp - el formulario de "Save As..." (ver GuardarComoPopup.h). Misma mecanica que los demas
//  formularios flotantes (Nueva textura, Exportar mapa UV): filas navegables con el teclado, tap/click en cada campo,
//  coordenadas LOCALES al popup en el hit-test. La lista de carpetas del proyecto se dibuja encima, pegada a la fila.
// ============================================================================
#include "w3dGraphics.h"
#include "ViewPorts/PopUp/NumPad.h"   // TecladoTextoAbrir / TecladoNumeroAbrir: el teclado en pantalla (Ajustes)
#include "W3dLang.h"
#include "GuardarComoPopup.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil
#include "FileBrowser.h"                    // AbrirFileBrowser
#include "ConfirmarPopup.h"
#include "io/W3dContenedor.h"               // W3dContenedorHayMontado
#include "io/RecursosProyecto.h"            // W3dCarpetasTodas
#include "io/w3dFilesystem.h"
#include "W3dAviso.h"                       // Notificar
#include "app/W3dTeclado.h"                 // W3dTecladoAbrir

extern bool g_redraw;

static GuardarComoPopup* gGC = NULL;
static std::string gGCtitulo, gGCnombre, gGCext, gGCcarpetaProy, gGCcarpetaDisco;
static bool gGCinterno = true;
static GuardarComoAccion gGCaccion = NULL;
static std::vector<std::string> gGCcarpetas;   // "" = la raiz + las de la biblioteca
static GuardarComoDestino gGCpendiente;        // lo que espera la confirmacion de sobrescribir

static void GCcerrar() { if (gGC) gGC->Cerrar(); }
static void GCguardar();
static void GCreabrir();
static std::string GCetiquetaCarpeta() {
    if (gGCinterno) return gGCcarpetaProy.empty() ? std::string(T("(root)")) : gGCcarpetaProy;
    return gGCcarpetaDisco.empty() ? w3dFileSystem::GetDefaultOutputDir() : gGCcarpetaDisco;
}
static void GCleerCampos() { if (gGC && gGC->pNombre) gGCnombre = gGC->pNombre->field.text; }
static void GCcarpetaDiscoElegida(const std::string& elegido) {
    if (!elegido.empty()) {
        if (w3dFileSystem::IsDir(elegido)) gGCcarpetaDisco = elegido;
        else {   // eligio un archivo: su carpeta + su nombre (pisarlo)
            gGCcarpetaDisco = w3dFileSystem::ParentPath(elegido);
            const size_t sl = elegido.find_last_of("/\\");
            gGCnombre = (sl == std::string::npos) ? elegido : elegido.substr(sl + 1);
        }
    }
    GCreabrir();
}
static void GCtocarCarpeta() {
    if (!gGC) return;
    if (gGCinterno) { gGC->listaAbierta = !gGC->listaAbierta; gGC->listaScroll = 0; g_redraw = true; return; }
    GCleerCampos();
    GCcerrar();
    AbrirFileBrowser(gGCtitulo, T("Use this folder"), gGCext, GCcarpetaDiscoElegida, true);
}
static void GCinternoCambio() {
    if (gGCinterno && !W3dContenedorHayMontado()) {   // sin proyecto guardado no hay .w3d donde meterlo
        gGCinterno = false;
        Notificar(T("Save the project first"), true);
    }
    if (gGC) { gGC->listaAbierta = false; gGC->SincronizarCarpeta(); }
    g_redraw = true;
}

GuardarComoPopup::GuardarComoPopup()
    : PopUpBase(gGCtitulo), grupo(NULL), pNombre(NULL), pInterno(NULL), pCarpeta(NULL), pBotones(NULL),
      listaAbierta(false), listaSel(0), listaScroll(0), lista(NULL) {
    grupo = new GroupPropertie(gGCtitulo);
    grupo->anchoValores = 0.62f;
    grupo->flecha = false;
    pNombre = new PropText(T("Name"), gGCnombre);
    grupo->properties.push_back(pNombre);
    pInterno = new PropBool(T("Inside the .w3d"));
    pInterno->value = &gGCinterno; pInterno->onChange = GCinternoCambio;
    grupo->properties.push_back(pInterno);
    pCarpeta = new PropButton(T("Folder"));
    pCarpeta->conLabel = true; pCarpeta->action = GCtocarCarpeta;
    grupo->properties.push_back(pCarpeta);
    pBotones = new PropButtonRow();
    pBotones->Agregar(T("Cancel"), GCcerrar);
    pBotones->Agregar(T("Save"), GCguardar);
    pBotones->activo = 1;
    grupo->properties.push_back(pBotones);
    grupo->selectIndex = 0;
    lista = new Card();
    SincronizarCarpeta();
    ResizeGrupo();
    Reubicar();
}
GuardarComoPopup::~GuardarComoPopup() { delete grupo; delete lista; }

void GuardarComoPopup::SincronizarCarpeta() { if (pCarpeta) pCarpeta->button->text = GCetiquetaCarpeta(); }
int GuardarComoPopup::ListaVisibles() const {
    const int n = (int)gGCcarpetas.size();
    const int maxFilas = (MenuPantallaH / 2) / (RenglonHeightGS + gapGS);
    int v = n < 8 ? n : 8; if (v > maxFilas) v = maxFilas; if (v < 1) v = 1;
    return v;
}
void GuardarComoPopup::ResizeGrupo() {
    int w = 270 * GlobalScale;
    const int maxW = MenuPantallaW - gapGS * 4;
    if (w > maxW) w = maxW;
    grupo->Resize(w, 0);
    int alto = grupo->height;
    if (listaAbierta) { int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh); if (ly + lh + borderGS > alto) alto = ly + lh + borderGS; }
    popUpWindow->Resize(w, alto);
}
void GuardarComoPopup::Reubicar() {
    x = (MenuPantallaW - popUpWindow->width) / 2;
    y = (MenuPantallaH - grupo->height) / 2;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + grupo->height + gapGS * 2)
        y = PopUpActive->y - grupo->height - gapGS * 2;   // el teclado en pantalla abierto encima: el formulario sube
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}
int GuardarComoPopup::FilaY(int idx) const {
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)grupo->properties.size(); j++) yRow += grupo->properties[j]->Resize(grupo->width);
    return yRow;
}
void GuardarComoPopup::ListaRect(int& lx, int& ly, int& lw, int& lh) const {
    int idx = 0; for (size_t j = 0; j < grupo->properties.size(); j++) if (grupo->properties[j] == pCarpeta) idx = (int)j;
    lx = grupo->colEtiqueta + 2 * borderGS;
    lw = grupo->propertiBox->width - bordersGS;
    ly = FilaY(idx) + RenglonHeightGS + gapGS;
    lh = ListaVisibles() * (RenglonHeightGS + gapGS) + gapGS;
}

void GuardarComoPopup::Render() {
    if (!grupo) return;
    SincronizarCarpeta();
    ResizeGrupo(); Reubicar();
    initView();
    w3dEngine::PushMatrix();
    grupo->Render();
    w3dEngine::PopMatrix();
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    grupo->card->RenderBorder(false);
    if (listaAbierta) {
        int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
        SetColorID(ColorID::headerColor, 1.0f);
        lista->Resize(lw, lh); lista->RenderObject(false);
        w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
        lista->RenderBorder(false);
        const int vis = ListaVisibles();
        for (int r = 0; r < vis; r++) {
            const int i = listaScroll + r;
            if (i >= (int)gGCcarpetas.size()) break;
            const int ry = gapGS + r * (RenglonHeightGS + gapGS);
            if (i == listaSel) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)borderGS, (GLfloat)ry, 0);
                SetColorID(ColorID::accent, 0.35f);
                lista->Resize(lw - bordersGS, RenglonHeightGS); lista->RenderObject(false);
                w3dEngine::PopMatrix();
            }
            SetColorID(gGCcarpetas[(size_t)i] == gGCcarpetaProy ? ColorID::accent : ColorID::blanco, 1.0f);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry + (RenglonHeightGS - LetterHeightGS) / 2), 0);
            RenderBitmapText(gGCcarpetas[(size_t)i].empty() ? std::string(T("(root)")) : gGCcarpetas[(size_t)i], textAlign::left, lw - bordersGS - gapGS);
            w3dEngine::PopMatrix();
        }
        w3dEngine::PopMatrix();
    }
    endView();
}

bool GuardarComoPopup::Click(int mx, int my) {
    const int myL = my - y, lx = mx - x;
    if (listaAbierta) {
        int ax, ay, aw, ah; ListaRect(ax, ay, aw, ah);
        listaAbierta = false;
        if (lx >= ax && lx < ax + aw && myL >= ay && myL < ay + ah) {
            const int i = listaScroll + (myL - ay - gapGS) / (RenglonHeightGS + gapGS);
            if (i >= 0 && i < (int)gGCcarpetas.size()) gGCcarpetaProy = gGCcarpetas[(size_t)i];
            SincronizarCarpeta();
            g_redraw = true;
            return true;
        }
        g_redraw = true;
    }
    if (!Contains(mx, my)) return false;
    const int boxStart = grupo->colEtiqueta + 2 * borderGS;
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (size_t j = 0; j < grupo->properties.size(); j++) {
        PropertieBase* p = grupo->properties[j];
        const int h = p->Resize(grupo->width);
        if (p->Seleccionable() && myL >= yRow && myL < yRow + h) {
            grupo->selectIndex = (int)j;
            const PropertyType::Enum tipo = p->GetType();
            if (tipo != PropertyType::Text) g_textFieldActivo = NULL;
            if (tipo == PropertyType::Text) {
                g_textFieldActivo = NULL;
                p->EditPropertie();
                #ifndef W3D_SYMBIAN
                if (ToolbarUsaTactil()) TecladoTextoAbrir();
                #endif
            } else if (tipo == PropertyType::Bool || tipo == PropertyType::Button) {
                GCleerCampos();
                p->EditPropertie();   // el check llama a GCinternoCambio; la carpeta abre la lista o el explorador
                if (p == pCarpeta) return true;
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
                return true;
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;
}
void GuardarComoPopup::Wheel(int delta) {
    if (!listaAbierta) return;
    const int maxS = (int)gGCcarpetas.size() - ListaVisibles();
    listaScroll += (delta > 0) ? -1 : 1;
    if (listaScroll > maxS) listaScroll = maxS; if (listaScroll < 0) listaScroll = 0;
    g_redraw = true;
}
bool GuardarComoPopup::Tecla(int tecla) {
    if (!grupo || grupo->properties.empty()) return true;
    const int n = (int)grupo->properties.size();
    if (listaAbierta) {
        const int nc = (int)gGCcarpetas.size();
        switch (tecla) {
            case LayoutKey::Up:   if (listaSel > 0) listaSel--; break;
            case LayoutKey::Down: if (listaSel + 1 < nc) listaSel++; break;
            case LayoutKey::Enter: if (listaSel >= 0 && listaSel < nc) gGCcarpetaProy = gGCcarpetas[(size_t)listaSel]; listaAbierta = false; SincronizarCarpeta(); break;
            case LayoutKey::Cancel: listaAbierta = false; break;
            default: break;
        }
        if (listaSel < listaScroll) listaScroll = listaSel;
        if (listaSel >= listaScroll + ListaVisibles()) listaScroll = listaSel - ListaVisibles() + 1;
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
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[grupo->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::Bool) { GCleerCampos(); sel->EditPropertie(); }
            else if (sel && sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->activo += (tecla == LayoutKey::Right) ? 1 : -1; row->ClampActivo(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[grupo->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ClampActivo();
                if (row->activo >= 0 && row->activo < (int)row->acciones.size() && row->acciones[row->activo]) row->acciones[row->activo]();
                return true;
            } else if (sel && (sel->GetType() == PropertyType::Text || sel->GetType() == PropertyType::Bool || sel->GetType() == PropertyType::Button)) {
                GCleerCampos();
                sel->EditPropertie();
                if (sel == pCarpeta) { listaSel = 0; for (size_t i = 0; i < gGCcarpetas.size(); i++) if (gGCcarpetas[i] == gGCcarpetaProy) listaSel = (int)i; return true; }
            } else { GCguardar(); return true; }
            break;
        }
        case LayoutKey::Cancel: GCcerrar(); return true;
    }
    g_redraw = true;
    return true;
}
void GuardarComoPopup::Cerrar() {
    listaAbierta = false;
    g_textFieldActivo = NULL;
    PopUpBase::Cerrar();
    g_redraw = true;
}

static GuardarComoDestino GCdestino() {
    GuardarComoDestino d;
    std::string nombre = gGCnombre;
    const size_t a = nombre.find_first_not_of(" \t"); nombre = (a == std::string::npos) ? std::string() : nombre.substr(a);
    while (!nombre.empty() && (nombre[nombre.size() - 1] == ' ' || nombre[nombre.size() - 1] == '\t')) nombre.erase(nombre.size() - 1);
    const size_t sl = nombre.find_last_of("/\\"); if (sl != std::string::npos) nombre = nombre.substr(sl + 1);
    if (!nombre.empty() && !gGCext.empty() &&
        (nombre.size() < gGCext.size() || nombre.substr(nombre.size() - gGCext.size()) != gGCext)) nombre += gGCext;
    d.nombre = nombre;
    d.interno = gGCinterno;
    d.carpetaProyecto = gGCcarpetaProy;
    d.carpetaDisco = gGCcarpetaDisco.empty() ? w3dFileSystem::GetDefaultOutputDir() : gGCcarpetaDisco;
    return d;
}
static void GCconfirmadoPisar() {
    std::string msg; bool existe = false;
    if (gGCaccion && !gGCaccion(gGCpendiente, true, &existe, msg) && !msg.empty()) Notificar(T(msg.c_str()), true);
    g_redraw = true;
}
bool GuardarComoAhora(bool forzar, std::string& msg) {
    GCleerCampos();
    GuardarComoDestino d = GCdestino();
    if (d.nombre.empty()) { msg = "Name"; return false; }
    if (!gGCaccion) { msg = "Nothing to save"; return false; }
    bool existe = false;
    if (gGCaccion(d, forzar, &existe, msg)) { GCcerrar(); return true; }
    if (existe) msg = "The file already exists. Do you want to replace it?";
    return false;
}
bool GuardarComoAbierto() { return gGC && PopUpActive == gGC; }
void GuardarComoFijar(const std::string& nombre, int interno, const std::string& carpetaProy, const std::string& carpetaDisco) {
    gGCnombre = nombre;
    if (gGC && gGC->pNombre) gGC->pNombre->field.SetText(nombre);
    if (interno >= 0) { gGCinterno = (interno != 0); GCinternoCambio(); }
    gGCcarpetaProy = carpetaProy;
    if (!carpetaDisco.empty()) gGCcarpetaDisco = carpetaDisco;
}
static void GCguardar() {
    GCleerCampos();
    GuardarComoDestino d = GCdestino();
    if (d.nombre.empty()) { if (gGC && gGC->pNombre) gGC->pNombre->error = true; g_redraw = true; return; }
    if (!gGCaccion) { GCcerrar(); return; }
    std::string msg; bool existe = false;
    if (gGCaccion(d, false, &existe, msg)) { GCcerrar(); return; }
    if (existe) {   // ya hay un archivo con ese nombre ahi: preguntar antes de pisarlo
        gGCpendiente = d;
        GCcerrar();
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir(std::string(T("The file already exists. Do you want to replace it?")) + "\n" + d.nombre, GCconfirmadoPisar);
        return;
    }
    if (!msg.empty()) Notificar(T(msg.c_str()), true);
}
static void GCreabrir() {
    gGCcarpetas.clear();
    gGCcarpetas.push_back(std::string());
    std::vector<std::string> cs; W3dCarpetasTodas(cs);
    for (size_t i = 0; i < cs.size(); i++) if (!cs[i].empty()) gGCcarpetas.push_back(cs[i]);
    if (gGC) { if (PopUpActive == gGC) PopUpActive = NULL; delete gGC; gGC = NULL; }
    gGC = new GuardarComoPopup();
    PopUpActive = gGC;
    g_redraw = true;
}
void AbrirGuardarComoPopup(const std::string& titulo, const std::string& nombre, const std::string& ext, bool interno,
                           const std::string& carpetaProyecto, GuardarComoAccion accion) {
    gGCtitulo = titulo; gGCnombre = nombre; gGCext = ext;
    gGCinterno = interno && W3dContenedorHayMontado();
    gGCcarpetaProy = carpetaProyecto;
    gGCaccion = accion;
    GCreabrir();
}
