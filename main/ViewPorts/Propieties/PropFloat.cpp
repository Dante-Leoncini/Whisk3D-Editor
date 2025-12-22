#include "PropFloat.h"

PropFloat::PropFloat(const std::string& Name): 
    PropertieBase(Name){
};

void PropFloat::RenderPropertiBox(Card* propertiBox){
    if (value){
        propertiBox->Render(false);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

void PropFloat::RenderPropertiValue(Card* propertiBox){
    if (value){
        RenderBitmapFloat(*value, textAlign::left, propertiBox->width -bordersGS);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

void PropFloat::RenderPropertiLabel(Card* propertiBox){
    if (value){
        RenderBitmapText(name, textAlign::right, propertiBox->width -bordersGS);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

int PropFloat::GetHeight(){
    return RenglonHeightGS + gapGS;
}