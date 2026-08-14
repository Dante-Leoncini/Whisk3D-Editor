#include "Gamepad.h"
#include "objects/Camera.h"               // CameraActive
#include "animation/Animation.h"          // CurrentFrame / estado de anim
#include "animation/SkeletalAnimation.h"  // pose del esqueleto
#include "animation/VertexAnimation.h"    // FindTargetAnim (el tipo completo)

// ---- (1) ENTRADA CRUDA DEL MANDO -------------------------------------------
float axisState[SDL_CONTROLLER_AXIS_MAX] = {0.0f};
bool buttonState[SDL_CONTROLLER_BUTTON_MAX] = {false};
GLfloat deadzone = 0.20f;

void RefreshInputControllerSDL(SDL_Event &e) {
    if (e.type == SDL_CONTROLLERAXISMOTION) {
        int axis = e.caxis.axis;
        float value = e.caxis.value / 32767.0f;
        axisState[axis] = (fabs(value) < deadzone) ? 0.0f : value;
    }
    else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        buttonState[e.cbutton.button] = true;
    }
    else if (e.type == SDL_CONTROLLERBUTTONUP) {
        buttonState[e.cbutton.button] = false;
    }
}

// ---- (2) EL OBJETO DE ESCENA -----------------------------------------------
// Aca vivia un character controller plataformero completo: stick + gravedad +
// salto + limites de mundo + animaciones, con sus constantes tuneadas (velocidad
// 0.03, gravedad 0.03, potenciaSalto 0.1, corral de -2 a +2). Nada de eso tenia
// que hacer colgado del editor: es logica y son datos de UN juego. Hoy la fisica
// es un script lua del proyecto y sus numeros son propiedades del script
// (`val_<nombre>` del .w3d / la tabla `propiedades` del .lua).
//
// El objeto quedo como lo que siempre fue en la practica: un colgadero de
// scripts con un target opcional. Ni siquiera se puede crear desde el menu Add,
// porque CUALQUIER objeto acepta scripts; sobrevive solo para abrir .w3d viejos.

ObjetoScript::ObjetoScript(Object* parent)
    : Object(parent, "Script", Vector3(0, 0, 0)), targetAnim(NULL)
{
}

// ---- objetivo() de lua, por HOOK -------------------------------------------
// El bind vive en el set COMPARTIDO (BindsJuego.cpp) para que exista tambien en
// el juego compilado; quien sabe que es un ObjetoScript es ESTE archivo, asi que
// la resolucion del target se instala desde aca (mismo patron que los hooks de
// la camara). Sin este .cpp linkeado, objetivo() devuelve nil, como siempre.
#include "script/BindsJuego.h"
namespace {
Object* ObjetivoDe(Object* duenio) {
    if (duenio && duenio->getType() == ObjectType::script)
        return ((ObjetoScript*)duenio)->target;
    return NULL;
}
struct InstalarHookObjetivo { InstalarHookObjetivo() { W3dObjetivoHook = ObjetivoDe; } };
InstalarHookObjetivo gInstalarHookObjetivo;
}

ObjectType ObjetoScript::getType() {
    return ObjectType::script;
}

void ObjetoScript::Reload() {
    ReloadTarget(this);   // resuelve target por nombre

    targetAnim = NULL;
    if (target && target->getType() == ObjectType::mesh)
        targetAnim = FindTargetAnim(static_cast<Mesh*>(target));
}

void ObjetoScript::RenderObject() {
    // no dibuja nada y no simula nada: lo que hace el objeto lo hacen sus scripts
}

ObjetoScript::~ObjetoScript() {
}
