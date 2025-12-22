#ifndef PROPFLOAT_H
#define PROPFLOAT_H

#include "PropertieBase.h"
#include "UI/card.h"

class PropFloat : public PropertieBase { 
    public:
        PropFloat(const std::string& Name, const std::string& Unit = "");

        float* value = nullptr;
        std::string unit = "";

        void RenderPropertiBox(Card* propertiBox) override;
        void RenderPropertiValue(Card* propertiBox) override;
        void RenderPropertiLabel(Card* propertiBox) override;
        int GetHeight() override;
};

#endif