#include "PropertieBase.h"

void CardTitulo(GLfloat* icon, const std::string& texto){
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

void DibujarTitulo(Object* obj){
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
    glTranslatef(0, RenglonHeightGS + gapGS, 0);
}

PropertieBase::PropertieBase(const std::string& Name): name(Name){
}

PropertieBase::~PropertieBase() {}

void PropertieBase::RenderPropertiBox(Card* propertiBox){};
void PropertieBase::RenderPropertiValue(Card* propertiBox){};
void PropertieBase::RenderPropertiLabel(Card* propertiBox){};
int PropertieBase::GetHeight(){return 0;};