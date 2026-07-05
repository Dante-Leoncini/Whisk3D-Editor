#ifndef MIRROR_H
#define MIRROR_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "WhiskUI/icons.h"
#include "objects/Objects.h"
#include "Target.h"
#include <GL/gl.h>

class Mirror : public Object, public Target {
    public:
        bool mirrorX = false;
        bool mirrorY = false;
        bool mirrorZ = true;
        bool RenderChildrens = true;

        Mirror(Object* parent, Vector3 pos = Vector3(0,0,0), bool x = false, bool y = false, bool z = true);

        ObjectType getType() override;

        void Reload() override;

        void RenderObject() override;

        ~Mirror();
};

#endif