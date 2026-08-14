#ifndef CONSTRUCTOR_H
#define CONSTRUCTOR_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include <filesystem>

#include "variables.h"
#include "objects/Camera.h"
#include "objects/Collection.h"
#include "objects/Mesh.h"

#include "WhiskUI/widgets/card.h"

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/ColorPicker.h"

#include "importers/import_w3d.h"

// Variable global visible en todos los archivos
extern bool running;

// Constructor universal para todas las plataformas
void ConstructUniversal(int argc, char* argv[]);

#endif