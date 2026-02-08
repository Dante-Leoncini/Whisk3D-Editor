#ifndef POPUPBASE_H
#define POPUPBASE_H

#include <string>
#include "UI/card.h"
#include "UI/colores.h"

class PopUpBase {
    public:
        std::string name = "Propertie";
        int x = 30, y = 30;
        Card* popUpWindow;

        void initView();

        virtual void Render();

        PopUpBase(const std::string& Name);
        ~PopUpBase();
};

extern PopUpBase* PopUpActive;

#endif