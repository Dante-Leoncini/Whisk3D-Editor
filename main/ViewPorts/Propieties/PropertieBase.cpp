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

void PropertieBase::button_up(){};
void PropertieBase::button_down(){};
void PropertieBase::button_left(){};
void PropertieBase::button_right(){};
bool PropertieBase::Cancel(){return false;};

void PropertieBase::RenderPropertiBox(Card* propertiBox){};
bool PropertieBase::EditPropertie(){return false;};
void PropertieBase::RenderPropertiBoxBorder(Card* propertiBox){};
void PropertieBase::RenderPropertiValue(Card* propertiBox){};
void PropertieBase::RenderPropertiLabel(Card* propertiBox){};
int PropertieBase::Resize(int w){return 0;};