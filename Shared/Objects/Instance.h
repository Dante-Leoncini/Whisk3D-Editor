class Instance : public Object, public Modifier {
    public:
        bool RenderChildrens = true;
        size_t count = 1;

        Instance(Object* parent, Object* instance = nullptr): Object(parent, "Instance"){
            IconType = static_cast<size_t>(IconType::instance);
            if (!instance) return;
            target = instance;
            posX = instance->posX;
            posY = instance->posY;
            posZ = instance->posZ;
            rotX = instance->rotX;
            rotY = instance->rotY;
            rotZ = instance->rotZ;
            scaleX = instance->scaleX;
            scaleY = instance->scaleY;
            scaleZ = instance->scaleZ;
        }

        ObjectType getType() override{
            return ObjectType::instance;
        }

        void Reload() override {
            ReloadTarget(this);
        }

        void RenderObject() override {
            if (!target && target != this) return;

            glPushMatrix();      
            for(size_t i = 0; i < count; i++){
                glPushMatrix();      

                glScalef(scaleX, scaleY, scaleZ);

                target->RenderObject();   

                if(RenderChildrens){
                    for(size_t c = 0; c < target->Childrens.size(); c++)
                        target->Childrens[c]->Render();
                }

                glPopMatrix();
                
                glTranslatef(posX, posZ, posY);
                glRotatef(rotX, 1, 0, 0);
                glRotatef(rotZ, 0, 1, 0);
                glRotatef(rotY, 0, 0, 1);
            }
            glPopMatrix();
        }

		~Instance() {
			delete name;
		}
};