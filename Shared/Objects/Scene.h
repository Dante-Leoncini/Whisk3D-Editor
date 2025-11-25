class Scene : public Object {
    public:
        Scene()
            : Object(nullptr, "Scene Collection") {
            IconType = static_cast<size_t>(IconType::archive);
        }

        ObjectType getType() override{
            return ObjectType::collection;
        }

		~Scene() {
			delete name;
		}
};