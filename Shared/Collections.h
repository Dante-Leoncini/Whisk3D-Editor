class Collection {
    public:
        //size_t ObjID = 0;
		Collection* Parent = nullptr;
        std::vector<Object*> Objects;
        std::vector<Collection*> Collections;
        bool select = false;
        bool SelectActivo = false;
        Object2D* name = nullptr;

        Collection(Collection* parent, const std::string& nombre)
            : Parent(parent){
            name = AddObject2D(UI::text);
            reinterpret_cast<Text*>(name->data)->SetValue(nombre);
        }

        void DeseleccionarTodo(){
            select = false;
            //colecciones hijas
            for(size_t c=0; c < Collections.size(); c++){
                Collections[c]->DeseleccionarTodo();				
            }    
            for(size_t o=0; o < Objects.size(); o++){
                Objects[o]->DeseleccionarTodo();		
            }     
        }

		~Collection() {
			delete name;
		}
};

std::vector<Collection*> Collections;
size_t CollectionActive = 0;

void AddToCollection(size_t CollectionID, Object* obj) {
    Collections[CollectionID]->Objects.push_back(obj);
}

void DeseleccionarTodo(){
	if (InteractionMode == ObjectMode){
		for(size_t c=0; c < Collections.size(); c++){
            Collections[c]->DeseleccionarTodo();		
		}
        SelectCount = 0;
	}
}

//outliner
void UpdateOutlinerColor(){
	/*for (size_t c = 0; c < Collections.size(); c++) {
		Object& obj = *Objects[c];
		if (SelectActivo == obj.Id && obj.seleccionado){
			SetColorOutlinerText(c, ListaColoresUbyte[accent][0], ListaColoresUbyte[accent][1], ListaColoresUbyte[accent][2]);
		}
		else if (obj.seleccionado){
			SetColorOutlinerText(c, ListaColoresUbyte[accentDark][0], ListaColoresUbyte[accentDark][1], ListaColoresUbyte[accentDark][2]);
		}
		else {	
			SetColorOutlinerText(c, ListaColoresUbyte[negro][0], ListaColoresUbyte[negro][1], ListaColoresUbyte[negro][2]);	
		}
	}*/
}

void SetColorOutlinerText(size_t Id, float r, float g, float b){
    //Texts[Objects2Doutliner[Id]].SetColor(r, g, b);
}

/*void InitAllOutliners(){
    for (size_t c = 0; c < Collections.size(); c++) {
        Object& obj = Objects[c];

        int Id2D = AddObject2D(UI::text);
        Objects2Doutliner.push_back(Id2D);

        Object2D& tempObject2D = Objects2D[Id2D];
        Text& tempText = Texts[tempObject2D.Id];
        tempText.SetValue(obj.name);
        tempText.SetScaleX(2);
        tempText.SetScaleY(2);
        tempObject2D.opacity = 255;
        tempText.SetX(0);
        tempText.SetY(0);
        tempText.SetColor(255, 255, 255);
    }
    UpdateOutlinerColor();
}*/