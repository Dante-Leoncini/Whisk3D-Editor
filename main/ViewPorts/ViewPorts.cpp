#include "ViewPorts/ViewPorts.h"

// -----------------------------
// Variables globales
// -----------------------------
ViewportBase* viewPortActive = nullptr;
ViewportBase* rootViewport = nullptr;

// UVs e indices
GLubyte indicesBorder[] = {
    0,1, 4, 1,4, 5,   1, 2, 5, 5, 2, 6,   2, 3, 6, 6, 3, 7,    
    4,5, 8, 8,5, 9,   6, 7,10,10, 7,11,
    8,9,12,12,9,13,   9,10,13,13,10,14,  10,11,14,14,11,15
};

GLubyte indicesScrollbar[] = {
    0,1,2, 2,1,3,
    2,3,4, 4,3,5,
    4,5,6, 6,5,7
};

GLfloat ScrollbarUV[16] = {0};
GLfloat ScrollbarBigUV[16] = {0};
GLfloat bourderUV[32] = {0};

void CalcBorderUV(int texW, int texH) {
    GLfloat* uv = bourderUV;

    // Coordenadas UV en píxeles (borde de 13px, esquinas de 6px, centro de 1px)
    float U[4] = { 115.0f / texW, 121.0f / texW, 122.0f / texW, 128.0f / texW };
    float V[4] = { 115.0f / texH, 121.0f / texH, 122.0f / texH, 128.0f / texH };

    // Generar los 16 pares UV (fila × columna)
    int k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            uv[k++] = U[x];
            uv[k++] = V[y];
        }
    }
}

void CalcScrollUV(int texW, int texH) {
    GLfloat* uv = ScrollbarUV;
    GLfloat* uvBig = ScrollbarBigUV;

    float VerticalU[2] = { 116.0f / texW, 119.0f / texW };
    float VerticalV[4] = { 109.0f / texH, 111.0f / texH, 112.0f / texH, 114.0f / texH };
    float VerticalUbig[2] = { 120.0f / texW, 127.0f / texW };
    float VerticalVbig[4] = { 109.0f / texH, 111.0f / texH, 112.0f / texH, 114.0f / texH };

    // Generar (fila × columna)
    int k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 2; x++) {
            uv[k++] = VerticalU[x];
            uv[k++] = VerticalV[y];
            k-=2;
            uvBig[k++] = VerticalUbig[x];
            uvBig[k++] = VerticalVbig[y];
        }
    }
}

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

// ------------------ Constructor ------------------
Scrollable::Scrollable() {
    std::fill(std::begin(scrollVerticalMesh), std::end(scrollVerticalMesh), 0);
    std::fill(std::begin(scrollVerticalBigMesh), std::end(scrollVerticalBigMesh), 0);
    std::fill(std::begin(scrollHorizontalMesh), std::end(scrollHorizontalMesh), 0);
    std::fill(std::begin(scrollHorizontalBigMesh), std::end(scrollHorizontalBigMesh), 0);
}

// ------------------ Scroll Y ------------------
void Scrollable::ScrollY(int dy){
    if (leftMouseDown && mouseOverScrollY) {
        PosY -= dy * scrollDragFactor;
        if (PosY > 0) PosY = 0;
        if (MaxPosY > PosY) PosY = MaxPosY;
    }
    else if (middleMouseDown) {
        PosY += dy;
        if (PosY > 0) PosY = 0;
        if (MaxPosY > PosY) PosY = MaxPosY;
    }
}

// ------------------ Scroll X ------------------
void Scrollable::ScrollX(int dx){
    if (leftMouseDown && mouseOverScrollX) {
        PosX += dx * scrollDragFactor;
        if (PosX > 0) PosX = 0;
        if (MaxPosX > PosX) PosX = MaxPosX;
    }
    else if (middleMouseDown) {
        PosX += dx;
        if (PosX > 0) PosX = 0;
        if (MaxPosX > PosX) PosX = MaxPosX;
    }
}

// ------------------ Mouse Over ------------------
void Scrollable::ScrollMouseOver(ViewportBase* current, int mx, int my){
    mx -= current->x;
    int barXStart = current->width - borderGS - 7*GlobalScale;
    int barXEnd   = current->width - borderGS - GlobalScale;
    int barYStart = borderGS + GlobalScale;
    int barYEnd   = current->height - borderGS - GlobalScale;

    mouseOverScrollY = (mx >= barXStart && mx <= barXEnd) && (my >= barYStart && my <= barYEnd);
}

// ------------------ Resize Scrollbar ------------------
void Scrollable::ResizeScrollbar(int width, int height, int MaxX, int MaxY){
    MaxPosX = -MaxX + width - borderGS*2;
    MaxPosY = MaxY + height - borderGS*2;

    if (MaxPosY > 0) MaxPosY = 0;
    if (MaxPosX > 0) MaxPosX = 0;
    if (MaxPosY > PosY) PosY = MaxPosY;
    if (MaxPosX > PosX) PosX = MaxPosX;

    scrollY = (MaxPosY < 0);
    scrollX = (MaxPosX < 0);

    int limite = borderGS + GlobalScale*5 + borderGS;

    // --- Scroll vertical ---
    float scrollHeight = 0.0f;
    if (scrollY) {
        float totalHeight = height - MaxPosY;
        float visibleHeight = height;
        scrollHeight = visibleHeight * (visibleHeight / totalHeight);
        if (scrollHeight < limite) scrollHeight = limite;

        float rangeContenido = -MaxPosY;
        float rangeScroll = height - scrollHeight;
        scrollPosFactor = rangeScroll / rangeContenido;
        scrollDragFactor = rangeContenido / rangeScroll;
    } else {
        scrollHeight = height;
        scrollPosFactor = 0;
        scrollDragFactor = 1;
    }

    // --- Scroll horizontal ---
    float scrollWidthGlobal = 0.0f;
    if (scrollX) {
        float totalWidth = width - MaxPosX;
        float visibleWidth = (float)width;
        float scrollWidth = visibleWidth * (visibleWidth / totalWidth);
        if (scrollWidth < limite) scrollWidth = (float)limite;

        float rangeContenidoX = -MaxPosX;
        float rangeScrollX = width - scrollWidth;
        scrollPosFactorX = rangeScrollX / rangeContenidoX;
        scrollDragFactorX = rangeContenidoX / rangeScrollX;

        scrollWidthGlobal = scrollWidth;
    } else {
        scrollWidthGlobal = width;
        scrollPosFactorX = 0.0f;
        scrollDragFactorX = 1.0f;
    }

    // --- Generar meshes ---
    GLshort horizontalX[4] = { (GLshort)(borderGS+GlobalScale), (GLshort)(borderGS + 3*GlobalScale),
                               (GLshort)(scrollWidthGlobal - 3*GlobalScale - borderGS), (GLshort)(scrollWidthGlobal - GlobalScale - borderGS) };
    GLshort horizontalY[2] = { (GLshort)(height - GlobalScale - borderGS), (GLshort)(height - 4*GlobalScale - borderGS) };

    GLshort verticalU[2] = { (GLshort)(width - GlobalScale - borderGS), (GLshort)(width - 4*GlobalScale - borderGS) };
    GLshort verticalV[4] = { (GLshort)(borderGS + GlobalScale), (GLshort)(borderGS + 3*GlobalScale),
                             (GLshort)(scrollHeight - 3*GlobalScale - borderGS), (GLshort)(scrollHeight - GlobalScale - borderGS) };

    GLshort verticalUbig[2] = { (GLshort)(width - GlobalScale - borderGS), (GLshort)(width - 8*GlobalScale - borderGS) };
    GLshort verticalVbig[4] = { (GLshort)(borderGS + GlobalScale), (GLshort)(borderGS + 3*GlobalScale),
                                (GLshort)(scrollHeight - 3*GlobalScale - borderGS), (GLshort)(scrollHeight - GlobalScale - borderGS) };

    int k = 0;
    for(int y=0;y<4;y++){
        for(int x=0;x<2;x++){
            scrollHorizontalMesh[k++] = horizontalX[y%4];
            scrollHorizontalMesh[k++] = horizontalY[x%2];
            k-=2;
            scrollVerticalMesh[k++] = verticalU[x];
            scrollVerticalMesh[k++] = verticalV[y];
            k-=2;
            scrollVerticalBigMesh[k++] = verticalUbig[x];
            scrollVerticalBigMesh[k++] = verticalVbig[y];
        }
    }
}

// ------------------ Dibujar Scrollbar ------------------
void Scrollable::DibujarScrollbar(ViewportBase* current){
    if (scrollY){
        glPushMatrix();
        glTranslatef(0, -PosY*scrollPosFactor, 0);
        if (current == viewPortActive && mouseOverScrollY){
            glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarBigUV);
            glVertexPointer(2, GL_SHORT, 0, scrollVerticalBigMesh);
        } else {
            glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarUV);
            glVertexPointer(2, GL_SHORT, 0, scrollVerticalMesh);
        }
        glDrawElements(GL_TRIANGLES, 3*2*3, GL_UNSIGNED_BYTE, indicesScrollbar);
        glPopMatrix();
    }

    if (scrollX){
        glPushMatrix();
        glTranslatef(-PosX*scrollPosFactorX, 0, 0);
        if (current == viewPortActive && mouseOverScrollX){
            glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarBigUV);
            glVertexPointer(2, GL_SHORT, 0, scrollHorizontalBigMesh);
        } else {
            glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarUV);
            glVertexPointer(2, GL_SHORT, 0, scrollHorizontalMesh);
        }
        glDrawElements(GL_TRIANGLES, 3*2*3, GL_UNSIGNED_BYTE, indicesScrollbar);
        glPopMatrix();
    }
}

// ------------------ Dibujar ------------------
void WithBorder::DibujarBordes(ViewportBase* current) {
    if (current == viewPortActive)
        glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
                  ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
    else
        glColor4f(ListaColores[static_cast<int>(ColorID::negro)][0], ListaColores[static_cast<int>(ColorID::negro)][1],
                  ListaColores[static_cast<int>(ColorID::negro)][2], ListaColores[static_cast<int>(ColorID::negro)][3]);

    glTexCoordPointer(2, GL_FLOAT, 0, bourderUV);
    glVertexPointer(2, GL_SHORT, 0, borderMesh);
    glDrawElements(GL_TRIANGLES, 48, GL_UNSIGNED_BYTE, indicesBorder);
}

// ------------------ Resize ------------------
void WithBorder::ResizeBorder(int width, int height) {
    GLshort U[4] = { 0, (GLshort)(6*GlobalScale), (GLshort)(width - 6*GlobalScale), (GLshort)(width) };
    GLshort V[4] = { 0, (GLshort)(6*GlobalScale), (GLshort)(height - 6*GlobalScale), (GLshort)(height) };

    int k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            borderMesh[k++] = U[x];
            borderMesh[k++] = V[y];
        }
    }
}

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
    int test_A = childA->width + move;
    int test_B = childB->width - move;

    if (test_A < MinViewportWidthGS || test_B < MinViewportWidthGS) return;

    if (childA) {
        childA->width += move;
        childA->Resize(childA->width, childA->height);
    }
    if (childB) {
        childB->x += move;
        childB->width -= move;
        childB->Resize(childB->width, childB->height);
    }

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
    int test_A = childA->height - move;
    int test_B = childB->height + move;

    if (test_A < MinViewportHeightGS || test_B < MinViewportHeightGS) return;

    if (childA) {
        childA->height -= move;
        childA->Resize(childA->width, childA->height);
    }
    if (childB) {
        childB->y -= move;
        childB->height += move;
        childB->Resize(childB->width, childB->height);
    }

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