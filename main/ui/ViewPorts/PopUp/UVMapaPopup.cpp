// ============================================================================
//  UVMapaPopup.cpp - la capa UV de un mapa del material (ver UVMapaPopup.h). Mismo esquema que NuevaTexturaPopup:
//  las coordenadas del hit-test son LOCALES al popup (mx - x, my - y).
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "UVMapaPopup.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsaTactil
#include "NumPad.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "undo/Undo.h"
#include "io/CambiosProyecto.h"             // W3dCambiosTocado

extern bool g_redraw;
extern void RebindMaterialMeshPart();
extern void RTInvalidar();

static UVMapaPopup* gUVP = NULL;
static Material* gUVmat = NULL;
static Mesh* gUVmesh = NULL;
static int gUVmapa = 0;
// lo que se edita (se aplica recien con Accept)
static float gUVidxMat = 1.0f;   // 1.. (la UI cuenta desde 1)
static bool  gUVusarMat = true;
static int   gUVsel = -1;        // -1 = "Use index"; j = la capa UV j de la malla
static float gUVidxObj = 1.0f;

// ---- aplicar (con undo, en UN paso) ----
struct UndoUVObj { Mesh* m; std::string mat; int mapa; std::string nom; int idx; };   // lo de ANTES (ida y vuelta)
static void UndoUVObjAplicar(void* d) {
    UndoUVObj* u = (UndoUVObj*)d;
    if (!u->m) return;
    Material* mt = BuscarMaterialPorNombre(u->mat);
    Material tmp(u->mat, true);
    const Material* c = mt ? mt : &tmp;
    const std::string nom = u->m->UVPersonalizado(c, u->mapa); const int idx = u->m->UVPersonalIndice(c, u->mapa);
    u->m->FijarUVPersonalizado(c, u->mapa, u->nom); u->m->FijarUVPersonalIndice(c, u->mapa, u->idx);
    u->nom = nom; u->idx = idx;
    RebindMaterialMeshPart();
    RTInvalidar();
    g_redraw = true;
}
static void UndoUVObjDesvincular(void* d, Object* borrado) { UndoUVObj* u = (UndoUVObj*)d; if ((Object*)u->m == borrado) u->m = NULL; }
static void UndoUVObjLiberar(void* d) { delete (UndoUVObj*)d; }

void UVMapaPopupAplicar(Material* mat, Mesh* m, int mapa, int indiceMat, bool usarMat, const std::string& uvNombre, int indiceObj) {
    if (!mat || mapa < 0 || mapa >= W3D_MAPAS) return;
    UndoGrupoIniciar();
    // el INDICE del material (compartido)
    const int im = indiceMat < 1 ? 0 : indiceMat - 1;
    if (mat->uvMapa[mapa] != im) {
        UndoMaterialModIniciar(mat);
        mat->uvMapa[mapa] = im;
        UndoMaterialModCommit();
    }
    // lo de ESTE objeto: nada (usa el del material), un indice propio o una capa UV por nombre
    if (m) {
        const std::string nom = usarMat ? std::string() : uvNombre;
        const int idx = (usarMat || !uvNombre.empty()) ? -1 : (indiceObj < 1 ? 0 : indiceObj - 1);
        const std::string nomAntes = m->UVPersonalizado(mat, mapa);
        const int idxAntes = m->UVPersonalIndice(mat, mapa);
        if (nom != nomAntes || idx != idxAntes) {
            UndoUVObj* u = new UndoUVObj();
            u->m = m; u->mat = mat->name; u->mapa = mapa; u->nom = nomAntes; u->idx = idxAntes;
            m->FijarUVPersonalizado(mat, mapa, nom);
            m->FijarUVPersonalIndice(mat, mapa, idx);
            UndoExterno f; f.aplicar = UndoUVObjAplicar; f.desvincular = UndoUVObjDesvincular; f.liberar = UndoUVObjLiberar;
            UndoPushExterno(f, u);
            W3dCambiosTocado();
        }
    }
    UndoGrupoFinSecuencial();
    RTInvalidar();
    RebindMaterialMeshPart();
    g_redraw = true;
}

static void UVPcerrar() { if (gUVP) gUVP->Cerrar(); }
static void UVPaceptar() {
    std::string nom;
    if (!gUVusarMat && gUVsel >= 0 && gUVmesh && gUVsel < (int)gUVmesh->uvMaps.size() && gUVmesh->uvMaps[(size_t)gUVsel])
        nom = gUVmesh->uvMaps[(size_t)gUVsel]->nombre;
    UVMapaPopupAplicar(gUVmat, gUVmesh, gUVmapa, (int)(gUVidxMat + 0.5f), gUVusarMat, nom, (int)(gUVidxObj + 0.5f));
    UVPcerrar();
}
static void UVPabrirLista() { if (gUVP && !gUVusarMat) { gUVP->listaAbierta = !gUVP->listaAbierta; gUVP->listaScroll = 0; g_redraw = true; } }
static std::string NombreSel() {
    if (gUVsel < 0 || !gUVmesh || gUVsel >= (int)gUVmesh->uvMaps.size() || !gUVmesh->uvMaps[(size_t)gUVsel]) return T("Use index");
    return gUVmesh->uvMaps[(size_t)gUVsel]->nombre;
}

UVMapaPopup::UVMapaPopup(const std::string& titulo)
    : PopUpBase(T("UV Map")), grupo(NULL), pIdxMat(NULL), pUsarMat(NULL), pUV(NULL), pIdxObj(NULL), pBotones(NULL),
      dragField(NULL), dragMoved(false), lastDragMx(0), listaAbierta(false), listaScroll(0), lista(NULL) {
    grupo = new GroupPropertie(titulo);
    grupo->anchoValores = 0.5f;
    grupo->flecha = false;
    pIdxMat = new PropFloat(T("Material index"));
    pIdxMat->value = &gUVidxMat; pIdxMat->entero = true; pIdxMat->centrado = true; pIdxMat->flechas = true;
    pIdxMat->stepFino = 1.0f; pIdxMat->stepGrueso = 1.0f; pIdxMat->dragStep = 0.1f; pIdxMat->SetRango(1.0f, 9999.0f);
    grupo->properties.push_back(pIdxMat);
    pUsarMat = new PropBool(T("Use the material's UV index"));
    pUsarMat->value = &gUVusarMat;
    grupo->properties.push_back(pUsarMat);
    pUV = new PropButton(T("UV"));
    pUV->conLabel = true; pUV->button->desplegable = true; pUV->action = UVPabrirLista;
    grupo->properties.push_back(pUV);
    pIdxObj = new PropFloat(T("Object index"));
    pIdxObj->value = &gUVidxObj; pIdxObj->entero = true; pIdxObj->centrado = true; pIdxObj->flechas = true;
    pIdxObj->stepFino = 1.0f; pIdxObj->stepGrueso = 1.0f; pIdxObj->dragStep = 0.1f; pIdxObj->SetRango(1.0f, 9999.0f);
    grupo->properties.push_back(pIdxObj);
    pBotones = new PropButtonRow();
    pBotones->Agregar(T("Cancel"), UVPcerrar);
    pBotones->Agregar(T("Accept"), UVPaceptar);
    pBotones->activo = 1;
    grupo->properties.push_back(pBotones);
    grupo->selectIndex = 0;
    lista = new Card();
    Sincronizar();
    ResizeGrupo();
    Reubicar();
}
UVMapaPopup::~UVMapaPopup() { delete grupo; delete lista; }

// tildado: este objeto usa el indice del material (lo suyo no se elige); destildado: "UV" y, con "Use index", su indice
void UVMapaPopup::Sincronizar() {
    pUV->oculto = gUVusarMat;
    pUV->button->text = NombreSel();
    pIdxObj->value = (!gUVusarMat && gUVsel < 0) ? &gUVidxObj : NULL;
    if (gUVusarMat) listaAbierta = false;
}
void UVMapaPopup::ResizeGrupo() {
    int w = 260 * GlobalScale;
    const int maxW = MenuPantallaW - gapGS * 4;
    if (w > maxW) w = maxW;
    grupo->Resize(w, 0);
    int alto = grupo->height;
    if (listaAbierta) { int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh); if (ly + lh + borderGS > alto) alto = ly + lh + borderGS; }
    popUpWindow->Resize(w, alto);
}
void UVMapaPopup::Reubicar() {
    x = (MenuPantallaW - popUpWindow->width) / 2;
    y = (MenuPantallaH - grupo->height) / 2;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + grupo->height + gapGS * 2)
        y = PopUpActive->y - grupo->height - gapGS * 2;   // (el teclado numerico abajo: el popup sube)
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}
int UVMapaPopup::FilaY(int idx) const {
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)grupo->properties.size(); j++) yRow += grupo->properties[j]->Resize(grupo->width);
    return yRow;
}
int UVMapaPopup::ListaTotal() const { return 1 + (gUVmesh ? (int)gUVmesh->uvMaps.size() : 0); }
int UVMapaPopup::ListaFilas() const { const int t = ListaTotal(); return t < 10 ? t : 10; }
void UVMapaPopup::ListaRect(int& lx, int& ly, int& lw, int& lh) const {
    int idx = 0; for (size_t j = 0; j < grupo->properties.size(); j++) if (grupo->properties[j] == pUV) idx = (int)j;
    lx = grupo->colEtiqueta + 2 * borderGS;
    lw = grupo->propertiBox->width - bordersGS;
    ly = FilaY(idx) + RenglonHeightGS + gapGS;
    lh = ListaFilas() * (RenglonHeightGS + gapGS) + gapGS;
}

void UVMapaPopup::Render() {
    if (!grupo) return;
    Sincronizar(); ResizeGrupo(); Reubicar();
    initView();
    w3dEngine::PushMatrix();
    grupo->Render();
    w3dEngine::PopMatrix();
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    grupo->card->RenderBorder(false);
    if (listaAbierta) {   // "Use index" y, debajo de una linea, las capas UV de la malla (con scroll: la rueda)
        int lx, ly, lw, lh; ListaRect(lx, ly, lw, lh);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
        SetColorID(ColorID::headerColor, 1.0f);
        lista->Resize(lw, lh); lista->RenderObject(false);
        w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
        lista->RenderBorder(false);
        const int total = ListaTotal(), filas = ListaFilas();
        for (int f = 0; f < filas; f++) {
            const int t = listaScroll + f;
            if (t >= total) break;
            const int ry = gapGS + f * (RenglonHeightGS + gapGS);
            const int sel = t - 1;   // -1 = "Use index"
            if (t == 1) {   // la LINEA entre "Use index" y las capas UV
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry - gapGS / 2 - 1), 0);
                SetColorID(ColorID::grisLinea, 1.0f);
                lista->Resize(lw - bordersGS - 2 * gapGS, 2); lista->RenderObject(false);
                w3dEngine::PopMatrix();
            }
            SetColorID((sel == gUVsel) ? ColorID::accent : ColorID::blanco, 1.0f);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry + (RenglonHeightGS - LetterHeightGS) / 2), 0);
            std::string txt = T("Use index");
            if (sel >= 0 && gUVmesh && gUVmesh->uvMaps[(size_t)sel]) {
                char b[16]; snprintf(b, sizeof(b), "%d  ", sel + 1);
                txt = b + gUVmesh->uvMaps[(size_t)sel]->nombre;
            }
            RenderBitmapText(txt, textAlign::left, lw - bordersGS - gapGS);
            w3dEngine::PopMatrix();
        }
        w3dEngine::PopMatrix();
    }
    endView();
}

bool UVMapaPopup::Click(int mx, int my) {
    const int myL = my - y, lx = mx - x;
    if (listaAbierta) {
        int ax, ay, aw, ah; ListaRect(ax, ay, aw, ah);
        listaAbierta = false;
        if (lx >= ax && lx < ax + aw && myL >= ay && myL < ay + ah) {
            const int t = listaScroll + (myL - ay - gapGS) / (RenglonHeightGS + gapGS);
            if (t >= 0 && t < ListaTotal()) gUVsel = t - 1;
            Sincronizar();
            g_redraw = true;
            return true;
        }
        g_redraw = true;
    }
    if (!Contains(mx, my)) return false;
    const int boxStart = grupo->colEtiqueta + 2 * borderGS;
    const int boxW = grupo->propertiBox->width - bordersGS;
    const int aw = RenglonHeightGS;
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (size_t j = 0; j < grupo->properties.size(); j++) {
        PropertieBase* p = grupo->properties[j];
        const int h = p->Resize(grupo->width);
        if (h > 0 && p->Seleccionable() && myL >= yRow && myL < yRow + h) {
            grupo->selectIndex = (int)j;
            const PropertyType::Enum tipo = p->GetType();
            g_textFieldActivo = NULL;
            if (tipo == PropertyType::Float) {
                PropFloat* pf = static_cast<PropFloat*>(p);
                if (lx >= boxStart && lx < boxStart + aw) { pf->Set(*pf->value - 1.0f); g_redraw = true; return true; }
                if (lx >= boxStart + boxW - aw && lx < boxStart + boxW) { pf->Set(*pf->value + 1.0f); g_redraw = true; return true; }
                dragField = pf; dragMoved = false; lastDragMx = mx;
            } else if (tipo == PropertyType::Bool || tipo == PropertyType::Button) {
                p->EditPropertie();
                Sincronizar();
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
                if (celda >= 0) { row->activo = celda; if (row->acciones[(size_t)celda]) row->acciones[(size_t)celda](); return true; }
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;
}
bool UVMapaPopup::Motion(int mx, int my) {
    (void)my;
    if (dragField && dragField->value) {   // ARRASTRAR el valor (como en el panel de propiedades)
        const int dmx = mx - lastDragMx;
        if (dmx != 0) { dragMoved = true; lastDragMx = mx; dragField->Set(*dragField->value + dmx * dragField->dragStep); g_redraw = true; }
    }
    return true;
}
bool UVMapaPopup::Tecla(int tecla) {
    if (listaAbierta) {
        const int total = ListaTotal();
        if (tecla == LayoutKey::Up)   { gUVsel = (gUVsel + 1 + total - 1) % total - 1; }
        else if (tecla == LayoutKey::Down) { gUVsel = (gUVsel + 1 + 1) % total - 1; }
        else if (tecla == LayoutKey::Enter || tecla == LayoutKey::Cancel) listaAbierta = false;
        const int t = gUVsel + 1;
        if (t < listaScroll) listaScroll = t;
        if (t >= listaScroll + ListaFilas()) listaScroll = t - ListaFilas() + 1;
        Sincronizar();
        g_redraw = true;
        return true;
    }
    const int n = (int)grupo->properties.size();
    switch (tecla) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (tecla == LayoutKey::Down) ? +1 : -1;
            int s = grupo->selectIndex;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0;
                if (grupo->properties[(size_t)s]->Seleccionable() && grupo->properties[(size_t)s]->Resize(grupo->width) > 0) break; }
            grupo->selectIndex = s;
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[(size_t)grupo->selectIndex] : NULL;
            if (!sel) break;
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value + (tecla == LayoutKey::Right ? 1.0f : -1.0f)); }
            else if (sel->GetType() == PropertyType::Bool) { sel->EditPropertie(); Sincronizar(); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->activo += (tecla == LayoutKey::Right) ? 1 : -1; row->ClampActivo(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n) ? grupo->properties[(size_t)grupo->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ClampActivo();
                if (row->activo >= 0 && row->activo < (int)row->acciones.size() && row->acciones[(size_t)row->activo]) row->acciones[(size_t)row->activo]();
            } else if (sel && (sel->GetType() == PropertyType::Bool || sel->GetType() == PropertyType::Button)) { sel->EditPropertie(); Sincronizar(); }
            else UVPaceptar();
            break;
        }
        case LayoutKey::Cancel: UVPcerrar(); break;
    }
    g_redraw = true;
    return true;
}
void UVMapaPopup::Wheel(int delta) {
    if (!listaAbierta) return;
    listaScroll -= delta;
    const int maxS = ListaTotal() - ListaFilas();
    if (listaScroll > maxS) listaScroll = maxS;
    if (listaScroll < 0) listaScroll = 0;
    g_redraw = true;
}
void UVMapaPopup::Soltar() {
    if (dragField && !dragMoved && dragField->value) {   // un click sin arrastrar: se ESCRIBE el valor
        dragField->IniciarEdicionTexto();
        if (ToolbarUsaTactil()) NumPadAbrir();
    }
    dragField = NULL;
}
bool UVMapaPopup::Arrastrando() { return dragField != NULL; }
void UVMapaPopup::Cerrar() {
    listaAbierta = false;
    g_textFieldActivo = NULL;
    PopUpBase::Cerrar();
    g_redraw = true;
}

void AbrirUVMapaPopup(Material* mat, Mesh* m, int mapa, const std::string& titulo) {
    if (!mat || mapa < 0 || mapa >= W3D_MAPAS) return;
    gUVmat = mat; gUVmesh = m; gUVmapa = mapa;
    // lo de ahora: el indice del material; si el objeto usa lo suyo, su indice o su capa UV por nombre
    gUVidxMat = (float)(mat->uvMapa[mapa] + 1);
    const std::string nom = m ? m->UVPersonalizado(mat, mapa) : std::string();
    const int idx = m ? m->UVPersonalIndice(mat, mapa) : -1;
    gUVusarMat = nom.empty() && idx < 0;
    gUVsel = -1;
    if (!nom.empty() && m)
        for (size_t j = 0; j < m->uvMaps.size(); j++) if (m->uvMaps[j] && m->uvMaps[j]->nombre == nom) { gUVsel = (int)j; break; }
    gUVidxObj = (float)((idx >= 0 ? idx : mat->uvMapa[mapa]) + 1);
    if (gUVP) { if (PopUpActive == gUVP) PopUpActive = NULL; delete gUVP; gUVP = NULL; }
    gUVP = new UVMapaPopup(titulo);
    PopUpActive = gUVP;
    g_redraw = true;
}
