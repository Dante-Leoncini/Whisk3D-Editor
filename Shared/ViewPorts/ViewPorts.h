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

int FindViewportUnderMouse(int mx, int my) {
    if (ViewPortClickDown) return viewPortActive;
    // invertir coordenada Y de SDL a OpenGL
    int oglY = winH - my;  

    for (size_t i = 0; i < Viewports.size(); i++) {
        const Viewport& v = Viewports[i];
        if (v.type != View::ViewPort3D){
            continue;
        }
        if (mx >= v.x && mx < v.x + v.width &&
            oglY >= v.y && oglY < v.y + v.height
        ) {
            return v.ChildA;
        }
    }
    return -1; // no encontró nada
}

#include "./ViewPort3D.h"
#include "./Outliner.h"

// Método para actualizar cache
void UpdatePrecalculos() {
    if (viewPortActive > -1){
        precalculado.radY = Viewports3D[viewPortActive].rotY * M_PI / 180.0f;
        precalculado.radX = Viewports3D[viewPortActive].rotX * M_PI / 180.0f;

        precalculado.cosX = cos(precalculado.radX);
        precalculado.sinX = sin(precalculado.radX);
        precalculado.cosY = cos(precalculado.radY);
        precalculado.sinY = sin(precalculado.radY);
    }
}

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
    margin = 10 * scale;
    padding = 5 * scale;
    gap = 5 * scale;
    RenglonHeight = 12 * scale;
    SetIconScale(scale);

    for (size_t i = 0; i < Collections.size(); i++) {                  
        Collections[i]->Text.scaleX = scale;                
        Collections[i]->Text.scaleY = scale;    
        reinterpret_cast<Text*>(Collections[i]->Text.data)->UpdateCache();    
    }
}