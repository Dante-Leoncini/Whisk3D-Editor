#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "ViewPorts/ScrollBar.h"
#include "WhiskUI/draw/glesdraw.h"

GLubyte indicesScrollbar[] = {
    0,1,2, 2,1,3,
    2,3,4, 4,3,5,
    4,5,6, 6,5,7
};

GLfloat ScrollbarUV[16] = {
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f
};

GLfloat ScrollbarBigUV[16] = {
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f
};

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

// ------------------ Constructor ------------------
Scrollable::Scrollable()
    : PosX(0), PosY(0), MaxPosX(100), MaxPosY(0),
      scrollX(false), scrollY(false),
      mouseOverScrollY(false), mouseOverScrollX(false),
      mouseOverScrollYpress(false), mouseOverScrollXpress(false),
      scrollPosFactor(0.0f), scrollDragFactor(0.0f),
      scrollPosFactorX(0.0f), scrollDragFactorX(0.0f),
      viewPortActive(NULL)
{
    scrollTopOffset = 0;
    static const GLshort KDef[16] = {
        0,0,   6,0,   12,0,   18,0,
        0,6,   6,6,   12,6,   18,6
    };
    for (int i = 0; i < 16; i++) {
        scrollVerticalMesh[i] = KDef[i];
        scrollVerticalBigMesh[i] = KDef[i];
        scrollHorizontalMesh[i] = KDef[i];
        scrollHorizontalBigMesh[i] = KDef[i];
    }
}

// ------------------ Scroll Y ------------------
void Scrollable::ScrollY(int dy){
    if (leftMouseDown && mouseOverScrollY) {
        PosY -= dy * scrollDragFactor;
        if (PosY > 0){PosY = 0;}
        if (MaxPosY > PosY){PosY = MaxPosY;}
    }
    else if (middleMouseDown || MouseWheel) {
        PosY += dy;
        if (PosY > 0){PosY = 0;}
        if (MaxPosY > PosY){PosY = MaxPosY;}
    }
    //std::cout << "ScrollY: " << dy << std::endl;
    //std::cout << "ahora PosX: " << PosX << " PosY: " << PosY << std::endl;
}

// ------------------ Scroll X ------------------
void Scrollable::ScrollX(int dx){
    if (leftMouseDown && mouseOverScrollX) {
        PosX += dx * scrollDragFactorX;
        if (PosX > 0) PosX = 0;
        if (MaxPosX > PosX) PosX = MaxPosX;
    }
    else if (middleMouseDown) {
        PosX += dx;
        if (PosX > 0) PosX = 0;
        if (MaxPosX > PosX) PosX = MaxPosX;
    }
}

// ------------------ Scroll por TOUCH (arrastrar el contenido con el dedo, 1:1) ------------------
void Scrollable::ScrollByTouch(int dx, int dy){
    PosY += dy; if (PosY > 0) PosY = 0; if (MaxPosY > PosY) PosY = MaxPosY;
    PosX += dx; if (PosX > 0) PosX = 0; if (MaxPosX > PosX) PosX = MaxPosX;
}

// ------------------ Mouse Over ------------------
void Scrollable::ScrollMouseOver(ViewportBase* current, int mx, int my){
    // (mx, my vienen LOCALES al viewport)
    int barXStart = current->width - borderGS - 7*GlobalScale;
    int barXEnd   = current->width - borderGS - GlobalScale;
    int barYStart = borderGS + GlobalScale + scrollTopOffset;
    int barYEnd   = current->height - borderGS - GlobalScale;

    mouseOverScrollY = scrollY &&
        (mx >= barXStart && mx <= barXEnd) &&
        (my >= barYStart && my <= barYEnd);

    // la barra HORIZONTAL (abajo): mismo hover-agrandar que la vertical
    int hYStart = current->height - borderGS - 7*GlobalScale;
    int hYEnd   = current->height - borderGS - GlobalScale;
    mouseOverScrollX = scrollX && !mouseOverScrollY &&
        (my >= hYStart && my <= hYEnd) &&
        (mx >= borderGS && mx < barXStart);
}

// ------------------ Resize Scrollbar ------------------
void Scrollable::ResizeScrollbar(int width, int height, int MaxX, int MaxY,
                                 int topOffset){
    // la barra de botones de arriba achica el area visible y corre el
    // tope superior de la barra de scroll
    scrollTopOffset = topOffset;
    int alto = height - topOffset;
    MaxPosX = -MaxX + width - borderGS*2;
    MaxPosY = MaxY + alto - borderGS*2;

    if (MaxPosY > 0) MaxPosY = 0;
    if (MaxPosX > 0) MaxPosX = 0;
    if (MaxPosY > PosY) PosY = MaxPosY;
    if (MaxPosX > PosX) PosX = MaxPosX;

    scrollY = (MaxPosY < 0);
    scrollX = (MaxPosX < 0);

    int limite = borderGS + GlobalScale*5 + borderGS;

    // la VERTICAL llega siempre hasta el borde inferior; la HORIZONTAL
    // es la que cede la esquina (no llega al borde derecho si hay
    // barra vertical, para no pisarla)
    int areaScroll = borderGS + GlobalScale * 9;
    int anchoH = width - (scrollY ? areaScroll : 0); // recorrido horizontal

    // --- Scroll vertical ---
    float scrollHeight = 0.0f;
    if (scrollY) {
        float totalHeight = (float)(alto - MaxPosY);
        scrollHeight = alto * ((float)alto / totalHeight);
        if (scrollHeight < limite) scrollHeight = limite;
        if (scrollHeight > alto) scrollHeight = (float)alto;

        float rangeContenido = -MaxPosY;
        float rangeScroll = alto - scrollHeight;
        scrollPosFactor = rangeScroll / rangeContenido;
        scrollDragFactor = rangeContenido / rangeScroll;
    } else {
        scrollHeight = (float)alto;
        scrollPosFactor = 0;
        scrollDragFactor = 1;
    }

    // --- Scroll horizontal ---
    float scrollWidthGlobal = 0.0f;
    if (scrollX) {
        float totalWidth = (float)(width - MaxPosX);
        float scrollWidth = anchoH * ((float)width / totalWidth);
        if (scrollWidth < limite) scrollWidth = (float)limite;
        if (scrollWidth > anchoH) scrollWidth = (float)anchoH;

        float rangeContenidoX = -MaxPosX;
        float rangeScrollX = anchoH - scrollWidth;
        scrollPosFactorX = rangeScrollX / rangeContenidoX;
        scrollDragFactorX = rangeContenidoX / rangeScrollX;

        scrollWidthGlobal = scrollWidth;
    } else {
        scrollWidthGlobal = (float)anchoH;
        scrollPosFactorX = 0.0f;
        scrollDragFactorX = 1.0f;
    }

    // --- Generar meshes ---
    GLshort horizontalX[4] = { (GLshort)(borderGS+GlobalScale), (GLshort)(borderGS + 3*GlobalScale),
                               (GLshort)(scrollWidthGlobal - 3*GlobalScale - borderGS), (GLshort)(scrollWidthGlobal - GlobalScale - borderGS) };
    GLshort horizontalY[2] = { (GLshort)(height - GlobalScale - borderGS), (GLshort)(height - 4*GlobalScale - borderGS) };
    GLshort horizontalYbig[2] = { (GLshort)(height - GlobalScale - borderGS), (GLshort)(height - 8*GlobalScale - borderGS) };

    GLshort verticalU[2] = { (GLshort)(width - GlobalScale - borderGS), (GLshort)(width - 4*GlobalScale - borderGS) };
    GLshort verticalV[4] = { (GLshort)(topOffset + borderGS + GlobalScale), (GLshort)(topOffset + borderGS + 3*GlobalScale),
                             (GLshort)(topOffset + scrollHeight - 3*GlobalScale - borderGS), (GLshort)(topOffset + scrollHeight - GlobalScale - borderGS) };

    GLshort verticalUbig[2] = { (GLshort)(width - GlobalScale - borderGS), (GLshort)(width - 8*GlobalScale - borderGS) };
    GLshort verticalVbig[4] = { (GLshort)(topOffset + borderGS + GlobalScale), (GLshort)(topOffset + borderGS + 3*GlobalScale),
                                (GLshort)(topOffset + scrollHeight - 3*GlobalScale - borderGS), (GLshort)(topOffset + scrollHeight - GlobalScale - borderGS) };

    int k = 0;
    for(int y=0;y<4;y++){
        for(int x=0;x<2;x++){
            scrollHorizontalMesh[k]      = horizontalX[y%4];
            scrollHorizontalMesh[k+1]    = horizontalY[x%2];
            scrollHorizontalBigMesh[k]   = horizontalX[y%4];
            scrollHorizontalBigMesh[k+1] = horizontalYbig[x%2];
            scrollVerticalMesh[k]        = verticalU[x];
            scrollVerticalMesh[k+1]      = verticalV[y];
            scrollVerticalBigMesh[k]     = verticalUbig[x];
            scrollVerticalBigMesh[k+1]   = verticalVbig[y];
            k += 2;
        }
    }
}

// ------------------ Dibujar Scrollbar ------------------
void Scrollable::DibujarScrollbar(ViewportBase* current){
    if (scrollY){
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(0, (int)(-PosY * scrollPosFactor), 0);
        //si es la vista activa
        const GLfloat* uvSel;
        const GLshort* meshSel;
        if (mouseOverScrollY){
            uvSel = ScrollbarBigUV;
            meshSel = scrollVerticalBigMesh;
            if (ViewPortClickDown && mouseOverScrollYpress){
                w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
                        ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
            }
            else {
                w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
                        ListaColores[static_cast<int>(ColorID::blanco)][2], ListaColores[static_cast<int>(ColorID::blanco)][3]);
            }
        }
        else {
            uvSel = ScrollbarUV;
            meshSel = scrollVerticalMesh;
            w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1],
                    ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
        }

        W3dDrawElemsB(meshSel, uvSel, indicesScrollbar, 18);
        w3dEngine::PopMatrix();
    }
    if (scrollX){
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((int)(-PosX * scrollPosFactorX), 0, 0);
        //si es la vista activa
        const GLfloat* uvSel;
        const GLshort* meshSel;
        if (mouseOverScrollX){
            uvSel = ScrollbarBigUV;
            meshSel = scrollHorizontalBigMesh;
            if (ViewPortClickDown && mouseOverScrollXpress){
                w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
                        ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
            }
            else {
                w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
                        ListaColores[static_cast<int>(ColorID::blanco)][2], ListaColores[static_cast<int>(ColorID::blanco)][3]);
            }
        }
        else {
            uvSel = ScrollbarUV;
            meshSel = scrollHorizontalMesh;
            w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1],
                    ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
        }

        W3dDrawElemsB(meshSel, uvSel, indicesScrollbar, 18);
        w3dEngine::PopMatrix();
    }
}