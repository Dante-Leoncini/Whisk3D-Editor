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

void PropFloat::RenderPropertiBoxBorder(Card* propertiBox){
    if (value){
        glTranslatef(0, -RenglonHeightGS - gapGS, 0); 
        propertiBox->RenderBorder(false);
        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
    }
};

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
};

void PropFloat::button_up(){
    *value += 0.01f;
};

void PropFloat::button_down(){
    *value -= 0.01f;
};

void PropFloat::button_left(){
    *value -= 0.1f;
};

void PropFloat::button_right(){
    *value += 0.1f;
};

bool PropFloat::Cancel(){
    std::cout << "se cancelo originalValue: "<< originalValue << "\n";
    *value = originalValue;
    std::cout << "quedo asi: "<< *value << "\n";
    editando = false;
    return editando;
};

bool PropFloat::EditPropertie(){
    editando = !editando;
    if (editando){
        originalValue = *value;
        std::cout << "se guardo originalValue: "<< originalValue << "\n";
    }
    return editando;
};

PropertyType PropFloat::GetType(){
    return PropertyType::Float;
}

int PropFloat::Resize(int w){
    width = w;
    return RenglonHeightGS + gapGS;
}