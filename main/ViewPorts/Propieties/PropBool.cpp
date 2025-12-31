#include "PropBool.h"

PropBool::PropBool(const std::string& Name): 
    PropertieBase(Name){
};

void PropBool::RenderPropertiBox(Card* propertiBox){
    if (value){
        propertiBox->Render(false);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

void PropBool::RenderPropertiBoxBorder(Card* propertiBox){
    if (value){
        glTranslatef(0, -RenglonHeightGS - gapGS, 0); 
        propertiBox->RenderBorder(false);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
};

void PropBool::RenderPropertiValue(Card* propertiBox){
    if (value){
        //RenderBitmapFloat(*value, textAlign::left, propertiBox->width -bordersGS);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

void PropBool::RenderPropertiLabel(Card* propertiBox){
    if (value){
        RenderBitmapText(name, textAlign::right, propertiBox->width -bordersGS);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
};

bool PropBool::EditPropertie(){
    if (value){
        *value = !*value;
    }
    return false;
};

PropertyType PropBool::GetType(){
    return PropertyType::Bool;
}

int PropBool::Resize(int w){
    if (!value) return 0;
    width = w;
    return RenglonHeightGS + gapGS;
}