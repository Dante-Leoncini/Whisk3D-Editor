#ifndef UI_OBJECT_H
#define UI_OBJECT_H
#include "objects/Objects.h"

// =====================================================================
//  UI — la RAIZ de una interfaz 2D (el HUD de un juego, la pantalla de
//  un programa, una animacion 2D). Se edita en el viewport "Editor 2D";
//  en el viewport 3D no dibuja nada (es contenido 2D, no de la escena).
//
//  V1: solo el objeto (aparece en el outliner con el icono de textura y
//  se crea desde Add). Los widgets/imagenes que cuelguen de el y el
//  export para usarlo en los juegos vienen despues.
// =====================================================================
class UI : public Object {
public:
    UI(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "UI", pos) {}
    ObjectType getType() override { return ObjectType::ui; }
    void RenderObject() override {}   // nada en el 3D: vive en el Editor 2D
};
#endif
