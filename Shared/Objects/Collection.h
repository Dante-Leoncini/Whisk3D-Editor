class Collection : public Object {
    public:
        Collection(Object* parent = nullptr)
            : Object(parent, "Collection") {
            IconType = static_cast<size_t>(IconType::archive);
        }

        ObjectType getType() override{
            return ObjectType::collection;
        }
};