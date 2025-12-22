#include "PropGap.h"

PropGap::PropGap(const std::string& Name): 
    PropertieBase(Name){
};

void PropGap::RenderPropertiBox(Card* propertiBox){
    glTranslatef(0, gapGS, 0); 
}

void PropGap::RenderPropertiValue(Card* propertiBox){
    glTranslatef(0, gapGS, 0); 
}

void PropGap::RenderPropertiLabel(Card* propertiBox){
    glTranslatef(0, gapGS, 0); 
}

int PropGap::GetHeight(){
    return gapGS;
}