#include "GroupPropertie.h"

GroupPropertie::GroupPropertie(const std::string& Name): name(Name) {
    card = new Card(nullptr, 300, 300);
    propertiBox = new Card(nullptr, 300, RenglonHeightGS);
}

void GroupPropertie::Resize(int Width, int Height){
    width = Width;
    height = RenglonHeightGS + gapGS + borderGS;
    for (size_t i = 0; i < properties.size(); ++i){
        height += properties[i]->GetHeight();
    }
    //height = (properties.size() + 1) * (RenglonHeightGS + gapGS) + borderGS;
    card->Resize(Width, height);

    int widthProperties = width - bordersGS;
    int heightProperties = height - bordersGS;

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
        properties[i]->RenderPropertiValue(propertiBox);
    }
    glTranslatef(-propertiBox->width-borderGS, GlobalScale, 0); 
};

void GroupPropertie::RenderPropertiLabel(){
    glTranslatef(propertiBox->width - gapGS, 0, 0); 
    for (size_t i = 0; i < properties.size(); ++i){
        properties[i]->RenderPropertiLabel(propertiBox);
    }
    glTranslatef(-propertiBox->width + gapGS, GlobalScale, 0); 
};

void GroupPropertie::Render(){    
    glColor4f(ListaColores[static_cast<int>(ColorID::gris)][0], 
                ListaColores[static_cast<int>(ColorID::gris)][1],
                ListaColores[static_cast<int>(ColorID::gris)][2], 1.0f);

    card->RenderObject(false);

    glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                ListaColores[static_cast<int>(ColorID::grisUI)][1],
                ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);

    glTranslatef(borderGS, borderGS, 0);

    CardTitulo(
        open
            ? IconsUV[static_cast<size_t>(IconType::arrow)]->uvs
            : IconsUV[static_cast<size_t>(IconType::arrowRight)]->uvs,
        name
    );
    
    glTranslatef(0, RenglonHeightGS + gapGS, 0); 

    if (open){
        glColor4f(ListaColores[static_cast<int>(ColorID::background)][0], 
                  ListaColores[static_cast<int>(ColorID::background)][1],
                  ListaColores[static_cast<int>(ColorID::background)][2], 1.0f);

        glPushMatrix();
        RenderPropertiBox();
        glPopMatrix();


        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], 
                    ListaColores[static_cast<int>(ColorID::grisUI)][1],
                    ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);

        glPushMatrix();                    
        RenderPropertiValue();
        glPopMatrix();

        RenderPropertiLabel();
        
        /*glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        DibujarPropiedadFloat("Location X ", ObjActivo->pos.x);

        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        DibujarPropiedadFloat("         Y ", ObjActivo->pos.z);

        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        DibujarPropiedadFloat("         Z ", ObjActivo->pos.y);

        Vector3 euler = ObjActivo->rot.ToEulerYXZ();

        glTranslatef(0, RenglonHeightGS + marginGS, 0); 
        DibujarPropiedadFloat("Rotation X ", euler.x);

        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        DibujarPropiedadFloat("         Y ", euler.z);

        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        DibujarPropiedadFloat("         Z ", euler.y);

        glTranslatef(0, RenglonHeightGS + marginGS, 0); 
        DibujarPropiedadFloat("   Scale X ", ObjActivo->scale.x);

        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        DibujarPropiedadFloat("         Y ", ObjActivo->scale.z);

        glTranslatef(0, RenglonHeightGS + gapGS, 0); 
        DibujarPropiedadFloat("         Z ", ObjActivo->scale.y);*/
    }

    glTranslatef(-borderGS, borderGS + gapGS, 0);
}

std::vector<GroupPropertie*> GroupProperties;