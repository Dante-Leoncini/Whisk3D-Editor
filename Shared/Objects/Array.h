class Array : public Object, public Modifier {
    public:        
        Array(Object* parent): Object(parent, "Array"){
            IconType = static_cast<size_t>(IconType::array);
        }

        ObjectType getType() override{
            return ObjectType::array;
        }

        void Reload() override {
            ReloadTarget(this);
        }

        void RenderObject() override {        
        }

		~Array() {
			delete name;
		}
};