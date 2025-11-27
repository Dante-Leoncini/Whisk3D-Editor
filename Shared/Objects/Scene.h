#pragma once

// Forward declaration
class Scene;

// Definición de variable global
Scene* scene = nullptr;

class Scene : public Object {
    public:
        bool limpiarPantalla = true;
        GLfloat backgroundColor[4]  = { 0.0f, 0.0f, 0.0f, 0.0f};

        void SetLimpiarPantalla(){
            limpiarPantalla = !limpiarPantalla;
        }

        void SetBackground(GLfloat R, GLfloat G, GLfloat B, GLfloat A){
            backgroundColor[0] = R;
            backgroundColor[1] = G;
            backgroundColor[2] = B;
            backgroundColor[3] = A;
        }

        Scene()
            : Object(nullptr, "Scene Collection") {
            IconType = static_cast<size_t>(IconType::archive);
            scene = this;
        }

        ObjectType getType() override{
            return ObjectType::collection;
        }

		~Scene() {
			delete name;
		}
};