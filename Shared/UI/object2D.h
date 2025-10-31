enum class UI {
    text,
    empty,
    Image,
    Rectangle
};

class Object2D {
	public:
		UI type = UI::empty;
		bool visible = true;
		int x = 0, y = 0;
		int scaleX = 1;
		int scaleY = 1;
		GLubyte opacity = 255;

        Object2D* Childrens;
        size_t ChildrensCount = 0;        

        //punteros
        Object2D* Parent = nullptr;
        void* data = nullptr; // Apunta al tipo real (Rectangle*, Text*, etc.)

        ~Object2D(); // 👈 solo la declaración
};

#include "./text.h"
#include "./image.h"
#include "./rectangle.h"

Object2D::~Object2D() {
    // Liberar memoria de data según el tipo
    switch (type) {
        case UI::Rectangle:
            delete reinterpret_cast<Rectangle*>(data);
            break;
        case UI::text:
            delete reinterpret_cast<Text*>(data);
            break;
        case UI::Image:
            delete reinterpret_cast<Image*>(data);
            break;
        default:
            break;
    }

    // Si tiene hijos, liberarlos también
    if (ChildrensCount > 0 && Childrens)
        delete[] Childrens;
}

Object2D* AddObject2D(UI type = UI::empty, Object2D** outPtr = nullptr) {
    // Crear el objeto en memoria dinámica
    Object2D* obj = new Object2D();
    obj->type = type;
    obj->visible = true;
    obj->x = 0;
    obj->y = 0;
    obj->scaleX = 1;
    obj->scaleY = 1;
    obj->Parent = nullptr;
    obj->data = nullptr;

    // Asignar memoria al contenido según tipo
    switch(type) {
        case UI::Rectangle:
            obj->data = AddRectangle(*obj);
            break;

        case UI::text:
            obj->data = AddText(*obj);
            break;

        case UI::Image:
            obj->data = AddImage(*obj);
            break;

        default:
            break;
    }

    // Si se pasó un puntero externo, guardar ahí la dirección
    if (outPtr)
        *outPtr = obj;

    // Devolver el puntero
    return obj;
}

void RenderObject2D( Object2D& obj ){
	if (!obj.visible){
		return;
	}

	switch (obj.type) {
        case UI::Rectangle: {
            Rectangle* rect = static_cast<Rectangle*>(obj.data);
            if (rect) rect->Render();
            break;
        }
        case UI::text: {
            Text* txt = static_cast<Text*>(obj.data);
            if (txt) txt->Render();
            break;
        }
		case UI::empty:
        default:
            break;
    }
}