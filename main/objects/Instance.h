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

        // la UNICA ref por puntero de la instancia: su target (ver Objects.h). Con
        // esto el destructor la suelta Y el borrado (DeleteUndo) la limpia/restaura.
        int     RefsPropias() const override { return 1; }
        Object* RefPropia(int i) const override { return (i == 0) ? target : NULL; }
        void    SetRefPropia(int i, Object* o) override { if (i == 0) target = o; }
        std::string* RefPropiaNombre(int i) override { return (i == 0) ? &targetName : NULL; }

        ~Instance();
};

#endif