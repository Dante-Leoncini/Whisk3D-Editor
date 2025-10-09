enum class View {
    ViewPort3D,
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
        float aspect = 1;
		int Parent = -1;
		int ChildA = -1; //antiguamente era el ID. pero se podria reutilizar como ChildA y para ahorrar memoria son lo mismo
		int ChildB = -1; //si esta solo. puede quedar en -1
        bool redibujar = true;
};
std::vector<Viewport> Viewports;

#include "./ViewPort3D.h"

int AddViewport(View type, int parent, int width, int height, int x, int y, float weightX, float weightY) {
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
    view.aspect = (float)width / (float)height;

    if (type == View::ViewPort3D){
        view.ChildA = AddViewport3D(Viewports.size());
    }

    Viewports.push_back(view);
    //std::cout << "creado viewport: " << (Viewports.size()) << std::endl;
	return Viewports.size() -1;
}

void SplitView(int id, View split){
    float weightX = Viewports[id].weightX;
    float weightY = Viewports[id].weightY;
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
        weightY
    );
    int newIdRight = AddViewport(
        Viewports[id].type, 
        id, 
        Viewports[id].width/2,
        Viewports[id].height,
        Viewports[id].width/2 + Viewports[id].x,
        Viewports[id].y,
        weightX,
        weightY
    );

    //Los llamo A y 
    Viewports[id].type = split;
    Viewports[id].ChildA = newIdLeft;
    Viewports[id].ChildB = newIdRight;
}

void OnResizeViewport(int id){
    Viewport& viewActual = Viewports[id];  
    switch (viewActual.type){
        case View::ViewPort3D: {
            viewActual.aspect = (float)viewActual.width / (float)viewActual.height;
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
        case View::Column:
        case View::Row: {
            //std::cout << "Es un Row " << (VievId+1) << std::endl;
            RenderViewports(view.ChildA);
            RenderViewports(view.ChildB);
            break;
        }
        default:
            break;
    }
}