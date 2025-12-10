#ifndef INSTANCE_H
#define INSTANCE_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include "UI/icons.h"
#include "Objects.h"
#include "Target.h"
#include <GL/gl.h>

class Instance : public Object, public Target {
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