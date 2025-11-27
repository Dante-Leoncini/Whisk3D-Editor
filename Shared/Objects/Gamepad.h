class Gamepad : public Object {
    public:
		std::string targetName;
        
        Gamepad(Object* parent): Object(parent, "Gamepad"){
            IconType = static_cast<size_t>(IconType::gamepad);
        }

        ObjectType getType() override{
            return ObjectType::gamepad;
        }

        void SetTarget(const std::string& NewValue){
            targetName = NewValue;
        }

        void RenderObject() override {        
        }

		~Gamepad() {
			delete name;
		}
};