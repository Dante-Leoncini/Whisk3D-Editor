class Mirror : public Object, public Modifier {
    public:
        bool mirrorX = false;
        bool mirrorY = false;
        bool mirrorZ = true;
        bool RenderChildrens = true;

        Mirror(Object* parent, bool x = false, bool y = false, bool z = true): Object(parent, "Mirror"){
            IconType = static_cast<size_t>(IconType::mirror);
            mirrorX = x;
            mirrorY = y;
            mirrorZ = z;
        }

        ObjectType getType() override{
            return ObjectType::mirror;
        }

        void Reload() override {
            ReloadTarget(this);
        }

        void RenderObject() override {
            if (!target && target != this) return;
            glFrontFace(GL_CW);     // invertir frente mientras renderea el espejo

            if (mirrorZ){         
                //glScalef(0, -1, 0);
                glPushMatrix();

                glScalef(1, -1, 1);
                glScalef(scaleX, scaleZ, scaleY);
                glTranslatef(target->posX, target->posZ, target->posY);
                glRotatef(rotX, 1, 0, 0);
                glRotatef(rotZ, 0, 1, 0);
                glRotatef(rotY, 0, 0, 1);

                target->RenderObject();  

                if(RenderChildrens){
                    for(size_t c = 0; c < target->Childrens.size(); c++)
                        target->Childrens[c]->Render();
                }

                glPopMatrix();
            }
            glFrontFace(GL_CCW);    // restaurar
        }

		~Mirror() {
			delete name;
		}
};