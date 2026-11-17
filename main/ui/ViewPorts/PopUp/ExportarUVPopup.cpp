// ============================================================================
//  ExportarUVPopup.cpp - el formulario de "Export UV Layout" (ver ExportarUVPopup.h). Misma mecanica que el de
//  "Nueva textura": filas navegables con el teclado, tap/click en cada campo, arrastre en los numericos y tap
//  para escribir el valor (con el teclado en pantalla en tactil). Coordenadas LOCALES al popup en el hit-test.
// ============================================================================
#include "w3dGraphics.h"
#include "app/W3dTeclado.h"   // W3dTecladoAbrir: el teclado del celular (y el layout que se adapta)
#include "W3dLang.h"
#include "ExportarUVPopup.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil
#include "ViewPorts/UVEditor.h"             // UVTexturaMostrada
#include "FileBrowser.h"                    // AbrirFileBrowser / W3dRutaEnCarpeta
#include "ConfirmarPopup.h"                 // sobrescribir un PNG que ya existe
#include "NumPad.h"
#include "io/UVExportar.h"
#include "io/w3dFilesystem.h"
#include "objects/Mesh.h"
#include "W3dAviso.h"                       // Notificar
#include "variables.h"                      // ObjActivo
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>                   // SDL_StartTextInput (teclado del celular para el nombre)
#endif

extern bool g_redraw;

static ExportarUVPopup* gEU = NULL;
static std::string gEUnombre, gEUcarpeta;
static float gEUancho = 512.0f, gEUalto = 512.0f;
static bool gEUconservar = false;   // reabrir tras elegir carpeta: no pisar lo tipeado con los defaults

static Mesh* EUMalla() { return (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL; }
static void EUGuardarCampos() {   // lo tipeado -> estado (sobrevive a cerrar/reabrir el formulario)
    if (!gEU) return;
    if (gEU->pNombre) gEUnombre = gEU->pNombre->field.text;
    if (gEU->pCarpeta) gEUcarpeta = gEU->pCarpeta->field.text;
}
static void EUcerrar() { if (gEU) gEU->Cerrar(); }
static void EUexportar();
static void EUcarpetaElegida(const std::string& elegido) {
    if (!elegido.empty()) {
        if (w3dFileSystem::IsDir(elegido)) gEUcarpeta = elegido;
        else {   // eligio un PNG: su carpeta + su nombre (sobrescribirlo)
            gEUcarpeta = w3dFileSystem::ParentPath(elegido);
            const size_t sl = elegido.find_last_of("/\\");
            gEUnombre = (sl == std::string::npos) ? elegido : elegido.substr(sl + 1);
        }
    }
    gEUconservar = true;
    AbrirExportarUVPopup();
}
static void EUelegirCarpeta() {
    EUGuardarCampos();
    EUcerrar();
    AbrirFileBrowser(T("Export UV Layout"), T("Use this folder"), ".png", EUcarpetaElegida, true);
}

ExportarUVPopup::ExportarUVPopup()
    : PopUpBase(T("Export UV Layout")), grupo(NULL), pNombre(NULL), pCarpeta(NULL), pAncho(NULL), pAlto(NULL),
      pBotones(NULL), dragField(NULL), dragMoved(false), lastDragMx(0) {
    grupo = new GroupPropertie(T("Export UV Layout"));
    grupo->anchoValores = 0.62f;
    grupo->flecha = false;   // ventana flotante: sin la flecha de plegar
    pNombre = new PropText(T("Name"), gEUnombre);
    grupo->properties.push_back(pNombre);
    pCarpeta = new PropText(T("Folder"), gEUcarpeta);
    pCarpeta->onClick = EUelegirCarpeta;   // la carpeta se elige con el explorador (no se tipea)
    grupo->properties.push_back(pCarpeta);
    pAncho = new PropFloat(T("Width"), "px");
    pAncho->value = &gEUancho; pAncho->entero = true; pAncho->centrado = true; pAncho->flechas = true;
    pAncho->stepFino = 1.0f; pAncho->stepGrueso = 1.0f; pAncho->dragStep = 1.0f; pAncho->SetRango(8.0f, 8192.0f);
    grupo->properties.push_back(pAncho);
    pAlto = new PropFloat(T("Height"), "px");
    pAlto->value = &gEUalto; pAlto->entero = true; pAlto->centrado = true; pAlto->flechas = true;
    pAlto->stepFino = 1.0f; pAlto->stepGrueso = 1.0f; pAlto->dragStep = 1.0f; pAlto->SetRango(8.0f, 8192.0f);
    grupo->properties.push_back(pAlto);
    pBotones = new PropButtonRow();
    pBotones->Agregar(T("Cancel"), EUcerrar);
    pBotones->Agregar(T("Export"), EUexportar);
    pBotones->activo = 1;
    grupo->properties.push_back(pBotones);
    grupo->selectIndex = 0;
    ResizeGrupo();
    Reubicar();
}
ExportarUVPopup::~ExportarUVPopup() { delete grupo; }

void ExportarUVPopup::ResizeGrupo() {
    int w = 260 * GlobalScale;
    const int maxW = MenuPantallaW - gapGS * 4;
    if (w > maxW) w = maxW;
    grupo->Resize(w, 0);
    popUpWindow->Resize(w, grupo->height);
}
void ExportarUVPopup::Reubicar() {   // centrado; con el teclado en pantalla abierto ENCIMA, el formulario sube
    x = (MenuPantallaW - popUpWindow->width) / 2;
    y = (MenuPantallaH - grupo->height) / 2;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + grupo->height + gapGS * 2)
        y = PopUpActive->y - grupo->height - gapGS * 2;
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}
int ExportarUVPopup::FilaY(int idx) const {
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)grupo->properties.size(); j++) yRow += grupo->properties[j]->Resize(grupo->width);
    return yRow;
}

void ExportarUVPopup::Render() {
    if (!grupo) return;
    ResizeGrupo(); Reubicar();
    initView();
    w3dEngine::PushMatrix();
    grupo->Render();
    w3dEngine::PopMatrix();
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    grupo->card->RenderBorder(false);
    endView();
}

bool ExportarUVPopup::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera -> el caller lo cierra (cancela)
    const int myL = my - y, lx = mx - x;
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
                // flechas: mitad / doble (las texturas van por potencias de 2); el valor exacto se tipea
                if (lx >= boxStart && lx < boxStart + aw) { pf->Set(*pf->value * 0.5f); g_redraw = true; return true; }
                if (lx >= boxStart + boxW - aw && lx < boxStart + boxW) { pf->Set(*pf->value * 2.0f); g_redraw = true; return true; }
                dragField = pf; dragMoved = false; lastDragMx = mx;
            } else if (tipo == PropertyType::Text) {
                g_textFieldActivo = NULL;
                const bool esCarpeta = (p == pCarpeta);
                p->EditPropertie();   // la carpeta abre el explorador (cierra este formulario); el nombre se edita
                if (esCarpeta) return true;
                #ifndef W3D_SYMBIAN
                if (ToolbarUsaTactil()) TecladoTextoAbrir();
                #endif
            } else if (tipo == PropertyType::ButtonRow) {
                // las celdas por geometria (los botones no guardan su rect al dibujarse dentro del popup)
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
    return true;   // adentro pero fuera de un campo: consumir
}
bool ExportarUVPopup::Motion(int mx, int my) {
    (void)my;
    if (dragField && dragField->value) {
        const int dmx = mx - lastDragMx; lastDragMx = mx;
        if (dmx != 0) dragMoved = true;
        dragField->Set(*dragField->value + dmx * dragField->dragStep);
        g_redraw = true;
    }
    return true;
}
bool ExportarUVPopup::Tecla(int tecla) {
    if (!grupo || grupo->properties.empty()) return true;
    const int n = (int)grupo->properties.size();
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
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value * (tecla == LayoutKey::Right ? 2.0f : 0.5f)); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->activo += (tecla == LayoutKey::Right) ? 1 : -1; row->ClampActivo(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[grupo->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ClampActivo();
                if (row->activo >= 0 && row->activo < (int)row->acciones.size() && row->acciones[row->activo]) row->acciones[row->activo]();
                return true;
            } else if (sel && sel->GetType() == PropertyType::Text) {
                sel->EditPropertie();
                if (sel == pCarpeta) return true;   // abrio el explorador
            } else { EUexportar(); return true; }   // OK sobre cualquier otro campo: exportar
            break;
        }
        case LayoutKey::Cancel: EUcerrar(); return true;
    }
    g_redraw = true;
    return true;
}
void ExportarUVPopup::Soltar() {
    if (dragField && !dragMoved && dragField->value) {
        dragField->IniciarEdicionTexto();            // mouse: se escribe directo en el campo
        if (ToolbarUsaTactil()) TecladoNumeroAbrir();       // tactil: ademas el teclado en pantalla (el formulario sube)
    }
    dragField = NULL;
}
bool ExportarUVPopup::Arrastrando() { return dragField != NULL; }
void ExportarUVPopup::Cerrar() {
    g_textFieldActivo = NULL;
    PopUpBase::Cerrar();
    g_redraw = true;
}

// el nombre con .png y la ruta final (carpeta + nombre)
static std::string EURuta() {
    std::string nombre = gEUnombre;
    const size_t a = nombre.find_first_not_of(" \t"); nombre = (a == std::string::npos) ? std::string() : nombre.substr(a);
    while (!nombre.empty() && (nombre[nombre.size() - 1] == ' ' || nombre[nombre.size() - 1] == '\t')) nombre.erase(nombre.size() - 1);
    if (nombre.empty()) return std::string();
    if (nombre.size() < 4 || nombre.substr(nombre.size() - 4) != ".png") nombre += ".png";
    std::string carpeta = gEUcarpeta.empty() ? w3dFileSystem::GetDefaultOutputDir() : gEUcarpeta;
    return W3dRutaEnCarpeta(carpeta, nombre, "");
}
bool ExportarUVAhora(std::string& rutaOut, std::string& msg) {
    EUGuardarCampos();
    rutaOut = EURuta();
    if (rutaOut.empty()) { msg = "Name"; return false; }
    Mesh* m = EUMalla();
    const int w = (int)(gEUancho + 0.5f), h = (int)(gEUalto + 0.5f);
    return UVExportarPNG(m, UVTexturaMostrada(m), w, h, rutaOut, msg);
}
static void EUhacerExport() {
    std::string ruta, msg;
    if (ExportarUVAhora(ruta, msg)) Notificar(std::string(T("UV layout saved")) + ": " + ruta, false);
    else                            Notificar(T(msg.c_str()), true);
    g_redraw = true;
}
static void EUexportar() {
    EUGuardarCampos();
    if (EURuta().empty()) { if (gEU && gEU->pNombre) gEU->pNombre->error = true; g_redraw = true; return; }
    if (!EUMalla() || !EUMalla()->uv) { Notificar(T("No UVs to export"), true); return; }
    const std::string ruta = EURuta();
    EUcerrar();
#ifndef W3D_SYMBIAN
    if (w3dFileSystem::FileExists(ruta)) {   // ya existe: preguntar antes de pisarlo
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir(std::string(T("The file already exists. Do you want to replace it?")) + "\n" + ruta, EUhacerExport);
        return;
    }
#endif
    EUhacerExport();
}

void AbrirExportarUVPopup() {
    if (!gEUconservar) {   // apertura normal: nombre y tamano de la textura mostrada (o los defaults)
        Mesh* m = EUMalla();
        Texture* t = UVTexturaMostrada(m);
        int w = 512, h = 512; UVExportarTamDefecto(t, w, h);
        gEUancho = (float)w; gEUalto = (float)h;
        gEUnombre = UVExportarNombreDefecto(m, t);
        if (gEUcarpeta.empty()) gEUcarpeta = w3dFileSystem::GetDefaultOutputDir();
    }
    gEUconservar = false;
    if (gEU) { if (PopUpActive == gEU) PopUpActive = NULL; delete gEU; gEU = NULL; }
    gEU = new ExportarUVPopup();
    PopUpActive = gEU;
    g_redraw = true;
}
