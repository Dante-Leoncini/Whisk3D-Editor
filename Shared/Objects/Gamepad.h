class Gamepad : public Object, public Modifier {
    public:        
        Gamepad(Object* parent): Object(parent, "Gamepad"){
            IconType = static_cast<size_t>(IconType::gamepad);
        }

        ObjectType getType() override{
            return ObjectType::gamepad;
        }

        void Reload() override {
            ReloadTarget();
        }

        void RenderObject() override {        
        }

		~Gamepad() {
			delete name;
		}
};