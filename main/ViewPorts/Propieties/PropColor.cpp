#include "PropColor.h"

PropColor::PropColor(const std::string& Name): 
    PropertieBase(Name){
};

void PropColor::RenderPropertiBox(Card* propertiBox){
    if (value){
        propertiBox->Render(false);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

void PropColor::RenderPropertiBoxBorder(Card* propertiBox){
    if (value){
        glTranslatef(0, -RenglonHeightGS - gapGS, 0); 
        propertiBox->RenderBorder(false);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
};

void PropColor::RenderPropertiValue(Card* propertiBox){
    if (value){
        //RenderBitmapFloat(*value, textAlign::left, propertiBox->width -bordersGS, unit);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
}

void PropColor::RenderPropertiLabel(Card* propertiBox){
    if (value){
        RenderBitmapText(name, textAlign::right, propertiBox->width -bordersGS);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
};

void PropColor::button_up(){
    *value += 0.01f;
};

void PropColor::button_down(){
    *value -= 0.01f;
};

void PropColor::button_left(){
    *value -= 0.1f;
};

void PropColor::button_right(){
    *value += 0.1f;
};

void PropColor::SetGlColor(){
    if (!value) return;

    glColor4f(value[0], value[1], value[2], 1.0f);
};

bool PropColor::Cancel(){
    if (!value) return false;
    value[0] = originalValue[0];
    value[1] = originalValue[1];
    value[2] = originalValue[2];
    value[3] = originalValue[3];
    editando = false;
    return editando;
};

bool PropColor::EditPropertie(){
    editando = !editando;
    if (editando && value){
        originalValue[0] = value[0];
        originalValue[1] = value[1];
        originalValue[2] = value[2];
        originalValue[3] = value[3];
    }
    return editando;
}

PropertyType PropColor::GetType(){
    return PropertyType::Color;
}

int PropColor::Resize(int w){
    width = w;
    return RenglonHeightGS + gapGS;
}