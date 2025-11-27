class Array : public Object {
    public:
		std::string targetName;
        
        Array(Object* parent): Object(parent, "Array"){
            IconType = static_cast<size_t>(IconType::array);
        }

        ObjectType getType() override{
            return ObjectType::array;
        }

        void SetTarget(const std::string& NewValue){
            targetName = NewValue;
        }

        void RenderObject() override {        
        }

		~Array() {
			delete name;
		}
};