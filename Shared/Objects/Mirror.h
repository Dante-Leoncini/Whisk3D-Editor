class Mirror : public Object {
    public:
		std::string targetName;

        Mirror(Object* parent): Object(parent, "Mirror"){
            IconType = static_cast<size_t>(IconType::mirror);
        }

        void SetTarget(const std::string& NewValue){
            targetName = NewValue;
        }

        ObjectType getType() override{
            return ObjectType::mirror;
        }

        void RenderObject() override {        
        }

		~Mirror() {
			delete name;
		}
};