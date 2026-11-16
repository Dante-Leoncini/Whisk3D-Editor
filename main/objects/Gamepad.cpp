#include "Gamepad.h"
#include "objects/Camera.h"               // CameraActive
#include "animation/Animation.h"          // CurrentFrame / estado de anim
#include "animation/SkeletalAnimation.h"  // pose del esqueleto
#include "animation/VertexAnimation.h"    // FindTargetAnim (el tipo completo)

// ---- (1) ENTRADA CRUDA DEL MANDO: SDL en PC/Android/web; en Symbian, los joysticks del driver bthid (abajo)
float axisState[SDL_CONTROLLER_AXIS_MAX] = {0.0f};
bool buttonState[SDL_CONTROLLER_BUTTON_MAX] = {false};
GLfloat deadzone = 0.20f;

float axisStateM[W3D_MANDOS_SDL][SDL_CONTROLLER_AXIS_MAX] = {{0.0f}};
bool buttonStateM[W3D_MANDOS_SDL][SDL_CONTROLLER_BUTTON_MAX] = {{false}};
#ifndef W3D_SYMBIAN
static SDL_JoystickID gMandoId[W3D_MANDOS_SDL] = { -1, -1, -1, -1 };

// el lugar de un mando (por su id de SDL): el que ya tenia o el primero libre (-1 si ya hay 4)
static int LugarDe(SDL_JoystickID id) {
    for (int i = 0; i < W3D_MANDOS_SDL; i++) if (gMandoId[i] == id) return i;
    for (int i = 0; i < W3D_MANDOS_SDL; i++) if (gMandoId[i] < 0) { gMandoId[i] = id; return i; }
    return -1;
}

void RefreshInputControllerSDL(SDL_Event &e) {
    if (e.type == SDL_CONTROLLERAXISMOTION) {
        int axis = e.caxis.axis;
        float value = e.caxis.value / 32767.0f;
        axisState[axis] = (fabs(value) < deadzone) ? 0.0f : value;
        const int m = LugarDe(e.caxis.which);
        if (m >= 0) axisStateM[m][axis] = axisState[axis];
    }
    else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {
        const bool v = e.type == SDL_CONTROLLERBUTTONDOWN;
        buttonState[e.cbutton.button] = v;
        const int m = LugarDe(e.cbutton.which);
        if (m >= 0) buttonStateM[m][e.cbutton.button] = v;
    }
    else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
        // (su lugar queda libre y suelto: el proximo que se mueva lo toma)
        for (int i = 0; i < W3D_MANDOS_SDL; i++) {
            if (gMandoId[i] != e.cdevice.which) continue;
            gMandoId[i] = -1;
            for (int k = 0; k < SDL_CONTROLLER_AXIS_MAX; k++) axisStateM[i][k] = 0.0f;
            for (int k = 0; k < SDL_CONTROLLER_BUTTON_MAX; k++) buttonStateM[i][k] = false;
        }
    }
}
#else
// ---- SYMBIAN: los joysticks del driver Bluetooth bthid-s60 (W3dMandos del Core). Cada cuadro: el que aparece se da de
//      alta en el registro COMPARTIDO de controles (el mismo aviso "se detecto un nuevo control" que en PC y en el juego
//      compilado), el que desaparece se da de baja (y se suelta su estado: que el personaje no siga caminando solo), y
//      el estado va a los mismos lugares que llena SDL: de ahi EntradaPad (SimJuego.cpp) lo pasa a los scripts igual.
#include <e32std.h>
#include <stdio.h>
#include "input/W3dMandos.h"
#include "script/BindsJuego.h"   // W3dControlAlta / W3dControlBaja
#include "script/W3dScript.h"    // W3dScriptMandosVibrarHook

static unsigned TicksMs() { return (unsigned)User::NTickCount(); }   // (~ms, el reloj de LayoutTickFPS)
// vibrar() de lua: los mandos conectados vibran 'ms' milisegundos (el driver solo prende y apaga)
static unsigned gVibrarHasta = 0;   // (0 = no vibran)
static bool gConectado[W3D_MANDOS_SDL] = { false, false, false, false };
static void VibrarMandosSymbian(int ms, float fuerza) {
    if (ms <= 0 || fuerza <= 0.0f) return;
    gVibrarHasta = TicksMs() + (unsigned)ms;
    if (gVibrarHasta == 0) gVibrarHasta = 1;
    for (int i = 0; i < W3D_MANDOS_SDL; i++) if (gConectado[i]) w3dEngine::MandoVibrar(i, true);
}
struct MandosEngancharVibrar { MandosEngancharVibrar() { W3dScriptMandosVibrarHook = VibrarMandosSymbian; } };
static MandosEngancharVibrar g_mandosEngancheVibrar;

void MandosSymbianTick() {
    for (int k = 0; k < SDL_CONTROLLER_AXIS_MAX; k++) axisState[k] = 0.0f;
    for (int k = 0; k < SDL_CONTROLLER_BUTTON_MAX; k++) buttonState[k] = false;
    for (int i = 0; i < W3D_MANDOS_SDL; i++) {
        w3dEngine::MandoEstado e;
        const bool hay = w3dEngine::MandoLeer(i, e);
        const int id = 100 + i;   // (el id en el registro: el lugar del joystick en el driver)
        if (hay && !gConectado[i]) {
            const bool ps = (e.tipo == w3dEngine::MandoPS3 || e.tipo == w3dEngine::MandoPS4);
            char nombre[32];
            snprintf(nombre, sizeof(nombre), "%s %d", e.tipo == w3dEngine::MandoPS3 ? "DualShock 3" :
                                     e.tipo == w3dEngine::MandoPS4 ? "DualShock 4" : "Joystick", i + 1);
            W3dControlAlta(0, id, nombre, ps ? "playstation" : "generico");
        } else if (!hay && gConectado[i]) {
            W3dControlBaja(id, 0);
        }
        gConectado[i] = hay;
        for (int k = 0; k < SDL_CONTROLLER_AXIS_MAX; k++) {
            float v = hay ? e.ejes[k] : 0.0f;
            if (fabs(v) < deadzone) v = 0.0f;
            axisStateM[i][k] = v;
            if (fabs(v) > fabs(axisState[k])) axisState[k] = v;   // la mezcla: el que mas se mueve
        }
        for (int k = 0; k < SDL_CONTROLLER_BUTTON_MAX; k++) {
            buttonStateM[i][k] = hay && e.botones[k];
            if (buttonStateM[i][k]) buttonState[k] = true;
        }
    }
    if (gVibrarHasta && TicksMs() >= gVibrarHasta) {
        gVibrarHasta = 0;
        for (int i = 0; i < W3D_MANDOS_SDL; i++) w3dEngine::MandoVibrar(i, false);
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
