#ifndef INSTANCE_H
#define INSTANCE_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include "UI/icons.h"
#include "Objects.h"
#include "Modifier.h"
#include <GL/gl.h>

class Instance : public Object, public Modifier {
public:
    bool RenderChildrens = true;
    size_t count = 1;

    Instance(Object* parent, Object* instance = nullptr);

    ObjectType getType() override;

    void Reload() override;

    void RenderObject() override;

    ~Instance();
};

#endif