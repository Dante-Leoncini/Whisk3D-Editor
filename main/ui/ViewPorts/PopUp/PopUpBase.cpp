#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "NumPad.h"

PopUpBase* PopUpActive = NULL;

PopUpBase::PopUpBase(const std::string& Name) {
    // (eran inicializadores de clase: C++03)
    name = Name;
    x = 30;
    y = 30;
    popUpWindow = new Card(NULL, 300, 300);
}

PopUpBase::~PopUpBase() {
    delete popUpWindow;
}

bool PopUpBase::Contains(int mx, int my) const {
    return mx >= x && mx < x + popUpWindow->width &&
           my >= y && my < y + popUpWindow->height;
}

void PopUpBase::initView() {
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // arbol arriba-izquierda -> GL abajo-izquierda (4 OS)
    const int glY = W3dScreenHeight - y - popUpWindow->height;
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, popUpWindow->width, popUpWindow->height);
    w3dEngine::Viewport(x, glY, popUpWindow->width, popUpWindow->height);
    w3dEngine::Ortho(0, popUpWindow->width, popUpWindow->height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
    w3dEngine::Enable(w3dEngine::ColorMaterial);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    w3dEngine::PushMatrix();
}

void PopUpBase::endView() {
    w3dEngine::PopMatrix();
    w3dEngine::Disable(w3dEngine::ScissorTest);
}

void PopUpBase::Render() {}

bool PopUpBase::Click(int mx, int my) {
    return Contains(mx, my);
}

bool PopUpBase::Motion(int mx, int my) {
    return true;
}

bool PopUpBase::Key(int key) {
    return true;
}

void PopUpBase::Release() {}

bool PopUpBase::Dragging() {
    return false;
}

void PopUpBase::Close() {
    if (PopUpActive == this) PopUpActive = NULL;
}

// ---- CardPanel (ver el .h) ----
extern bool g_redraw;
CardPanel::CardPanel() : PopUpBase(""), group(NULL), dragField(NULL), dragMoved(false), lastDragMx(0) {}
bool CardPanel::Collapsed() { return group && !group->open; }
void CardPanel::Expand(bool open) { if (group) group->open = open; }
int CardPanel::RowY(int idx) const {
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)group->properties.size(); j++) yRow += group->properties[j]->Resize(group->width);
    return yRow;
}
void CardPanel::ClickRow(int mx, int my, bool onlyBuild) {
    const int myL = my - y + dz.scroll, lx = mx - x;
    const int boxStart = group->colLabel + 2 * borderGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h) {
            const PropertyType::Enum type = p->GetType();
            if (type == PropertyType::Float) {
                group->selectIndex = (int)j;
                g_activeTextField = NULL;
                dragField = static_cast<PropFloat*>(p); dragMoved = false; lastDragMx = mx;
                g_redraw = true;
                return;
            }
            if (onlyBuild) return;
            group->selectIndex = (int)j;
            g_activeTextField = NULL;
            if (type == PropertyType::Bool || type == PropertyType::Button) {
                p->EditPropertie();
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
                return;
            }
            g_redraw = true;
            return;
        }
        yRow += h;
    }
}
bool CardPanel::Motion(int mx, int my) {
    // arrastre VERTICAL en un panel desplazable: desplaza (y suelta el valor que estaba por arrastrarse)
    if (!dragMoved && dz.Move(mx, my)) { dragField = NULL; g_redraw = true; return true; }
    if (dz.scrolling) return true;
    if (dragField && dragField->value) {
        // desplazable: el valor recien se arrastra cuando el gesto se va claramente de costado
        if (dz.Scrollable() && !dragMoved) {
            const int threshold = 6 * (GlobalScale > 0 ? GlobalScale : 1);
            if (mx - dz.pressX < threshold && dz.pressX - mx < threshold) return true;
            lastDragMx = mx;
        }
        const int dmx = mx - lastDragMx; lastDragMx = mx;
        if (dmx != 0 || (dz.Scrollable() && !dragMoved)) dragMoved = true;
        if (dmx != 0) { dragField->Set(*dragField->value + dmx * dragField->dragStep); g_redraw = true; }
    }
    return true;
}
void CardPanel::Release() {
    const bool touch = dz.Up();
    if (dragField) {
        if (!dragMoved && dragField->value) {
            dragField->StartEditText();            // mouse: se escribe el valor exacto
            if (ToolbarUsesTouch()) KeyboardNumberOpen();       // tactil: el teclado numerico en pantalla
        }
        dragField = NULL;
        return;
    }
    if (touch && dz.Scrollable()) ClickRow(dz.pressX, dz.pressY, false);   // fue un toque: el click de verdad
}
void CardPanel::Wheel(int delta) { dz.Wheel(delta); g_redraw = true; }
bool CardPanel::Dragging() { return dragField != NULL || dz.pressed; }
bool CardPanel::KeyRows(int key) {
    const int n = (int)group->properties.size();
    PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (key == LayoutKey::Down) ? +1 : -1;
            int s = group->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (group->properties[s]->Selectable()) break; }
            group->selectIndex = s;
            dz.ToView(RowY(s), group->properties[(size_t)s]->Resize(group->width));   // la fila elegida, a la vista
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right:
            if (!sel) break;
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value + (key == LayoutKey::Right ? pf->fineStep : -pf->fineStep)); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->active += (key == LayoutKey::Right) ? 1 : -1; row->ActiveClamp(); }
            else if (sel->GetType() == PropertyType::Bool) sel->EditPropertie();
            break;
        case LayoutKey::Enter:
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ActiveClamp();
                if (row->active >= 0 && row->active < (int)row->actions.size() && row->actions[row->active]) row->actions[row->active]();
                return true;
            }
            if (sel && sel->GetType() == PropertyType::Float) static_cast<PropFloat*>(sel)->StartEditText();
            else if (sel) sel->EditPropertie();
            break;
        default: return false;
    }
    g_redraw = true;
    return true;
}
