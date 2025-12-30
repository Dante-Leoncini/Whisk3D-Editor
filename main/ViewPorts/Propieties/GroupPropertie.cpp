#include "GroupPropertie.h"

bool GroupPropertie::NextSelect(){
    if (!open){        
        selectIndex = -2;
        return true;
    }

    const int size = static_cast<int>(properties.size());

    while (true){
        selectIndex++;

        // llegamos al final
        if (selectIndex >= size){
            selectIndex = -2;
            return true;
        }

        //el idice es negativo
        if (selectIndex < 0){
            selectIndex = 0;
        }

        // saltar gaps
        if (properties[selectIndex]->GetType() == PropertyType::Gap)
            continue;

        // encontrado válido
        return false;
    }
}

bool GroupPropertie::PrevSelect(){
    if (!open){        
        selectIndex = -2;
        return true;
    }

    const int size = static_cast<int>(properties.size());

    while (true){
        selectIndex--;

        // llegamos al inicio
        if (selectIndex < -1){
            return true;
        }
        if (selectIndex < 0){
            return false;
        }

        // por seguridad (simetría con NextSelect)
        if (selectIndex >= size){
            selectIndex = size - 1;
        }

        // saltar gaps
        if (properties[selectIndex]->GetType() == PropertyType::Gap)
            continue;

        // encontrado válido
        return false;
    }
}

void GroupPropertie::selectLastIndexProperty(){
    selectIndex = properties.size() - 1;
}

GroupPropertie::GroupPropertie(const std::string& Name): name(Name) {
    card = new Card(nullptr, 300, 300);
    propertiBox = new Card(nullptr, 300, RenglonHeightGS);
}

void GroupPropertie::Resize(int Width, int Height){
    width = Width;
    height = RenglonHeightGS + gapGS + borderGS;
    int widthProperties = width - bordersGS;

    if (open){
        for (size_t i = 0; i < properties.size(); ++i){
            height += properties[i]->Resize(width);
        }
    }
    card->Resize(Width, height);
    int heightProperties = height - bordersGS;
    maxPixelsTitle = widthProperties - IconSizeGS - gapGS;

    propertiBox->Resize(widthProperties/2, RenglonHeightGS+GlobalScale*2);
}

void GroupPropertie::RenderPropertiBox(){
    glTranslatef(propertiBox->width, -GlobalScale, 0); 
    for (size_t i = 0; i < properties.size(); ++i){
        properties[i]->RenderPropertiBox(propertiBox);
    }
    glTranslatef(-propertiBox->width, GlobalScale, 0); 
};

void GroupPropertie::RenderPropertiValue(){
    glTranslatef(propertiBox->width+borderGS, 0, 0); 
    for (size_t i = 0; i < properties.size(); ++i){
        if (selectIndex == i){
            glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], 
                        ListaColores[static_cast<int>(ColorID::accent)][1],
                        ListaColores[static_cast<int>(ColorID::accent)][2], 1.0f);
        }
        else {
            glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                        ListaColores[static_cast<int>(ColorID::grisUI)][1],
                        ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);
        }
        properties[i]->RenderPropertiValue(propertiBox);
    }
    glTranslatef(-propertiBox->width-borderGS, GlobalScale, 0); 
};

void GroupPropertie::RenderPropertiLabel(){
    glTranslatef(propertiBox->width - gapGS, 0, 0); 
    for (size_t i = 0; i < properties.size(); ++i){
        if (selectIndex == i){
            glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], 
                        ListaColores[static_cast<int>(ColorID::accent)][1],
                        ListaColores[static_cast<int>(ColorID::accent)][2], 1.0f);
        }
        else {
            glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                        ListaColores[static_cast<int>(ColorID::grisUI)][1],
                        ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);
        }
        properties[i]->RenderPropertiLabel(propertiBox);
    }
    glTranslatef(-propertiBox->width + gapGS, GlobalScale, 0); 
};

void GroupPropertie::Render(){  
    if (!visible) return;

    glColor4f(ListaColores[static_cast<int>(ColorID::gris)][0], 
                ListaColores[static_cast<int>(ColorID::gris)][1],
                ListaColores[static_cast<int>(ColorID::gris)][2], 1.0f);

    card->RenderObject(false);

    glTranslatef(borderGS, borderGS, 0);

    if (selectIndex == -1){
        glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], 
                    ListaColores[static_cast<int>(ColorID::accent)][1],
                    ListaColores[static_cast<int>(ColorID::accent)][2], 1.0f);
    }
    else {
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                    ListaColores[static_cast<int>(ColorID::grisUI)][1],
                    ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);
    }

    CardTitulo(
        open
            ? IconsUV[static_cast<size_t>(IconType::arrow)]->uvs
            : IconsUV[static_cast<size_t>(IconType::arrowRight)]->uvs,
        name, maxPixelsTitle
    );
    
    glTranslatef(0, RenglonHeightGS + gapGS, 0); 

    if (open){
        glColor4f(ListaColores[static_cast<int>(ColorID::background)][0], 
                  ListaColores[static_cast<int>(ColorID::background)][1],
                  ListaColores[static_cast<int>(ColorID::background)][2], 1.0f);

        glPushMatrix();
        RenderPropertiBox();
        glPopMatrix();

        glPushMatrix();                    
        RenderPropertiValue();
        glPopMatrix();

        RenderPropertiLabel();
    }

    glTranslatef(-borderGS, borderGS + gapGS, 0);
}

std::vector<GroupPropertie*> GroupProperties;