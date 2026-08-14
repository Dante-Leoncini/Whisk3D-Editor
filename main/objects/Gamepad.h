#ifndef GAMEPAD_H
#define GAMEPAD_H

// ============================================================================
//  Este archivo tiene DOS cosas que conviene no confundir:
//
//   1) LA ENTRADA CRUDA DEL MANDO (axisState / buttonState / deadzone /
//      RefreshInputControllerSDL). Es input, es generico y se queda: cualquier
//      juego lo lee, y los scripts lo reciben por stick() / boton().
//
//   2) EL OBJETO DE ESCENA `ObjetoScript` (ObjectType::script): un colgadero de
//      scripts con un target opcional. Historicamente se llamaba "Gamepad" y
//      llevaba adentro un character controller plataformero -- velocidad,
//      gravedad, potenciaSalto, velocidadMaximaCaida y cuatro limites de mundo.
//      Eso era vocabulario de UN genero de juego viviendo en el editor y en el
//      formato de guardado: se dio de baja. Los valores de configuracion de un
//      script son datos del proyecto y viajan por el sistema generico de
//      propiedades (`val_<nombre>` del .w3d + la tabla `propiedades` del .lua),
//      que se leen con propiedad() / parametro().
//
//      El objeto tampoco se puede crear desde el menu Add (cualquier objeto
//      acepta scripts). Sobrevive para que un .w3d viejo siga abriendo: los dos
//      importadores migran el nodo `Gamepad` a un Empty con sus scripts.
// ============================================================================

#ifdef _WIN32
    #define NOMINMAX
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#ifndef W3D_SYMBIAN
#include <SDL2/SDL.h>
#endif
#include <cmath>
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif

#include "WhiskUI/draw/icons.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "Target.h"

#ifndef W3D_SYMBIAN
// ---- (1) ENTRADA CRUDA DEL MANDO (SDL): generica, la lee cualquier juego (PC/desktop) --------
extern float axisState[SDL_CONTROLLER_AXIS_MAX];
extern bool buttonState[SDL_CONTROLLER_BUTTON_MAX];
extern GLfloat deadzone;

// Refresca axisState/buttonState desde un evento SDL
void RefreshInputControllerSDL(SDL_Event &e);
#endif

struct VertexAnimationActive;   // solo por puntero (ver targetAnim)

// ---- (2) EL OBJETO DE ESCENA: scripts + un target opcional ------------------
class ObjetoScript : public Object, public Target {
    public:
        ObjetoScript(Object* parent);

        // el reproductor de vertex animation del target, si el target es una malla.
        // Muere con ella (~Mesh lo saca de VertexAnimationActives), asi que se
        // suelta junto con el target; Reload lo vuelve a resolver.
        VertexAnimationActive* targetAnim;

        ObjectType getType() W3D_OVERRIDE;
        void Reload() W3D_OVERRIDE;
        void RenderObject() W3D_OVERRIDE;

        // su unica ref por puntero: el target (ver Objects.h)
        int     RefsPropias() const W3D_OVERRIDE { return 1; }
        Object* RefPropia(int i) const W3D_OVERRIDE { return (i == 0) ? target : NULL; }
        void    SetRefPropia(int i, Object* o) W3D_OVERRIDE { if (i == 0) { target = o; targetAnim = NULL; } }
        std::string* RefPropiaNombre(int i) W3D_OVERRIDE { return (i == 0) ? &targetName : NULL; }

        ~ObjetoScript();
};

#endif
