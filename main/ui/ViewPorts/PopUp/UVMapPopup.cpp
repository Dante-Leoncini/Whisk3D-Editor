// ============================================================================
//  UVMapPopup.cpp - la capa UV de un mapa del material (ver UVMapPopup.h). Mismo esquema que NewTexturePopup:
//  las coordenadas del hit-test son LOCALES al popup (mx - x, my - y).
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "UVMapPopup.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "NumPad.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "undo/Undo.h"
#include "io/ProjectChanges.h"             // W3dTouchedChanges

extern bool g_redraw;
extern void RebindMaterialMeshPart();
extern void RTInvalidate();

static UVMapPopup* gUVP = NULL;
static Material* gUVmat = NULL;
static Mesh* gUVmesh = NULL;
static int gUVMap = 0;
// lo que se edita (se aplica recien con Accept)
static float gUVidxMat = 1.0f;   // 1.. (la UI cuenta desde 1)
static bool  gUVUseMat = true;
static int   gUVsel = -1;        // -1 = "Use index"; j = la capa UV j de la malla
static float gUVidxObj = 1.0f;

// ---- aplicar (con undo, en UN paso) ----
struct UndoUVObj { Mesh* m; std::string mat; int map; std::string name; int idx; };   // lo de ANTES (ida y vuelta)
static void UndoUVObjApply(void* d) {
    UndoUVObj* u = (UndoUVObj*)d;
    if (!u->m) return;
    Material* mt = FindMaterialByName(u->mat);
    Material tmp(u->mat, true);
    const Material* c = mt ? mt : &tmp;
    const std::string name = u->m->UVCustomized(c, u->map); const int idx = u->m->UVPersonalIndex(c, u->map);
    u->m->SetUVCustomized(c, u->map, u->name); u->m->SetUVPersonalIndex(c, u->map, u->idx);
    u->name = name; u->idx = idx;
    RebindMaterialMeshPart();
    RTInvalidate();
    g_redraw = true;
}
static void UndoUVObjUnlink(void* d, Object* deleted) { UndoUVObj* u = (UndoUVObj*)d; if ((Object*)u->m == deleted) u->m = NULL; }
static void UndoUVObjFree(void* d) { delete (UndoUVObj*)d; }

void UVMapPopupApply(Material* mat, Mesh* m, int map, int matIndex, bool useMat, const std::string& uvName, int objIndex) {
    if (!mat || map < 0 || map >= W3D_MAPS) return;
    UndoGroupStart();
    // el INDICE del material (compartido)
    const int im = matIndex < 1 ? 0 : matIndex - 1;
    if (mat->uvMap[map] != im) {
        UndoMaterialModStart(mat);
        mat->uvMap[map] = im;
        UndoMaterialModCommit();
    }
    // lo de ESTE objeto: nada (usa el del material), un indice propio o una capa UV por nombre
    if (m) {
        const std::string name = useMat ? std::string() : uvName;
        const int idx = (useMat || !uvName.empty()) ? -1 : (objIndex < 1 ? 0 : objIndex - 1);
        const std::string namePrev = m->UVCustomized(mat, map);
        const int idxPrev = m->UVPersonalIndex(mat, map);
        if (name != namePrev || idx != idxPrev) {
            UndoUVObj* u = new UndoUVObj();
            u->m = m; u->mat = mat->name; u->map = map; u->name = namePrev; u->idx = idxPrev;
            m->SetUVCustomized(mat, map, name);
            m->SetUVPersonalIndex(mat, map, idx);
            UndoExternal f; f.apply = UndoUVObjApply; f.unlink = UndoUVObjUnlink; f.free = UndoUVObjFree;
            UndoPushExternal(f, u);
            W3dTouchedChanges();
        }
    }
    UndoGroupSequentialEnd();
    RTInvalidate();
    RebindMaterialMeshPart();
    g_redraw = true;
}

static void UVPClose() { if (gUVP) gUVP->Close(); }
static void UVPAccept() {
    std::string name;
    if (!gUVUseMat && gUVsel >= 0 && gUVmesh && gUVsel < (int)gUVmesh->uvMaps.size() && gUVmesh->uvMaps[(size_t)gUVsel])
        name = gUVmesh->uvMaps[(size_t)gUVsel]->name;
    UVMapPopupApply(gUVmat, gUVmesh, gUVMap, (int)(gUVidxMat + 0.5f), gUVUseMat, name, (int)(gUVidxObj + 0.5f));
    UVPClose();
}
static void UVPOpenList() { if (gUVP && !gUVUseMat) { gUVP->listOpen = !gUVP->listOpen; gUVP->listScroll = 0; g_redraw = true; } }
static std::string SelName() {
    if (gUVsel < 0 || !gUVmesh || gUVsel >= (int)gUVmesh->uvMaps.size() || !gUVmesh->uvMaps[(size_t)gUVsel]) return T("Use index");
    return gUVmesh->uvMaps[(size_t)gUVsel]->name;
}

UVMapPopup::UVMapPopup(const std::string& title)
    : PopUpBase(T("UV Map")), group(NULL), pIdxMat(NULL), pUseMat(NULL), pUV(NULL), pIdxObj(NULL), pButtons(NULL),
      dragField(NULL), dragMoved(false), lastDragMx(0), listOpen(false), listScroll(0), list(NULL) {
    group = new GroupPropertie(title);
    group->valueWidth = 0.5f;
    group->arrow = false;
    pIdxMat = new PropFloat(T("Material index"));
    pIdxMat->value = &gUVidxMat; pIdxMat->integer = true; pIdxMat->centered = true; pIdxMat->arrows = true;
    pIdxMat->fineStep = 1.0f; pIdxMat->thickStep = 1.0f; pIdxMat->dragStep = 0.1f; pIdxMat->SetRange(1.0f, 9999.0f);
    group->properties.push_back(pIdxMat);
    pUseMat = new PropBool(T("Use the material's UV index"));
    pUseMat->value = &gUVUseMat;
    group->properties.push_back(pUseMat);
    pUV = new PropButton(T("UV"));
    pUV->withLabel = true; pUV->button->dropdown = true; pUV->action = UVPOpenList;
    group->properties.push_back(pUV);
    pIdxObj = new PropFloat(T("Object index"));
    pIdxObj->value = &gUVidxObj; pIdxObj->integer = true; pIdxObj->centered = true; pIdxObj->arrows = true;
    pIdxObj->fineStep = 1.0f; pIdxObj->thickStep = 1.0f; pIdxObj->dragStep = 0.1f; pIdxObj->SetRange(1.0f, 9999.0f);
    group->properties.push_back(pIdxObj);
    pButtons = new PropButtonRow();
    pButtons->Add(T("Cancel"), UVPClose);
    pButtons->Add(T("Accept"), UVPAccept);
    pButtons->active = 1;
    group->properties.push_back(pButtons);
    group->selectIndex = 0;
    list = new Card();
    Sync();
    ResizeGroup();
    Relocate();
}
UVMapPopup::~UVMapPopup() { delete group; delete list; }

// tildado: este objeto usa el indice del material (lo suyo no se elige); destildado: "UV" y, con "Use index", su indice
void UVMapPopup::Sync() {
    pUV->hidden = gUVUseMat;
    pUV->button->text = SelName();
    pIdxObj->value = (!gUVUseMat && gUVsel < 0) ? &gUVidxObj : NULL;
    if (gUVUseMat) listOpen = false;
}
void UVMapPopup::ResizeGroup() {
    int w = 260 * GlobalScale;
    const int maxW = MenuScreenW - gapGS * 4;
    if (w > maxW) w = maxW;
    group->Resize(w, 0);
    int height = group->height;
    if (listOpen) { int lx, ly, lw, lh; ListRect(lx, ly, lw, lh); if (ly + lh + borderGS > height) height = ly + lh + borderGS; }
    popUpWindow->Resize(w, height);
}
void UVMapPopup::Relocate() {
    x = (MenuScreenW - popUpWindow->width) / 2;
    y = (MenuScreenH - group->height) / 2;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + group->height + gapGS * 2)
        y = PopUpActive->y - group->height - gapGS * 2;   // (el teclado numerico abajo: el popup sube)
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}
int UVMapPopup::RowY(int idx) const {
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)group->properties.size(); j++) yRow += group->properties[j]->Resize(group->width);
    return yRow;
}
int UVMapPopup::ListTotal() const { return 1 + (gUVmesh ? (int)gUVmesh->uvMaps.size() : 0); }
int UVMapPopup::RowList() const { const int t = ListTotal(); return t < 10 ? t : 10; }
void UVMapPopup::ListRect(int& lx, int& ly, int& lw, int& lh) const {
    int idx = 0; for (size_t j = 0; j < group->properties.size(); j++) if (group->properties[j] == pUV) idx = (int)j;
    lx = group->colLabel + 2 * borderGS;
    lw = group->propertiBox->width - bordersGS;
    ly = RowY(idx) + RowHeightGS + gapGS;
    lh = RowList() * (RowHeightGS + gapGS) + gapGS;
}

void UVMapPopup::Render() {
    if (!group) return;
    Sync(); ResizeGroup(); Relocate();
    initView();
    w3dEngine::PushMatrix();
    group->Render();
    w3dEngine::PopMatrix();
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    group->card->RenderBorder(false);
    if (listOpen) {   // "Use index" y, debajo de una linea, las capas UV de la malla (con scroll: la rueda)
        int lx, ly, lw, lh; ListRect(lx, ly, lw, lh);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
        SetColorID(ColorID::headerColor, 1.0f);
        list->Resize(lw, lh); list->RenderObject(false);
        w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
        list->RenderBorder(false);
        const int total = ListTotal(), rows = RowList();
        for (int f = 0; f < rows; f++) {
            const int t = listScroll + f;
            if (t >= total) break;
            const int ry = gapGS + f * (RowHeightGS + gapGS);
            const int sel = t - 1;   // -1 = "Use index"
            if (t == 1) {   // la LINEA entre "Use index" y las capas UV
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry - gapGS / 2 - 1), 0);
                SetColorID(ColorID::grayLine, 1.0f);
                list->Resize(lw - bordersGS - 2 * gapGS, 2); list->RenderObject(false);
                w3dEngine::PopMatrix();
            }
            SetColorID((sel == gUVsel) ? ColorID::accent : ColorID::white, 1.0f);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry + (RowHeightGS - LetterHeightGS) / 2), 0);
            std::string txt = T("Use index");
            if (sel >= 0 && gUVmesh && gUVmesh->uvMaps[(size_t)sel]) {
                char b[16]; sprintf(b, "%d  ", sel + 1);
                txt = b + gUVmesh->uvMaps[(size_t)sel]->name;
            }
            RenderBitmapText(txt, textAlign::left, lw - bordersGS - gapGS);
            w3dEngine::PopMatrix();
        }
        w3dEngine::PopMatrix();
    }
    endView();
}

bool UVMapPopup::Click(int mx, int my) {
    const int myL = my - y, lx = mx - x;
    if (listOpen) {
        int ax, ay, aw, ah; ListRect(ax, ay, aw, ah);
        listOpen = false;
        if (lx >= ax && lx < ax + aw && myL >= ay && myL < ay + ah) {
            const int t = listScroll + (myL - ay - gapGS) / (RowHeightGS + gapGS);
            if (t >= 0 && t < ListTotal()) gUVsel = t - 1;
            Sync();
            g_redraw = true;
            return true;
        }
        g_redraw = true;
    }
    if (!Contains(mx, my)) return false;
    const int boxStart = group->colLabel + 2 * borderGS;
    const int boxW = group->propertiBox->width - bordersGS;
    const int aw = RowHeightGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (h > 0 && p->Selectable() && myL >= yRow && myL < yRow + h) {
            group->selectIndex = (int)j;
            const PropertyType::Enum type = p->GetType();
            g_activeTextField = NULL;
            if (type == PropertyType::Float) {
                PropFloat* pf = static_cast<PropFloat*>(p);
                if (lx >= boxStart && lx < boxStart + aw) { pf->Set(*pf->value - 1.0f); g_redraw = true; return true; }
                if (lx >= boxStart + boxW - aw && lx < boxStart + boxW) { pf->Set(*pf->value + 1.0f); g_redraw = true; return true; }
                dragField = pf; dragMoved = false; lastDragMx = mx;
            } else if (type == PropertyType::Bool || type == PropertyType::Button) {
                p->EditPropertie();
                Sync();
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
                if (cell >= 0) { row->active = cell; if (row->actions[(size_t)cell]) row->actions[(size_t)cell](); return true; }
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;
}
bool UVMapPopup::Motion(int mx, int my) {
    (void)my;
    if (dragField && dragField->value) {   // ARRASTRAR el valor (como en el panel de propiedades)
        const int dmx = mx - lastDragMx;
        if (dmx != 0) { dragMoved = true; lastDragMx = mx; dragField->Set(*dragField->value + dmx * dragField->dragStep); g_redraw = true; }
    }
    return true;
}
bool UVMapPopup::Key(int key) {
    if (listOpen) {
        const int total = ListTotal();
        if (key == LayoutKey::Up)   { gUVsel = (gUVsel + 1 + total - 1) % total - 1; }
        else if (key == LayoutKey::Down) { gUVsel = (gUVsel + 1 + 1) % total - 1; }
        else if (key == LayoutKey::Enter || key == LayoutKey::Cancel) listOpen = false;
        const int t = gUVsel + 1;
        if (t < listScroll) listScroll = t;
        if (t >= listScroll + RowList()) listScroll = t - RowList() + 1;
        Sync();
        g_redraw = true;
        return true;
    }
    const int n = (int)group->properties.size();
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (key == LayoutKey::Down) ? +1 : -1;
            int s = group->selectIndex;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0;
                if (group->properties[(size_t)s]->Selectable() && group->properties[(size_t)s]->Resize(group->width) > 0) break; }
            group->selectIndex = s;
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[(size_t)group->selectIndex] : NULL;
            if (!sel) break;
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value + (key == LayoutKey::Right ? 1.0f : -1.0f)); }
            else if (sel->GetType() == PropertyType::Bool) { sel->EditPropertie(); Sync(); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->active += (key == LayoutKey::Right) ? 1 : -1; row->ActiveClamp(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[(size_t)group->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ActiveClamp();
                if (row->active >= 0 && row->active < (int)row->actions.size() && row->actions[(size_t)row->active]) row->actions[(size_t)row->active]();
            } else if (sel && (sel->GetType() == PropertyType::Bool || sel->GetType() == PropertyType::Button)) { sel->EditPropertie(); Sync(); }
            else UVPAccept();
            break;
        }
        case LayoutKey::Cancel: UVPClose(); break;
    }
    g_redraw = true;
    return true;
}
void UVMapPopup::Wheel(int delta) {
    if (!listOpen) return;
    listScroll -= delta;
    const int maxS = ListTotal() - RowList();
    if (listScroll > maxS) listScroll = maxS;
    if (listScroll < 0) listScroll = 0;
    g_redraw = true;
}
void UVMapPopup::Release() {
    if (dragField && !dragMoved && dragField->value) {   // un click sin arrastrar: se ESCRIBE el valor
        dragField->StartEditText();
        if (ToolbarUsesTouch()) KeyboardNumberOpen();
    }
    dragField = NULL;
}
bool UVMapPopup::Dragging() { return dragField != NULL; }
void UVMapPopup::Close() {
    listOpen = false;
    g_activeTextField = NULL;
    PopUpBase::Close();
    g_redraw = true;
}

void OpenUVMapPopup(Material* mat, Mesh* m, int map, const std::string& title) {
    if (!mat || map < 0 || map >= W3D_MAPS) return;
    gUVmat = mat; gUVmesh = m; gUVMap = map;
    // lo de ahora: el indice del material; si el objeto usa lo suyo, su indice o su capa UV por nombre
    gUVidxMat = (float)(mat->uvMap[map] + 1);
    const std::string name = m ? m->UVCustomized(mat, map) : std::string();
    const int idx = m ? m->UVPersonalIndex(mat, map) : -1;
    gUVUseMat = name.empty() && idx < 0;
    gUVsel = -1;
    if (!name.empty() && m)
        for (size_t j = 0; j < m->uvMaps.size(); j++) if (m->uvMaps[j] && m->uvMaps[j]->name == name) { gUVsel = (int)j; break; }
    gUVidxObj = (float)((idx >= 0 ? idx : mat->uvMap[map]) + 1);
    if (gUVP) { if (PopUpActive == gUVP) PopUpActive = NULL; delete gUVP; gUVP = NULL; }
    gUVP = new UVMapPopup(title);
    PopUpActive = gUVP;
    g_redraw = true;
}
