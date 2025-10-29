class Collection {
    public:
        size_t ObjID = 0;
        bool select = false;
        bool SelectActivo = false;
        Object2D& Text; // referencia directa

        Collection(size_t objID, Object2D& text)
            : ObjID(objID), Text(text) {}
};

std::vector<Collection*> Collections;

void AddToCollection(size_t objID, const std::string& name) {
    // Crear el Object2D y guardar su dirección
    Object2D* newText = AddObject2D(UI::text);

    // Crear la colección en memoria dinámica
    Collection* newCollection = new Collection(objID, *newText);

    reinterpret_cast<Object2D*>(newText)->scaleX = GlobalScale;
    reinterpret_cast<Object2D*>(newText)->scaleY = GlobalScale;
    reinterpret_cast<Text*>(newText->data)->SetValue(name);

    // Guardar el puntero, sin copiar nada
    Collections.push_back(newCollection);
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