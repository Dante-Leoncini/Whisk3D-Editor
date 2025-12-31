#ifndef PROPLIST_H
#define PROPLIST_H

#include "PropertieBase.h"
#include "UI/card.h"
#include "objects/Mesh.h"

class PropList : public PropertieBase { 
    public:
        Card* listBox;
        int selectIndex = 0;
        int originalIndex = 0;

        PropList(const std::string& Name);

        bool EditPropertie() override;
        bool Cancel() override;

        PropertyType GetType() override;
};

class PropListMeshParts : public PropList { 
    public:
        PropListMeshParts(const std::string& Name);

        Mesh* mesh = nullptr;

        void button_up() override;
        void button_down() override;
        void button_left() override;
        void button_right() override;
        
        void RenderPropertiBox(Card* propertiBox) override;
        void RenderPropertiBoxBorder(Card* propertiBox) override;
        void RenderPropertiValue(Card* propertiBox) override;
        void RenderPropertiLabel(Card* propertiBox) override;
        int Resize(int width) override;
};

#endif