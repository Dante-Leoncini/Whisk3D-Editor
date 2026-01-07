#ifndef PROPERTIEBASE_H
#define PROPERTIEBASE_H

#include <string>
#include <GL/gl.h>

#include "UI/UI.h"
#include "UI/colores.h"
#include "UI/card.h"
#include "UI/bitmapText.h"
#include "objects/Objects.h"

enum class PropertyType {
    Base,
    Gap,
    Float,
    Int,
    Bool,
    List,
    SelectText,
    Color
};

void CardTitulo(GLfloat* icon, const std::string& texto, int maxPixels = 1920);

class PropertieBase {
    public:
        std::string name = "?";
        int width = 300;

        PropertieBase(const std::string& Name);
        virtual ~PropertieBase();

        virtual PropertyType GetType();

        bool editando = false;

        virtual void button_up();
        virtual void button_down();
        virtual void button_left();
        virtual void button_right();
        virtual bool Cancel();

        virtual bool EditPropertie();

        virtual void RenderPropertiBox(Card* propertiBox);
        virtual void RenderPropertiBoxBorder(Card* propertiBox);
        virtual void RenderPropertiValue(Card* propertiBox);
        virtual void RenderPropertiLabel(Card* propertiBox);
        virtual int Resize(int w);
};

#endif