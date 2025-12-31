#include "Properties.h"

GroupPropertie* propTransform;
GroupPropertie* propMeshParts;

void DibujarTitulo(Object* obj, int maxPixels){
    glColor4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
              ListaColores[static_cast<int>(ColorID::blanco)][2], 1.0f);

    //icono de la coleccion
    glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[obj->IconType]->uvs);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //texto render        
    glPushMatrix();            
    glTranslatef(IconSizeGS + gapGS, 0, 0);  
    RenderBitmapText(obj->name, textAlign::left, maxPixels);
    glPopMatrix(); 
    glTranslatef(0, RenglonHeightGS + gapGS, 0);
}

void ConstructorProperties(){
    propTransform = new GroupPropertie("Transform");

    propTransform->properties.push_back(new PropFloat("Location X"));
    propTransform->properties.push_back(new PropFloat("Y"));
    propTransform->properties.push_back(new PropFloat("Z"));

    propTransform->properties.push_back(new PropGap(""));

    propTransform->properties.push_back(new PropFloat("Rotation X", "°"));
    propTransform->properties.push_back(new PropFloat("Y", "°"));
    propTransform->properties.push_back(new PropFloat("Z", "°"));

    propTransform->properties.push_back(new PropGap(""));

    propTransform->properties.push_back(new PropFloat("Scale X"));
    propTransform->properties.push_back(new PropFloat("Y"));
    propTransform->properties.push_back(new PropFloat("Z"));

    GroupProperties.push_back(propTransform);                                                      

    propMeshParts = new GroupPropertie("Mesh Parts");

    propMeshParts->properties.push_back(new PropListMeshParts("Mesh Parts"));

    propMeshParts->properties.push_back(new PropBool("Texture"));
    propMeshParts->properties.push_back(new PropBool("Transparent"));
    propMeshParts->properties.push_back(new PropBool("Vertex Color"));
    propMeshParts->properties.push_back(new PropBool("Lighting"));
    propMeshParts->properties.push_back(new PropBool("Repeat"));
    propMeshParts->properties.push_back(new PropBool("Culling"));
    propMeshParts->properties.push_back(new PropBool("Depth Test"));
    

    GroupProperties.push_back(propMeshParts);
}


void Properties::RefreshPropMeshParts(){
    if (ObjActivo->getType() != ObjectType::mesh){
        propMeshParts->visible = false;
        static_cast<PropListMeshParts*>(propMeshParts->properties[0])->mesh = nullptr;
        return;
    }

    propMeshParts->visible = true;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    static_cast<PropListMeshParts*>(propMeshParts->properties[0])->mesh = mesh;
    static_cast<PropListMeshParts*>(propMeshParts->properties[0])->selectIndex = 0;

    if (mesh->materialsGroup.empty()) return;

    MaterialGroup& mg = mesh->materialsGroup[0];
    if (!mg.material) return;
    Material* material = mg.material;

    static_cast<PropBool*>(propMeshParts->properties[1])->value = &material->transparent;
    static_cast<PropBool*>(propMeshParts->properties[2])->value = &material->transparent;
    static_cast<PropBool*>(propMeshParts->properties[3])->value = &material->vertexColor;
    static_cast<PropBool*>(propMeshParts->properties[4])->value = &material->lighting;
    static_cast<PropBool*>(propMeshParts->properties[5])->value = &material->repeat;
    static_cast<PropBool*>(propMeshParts->properties[6])->value = &material->culling;
    static_cast<PropBool*>(propMeshParts->properties[7])->value = &material->depth_test;

    /*
    bool culling = true;
    bool uv8bit = false;
    int interpolacion = 0;
    Texture* texture = nullptr;
    GLfloat diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat specular[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    std::string name = "";*/
};

void Properties::RefreshTargetProperties(){
    if (!ObjActivo || ObjActivo == target ) return;

    //posicion
    static_cast<PropFloat*>(propTransform->properties[0])->value = &ObjActivo->pos.x;
    static_cast<PropFloat*>(propTransform->properties[1])->value = &ObjActivo->pos.z;
    static_cast<PropFloat*>(propTransform->properties[2])->value = &ObjActivo->pos.y;

    //rotacion
    static_cast<PropFloat*>(propTransform->properties[4])->value = &ObjActivo->rotEuler.x;
    static_cast<PropFloat*>(propTransform->properties[5])->value = &ObjActivo->rotEuler.y;
    static_cast<PropFloat*>(propTransform->properties[6])->value = &ObjActivo->rotEuler.z;

    //escala NOTA: se saltea un numero por el gap
    static_cast<PropFloat*>(propTransform->properties[8])->value = &ObjActivo->scale.x;
    static_cast<PropFloat*>(propTransform->properties[9])->value = &ObjActivo->scale.y;
    static_cast<PropFloat*>(propTransform->properties[10])->value = &ObjActivo->scale.z;

    //Mesh Parts
    RefreshPropMeshParts();

    Resize(width, height);
}

// Constructor
Properties::Properties() : ViewportBase() {
}
        
void Properties::Resize(int newW, int newH){
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);

    int WidthCard = width-borderGS-marginGS;
    int heightCard = borderGS + borderGS + borderGS + (RenglonHeightGS + gapGS)*10;
    maxPixelsTitle = WidthCard - IconSizeGS - gapGS;

    for (size_t i=0; i < GroupProperties.size(); i++){
        GroupProperties[i]->Resize(WidthCard, heightCard);
    }

    ResizeScrollbar(newW, newH, 0, -2000);
}

void Properties::Render(){
    RefreshTargetProperties();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Limpiar pantalla
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height); // igual a tu viewport
    glClearColor(
        ListaColores[static_cast<int>(ColorID::background)][0],
        ListaColores[static_cast<int>(ColorID::background)][1],
        ListaColores[static_cast<int>(ColorID::background)][2],
        ListaColores[static_cast<int>(ColorID::background)][3]
    );

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(x, y, width, height); // x, y, ancho, alto
    glOrtho(0, width, height, 0, -1, 1);

    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDisable( GL_CULL_FACE );
    glDisable( GL_LIGHTING );
    glEnable(GL_COLOR_MATERIAL);

    glBindTexture(GL_TEXTURE_2D, Textures[0]->iID);

    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);  
    glEnable(GL_TEXTURE_2D);
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);  

    if (ObjActivo){
        glPushMatrix();   
        glTranslatef(PosX + borderGS, PosY + borderGS, 0);   

        DibujarTitulo(ObjActivo, maxPixelsTitle);

        //render de los grupos de propiedades    
        for (size_t i=0; i < GroupProperties.size(); i++){
            GroupProperties[i]->Render();
        }

        //si es lampara
        /*if (ObjActivo->getType() == ObjectType::light){
            Light* light = static_cast<Light*>(ObjActivo);

            glTranslatef(0, RenglonHeightGS + marginGS, 0);
            DibujarPropiedadFloat(" Ambient R ", light->ambient[0]);

            glTranslatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         G ", light->ambient[1]);

            glTranslatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         B ", light->ambient[2]);

            glTranslatef(0, RenglonHeightGS + marginGS, 0);
            DibujarPropiedadFloat(" Diffuse R ", light->diffuse[0]);

            glTranslatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         G ", light->diffuse[1]);

            glTranslatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         B ", light->diffuse[2]);

            glTranslatef(0, RenglonHeightGS + marginGS, 0);
            DibujarPropiedadFloat("Specular R ", light->specular[0]);

            glTranslatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         G ", light->specular[1]);

            glTranslatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         B ", light->specular[2]);
        }

        //si es camara
        if (ObjActivo->getType() == ObjectType::camera){
            Camera* camera = static_cast<Camera*>(ObjActivo);

            glTranslatef(0, RenglonHeightGS + marginGS, 0);
            if (camera->Riel){
                DibujarPropiedadFloat("Tiene Riel! ", 0.0f);

                glTranslatef(0, RenglonHeightGS + marginGS, 0);
                DibujarPropiedadFloat("Offset Riel: ", camera->offsetRiel);
            }
            else {
                DibujarPropiedadFloat("No tiene Riel ", 0.0f);
            }
        }

        //si es camara
        if (ObjActivo->getType() == ObjectType::mesh){
            Mesh* mesh = static_cast<Mesh*>(ObjActivo);

            glColor4f(ListaColores[static_cast<int>(ColorID::blanco)][0], 
                    ListaColores[static_cast<int>(ColorID::blanco)][1],
                    ListaColores[static_cast<int>(ColorID::blanco)][2], 1.0f);

            glTranslatef(0, RenglonHeightGS + marginGS, 0);
            CardTitulo(
                IconsUV[static_cast<size_t>(IconType::arrow)]->uvs,
                "Materiales"
            );

            glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                    ListaColores[static_cast<int>(ColorID::grisUI)][1],
                    ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);

            for (size_t i = 0; i < mesh->materialsGroup.size(); ++i){
                Material* mat = mesh->materialsGroup[i].material;
                glTranslatef(0, RenglonHeightGS + marginGS, 0);
                DibujarPropiedadInt(mat->name, mesh->materialsGroup.size());
            }
        }*/

        glPopMatrix();
    }

    //glDisable(GL_SCISSOR_TEST);
    DibujarBordes(this);
    DibujarScrollbar(this);
}

void Properties::button_left(){
    if (!editando){
        SetOpenGroup(false);
    }
    else {
        GroupProperties[selectIndex]->button_left();
    }
}

void Properties::button_right(){
    if (!editando){
        SetOpenGroup(true);
    }
    else {
        GroupProperties[selectIndex]->button_right();
    }
}

void Properties::mouse_button_up(SDL_Event &e){
    if (!editando) ViewPortClickDown = false;
}

void Properties::event_mouse_wheel(SDL_Event &e){
    if (editando) return;
    MouseWheel = true;
    ScrollY(e.wheel.y*12*GlobalScale);
    MouseWheel = false;
}

void Properties::FindMouseOver(int mx, int my){
}

void Properties::event_mouse_motion(int mx, int my) {
    if (editando) return;

    if (middleMouseDown || leftMouseDown) {
        ViewPortClickDown = true;

        ScrollX(dx);
        ScrollY(dy);
        return;
    }
    //si no se esta haciendo click. entonces miras si el mouse esta encima de algo
    else if (scrollY){
        FindMouseOver(mx, my);
    }
}

void Properties::event_key_down(SDL_Event &e){       
    #if SDL_MAJOR_VERSION == 2
        SDL_Keycode key = e.key.keysym.sym; //SDL2            
    #elif SDL_MAJOR_VERSION == 3
        SDL_Keycode key = e.key.key; // SDL3
    #endif
    if (e.key.repeat == 0) { 
        switch (key) {
            case SDLK_LEFT:
                button_left();
                break;
            case SDLK_RIGHT:
                button_right();
                break;
            case SDLK_UP: 
                button_up();
                break;
            case SDLK_DOWN:
                button_down();
                break;
            case SDLK_RETURN:
                EnterPropertieSelect();
                break;
            case SDLK_ESCAPE:
                Cancel();
                break;
        };
    }   
    else {
        // Evento repetido por mantener apretada
        switch (key) {
            case SDLK_LEFT:
                button_left();
                break;
            case SDLK_RIGHT:
                button_right();
                break;
            case SDLK_UP: 
                button_up();
                break;
            case SDLK_DOWN:
                button_down();
                break;
        }
    }
}

void Properties::EnterPropertieSelect(){
    editando = GroupProperties[selectIndex]->EnterPropertieSelect();
    ViewPortClickDown = editando;
}

void Properties::Cancel(){
    editando = GroupProperties[selectIndex]->Cancel();
    ViewPortClickDown = editando;
};

void Properties::SetOpenGroup(bool open){
    GroupProperties[selectIndex]->open = open;
    if (!open){
        GroupProperties[selectIndex]->selectIndex = -1;
    }
    Resize(width, height);
}

void Properties::button_up(){
    if (!editando){
        PrevSelect();
    }
    else {
        GroupProperties[selectIndex]->button_up();
    }
}

void Properties::button_down(){
    if (!editando){
        NextSelect();
    }
    else {
        GroupProperties[selectIndex]->button_down();
    }
}

void Properties::NextSelect(){
    if (GroupProperties[selectIndex]->NextSelect()){
        selectIndex++;
        if (selectIndex >= static_cast<int>(GroupProperties.size())){
            selectIndex = 0;
        }
        GroupProperties[selectIndex]->selectIndex = -1;
    }
}

void Properties::PrevSelect(){
    if (GroupProperties[selectIndex]->PrevSelect()){
        selectIndex--;

        if (selectIndex < 0){
            selectIndex = static_cast<int>(GroupProperties.size()) - 1;
        }

        if (GroupProperties[selectIndex]->open){
            GroupProperties[selectIndex]->selectLastIndexProperty();
        }
        else {
            GroupProperties[selectIndex]->selectIndex = -1;
        }
    }
}

void Properties::event_key_up(SDL_Event &e){
}

void Properties::key_down_return(){
}

Properties::~Properties() {}