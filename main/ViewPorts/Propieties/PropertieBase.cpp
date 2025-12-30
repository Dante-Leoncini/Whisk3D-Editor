#include "PropertieBase.h"

void CardTitulo(GLfloat* icon, const std::string& texto, int maxPixels){
    //icono de la coleccion
    glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
    glTexCoordPointer(2, GL_FLOAT, 0, icon);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //texto render        
    glPushMatrix();            
    glTranslatef(IconSizeGS + gapGS, 0, 0);  
    RenderBitmapText(texto, textAlign::left, maxPixels);
    glPopMatrix(); 
}

PropertieBase::PropertieBase(const std::string& Name): name(Name){
}

PropertieBase::~PropertieBase() {}

PropertyType PropertieBase::GetType(){
    return PropertyType::Base;
}

void PropertieBase::RenderPropertiBox(Card* propertiBox){};
void PropertieBase::RenderPropertiValue(Card* propertiBox){};
void PropertieBase::RenderPropertiLabel(Card* propertiBox){};
int PropertieBase::Resize(int width){return 0;};