#ifndef PROPGAP_H
#define PROPGAP_H

#include "PropertieBase.h"
#include "UI/card.h"

class PropGap : public PropertieBase { 
    public:
        PropGap(const std::string& Name);

        PropertyType GetType() override;
        
        void RenderPropertiBox(Card* propertiBox) override;
        void RenderPropertiValue(Card* propertiBox) override;
        void RenderPropertiLabel(Card* propertiBox) override;
        int GetHeight() override;
};

#endif