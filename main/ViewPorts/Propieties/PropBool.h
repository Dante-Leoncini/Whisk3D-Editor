#ifndef PROPBOOL_H
#define PROPBOOL_H

#include "PropertieBase.h"
#include "UI/card.h"

class PropBool : public PropertieBase { 
    public:
        PropBool(const std::string& Name);

        bool* value = nullptr;

        PropertyType GetType() override;

        bool EditPropertie() override;

        void RenderPropertiBox(Card* propertiBox) override;
        void RenderPropertiBoxBorder(Card* propertiBox) override;
        void RenderPropertiValue(Card* propertiBox) override;
        void RenderPropertiLabel(Card* propertiBox) override;
        int Resize(int w) override;
};

#endif