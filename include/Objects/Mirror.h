#ifndef MIRROR_H
#define MIRROR_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include "../UI/icons.h"
#include "Objects/Objects.h"
#include "Objects/Modifier.h"
#include <GL/gl.h>

class Mirror : public Object, public Modifier {
public:
    bool mirrorX = false;
    bool mirrorY = false;
    bool mirrorZ = true;
    bool RenderChildrens = true;

    Mirror(Object* parent, bool x = false, bool y = false, bool z = true);

    ObjectType getType() override;

    void Reload() override;

    void RenderObject() override;

    ~Mirror();
};

#endif