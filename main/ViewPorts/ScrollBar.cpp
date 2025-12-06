#include "ViewPorts/ScrollBar.h"

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
Scrollable::Scrollable() {}

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
        glTranslatef(0, (int)(-PosY * scrollPosFactor), 0);       
        //si es la vista activa
        if (current == viewPortActive && mouseOverScrollY){
            glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarBigUV);
            glVertexPointer(2, GL_SHORT, 0, scrollVerticalBigMesh);
            if (ViewPortClickDown && mouseOverScrollYpress){
                glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
                        ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
            }
            else {
                glColor4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
                        ListaColores[static_cast<int>(ColorID::blanco)][2], ListaColores[static_cast<int>(ColorID::blanco)][3]);
            }
        }
        else {
            glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarUV);
            glVertexPointer(2, GL_SHORT, 0, scrollVerticalMesh);
            glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1],
                    ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
        }

        glDrawElements(GL_TRIANGLES, 3*2*3, GL_UNSIGNED_BYTE, indicesScrollbar);
        glPopMatrix();
    }
    if (scrollX){
        glPushMatrix();          
        glTranslatef((int)(-PosX * scrollPosFactorX), 0, 0);       
        //si es la vista activa
        if (current == viewPortActive && mouseOverScrollX){
            glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarBigUV);
            glVertexPointer(2, GL_SHORT, 0, scrollHorizontalBigMesh);
            if (ViewPortClickDown && mouseOverScrollXpress){
                glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
                        ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
            }
            else {
                glColor4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
                        ListaColores[static_cast<int>(ColorID::blanco)][2], ListaColores[static_cast<int>(ColorID::blanco)][3]);
            }
        }
        else {
            glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarUV);
            glVertexPointer(2, GL_SHORT, 0, scrollHorizontalMesh);
            glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1],
                    ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
        }

        glDrawElements(GL_TRIANGLES, 3*2*3, GL_UNSIGNED_BYTE, indicesScrollbar);
        glPopMatrix();
    }
}