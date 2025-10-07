enum class UI {
    text,
    empty
};

class Object2D {
	public:
		UI type = UI::empty;
		bool visible = true;
		int x = 0, y = 0;
		int scaleX = 1;
		int scaleY = 1;
		float opacity = 1.0f;
		int Id = -1;
		int Parent = -1;
		std::vector<Children> Childrens;
		std::string name;
};

std::vector<Object2D> Objects2D;

#include "./text.h"

int AddObject2D(UI type = UI::empty) {
    Object2D obj;
    obj.type = type;

    // podés inicializar más campos si querés:
    obj.visible = true;
    obj.x = 0;
    obj.y = 0;
    obj.scaleX = 1;
    obj.scaleY = 1;
    obj.Id = static_cast<int>(Objects2D.size()); // id incremental
    obj.Parent = -1;
    obj.name = "Object";

	if (type == UI::text) {
        // Guardar índice en el Object2D
        obj.Id = AddText(Objects2D.size());
        obj.name = "Text" + std::to_string(obj.Id+1);
    } else {
        // Objeto normal (no texto)
        obj.Id = -1;
        obj.name = "Empty " + std::to_string(Objects2D.size());
    }

    Objects2D.push_back(obj);
	return Objects2D.size() -1;
}

void RenderObject2D( int objId ){
	Object2D& obj = Objects2D[objId];
	if (!obj.visible){
		return;
	}

	switch (obj.type) {
        case UI::text: {
			Text& txt = Texts[obj.Id];
            glColor4f(txt.r, txt.g, txt.b, obj.opacity);
			txt.Render();
            break;
		}
		case UI::empty:
        default:
            break;
    }
}