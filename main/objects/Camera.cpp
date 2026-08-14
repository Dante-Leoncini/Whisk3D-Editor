#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "Camera.h"
#include <math.h>
#include <algorithm>
#include "WhiskUI/theme/colores.h"
#include "render/OpcionesRender.h" // g_renderAspect (la geometria de la camara sigue el aspecto del render)

// Geometría de la cámara
GLfloat CameraVertices[CameraVertexSize] = {    
    0, 0, 0, // origen
    0.35f*2,  0.144f*2,  0.192f*2,
    0.35f*2, -0.144f*2,  0.192f*2,
    0.35f*2, -0.144f*2, -0.192f*2,
    0.35f*2,  0.144f*2, -0.192f*2,
    0.35f*2,  0.17f*2,  0.128f*2,
    0.35f*2,  0.17f*2, -0.128f*2,
    0.35f*2,  0.31f*2,  0,
};

const MeshIndex CameraFaceActive[3] = {5, 6, 7};

// RESPONSIVE: reescribe CameraVertices para que el rectangulo del "film" y el triangulito de arriba sigan el
// ASPECTO del render (ancho/alto). Se FIJA la ALTURA y se escala el ANCHO (z) por el aspecto: 1:1 = cuadrada,
// 4:3 = 4:3, 16:9 = ancha, etc. Barato -> se llama por frame antes de dibujar el gizmo. Devuelve true si cambio.
static float gCamGeomAspect = -1.0f; // ultimo aspecto con el que se armo la geometria
bool ActualizarGeometriaCamara() {
    float aspect = g_renderAspect;
    if (aspect < 0.05f) aspect = 0.05f; if (aspect > 20.0f) aspect = 20.0f;
    if (aspect == gCamGeomAspect) return false; // sin cambios
    gCamGeomAspect = aspect;
    const float x = 0.7f;      // profundidad del film (0.35*2, como el original)
    const float H = 0.288f;    // MEDIA-altura fija del rectangulo (0.144*2)
    float Wd = H * aspect;     // MEDIA-ancho segun el aspecto (4:3 -> 0.384, = original)
    float Zt = Wd * (0.256f / 0.384f); // el triangulito escala con el ancho (misma proporcion que el original)
    GLfloat v[CameraVertexSize] = {
        0, 0, 0,          // origen (apex del frustum)
        x,  H,  Wd,       // 1  esquinas del rectangulo (film)
        x, -H,  Wd,       // 2
        x, -H, -Wd,       // 3
        x,  H, -Wd,       // 4
        x, 0.34f,  Zt,    // 5  triangulito de arriba (camara activa): base
        x, 0.34f, -Zt,    // 6
        x, 0.62f, 0,      // 7  apice del triangulito
    };
    for (int i = 0; i < CameraVertexSize; i++) CameraVertices[i] = v[i];
    return true;
}

const GLushort CameraEdges[CameraEdgesSize] = {
    0, 1,
    0, 2,
    0, 3,
    0, 4,
    1, 2,
    2, 3,
    3, 4,
    4, 1,
    5, 6,
    6, 7,
    7, 5,
};

Camera* CameraActive = NULL;

// ------------------- MÉTODOS -------------------

Camera::Camera(Object* parent, Vector3 pos, Vector3 Rot)
    : Object(parent, T("Camera"), pos, Rot), offsetRiel(0), Riel(NULL), rielNodo(-1.0f),
      usarRotacionDelRiel(false), rielIndice(-1.0f),
      fov(45.0f), orthographic(false), nearClip(0.01f), farClip(1000.0f),
      aspecto(0.0f)
{

    if (!CameraActive){
        CameraActive = this;
    }
}

ObjectType Camera::getType() {
    return ObjectType::camera;
}

// el aspecto DECLARADO por esta camara: el propio (`aspecto:` del .w3d) o, si esta
// en 0, la cabecera `aspecto` del .cap de su riel. 0 = nadie declara.
float Camera::AspectoDeclarado() const {
    if (aspecto > 0.01f) return aspecto;
    if (Riel && Riel->aspecto > 0.01f) return Riel->aspecto;
    return 0.0f;
}

// ver Camera.h: el aspecto con el que dibuja (y recorta y proyecta) el modo juego.
float W3dAspectoJuego() {
    if (CameraActive) {
        float a = CameraActive->AspectoDeclarado();
        if (a > 0.01f) return a;
    }
    return (g_renderAspect > 1e-4f) ? g_renderAspect : 1.0f;
}

void Camera::Reload() {
    ReloadTarget(this);
    ReloadRiel(this);
    rielNodo = -1.0f;   // el nodo manual es estado de JUEGO: abrir un proyecto no lo hereda
    rielIndice = -1.0f; // (y con el, el indice muestreado para la mirada del riel)
}

#include <cmath> // Necesario para std::abs() y std::sqrt() si no está en Vector3.h

bool Camera::MiradaLaManejaElRiel() const {
    return usarRotacionDelRiel && Riel && Riel->TieneRotacion();
}

void Camera::UpdateLookAt() {
    // MODO MIRADA DEL RIEL: la orientacion ya la escribio UpdatePosition con el yaw/pitch
    // del nodo. Si esto siguiera, el look-at la pisaria en el acto y el modo no existiria.
    if (MiradaLaManejaElRiel()) return;
    if (!target) return;

    //std::cout << "Camara pos X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z << std::endl;
    //std::cout << "Target X: " << target->pos.x << " Y: " << target->pos.y << " Z: " << target->pos.z << std::endl;

    MirarHacia(target->pos - pos);
}

void Camera::MirarHacia(const Vector3& dir, float rollGrados) {
    if (dir.LengthSq() <= 1e-12f) return;   // sin direccion no hay nada que orientar
    forwardVector = dir.Normalized();

    // CORRECCIÓN 1: Usar WorldUp POSITIVO (Y-Up estándar)
    Vector3 worldUp(0, 1, 0);

    // Opcional pero recomendado: Anti-Gimbal Lock
    if (fabsf(forwardVector.Dot(worldUp)) > 0.999f) { 
        worldUp = Vector3(1, 0, 0); 
    }

    // 2. Cálculo del Eje Right (X local) - Orden para que funcione el Yaw (no invertido)
    // Orden Canónico: Cross(WorldUp, Dir)
    rightVector = Vector3::Cross(worldUp, forwardVector).Normalized();

    // 3. Cálculo del Eje Up (Y local)
    // El orden Cross(Right, Dir) es el estándar.
    Vector3 up = Vector3::Cross(rightVector, forwardVector).Normalized();

    // 3bis. ROLL: alabeo alrededor de la propia direccion de mirada. El look-at siempre
    // pasa 0 (nivelado con el horizonte, como antes); la mirada del riel pasa el roll
    // autoral del nodo. Se rota el par right/up, no el forward (que es el eje).
    if (rollGrados != 0.0f) {
        const float a = rollGrados * 3.14159265358979323846f / 180.0f;
        const float c = cosf(a), s = sinf(a);
        Vector3 r2 = rightVector * c + up * s;
        Vector3 u2 = up * c - rightVector * s;
        rightVector = r2.Normalized();
        up = u2.Normalized();
    }

    // 4. Forward (Eje Z cámara de Vista)
    Vector3 forward = (-forwardVector).Normalized();
    
    // 5. Construcción de matriz (column-major)
    // Los signos deben ser: [-Right | -Up | +Forward] o [+Right | +Up | -Forward]
    Matrix4 M;
    M.Identity();

    // Columna X (Right): Invertimos el signo para corregir el Yaw.
    M.m[0] = -rightVector.x;
    M.m[1] = -rightVector.y;
    M.m[2] = -rightVector.z;

    // Columna Y (Up):
    M.m[4] = -up.x;
    M.m[5] = -up.y;
    M.m[6] = -up.z;

    // Columna Z (Forward): Positivo (mira a lo largo de -Z)
    M.m[8]  = forward.x;
    M.m[9]  = forward.y;
    M.m[10] = forward.z;

    SetRot(Quaternion::FromMatrix(M));   // por la puerta: el euler que se guarda queda al dia
}

void Camera::SetRiel(const std::string& NewValue) {
    RielName = NewValue;
    Riel = NULL;
    rielNodo = -1.0f;   // riel nuevo = seguimiento nuevo (el indice viejo no significa nada aca)
    rielIndice = -1.0f;
}

// Busca y asigna el riel según RielName
void Camera::ReloadRiel(Object* me) {
    Riel = NULL;
    // SIN riel no hay nada que buscar ni que avisar: la mayoria de las camaras no
    // viajan por una curva y Reload() llama a esto SIEMPRE, asi que cada apertura de
    // proyecto escupia un "RielTarget '' no encontrado" por camara. Vacio no es error.
    if (RielName.empty()) return;
    Object* obj = FindObjectByName(SceneCollection, RielName);
#ifdef W3D_SYMBIAN
    // sin RTTI en RVCT: el tipo se chequea por getType()
    Curve* RielTarget = (obj && obj->getType() == ObjectType::curve)
                        ? (Curve*)obj : NULL;
    if (!RielTarget){
        return;
    }
#else
    Curve* RielTarget = dynamic_cast<Curve*>(obj);
    if (!RielTarget){
#ifndef W3D_SYMBIAN
        std::cout << "RielTarget '"<< RielName <<"' no encontrado...\n";
#endif
        return;
    }
#endif

    // 1) Evitar apuntarse a sí mismo
    if (RielTarget == me) {
#ifndef W3D_SYMBIAN
        std::cout << "ERROR: Target es el mismo objeto → prohibido\n";
#endif
        return;
    }

    // 2) Evitar apuntar a ancestros
    if (IsMyAncestor(me, RielTarget)) {
#ifndef W3D_SYMBIAN
        std::cout << "ERROR: Target es un ancestro → generaría recursion\n";
#endif
        return;
    }

    // 3) Asignar correctamente
    Riel = RielTarget;
#ifndef W3D_SYMBIAN
    std::cout << "Riel '"<< RielName <<"' encontrado!\n";
#endif
}

// CAMBIO DE RIEL EN CALIENTE. Ver Camera.h. El riel se cambia SIEMPRE por objeto (una Curve
// que ya vive en la escena y que el .w3d cargo al abrir); NO se carga un .cap desde aca. Dos
// razones: Curve::LoadFromFile abre con std::ifstream y NO pasa por el VFS, asi que dentro de
// un contenedor v4 empaquetado una ruta no existe como archivo; y toda Curve nueva se cuelga
// sola de SceneCollection (Objects.cpp:96), o sea que un "cache de curvas por ruta" serian
// objetos de escena a medias, que el guardado escribiria igual. Declarando los rieles en el
// .w3d el ciclo de vida ya esta resuelto -los borra la escena- y no hay ni fuga ni recarga.
bool Camera::SetRielObjeto(Curve* nuevo) {
    if (nuevo) {
        if ((Object*)nuevo == (Object*)this) return false;      // apuntarse a si mismo
        if (IsMyAncestor(this, (Object*)nuevo)) return false;    // ancestro -> recursion
    }
    Riel     = nuevo;
    RielName = nuevo ? nuevo->name : std::string();
    // RESET DEL SEGUIMIENTO. Hoy el seguimiento NO guarda estado entre frames: UpdatePosition
    // arranca cada vez de Curve::FindNearest, que es una consulta al KD-tree de la curva NUEVA
    // -no hay cursor de nodo ni banda muerta que puedan quedar apuntando al riel viejo-. Lo
    // unico que sobrevive al cambio es el nodo manual, que es indice del riel viejo y aca no
    // significa nada, y la POSICION ya escrita: por eso se reposiciona en el acto (abajo).
    rielNodo = -1.0f;
    rielIndice = -1.0f;
    UpdatePosition();   // sin esto queda un frame en el riel viejo (en el bonus: a 1001 unidades)
    return true;
}

Vector3 Camera::PuntoDelRiel(float indice) const {
    if (!Riel || Riel->vertexSize <= 0) return this->pos;
    const int ultimo = Riel->vertexSize - 1;
    if (indice < 0.0f) indice = 0.0f;
    if (indice > (float)ultimo) indice = (float)ultimo;
    int baseIndex = (int)floor(indice);
    if (baseIndex > ultimo) baseIndex = ultimo;
    float t = indice - (float)baseIndex;
    int nextIndex = std::min(baseIndex + 1, ultimo);
    Vector3 P0 = Riel->GetPoint(baseIndex);
    Vector3 P1 = Riel->GetPoint(nextIndex);
    // BASE a proposito: esto termina en this->pos, que se guarda en el .w3d (ver UpdatePosition)
    return (P0 * (1.0f - t) + P1 * t) + Riel->GetGlobalPositionBase();
}

extern bool g_showCamera; // toggle del submenu "Objects" (overlays)

void Camera::RenderObject() {
    if (!g_showCamera) return; // oculto por el toggle "Camera" del menu de overlays
#ifdef W3D_SYMBIAN
    // MISMO gate que PC: sin overlays (render a PNG / limpieza de pantalla) o mirando DESDE la camara
    // -> no dibujar el gizmo. Antes Symbian lo dibujaba SIEMPRE (se veia en el render del N95).
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;
    bool geomCambio = ActualizarGeometriaCamara(); // geometria responsive al aspecto del render
    // gizmo de PC (piramide de lineas + triangulo si es la activa) pero con
    // drawArrays: vertices expandidos una sola vez desde CameraEdges (re-expande si cambio el aspecto)
    static GLfloat lineV[CameraEdgesSize * 3];
    static GLfloat faceV[9];
    static bool armado = false;
    if (!armado || geomCambio) {
        for (int i = 0; i < CameraEdgesSize; i++) {
            int vi = CameraEdges[i];
            lineV[i*3+0] = CameraVertices[vi*3+0];
            lineV[i*3+1] = CameraVertices[vi*3+1];
            lineV[i*3+2] = CameraVertices[vi*3+2];
        }
        for (int i = 0; i < 3; i++) {
            int vi = CameraFaceActive[i];
            faceV[i*3+0] = CameraVertices[vi*3+0];
            faceV[i*3+1] = CameraVertices[vi*3+1];
            faceV[i*3+2] = CameraVertices[vi*3+2];
        }
        armado = true;
    }
    w3dEngine::PushMatrix();
    w3dEngine::Rotatef(90.0f, 0, 1, 0);
    const float* c;
    if (ObjActivo == this && select) c = ListaColores[static_cast<int>(ColorID::accent)];
    else if (select)                 c = ListaColores[static_cast<int>(ColorID::accentDark)];
    else                             c = ListaColores[static_cast<int>(ColorID::grisUI)];
    w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    GLboolean luzEstaba = w3dEngine::IsEnabled(w3dEngine::Lighting); // restaurar al salir!
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::LineWidth(1);
    w3dEngine::VertexPointer3f(0, lineV);
    w3dEngine::TexCoordPointer2f(12, lineV); // dummy valido
    w3dEngine::DrawLines(CameraEdgesSize);
    if (CameraActive == this) {
        // triangulo SOLIDO arriba = camara activa (como Blender); PC
        // tambien apaga el culling porque la cara se ve de ambos lados
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::VertexPointer3f(0, faceV);
        w3dEngine::TexCoordPointer2f(12, faceV);
        w3dEngine::DrawTrianglesArray(3);
    }
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (luzEstaba) w3dEngine::Enable(w3dEngine::Lighting);
    w3dEngine::PopMatrix();
    return;
#else
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;
    ActualizarGeometriaCamara(); // geometria responsive al aspecto del render (usa CameraVertices directo)

    w3dEngine::PushMatrix();
    w3dEngine::Rotatef(90.0f, 0, 1, 0);

    if (ObjActivo == this && select){
        w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
    }
    else if (select){
        w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
    }
    else {
        w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::grisUI)]);
    }

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Texture2D); 
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::LineWidth(1);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::Disable(w3dEngine::ColorMaterial);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DepthMask(true);

    w3dEngine::VertexPointer3f(0, CameraVertices);
    w3dEngine::DrawLinesIndexed(CameraEdgesSize, CameraEdges);

    if (CameraActive == this){		
        w3dEngine::Disable(w3dEngine::CullFace);	
        w3dEngine::DrawTriangles(3, CameraFaceActive);	
    }
    w3dEngine::PopMatrix();
#endif // !W3D_SYMBIAN
}

// EL RIEL: pone this->pos sobre la curva 'Riel', en el punto mas cercano al 'target' (menos el
// offset de indice). OJO CON LO QUE ESTA FUNCION ES: ESCRIBE this->pos, y Camera::pos SE GUARDA
// en el .w3d (GuardarW3D.cpp), pero la llama el camino de DIBUJO (Viewport3D::UpdateViewOrbit) y
// el de PICK, o sea una vez por viewport y por frame.
// Ese horneado -que la posicion guardada de la camara la escriba el render- es viejo y NO se
// arregla aca (queda para su propio redisenio). Lo que si es de esta tanda: las dos lecturas de
// abajo tienen que ser BASE. Con la EFECTIVA, un constraint en el target, en el riel o en
// cualquier ancestro de los dos mete la orientacion de la camara que dibujo ULTIMO adentro de
// this->pos, asi que el .w3d guardado pasaba a depender de cual viewport se dibujo al final: con
// dos vistas abiertas, el mismo proyecto guardado dos veces daba dos archivos distintos. Base ->
// el riel y el target valen lo mismo se mire desde donde se mire, y el horneado (que sigue) al
// menos es DETERMINISTA.
void Camera::UpdatePosition() {
    if (!Riel) return;

    // el indice DONDE LA CAMARA SE PARA de verdad: PuntoDelRiel clampea la POSICION a
    // [0, ultimo], asi que el indice reportado (rielIndice -> rielDe() de lua, VisZona,
    // mirada del riel) tiene que sufrir EL MISMO clamp. Antes el offsetRiel podia
    // dejarlo NEGATIVO al principio del riel (mira proyectada en el nodo 0 - offset 8
    // = indice -8) con la camara parada en el nodo 0: rielDe() devolvia -8, el lua del
    // juego lo trataba como "sin indice" y apagaba su formula de mirada -> la camara
    // quedaba en look-at pelado y giraba de mas (medido en el nivel real: yaw 87 con
    // un clamp autoral de +-12.9). Camera.h ya documentaba rielIndice como "el indice
    // donde SE PARO la camara": esto lo hace cierto tambien en las puntas.
    const float ultimoIdx = (Riel->vertexSize > 0) ? (float)(Riel->vertexSize - 1) : 0.0f;

    // NODO MANUAL (setRielNodo): la camara se planta en el indice pedido y no mira al target.
    // El indice es FINAL -offsetRiel NO se le resta-: el que maneja el indice a mano ya sabe
    // en que nodo quiere estar (el caso que lo pidio: el indice ES el progreso de un viaje).
    if (rielNodo >= 0.0f) {
        this->pos = PuntoDelRiel(rielNodo);
        rielIndice = (rielNodo > ultimoIdx) ? ultimoIdx : rielNodo;
        AplicarMiradaDelRiel();
        return;
    }

    if (!target) return;

    Vector3 P = target->GetGlobalPositionBase();

    int idx = Riel->FindNearest(P);
    if (idx < 0) return;

    // índices vecinos
    int i1 = idx;
    int i0 = std::max(0,     i1 - 1);
    int i2 = std::min(Riel->vertexSize - 1, i1 + 1);

    // obtener puntos reales
    Vector3 A = Riel->GetPoint(i0);
    Vector3 B = Riel->GetPoint(i1);
    Vector3 C = Riel->GetPoint(i2);

    float tAB, tBC;
    Vector3 pAB = ClosestPointOnSegment(A, B, P, tAB);
    Vector3 pBC = ClosestPointOnSegment(B, C, P, tBC);

    // distancias para elegir cuál segmento es mejor
    float dAB = (pAB - P).LengthSq();
    float dBC = (pBC - P).LengthSq();

    // de los dos segmentos vecinos gana el que pasa mas cerca del target, y lo que se
    // guarda es su INDICE fraccionario (el punto proyectado en si no se usa: la posicion
    // sale de re-muestrear el riel ya corrido por offsetRiel, abajo).
    float finalIndexFloat = (dAB < dBC) ? ((float)i0 + tAB)   // ejemplo: 12.35 entre 12 y 13
                                        : ((float)i1 + tBC);

    // aplicar offset hacia atrás (en NODOS) y muestrear: el clamp a [0, vertexSize-1] y la
    // interpolacion viven en PuntoDelRiel, que comparte con el nodo manual de arriba.
    rielIndice = finalIndexFloat - (float)offsetRiel;
    if (rielIndice < 0.0f) rielIndice = 0.0f;              // el riel se acabo: parada en el nodo 0
    else if (rielIndice > ultimoIdx) rielIndice = ultimoIdx; // (o en el ultimo, del otro lado)
    this->pos = PuntoDelRiel(rielIndice);
    AplicarMiradaDelRiel();
}

// ---------------------------------------------------------------------------
//  MIRADA DEL RIEL: la orientacion sale del yaw/pitch/roll grabado en el nodo, no
//  de mirar al target. No hace nada si el modo esta apagado o si el .cap no trae
//  el canal 'r' -- o sea que el default (y todo proyecto de hoy) no cambia en nada.
//
//  CONVENCION (documentada en Curve.h, junto al dato que la decide). El canal 'r'
//  guarda pitch/yaw/roll en grados y la direccion de mirada, EN EL ESPACIO DEL MOTOR,
//  es la canonica de yaw/pitch:
//      forward = ( -sin(yaw)*cos(pitch),  sin(pitch),  -cos(yaw)*cos(pitch) )
//
//  Si el .cap declara que viene con la Z al reves (`ejes x y -z`, que es el default
//  legacy), el cargador espeja los PUNTOS y el forward tiene que sufrir EL MISMO
//  espejo, o la camara queda mirando justo para el otro lado. Ese espejo se lee de
//  Curve::signoZ -- UN dato del archivo -- en vez de estar acordado de palabra entre
//  esta formula y la del cargador: antes eran dos cuentas atadas tacitamente, y el
//  que arreglara una daba vuelta la camara.
//  Comprobado contra un riel real autoreado con la Z espejada: en su nodo 900 el
//  look-at da (-0.007, -0.047, 0.999), que es lo que sale con yaw~0 / pitch~-2.7.
// ---------------------------------------------------------------------------
void Camera::AplicarMiradaDelRiel() {
    if (!MiradaLaManejaElRiel()) return;
    Vector3 pyr(0, 0, 0);
    float fovRiel = fov;
    if (!Riel->RotacionEnNodo(rielIndice, &pyr, &fovRiel)) return;
    const float k = 3.14159265358979323846f / 180.0f;
    const float p = pyr.x * k, y = pyr.y * k;
    const float cp = cosf(p);
    const float sz = Riel->signoZ;   // el MISMO espejo que sufrieron los puntos
    MirarHacia(Vector3(-sinf(y) * cp, sinf(p), sz * -cosf(y) * cp), pyr.z);
    // el FOV autoral, si el riel lo trae. Ojo: escribe Camera::fov, que SE GUARDA en
    // el .w3d (mismo horneado que this->pos, ver arriba); es deterministico -depende
    // del riel y del target, no de que viewport dibujo ultimo- y solo pasa con el
    // modo prendido y un .cap con lineas 'f'.
    if (Riel->fovNodo && fovRiel > 1.0f && fovRiel < 179.0f) fov = fovRiel;
}

Camera::~Camera() {
    // si esta camara era la ACTIVA, soltar el puntero global: sino queda
    // COLGADO y el render (CameraActive->UpdatePosition/UpdateLookAt) crashea
    // al borrar la camara. (mismo problema que tenia la luz)
    if (CameraActive == this) CameraActive = NULL;
}
// ============================================================================
//  INSTALACION DE LOS HOOKS DE CAMARA PARA LOS BINDS DE LUA
//
//  pantallaDe / setRiel / setRielNodo / setMiradaRiel / rielDe viven en
//  script/BindsJuego.cpp, que se compila IGUAL para el Play del editor y para el
//  juego compilado, y por eso no puede incluir Camera.h (arrastra variables.h,
//  que es solo del editor). Antes eso se resolvia con `#ifdef W3D_GAME_RUNTIME`:
//  en un binario compilado los cinco binds devolvian 0,0,false / false y no
//  hacian NADA, en silencio -- o sea que el mismo .lua se comportaba distinto en
//  Play y compilado, justo lo contrario del contrato de BindsJuego.h.
//
//  Ahora los binds llaman a un hook y quien lo instala es este archivo: el que
//  linkea el objeto Camera. Cualquier build que traiga camara tiene los binds
//  funcionando de verdad, y donde no haya camara el bind sigue siendo un no-op
//  seguro (el hook queda en NULL). Es el MISMO patron que ya usaban
//  W3dPVSSetSectorHook y W3dParticulasEmitirHook.
// ============================================================================
#include "script/BindsJuego.h"
#include "objects/CameraBase.h"     // View/ProjectionMatrix para proyectar mundo -> lienzo
#include "objects/Curve.h"

namespace {

Camera* ComoCamara(Object* o) {
    return (o && o->getType() == ObjectType::camera) ? (Camera*)o : NULL;
}

// mundo -> px del lienzo (centro 0,0), + si el objeto esta DELANTE de la camara.
// Usa la camara ACTIVA con SU lente y el aspecto del render (los mismos numeros
// del modo juego; antes del primer render cae al aspecto del lienzo).
// OJO: con camara ORTOGRAFICA proyecta con la lente en perspectiva (aproximado).
bool ProyectarAPantalla(Object* obj, float lienzoW, float lienzoH,
                        float* sx, float* sy, bool* delante) {
    if (!obj || !CameraActive) return false;
    CameraBase cam;
    cam.pos   = CameraActive->GetGlobalPosition();
    cam.rot   = CameraActive->Rot();
    cam.fov   = CameraActive->fov;
    cam.nearZ = CameraActive->nearClip;
    cam.farZ  = CameraActive->farClip;
    // el MISMO aspecto que dibuja el modo juego: el DECLARADO por la camara (propio
    // o del riel) manda; sino el del render; sino (antes del primer render) el lienzo.
    float aspect = CameraActive->AspectoDeclarado();
    if (aspect <= 0.01f) aspect = (g_renderAspect > 1e-4f) ? g_renderAspect
                                            : ((lienzoH > 0.0f) ? lienzoW / lienzoH : 1.0f);
    Matrix4 M = cam.ProjectionMatrix(aspect) * cam.ViewMatrix();
    Vector3 p = obj->GetGlobalPosition();
    const float* m = M.m;   // column-major: fila i = (m[i], m[4+i], m[8+i], m[12+i])
    float cx = m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12];
    float cy = m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13];
    float cw = m[3]*p.x + m[7]*p.y + m[11]*p.z + m[15];
    *delante = cw > 1e-4f;                       // clip.w > 0 = delante del ojo
    if (cw > -1e-6f && cw < 1e-6f) cw = 1e-6f;   // no dividir por cero en el plano de la camara
    *sx =  (cx / cw) * lienzoW * 0.5f;           // NDC -> px de lienzo (centro 0,0)
    *sy = -(cy / cw) * lienzoH * 0.5f;           // y de GL sube; el lienzo BAJA (posPx)
    return true;
}

// El riel llega por OBJETO o por NOMBRE. 'soltar' (el lua paso nil) es el pedido
// explicito de dejar la camara donde esta; un nombre que no existe es error.
bool RielSet(Object* camObj, Object* curvaObj, const char* curvaNombre,
             bool soltar, const int* offset) {
    Camera* cam = ComoCamara(camObj);
    if (!cam) return false;
    Curve* cv = NULL;
    if (!soltar) {
        Object* o = curvaObj;
        if (!o && curvaNombre) o = FindObjectByName(SceneCollection, curvaNombre);
        cv = (o && o->getType() == ObjectType::curve) ? (Curve*)o : NULL;
        if (!cv) return false;                  // pidieron un riel que no existe
    }
    if (offset) cam->offsetRiel = *offset;
    return cam->SetRielObjeto(cv);
}

bool RielNodo(Object* camObj, float nodo) {
    Camera* cam = ComoCamara(camObj);
    if (!cam) return false;
    cam->rielNodo = (nodo < 0.0f) ? -1.0f : nodo;
    cam->UpdatePosition();   // que el cambio se vea en ESTE tick, como setRiel
    return true;
}

bool RielMirada(Object* camObj, bool on) {
    Camera* cam = ComoCamara(camObj);
    if (!cam) return false;
    cam->usarRotacionDelRiel = on;
    cam->UpdatePosition();   // que se vea en ESTE tick, como setRiel/setRielNodo
    cam->UpdateLookAt();     // (no hace nada si ahora manda el riel; reencuadra si se apago)
    return true;
}

bool RielEstado(Object* camObj, const char** nombre, int* offset, float* nodo, bool* mirada,
                float* indice) {
    Camera* cam = ComoCamara(camObj);
    if (!cam) return false;
    *nombre = cam->Riel ? cam->Riel->name.c_str() : "";
    *offset = cam->offsetRiel;
    *nodo   = cam->rielNodo;
    *mirada = cam->usarRotacionDelRiel;
    // el INDICE FRACCIONARIO real de este tick (seguimiento o nodo manual, con el
    // offset ya restado): el ancla que espera setVisCurvaT en una VisZona modo curva.
    if (indice) *indice = cam->Riel ? cam->rielIndice : -1.0f;
    return true;
}

// LENTE: fov + planos de recorte. Se validan igual que al abrir un .w3d, para que
// un valor absurdo desde un script no deje la camara sin nada que dibujar.
bool LenteLeer(Object* camObj, float* fov, float* cerca, float* lejos) {
    Camera* cam = ComoCamara(camObj);
    if (!cam) return false;
    *fov = cam->fov; *cerca = cam->nearClip; *lejos = cam->farClip;
    return true;
}

bool LenteEscribir(Object* camObj, const float* fov, const float* cerca, const float* lejos) {
    Camera* cam = ComoCamara(camObj);
    if (!cam) return false;
    if (fov   && *fov   > 1.0f && *fov < 179.0f) cam->fov      = *fov;
    if (cerca && *cerca > 0.0f)                  cam->nearClip = *cerca;
    if (lejos && *lejos > cam->nearClip)         cam->farClip  = *lejos;
    // si movieron el cercano por encima del lejano, correr el lejano detras suyo:
    // un frustum invertido no dibuja nada y el sintoma seria "se apago la pantalla"
    if (cam->farClip <= cam->nearClip) cam->farClip = cam->nearClip * 1000.0f;
    g_redraw = true;
    return true;
}

// camaraXZ(): el ADELANTE y la DERECHA de la camara ACTIVA aplastados al piso.
// Es con lo que un juego mueve al personaje "relativo a como estas mirando".
// Vivia en ScriptUI.cpp (registro SOLO del editor), asi que en el juego
// compilado el bind no existia y el .lua que lo llamaba se caia por frame.
bool CamaraXZ(float* fx, float* fz, float* rx, float* rz) {
    if (!CameraActive) return false;
    Vector3 f = CameraActive->forwardVector, r = CameraActive->rightVector;
    Vector3 fXZ = Vector3(f.x, 0.0f, f.z).Normalized();
    Vector3 rXZ = Vector3(r.x, 0.0f, r.z).Normalized();
    *fx = fXZ.x; *fz = fXZ.z; *rx = rXZ.x; *rz = rXZ.z;
    return true;
}

// constructor estatico simple: corre antes de main (igual que los otros hooks)
struct InstalarHooksCamara {
    InstalarHooksCamara() {
        W3dCamaraXZHook         = CamaraXZ;
        W3dCamaraProyectarHook  = ProyectarAPantalla;
        W3dCamaraRielSetHook    = RielSet;
        W3dCamaraRielNodoHook   = RielNodo;
        W3dCamaraRielMiradaHook = RielMirada;
        W3dCamaraRielEstadoHook = RielEstado;
        W3dCamaraLenteHook      = LenteLeer;
        W3dCamaraSetLenteHook   = LenteEscribir;
    }
};
InstalarHooksCamara gInstalarHooksCamara;

}  // namespace

// ============================================================================
//  EL TICK DE JUEGO DE LAS CAMARAS CON RIEL (lo llama TickReal, SimJuego.cpp).
//
//  El indice fraccionario del riel (rielIndice, lo que devuelve rielDe() y lo
//  que una VisZona modo curva espera en setVisCurvaT) lo horneaba SOLO el
//  camino de DIBUJO (Viewport3D::UpdateViewOrbit -> UpdatePosition). En ticks
//  sin render -- el harness con `simplay`, o cualquier tick que corra antes
//  del primer frame -- los scripts leian el indice VIEJO y la playlist de
//  celdas recortaba la malla con la lista de OTRO nodo: en el material de
//  referencia eso se veia como triangulos faltantes (cunas negras) con la
//  camara de juego. Hornearlo tambien en el tick deja rielDe() fresco siempre;
//  UpdatePosition es deterministica e idempotente (depende del riel y del
//  target, no del viewport), asi que el dibujo que venga despues recalcula
//  exactamente lo mismo.
// ============================================================================
void W3dCamarasRielTick() {
    if (!SceneCollection) return;
    std::vector<Object*> st;
    st.push_back(SceneCollection);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (o->getType() == ObjectType::camera) {
            Camera* c = (Camera*)o;
            if (c->Riel) c->UpdatePosition();
        }
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
}
