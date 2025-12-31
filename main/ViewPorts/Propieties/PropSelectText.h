#ifndef PROPSELECTTEXT_H
#define PROPSELECTTEXT_H

#include "PropertieBase.h"
#include "UI/card.h"

class PropSelectText : public PropertieBase { 
    public:
        PropSelectText(const std::string& Name);

        PropertyType GetType() override;

        void button_up() override;
        void button_down() override;
        void button_left() override;
        void button_right() override;
        bool Cancel() override;

        bool EditPropertie() override;

        void RenderPropertiBox(Card* propertiBox) override;
        void RenderPropertiBoxBorder(Card* propertiBox) override;
        void RenderPropertiValue(Card* propertiBox) override;
        void RenderPropertiLabel(Card* propertiBox) override;
        int Resize(int w) override;
};

#endif