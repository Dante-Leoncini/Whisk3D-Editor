#ifndef PROPLIST_H
#define PROPLIST_H

#include "PropertieBase.h"
#include "UI/card.h"
#include "objects/Mesh.h"

class PropList : public PropertieBase { 
    public:
        Card* listBox;

        PropList(const std::string& Name);

        PropertyType GetType() override;
};

class PropListMeshParts : public PropList { 
    public:
        PropListMeshParts(const std::string& Name);

        Mesh* mesh = nullptr;
        
        void RenderPropertiBox(Card* propertiBox) override;
        void RenderPropertiValue(Card* propertiBox) override;
        void RenderPropertiLabel(Card* propertiBox) override;
        int Resize(int width) override;
};

#endif