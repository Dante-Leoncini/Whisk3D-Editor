// ============================================================================
//  ShortcutsPopup.cpp - la ventana de atajos de teclado (ver ShortcutsPopup.h). Arriba el formulario (contexto + botones),
//  abajo la lista de funciones del contexto con sus combinaciones. Coordenadas LOCALES al popup en el hit-test.
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "ShortcutsPopup.h"
#include "app/W3dShortcuts.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "W3dNotice.h"                      // Notificar
#include "objects/Textures.h"               // la fuente (el desplegable se dibuja aca)

extern bool g_redraw;

static ShortcutsPopup* gAt = NULL;
static int gAtCtx = W3dATC_3D;   // el ultimo contexto mirado (al reabrir se vuelve ahi)

// UNA COSA ES EL VIEWPORT Y OTRA EL CONTEXTO: arriba se elige el viewport (o "todos") y abajo, en que situacion de
// ese viewport valen las combinaciones (un modo, o con un mover/rotar/escalar en curso). Los contextos de una misma
// vista se pisan entre si de lo mas especifico a lo comun (W3dShortcutContexts). "Durante un transform" es el mismo
// en la 3D, el UV, el timeline y el 2D: aparece en los cuatro.
struct ShortcutView { const char* name; int n; int ctx[4]; const char* nameCtx[4]; };
static const ShortcutView kViews[] = {
    { "All viewports",  1, { W3dATC_GLOBAL },                                            { "Always" } },
    { "3D Viewport",    4, { W3dATC_3D, W3dATC_3D_OBJ, W3dATC_3D_EDIT, W3dATC_XFORM },   { "Any mode", "Object Mode", "Edit Mode", "During a transform" } },
    { "UV Editor",      2, { W3dATC_UV, W3dATC_XFORM },                                  { "General", "During a transform" } },
    { "Timeline",       2, { W3dATC_TIMELINE, W3dATC_XFORM },                            { "General", "During a transform" } },
    { "Outliner",       1, { W3dATC_OUTLINER },                                          { "General" } },
    { "2D Editor",      2, { W3dATC_2D, W3dATC_XFORM },                                  { "General", "During a transform" } },
    { "Properties",     1, { W3dATC_PROPS },                                             { "General" } },
    { "Script Editor",  1, { W3dATC_IDE },                                               { "General" } },
};
static const int kNViews = (int)(sizeof(kViews) / sizeof(kViews[0]));
static int CtxView(int ctx, int preferred) {
    if (preferred >= 0 && preferred < kNViews)
        for (int k = 0; k < kViews[preferred].n; k++) if (kViews[preferred].ctx[k] == ctx) return preferred;
    for (int v = 0; v < kNViews; v++) for (int k = 0; k < kViews[v].n; k++) if (kViews[v].ctx[k] == ctx) return v;
    return 0;
}
static const char* NameCtxInView(int v, int ctx) {
    if (v < 0 || v >= kNViews) return "?";
    for (int k = 0; k < kViews[v].n; k++) if (kViews[v].ctx[k] == ctx) return kViews[v].nameCtx[k];
    return kViews[v].nameCtx[0];
}

static void AtClose() { if (gAt) gAt->Close(); }
static void AtView() { if (gAt) gAt->OpenDropdown(0); }
static void AtContext() { if (gAt) gAt->OpenDropdown(1); }
static void AtAdd() {
    if (!gAt) return;
    const int i = gAt->Selected();
    if (i < 0) return;
    g_shortcutCapturing = i;   // controls.cpp le da la proxima combinacion (Esc = no cambiar nada)
    g_redraw = true;
}
static void AtEmpty() {
    if (!gAt) return;
    const int i = gAt->Selected();
    if (i < 0) return;
    g_shortcutCapturing = -1;
    W3dShortcutEmpty(i);
    g_redraw = true;
}
static void AtReset() {
    if (!gAt) return;
    const int i = gAt->Selected();
    if (i < 0) return;
    g_shortcutCapturing = -1;
    W3dShortcutReset(i);
    g_redraw = true;
}
static void AtResetAll() {
    g_shortcutCapturing = -1;
    W3dShortcutsResetAll();
    Notify(T("All shortcuts restored"), false);
    g_redraw = true;
}

ShortcutsPopup::ShortcutsPopup()
    : PopUpBase(T("Keyboard Shortcuts")), group(NULL), pView(NULL), pContext(NULL), pRow(NULL), pGeneral(NULL), list(NULL),
      offset(NULL), offsetThat(0), view(0), ctx(gAtCtx), sel(0), scroll(0) {
    view  = CtxView(ctx, -1);
    group = new GroupPropertie(T("Keyboard Shortcuts"));
    group->valueWidth = 0.62f;
    group->arrow = false;
    pView = new PropButton(T("Viewport"));
    pView->withLabel = true;
    pView->button->dropdown = true;
    pView->action = AtView;
    group->properties.push_back(pView);
    pContext = new PropButton(T("Context"));
    pContext->withLabel = true;
    pContext->button->dropdown = true;
    pContext->action = AtContext;
    group->properties.push_back(pContext);
    pRow = new PropButtonRow();
    pRow->Add(T("Add shortcut"), AtAdd);
    pRow->Add(T("Clear"), AtEmpty);
    pRow->Add(T("Reset"), AtReset);
    group->properties.push_back(pRow);
    pGeneral = new PropButtonRow();
    pGeneral->Add(T("Reset all"), AtResetAll);
    pGeneral->Add(T("Close"), AtClose);
    pGeneral->active = 1;
    group->properties.push_back(pGeneral);
    group->selectIndex = -1;
    list = new Card();
    BuildRows();
    ResizeGroup();
    Relocate();
}
ShortcutsPopup::~ShortcutsPopup() { delete group; delete list; delete offset; }

void ShortcutsPopup::BuildRows() {
    rows.clear();
    for (int i = 0; i < W3dShortcutsCount(); i++) if (W3dShortcutInfo(i).ctx == ctx) rows.push_back(i);
    if (sel >= (int)rows.size()) sel = (int)rows.size() - 1;
    if (sel < 0) sel = 0;
    view = CtxView(ctx, view);
    if (pView) pView->button->text = T(kViews[view].name);
    if (pContext) pContext->button->text = T(NameCtxInView(view, ctx));
    EnsureVisible();
}
void ShortcutsPopup::PickView(int v) {
    if (v < 0 || v >= kNViews) return;
    g_shortcutCapturing = -1;
    view = v; ctx = kViews[v].ctx[0]; gAtCtx = ctx;
    sel = 0; scroll = 0;
    BuildRows();
    g_redraw = true;
}
void ShortcutsPopup::PickContext(int c) {
    if (c < 0 || c >= kViews[view].n) return;
    g_shortcutCapturing = -1;
    ctx = kViews[view].ctx[c]; gAtCtx = ctx;
    sel = 0; scroll = 0;
    BuildRows();
    g_redraw = true;
}
void ShortcutsPopup::ChangeContext(int step) { PickView((view + step + kNViews) % kNViews); }
// los desplegables de arriba (Viewport / Contexto): un menu de los de siempre, pero lo dibuja y lo maneja ESTA ventana
// (los menus globales del editor quedan debajo de una ventana modal)
void ShortcutsPopup::OpenDropdown(int that) {
    PropButton* pb = (that == 0) ? pView : pContext;
    if (!pb) return;
    if (!offset) offset = new PopupMenu();
    offset->Clear();
    offsetThat = that;
    if (that == 0) for (int v = 0; v < kNViews; v++) offset->Add(T(kViews[v].name), v)->green = (v == view);
    else for (int k = 0; k < kViews[view].n; k++) offset->Add(T(kViews[view].nameCtx[k]), k)->green = (kViews[view].ctx[k] == ctx);
    offset->Resize();
    // pegado abajo del boton, EN PANTALLA. La tarjeta se dibuja en coordenadas de la ventana y nadie le anota al
    // boton donde quedo (sx/sy los llena el panel de Propiedades al dibujar las suyas): se calcula aca con el mismo
    // recorrido de filas que el click. Antes quedaba en -10000 y el menu salia en la esquina de arriba a la izquierda.
    int yRow = borderGS + RowHeightGS + gapGS;   // (la cabecera de la tarjeta)
    for (size_t j = 0; j < group->properties.size() && group->properties[j] != pb; j++) yRow += group->properties[j]->Resize(group->width);
    pb->Resize(group->width);
    pb->button->sx = x + borderGS + borderGS + pb->ColButton();
    pb->button->sy = y + yRow;
    offset->Open(pb->button->sx + pb->button->width - offset->width, pb->button->sy + pb->button->height - GlobalScale,
                MenuScreenW, MenuScreenH);
    g_redraw = true;
}
int ShortcutsPopup::Selected() const {
    return (sel >= 0 && sel < (int)rows.size()) ? rows[(size_t)sel] : -1;
}
int ShortcutsPopup::Visible() const {
    const int row = RowHeightGS + gapGS;
    int free = MenuScreenH - gapGS * 4 - (group ? group->height : 0) - gapGS * 2;
    int v = free / (row > 0 ? row : 1);
    if (v > 14) v = 14;
    if (v > (int)rows.size()) v = (int)rows.size();
    if (v < 3) v = 3;
    return v;
}
void ShortcutsPopup::EnsureVisible() {
    const int v = Visible();
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + v) scroll = sel - v + 1;
    const int maxS = (int)rows.size() - v;
    if (scroll > maxS) scroll = maxS;
    if (scroll < 0) scroll = 0;
}
void ShortcutsPopup::ListRect(int& lx, int& ly, int& lw, int& lh) const {
    lx = borderGS * 2;
    lw = popUpWindow->width - borderGS * 4;
    ly = group->height + gapGS;
    lh = Visible() * (RowHeightGS + gapGS) + gapGS;
}
void ShortcutsPopup::ResizeGroup() {
    int w = 340 * GlobalScale;
    const int maxW = MenuScreenW - gapGS * 4;
    if (w > maxW) w = maxW;
    group->Resize(w, 0);
    popUpWindow->Resize(w, group->height);   // (el ancho lo usa ListRect)
    int lx, ly, lw, lh; ListRect(lx, ly, lw, lh);
    popUpWindow->Resize(w, ly + lh + borderGS * 2);
}
void ShortcutsPopup::Relocate() {
    x = (MenuScreenW - popUpWindow->width) / 2;
    y = (MenuScreenH - popUpWindow->height) / 2;
    if (x < gapGS) x = gapGS;
    if (y < gapGS) y = gapGS;
}

void ShortcutsPopup::Render() {
    if (!group) return;
    pView->button->text = T(kViews[view].name);
    pContext->button->text = T(NameCtxInView(view, ctx));
    const bool waiting = (g_shortcutCapturing >= 0);
    pRow->buttons[0]->text = waiting ? T("Press a key...") : T("Add shortcut");
    ResizeGroup(); Relocate();
    initView();
    // el fondo de la ventana entera (el grupo dibuja solo su parte de arriba)
    SetColorID(ColorID::background, 1.0f);
    popUpWindow->RenderObject(false);
    w3dEngine::PushMatrix();
    group->Render();
    w3dEngine::PopMatrix();
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    popUpWindow->RenderBorder(false);

    int lx, ly, lw, lh; ListRect(lx, ly, lw, lh);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
    SetColorID(ColorID::headerColor, 1.0f);
    list->Resize(lw, lh); list->RenderObject(false);
    const int v = Visible();
    const int colCombos = lw * 55 / 100;   // nombre a la izquierda, combinaciones a la derecha
    for (int r = 0; r < v; r++) {
        const int f = scroll + r;
        if (f >= (int)rows.size()) break;
        const int i = rows[(size_t)f];
        const int ry = gapGS + r * (RowHeightGS + gapGS);
        if (f == sel) {
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)borderGS, (GLfloat)ry, 0);
            SetColorID(ColorID::accent, waiting ? 0.6f : 0.35f);
            list->Resize(lw - bordersGS, RowHeightGS); list->RenderObject(false);
            w3dEngine::PopMatrix();
        }
        const int ty = ry + (RowHeightGS - LetterHeightGS) / 2;
        SetColorID(ColorID::white, 1.0f);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)ty, 0);
        RenderBitmapText(T(W3dShortcutInfo(i).name), textAlign::left, colCombos - borderGS - gapGS * 2);
        w3dEngine::PopMatrix();
        std::string combos = W3dShortcutText(i);
        if (waiting && i == g_shortcutCapturing) combos = T("Press a key...");
        else if (combos.empty()) combos = "-";
        // cambiada por el usuario: en el color de acento (de fabrica: blanco). En la fila elegida el fondo ya es
        // de acento: ahi va en blanco para que se lea.
        SetColorID((W3dShortcutIsDefault(i) || f == sel) ? ColorID::white : ColorID::accent, 1.0f);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)colCombos, (GLfloat)ty, 0);
        RenderBitmapText(combos, textAlign::left, lw - colCombos - gapGS);
        w3dEngine::PopMatrix();
    }
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    list->Resize(lw, lh); list->RenderBorder(false);
    w3dEngine::PopMatrix();
    endView();
    // el DESPLEGABLE abierto, encima de todo (en coordenadas de PANTALLA, como los menus del editor)
    if (offset && offset->open) {
        w3dEngine::Viewport(0, 0, MenuScreenW, MenuScreenH);
        w3dEngine::Disable(w3dEngine::ScissorTest);
        w3dEngine::MatrixMode(w3dEngine::Projection);
        w3dEngine::LoadIdentity();
        w3dEngine::Ortho(0, MenuScreenW, MenuScreenH, 0, -1, 1);
        w3dEngine::MatrixMode(w3dEngine::ModelView);
        w3dEngine::LoadIdentity();
        w3dEngine::Disable(w3dEngine::DepthTest);
        w3dEngine::Enable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::Blend);
        w3dEngine::BlendAlpha();
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        w3dEngine::DisableArray(w3dEngine::NormalArray);
        if (!Textures.empty() && Textures[0]) w3dEngine::BindTexture(Textures[0]->iID);
        offset->Render();
    }
}
bool ShortcutsPopup::Motion(int mx, int my) {
    if (offset && offset->open) { offset->MouseMove(mx, my); g_redraw = true; }
    return true;
}

bool ShortcutsPopup::Click(int mx, int my) {
    // el desplegable abierto se queda con el click: elegir una opcion, o cerrarlo tocando afuera
    if (offset && offset->open) {
        const bool inside = offset->Contains(mx, my);
        const int id = inside ? offset->Click(mx, my) : -1;
        offset->Close();
        if (id >= 0) { if (offsetThat == 0) PickView(id); else PickContext(id); }
        g_redraw = true;
        if (inside || Contains(mx, my)) return true;
    }
    if (!Contains(mx, my)) { g_shortcutCapturing = -1; return false; }
    const int myL = my - y, lxL = mx - x;
    int lx, ly, lw, lh; ListRect(lx, ly, lw, lh);
    if (lxL >= lx && lxL < lx + lw && myL >= ly && myL < ly + lh) {
        const int f = scroll + (myL - ly - gapGS) / (RowHeightGS + gapGS);
        if (f >= 0 && f < (int)rows.size()) {
            // tocar la fila ya elegida = esperar una combinacion para ella (lo mismo que "Add shortcut")
            if (f == sel && g_shortcutCapturing < 0) AtAdd();
            else { g_shortcutCapturing = -1; sel = f; }
        }
        g_redraw = true;
        return true;
    }
    const int boxStart = group->colLabel + 2 * borderGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h) {
            const PropertyType::Enum type = p->GetType();
            if (type == PropertyType::Button) p->EditPropertie();
            else if (type == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(p);
                const int cw = row->CellWidth(group->width), x0 = boxStart - group->colLabel;
                int cell = -1, vis = 0;
                for (size_t b = 0; b < row->buttons.size(); b++) {
                    if (!row->buttons[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lxL >= cx && lxL < cx + cw) { cell = (int)b; break; }
                    vis++;
                }
                if (cell >= 0) { row->active = cell; if (row->actions[(size_t)cell]) row->actions[(size_t)cell](); }
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;
}
void ShortcutsPopup::Wheel(int delta) {
    scroll += (delta > 0) ? -1 : 1;
    const int maxS = (int)rows.size() - Visible();
    if (scroll > maxS) scroll = maxS;
    if (scroll < 0) scroll = 0;
    g_redraw = true;
}
bool ShortcutsPopup::Key(int key) {
    // con el desplegable abierto, las flechas lo recorren, OK elige y Esc lo cierra (sin cerrar la ventana)
    if (offset && offset->open) {
        switch (key) {
            case LayoutKey::Up:   offset->button_up(); break;
            case LayoutKey::Down: offset->button_down(); break;
            case LayoutKey::Enter: { const int id = offset->Enter(); offset->Close();
                                     if (id >= 0) { if (offsetThat == 0) PickView(id); else PickContext(id); } break; }
            case LayoutKey::Cancel: offset->Close(); break;
            default: break;
        }
        g_redraw = true;
        return true;
    }
    switch (key) {
        case LayoutKey::Up:    if (sel > 0) sel--; EnsureVisible(); break;
        case LayoutKey::Down:  if (sel + 1 < (int)rows.size()) sel++; EnsureVisible(); break;
        case LayoutKey::Left:  ChangeContext(-1); break;
        case LayoutKey::Right: ChangeContext(+1); break;
        case LayoutKey::Enter: AtAdd(); break;
        case LayoutKey::Cancel: AtClose(); return true;
        default: break;
    }
    g_redraw = true;
    return true;
}
void ShortcutsPopup::Close() {
    g_shortcutCapturing = -1;
    if (offset) offset->Close();
    PopUpBase::Close();
    g_redraw = true;
}

void OpenShortcutsPopup() {
    if (gAt) { if (PopUpActive == gAt) PopUpActive = NULL; delete gAt; gAt = NULL; }
    gAt = new ShortcutsPopup();
    PopUpActive = gAt;
    g_redraw = true;
}
bool ShortcutsPopupOpen() { return gAt && PopUpActive == gAt; }
ShortcutsPopup* ShortcutsCurrentPopup() { return gAt; }
