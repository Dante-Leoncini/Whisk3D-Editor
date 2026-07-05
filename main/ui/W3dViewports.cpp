/*
 * ==============================================================================
 *  W3dViewports.cpp — ver W3dViewports.h. Logica identica a la de
 *  main/ViewPorts/ViewPorts.cpp de PC, reescrita en dialecto C++03 portable.
 * ==============================================================================
 */

#include "W3dViewports.h"

int W3dMinViewportWidth  = 60;
int W3dMinViewportHeight = 60;
int W3dPaddingViewport   = 4;

// redondeo simple (evita depender de <cmath> en el core)
static int RoundToInt(float v){
    return (v >= 0.0f) ? (int)(v + 0.5f) : (int)(v - 0.5f);
}

// ----------------------------------------------------------------------------
// W3dViewportBase
// ----------------------------------------------------------------------------

W3dViewportBase::W3dViewportBase()
    : x(0), y(0), width(0), height(0), parent(0) {}

W3dViewportBase::~W3dViewportBase() {}

int W3dViewportBase::Kind() const { return W3dView_Leaf; }

bool W3dViewportBase::Contains(int mx, int my) const {
    return (mx >= x && mx < x + width && my >= y && my < y + height);
}

bool W3dViewportBase::isLeaf() const { return true; }

void W3dViewportBase::Resize(int newW, int newH) {
    width = newW;
    height = newH;
}

void W3dViewportBase::OnEvent(const W3dEvent& /*e*/) {}

// ----------------------------------------------------------------------------
// W3dViewportRow (divide en 2 columnas)
// ----------------------------------------------------------------------------

W3dViewportRow::W3dViewportRow(W3dViewportBase* a, W3dViewportBase* b, float frac)
    : childA(a), childB(b), splitFrac(frac) {
    if (childA) childA->parent = this;
    if (childB) childB->parent = this;
}

W3dViewportRow::~W3dViewportRow() {
    delete childA;
    delete childB;
}

int W3dViewportRow::Kind() const { return W3dView_Row; }

bool W3dViewportRow::isLeaf() const { return !childA && !childB; }

void W3dViewportRow::SetSizeChildrens(int move) {
    if (!childA || !childB) return;

    int test_A = childA->width + move;
    int test_B = childB->width - move;
    if (test_A < W3dMinViewportWidth || test_B < W3dMinViewportWidth) return;

    childA->width += move;
    childA->Resize(childA->width, childA->height);

    childB->x += move;
    childB->width -= move;
    childB->Resize(childB->width, childB->height);

    splitFrac = (float)childA->width / (float)width;
}

void W3dViewportRow::Resize(int newW, int newH) {
    width = newW;
    height = newH;
    if (isLeaf()) return;

    if (splitFrac < 0.0f) splitFrac = 0.0f;
    if (splitFrac > 1.0f) splitFrac = 1.0f;

    int wA = RoundToInt(width * splitFrac);
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

void W3dViewportRow::Render() {
    if (childA) childA->Render();
    if (childB) childB->Render();
}

// ----------------------------------------------------------------------------
// W3dViewportColumn (divide en 2 filas)
// ----------------------------------------------------------------------------

W3dViewportColumn::W3dViewportColumn(W3dViewportBase* a, W3dViewportBase* b, float frac)
    : childA(a), childB(b), splitFrac(frac) {
    if (childA) childA->parent = this;
    if (childB) childB->parent = this;
}

W3dViewportColumn::~W3dViewportColumn() {
    delete childA;
    delete childB;
}

int W3dViewportColumn::Kind() const { return W3dView_Column; }

bool W3dViewportColumn::isLeaf() const { return !childA && !childB; }

void W3dViewportColumn::SetSizeChildrens(int move) {
    if (!childA || !childB) return;

    int test_A = childA->height - move;
    int test_B = childB->height + move;
    if (test_A < W3dMinViewportHeight || test_B < W3dMinViewportHeight) return;

    childA->height -= move;
    childA->Resize(childA->width, childA->height);

    childB->y -= move;
    childB->height += move;
    childB->Resize(childB->width, childB->height);

    splitFrac = (float)childA->height / (float)height;
}

void W3dViewportColumn::Resize(int newW, int newH) {
    width = newW;
    height = newH;
    if (isLeaf()) return;

    if (splitFrac < 0.0f) splitFrac = 0.0f;
    if (splitFrac > 1.0f) splitFrac = 1.0f;

    int hA = RoundToInt(height * splitFrac);
    int hB = height - hA;

    if (childA) {
        childA->x = x;
        childA->y = y;
        childA->width = width;
        childA->height = hA;
        childA->Resize(width, hA);
    }
    if (childB) {
        childB->x = x;
        childB->y = y + hA;
        childB->width = width;
        childB->height = hB;
        childB->Resize(width, hB);
    }
}

void W3dViewportColumn::Render() {
    if (childA) childA->Render();
    if (childB) childB->Render();
}

// ----------------------------------------------------------------------------
// Ruteo del mouse (mismo recorrido que FindViewportUnderMouse de PC, sin las
// llamadas a SDL_SetCursor: la app decide el cursor segun el Kind devuelto)
// ----------------------------------------------------------------------------

static bool W3dIsInside(const W3dViewportBase* v, int mx, int my) {
    return mx >= v->x && mx < v->x + v->width &&
           my >= v->y && my < v->y + v->height;
}

static bool W3dIsInPadding(const W3dViewportBase* a, const W3dViewportBase* b,
                           bool isRow, int mx, int my) {
    if (!a || !b) return false;

    if (isRow) {
        int splitX = a->x + a->width;
        if (mx >= splitX - W3dPaddingViewport && mx < splitX + W3dPaddingViewport &&
            my >= a->y && my < a->y + a->height) {
            return true;
        }
    } else {
        int splitY = a->y + a->height;
        if (my >= splitY - W3dPaddingViewport && my < splitY + W3dPaddingViewport &&
            mx >= a->x && mx < a->x + a->width) {
            return true;
        }
    }
    return false;
}

W3dViewportBase* W3dFindViewportUnderMouse(W3dViewportBase* vp, int mx, int my) {
    if (!vp) return 0;

    if (vp->Kind() == W3dView_Row) {
        W3dViewportRow* row = (W3dViewportRow*)vp;
        if (W3dIsInPadding(row->childA, row->childB, true, mx, my))
            return vp;
        if (row->childA && W3dIsInside(row->childA, mx, my))
            return W3dFindViewportUnderMouse(row->childA, mx, my);
        if (row->childB && W3dIsInside(row->childB, mx, my))
            return W3dFindViewportUnderMouse(row->childB, mx, my);
    }
    else if (vp->Kind() == W3dView_Column) {
        W3dViewportColumn* col = (W3dViewportColumn*)vp;
        if (W3dIsInPadding(col->childA, col->childB, false, mx, my))
            return vp;
        if (col->childA && W3dIsInside(col->childA, mx, my))
            return W3dFindViewportUnderMouse(col->childA, mx, my);
        if (col->childB && W3dIsInside(col->childB, mx, my))
            return W3dFindViewportUnderMouse(col->childB, mx, my);
    }
    else if (vp->Contains(mx, my)) {
        // hoja: el borde (padding) no cuenta como "adentro"
        if (mx <= vp->x + W3dPaddingViewport || mx >= vp->x + vp->width - W3dPaddingViewport ||
            my <= vp->y + W3dPaddingViewport || my >= vp->y + vp->height - W3dPaddingViewport) {
            return 0;
        }
        return vp;
    }

    return 0;
}
