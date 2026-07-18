#ifndef INSTANCE_H
#define INSTANCE_H

#ifdef _WIN32
    #define NOMINMAX
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "WhiskUI/draw/icons.h"
#include "objects/Objects.h"
#include "Target.h"
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif

class Instance : public Object, public Target {
    public:
        bool RenderChildrens;   // (inicializados en el ctor: C++03)
        size_t count;
        bool mirror;            // true: renderiza el target ESPEJADO (Mirror)
        int mirrorEje;          // 0=X, 1=Y, 2=Z (eje del espejo)

        Instance(Object* parent, Object* instance = NULL);

        ObjectType getType() override;

        void Reload() override;

        void RenderObject() override;

        // si borran el objeto al que apunta esta instancia, soltamos el target
        // (queda una instancia vacia que no dibuja nada, en vez de crashear)
        void DesvincularDe(Object* borrado) override {
            if (target == borrado) target = NULL;
        }

        ~Instance();
};

#endif