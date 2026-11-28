#include "W3dColors.h"
#include "WhiskUI/theme/colors.h"       // ColorList (colores genericos de la UI)
#include "objects/RenderColors.h"  // gRenderColors del core

#include <fstream>
#include <sstream>
#include <string>

float W3dColors[W3dColor_COUNT][4] = {
    { 0.94f,  0.59f,  0.17f,  0.25f },  // naranjaFace
    { 0.757f, 0.757f, 0.757f, 1.0f  },  // rojoEje
    { 0.22f,  0.22f,  0.22f,  1.0f  },  // LineaPiso
    { 0.56f,  0.23f,  0.28f,  1.0f  },  // LineaPisoRoja
    { 0.38f,  0.53f,  0.15f,  1.0f  },  // LineaPisoVerde
    { 0.88f,  0.48f,  0.54f,  1.0f  },  // ColorTransformX
    { 0.65f,  0.81f,  0.38f,  1.0f  },  // ColorTransformY
    { 0.46f,  0.67f,  0.89f,  1.0f  },  // ColorTransformZ
    { 1.0f,   1.0f,   0.0f,   1.0f  },  // normalVertex
    { 1.0f,   0.0f,   1.0f,   1.0f  },  // normalCustom
    { 0.0f,   1.0f,   1.0f,   1.0f  },  // normalFace
    { 0.55f,  0.85f,  0.0f,   1.0f  }   // seleccionInactiva
};

GLubyte W3dColorsUbyte[W3dColor_COUNT][4] = {
    { (GLubyte)(0.94*255),  (GLubyte)(0.59*255),  (GLubyte)(0.17*255),  (GLubyte)(0.25*255) },
    { (GLubyte)(0.757*255), (GLubyte)(0.757*255), (GLubyte)(0.757*255), 255 },
    { (GLubyte)(0.22*255),  (GLubyte)(0.22*255),  (GLubyte)(0.22*255),  255 },
    { (GLubyte)(0.56*255),  (GLubyte)(0.23*255),  (GLubyte)(0.28*255),  255 },
    { (GLubyte)(0.38*255),  (GLubyte)(0.53*255),  (GLubyte)(0.15*255),  255 },
    { (GLubyte)(0.88*255),  (GLubyte)(0.48*255),  (GLubyte)(0.54*255),  255 },
    { (GLubyte)(0.65*255),  (GLubyte)(0.81*255),  (GLubyte)(0.38*255),  255 },
    { (GLubyte)(0.46*255),  (GLubyte)(0.67*255),  (GLubyte)(0.89*255),  255 },
    { 255, 255, 0,   255 },
    { 255, 0,   255, 255 },
    { 0,   255, 255, 255 },
    { (GLubyte)(0.55*255), (GLubyte)(0.85*255), 0, 255 }
};

int editorColorIdByName(const char* name) {
    std::string n(name);
    if (n == "orangeFace") return W3dColor_orangeFace;
    if (n == "redAxis") return W3dColor_redAxis;
    if (n == "LineFloor") return W3dColor_LineFloor;
    if (n == "LineRedFloor") return W3dColor_LineFloorRed;
    if (n == "LineGreenFloor") return W3dColor_LineFloorGreen;
    if (n == "ColorTransformX") return W3dColor_ColorTransformX;
    if (n == "ColorTransformY") return W3dColor_ColorTransformY;
    if (n == "ColorTransformZ") return W3dColor_ColorTransformZ;
    if (n == "normalVertex") return W3dColor_normalVertex;
    if (n == "normalCustom") return W3dColor_normalCustom;
    if (n == "normalFace") return W3dColor_normalFace;
    if (n == "inactiveSelection") return W3dColor_selectionInactive;
    return -1;
}

bool loadEditorColorsW3d(const char* aPath) {
    std::ifstream file(aPath);
    if (!file.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ls(line);
        std::string name;
        float r, g, b, a;
        if (!(ls >> name >> r >> g >> b >> a)) continue;
        int id = editorColorIdByName(name.c_str());
        if (id < 0) continue;
        W3dColors[id][0] = r;
        W3dColors[id][1] = g;
        W3dColors[id][2] = b;
        W3dColors[id][3] = a;
        W3dColorsUbyte[id][0] = (GLubyte)(r * 255.0f);
        W3dColorsUbyte[id][1] = (GLubyte)(g * 255.0f);
        W3dColorsUbyte[id][2] = (GLubyte)(b * 255.0f);
        W3dColorsUbyte[id][3] = (GLubyte)(a * 255.0f);
    }
    return true;
}

void SyncRenderColors() {
    for (int k = 0; k < 4; k++) {
        gRenderColors[RC_wireframe][k]    = ColorList[static_cast<int>(ColorID::grayUI)][k];
        gRenderColors[RC_selActive][k]    = ColorList[static_cast<int>(ColorID::accent)][k];
        gRenderColors[RC_selInactive][k]  = W3dColors[W3dColor_selectionInactive][k];
        gRenderColors[RC_normalFace][k]   = W3dColors[W3dColor_normalFace][k];
        gRenderColors[RC_normalVert][k]   = W3dColors[W3dColor_normalVertex][k];
        gRenderColors[RC_normalCustom][k] = W3dColors[W3dColor_normalCustom][k];
        gRenderColors[RC_gizmoDark][k]    = ColorList[static_cast<int>(ColorID::accentDark)][k];
        gRenderColors[RC_editEdge][k]     = W3dColors[W3dColor_LineFloor][k];   // la jaula del Edit Mode (EditMesh, Core)
        gRenderColors[RC_background][k]   = ColorList[static_cast<int>(ColorID::background)][k];
    }
}
