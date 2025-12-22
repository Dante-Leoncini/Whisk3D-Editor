#ifndef PROPFLOAT_H
#define PROPFLOAT_H

#include "PropertieBase.h"
#include "UI/card.h"

class PropFloat : public PropertieBase { 
    public:
        PropFloat(const std::string& Name);

        float* value = nullptr;

        void Resize(int Width, int Height) override;
        void RenderPropertiBox(Card* propertiBox) override;
        void RenderPropertiValue(Card* propertiBox) override;
        void RenderPropertiLabel(Card* propertiBox) override;
};

#endif