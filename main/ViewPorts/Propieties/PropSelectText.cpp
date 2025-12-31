#include "PropSelectText.h"

PropSelectText::PropSelectText(const std::string& Name): 
    PropertieBase(Name){
};

void PropSelectText::RenderPropertiBox(Card* propertiBox){
    propertiBox->Render(false);
    glTranslatef(0, RenglonHeightGS + gapGS, 0); 
}

void PropSelectText::RenderPropertiBoxBorder(Card* propertiBox){
    glTranslatef(0, -RenglonHeightGS - gapGS, 0); 
    propertiBox->RenderBorder(false);
    glTranslatef(0, RenglonHeightGS + gapGS, 0); 
};

void PropSelectText::RenderPropertiValue(Card* propertiBox){
    //RenderBitmapFloat(*value, textAlign::left, propertiBox->width -bordersGS, unit);
    glTranslatef(0, RenglonHeightGS + gapGS, 0); 
}

void PropSelectText::RenderPropertiLabel(Card* propertiBox){
    RenderBitmapText(name, textAlign::right, propertiBox->width -bordersGS);
    glTranslatef(0, RenglonHeightGS + gapGS, 0); 
};

void PropSelectText::button_up(){
};

void PropSelectText::button_down(){
};

void PropSelectText::button_left(){
};

void PropSelectText::button_right(){
};

bool PropSelectText::Cancel(){
    //std::cout << "se cancelo originalValue: "<< originalValue << "\n";
    //*value = originalValue;
    //std::cout << "quedo asi: "<< *value << "\n";
    editando = false;
    return editando;
};

bool PropSelectText::EditPropertie(){
    editando = !editando;
    if (editando){
        //originalValue = *value;
        //std::cout << "se guardo originalValue: "<< originalValue << "\n";
    }
    return editando;
};

PropertyType PropSelectText::GetType(){
    return PropertyType::SelectText;
}

int PropSelectText::Resize(int w){
    width = w;
    return RenglonHeightGS + gapGS;
}