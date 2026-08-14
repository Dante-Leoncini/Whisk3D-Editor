#ifndef CAMERA_H
#define CAMERA_H

#ifdef _WIN32
    #define NOMINMAX
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "Target.h"
#include "Curve.h"
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // portable: iconos compartidos
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include "variables.h"
    #include "WhiskUI/theme/colores.h"
    #include <GL/gl.h>
#endif

// Constantes para la geometría de la cámara
static const int CameraVertexSize = 8 * 3;
static const int CameraEdgesSize = 11 * 2;

extern GLfloat CameraVertices[CameraVertexSize];
extern const MeshIndex CameraFaceActive[3];
extern const GLushort CameraEdges[CameraEdgesSize];

// Forward declarations
class Camera;

extern Camera* CameraActive;

class Camera : public Object, public Target {
    public:
        std::string RielName;   // (inicializados en el constructor: C++03)
        int offsetRiel;
        Curve* Riel;

        // NODO MANUAL del riel: -1 = SEGUIMIENTO (la camara se proyecta sobre el riel en el
        // punto mas cercano al target, que es el modo de toda la vida). >= 0 = la camara se
        // planta en ESE indice de nodo (float, interpolado entre nodo y nodo) y NO mira al
        // target para elegir donde pararse. El caso que lo pidio: el viaje de una
        // plataforma va con indice = progreso del path (150 nodos 1 a 1, con el ease in/out
        // horneado en el espaciado de los nodos), no con la proyeccion del target. Es estado de
        // JUEGO, no de proyecto: no se guarda en el .w3d y Reload() lo vuelve a -1.
        float rielNodo;

        // MODO DE MIRADA. false (default, comportamiento de siempre) = LOOK-AT: la camara
        // apunta al target, frame a frame. true = MIRADA DEL RIEL: la orientacion sale del
        // yaw/pitch/roll AUTORAL grabado en el nodo del riel (interpolado por arco corto),
        // y el target solo decide DONDE se para la camara, no hacia donde mira.
        // El porque: el juego original grababa la mirada nodo por nodo (docs/04 6) y el
        // port la re-calculaba con look-at, asi que el yaw se apartaba ~6.5 grados y ademas
        // OSCILABA con el desplazamiento lateral del personaje -algo que el original no
        // hace-. Solo tiene efecto si el .cap del riel trae el canal 'r'; sin el, se sigue
        // mirando al target (no hay nada que usar). Se guarda en el .w3d y se prende/apaga
        // desde lua con setMiradaRiel().
        bool usarRotacionDelRiel;

        // ultimo indice del riel donde se paro la camara (lo deja UpdatePosition; lo usa
        // la mirada del riel para muestrear la rotacion EN EL MISMO punto). -1 = ninguno.
        float rielIndice;

        // LENTE de la camara (editable en el panel + animable). Antes vivia solo en el visor
        // (fovDeg global / orthographic del viewport); ahora es PROPIO de la camara para que "mirar
        // por la camara" use SU lente y el fov se pueda animar (canal AnimFov). Ver ViewPort3D.
        float fov;            // campo de vision vertical en grados (perspectiva). Default 45.
        bool  orthographic;   // true = proyeccion ortografica (sin perspectiva)
        float nearClip;       // distancia MINIMA de dibujado (plano cercano). Default 0.01
        float farClip;        // distancia MAXIMA de dibujado (plano lejano). Default 1000

        // ENCUADRE DECLARADO (aspecto de imagen ancho/alto, ej 1.481481 = 40:27).
        // 0 = no declara (default): el modo juego dibuja con el aspecto del render,
        // como siempre. > 0 = camara CINEMATOGRAFICA: el modo juego dibuja EXACTAMENTE
        // este frustum y rellena el resto de la ventana con barras negras (letterbox/
        // pillarbox); el culling y pantallaDe() usan el mismo numero. Se declara con
        // `aspecto:` en el .w3d (texto y v4) o, si esta en 0, se hereda de la cabecera
        // `aspecto` del .cap del riel por el que viaja (ver Curve::aspecto).
        float aspecto;

        // el aspecto EFECTIVO declarado por esta camara: el propio, o el del riel
        // si el propio esta en 0. Devuelve 0 si nadie declara nada.
        float AspectoDeclarado() const;

        // ¡Nuevas variables para guardar los ejes ya calculados!
        Vector3 rightVector;
        Vector3 forwardVector;

        Camera(Object* parent = NULL, Vector3 pos = Vector3(0,0,0), Vector3 Rot = Vector3(0,0,0));

        ObjectType getType() override;

        void Reload() override;
        
        void UpdateLookAt();

        // orienta la camara hacia 'dir' (no hace falta normalizarla) con 'rollGrados' de
        // alabeo alrededor de esa misma direccion. Es el UNICO lugar donde se arma la base
        // de la camara: lo comparten el look-at al target y la mirada del riel, asi los dos
        // modos producen exactamente la misma clase de rotacion.
        void MirarHacia(const Vector3& dir, float rollGrados = 0.0f);

        // true si ESTE frame la orientacion la manda el riel (modo prendido + riel con
        // canal de rotacion). Cuando da true, UpdateLookAt() no toca nada.
        bool MiradaLaManejaElRiel() const;

        // orienta la camara con la rotacion del nodo 'rielIndice' (interpolada). La llama
        // UpdatePosition despues de plantar la posicion; no hace nada si el modo esta
        // apagado o si el riel no trae rotacion.
        void AplicarMiradaDelRiel();

        void RenderObject() override;

        void UpdatePosition();

        void SetRiel(const std::string& NewValue);
        void ReloadRiel(Object* me);

        // CAMBIO DE RIEL EN CALIENTE (bind lua setRiel). Recibe la Curve YA CARGADA -de la
        // escena- y deja el par (Riel, RielName) coherente para que el .w3d se guarde bien.
        // NULL = soltar el riel (la camara deja de viajar y se queda donde este).
        // Rechaza los mismos dos casos que ReloadRiel (uno mismo / un ancestro) para no armar
        // recursion, y REPOSICIONA en el acto: sin eso la camara pasa un frame entero en la
        // posicion del riel VIEJO, que en el caso medido estaba a 400443 unidades crudas.
        // Devuelve false si el riel no se pudo aplicar (y no toca nada).
        bool SetRielObjeto(Curve* nuevo);

        // punto del riel en el indice 'indice' (float: interpola entre nodo y nodo), en
        // coordenadas de MUNDO (ya suma la posicion global BASE de la curva). Clampea a
        // [0, vertexSize-1]. Es el trozo comun del seguimiento y del nodo manual.
        Vector3 PuntoDelRiel(float indice) const;

        // DOS refs por puntero (ver Objects.h): 0 = el target al que mira, 1 = el RIEL
        // (una Curve). El riel se guarda como Curve*, asi que el cast vive aca -donde
        // el tipo se conoce- y no en el que maneja los borrados.
        int     RefsPropias() const override { return 2; }
        Object* RefPropia(int i) const override {
            if (i == 0) return target;
            if (i == 1) return (Object*)Riel;
            return NULL;
        }
        void SetRefPropia(int i, Object* o) override {
            if (i == 0) target = o;
            else if (i == 1) Riel = (o && o->getType() == ObjectType::curve) ? (Curve*)o : NULL;
        }
        std::string* RefPropiaNombre(int i) override {
            if (i == 0) return &targetName;
            if (i == 1) return &RielName;
            return NULL;
        }

        ~Camera();
};

// EL ASPECTO CON EL QUE DIBUJA EL JUEGO (una sola cuenta para todos): el declarado
// por la camara activa (propio o heredado del riel, ver Camera::aspecto) o, si nadie
// declara, el aspecto del render (g_renderAspect). La leen el frustum del modo juego
// (Viewport3D con ViewFromCameraActive), el Culling y pantallaDe(), asi los tres
// recortan/proyectan con LOS MISMOS numeros que se dibuja.
float W3dAspectoJuego();

#endif