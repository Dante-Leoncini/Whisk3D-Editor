// ============================================================================
//  SaveAsPopup.cpp - el formulario de "Save As..." (ver SaveAsPopup.h). Misma mecanica que los demas
//  formularios flotantes (Nueva textura, Exportar mapa UV): filas navegables con el teclado, tap/click en cada campo,
//  coordenadas LOCALES al popup en el hit-test. La lista de carpetas del proyecto se dibuja encima, pegada a la fila.
// ============================================================================
#include "w3dGraphics.h"
#include "ViewPorts/PopUp/NumPad.h"   // KeyboardTextOpen / KeyboardNumberOpen: el teclado en pantalla (Ajustes)
#include "W3dLang.h"
#include "SaveAsPopup.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "FileBrowser.h"                    // OpenFileBrowser
#include "ConfirmPopup.h"
#include "io/W3dContainer.h"                // W3dContainerHasMounted
#include "io/ProjectResources.h"            // W3dFoldersAll
#include "io/w3dFilesystem.h"
#include "W3dNotice.h"                       // Notificar
#include "app/W3dKeyboard.h"                 // W3dKeyboardOpen

extern bool g_redraw;

static SaveAsPopup* gGC = NULL;
static std::string gGCTitle, gGCName, gGCext, gGCFolderProj, gGCFolderDisk;
static bool gGCInternal = true;
static SaveAsAction gGCAction = NULL;
static std::vector<std::string> gGCFolders;   // "" = la raiz + las de la biblioteca
static SaveAsDestination gGCPending;          // lo que espera la confirmacion de sobrescribir

static void GCClose() { if (gGC) gGC->Close(); }
static void GCSave();
static void GCReopen();
static std::string GCLabelFolder() {
    if (gGCInternal) return gGCFolderProj.empty() ? std::string(T("(root)")) : gGCFolderProj;
    return gGCFolderDisk.empty() ? W3dFileBrowserStartFolder() : gGCFolderDisk;
}
static void GCReadFields() { if (gGC && gGC->pName) gGCName = gGC->pName->field.text; }
static void GCFolderPickedDisk(const std::string& picked) {
    if (!picked.empty()) {
        if (w3dFileSystem::IsDir(picked)) gGCFolderDisk = picked;
        else {   // eligio un archivo: su carpeta + su nombre (pisarlo)
            gGCFolderDisk   = w3dFileSystem::ParentPath(picked);
            const size_t sl = picked.find_last_of("/\\");
            gGCName = (sl == std::string::npos) ? picked : picked.substr(sl + 1);
        }
    }
    GCReopen();
}
static void GCTouchFolder() {
    if (!gGC) return;
    if (gGCInternal) { gGC->listOpen = !gGC->listOpen; gGC->listScroll = 0; g_redraw = true; return; }
    GCReadFields();
    GCClose();
    OpenFileBrowser(gGCTitle, T("Use this folder"), gGCext, GCFolderPickedDisk, true, gGCFolderDisk);
}
static void GCInternalChange() {
    if (gGCInternal && !W3dContainerHasMounted()) {   // sin proyecto guardado no hay .w3d donde meterlo
        gGCInternal = false;
        Notify(T("Save the project first"), true);
    }
    if (gGC) { gGC->listOpen = false; gGC->SyncFolder(); }
    g_redraw = true;
}

SaveAsPopup::SaveAsPopup()
    : PopUpBase(gGCTitle), group(NULL), pName(NULL), pInternal(NULL), pFolder(NULL), pButtons(NULL),
      listOpen(false), listSel(0), listScroll(0), list(NULL) {
    group = new GroupPropertie(gGCTitle);
    group->valueWidth = 0.62f;
    group->arrow = false;
    pName = new PropText(T("Name"), gGCName);
    group->properties.push_back(pName);
    pInternal = new PropBool(T("Inside the .w3d"));
    pInternal->value = &gGCInternal; pInternal->onChange = GCInternalChange;
    group->properties.push_back(pInternal);
    pFolder = new PropButton(T("Folder"));
    pFolder->withLabel = true; pFolder->action = GCTouchFolder;
    group->properties.push_back(pFolder);
    pButtons = new PropButtonRow();
    pButtons->Add(T("Cancel"), GCClose);
    pButtons->Add(T("Save"), GCSave);
    pButtons->active = 1;
    group->properties.push_back(pButtons);
    group->selectIndex = 0;
    list = new Card();
    SyncFolder();
    ResizeGroup();
    Relocate();
}
SaveAsPopup::~SaveAsPopup() { delete group; delete list; }

void SaveAsPopup::SyncFolder() { if (pFolder) pFolder->button->text = GCLabelFolder(); }
int SaveAsPopup::VisibleList() const {
    const int n = (int)gGCFolders.size();
    const int maxRows = (MenuScreenH / 2) / (RowHeightGS + gapGS);
    int v = n < 8 ? n : 8; if (v > maxRows) v = maxRows; if (v < 1) v = 1;
    return v;
}
void SaveAsPopup::ResizeGroup() {
    int w = 270 * GlobalScale;
    const int maxW = MenuScreenW - gapGS * 4;
    if (w > maxW) w = maxW;
    group->Resize(w, 0);
    int height = group->height;
    if (listOpen) { int lx, ly, lw, lh; ListRect(lx, ly, lw, lh); if (ly + lh + borderGS > height) height = ly + lh + borderGS; }
    popUpWindow->Resize(w, height);
}
void SaveAsPopup::Relocate() {
    x = (MenuScreenW - popUpWindow->width) / 2;
    y = (MenuScreenH - group->height) / 2;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + group->height + gapGS * 2)
        y = PopUpActive->y - group->height - gapGS * 2;   // el teclado en pantalla abierto encima: el formulario sube
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}
int SaveAsPopup::RowY(int idx) const {
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)group->properties.size(); j++) yRow += group->properties[j]->Resize(group->width);
    return yRow;
}
void SaveAsPopup::ListRect(int& lx, int& ly, int& lw, int& lh) const {
    int idx = 0; for (size_t j = 0; j < group->properties.size(); j++) if (group->properties[j] == pFolder) idx = (int)j;
    lx = group->colLabel + 2 * borderGS;
    lw = group->propertiBox->width - bordersGS;
    ly = RowY(idx) + RowHeightGS + gapGS;
    lh = VisibleList() * (RowHeightGS + gapGS) + gapGS;
}

void SaveAsPopup::Render() {
    if (!group) return;
    SyncFolder();
    ResizeGroup(); Relocate();
    initView();
    w3dEngine::PushMatrix();
    group->Render();
    w3dEngine::PopMatrix();
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    group->card->RenderBorder(false);
    if (listOpen) {
        int lx, ly, lw, lh; ListRect(lx, ly, lw, lh);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
        SetColorID(ColorID::headerColor, 1.0f);
        list->Resize(lw, lh); list->RenderObject(false);
        w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
        list->RenderBorder(false);
        const int vis = VisibleList();
        for (int r = 0; r < vis; r++) {
            const int i = listScroll + r;
            if (i >= (int)gGCFolders.size()) break;
            const int ry = gapGS + r * (RowHeightGS + gapGS);
            if (i == listSel) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)borderGS, (GLfloat)ry, 0);
                SetColorID(ColorID::accent, 0.35f);
                list->Resize(lw - bordersGS, RowHeightGS); list->RenderObject(false);
                w3dEngine::PopMatrix();
            }
            SetColorID(gGCFolders[(size_t)i] == gGCFolderProj ? ColorID::accent : ColorID::white, 1.0f);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry + (RowHeightGS - LetterHeightGS) / 2), 0);
            RenderBitmapText(gGCFolders[(size_t)i].empty() ? std::string(T("(root)")) : gGCFolders[(size_t)i], textAlign::left, lw - bordersGS - gapGS);
            w3dEngine::PopMatrix();
        }
        w3dEngine::PopMatrix();
    }
    endView();
}

bool SaveAsPopup::Click(int mx, int my) {
    const int myL = my - y, lx = mx - x;
    if (listOpen) {
        int ax, ay, aw, ah; ListRect(ax, ay, aw, ah);
        listOpen = false;
        if (lx >= ax && lx < ax + aw && myL >= ay && myL < ay + ah) {
            const int i = listScroll + (myL - ay - gapGS) / (RowHeightGS + gapGS);
            if (i >= 0 && i < (int)gGCFolders.size()) gGCFolderProj = gGCFolders[(size_t)i];
            SyncFolder();
            g_redraw = true;
            return true;
        }
        g_redraw = true;
    }
    if (!Contains(mx, my)) return false;
    const int boxStart = group->colLabel + 2 * borderGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h) {
            group->selectIndex = (int)j;
            const PropertyType::Enum type = p->GetType();
            if (type != PropertyType::Text) g_activeTextField = NULL;
            if (type == PropertyType::Text) {
                g_activeTextField = NULL;
                p->EditPropertie();
                #ifndef W3D_SYMBIAN
                if (ToolbarUsesTouch()) KeyboardTextOpen();
                #endif
            } else if (type == PropertyType::Bool || type == PropertyType::Button) {
                GCReadFields();
                p->EditPropertie();   // el check llama a GCInternalChange; la carpeta abre la lista o el explorador
                if (p == pFolder) return true;
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
                if (cell >= 0) { row->active = cell; if (row->actions[(size_t)cell]) row->actions[(size_t)cell](); }
                return true;
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;
}
void SaveAsPopup::Wheel(int delta) {
    if (!listOpen) return;
    const int maxS = (int)gGCFolders.size() - VisibleList();
    listScroll += (delta > 0) ? -1 : 1;
    if (listScroll > maxS) listScroll = maxS; if (listScroll < 0) listScroll = 0;
    g_redraw = true;
}
bool SaveAsPopup::Key(int key) {
    if (!group || group->properties.empty()) return true;
    const int n = (int)group->properties.size();
    if (listOpen) {
        const int nc = (int)gGCFolders.size();
        switch (key) {
            case LayoutKey::Up:   if (listSel > 0) listSel--; break;
            case LayoutKey::Down: if (listSel + 1 < nc) listSel++; break;
            case LayoutKey::Enter: if (listSel >= 0 && listSel < nc) gGCFolderProj = gGCFolders[(size_t)listSel]; listOpen = false; SyncFolder(); break;
            case LayoutKey::Cancel: listOpen = false; break;
            default: break;
        }
        if (listSel < listScroll) listScroll = listSel;
        if (listSel >= listScroll + VisibleList()) listScroll = listSel - VisibleList() + 1;
        g_redraw = true;
        return true;
    }
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (key == LayoutKey::Down) ? +1 : -1;
            int s = group->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (group->properties[s]->Selectable()) break; }
            group->selectIndex = s;
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::Bool) { GCReadFields(); sel->EditPropertie(); }
            else if (sel && sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->active += (key == LayoutKey::Right) ? 1 : -1; row->ActiveClamp(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ActiveClamp();
                if (row->active >= 0 && row->active < (int)row->actions.size() && row->actions[row->active]) row->actions[row->active]();
                return true;
            } else if (sel && (sel->GetType() == PropertyType::Text || sel->GetType() == PropertyType::Bool || sel->GetType() == PropertyType::Button)) {
                GCReadFields();
                sel->EditPropertie();
                if (sel == pFolder) { listSel = 0; for (size_t i = 0; i < gGCFolders.size(); i++) if (gGCFolders[i] == gGCFolderProj) listSel = (int)i; return true; }
            } else { GCSave(); return true; }
            break;
        }
        case LayoutKey::Cancel: GCClose(); return true;
    }
    g_redraw = true;
    return true;
}
void SaveAsPopup::Close() {
    listOpen = false;
    g_activeTextField = NULL;
    PopUpBase::Close();
    g_redraw = true;
}

static SaveAsDestination GCDestination() {
    SaveAsDestination d;
    std::string name = gGCName;
    const size_t a = name.find_first_not_of(" \t"); name = (a == std::string::npos) ? std::string() : name.substr(a);
    while (!name.empty() && (name[name.size() - 1] == ' ' || name[name.size() - 1] == '\t')) name.erase(name.size() - 1);
    const size_t sl = name.find_last_of("/\\"); if (sl != std::string::npos) name = name.substr(sl + 1);
    if (!name.empty() && !gGCext.empty() &&
        (name.size() < gGCext.size() || name.substr(name.size() - gGCext.size()) != gGCext)) name += gGCext;
    d.name = name;
    d.internal = gGCInternal;
    d.folderProject = gGCFolderProj;
    d.folderDisk = gGCFolderDisk.empty() ? W3dFileBrowserStartFolder() : gGCFolderDisk;
    return d;
}
static void GCConfirmedOverwrite() {
    std::string msg; bool exists = false;
    if (gGCAction && !gGCAction(gGCPending, true, &exists, msg) && !msg.empty()) Notify(T(msg.c_str()), true);
    g_redraw = true;
}
bool SaveAsNow(bool force, std::string& msg) {
    GCReadFields();
    SaveAsDestination d = GCDestination();
    if (d.name.empty()) { msg = "Name"; return false; }
    if (!gGCAction) { msg = "Nothing to save"; return false; }
    bool exists = false;
    if (gGCAction(d, force, &exists, msg)) { GCClose(); return true; }
    if (exists) msg = "The file already exists. Do you want to replace it?";
    return false;
}
bool SaveAsOpen() { return gGC && PopUpActive == gGC; }
void SaveAsSet(const std::string& name, int internal, const std::string& folderProj, const std::string& folderDisk) {
    gGCName = name;
    if (gGC && gGC->pName) gGC->pName->field.SetText(name);
    if (internal >= 0) { gGCInternal = (internal != 0); GCInternalChange(); }
    gGCFolderProj = folderProj;
    if (!folderDisk.empty()) gGCFolderDisk = folderDisk;
}
static void GCSave() {
    GCReadFields();
    SaveAsDestination d = GCDestination();
    if (d.name.empty()) { if (gGC && gGC->pName) gGC->pName->error = true; g_redraw = true; return; }
    if (!gGCAction) { GCClose(); return; }
    std::string msg; bool exists = false;
    if (gGCAction(d, false, &exists, msg)) { GCClose(); return; }
    if (exists) {   // ya hay un archivo con ese nombre ahi: preguntar antes de pisarlo
        gGCPending = d;
        GCClose();
        if (!confirmPopup) confirmPopup = new ConfirmPopup();
        confirmPopup->Open(std::string(T("The file already exists. Do you want to replace it?")) + "\n" + d.name, GCConfirmedOverwrite);
        return;
    }
    if (!msg.empty()) Notify(T(msg.c_str()), true);
}
void SaveAsPress() { GCSave(); }
static void GCReopen() {
    gGCFolders.clear();
    gGCFolders.push_back(std::string());
    std::vector<std::string> cs; W3dFoldersAll(cs);
    for (size_t i = 0; i < cs.size(); i++) if (!cs[i].empty()) gGCFolders.push_back(cs[i]);
    if (gGC) { if (PopUpActive == gGC) PopUpActive = NULL; delete gGC; gGC = NULL; }
    gGC = new SaveAsPopup();
    PopUpActive = gGC;
    g_redraw = true;
}
void OpenSaveAsPopup(const std::string& title, const std::string& name, const std::string& ext, bool internal,
                           const std::string& folderProject, SaveAsAction action, const std::string& folderDisk) {
    gGCTitle = title; gGCName = name; gGCext = ext;
    if (!folderDisk.empty() && w3dFileSystem::IsDir(folderDisk)) gGCFolderDisk = folderDisk;
    gGCInternal = internal && W3dContainerHasMounted();
    gGCFolderProj = folderProject;
    gGCAction = action;
    GCReopen();
}
