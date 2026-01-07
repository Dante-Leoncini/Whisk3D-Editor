#ifndef PROPCOLOR_H
#define PROPCOLOR_H

#include "PropertieBase.h"
#include "UI/card.h"

class PropColor : public PropertieBase { 
    public:
        PropColor(const std::string& Name);

        GLfloat* value = nullptr;   // apunta a float[4]
        GLfloat originalValue[4];
        void SetGlColor();

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