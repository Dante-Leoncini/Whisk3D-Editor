enum class Object2DType {
    text,
    empty,
    Image,
    Rectangle
};

class Object2D {
	public:
        Object2D* Parent = nullptr;
		bool visible = true;
		int x = 0, y = 0;
		int scaleX = 1;
		int scaleY = 1;
		GLubyte opacity = 255;
		//colores RGB
		GLubyte color[3] = { 255, 255, 255 };
        std::vector<Object2D*> Childrens;

        Object2D(Object2D* parent): Parent(parent){}   

        virtual Object2DType getType(){
            return Object2DType::empty;
        }

        virtual void RenderObject(bool usarColorPropio = true){}

        virtual ~Object2D(){
            for (Object2D* c : Childrens){
                delete c;
            }

            Childrens.clear();
        };

        void Render(bool usarColorPropio = true){   
            if (!visible) return;

            RenderObject(usarColorPropio);

            // Procesar cada hijo
            for (size_t c = 0; c < Childrens.size(); c++) {
                Childrens[c]->Render();
            }
        }
};