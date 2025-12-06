#include "ViewPorts/ViewPorts.h"

// -----------------------------
// Variables globales
// -----------------------------
ViewportBase* viewPortActive = nullptr;
ViewportBase* rootViewport = nullptr;

// -----------------------------
// ViewportBase
// -----------------------------
bool ViewportBase::Contains(int mx, int my) const {
    return (mx >= x && mx < x + width && my >= y && my < y + height);
}

bool ViewportBase::isLeaf() const { return true; }

void ViewportBase::Resize(int newW, int newH) {
    width = newW;
    height = newH;
}

ViewportBase::ViewportBase() {}
ViewportBase::~ViewportBase(){}
void ViewportBase::event_mouse_motion(int mx, int my) {}
void ViewportBase::button_left() {}
void ViewportBase::event_key_down(SDL_Event &e) {}
void ViewportBase::event_key_up(SDL_Event &e) {}
void ViewportBase::event_mouse_wheel(SDL_Event &e) {}
void ViewportBase::mouse_button_up(SDL_Event &e) {}
void ViewportBase::event_finger_motion(float pinch){}

// ------------------ Constructor / Destructor ------------------
ViewportRow::ViewportRow(ViewportBase* a, ViewportBase* b, float frac)
    : childA(a), childB(b), splitFrac(frac) {}

ViewportRow::~ViewportRow() {
    //delete childA;
    delete childB;
}

// ------------------ Leaf check ------------------
bool ViewportRow::isLeaf() const {
    return !childA && !childB;
}

// ------------------ Resize children ------------------
void ViewportRow::SetSizeChildrens(int move) {
    if (!childA || !childB) return;  // validar antes
    
    int test_A = childA->width + move;
    int test_B = childB->width - move;

    if (test_A < MinViewportWidthGS || test_B < MinViewportWidthGS) return;

    childA->width += move;
    childA->Resize(childA->width, childA->height);

    childB->x += move;
    childB->width -= move;
    childB->Resize(childB->width, childB->height);

    splitFrac = static_cast<float>(childA->width) / static_cast<float>(width);
}

// ------------------ Resize ------------------
void ViewportRow::Resize(int newW, int newH) {
    width = newW;
    height = newH;

    if (isLeaf()) return;

    if (splitFrac < 0.0f) splitFrac = 0.0f;
    if (splitFrac > 1.0f) splitFrac = 1.0f;

    int wA = static_cast<int>(std::round(width * splitFrac));
    int wB = width - wA;

    if (childA) {
        childA->x = x;
        childA->y = y;
        childA->width = wA;
        childA->height = height;
        childA->Resize(wA, height);
    }
    if (childB) {
        childB->x = x + wA;
        childB->y = y;
        childB->width = wB;
        childB->height = height;
        childB->Resize(wB, height);
    }
}

// ------------------ Render ------------------
void ViewportRow::Render() {
    if (isLeaf()) return;

    if (childA) childA->Render();
    if (childB) childB->Render();
}

// ------------------ Input ------------------
void ViewportRow::button_left() {
    leftMouseDown = true;
    ViewPortClickDown = true;
}

void ViewportRow::event_mouse_motion(int mx, int my) {
    if (leftMouseDown) {
        ViewPortClickDown = true;
        SetSizeChildrens(dx); // dx debe definirse en el contexto de movimiento
    }
}

// ------------------ Leaf check ------------------
ViewportColumn::ViewportColumn(ViewportBase* a, ViewportBase* b, float frac)
    : childA(a), childB(b), splitFrac(frac) {
}

bool ViewportColumn::isLeaf() const {
    return !childA && !childB;
}

// ------------------ Resize children vertical ------------------
void ViewportColumn::SetSizeChildrens(int move) {
    if (!childA || !childB) return;  // validar antes

    int test_A = childA->height - move;
    int test_B = childB->height + move;

    if (test_A < MinViewportHeightGS || test_B < MinViewportHeightGS) return;

    childA->height -= move;
    childA->Resize(childA->width, childA->height);

    childB->y -= move;
    childB->height += move;
    childB->Resize(childB->width, childB->height);

    splitFrac = static_cast<float>(childA->height) / static_cast<float>(height);
}

// ------------------ Resize ------------------
void ViewportColumn::Resize(int newW, int newH) {
    width = newW;
    height = newH;

    if (isLeaf()) return;

    if (splitFrac < 0.0f) splitFrac = 0.0f;
    if (splitFrac > 1.0f) splitFrac = 1.0f;

    int hA = static_cast<int>(std::round(height * splitFrac));
    int hB = height - hA;

    if (childA) {
        childA->x = x;
        childA->y = y;
        childA->width  = width;
        childA->height = hA;
        childA->Resize(width, hA);
    }
    if (childB) {
        childB->x = x;
        childB->y = y + hA;
        childB->width  = width;
        childB->height = hB;
        childB->Resize(width, hB);
    }
}

// ------------------ Render ------------------
void ViewportColumn::Render() {
    if (isLeaf()) return;

    if (childA) childA->Render();
    if (childB) childB->Render();
}

// ------------------ Input ------------------
void ViewportColumn::button_left() {
    leftMouseDown = true;
    ViewPortClickDown = true;
}

void ViewportColumn::event_mouse_motion(int mx, int my) {
    if (leftMouseDown) {
        ViewPortClickDown = true;
        SetSizeChildrens(dy); // dy debe definirse en el contexto de movimiento
    }
}

ViewportColumn::~ViewportColumn() {
    //delete childA;
    delete childB;
}

ViewportBase* FindViewportUnderMouse(ViewportBase* vp, int mx, int my) {
    if (!vp) return nullptr;

    const int PADDING = paddingViewportGS;

    auto isInside = [&](ViewportBase* v) {
        return mx >= v->x && mx < v->x + v->width &&
               my >= v->y && my < v->y + v->height;
    };

    auto isInPadding = [&](ViewportBase* a, ViewportBase* b, bool isRow) {
        if (!a || !b) return false;

        if (isRow) {
            int splitX = a->x + a->width;
            if (mx >= splitX - PADDING && mx < splitX + PADDING &&
                my >= a->y && my < a->y + a->height) 
            {
                SDL_SetCursor(cursorScaleHorizontal);
                return true;
            }
        } else {
            int splitY = a->y + a->height;
            if (my >= splitY - PADDING && my < splitY + PADDING &&
                mx >= a->x && mx < a->x + a->width) 
            {
                SDL_SetCursor(cursorScaleVertical);
                return true;
            }
        }
        SDL_SetCursor(cursorDefault);
        return false;
    };

    // -----------------------------
    // ViewportRow (divide en columnas)
    // -----------------------------
    if (auto row = dynamic_cast<ViewportRow*>(vp)) {
        if (isInPadding(row->childA, row->childB, true))
            return vp;

        if (row->childA && isInside(row->childA))
            return FindViewportUnderMouse(row->childA, mx, my);

        if (row->childB && isInside(row->childB))
            return FindViewportUnderMouse(row->childB, mx, my);
    }
    // -----------------------------
    // ViewportColumn (divide en filas)
    // -----------------------------
    else if (auto col = dynamic_cast<ViewportColumn*>(vp)) {
        if (isInPadding(col->childA, col->childB, false))
            return vp;

        if (col->childA && isInside(col->childA))
            return FindViewportUnderMouse(col->childA, mx, my);

        if (col->childB && isInside(col->childB))
            return FindViewportUnderMouse(col->childB, mx, my);
    }
    // -----------------------------
    // Viewport final (sin hijos)
    // -----------------------------
    else if (vp->Contains(mx, my)) {
        if (mx <= vp->x + PADDING || mx >= vp->x + vp->width - PADDING ||
            my <= vp->y + PADDING || my >= vp->y + vp->height - PADDING)
        {
            return nullptr;
        }
        return vp;
    }

    return nullptr;
}

void SetGlobalScale(int scale){
    GlobalScale = scale;
    marginGS = margin * scale;
    paddingGS = padding * scale;
    gapGS = gap * scale;
    RenglonHeightGS = RenglonHeight * scale;
    borderGS = border * scale;
    LetterWidthGS = LetterWidth * scale;
    LetterHeightGS = LetterHeight * scale;    
    paddingViewportGS = paddingViewport * scale;
    MinViewportHeightGS = MinViewportHeight * scale;
    MinViewportWidthGS = MinViewportWidth * scale;

    SetIconScale(scale);

    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {                  
        SceneCollection->Childrens[i]->name->scaleX = scale;                
        SceneCollection->Childrens[i]->name->scaleY = scale;    
        SceneCollection->Childrens[i]->name->UpdateCache();    
    }
}

void CheckWarpMouseInViewport(int mx, int my, const ViewportBase* vp) {
    if (!vp) return;

    int vx = vp->x;
    int vy = vp->y;
    int vw = vp->width;
    int vh = vp->height;
    
    int left   = vx + borderGS;
    int right  = vx + vw - borderGS;
    int top    = winH - vy - vh + borderGS;            // superior en sistema de ventana
    int bottom = winH - vy - borderGS;       // inferior en sistema de ventana

    //std::cout << "winW: " << winW << " winH: " << winH << std::endl;
    //std::cout << "top: " << top << " bottom: " << bottom << std::endl;
    //std::cout << "my: " << my << std::endl;

    bool warped = false;

    // Wrap horizontal
    if (mx <= left) {
        mx = right - 1;
        warped = true;
    } else if (mx >= right) {
        mx = left + 1;
        warped = true;
    }

    // Wrap vertical
    if (my <= top) {
        my = bottom - 1;
        warped = true;
    } 
    else if (my >= bottom) {
        my = top + 1;
        warped = true;
    }

    if (warped) {
        SDL_WarpMouseInWindow(window, mx, my);
        dx = 0;
        dy = 0;
    } else {
        dx = mx - lastMouseX;
        dy = my - lastMouseY;
    }

    lastMouseX = mx;
    lastMouseY = my;
}