//valores globales para controles
float axisState[SDL_CONTROLLER_AXIS_MAX] = {0.0f};
bool buttonState[SDL_CONTROLLER_BUTTON_MAX] = {false};
GLfloat deadzone = 0.20f;
GLfloat velocidad = 0.05f;

void RefreshInputControllerSDL(SDL_Event &e){    
    if(e.type == SDL_CONTROLLERAXISMOTION){
        int axis = e.caxis.axis;
        float value = e.caxis.value / 32767.0f;
        axisState[axis] = (fabs(value) < deadzone) ? 0.0f : value;
    }
    else if(e.type == SDL_CONTROLLERBUTTONDOWN){
        buttonState[e.cbutton.button] = true;
    }
    else if(e.type == SDL_CONTROLLERBUTTONUP){
        buttonState[e.cbutton.button] = false;
    }
}

class Gamepad : public Object, public Modifier {
    public:      
        Gamepad(Object* parent): Object(parent, "Gamepad"){
            IconType = static_cast<size_t>(IconType::gamepad);
        }

        ObjectType getType() override{
            return ObjectType::gamepad;
        }

        void Reload() override {
            ReloadTarget(this);
        }

        void RenderObject() override {      
            Update();  
        }

        void Update();   // Solo declaración

		~Gamepad() {
			delete name;
		}
};