enum class View {
    ViewPort3D
};

class Viewport {
	public:
		View type = View::ViewPort3D;
		int x = 0, y = 0;
		int width = 0, height = 0;
        float aspect = 1;
		int Id = -1;
		int Parent = -1;
        bool redibujar = true;
};
std::vector<Viewport> Viewports;

#include "./ViewPort3D.h"

int AddViewport(View type, int parent, int width, int height, int x, int y) {
    Viewport view;
    view.type = type;
    view.x = x;
    view.y = y;
    view.width = width;
    view.height = height;
    view.Id = -1;
    view.Parent = parent;
    view.aspect = (float)width / (float)height;

    if (type == View::ViewPort3D){
        view.Id = AddViewport3D(Viewports.size());
    }

    Viewports.push_back(view);
	return Viewports.size() -1;
}

void OnResizeViewports(int w, int h){
    winW = w;
    winH = h;
	for (size_t v = 0; v < Viewports.size(); v++) {    
        Viewport& view = Viewports[v];    

        //significa que es el padre de todos y por lo tanto ocupa toda la pantalla
        if (view.Parent == -1){
            view.width = w;
            view.height = h;
        }
        view.aspect = (float)view.width / (float)view.height;
        view.redibujar = true;
	}
}

void RenderViewports(){
	for (size_t v = 0; v < Viewports.size(); v++) {    
        Viewport& view = Viewports[v];    
        switch (view.type) {
            case View::ViewPort3D: {
                Viewports3D[view.Id].Render();
                break;
            }
            default:
                break;
        }
	}
}