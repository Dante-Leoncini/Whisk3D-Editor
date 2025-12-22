#ifndef PROPERTIEBASE_H
#define PROPERTIEBASE_H

#include <string>
#include <GL/gl.h>

#include "UI/UI.h"
#include "UI/colores.h"
#include "UI/card.h"
#include "UI/bitmapText.h"
#include "objects/Objects.h"

void CardTitulo(GLfloat* icon, const std::string& texto);
void DibujarTitulo(Object* obj);

class PropertieBase {
    public:
        std::string name = "?";
        PropertieBase(const std::string& Name);
        virtual ~PropertieBase();

        virtual void Resize(int Width, int Height);
        virtual void RenderPropertiBox(Card* propertiBox);
        virtual void RenderPropertiValue(Card* propertiBox);
        virtual void RenderPropertiLabel(Card* propertiBox);
};

#endif