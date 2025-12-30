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

    glTranslatef(propertiBox->width, RenglonHeightGS + gapGS, 0); 
}

void PropListMeshParts::RenderPropertiValue(Card* propertiBox){
    if (mesh){
        for (size_t i = 0; i < mesh->materialsGroup.size(); ++i){
            glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        }
    }
}

void PropListMeshParts::RenderPropertiLabel(Card* propertiBox){
    if (mesh){
        glTranslatef(-propertiBox->width + gapGS + gapGS, borderGS, 0); 
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                ListaColores[static_cast<int>(ColorID::grisUI)][1],
                ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);


        for (size_t i = 0; i < mesh->materialsGroup.size(); ++i){
            Material* mat = mesh->materialsGroup[i].material;
            if (mat){
                RenderBitmapText(mat->name, textAlign::left, propertiBox->width -bordersGS);
            }
            glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        }
        glTranslatef(propertiBox->width - gapGS - gapGS, 0, 0); 
    }
}

int PropListMeshParts::Resize(int width){
    int altura = bordersGS;
    if (mesh){
        altura += (RenglonHeightGS + gapGS) * mesh->materialsGroup.size() - gapGS;
        std::cout << "altura List: "<< altura << "\n";
    }

    listBox->Resize(width, altura);

    return altura + gapGS;
}