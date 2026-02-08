#include "ColorPicker.h"

ColorPicker::ColorPicker(): PopUpBase("ColorPicker") {
}

void ColorPicker::Render(){
    initView();

    glColor4f(ListaColores[static_cast<int>(ColorID::background)][0], 
                ListaColores[static_cast<int>(ColorID::background)][1],
                ListaColores[static_cast<int>(ColorID::background)][2], 1.0f);

    popUpWindow->Render(false);

    glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], 
                ListaColores[static_cast<int>(ColorID::accent)][1],
                ListaColores[static_cast<int>(ColorID::accent)][2], 1.0f);
    popUpWindow->RenderBorder(false);
}

ColorPicker* colorPicker = nullptr;