#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <string>
#include <iostream>

#include "PopUpBase.h"

class ColorPicker: public PopUpBase {
    public:

        void Render() override;

        ColorPicker();
        ~ColorPicker();
};

extern ColorPicker* colorPicker;

#endif