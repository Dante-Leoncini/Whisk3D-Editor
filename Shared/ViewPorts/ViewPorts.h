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

GLubyte indicesScrollbarVertical[] = {
    0,1,2, 2,1,3,
    2,3,4, 4,3,5,
    4,5,6, 6,5,7
};

GLubyte indicesScrollbarHorizontal[] = {
    0,1,4, 4,1,5, 
    1,2,5, 5,2,6,
    2,3,6, 6,3,7
};

GLfloat ScrollbarHorizontalUV[16] = {
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f
};

GLfloat ScrollbarHorizontalBigUV[16] = {
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f
};

GLfloat ScrollbarVerticalUV[16] = {
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f
};

GLfloat ScrollbarVerticalBigUV[16] = {
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
    GLfloat* uv = ScrollbarVerticalUV;
    GLfloat* uvBig = ScrollbarVerticalBigUV;

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

        float scrollHeight = 0;
        float scrollPosFactor = 0;

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
            PosY += dy;  
            if (PosY > 0){PosY = 0;}
            if (MaxPosY > PosY){PosY = MaxPosY;}
            //std::cout << "ahora PosX: " << PosX << " PosY: " << PosY << std::endl;
        }

        void ScrollX(int dx){
            PosX += dx;  

            if (PosX > 0){PosX = 0;}
            //if (MaxPosX < PosX){PosX = MaxPosX;}
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
            //MaxPosX = MaxX;
            MaxPosY = MaxY + height - borderGS - borderGS;
            scrollY = (MaxPosY < 0);
            //evita que el contenido quede enganchado en la parte inferior y deje espacio arriba
            if (MaxPosY > 0){MaxPosY = 0;};
            //Si hay espacio disponible. acomoda el contenido automaticamente
            if (MaxPosY > PosY){PosY = MaxPosY;};

            // --- Cálculo proporcional del tamaño de la barra ---
            if (MaxPosY < 0) {
                float totalHeight = height - MaxPosY;   // altura total del contenido
                float visibleHeight = height;           // lo que se ve
                scrollHeight = visibleHeight * (visibleHeight / totalHeight);
                // límite mínimo opcional
                int limite = borderGS + GlobalScale*5 + borderGS;
                if (scrollHeight < limite){
                    scrollHeight = limite;
                } 

                // --- Calcular factor de desplazamiento ---
                float rangeContenido = -MaxPosY; // cuánto se puede desplazar el contenido
                float rangeScroll = height - scrollHeight;
                scrollPosFactor = rangeScroll / rangeContenido;
            } else {
                scrollHeight = height; // por defecto (si no hay scroll)
                scrollPosFactor = 0;
            }

            //cambia el tamaño del borde del viewportResizeBorder
            GLshort verticalU[2] = { (GLshort)(width - GlobalScale - borderGS), (GLshort)(width - 4*GlobalScale - borderGS) };
            GLshort verticalV[4] = { (GLshort)(borderGS+ 1*GlobalScale), (GLshort)(borderGS + 3*GlobalScale), (GLshort)(scrollHeight - 3*GlobalScale - borderGS), (GLshort)(scrollHeight - GlobalScale - borderGS) };

            GLshort verticalUbig[2] = { (GLshort)(width - GlobalScale - borderGS), (GLshort)(width - 8*GlobalScale - borderGS)};
            GLshort verticalVbig[4] = { (GLshort)(borderGS+ 1*GlobalScale), (GLshort)(borderGS + 3*GlobalScale), (GLshort)(scrollHeight - 3*GlobalScale - borderGS), (GLshort)(scrollHeight - GlobalScale - borderGS) };

            // Generar los 16 pares UV (fila × columna)
            int k = 0;
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 2; x++) {
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
                if (mouseOverScrollY){
                    glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarVerticalBigUV);
                    glVertexPointer(2, GL_SHORT, 0, scrollVerticalBigMesh);
                    glColor4f(ListaColores[accent][0], ListaColores[accent][1],
                            ListaColores[accent][2], ListaColores[accent][3]);
                }
                else {
                    glTexCoordPointer(2, GL_FLOAT, 0, ScrollbarVerticalUV);
                    glVertexPointer(2, GL_SHORT, 0, scrollVerticalMesh);
                    glColor4f(ListaColores[negro][0], ListaColores[negro][1],
                            ListaColores[negro][2], ListaColores[negro][3]);
                }

                glDrawElements(GL_TRIANGLES, 3*2*3, GL_UNSIGNED_BYTE, indicesScrollbarVertical);
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

        void Render() override {
            // If leaf, nothing to render here (or you could draw a background)
            if (isLeaf()) return;

            childA->Render();
            childB->Render();
        }
};

#include "./ViewPort3D.h"
#include "./Outliner.h"

ViewportBase* rootViewport = new ViewportRow(
    new Viewport3D(), 
    new ViewportColumn(
        new Viewport3D(), new Outliner(), 0.3f
    ),
    0.7f
);

ViewportBase* FindViewportUnderMouse(ViewportBase* vp, int mx, int my) {
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
}

void SetGlobalScale(int scale){
    GlobalScale = scale;
    marginGS = margin * scale;
    paddingGS = padding * scale;
    gapGS = gap * scale;
    RenglonHeightGS = RenglonHeight * scale;
    borderGS = border * scale;
    SetIconScale(scale);

    for (size_t i = 0; i < Collections.size(); i++) {                  
        Collections[i]->name->scaleX = scale;                
        Collections[i]->name->scaleY = scale;    
        reinterpret_cast<Text*>(Collections[i]->name->data)->UpdateCache();    
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