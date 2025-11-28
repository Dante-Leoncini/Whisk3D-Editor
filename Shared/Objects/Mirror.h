class Mirror : public Object, public Modifier {
    public:
		std::string targetName = "";
		Object* target = nullptr;

        Mirror(Object* parent): Object(parent, "Mirror"){
            IconType = static_cast<size_t>(IconType::mirror);
        }

        void SetTarget(const std::string& NewValue){
            targetName = NewValue;
        }

        ObjectType getType() override{
            return ObjectType::mirror;
        }

        void Reload() override {
            ReloadTarget();
        }

        void RenderObject() override {
            if (!target) return;
            
            glPushMatrix();
            glScalef(scaleX, scaleZ, scaleY);
            
            target->RenderObject();

            glPopMatrix();
        }

		~Mirror() {
			delete name;
		}
};