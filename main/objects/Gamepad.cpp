#include "Gamepad.h"
#include "objects/Camera.h"               // CameraActive
#include "animation/Animation.h"          // CurrentFrame / estado de anim
#include "animation/SkeletalAnimation.h"  // pose del esqueleto
#include "animation/VertexAnimation.h"    // FindTargetAnim (el tipo completo)

// ---- (1) ENTRADA CRUDA DEL MANDO: SDL en PC/Android/web; en Symbian, los joysticks del driver bthid (abajo)
float axisState[SDL_CONTROLLER_AXIS_MAX] = {0.0f};
bool buttonState[SDL_CONTROLLER_BUTTON_MAX] = {false};
GLfloat deadzone = 0.20f;

float axisStateM[W3D_GAMEPADS_SDL][SDL_CONTROLLER_AXIS_MAX] = {{0.0f}};
bool buttonStateM[W3D_GAMEPADS_SDL][SDL_CONTROLLER_BUTTON_MAX] = {{false}};
#ifndef W3D_SYMBIAN
static SDL_JoystickID gGamepadId[W3D_GAMEPADS_SDL] = { -1, -1, -1, -1 };

// el lugar de un mando (por su id de SDL): el que ya tenia o el primero libre (-1 si ya hay 4)
static int PlaceOf(SDL_JoystickID id) {
    for (int i = 0; i < W3D_GAMEPADS_SDL; i++) if (gGamepadId[i] == id) return i;
    for (int i = 0; i < W3D_GAMEPADS_SDL; i++) if (gGamepadId[i] < 0) { gGamepadId[i] = id; return i; }
    return -1;
}

void RefreshInputControllerSDL(SDL_Event &e) {
    if (e.type == SDL_CONTROLLERAXISMOTION) {
        int axis = e.caxis.axis;
        float value = e.caxis.value / 32767.0f;
        axisState[axis] = (fabs(value) < deadzone) ? 0.0f : value;
        const int m = PlaceOf(e.caxis.which);
        if (m >= 0) axisStateM[m][axis] = axisState[axis];
    }
    else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {
        const bool v = e.type == SDL_CONTROLLERBUTTONDOWN;
        buttonState[e.cbutton.button] = v;
        const int m = PlaceOf(e.cbutton.which);
        if (m >= 0) buttonStateM[m][e.cbutton.button] = v;
    }
    else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
        // (su lugar queda libre y suelto: el proximo que se mueva lo toma)
        for (int i = 0; i < W3D_GAMEPADS_SDL; i++) {
            if (gGamepadId[i] != e.cdevice.which) continue;
            gGamepadId[i] = -1;
            for (int k = 0; k < SDL_CONTROLLER_AXIS_MAX; k++) axisStateM[i][k] = 0.0f;
            for (int k = 0; k < SDL_CONTROLLER_BUTTON_MAX; k++) buttonStateM[i][k] = false;
        }
    }
}
#else
// ---- SYMBIAN: los joysticks del driver Bluetooth bthid-s60 (W3dMandos del Core). Cada cuadro: el que aparece se da de
//      alta en el registro COMPARTIDO de controles (el mismo aviso "se detecto un nuevo control" que en PC y en el juego
//      compilado), el que desaparece se da de baja (y se suelta su estado: que el personaje no siga caminando solo), y
//      el estado va a los mismos lugares que llena SDL: de ahi EntryPad (SimGame.cpp) lo pasa a los scripts igual.
#include <e32std.h>
#include <stdio.h>
#include "input/W3dGamepads.h"
#include "script/BindsGame.h"   // W3dControlAdd / W3dControlRemove
#include "script/W3dScript.h"   // W3dScriptGamepadsVibrateHook

static unsigned TicksMs() { return (unsigned)User::NTickCount(); }   // (~ms, el reloj de LayoutTickFPS)
// vibrate() de lua: los mandos conectados vibran 'ms' milisegundos (el driver solo prende y apaga)
static unsigned gVibrateUntil = 0;   // (0 = no vibran)
static bool gConnected[W3D_GAMEPADS_SDL] = { false, false, false, false };
static void VibrateGamepadsSymbian(int ms, float force) {
    if (ms <= 0 || force <= 0.0f) return;
    gVibrateUntil = TicksMs() + (unsigned)ms;
    if (gVibrateUntil == 0) gVibrateUntil = 1;
    for (int i = 0; i < W3D_GAMEPADS_SDL; i++) if (gConnected[i]) w3dEngine::GamepadVibrate(i, true);
}
struct GamepadsHookVibrate { GamepadsHookVibrate() { W3dScriptGamepadsVibrateHook = VibrateGamepadsSymbian; } };
static GamepadsHookVibrate g_gamepadsHookVibrate;

void GamepadsSymbianTick() {
    for (int k = 0; k < SDL_CONTROLLER_AXIS_MAX; k++) axisState[k] = 0.0f;
    for (int k = 0; k < SDL_CONTROLLER_BUTTON_MAX; k++) buttonState[k] = false;
    for (int i = 0; i < W3D_GAMEPADS_SDL; i++) {
        w3dEngine::GamepadState e;
        const bool has = w3dEngine::GamepadRead(i, e);
        const int id = 100 + i;   // (el id en el registro: el lugar del joystick en el driver)
        if (has && !gConnected[i]) {
            const bool ps = (e.type == w3dEngine::GamepadPS3 || e.type == w3dEngine::GamepadPS4);
            char name[32];
            sprintf(name, "%s %d", e.type == w3dEngine::GamepadPS3 ? "DualShock 3" :
                                     e.type == w3dEngine::GamepadPS4 ? "DualShock 4" : "Joystick", i + 1);
            W3dControlAdd(0, id, name, ps ? "playstation" : "generic");
        } else if (!has && gConnected[i]) {
            W3dControlRemove(id, 0);
        }
        gConnected[i] = has;
        for (int k = 0; k < SDL_CONTROLLER_AXIS_MAX; k++) {
            float v = has ? e.axes[k] : 0.0f;
            if (fabs(v) < deadzone) v = 0.0f;
            axisStateM[i][k] = v;
            if (fabs(v) > fabs(axisState[k])) axisState[k] = v;   // la mezcla: el que mas se mueve
        }
        for (int k = 0; k < SDL_CONTROLLER_BUTTON_MAX; k++) {
            buttonStateM[i][k] = has && e.buttons[k];
            if (buttonStateM[i][k]) buttonState[k] = true;
        }
    }
    if (gVibrateUntil && TicksMs() >= gVibrateUntil) {
        gVibrateUntil = 0;
        for (int i = 0; i < W3D_GAMEPADS_SDL; i++) w3dEngine::GamepadVibrate(i, false);
    }
}
#endif

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

ObjectScript::ObjectScript(Object* parent)
    : Object(parent, "Script", Vector3(0, 0, 0)), targetAnim(NULL)
{
}

// ---- target() de lua, por HOOK -------------------------------------------
// El bind vive en el set COMPARTIDO (BindsGame.cpp) para que exista tambien en
// el juego compilado; quien sabe que es un ObjectScript es ESTE archivo, asi que
// la resolucion del target se instala desde aca (mismo patron que los hooks de
// la camara). Sin este .cpp linkeado, target() devuelve nil, como siempre.
#include "script/BindsGame.h"
namespace {
Object* TargetOf(Object* owner) {
    if (owner && owner->getType() == ObjectType::script)
        return ((ObjectScript*)owner)->target;
    return NULL;
}
struct InstallHookTarget { InstallHookTarget() { W3dTargetHook = TargetOf; } };
InstallHookTarget gInstallHookTarget;
}

ObjectType ObjectScript::getType() {
    return ObjectType::script;
}

void ObjectScript::Reload() {
    ReloadTarget(this);   // resuelve target por nombre

    targetAnim = NULL;
    if (target && target->getType() == ObjectType::mesh)
        targetAnim = FindTargetAnim(static_cast<Mesh*>(target));
}

void ObjectScript::RenderObject() {
    // no dibuja nada y no simula nada: lo que hace el objeto lo hacen sus scripts
}

ObjectScript::~ObjectScript() {
}
