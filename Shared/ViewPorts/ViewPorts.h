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

class Viewport {
	public:
		View type = View::ViewPort3D;
		int x = 0, y = 0;
		int width = 0, height = 0;
        float weightX = 1;
        float weightY = 1;
		int Parent = -1;
		int ChildA = -1; //antiguamente era el ID. pero se podria reutilizar como ChildA y para ahorrar memoria son lo mismo
		int ChildB = -1; //si esta solo. puede quedar en -1
        bool redibujar = true;
};
std::vector<Viewport> Viewports;
int viewPortActive = -1;
bool ViewPortClickDown = false;

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
    
// índices de los 8 quads que forman el borde (sin centro)
GLubyte indices[] = {
    0,1, 4, 1,4, 5,   1, 2, 5, 5, 2, 6,   2, 3, 6, 6, 3, 7,    
    4,5, 8, 8,5, 9,                       6, 7,10,10, 7,11,
    8,9,12,12,9,13,   9,10,13,13,10,14,  10,11,14,14,11,15
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

void ResizeBorder(GLshort* borderMesh, int viewW, int viewH) {
    GLshort U[4] = { 0, (GLshort)(6*GlobalScale), (GLshort)(viewW - 6*GlobalScale), (GLshort)(viewW) };
    GLshort V[4] = { 0, (GLshort)(6*GlobalScale), (GLshort)(viewH - 6*GlobalScale), (GLshort)(viewH) };

    // Generar los 16 pares UV (fila × columna)
    int k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            borderMesh[k++] = U[x];
            borderMesh[k++] = V[y];
        }
    }
}

void DibujarBordes(const GLshort* borderMesh) {
    glTexCoordPointer(2, GL_FLOAT, 0, bourderUV);
    glVertexPointer(2, GL_SHORT, 0, borderMesh);
    //glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_BYTE, indices);
    glDrawElements(GL_TRIANGLES, (3*2)*8, GL_UNSIGNED_BYTE, indices);
}

int FindViewportUnderMouse(int mx, int my) {
    if (ViewPortClickDown)
        return viewPortActive;

    int oglY = winH - my;

    for (size_t i = 0; i < Viewports.size(); i++) {
        const Viewport& v = Viewports[i];

        // Ignorar contenedores
        if (v.type == View::Row || v.type == View::Column)
            continue;

        // Detectar si el mouse está dentro del área de este viewport
        if (mx >= v.x && mx < v.x + v.width &&
            oglY >= v.y && oglY < v.y + v.height)
        {
            // 👇 devolver el índice del viewport real
            return (int)i;
        }
    }

    return -1;
}

#include "./ViewPort3D.h"
#include "./Outliner.h"

int AddViewport(View type, int parent, int width, int height, int x, int y, float weightX, float weightY, bool NewViewPort = true) {
    Viewport view;
    view.type = type;
    view.x = x;
    view.y = y;
    view.width = width;
    view.height = height;
    view.weightX = weightX;
    view.weightY = weightY;
    view.Parent = parent;
    view.ChildA = -1;
    view.ChildB = -1;
    //view.aspect = (float)width / (float)height;

    if (NewViewPort){
        if (type == View::ViewPort3D){
            view.ChildA = AddViewport3D(Viewports.size());
        }
        else if (type == View::Outliner){
            view.ChildA = AddOutliner(Viewports.size());
        }
    }

    Viewports.push_back(view);
    //std::cout << "creado viewport: " << (Viewports.size()) << std::endl;
	return Viewports.size() -1;
}

void SplitView(int id, View split){
    float weightX = 1.0f;
    float weightY = 1.0f;
    switch (split){
        case View::Row: {
            weightX = weightX/2;
            break;
        }
        case View::Column: {
            weightY = weightY/2;
            break;
        }
        default:
            break;
    }

    int newIdLeft = AddViewport(
        Viewports[id].type, 
        id,
        Viewports[id].width/2,
        Viewports[id].height,
        Viewports[id].width/2 + Viewports[id].x,
        Viewports[id].y,
        weightX,
        weightY,
        false
    );
    //ChildA hereda el ChildA de ViewPort dividido
    Viewports[newIdLeft].ChildA = Viewports[id].ChildA;
    Viewports3D[Viewports[id].ChildA].Parent = newIdLeft;

    int newIdRight = AddViewport(
        Viewports[id].type, 
        id, 
        Viewports[id].width/2,
        Viewports[id].height,
        Viewports[id].width/2 + Viewports[id].x,
        Viewports[id].y,
        weightX,
        weightY,
        true
    );

    //Los llamo A y 
    Viewports[id].type = split;
    Viewports[id].ChildA = newIdLeft;
    Viewports[id].ChildB = newIdRight;
    //std::cout << "creado viewport ChildA: " << Viewports[id].ChildA << std::endl;
    //std::cout << "creado viewport ChildB: " << Viewports[id].ChildB << std::endl;
    //std::cout << "Count de Vieports3D: " << Viewports3D.size() << std::endl;
}

void SetWidthViewport(int id, int width){
    if (Viewports[id].Parent != -1){
        Viewport& viewParent = Viewports[Viewports[id].Parent];    
        Viewport& viewA = Viewports[viewParent.ChildA];  
        viewA.width = width;
        viewA.weightX = (float)viewA.width / (float)viewParent.width;

        Viewport& viewB = Viewports[viewParent.ChildB];  
        viewB.width = viewParent.width - viewA.width;
        viewB.weightX = (float)viewB.width / (float)viewParent.width;
    }
}

void SetHeightViewport(int id, int height){
    if (Viewports[id].Parent != -1){
        Viewport& viewParent = Viewports[Viewports[id].Parent];    
        Viewport& viewA = Viewports[viewParent.ChildA];  
        viewA.height = height;
        viewA.weightY = (float)viewA.height / (float)viewParent.height;

        Viewport& viewB = Viewports[viewParent.ChildB];  
        viewB.height = viewParent.height - viewA.height;
        viewB.weightY = (float)viewB.height / (float)viewParent.height;
    }
}

void OnResizeViewport(int id){
    Viewport& viewActual = Viewports[id];  
    switch (viewActual.type){
        case View::ViewPort3D: {
            Viewports3D[viewActual.ChildA].OnResize();
            break;
        }
        case View::Outliner: {
            Outliners[viewActual.ChildA]->OnResize();
            break;
        }
        case View::Column: {
            Viewport& viewA = Viewports[viewActual.ChildA];  
            viewA.x = viewActual.x;
            viewA.y = viewActual.y;
            viewA.width = viewActual.width;
            viewA.height = viewActual.height*viewA.weightY;

            Viewport& viewB = Viewports[viewActual.ChildB];  
            viewB.x = viewActual.x;
            viewB.y = viewA.y + viewA.height;
            viewB.width = viewActual.width;
            viewB.height = viewActual.height - viewA.height;

            OnResizeViewport(viewActual.ChildA);
            OnResizeViewport(viewActual.ChildB);
            break;
        }
        case View::Row: {
            Viewport& viewA = Viewports[viewActual.ChildA];  
            viewA.x = viewActual.x;
            viewA.y = viewActual.y;
            viewA.width = viewActual.width*viewA.weightX;
            viewA.height = viewActual.height;

            Viewport& viewB = Viewports[viewActual.ChildB];  
            viewB.x = viewA.x + viewA.width;
            viewB.y = viewActual.y;
            viewB.width = viewActual.width - viewA.width;
            viewB.height = viewActual.height;

            OnResizeViewport(viewActual.ChildA);
            OnResizeViewport(viewActual.ChildB);
            break;
        }
        default:
            break;
    }
}

void OnResizeViewports(int w, int h){
    winW = w;
    winH = h;

    //siempre arranca renderizando al padre. y recursivamente va renderizando a los hijos 
    Viewport& BaseView = Viewports[0];      
    BaseView.x = 0;
    BaseView.y = 0;
    BaseView.width = w;
    BaseView.height = h;
    OnResizeViewport(0);
}

void RenderViewports(int VievId = 0){
    //siempre arranca renderizando al padre. y recursivamente va renderizando a los hijos 
    Viewport& view = Viewports[VievId];    
    switch (view.type) {
        case View::ViewPort3D: {
            //std::cout << "Es un ViewPort3D " << (VievId+1) << std::endl;
            Viewports3D[view.ChildA].Render();
            break;
        }
        case View::Outliner: {
            //std::cout << "Es un Outliner " << (VievId+1) << std::endl;
            Outliners[view.ChildA]->Render();
            break;
        }
        case View::Column:
        case View::Row: {
            //std::cout << "Es un Row/column " << (VievId+1) << std::endl;
            RenderViewports(view.ChildA);
            RenderViewports(view.ChildB);
            break;
        }
        default:
            break;
    }
}

void SetViewPort(int id, View type){
    Viewports[id].type = type; 

    switch (type) {
        case View::Outliner: {
            Viewports[id].ChildA = AddOutliner(id);
            //Outliners[Viewports[id].ChildA].InitOutliner();
            break;
        }
        default:
            break;
    }
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