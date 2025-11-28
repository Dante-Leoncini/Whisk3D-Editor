class Mirror : public Object, public Modifier {
    public:
        Mirror(Object* parent): Object(parent, "Mirror"){
            IconType = static_cast<size_t>(IconType::mirror);
        }

        ObjectType getType() override{
            return ObjectType::mirror;
        }

        void Reload() override {
            ReloadTarget(this);
        }

        void RenderObject() override {
            if (!target && target != this) return;
            
            glPushMatrix();
            glScalef(scaleX, scaleZ, scaleY);
            
            target->RenderObject();   

            glPopMatrix();
        }

		~Mirror() {
			delete name;
		}
};