enum class View {
    ViewPort3D,
    Outliner,
    Properties,
    UVeditor,
    Timeline,
    GraphEditor,
    Row,
    Column
};
    
GLubyte indicesBorder[] = {
    0,1, 4, 1,4, 5,   1, 2, 5, 5, 2, 6,   2, 3, 6, 6, 3, 7,    
    4,5, 8, 8,5, 9,                       6, 7,10,10, 7,11,
    8,9,12,12,9,13,   9,10,13,13,10,14,  10,11,14,14,11,15
};

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

GLfloat bourderUV[32] = {
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f
};

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

// Adelantamos la clase (por si se usa en otros headers)
class ViewportBase;

// Adelantamos la variable global (sin definirla todavía)
ViewportBase* viewPortActive = nullptr;

class ViewportBase {
	public:
		int x = 0, y = 0;
		int width = 0, height = 0;

        ViewportBase* parent = nullptr;

        ViewportBase(){}
        virtual bool Contains(int mx, int my) const {
            return (mx >= x && mx < x + width && my >= y && my < y + height);
        }
        
        virtual bool isLeaf() const { return true; } // por defecto, no tiene hijos

        virtual ~ViewportBase() {}

        // Métodos virtuales para que cada vista defina su comportamiento
        virtual void Render() = 0;
        virtual void Resize(int newW, int newH) {
            width = newW;
            height = newH;
        }

        //controles
        virtual void event_mouse_motion(int mx, int my) {}
        virtual void button_left() {}
        virtual void event_key_down(SDL_Event &e) {}
        virtual void event_mouse_wheel(SDL_Event &e) {}
        virtual void mouse_button_up(SDL_Event &e) {}

};

class Scrollable {
    public:
        int PosX = 0;
        int PosY = 0;
        int MaxPosX = 100;
        int MaxPosY = 0;
        bool scrollX = false;
        bool scrollY = false;
        bool mouseOverScrollY = false;
        bool mouseOverScrollX = false;
        bool mouseOverScrollYpress = false;
        bool mouseOverScrollXpress = false;

        float scrollPosFactor = 0.0f;
        float scrollDragFactor = 0.0f;
        float scrollPosFactorX = 0.0f;
        float scrollDragFactorX = 0.0f;

        GLshort scrollVerticalMesh[16] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6
        };

        GLshort scrollVerticalBigMesh[16] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6
        };

        GLshort scrollHorizontalMesh[16] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6
        };

        GLshort scrollHorizontalBigMesh[16] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6
        };

        void ScrollY(int dy){
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
            //std::cout << "ahora PosX: " << PosX << " PosY: " << PosY << std::endl;
        }

        void ScrollX(int dx){
            if (leftMouseDown && mouseOverScrollX) {
                PosX += dy * scrollDragFactor;  
                if (PosX > 0){PosX = 0;}
                if (MaxPosX > PosX){PosX = MaxPosX;}
            }
            else if (middleMouseDown) {
                std::cout << "PosX: " << PosX << " MaxPosX: " << MaxPosX << std::endl;
                PosX += dx;
                if (PosX > 0){PosX = 0;}
                if (MaxPosX > PosX){PosX = MaxPosX;}
            }
        }

        void ScrollMouseOver(ViewportBase* current, int mx, int my){
            mx -= current->x;
            //my -= y;

            // Área de la barra de scroll vertical
            int barXStart = current->width - borderGS - 7* GlobalScale;
            int barXEnd = current->width - borderGS - GlobalScale;
            int barYStart = borderGS + GlobalScale;
            int barYEnd = current->height - borderGS - GlobalScale;
            bool dentroX = (mx >= barXStart && mx <= barXEnd);
            bool dentroY = (my >= barYStart && my <= barYEnd);

            //std::cout << "mx: " << mx << "my: " << my << std::endl;
            mouseOverScrollY = dentroX && dentroY;
        }

        void ResizeScrollbar(int width, int height, int MaxX, int MaxY){
            MaxPosX = -MaxX + width - borderGS - borderGS;// + width - borderGS - borderGS;
            MaxPosY = MaxY + height - borderGS - borderGS;
            //evita que el contenido quede enganchado en la parte inferior y deje espacio arriba
            if (MaxPosY > 0){MaxPosY = 0;};
            if (MaxPosX > 0){MaxPosX = 0;};
            //Si hay espacio disponible. acomoda el contenido automaticamente
            if (MaxPosY > PosY){PosY = MaxPosY;};
            if (MaxPosX > PosX){PosX = MaxPosX;};

            scrollY = (MaxPosY < 0);
            scrollX = (MaxPosX < 0);


            // mínimos y valores por defecto
            int limite = borderGS + GlobalScale * 5 + borderGS;
            
            // --- Cálculo proporcional del tamaño de la barra ---
            float scrollHeight = 0.0f;
            if (MaxPosY < 0) {
                float totalHeight = height - MaxPosY;   // altura total del contenido
                float visibleHeight = height;           // lo que se ve
                scrollHeight = visibleHeight * (visibleHeight / totalHeight);
                // límite mínimo opcional
                if (scrollHeight < limite){
                    scrollHeight = limite;
                } 

                // --- Calcular factor de desplazamiento ---
                float rangeContenido = -MaxPosY; // cuánto se puede desplazar el contenido
                float rangeScroll = height - scrollHeight;
                scrollPosFactor = rangeScroll / rangeContenido;
                scrollDragFactor = rangeContenido / rangeScroll;  // barra → contenido (inverso)
            } else {
                scrollHeight = height; // por defecto (si no hay scroll)
                scrollPosFactor = 0;
                scrollDragFactor = 1;
            }

            // --- Cálculo proporcional X (ancho de la barra horizontal) ---
            float scrollWidthGlobal = 0.0f;
            if (MaxPosX < 0) {
                float totalWidth = width - MaxPosX;       // visible + overflow
                float visibleWidth = (float)width;
                float scrollWidth = visibleWidth * (visibleWidth / totalWidth);
                if (scrollWidth < limite) scrollWidth = (float)limite;

                float rangeContenidoX = -MaxPosX;             // cuánto se puede desplazar el contenido X
                float rangeScrollX = (float)width - scrollWidth; // espacio horizontal para la barra
                if (rangeContenidoX <= 0.0f || rangeScrollX <= 0.0f) {
                    scrollPosFactorX = 0.0f;
                    scrollDragFactorX = 1.0f;
                } else {
                    scrollPosFactorX = rangeScrollX / rangeContenidoX;
                    scrollDragFactorX = rangeContenidoX / rangeScrollX;
                }

                // guardamos el scrollWidth en una variable global/externa si la necesitás luego
                scrollWidthGlobal = (int)scrollWidth; // opcional, si usás una variable global
            } else {
                // no hay scroll horizontal
                scrollWidthGlobal = width; // o scrollWidth = width si preferís float
                scrollPosFactorX = 0.0f;
                scrollDragFactorX = 1.0f;
            }

            //la abrra horizontal usa lo mismo que la vertical. pero los vertices estan en otra posicion
            GLshort horizontalX[4] = { (GLshort)(borderGS+ 1*GlobalScale), (GLshort)(borderGS + 3*GlobalScale), (GLshort)(scrollWidthGlobal - 3*GlobalScale - borderGS), (GLshort)(scrollWidthGlobal - GlobalScale - borderGS) };
            GLshort horizontalY[2] = { (GLshort)(height - GlobalScale - borderGS), (GLshort)(height - 4*GlobalScale - borderGS) };

            //cambia el tamaño del borde del viewportResizeBorder
            GLshort verticalU[2] = { (GLshort)(width - GlobalScale - borderGS), (GLshort)(width - 4*GlobalScale - borderGS) };
            GLshort verticalV[4] = { (GLshort)(borderGS+ 1*GlobalScale), (GLshort)(borderGS + 3*GlobalScale), (GLshort)(scrollHeight - 3*GlobalScale - borderGS), (GLshort)(scrollHeight - GlobalScale - borderGS) };

            GLshort verticalUbig[2] = { (GLshort)(width - GlobalScale - borderGS), (GLshort)(width - 8*GlobalScale - borderGS)};
            GLshort verticalVbig[4] = { (GLshort)(borderGS+ 1*GlobalScale), (GLshort)(borderGS + 3*GlobalScale), (GLshort)(scrollHeight - 3*GlobalScale - borderGS), (GLshort)(scrollHeight - GlobalScale - borderGS) };

            // Generar los 16 pares UV (fila × columna)
            int k = 0;
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 2; x++) {
                    scrollHorizontalMesh[k++] = horizontalX[y];
                    scrollHorizontalMesh[k++] = horizontalY[x];
                    k-=2;
                    scrollVerticalMesh[k++] = verticalU[x];
                    scrollVerticalMesh[k++] = verticalV[y];
                    k-=2;
                    scrollVerticalBigMesh[k++] = verticalUbig[x];
                    scrollVerticalBigMesh[k++] = verticalVbig[y];
                }
            }
        }

        void DibujarScrollbar(ViewportBase* current){
            if (scrollY){
                glPushMatrix();          
                glTranslatef(0, (int)(-PosY * scrollPosFactor), 0);       
                //si es la vista activa
                if (current == viewPortActive && mouseOverScrollY){
                    glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarBigUV);
                    glVertexPointer(2, GL_SHORT, 0, scrollVerticalBigMesh);
                    if (ViewPortClickDown && mouseOverScrollYpress){
                        glColor4f(ListaColores[accent][0], ListaColores[accent][1],
                                ListaColores[accent][2], ListaColores[accent][3]);
                    }
                    else {
                        glColor4f(ListaColores[blanco][0], ListaColores[blanco][1],
                                ListaColores[blanco][2], ListaColores[blanco][3]);
                    }
                }
                else {
                    glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarUV);
                    glVertexPointer(2, GL_SHORT, 0, scrollVerticalMesh);
                    glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1],
                            ListaColores[grisUI][2], ListaColores[grisUI][3]);
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
                        glColor4f(ListaColores[accent][0], ListaColores[accent][1],
                                ListaColores[accent][2], ListaColores[accent][3]);
                    }
                    else {
                        glColor4f(ListaColores[blanco][0], ListaColores[blanco][1],
                                ListaColores[blanco][2], ListaColores[blanco][3]);
                    }
                }
                else {
                    glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarUV);
                    glVertexPointer(2, GL_SHORT, 0, scrollHorizontalMesh);
                    glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1],
                            ListaColores[grisUI][2], ListaColores[grisUI][3]);
                }

                glDrawElements(GL_TRIANGLES, 3*2*3, GL_UNSIGNED_BYTE, indicesScrollbar);
                glPopMatrix();
            }
        }
};

class WithBorder {
    public:
        GLshort borderMesh[32] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6,
            0,12,  6,12,  12,12,  18,12,
            0,18,  6,18,  12,18,  18,18
        };

        void DibujarBordes(ViewportBase* current) {
            //si es la vista activa
            if (current == viewPortActive)
                glColor4f(ListaColores[accent][0], ListaColores[accent][1],
                        ListaColores[accent][2], ListaColores[accent][3]);
            else
                glColor4f(ListaColores[negro][0], ListaColores[negro][1],
                        ListaColores[negro][2], ListaColores[negro][3]);

            glTexCoordPointer(2, GL_FLOAT, 0, bourderUV);
            glVertexPointer(2, GL_SHORT, 0, borderMesh);
            glDrawElements(GL_TRIANGLES, 48, GL_UNSIGNED_BYTE, indicesBorder);
        }

        void ResizeBorder(int width, int height){
            //cambia el tamaño del borde del viewportResizeBorder
            GLshort U[4] = { 0, (GLshort)(6*GlobalScale), (GLshort)(width - 6*GlobalScale), (GLshort)(width) };
            GLshort V[4] = { 0, (GLshort)(6*GlobalScale), (GLshort)(height - 6*GlobalScale), (GLshort)(height) };

            // Generar los 16 pares UV (fila × columna)
            int k = 0;
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    borderMesh[k++] = U[x];
                    borderMesh[k++] = V[y];
                }
            }
        }
};

class ViewportRow : public ViewportBase {
    public:
        ViewportBase* childA = nullptr;
        ViewportBase* childB = nullptr;
        float splitFrac = 0.5f;       

        bool isLeaf() const override {
            return !childA && !childB;
        }

        ViewportRow(ViewportBase* a = nullptr, ViewportBase* b = nullptr, float frac = 0.5f)
            : childA(a), childB(b), splitFrac(frac) {}

        ~ViewportRow(){ delete childA; delete childB; }

        void SetSizeChildrens(int move){
            int test_A = childA->width + move;
            int test_B = childB->width - move;

            //el viewport es menor a cierto tamaño y explotaria todo
            if (test_A < MinViewportWidthGS || test_B < MinViewportWidthGS) return;

            if (childA) {
                //childA->x = x;
                childA->width = childA->width + move;
                childA->Resize(childA->width, childA->height);
            }
            if (childB) {
                childB->x += move;
                childB->width = childB->width - move;
                childB->Resize(childB->width, childB->height);
            }
            splitFrac = (float)childA->width / (float)width;
        }

        void Resize(int newW, int newH) override {
            width = newW;
            height = newH;

            if (!childA && !childB) return;

            // clamp fraction
            if (splitFrac < 0.0f) splitFrac = 0.0f;
            if (splitFrac > 1.0f) splitFrac = 1.0f;

            int wA = static_cast<int>(std::round(width * splitFrac));
            int wB = width - wA;

            if (childA) {
                childA->x = x;
                childA->y = y;
                childA->width  = wA;
                childA->height = height;
                childA->Resize(wA, height);
            }
            if (childB) {
                childB->x = x + wA;
                childB->y = y;
                childB->width  = wB;
                childB->height = height;
                childB->Resize(wB, height);
            }
        }

        void Render() override {
            // If leaf, nothing to render here (or you could draw a background)
            if (isLeaf()) return;

            childA->Render();
            childB->Render();
        }

        void button_left() override {
            leftMouseDown = true;
            ViewPortClickDown = true;
        }

        void event_mouse_motion(int mx, int my) override {
            if (leftMouseDown) {
                ViewPortClickDown = true;
                SetSizeChildrens(dx);
                return;
            }
        }
};

class ViewportColumn : public ViewportBase {
    public:
        ViewportBase* childA = nullptr;
        ViewportBase* childB = nullptr;
        float splitFrac = 0.5f;    

        bool isLeaf() const override {
            return !childA && !childB;
        }

        ViewportColumn(ViewportBase* a = nullptr, ViewportBase* b = nullptr, float frac = 0.5f)
            : childA(a), childB(b), splitFrac(frac) {}

        ~ViewportColumn(){ delete childA; delete childB; }

        void Resize(int newW, int newH) override {
            width = newW;
            height = newH;
            if (!childA && !childB) return;

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

        void SetSizeChildrens(int move){
            int test_A = childA->height - move;
            int test_B = childB->height + move;

            //el viewport es menor a cierto tamaño y explotaria todo
            if (test_A < MinViewportHeightGS || test_B < MinViewportHeightGS) return;

            if (childA) {
                childA->height -= move;
                childA->Resize(childA->width, childA->height);
            }
            if (childB) {
                //se que es boludo. pero el b es el de arriba y no el de abajo
                childB->y -= move;
                childB->height += move;
                childB->Resize(childB->width, childB->height);
            }
            splitFrac = (float)childA->height / (float)height;
        }

        void Render() override {
            // If leaf, nothing to render here (or you could draw a background)
            if (isLeaf()) return;

            childA->Render();
            childB->Render();
        }

        void button_left() override {
            leftMouseDown = true;
            ViewPortClickDown = true;
        }

        void event_mouse_motion(int mx, int my) override {
            if (leftMouseDown) {
                ViewPortClickDown = true;
                SetSizeChildrens(dy);
                return;
            }
        }
};

#include "./ViewPort3D.h"
#include "./Outliner.h"

ViewportBase* rootViewport = nullptr;

/*ViewportBase* FindViewportUnderMouse(ViewportBase* vp, int mx, int my) {
    if (!vp) return nullptr;

    // Si es un ViewportRow, revisar sus hijos
    if (auto row = dynamic_cast<ViewportRow*>(vp)) {
        if (row->childA && 
            mx >= row->childA->x && mx < row->childA->x + row->childA->width &&
            my >= row->childA->y && my < row->childA->y + row->childA->height)
            return FindViewportUnderMouse(row->childA, mx, my);

        if (row->childB && 
            mx >= row->childB->x && mx < row->childB->x + row->childB->width &&
            my >= row->childB->y && my < row->childB->y + row->childB->height)
            return FindViewportUnderMouse(row->childB, mx, my);
    }

    // Si es un ViewportColumn, revisar sus hijos
    else if (auto col = dynamic_cast<ViewportColumn*>(vp)) {
        if (col->childA && 
            mx >= col->childA->x && mx < col->childA->x + col->childA->width &&
            my >= col->childA->y && my < col->childA->y + col->childA->height)
            return FindViewportUnderMouse(col->childA, mx, my);

        if (col->childB && 
            mx >= col->childB->x && mx < col->childB->x + col->childB->width &&
            my >= col->childB->y && my < col->childB->y + col->childB->height)
            return FindViewportUnderMouse(col->childB, mx, my);
    }

    // Si no es row ni column, comprobar el viewport actual
    else if (vp->Contains(mx, my)) {
        return vp;
    }

    return nullptr;
}*/

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
            // Padding horizontal entre childA y childB
            int splitX = a->x + a->width;
            if (mx >= splitX - PADDING && mx < splitX + PADDING &&
                my >= a->y && my < a->y + a->height) 
            {
                //std::cout << "[ROW] Mouse está entre A y B (zona de resize horizontal)\n";
                SDL_SetCursor(cursorScaleHorizontal);
                return true;
            }
        } else {
            // Padding vertical entre childA y childB
            int splitY = a->y + a->height;
            if (my >= splitY - PADDING && my < splitY + PADDING &&
                mx >= a->x && mx < a->x + a->width) 
            {
                //std::cout << "[COLUMN] Mouse está entre A y B (zona de resize vertical)\n";
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

        // ¿Está el mouse sobre la línea entre A y B?
        if (isInPadding(row->childA, row->childB, true))
            return vp;

        // Normal: buscar hijos
        if (row->childA && isInside(row->childA))
            return FindViewportUnderMouse(row->childA, mx, my);

        if (row->childB && isInside(row->childB))
            return FindViewportUnderMouse(row->childB, mx, my);
    }

    // -----------------------------
    // ViewportColumn (divide en filas)
    // -----------------------------
    else if (auto col = dynamic_cast<ViewportColumn*>(vp)) {

        // ¿Está el mouse sobre la línea entre A y B?
        if (isInPadding(col->childA, col->childB, false))
            return vp;

        if (col->childA && isInside(col->childA))
            return FindViewportUnderMouse(col->childA, mx, my);

        if (col->childB && isInside(col->childB))
            return FindViewportUnderMouse(col->childB, mx, my);
    }

    // Viewport final (sin hijos)
    else if (vp->Contains(mx, my)) {
        // Verificar borde del viewport base (padding externo)
        if (mx <= vp->x + PADDING || mx >= vp->x + vp->width - PADDING ||
            my <= vp->y + PADDING || my >= vp->y + vp->height - PADDING)
        {
            //std::cout << "[VIEWPORT] Mouse en el padding externo del viewport\n";
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

    for (size_t i = 0; i < Objects.size(); i++) {                  
        Objects[i]->name->scaleX = scale;                
        Objects[i]->name->scaleY = scale;    
        reinterpret_cast<Text*>(Objects[i]->name->data)->UpdateCache();    
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