#include "Properties.h"

// Constructor
Properties::Properties() : ViewportBase() {
    card = new Card(nullptr, 300, 300);
}
        
void Properties::Resize(int newW, int newH){
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);
    card->Resize(width-borderGS-borderGS, 100);
}

void Properties::Render(){
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
        //std::cout << "PosX: " << PosX << " x: " << x << std::endl;
        glTranslatef(PosX + borderGS, PosY + borderGS, 0);   

        DibujarTitulo(ObjActivo);

        glTranslatef(0, RenglonHeightGS + gapGS, 0);

        glColor4f(ListaColores[static_cast<int>(ColorID::gris)][0], 
                  ListaColores[static_cast<int>(ColorID::gris)][1],
                  ListaColores[static_cast<int>(ColorID::gris)][2], 1.0f);

        card->RenderObject(false);

        CardTitulo(
            IconsUV[static_cast<size_t>(IconType::arrowRight)]->uvs,
            "Prueba de Texto"
        );

        /*glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                  ListaColores[static_cast<int>(ColorID::grisUI)][1],
                  ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);

        card->RenderBorder(false);*/

        glPopMatrix(); 
    }

    //glDisable(GL_SCISSOR_TEST);
    DibujarBordes(this);
    DibujarScrollbar(this);
}

void Properties::CardTitulo(GLfloat* icon, const std::string& texto){
    glColor4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
              ListaColores[static_cast<int>(ColorID::blanco)][2], 1.0f);

    //icono de la coleccion
    glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
    glTexCoordPointer(2, GL_FLOAT, 0, icon);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //texto render        
    glPushMatrix();            
    glTranslatef(IconSizeGS + gapGS, 0, 0);  
    RenderBitmapText(texto);
    glPopMatrix(); 
}

void Properties::DibujarTitulo(Object* obj){
    glColor4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
              ListaColores[static_cast<int>(ColorID::blanco)][2], 1.0f);

    //icono de la coleccion
    glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[obj->IconType]->uvs);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //texto render        
    glPushMatrix();            
    glTranslatef(IconSizeGS + gapGS, 0, 0);  
    RenderBitmapText(obj->name);
    glPopMatrix(); 
}

void Properties::button_left(){
}

void Properties::mouse_button_up(SDL_Event &e){
}

void Properties::event_mouse_wheel(SDL_Event &e){
}

void Properties::FindMouseOver(int mx, int my){
}

void Properties::event_mouse_motion(int mx, int my) {
}

void Properties::event_key_down(SDL_Event &e){         
}

void Properties::event_key_up(SDL_Event &e){
}

void Properties::key_down_return(){
}

Properties::~Properties() {}