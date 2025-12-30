#include "PropFloat.h"

PropFloat::PropFloat(const std::string& Name, const std::string& Unit): 
    unit(Unit), PropertieBase(Name){
};

void PropFloat::RenderPropertiBox(Card* propertiBox){
    if (value){
        propertiBox->Render(false);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

void PropFloat::RenderPropertiValue(Card* propertiBox){
    if (value){
        RenderBitmapFloat(*value, textAlign::left, propertiBox->width -bordersGS, unit);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

void PropFloat::RenderPropertiLabel(Card* propertiBox){
    if (value){
        RenderBitmapText(name, textAlign::right, propertiBox->width -bordersGS);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

PropertyType PropFloat::GetType(){
    return PropertyType::Float;
}

int PropFloat::Resize(int w){
    width = w;
    return RenglonHeightGS + gapGS;
}