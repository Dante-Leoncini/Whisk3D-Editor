#include "Outliner.h"
#include <GL/gl.h>

// Constructor
Outliner::Outliner() : ViewportBase() {
    Renglon = new Rec2D();
    CantidadRenglones = 5;
}

//para hacer el calculo si o si hay que hacerlo de forma recursiva
void Outliner::CalcularRenglon(Object* obj, int* MaxPosXtemp, int* MaxPosYtemp){
    int rowWidth = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS; 
    *MaxPosYtemp -= RenglonHeightGS;
    int textWidth = obj->name.size() * LetterWidthGS;
    rowWidth += textWidth + gapGS;

    // guardar ancho máximo
    if (rowWidth > *MaxPosXtemp) *MaxPosXtemp = rowWidth;
    
    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (obj->Childrens.size() < 1 || !obj->desplegado) return;

    //std::cout << "textWidth: " << textWidth << " rowWidth: " << rowWidth << std::endl;
    for (size_t o = 0; o < obj->Childrens.size(); o++) {
        CalcularRenglon(obj->Childrens[o], MaxPosXtemp, MaxPosYtemp);
        /*int rowWidthObj = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS;
        *MaxPosYtemp -= RenglonHeightGS;

        // texto del objeto
        int textWidthObj = reinterpret_cast<Text*>(obj->Childrens[o]->name->data)->letters.size() * LetterWidthGS;
        rowWidthObj += textWidthObj + gapGS;

        if (rowWidthObj > *MaxPosXtemp) *MaxPosXtemp = rowWidthObj;
        //std::cout << "caracteres obj: " << rowWidthObj << std::endl;*/
    }
}
        
void Outliner::Resize(int newW, int newH){
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);

    Renglon->SetSize(0, 0, width, RenglonHeightGS);

    // Calcular cuántos renglones entran en la altura
    CantidadRenglones = static_cast<int>(
        std::ceil(static_cast<float>(height) / static_cast<float>(RenglonHeightGS))
    );

    int MaxPosXtemp = 0;
    int MaxPosYtemp = 0;

    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {   
        CalcularRenglon(SceneCollection->Childrens[c], &MaxPosXtemp, &MaxPosYtemp);
    }
    //este es el gap para la barra de desplazamiento de abajo
    MaxPosYtemp -= marginGS;
    //std::cout << "MaxPosXtemp: " << MaxPosXtemp << " width: " << width << std::endl;
    //std::cout << "MaxPosYtemp: "<< MaxPosYtemp << std::endl;
    //std::cout << "Ancho: " << newW << " Alto: "<< newH << std::endl;
    ResizeScrollbar(newW, newH, MaxPosXtemp, MaxPosYtemp);
}

void Outliner::Render(){
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Limpiar pantalla
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height); // igual a tu viewport
    glClearColor(ListaColores[static_cast<int>(ColorID::background)][0],
        ListaColores[static_cast<int>(ColorID::background)][1],
        ListaColores[static_cast<int>(ColorID::background)][2],
        ListaColores[static_cast<int>(ColorID::background)][3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    glViewport(x, y, width, height); // x, y, ancho, alto
    glOrtho(0, width, height, 0, -1, 1);

    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDisable( GL_CULL_FACE );
    glDisable( GL_LIGHTING );
    glDisable( GL_TEXTURE_2D );
    glDisable( GL_BLEND );
    glEnable(GL_COLOR_MATERIAL);

    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    //de aca en adelante es como antes
    glPushMatrix();        
    size_t RenglonesY = 0;
    glTranslatef(0, PosY + borderGS, 0);
    for (size_t i = 0; i < CantidadRenglones; i++) {
        glPushMatrix();                   
        glTranslatef(0, RenglonesY, 0);
        RenglonesY += RenglonHeightGS;
        // Renglón Seleccionado
        if (i % 2 == 0) {
            glColor4ub(ListaColoresUbyte[static_cast<int>(ColorID::gris)][0], ListaColoresUbyte[static_cast<int>(ColorID::gris)][1], ListaColoresUbyte[static_cast<int>(ColorID::gris)][2], 255);
        }
        else {
            // Renglón impar
            glColor4ub(ListaColoresUbyte[static_cast<int>(ColorID::background)][0], ListaColoresUbyte[static_cast<int>(ColorID::background)][1], ListaColoresUbyte[static_cast<int>(ColorID::background)][2], 255);
        }
        //RenderObject2D(*Renglon);
        Renglon->Render(false);
        glPopMatrix();
    }
    glPopMatrix();  

    glBindTexture(GL_TEXTURE_2D, Textures[0]->iID);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);  
    glEnable(GL_TEXTURE_2D);
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);      
    glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1],
            ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);

    //esto es para recortar y que no se ponga el texto encima de los ojos de la derecha
    glEnable(GL_SCISSOR_TEST);
    if (scrollX){
        glScissor(x, y+ marginGS, width - IconSizeGS - marginGS - borderGS -gapGS, height - marginGS); // igual a tu viewport - los ojos
    }
    else {
        glScissor(x, y, width - IconSizeGS - marginGS - borderGS -gapGS, height); // igual a tu viewport - los ojos
    }

    RenglonesY = 0;  
    glPushMatrix();          
    glTranslatef(marginGS + PosX, PosY + borderGS, 0);            
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++){    
        DibujarRenglon(SceneCollection->Childrens[c], !SceneCollection->Childrens[c]->visible); 
        glTranslatef(0, RenglonHeightGS, 0);     
    }
    glPopMatrix();  

    glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
    glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1], ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    RenglonesY = 0;

    glPushMatrix();   
    //no usa PosX porque los ojos siempre estan en la misma posicion en X. al borde
    glTranslatef(width - IconSizeGS - marginGS - borderGS, GlobalScale + PosY + borderGS, 0);
    
    if (scrollX){
        glScissor(x, y+ marginGS, width - marginGS - borderGS, height - marginGS); // igual a tu viewport - los ojos
    }
    else {
        glScissor(x, y, width - marginGS - borderGS, height); // igual a tu viewport - los ojos
    }
    
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {  
        DibujarOjos(SceneCollection->Childrens[c], !SceneCollection->Childrens[c]->visible);     
    }
    glPopMatrix();     
    glDisable(GL_SCISSOR_TEST);

    DibujarBordes(this);
    DibujarScrollbar(this);
}

void Outliner::DibujarRenglon(Object* obj, bool hidden){
    glPushMatrix();     
    GLfloat opacityRow = hidden ? 0.5f : 1.0f;

    if (obj == ObjActivo){
        //std::cout << "Objeto activo en el outliner: " << reinterpret_cast<Text*>(SceneCollection->Childrens[c]->name->data)->value << "\n";
        if (obj->select){
            glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
                    ListaColores[static_cast<int>(ColorID::accent)][2], opacityRow);
        }
        else {
            glColor4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
                    ListaColores[static_cast<int>(ColorID::blanco)][2], opacityRow);
        }
    }
    else if (obj->select){
        glColor4f(ListaColores[static_cast<int>(ColorID::accentDark)][0], ListaColores[static_cast<int>(ColorID::accentDark)][1],
                ListaColores[static_cast<int>(ColorID::accentDark)][2], opacityRow);
    }
    else {
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1],
                ListaColores[static_cast<int>(ColorID::grisUI)][2], opacityRow);
    }
    
    //icono desplegar
    glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño

    //si no tiene hijos. no hagas la flecha
    if (obj->Childrens.size() < 1 || !obj->desplegado){
        glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::arrowRight)]->uvs);
    }
    else {
        glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::arrow)]->uvs);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //icono de la coleccion
    glTranslatef(IconSizeGS + gapGS, 0, 0);   
    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[obj->IconType]->uvs);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //texto render                   
    glTranslatef(IconSizeGS + gapGS, 0, 0);  
    RenderBitmapText(obj->name);
    
    glPopMatrix(); 

    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (obj->Childrens.size() < 1 || !obj->desplegado) return;

    //linea
    glVertexPointer(2, GL_SHORT, 0, IconLineMesh); //todos los iconos comparten los vertices y tamaño
    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
    glPushMatrix();  
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        glTranslatef(0, RenglonHeightGS, 0);                
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glPopMatrix(); 

    //flechas
    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
    glPushMatrix();  
    DibujarLineaDesplegada(obj);
    glPopMatrix(); 

    //renglon normal
    glTranslatef(IconSizeGS + gapGS, 0, 0);    
    for (size_t o = 0; o < obj->Childrens.size(); o++){     
        glTranslatef(0, RenglonHeightGS, 0);                    
        DibujarRenglon(obj->Childrens[o],
            hidden ? true : !obj->Childrens[o]->visible);
    }
    glTranslatef(-IconSizeGS - gapGS, 0, 0);  
}

void Outliner::DibujarLineaDesplegada(Object* obj){
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        glTranslatef(0, RenglonHeightGS, 0);                
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        DibujarLineaDesplegada(obj->Childrens[o]);
    }
}

void Outliner::DibujarOjos(Object* obj, bool hidden){
    GLfloat opacityRow = hidden ? 0.5f : 1.0f;
    //std::cout << "dibujo ojo '" << reinterpret_cast<Text*>(obj->name->data)->value << "'"<< std::endl;
    if (obj->visible){
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1], ListaColores[static_cast<int>(ColorID::grisUI)][2], opacityRow);
        glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::visible)]->uvs);
    }
    else {
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1], ListaColores[static_cast<int>(ColorID::grisUI)][2], opacityRow);
        glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::hidden)]->uvs);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);                
    glTranslatef(0, RenglonHeightGS, 0); 
    
    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (obj->Childrens.size() < 1 || !obj->desplegado) return;

    for (size_t o = 0; o < obj->Childrens.size(); o++){ 
        DibujarOjos(obj->Childrens[o],
                    hidden ? true : !obj->Childrens[o]->visible);
    }
}

void Outliner::button_left(){
    if (mouseOverScrollY){
        mouseOverScrollYpress = true;
    }
}

void Outliner::mouse_button_up(SDL_Event &e){
    ViewPortClickDown = false;
    if (e.button.button == SDL_BUTTON_LEFT) {  
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
    //else if (e.button.button == SDL_BUTTON_MIDDLE) {
    //    middleMouseDown = false;
    //}
    FindMouseOver(lastMouseX,lastMouseY);
}

void Outliner::event_mouse_wheel(SDL_Event &e){
    MouseWheel = true;
    ScrollY(e.wheel.y*6*GlobalScale);
    MouseWheel = false;
}

void Outliner::FindMouseOver(int mx, int my){
    ScrollMouseOver(this, mx, my);
}

void Outliner::event_mouse_motion(int mx, int my) {
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

void Outliner::event_key_down(SDL_Event &e){
    #if SDL_MAJOR_VERSION == 2
        SDL_Keycode key = e.key.keysym.sym; //SDL2            
    #elif SDL_MAJOR_VERSION == 3
        SDL_Keycode key = e.key.key; // SDL3
    #endif
    if (e.key.repeat == 0) { 
        switch (key) {
            case SDLK_A:  
                SeleccionarTodo(true);
                break;
            case SDLK_H:
                ChangeVisibilityObj();
                break;  
            case SDLK_X:   
                if (estado == editNavegacion){
                    Eliminar(true);
                }
                break;
            case SDLK_LEFT:
                SetDesplegado(false);
                break;
            case SDLK_RIGHT:
                SetDesplegado(true);
                break;
            case SDLK_UP:
                changeSelect(SelectMode::PrevSingle, true);
                break;
            case SDLK_DOWN:
                changeSelect(SelectMode::NextSingle, true);
                break;
        };
    }            
}

void Outliner::event_key_up(SDL_Event &e){
    #if SDL_MAJOR_VERSION == 2
        SDL_Keycode key = e.key.keysym.sym; //SDL2            
    #elif SDL_MAJOR_VERSION == 3
        SDL_Keycode key = e.key.key; // SDL3
    #endif
    switch (key) {
        case SDLK_LSHIFT:
            if (ShiftCount < 20){
                changeSelect(SelectMode::NextSingle, true);
            }
            ShiftCount = 0;
            LShiftPressed = false;
            break;
        case SDLK_LALT:
            LAltPressed = false;
            break;
    }
}

void Outliner::key_down_return(){
}

Outliner::~Outliner() {
    delete Renglon;
}