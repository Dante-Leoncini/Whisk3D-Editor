#include "PropList.h"

PropList::PropList(const std::string& Name): 
    PropertieBase(Name){
    listBox = new Card(nullptr, 300, 300);
};

PropertyType PropList::GetType(){
    return PropertyType::List;
}

PropListMeshParts::PropListMeshParts(const std::string& Name): 
    PropList(Name){
};

void PropListMeshParts::RenderPropertiBox(Card* propertiBox){
    glTranslatef(-propertiBox->width, 0, 0); 
    listBox->Render(false);
    glTranslatef(propertiBox->width, listBox->height + gapGS, 0);
}

void PropListMeshParts::RenderPropertiBoxBorder(Card* propertiBox){
    glTranslatef(-propertiBox->width, -listBox->height - gapGS, 0); 
    listBox->RenderBorder(false);
    glTranslatef(propertiBox->width, listBox->height + gapGS, 0); 
};

void PropListMeshParts::RenderPropertiValue(Card* propertiBox){
    glTranslatef(0, listBox->height + gapGS, 0);
}

void PropListMeshParts::RenderPropertiLabel(Card* propertiBox){
    if (mesh){
        glTranslatef(-propertiBox->width + gapGS + gapGS, borderGS, 0); 
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                ListaColores[static_cast<int>(ColorID::grisUI)][1],
                ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);

        for (size_t i = 0; i < mesh->materialsGroup.size(); ++i){
            if (selectIndex == i){
                glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], 
                        ListaColores[static_cast<int>(ColorID::accent)][1],
                        ListaColores[static_cast<int>(ColorID::accent)][2], 1.0f);
            }
            RenderBitmapText(mesh->materialsGroup[i].name, textAlign::left, width -bordersGS);

            if (selectIndex == i){
                glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                        ListaColores[static_cast<int>(ColorID::grisUI)][1],
                        ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);
            }
            glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        }
        glTranslatef(propertiBox->width - gapGS - gapGS, borderGS, 0); 
    }
}

void PropListMeshParts::button_left(){};
void PropListMeshParts::button_right(){};

bool PropList::Cancel(){
    selectIndex = originalIndex;
    editando = false;
    return editando;
};

bool PropList::EditPropertie(){
    editando = !editando;
    if (editando){
        originalIndex = selectIndex;
    }
    return editando;
};

void PropListMeshParts::button_up(){
    selectIndex--;
    if (selectIndex < 0) selectIndex = mesh->materialsGroup.size() - 1;
};

void PropListMeshParts::button_down(){
    selectIndex++;
    if (selectIndex >= mesh->materialsGroup.size()) selectIndex = 0;
};

int PropListMeshParts::Resize(int w){
    width = w -bordersGS;
    int altura = bordersGS;
    if (mesh){
        altura += (RenglonHeightGS + gapGS) * mesh->materialsGroup.size() - gapGS;
        //std::cout << "altura List: "<< altura << "\n";
    }

    listBox->Resize(w- bordersGS, altura);

    return altura + gapGS;
}