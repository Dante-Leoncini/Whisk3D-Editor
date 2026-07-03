#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "ViewPorts/ViewPort3D.h"
#include "Undo.h" // Ctrl+Z: confirmar transform
#include "objects/CameraBase.h" // camara base del core (la vista)
#include <cmath>
#include <cstdio>  // sprintf (formateo portable de la barra de estado)
#include <string>
#include "WhiskUI/glesdraw.h"
#include "ui/W3dColors.h" // W3dColores: colores del editor (piso, ejes de transformacion)
#include "render/OpcionesRender.h" // flags del overlay de normales
#include "objects/Mesh.h"          // overlay de estadisticas (vertsAgrupados, faces3d)
#include "objects/EditMesh.h"      // foco al centro de la seleccion en edit mode
#include "ViewPorts/LayoutInput.h" // LayoutDeleteEdit (menu Delete en edit mode)
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup al borrar con la tecla)
#ifdef W3D_SYMBIAN
extern int W3dPantallaAlto; // los headers GLES + tipos GL los da GeometriaUI.h (via ViewPort3D.h)
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_COORD_REPLACE
#define GL_COORD_REPLACE 0x8862
#endif
#endif

PopupMenu* MenuAdd = NULL;
PopupMenu* MenuSelect = NULL;    // seleccion: All / None / Invert
PopupMenu* MenuObject = NULL;    // operaciones de objeto (solo si hay seleccion)
PopupMenu* MenuMesh = NULL;      // edit mode: operaciones de malla (Transform + Extrude)
PopupMenu* MenuTransform = NULL; // submenu de Object: Move / Rotate / Scale
PopupMenu* MenuSetOrigin = NULL; // submenu de Object: Geometry/Origin/Cursor
PopupMenu* MenuApply = NULL;     // submenu de Object: Apply Location/Rotation/Scale/All (Ctrl A)
PopupMenu* MenuView = NULL;      // boton "View" (antes de Select): submenus Cameras + Viewpoint
PopupMenu* MenuViewpoint = NULL; // submenu de View: Camera/Top/Bottom/Front/Back/Right/Left (numpad)
PopupMenu* MenuCameras = NULL;   // submenu de View: Set Active Object as Camera / Active Camera
PopupMenu* MenuOverlays = NULL;  // overlays del viewport (checkboxes)
PopupMenu* MenuRender = NULL;    // modo de vista: Render/Material/Solid/Wireframe
PopupMenu* MenuOrient = NULL;    // orientacion de transform: Global/Local/View
PopupMenu* MenuMode = NULL;      // modo del objeto activo: Object/Edit/Vertex/Weight/Texture
PopupMenu* MenuSelMode = NULL;   // edit mode: sub-elemento Vertex/Edge/Face

// busca un boton de la barra por su ROL (no por indice) -> reordenar la barra no rompe nada.
Button* BarRolBtn(std::vector<Button*>& B, int rol){
    for (size_t i = 0; i < B.size(); i++) if (B[i] && B[i]->rol == rol) return B[i];
    return NULL;
}
int BarRolIdx(std::vector<Button*>& B, int rol){
    for (size_t i = 0; i < B.size(); i++) if (B[i] && B[i]->rol == rol) return (int)i;
    return -1;
}

// "rotar desde la vista" (trackball): el angulo lo da la posicion del mouse
// alrededor del pivot EN pantalla. gTrackballCap = ya capturamos el angulo
// inicial; los Track* son endpoints (coords de pantalla del viewport) de la
// linea punteada pivot->mouse.
// gTrackballCap es GLOBAL (variables.h): SetRotacion() lo resetea al arrancar
static float gTrackballAng0 = 0.0f;
static float gTrackPivX = 0, gTrackPivY = 0, gTrackMouseX = 0, gTrackMouseY = 0;
static bool  gLineaValida = false; // hay endpoints validos para la linea punteada

Viewport3D::Viewport3D(Vector3 pos){
    // barra: [0] tipo de viewport (BarCrear), [1] Select, [2] Add,
    //        [3] Object (solo con algo seleccionado), [4] Overlays
    // barra: [0] tipo de viewport, [1] Mode (selector de modo, con icono),
    // [2] SelMode (Vertex/Edge/Face, SOLO en edit mode, al lado de Mode),
    // [3] Select, [4] Add, [5] Object, [6] Overlays, [7] Render, [8] Orient.
    BarCrear(); // [0] = icono de tipo/split
    // Cada boton lleva su ROL estable; el dispatch los busca por rol (BarRolBtn) -> el ORDEN
    // VISUAL de aca se puede cambiar libremente sin tocar el dispatch. Orient va ANTES de Select.
    Button* b;
    b = new Button("Object Mode", (int)IconType::object); b->rol = BR_Mode;
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo si el activo es malla
    b = new Button("", (int)IconType::selVertex); b->rol = BR_SelMode;     // SOLO icono (Vertex/Edge/Face)
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo en edit mode
    b = new Button("", (int)IconType::pivotMedian); b->rol = BR_Pivot;     // Transform Pivot (solo icono)
        b->desplegable = true; BarButtons.push_back(b);
    b = new Button("Orient"); b->rol = BR_Orient;                          // orientacion del transform
        b->desplegable = true; BarButtons.push_back(b);                    // (movido ANTES de Select)
    b = new Button("View"); b->rol = BR_View; b->desplegable = true; BarButtons.push_back(b); // Viewpoint (antes de Select)
    b = new Button("Select"); b->rol = BR_Select; b->desplegable = true; BarButtons.push_back(b);
    b = new Button("Add"); b->rol = BR_Add; b->desplegable = true; BarButtons.push_back(b);
    b = new Button("Object"); b->rol = BR_Object;
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo con seleccion (no edit)
    b = new Button("Overlays"); b->rol = BR_Overlays; b->desplegable = true; BarButtons.push_back(b);
    b = new Button("Render"); b->rol = BR_Render; b->desplegable = true; BarButtons.push_back(b);
    b = new Button("UV"); b->rol = BR_UV;                                  // Mark Seam + proyecciones
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // SOLO en edit
    barAlpha = 0.5f; // en el 3D la barra deja ver la escena detras
    if (!MenuAdd){
        // el desplegable del Add (las opciones todavia no hacen nada)
        MenuAdd = new PopupMenu();
        MenuAdd->Agregar("Plane", 0, IconType::plane);
        MenuAdd->Agregar("Cube", 1, IconType::object);
        MenuAdd->Agregar("Circle", 2, IconType::circle);
        MenuAdd->Agregar("UV Sphere", 12, IconType::circle); // mismo icono que circle
        MenuAdd->Agregar("Cone", 13, IconType::cono);
        MenuAdd->Agregar("Cylinder", 14, IconType::cilindro);
        MenuAdd->Agregar("Vertex", 3, IconType::mesh);
        MenuAdd->Agregar("Empty", 4, IconType::empty);
        MenuAdd->Agregar("Camera", 5, IconType::camera);
        MenuAdd->Agregar("Light", 6, IconType::light);
        MenuAdd->Agregar("Collection", 8, IconType::archive);
        MenuAdd->Agregar("import Wavefront", 7, IconType::archive);
        // objetos especiales linkeados a un objeto (target): renderizan a otro
        MenuAdd->Agregar("Duplicate Linked", 9, IconType::instance);
        MenuAdd->Agregar("Array", 10, IconType::array);
        MenuAdd->Agregar("Mirror", 11, IconType::mirror);
    }
    if (!MenuSelect){
        MenuSelect = new PopupMenu();
        MenuSelect->Agregar("All", 0)->atajo = "A";
        MenuSelect->Agregar("None", 1)->atajo = "Alt A";
        MenuSelect->Agregar("Invert", 2)->atajo = "Ctrl I";
    }
    if (!MenuObject){
        // submenu Transform (Move/Rotate/Scale = G/R/S)
        MenuTransform = new PopupMenu();
        MenuTransform->Agregar("Move", 100)->atajo = "G";
        MenuTransform->Agregar("Rotate", 101)->atajo = "R";
        MenuTransform->Agregar("Scale", 102)->atajo = "S";
        // submenu Set Origin (mueve el origen y/o la geometria)
        MenuSetOrigin = new PopupMenu();
        MenuSetOrigin->Agregar("Geometry to Origin", 200);
        MenuSetOrigin->Agregar("Origin to Geometry", 201);
        MenuSetOrigin->Agregar("Origin to 3D Cursor", 202);
        // submenu Apply (Ctrl A): hornea el transform en la malla (ids 220-223 -> LayoutAccionObject -> AplicarTransform)
        MenuApply = new PopupMenu();
        MenuApply->titulo = "Apply"; // titulo visible tambien cuando se abre standalone con Ctrl+A
        MenuApply->Agregar("Location", 220);
        MenuApply->Agregar("Rotation", 221);
        MenuApply->Agregar("Scale", 222);
        MenuApply->Agregar("All Transforms", 223);
        MenuObject = new PopupMenu();
        MenuObject->Agregar("Transform", 0, -1, MenuTransform);    // abre submenu
        MenuObject->Agregar("Set Origin", 4, -1, MenuSetOrigin);  // abre submenu
        MenuObject->Agregar("Apply", 6, -1, MenuApply)->atajo = "Ctrl A"; // submenu Location/Rotation/Scale/All
        MenuObject->Agregar("Duplicate Objects", 1)->atajo = "Shift D";
        MenuObject->Agregar("Duplicate Linked", 2)->atajo = "Alt D";
        MenuObject->Agregar("Join", 5)->atajo = "Ctrl J"; // une las mallas seleccionadas en el objeto activo
        MenuObject->Agregar("Delete", 3)->atajo = "X";
        // menu "Mesh" (Edit Mode): submenus Vertex/Edge/Face/UV (estilo Blender) +
        // Transform. Comparte la accion con Object (LayoutAccionObject maneja todos
        // los ids: 100-102 transform, 300 extrude, 310 F, 314 duplicate, 315 UV).
        PopupMenu* mV = new PopupMenu();
        mV->Agregar("New Edge/Face from Vertices", 310)->atajo = "F";
        mV->Agregar("Extrude Vertices", 300)->atajo = "E";
        mV->Agregar("Duplicate", 314)->atajo = "Shift D";
        PopupMenu* mE = new PopupMenu();
        mE->Agregar("Extrude Edges", 300)->atajo = "E";
        mE->Agregar("Duplicate", 314)->atajo = "Shift D";
        PopupMenu* mF = new PopupMenu();
        mF->Agregar("Extrude Faces", 300)->atajo = "E";
        mF->Agregar("Duplicate", 314)->atajo = "Shift D";
        PopupMenu* mUV = new PopupMenu();
        mUV->Agregar("Unwrap", 315); // (pendiente)
        MenuMesh = new PopupMenu();
        MenuMesh->Agregar("Transform", 0, -1, MenuTransform);
        MenuMesh->Agregar("Vertex", 0, -1, mV);
        MenuMesh->Agregar("Edge", 0, -1, mE);
        MenuMesh->Agregar("Face", 0, -1, mF);
        MenuMesh->Agregar("UV", 0, -1, mUV);
    }
    if (!MenuView){
        // boton "View" (antes de Select): submenu Viewpoint con los 7 puntos de vista (mismos atajos del numpad).
        // ids 400-406 -> LayoutAccionView -> Viewport3DActive->SetViewpoint(...). Antes eran atajos "ocultos".
        MenuViewpoint = new PopupMenu();
        MenuViewpoint->Agregar("Camera", 400)->atajo = "Num 0";
        MenuViewpoint->Agregar("Top",    401)->atajo = "Num 7";
        MenuViewpoint->Agregar("Bottom", 402)->atajo = "Ctrl Num 7";
        MenuViewpoint->Agregar("Front",  403)->atajo = "Num 1";
        MenuViewpoint->Agregar("Back",   404)->atajo = "Ctrl Num 1";
        MenuViewpoint->Agregar("Right",  405)->atajo = "Num 3";
        MenuViewpoint->Agregar("Left",   406)->atajo = "Ctrl Num 3";
        // submenu Cameras: setear el objeto activo como camara / ver desde la camara activa
        MenuCameras = new PopupMenu();
        MenuCameras->Agregar("Set Active Object as Camera", 410)->atajo = "Ctrl Num 0";
        MenuCameras->Agregar("Active Camera",               411)->atajo = "Num 0";
        MenuView = new PopupMenu();
        MenuView->Agregar("Cameras",   0, -1, MenuCameras);   // abre submenu (antes de Viewpoint, como Blender)
        MenuView->Agregar("Viewpoint", 0, -1, MenuViewpoint); // abre submenu
    }
    if (!MenuRender){
        MenuRender = new PopupMenu();
        MenuRender->Agregar("Render Preview", 0);
        MenuRender->Agregar("Material Preview", 1);
        MenuRender->Agregar("Solid Preview", 2);
        MenuRender->Agregar("Wireframe Preview", 3);
    }
    if (!MenuOrient){
        // orientacion usada al constrenir a un eje (X/Y/Z) y por el extrude. Default Global.
        MenuOrient = new PopupMenu();
        MenuOrient->titulo = "Transform Orientations";
        MenuOrient->Agregar("Global", 0);
        MenuOrient->Agregar("Local", 1);
        MenuOrient->Agregar("Normal", 3); // = la direccion de la normal (lo que hace el extrude)
        MenuOrient->Agregar("View", 2);
    }
    if (!MenuMode){
        // modo del objeto activo (solo malla). Edit y los Paint todavia no
        // hacen nada: dejamos la opcion para mas adelante.
        MenuMode = new PopupMenu();
        MenuMode->Agregar("Object Mode", 0, IconType::object); // icono de objeto
        MenuMode->Agregar("Edit Mode", 1, IconType::mesh);     // icono de malla 3d
        MenuMode->Agregar("Vertex Paint", 2, IconType::mesh);
        MenuMode->Agregar("Weight Paint", 3, IconType::mesh);
        MenuMode->Agregar("Texture Paint", 4, IconType::mesh);
    }
    if (!MenuSelMode){
        // sub-elemento de Edit Mode (la seleccion -todo/nada/invertir- y el render
        // se refieren a esto). Vertex es el default.
        MenuSelMode = new PopupMenu();
        MenuSelMode->Agregar("Vertex", SelVertex, IconType::selVertex);
        MenuSelMode->Agregar("Edge",   SelEdge,   IconType::selEdge);
        MenuSelMode->Agregar("Face",   SelFace,   IconType::selFace);
    }
    // (eran inicializadores de clase: C++03)
    orthographic = false;
    ViewFromCameraActive = false;
    showOverlays = true;
    ShowUi = true;
    showFloor = true;
    showYaxis = true;
    showXaxis = true;
    CameraToView = false;
    showOrigins = true;
    show3DCursor = true;
    ShowRelantionshipsLines = true;
    limpiarPantalla = true;
    view = RenderType::MaterialPreview;
    nearClip = 0.01f;
    farClip = 1000.0f;
    aspect = 1.0f;
    viewRot = Quaternion::FromEuler(-30.0f, -23.0f, 0.0f);
    orbitDistance = 10.0f;

    RecalcOrbitPosition();
}

Viewport3D::~Viewport3D() {};

void Viewport3D::AbrirMenuOverlays(int x, int y){
    if (!MenuOverlays){
        MenuOverlays = new PopupMenu();
    }
    // se reconstruye en cada apertura apuntando a los flags de ESTE viewport
    // (cada instancia 3D tiene los suyos: split view = overlays independientes).
    // Sin titulo: el primer item ES el master "Show Overlays". Si esta off el
    // resto se ve en gris (->gris = &showOverlays) porque sin overlay no se ven.
    MenuOverlays->Limpiar();
    MenuOverlays->AgregarCheck("Show Overlays", 0, &showOverlays);
    MenuOverlays->AgregarCheck("Floor", 1, &showFloor)->gris = &showOverlays;
    MenuOverlays->AgregarCheck("X Axis", 2, &showXaxis)->gris = &showOverlays;
    MenuOverlays->AgregarCheck("Y Axis", 3, &showYaxis)->gris = &showOverlays;
    MenuOverlays->AgregarCheck("Origins", 4, &showOrigins)->gris = &showOverlays;
    MenuOverlays->AgregarCheck("3D Cursor", 5, &show3DCursor)->gris = &showOverlays;
    MenuOverlays->AgregarCheck("Relationship Lines", 6, &ShowRelantionshipsLines)->gris = &showOverlays;
    // Normales (solo en meshes seleccionadas): 3 toggles + slider de tamano
    MenuOverlays->AgregarCheck("Vertex Normal", 7, &OverlayVertexNormal, IconType::normalVertex)->gris = &showOverlays;
    MenuOverlays->AgregarCheck("Custom Normal", 8, &OverlayCustomNormal, IconType::normalCustom)->gris = &showOverlays;
    MenuOverlays->AgregarCheck("Face Normal",   9, &OverlayFaceNormal,   IconType::normalFace)->gris = &showOverlays;
    MenuOverlays->AgregarFloat("Normal Size",  10, &OverlayNormalSize, 0.0f, 1.0f)->gris = &showOverlays;
    // texto blanco arriba a la derecha del viewport
    MenuOverlays->AgregarCheck("Statistics", 11, &OverlayStatistics)->gris = &showOverlays;
    MenuOverlays->AgregarCheck("FPS",        12, &OverlayFps)->gris = &showOverlays;
    // Clear Screen: limpia el framebuffer (glClear) cada frame. NO es un overlay (no se grisa con
    // Show Overlays). Apagarlo gana rendimiento en juegos/renders donde la escena llena la pantalla.
    // ON por defecto (limpiarPantalla = true en el ctor).
    MenuOverlays->AgregarCheck("Clear Screen", 13, &limpiarPantalla);
    MenuOverlays->action = NULL; // el toggle lo hace el propio item (checkbox)
    MenuOverlays->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = MenuOverlays;
}

void Viewport3D::SetLimpiarPantalla(bool value) {
    limpiarPantalla = value;
}

#ifndef W3D_SYMBIAN
void Viewport3D::event_mouse_wheel(SDL_Event &e) {
    // si el mouse esta sobre la BARRA, la rueda la scrollea (en PC no entran
    // todos los botones); si no, hace zoom de la camara.
    int mx, my; SDL_GetMouseState(&mx, &my);
    int barH = BarHeight();
    int yBar = barAbajo ? (y + height - barH) : y;
    if (mx >= x && mx < x + width && my >= yBar && my < yBar + barH) {
        barScrollManual -= (int)(e.wheel.y * 40); // rueda arriba = scroll a la izquierda
        if (barScrollManual < 0) barScrollManual = 0;
        ActualizarBarra(); // re-clampea contra el ancho total
        return;
    }
    if (!ViewFromCameraActive) {
        Zoom(e.wheel.y* 2.0f); //podria multiplciarse por un valor por sensibilidad  * 1.0f
    }
}
#endif

// suma al radio del foco el bounding de cada objeto seleccionado (esfera centrada en 'c' que
// envuelve la seleccion entera): max(|c - PuntoFoco| + RadioFoco) sobre los seleccionados.
static void CalcRadioFocoRec(Object* obj, const Vector3& c, float& r){
    if (obj->select){
        float d = (obj->PuntoFoco() - c).Length() + obj->RadioFoco();
        if (d > r) r = d;
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) CalcRadioFocoRec(obj->Childrens[i], c, r);
}

void Viewport3D::EnfocarObject() {
    // Edit Mode: enfocar la SELECCION de sub-elementos (vertices/aristas/caras): centro + radio
    // -> centra Y ajusta el zoom. Si no hay nada seleccionado, cae al foco del objeto.
    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        float cx, cy, cz, rL;
        if (m->edit && m->edit->CentroRadioSeleccion(cx, cy, cz, rL)) {
            Vector3 c = m->LocalAMundo(Vector3(cx, cy, cz));
            EncuadrarRadio(c, m->EscalarRadioLocal(Vector3(cx, cy, cz), rL)); // radio local -> mundo
            return;
        }
    }
    if (!ObjSelects.empty()) {
        // el FOCO mira el CENTRO GEOMETRICO de la seleccion (las mallas aportan su centro de
        // vertices), NO el pivote de transform. El radio envuelve toda la seleccion (con su tamaño).
        Vector3 c = CentroFocoSeleccion();
        float r = 0.0f;
        if (SceneCollection)
            for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
                CalcRadioFocoRec(SceneCollection->Childrens[i], c, r);
        EncuadrarRadio(c, r);
    }
}

void Viewport3D::EncuadrarRadio(const Vector3& centro, float radio){
    pivot = centro;
    if (radio < 0.05f) radio = 0.05f; // un vertice/punto: radio minimo (zoom cercano pero no degenerado)
    // distancia para que una esfera de 'radio' entre en el FOV. Se usa el menor half-FOV (vertical vs
    // horizontal, segun el aspect) para que entre en las DOS direcciones. *1.25 = padding pedido.
    float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    float halfV = fovDeg * 0.5f * 3.14159265f / 180.0f;
    float halfH = atanf(tanf(halfV) * aspect);
    float ang = halfV < halfH ? halfV : halfH;
    float s = sinf(ang); if (s < 0.01f) s = 0.01f;
    orbitDistance = radio / s * 1.25f;
    if (orbitDistance < 0.02f)     orbitDistance = 0.02f;
    if (orbitDistance > farClip)   orbitDistance = farClip;
    RecalcOrbitPosition();
}

void Viewport3D::Zoom(float delta){
    // zoom PROPORCIONAL a la distancia (multiplicativo/exponencial): el paso se adapta al tamaño de
    // lo que se ve. Cerca de algo chico el paso es chico (suave, usable en vertices/triangulos);
    // lejos de algo grande el paso es grande (fuerte). delta>0 = acercar; ~12% por notch de rueda.
    orbitDistance *= expf(-delta * 0.06f);
    if (orbitDistance < 0.02f)   orbitDistance = 0.02f;
    if (orbitDistance > farClip) orbitDistance = farClip;
    RecalcOrbitPosition();
}

void Viewport3D::UpdateViewOrbit() {
    if (CameraActive){
        CameraActive->UpdatePosition();
        CameraActive->UpdateLookAt();
    }

    // La VISTA la arma la camara BASE del core (CameraBase): seteamos su pos/rot
    // segun la fuente (la camara activa del editor, o la orbita del viewport) y le
    // pedimos la matriz de vista. Antes esto estaba duplicado a mano en las 2 ramas.
    CameraBase cam;
    if (ViewFromCameraActive && CameraActive) {
        cam.pos = CameraActive->pos;
        cam.rot = CameraActive->rot;
    } else {
        cam.pos = viewPos;
        cam.rot = viewRot;
    }

    ::viewPosGlobal = cam.pos;
    ::rotGlobal     = cam.rot;
    g_renderCamPos  = cam.pos; // el chrome equirect calcula el reflejo respecto de esta camara
    g_renderCamRight   = cam.rot * Vector3(1, 0, 0); // base para el MATCAP por software (eye space del N95)
    g_renderCamUp      = cam.rot * Vector3(0, 1, 0);
    g_renderCamForward = cam.rot * Vector3(0, 0, -1); // hacia la escena
    // luz del NORMAL MAPPING: en RENDER preview usa la luz de ESCENA (Lights[0]) -> el relieve responde a la
    // lampara y su color. En material/solid preview usa la CAMARA (headlight) en blanco -> el material preview NO
    // depende de las luces de escena (Dante: "no se tendrian que ver las luces en material preview").
    if (view == RenderType::Rendered && !Lights.empty() && Lights[0]) {
        Matrix4 LW; Lights[0]->GetWorldMatrix(LW);
        g_renderLightPos   = Vector3(LW.m[12], LW.m[13], LW.m[14]);
        g_renderLightColor = Vector3(Lights[0]->diffuse[0], Lights[0]->diffuse[1], Lights[0]->diffuse[2]);
    } else {
        g_renderLightPos   = cam.pos;          // headlight neutro
        g_renderLightColor = Vector3(1, 1, 1); // blanco
    }

    w3dEngine::LoadMatrix(cam.ViewMatrix().m);
}

void Viewport3D::RotateOrbit() {
    float sens = 0.3f;

    // Usamos dx y dy como deltas directos
    float deltaYaw = -dx * sens;
    float deltaPitch = -dy * sens;

    // 1. Crear cuaternión de Yaw (Eje Y Global)
    Quaternion qYaw = Quaternion::FromAxisAngle(Vector3(0, 1, 0), deltaYaw);

    // 2. Crear cuaternión de Pitch (Eje X Local)
    // Nota: Usamos (1,0,0) puro porque al multiplicar a la derecha,
    // el cuaternión interpreta esto como el eje X de la propia cámara.
    Quaternion qPitch = Quaternion::FromAxisAngle(Vector3(1, 0, 0), deltaPitch);

    // 3. Aplicar las rotaciones en orden "Sandwich":
    // Yaw Global (Izquierda) * viewRot Actual * Pitch Local (Derecha)
    viewRot = qYaw * viewRot * qPitch;

    // 4. Normalizar siempre para evitar deformaciones por errores de flotantes
    viewRot.normalize();

    RecalcOrbitPosition();
}

void Viewport3D::OrbitarFlecha(int ndx, int ndy){
    float odx = dx, ody = dy;        // preservar el delta del mouse
    dx = (float)ndx; dy = (float)ndy;
    RotateOrbit();
    dx = odx; dy = ody;
}

void Viewport3D::Pan(){
    ShiftCount = 100;
    const float speed = orbitDistance * 0.002f;

    // mover en el plano de la cámara
    Vector3 right = viewRot * Vector3(1,0,0);
    Vector3 up    = viewRot * Vector3(0,1,0);

    pivot = pivot - right * (dx * speed);
    pivot = pivot + up    * (dy * speed);

    RecalcOrbitPosition();
}

// paneo por TECLADO (mismo patron que OrbitarFlecha): setea dx/dy, panea, restaura. Lo usa el keypad del N95.
void Viewport3D::PanFlecha(int ndx, int ndy){
    float odx = dx, ody = dy;
    dx = (float)ndx; dy = (float)ndy;
    Pan();
    dx = odx; dy = ody;
}

// PRIMERA PERSONA (#+flechas en el N95): la camara NO se mueve, GIRA la mirada. Rota viewRot (yaw global +
// pitch local, igual que RotateOrbit) y recoloca el PIVOT adelante para que viewPos quede FIJO.
void Viewport3D::MirarPrimeraPersona(int ndx, int ndy){
    Vector3 savedPos = viewPos;          // la camara se queda donde esta
    float sens = 0.3f;
    Quaternion qYaw   = Quaternion::FromAxisAngle(Vector3(0, 1, 0), -(float)ndx * sens);
    Quaternion qPitch = Quaternion::FromAxisAngle(Vector3(1, 0, 0), -(float)ndy * sens);
    viewRot = qYaw * viewRot * qPitch;
    viewRot.normalize();
    Vector3 forward = viewRot * Vector3(0, 0, -1);
    pivot = savedPos + forward * orbitDistance; // el pivot va adelante -> RecalcOrbitPosition deja viewPos=savedPos
    RecalcOrbitPosition();
}

void Viewport3D::RollOrbit(float angleDeg) {
    // Eje Z local (0, 0, 1) o (0, 0, -1).
    // Usamos -1 para que sea consistente con la dirección de la vista (Forward).
    Quaternion qRoll = Quaternion::FromAxisAngle(Vector3(0, 0, -1), angleDeg);

    // Multiplicar por la DERECHA = Rotación Local
    viewRot = viewRot * qRoll;

    viewRot.normalize();
    RecalcOrbitPosition();
}

void Viewport3D::RecalcOrbitPosition(){
    Vector3 forward = viewRot * Vector3(0,0,-1);
    viewPos = pivot - forward * orbitDistance;

    // Extraer ejes locales desde el quaternion de la vista
    camRight   = viewRot * Vector3(1, 0, 0);
    camUp      = viewRot * Vector3(0, 1, 0);
    camForward = viewRot * Vector3(0, 0, -1);
}

bool Viewport3D::ProyectarPunto(const Vector3& p, float& sx, float& sy){
    // ejes de camara de ESTE viewport (no los globals, que pisa el ultimo
    // renderizado en multi-3D)
    Vector3 cr = viewRot * Vector3(1, 0, 0);
    Vector3 cu = viewRot * Vector3(0, 1, 0);
    Vector3 cf = viewRot * Vector3(0, 0, -1); // hacia la escena
    Vector3 rel = p - viewPos;
    float ez = rel.Dot(cf);                   // distancia hacia adelante
    if (ez < 0.0001f) return false;           // detras de la camara
    float ex = rel.Dot(cr);
    float ey = rel.Dot(cu);
    float aspectR = (height > 0) ? (float)width / (float)height : 1.0f;
    float ndcX, ndcY;
    if (orthographic) {
        // ORTOGRAFICA: sin division por ez (mismo extent que el Render: size=5)
        const float size = 5.0f;
        ndcX = ex / (size * aspectR);
        ndcY = ey / size;
    } else {
        float fRad = fovDeg * 3.14159265f / 180.0f;
        float f = 1.0f / tanf(fRad * 0.5f);
        ndcX = (ex * (f / aspectR)) / ez;
        ndcY = (ey * f) / ez;
    }
    sx = (ndcX * 0.5f + 0.5f) * (float)width;
    sy = (1.0f - (ndcY * 0.5f + 0.5f)) * (float)height; // pantalla: Y hacia abajo
    return true;
}

// pivote del GIZMO de transform (linea punteada + lineas de eje X/Y/Z): es el
// TransformPivotPoint (median / 3D cursor / active), NO el objeto activo (que solo
// coincide con el pivote en modo Active o si esta en el median). En Individual
// Origins cae al origen del activo (no hay un pivote unico).
static Vector3 GizmoPivot(){
    if (g_transformPivot == PivotIndividual && ObjActivo) return ObjActivo->GetGlobalPosition();
    return TransformPivotPoint;
}

void Viewport3D::ActualizarLineaTransform(int mx, int my){
    if ((estado == rotacion || estado == EditScale) && ObjActivo){
        float px, py;
        if (ProyectarPunto(GizmoPivot(), px, py)){
            gTrackPivX = px; gTrackPivY = py;
            gTrackMouseX = (float)mx - (float)x;
            gTrackMouseY = (float)my - (float)y;
            gLineaValida = true;
        }
    }
}

void Viewport3D::RotarDesdeVista(int mx, int my){
    if (!ObjActivo) return;
    float px, py;
    if (!ProyectarPunto(GizmoPivot(), px, py)) return; // el angulo se mide desde el PIVOTE
    float lmx = (float)mx - (float)x, lmy = (float)my - (float)y;
    float ang = 180.0f - atan2f(lmy - py, lmx - px) * 180.0f / 3.14159265f;
    if (!gTrackballCap) { gTrackballCap = true; gTrackballAng0 = ang; }
    float delta = ang - gTrackballAng0;
    gAnguloTransform = delta; // barra de estado (angulo del mouse)
    Vector3 cf = viewRot * Vector3(0, 0, -1);
    // -delta: el usuario lo quiere al reves (mouse horario -> objeto horario)
    // Edit Mode: el trackball gira los VERTICES seleccionados (rotacion absoluta).
    if (InteractionMode == EditMode && EditXformActivo()) {
        EditXformRotAbs(Quaternion::FromAxisAngle(cf, -delta));
        return;
    }
    for (size_t o = 0; o < estadoObjetos.size(); o++) {
        Object& ob = *estadoObjetos[o].obj;
        ob.rot = Quaternion::FromAxisAngle(cf, -delta) * estadoObjetos[o].rot;
        ob.rot.normalize();
        ob.rotEuler = ob.rot.ToEulerYXZ();
    }
    AplicarPivotATransform(); // gira las posiciones alrededor del pivote
}

void Viewport3D::SetViewpoint(Viewpoint value) {
    ViewFromCameraActive = false;
    CameraToView = false;

    switch (value) {
        case Viewpoint::front: {
            // Front: Mirando hacia -Z. Up es +Y. (Identidad)
            viewRot = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
            break;
        }
        case Viewpoint::back: {
            // Back: Mirando hacia +Z. Rotamos 180 en Y.
            viewRot = Quaternion::FromAxisAngle(Vector3(0,1,0), 180.0f);
            break;
        }
        case Viewpoint::right: {
            // Right: Mirando hacia -X. Rotamos 90 grados a la derecha (Y).
            // Nota: Dependiendo de tu convención puede ser 90 o -90.
            // Generalmente +90 en Y convierte el vector Forward (-Z) en (-X).
            viewRot = Quaternion::FromAxisAngle(Vector3(0,1,0), 90.0f);
            break;
        }
        case Viewpoint::left: {
            viewRot = Quaternion::FromAxisAngle(Vector3(0,1,0), -90.0f);
            break;
        }
        case Viewpoint::top: {
            // Top: Mirando hacia -Y. Up es -Z (para que el top de la pantalla sea Norte).
            // Esto es rotar X en 90 grados (pitch down).
            viewRot = Quaternion::FromAxisAngle(Vector3(1,0,0), -90.0f);
            break;
        }
        case Viewpoint::bottom: {
            // Bottom: Mirando hacia +Y.
            viewRot = Quaternion::FromAxisAngle(Vector3(1,0,0), 90.0f);
            break;
        }
    }

    // IMPORTANTE:
    // 1. Normalizar por seguridad (aunque los hardcodeados ya son unitarios)
    viewRot.normalize();

    // 2. Recalcular la posición física de la cámara basada en el nuevo ángulo y el pivote existente
    RecalcOrbitPosition();
}

// Ejemplo de implementación de un método
void Viewport3D::ReloadLights() {
    ::view = view;
    ::showOverlayGlobal = showOverlays;
    ::ViewFromCameraActiveGlobal = ViewFromCameraActive;
    // OJO: el ACTIVO lo decide el hover del mouse, no el render (con dos
    // viewports 3D, el ultimo dibujado robaba la orbita y el G/R/S)
    if (!Viewport3DActive) Viewport3DActive = this;

#ifdef W3D_SYMBIAN
    // N95: SOLO las luces de la escena. Apagar GL_LIGHT0..7 "a lo bestia" (aunque no existan) CUELGA el driver
    // GLES1 del MBX al arrancar (Dante: crash). Esto es lo que hacia el codigo original -> seguro en el N95.
    for (size_t l = 0; l < Lights.size(); l++) {
        w3dEngine::SetLightEnabled(Lights[l]->LightID, false);
    }
#else
    // PC: apaga TODOS los 8 GL lights. Si una luz cambio su numero de GL light el viejo slot quedaba prendido
    // (luz "fantasma" -> Dante: "quedan muchas luces prendidas"). Luego cada Light enciende el suyo.
    for (int i = 0; i < MAX_LIGHTS; i++) {
        w3dEngine::SetLightEnabled(GL_LIGHT0 + i, false);
    }
#endif

    switch(view){
        // Solid usa la MISMA luz que Material Preview (se ven igual; la unica
        // diferencia es que Solid no usa texturas ni los materiales reales)
        case RenderType::MaterialPreview:
        case RenderType::Solid:
            w3dEngine::Enable(w3dEngine::Light0);
            w3dEngine::Light0fv(w3dEngine::LightDiffuse, MaterialPreviewDiffuse);
            w3dEngine::Light0fv(w3dEngine::LightAmbient, MaterialPreviewAmbient);
            w3dEngine::Light0fv(w3dEngine::LightSpecular, MaterialPreviewSpecular);
            w3dEngine::Light0f(w3dEngine::LightConstantAtt, 1.0f);
            w3dEngine::Light0f(w3dEngine::LightLinearAtt, 0.0f);
            w3dEngine::Light0f(w3dEngine::LightQuadraticAtt, 0.0f);
            break;

        case RenderType::ZBuffer:
            w3dEngine::Disable(w3dEngine::Light0);
            break;

        default:
            break;
    }
}

// Cicla los modos de vista: Material Preview -> Render -> Wireframe -> Solid -> (vuelve)
void Viewport3D::ChangeViewType(){
    if      (view == RenderType::MaterialPreview) view = RenderType::Rendered;
    else if (view == RenderType::Rendered)        view = RenderType::Wireframe;
    else if (view == RenderType::Wireframe)       view = RenderType::Solid;
    else                                          view = RenderType::MaterialPreview; // Solid/otro -> vuelve
}

// Redimensiona el viewport
void Viewport3D::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH); // Llama a la función base
    ResizeBorder(newW, newH);         // Ajusta los bordes
    aspect = (float)newW / (float)newH;
}

// Mostrar u ocultar overlays
void Viewport3D::SetShowOverlays(bool valor) {
    showOverlays = valor;
}

void Viewport3D::Render() {
    ReloadLights();

    // Configuración de la matriz de proyección
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    if (orthographic) {
        float size = 5.0f;
    w3dEngine::Ortho(-size * aspect, size * aspect,
                -size, size,
                nearClip, farClip);
    }
    else {
        w3dEngine::Perspective(fovDeg, aspect, nearClip, farClip);
    }

    // Matriz de modelo
    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // Limpiar pantalla
    const int glY = W3dPantallaAlto - y - height; // arbol arriba-izq -> GL
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, width, height);

    if (view == RenderType::ZBuffer) {
        w3dEngine::Enable(w3dEngine::Fog);
        w3dEngine::FogMode(true);
        w3dEngine::FogStart(nearClip);
        w3dEngine::FogEnd(farClip);
        GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        w3dEngine::FogColor(fogColor);
    } else {
        w3dEngine::Disable(w3dEngine::Fog);
        w3dEngine::ClearColor(ListaColores[static_cast<int>(ColorID::background)][0], ListaColores[static_cast<int>(ColorID::background)][1],
                     ListaColores[static_cast<int>(ColorID::background)][2], ListaColores[static_cast<int>(ColorID::background)][3]);
    }

    if (limpiarPantalla) {
        if (view == RenderType::ZBuffer) {
            w3dEngine::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        } else if (view == RenderType::Rendered) {
            w3dEngine::ClearColor(scene->backgroundColor[0], scene->backgroundColor[1],
                         scene->backgroundColor[2], scene->backgroundColor[3]);
        } else {
            w3dEngine::ClearColor(ListaColores[static_cast<int>(ColorID::background)][0], ListaColores[static_cast<int>(ColorID::background)][1],
                         ListaColores[static_cast<int>(ColorID::background)][2], ListaColores[static_cast<int>(ColorID::background)][3]);
        }
        w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    } else {
        w3dEngine::Clear(w3dEngine::DepthBuffer);
    }

    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Viewport(x, glY, width, height);

    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Disable(w3dEngine::ColorMaterial);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    if (view == RenderType::Solid || view == RenderType::MaterialPreview) {
        w3dEngine::Light0fv(w3dEngine::LightPosition, MaterialPreviewPosition);
    }

    UpdateViewOrbit();

    w3dEngine::Enable(w3dEngine::DepthTest);

    // Dibujar overlays
    if (showOverlays) {
            w3dEngine::Material(w3dEngine::MatDiffuse,  ListaColores[static_cast<int>(ColorID::negro)]);
            w3dEngine::Material(w3dEngine::MatAmbient,  ListaColores[static_cast<int>(ColorID::negro)]);
            w3dEngine::Material(w3dEngine::MatSpecular, ListaColores[static_cast<int>(ColorID::negro)]);

        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Disable(w3dEngine::Lighting);
        w3dEngine::Disable(w3dEngine::Blend);

        w3dEngine::TexFilter(false);
        w3dEngine::TexFilter(false);
        w3dEngine::BlendAlpha();

        w3dEngine::Enable(w3dEngine::DepthTest);
        w3dEngine::Disable(w3dEngine::Texture2D);

        w3dEngine::DisableArray(w3dEngine::TexCoordArray);
        w3dEngine::DisableArray(w3dEngine::NormalArray);

        w3dEngine::Disable(w3dEngine::ColorMaterial);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        //w3dEngine::DepthMask(true);

        if (showOverlays && (showFloor || showXaxis || showYaxis)) RenderFloor();
        w3dEngine::DepthMask(true);
    }

    // Edit Mode: marcar la malla ACTIVA para que su render dibuje el overlay de
    // edicion (vertices como puntos + bordes encima) en vez del contorno de objeto.
    // (misma derivacion COMPARTIDA que usa el cambio de modo; PC la refresca por frame
    //  para seguir a ObjActivo, Symbian la setea al cambiar de modo)
    ActualizarEditMeshActivo();

    // master de overlays para el render del CORE (Mesh::Render lee g_mostrarOverlays para no dibujar
    // contornos de seleccion ni el overlay de edit). Se setea por frame = el showOverlays del viewport.
    g_mostrarOverlays = showOverlays;

    // Renderiza la escena recursivamente
    SceneCollection->Render();

    LoopCutRenderPreview(); // preview del corte (loop cut) encima de la escena

    if (showOverlays) RenderOverlay();
    if (ShowUi) RenderUI();
}

void Viewport3D::RenderFloor() {
    //hay un error en la malla 3d!!!
    //explico... resulta que el fog se calcula en el vertice
    //pero como el vertice esta fuera del nearClip y el farClip se ve mal casi siempre
    //excepto en ciertos angulos... parece un glich. pero es asi openGL viejo
    //la solucion seria poner un vertice en el medio de la linea. eso arreglaria bastante el problema
    //por ahora el fog se quita... triste
    w3dEngine::Enable(w3dEngine::Fog);
    w3dEngine::FogMode(true);
    w3dEngine::FogStart(nearClip);
    w3dEngine::FogEnd(30.0f);

    if (view == RenderType::Rendered) {
        w3dEngine::FogColor(scene->backgroundColor);
    } else {
        w3dEngine::FogColor(ListaColores[static_cast<int>(ColorID::background)]);
    }

    w3dEngine::LineWidth(1);
    w3dEngine::VertexPointer3f(0, objVertexdataFloor);

    // la grilla solo si "Floor" esta activo (antes se dibujaba siempre: por eso
    // apagar Floor no la ocultaba)
    if (showFloor) {
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPiso][0],
            W3dColores[W3dColor_LineaPiso][1],
            W3dColores[W3dColor_LineaPiso][2],
            W3dColores[W3dColor_LineaPiso][3]
        );
        W3dDrawLinesF(objVertexdataFloor, objFacedataFloor, objFacesFloor);
    }

    // Linea Roja
    if (showXaxis) {
        w3dEngine::LineWidth(2);
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPisoRoja][0],
            W3dColores[W3dColor_LineaPisoRoja][1],
            W3dColores[W3dColor_LineaPisoRoja][2],
            W3dColores[W3dColor_LineaPisoRoja][3]
        );
        W3dDrawLinesF(objVertexdataFloor, EjeRojo, 2);
        w3dEngine::LineWidth(1);
    } else if (showFloor) {
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPiso][0],
            W3dColores[W3dColor_LineaPiso][1],
            W3dColores[W3dColor_LineaPiso][2],
            W3dColores[W3dColor_LineaPiso][3]
        );
        W3dDrawLinesF(objVertexdataFloor, EjeRojo, 2);
    }

    // Linea Verde
    if (showYaxis) {
        w3dEngine::LineWidth(2);
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPisoVerde][0],
            W3dColores[W3dColor_LineaPisoVerde][1],
            W3dColores[W3dColor_LineaPisoVerde][2],
            W3dColores[W3dColor_LineaPisoVerde][3]
        );
        W3dDrawLinesF(objVertexdataFloor, EjeVerde, 2);
        w3dEngine::LineWidth(1);
    } else if (showFloor) {
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPiso][0],
            W3dColores[W3dColor_LineaPiso][1],
            W3dColores[W3dColor_LineaPiso][2],
            W3dColores[W3dColor_LineaPiso][3]
        );
        W3dDrawLinesF(objVertexdataFloor, EjeVerde, 2);
    }

    w3dEngine::Disable(w3dEngine::Fog);
}

// linea-guia por 'c' a lo largo de 'dir' (world space, larga a ambos lados)
static void DibujarLineaEjeMundo(const Vector3& c, const Vector3& dir, int colorIdx) {
    const float L = 1000.0f;
    GLfloat v[6] = {
        c.x - dir.x * L, c.y - dir.y * L, c.z - dir.z * L,
        c.x + dir.x * L, c.y + dir.y * L, c.z + dir.z * L
    };
    w3dEngine::Color4fv(W3dColores[colorIdx]);
    w3dEngine::VertexPointer3f(0, v);
    w3dEngine::DrawLines(2);
}

void Viewport3D::RenderAllAxisTransform() {
    if (!ObjActivo) return;
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::DisableArray(w3dEngine::NormalArray);        // arrays colgados: N95 quisquilloso
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::LineWidth(2);

    // las guias se dibujan en WORLD space, a lo largo del eje EN LA ORIENTACION
    // elegida (global/local/view). Un solo eje = una linea; plano = las dos del
    // plano (excluye la del shift); XYZ = las tres. Centradas en el PIVOTE (no en
    // el objeto activo, que solo coincide en modo Active).
    Vector3 c = GizmoPivot();
    // EXTRUDE (o move con orientacion NORMAL via gEVuseCustom): la direccion REAL es gTransformNormal (la normal en
    // mundo), NO un eje X/Y/Z. Sin esto se dibujaba la linea del eje viejo de axisSelect (rojo/X por defecto) aunque la
    // transformacion vaya por la normal. UNA linea a lo largo de la normal (color Z: la normal es el "Z" de su orientacion).
    // Al constreñir a un eje real (tecla X/Y/Z) gEVuseCustom se apaga -> vuelve a la linea del eje. Compartido 4 OS.
    if (gEVuseCustom) {
        DibujarLineaEjeMundo(c, gTransformNormal, W3dColor_ColorTransformZ);
        return;
    }
    int a = axisSelect;
    bool drawX = (a == X || a == XYZ || a == PlaneY || a == PlaneZ);
    bool drawY = (a == Y || a == XYZ || a == PlaneX || a == PlaneZ);
    bool drawZ = (a == Z || a == XYZ || a == PlaneX || a == PlaneY);
    if (drawX) DibujarLineaEjeMundo(c, EjeOrientado(*ObjActivo, X), W3dColor_ColorTransformX);
    if (drawY) DibujarLineaEjeMundo(c, EjeOrientado(*ObjActivo, Y), W3dColor_ColorTransformY);
    if (drawZ) DibujarLineaEjeMundo(c, EjeOrientado(*ObjActivo, Z), W3dColor_ColorTransformZ);
}

void Viewport3D::RenderOverlay() {
    w3dEngine::Material(w3dEngine::MatDiffuse,  ListaColores[static_cast<int>(ColorID::negro)]);
    w3dEngine::Material(w3dEngine::MatAmbient,  ListaColores[static_cast<int>(ColorID::negro)]);
    w3dEngine::Material(w3dEngine::MatSpecular, ListaColores[static_cast<int>(ColorID::negro)]);

    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Texture2D);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    if (!SceneCollection->Childrens.empty()) {
        w3dEngine::LineWidth(1);

        if (ShowRelantionshipsLines) RenderRelantionshipsLines();
        RenderIcons3D();

        w3dEngine::Disable(w3dEngine::DepthTest);

        RenderLightLines(); // linea de cada luz al piso (espacio mundo)
        if (showOrigins) RenderOrigins();

        w3dEngine::Disable(w3dEngine::Blend);
        w3dEngine::Disable(w3dEngine::Texture2D);

        if (!SceneCollection->Childrens.empty() &&
            (estado == translacion || estado == rotacion || estado == EditScale))
            RenderAllAxisTransform();
    }

    if (show3DCursor) Render3Dcursor();

    // (la barra de botones 2D NO se dibuja aca: es chrome del area, no un
    //  overlay. Va en RenderUI() junto con los bordes para que se vea aunque
    //  "Show Overlays" este off.)

#ifdef W3D_SYMBIAN
    // baseline que asumen el outliner/properties/cursor en el N95:
    // texcoords habilitados, sin luz, sin scissor, depth para el proximo
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::PointSprite);
    w3dEngine::DepthMask(true);
#endif
}

void Viewport3D::RenderRelantionshipsLines() {
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::DepthMask(false);
    w3dEngine::TexCoordPointer2f(0, lineUV);
    w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1],
              ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    w3dEngine::BindTexture(Textures[3]->iID);
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
    w3dEngine::TexWrap(true);
    w3dEngine::TexWrap(true);

    RenderLinkLines(SceneCollection);

    w3dEngine::DepthMask(true);
}

void Viewport3D::Render3Dcursor() {
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::PushMatrix();
    // SIN swap Y/Z: cursor3D.pos esta en convencion de OBJETO (la usan los snaps,
    // el add-at-cursor y el pivot PivotCursor3D). Antes swapeaba (x,z,y) y el cursor
    // se veia en otro lado que su pivote/los snaps.
    w3dEngine::Translatef(cursor3D.pos.x, cursor3D.pos.y, cursor3D.pos.z);

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::PointSprite);
    w3dEngine::PointSize(16 * GlobalScale); // cursor 3D: 16 relativo a la UI
    w3dEngine::VertexPointer3s(0, pointVertex);
    w3dEngine::BindTexture(Textures[2]->iID);
#ifndef W3D_SYMBIAN
    // pixel perfect: sin filtrado
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    w3dEngine::PointSpriteCoordReplace(true);
    w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
              ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
    w3dEngine::DrawPoints(1);
    w3dEngine::PointSpriteCoordReplace(false);

    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::PointSprite);
    w3dEngine::Disable(w3dEngine::Blend);

    w3dEngine::VertexPointer3f(0, Cursor3DVertices);
    w3dEngine::LineWidth(2);
    w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1],
              ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    W3dDrawLinesF(Cursor3DVertices, Cursor3DEdges, Cursor3DEdgesSize);

    w3dEngine::PopMatrix();
}

void Viewport3D::RenderUI() {
    // barra de botones 2D del area: NO es un overlay, es chrome del area (como
    // los bordes). Se dibuja siempre que ShowUi este on, aunque "Show Overlays"
    // este off. (Usa el viewport ya seteado en Render(): x, glY, width, height.)
    {
        w3dEngine::MatrixMode(w3dEngine::Projection);
        w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, width, height, 0, -1, 1);
        w3dEngine::MatrixMode(w3dEngine::ModelView);
        w3dEngine::LoadIdentity();
        // estado 2D COMPLETO (no asumir lo que dejo el render de la escena:
        // antes la barra iba al final de RenderOverlay que ya lo seteaba; con
        // overlays off corre justo despues de los meshes 3D que dejan CULL_FACE
        // y NORMAL/COLOR arrays activos -> los botones se veian medio culeados
        // (un solo triangulo) y el texto desaparecia).
        w3dEngine::Disable(w3dEngine::DepthTest);
        w3dEngine::Disable(w3dEngine::Lighting);
        w3dEngine::Disable(w3dEngine::Fog);
        w3dEngine::Disable(w3dEngine::PointSprite);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Enable(w3dEngine::ColorMaterial);
        w3dEngine::Enable(w3dEngine::Texture2D);
        w3dEngine::Enable(w3dEngine::Blend);
        w3dEngine::BlendAlpha();
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        w3dEngine::DisableArray(w3dEngine::NormalArray);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::BindTexture(Textures[0]->iID);
        w3dEngine::TexFilter(false);
        w3dEngine::TexFilter(false);
        // durante un transform (mover/rotar/escalar, o ubicar un duplicado) la
        // barra de botones se reemplaza por una barra de estado con los valores
        bool transformando = (Viewport3DActive == this &&
            (estado == translacion || estado == rotacion || estado == EditScale) &&
            (InteractionMode == ObjectMode ||
             (InteractionMode == EditMode && EditXformActivo())));
        if (transformando) {
            RenderBarraTransform();
        } else {
            // el boton [1] (modo) solo si el objeto ACTIVO es una malla; muestra el
            // modo actual con su icono (object en Object Mode, mesh en el resto).
            bool esMesh = ObjActivo && ObjActivo->getType() == ObjectType::mesh;
            // se buscan por ROL (no por indice) -> reordenar la barra no rompe esto.
            Button* bMode = BarRolBtn(BarButtons, BR_Mode); // solo si el activo es malla
            if (bMode) {
                bMode->visible = esMesh;
                if (!esMesh) InteractionMode = ObjectMode;
                bMode->text = (InteractionMode == EditMode)     ? "Edit Mode" :
                              (InteractionMode == VertexPaint)  ? "Vertex Paint" :
                              (InteractionMode == WeightPaint)  ? "Weight Paint" :
                              (InteractionMode == TexturePaint) ? "Texture Paint" : "Object Mode";
                bMode->icon = (InteractionMode == ObjectMode) ? (int)IconType::object : (int)IconType::mesh;
            }
            bool enEdit = (esMesh && InteractionMode == EditMode);
            Button* bSelM = BarRolBtn(BarButtons, BR_SelMode); // sub-elemento, SOLO en edit (icono)
            if (bSelM) {
                bSelM->visible = enEdit;
                bSelM->icon = (EditSelectMode == SelEdge) ? (int)IconType::selEdge :
                              (EditSelectMode == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
            }
            Button* bUV = BarRolBtn(BarButtons, BR_UV);       // UV: SOLO en edit
            if (bUV) bUV->visible = enEdit;
            Button* bAdd = BarRolBtn(BarButtons, BR_Add);     // Add: SOLO en Object Mode
            if (bAdd) bAdd->visible = (InteractionMode == ObjectMode);
            Button* bPiv = BarRolBtn(BarButtons, BR_Pivot);   // Pivot: solo icono = el modo actual
            if (bPiv) bPiv->icon = (g_transformPivot == PivotCursor3D)   ? (int)IconType::pivotCursor :
                                   (g_transformPivot == PivotIndividual) ? (int)IconType::pivotIndividual :
                                   (g_transformPivot == PivotActive)     ? (int)IconType::pivotActive :
                                                                           (int)IconType::pivotMedian;
            Button* bObj = BarRolBtn(BarButtons, BR_Object);  // "Object" / contexto Vertex/Edge/Face
            if (bObj) {
                bObj->visible = HayObjetosSeleccionados();
                bObj->text = !enEdit ? "Object" :
                    (EditSelectMode == SelEdge) ? "Edge" :
                    (EditSelectMode == SelFace) ? "Face" : "Vertex";
            }
            Button* bOri = BarRolBtn(BarButtons, BR_Orient);  // muestra la orientacion actual
            if (bOri) bOri->text = (transformOrientation == LocalOrient)  ? "Local" :
                                   (transformOrientation == ViewOrient)   ? "View"  :
                                   (transformOrientation == NormalOrient) ? "Normal" : "Global";
            RenderBar();
        }
        // estadisticas/fps (texto blanco arriba a la derecha; misma ortho 2D)
        RenderEstadisticas();
    }

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    w3dEngine::Viewport(x, W3dPantallaAlto - y - height, width, height);
    w3dEngine::Ortho(0, width, height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::BlendAlpha();

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BindTexture(Textures[0]->iID);
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    DibujarBordes(this);
}

// float -> texto portable (sin %f, que no anda en Symbian): parte entera con %d
// y los decimales rellenados a mano. Mismo truco que RenderBitmapFloat.
static std::string W3dFmtF(float v, int dec){
    char buf[48];
    int neg = (v < 0.0f) ? 1 : 0;
    float av = neg ? -v : v;
    int ent = (int)av;
    long mul = 1; for (int i = 0; i < dec; i++) mul *= 10;
    int frac = (int)((av - (float)ent) * (float)mul + 0.5f);
    if (frac >= (int)mul) { ent++; frac -= (int)mul; }
    sprintf(buf, "%s%d", neg ? "-" : "", ent);
    std::string r = buf;
    if (dec > 0){
        sprintf(buf, "%d", frac);
        std::string fs = buf;
        while ((int)fs.size() < dec) fs = "0" + fs;
        r += "." + fs;
    }
    return r;
}

// busca el estado guardado (pos/rot/scale al iniciar el transform) del objeto
static SaveState* W3dEstadoGuardado(Object* o){
    for (size_t i = 0; i < estadoObjetos.size(); i++)
        if (estadoObjetos[i].obj == o) return &estadoObjetos[i];
    return NULL;
}

// el texto de la barra de estado: operacion + valores en vivo del objeto activo
// (mismo mapeo de ejes que el panel Properties: Location Y=z, Z=y; Scale x/y/z)
static const char* W3dOrientPalabra(){
    if (transformOrientation == LocalOrient) return "local";
    if (transformOrientation == ViewOrient)  return "view";
    return "global";
}

// arma el texto: un solo eje muestra SOLO ese eje ("X: .. along global X"); un
// plano muestra los dos del plano; libre muestra los tres (sin sufijo).
static std::string W3dLineaTransform(const char* titulo, float vx, float vy, float vz){
    int a = axisSelect;
    std::string ori = W3dOrientPalabra();
    if (a == X || a == Y || a == Z){
        const char* L = (a == X) ? "X" : (a == Y) ? "Y" : "Z";
        float v = (a == X) ? vx : (a == Y) ? vy : vz;
        return std::string(titulo) + " " + L + ": " + W3dFmtF(v, 4) + "  along " + ori + " " + L;
    }
    if (a == PlaneX || a == PlaneY || a == PlaneZ){
        std::string s = titulo, ejes;
        if (a != PlaneX){ s += "  X: " + W3dFmtF(vx, 4); ejes += "X"; }
        if (a != PlaneY){ s += "  Y: " + W3dFmtF(vy, 4); if (!ejes.empty()) ejes += "-"; ejes += "Y"; }
        if (a != PlaneZ){ s += "  Z: " + W3dFmtF(vz, 4); if (!ejes.empty()) ejes += "-"; ejes += "Z"; }
        return s + "  along " + ori + " " + ejes;
    }
    // libre (3 ejes)
    return std::string(titulo) + "  X: " + W3dFmtF(vx, 4) +
                         "  Y: " + W3dFmtF(vy, 4) + "  Z: " + W3dFmtF(vz, 4);
}

// sufijo "  along <orient> <ejes>" para un valor numerico, segun axisSelect
static std::string W3dSufijoEjes(){
    int a = axisSelect;
    std::string ori = W3dOrientPalabra();
    if (a==X||a==Y||a==Z) return std::string("  along ") + ori + " " + ((a==X)?"X":(a==Y)?"Y":"Z");
    if (a==PlaneX) return std::string("  along ") + ori + " Y-Z";
    if (a==PlaneY) return std::string("  along ") + ori + " X-Z";
    if (a==PlaneZ) return std::string("  along ") + ori + " X-Y";
    return ""; // libre
}

static std::string W3dTextoTransform(){
    // ENTRADA NUMERICA: muestra la EXPRESION tipeada + su resultado (estilo Blender
    // "Move: [(2*3)+3] = 9  along global X"). Vale para objetos y malla.
    if (NumInputActivo()){
        float v = 0.0f; bool valido = NumInputValor(v);
        const char* op = (estado==rotacion) ? "Rotate" : (estado==EditScale) ? "Scale" : "Move";
        const char* unit = (estado==rotacion) ? "\xC2\xB0" : "";
        std::string expr = (NumInputNegado() ? "-[" : "[") + NumInputBuffer() + "]";
        std::string val = valido ? (W3dFmtF(v, 4) + unit) : "?";
        std::string suf = (estado==rotacion && (axisSelect==ViewAxis||axisSelect==XYZ||axisSelect==OrbitalAxis)) ? "" : W3dSufijoEjes();
        return std::string(op) + ": " + expr + " = " + val + suf;
    }
    // Edit Mode: los valores son los del TRANSFORM DE MALLA (los vertices), no del
    // objeto (que no se mueve). Mismos labels de eje/orientacion. La rotacion usa
    // gAnguloTransform igual que objetos, asi que cae al codigo de abajo.
    const bool edit = (InteractionMode == EditMode && EditXformActivo());
    if (edit){
        if (estado == translacion){
            Vector3 d = EditXformTransDelta(); // mundo (engine xyz) -> user X/Y/Z (swap z/y)
            return W3dLineaTransform("Translate", d.x, d.z, d.y);
        }
        if (estado == EditScale){
            float f = EditXformScaleFactor();
            return W3dLineaTransform("Scale", f, f, f);
        }
        // rotacion: cae al bloque comun (gAnguloTransform + axisSelect/orientacion)
    }
    if (!ObjActivo) return "";
    SaveState* st = W3dEstadoGuardado(ObjActivo);
    if (estado == translacion){
        Vector3 p = ObjActivo->pos;
        Vector3 p0 = st ? st->pos : p;
        // X/Y/Z del usuario = engine x / z / y (swap del modelo)
        return W3dLineaTransform("Translate", p.x - p0.x, p.z - p0.z, p.y - p0.y);
    }
    if (estado == EditScale){
        Vector3 s = ObjActivo->scale;
        Vector3 s0 = st ? st->scale : Vector3(1,1,1);
        float fx = (s0.x != 0.0f) ? s.x / s0.x : s.x;
        float fy = (s0.y != 0.0f) ? s.y / s0.y : s.y;
        float fz = (s0.z != 0.0f) ? s.z / s0.z : s.z;
        return W3dLineaTransform("Scale", fx, fz, fy); // Y=scale.z, Z=scale.y
    }
    if (estado == rotacion){
        std::string r = "Rotate  " + W3dFmtF(gAnguloTransform, 1) + "\xC2\xB0";
        int a = axisSelect;
        if (a == X || a == Y || a == Z){
            const char* L = (a == X) ? "X" : (a == Y) ? "Y" : "Z";
            r += std::string("  along ") + W3dOrientPalabra() + " " + L;
        }
        else if (a == OrbitalAxis) r += "  Orbital";
        return r;
    }
    return "";
}

// linea PUNTEADA 2D (coords de pantalla del viewport, Y abajo) entre dos puntos.
static void DibujarLineaPunteada2D(float ax, float ay, float bx, float by){
    float dx = bx - ax, dy = by - ay;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 2.0f) return;
    float ux = dx / len, uy = dy / len;
    const float dash = 6.0f, gap = 5.0f, paso = dash + gap;
    GLfloat v[4 * 128]; // hasta 128 guiones
    int n = 0;
    for (float t = 0.0f; t < len && n < 128; t += paso){
        float t2 = t + dash; if (t2 > len) t2 = len;
        v[n*4+0] = ax + ux*t;  v[n*4+1] = ay + uy*t;
        v[n*4+2] = ax + ux*t2; v[n*4+3] = ay + uy*t2;
        n++;
    }
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    // verde como el acento de la UI
    w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::accent)][0],
              ListaColores[static_cast<int>(ColorID::accent)][1],
              ListaColores[static_cast<int>(ColorID::accent)][2], 0.9f);
    w3dEngine::LineWidth(1);
    w3dEngine::VertexPointer2f(0, v);
    w3dEngine::DrawLines(n * 2);
    // RESTAURAR textura: si no, el fondo/texto de la barra (que sigue) salen
    // como bloques solidos sin textura ni alpha.
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
}

void Viewport3D::RenderBarraTransform(){
    if (!barCard) return;
    int barH = BarHeight();
    int yBar = barAbajo ? height - barH : 0;

    // linea punteada VERDE del pivot al mouse: en ROTAR y ESCALAR (no translate). La barra es chrome
    // (se ve aunque Show Overlays este off), pero la linea punteada SI es overlay del transform: con
    // overlays off NO se dibuja (igual que los ejes de constraint, que viven en RenderOverlay).
    if (showOverlays && (estado == rotacion || estado == EditScale) && gLineaValida){
        DibujarLineaPunteada2D(gTrackPivX, gTrackPivY, gTrackMouseX, gTrackMouseY);
    }

    // fondo de la barra (mas opaco que la normal para que el texto se lea)
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)yBar, 0);
    w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::gris)][0],
              ListaColores[static_cast<int>(ColorID::gris)][1],
              ListaColores[static_cast<int>(ColorID::gris)][2], 0.85f);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);
    w3dEngine::PopMatrix();

    // texto de estado en color de acento (modo activo), centrado vertical
    std::string txt = W3dTextoTransform();
    w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::accent)][0],
              ListaColores[static_cast<int>(ColorID::accent)][1],
              ListaColores[static_cast<int>(ColorID::accent)][2], 1.0f);
    w3dEngine::PushMatrix();
    int ty = yBar + (barH - LetterHeightGS) / 2;
    w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)ty, 0);
    RenderBitmapText(txt, textAlign::left, width - gapGS * 2);
    w3dEngine::PopMatrix();
}

// suma recursiva de las estadisticas de malla de toda la escena
static void W3dContarMallas(Object* o, int& vAgr, int& vReal, int& fLog, int& fTri){
    if (!o) return;
    if (o->getType() == ObjectType::mesh){
        Mesh* m = (Mesh*)o;
        vAgr  += (m->vertsAgrupados > 0) ? m->vertsAgrupados : m->vertexSize;
        vReal += m->vertexSize;
        fLog  += !m->faces3d.empty() ? (int)m->faces3d.size() : (m->facesSize / 3);
        fTri  += m->facesSize / 3;
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        W3dContarMallas(o->Childrens[i], vAgr, vReal, fLog, fTri);
}

// texto blanco arriba a la derecha: vertices agrupados/reales, caras logicas/
// triangulos y fps. Se llama dentro del pase 2D de RenderUI (ortho + fuente ya
// seteados). Los contadores por malla estan precalculados (no se cuenta por frame).
void Viewport3D::RenderEstadisticas(){
    if (!showOverlays) return;
    if (!OverlayStatistics && !OverlayFps) return;
    w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::blanco)][0],
              ListaColores[static_cast<int>(ColorID::blanco)][1],
              ListaColores[static_cast<int>(ColorID::blanco)][2], 1.0f);
    const int margen = gapGS * 2;
    const int lineH = LetterHeightGS + gapGS;
    int ly = (barAbajo ? 0 : BarHeight()) + gapGS; // debajo de la barra si esta arriba
    char buf[64];
    if (OverlayStatistics){
        int vAgr=0, vReal=0, fLog=0, fTri=0;
        W3dContarMallas(SceneCollection, vAgr, vReal, fLog, fTri);
        sprintf(buf, "vertex: %d/%d", vAgr, vReal);
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(width - margen), (GLfloat)ly, 0);
        RenderBitmapText(buf, textAlign::right, width); w3dEngine::PopMatrix(); ly += lineH;
        sprintf(buf, "faces: %d/%d", fLog, fTri);
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(width - margen), (GLfloat)ly, 0);
        RenderBitmapText(buf, textAlign::right, width); w3dEngine::PopMatrix(); ly += lineH;
    }
    if (OverlayFps){
        sprintf(buf, "fps: %d", (int)(g_fpsActual + 0.5f));
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(width - margen), (GLfloat)ly, 0);
        RenderBitmapText(buf, textAlign::right, width); w3dEngine::PopMatrix(); ly += lineH;
    }
}

bool Viewport3D::RecalcViewPos() {
    if (!CameraActive) return false;
    /*Object& camera = *CameraActive;
    rot = camera.rot.Inverted();
    posX = -camera.posX;
    posY = -camera.posY;
    posZ = -camera.posZ;*/
    return true;
}

void Viewport3D::ChangePerspective(){
    orthographic = !orthographic;
}

//coloca el cursor 3d desde la vista 3d
void Viewport3D::SetCursor3D(){// 1) Calcular base de la cámara (forward/right/up)
    /*float pitch = rotX * DEG2RAD;
    float yaw = rotY * DEG2RAD;

    Vec3 forward(cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));
    forward = Normalize(forward);

    Vec3 worldUp(0, 1, 0);
    Vec3 right = Cross(forward, worldUp);
    float rlen = Len(right);
    if (rlen < 1e-8f) {
        right = Vec3(1, 0, 0); // Evitar degeneración en pitch ±90°
    } else {
        right = right * (1.0f / rlen);
    }

    Vec3 up = Cross(right, forward); // Unitario por construcción

    // 2) Posición de la cámara
    Vec3 pivotPos(PivotX + posX, PivotY + posY, PivotZ + posZ);
    Vec3 camPos = pivotPos - forward * zoom;

    // 3) Mouse a NDC
    float ndcX = (2.0f * (float)lastMouseX / (float)winW) - 1.0f;
    float ndcY = 1.0f - (2.0f * (float)lastMouseY / (float)winH);

    // 4) Calcular dirección del rayo en el espacio de la cámara
    float halfFovRad = fovDeg * DEG2RAD * 0.5f;
    float halfH = tanf(halfFovRad);
    float halfW = aspect * halfH;

    // Rayo en el espacio de la cámara (sin normalizar)
    Vec3 rayDir = forward + right * (ndcX * halfW) + up * (ndcY * halfH);

    // 5) Intersección con un plano perpendicular al forward, pasando por el pivot
    // Plano: punto = pivotPos, normal = forward
    // Rayo: origen = camPos, dirección = rayDir
    // Ecuación: dot((camPos + t * rayDir - pivotPos), forward) = 0
    float denom = Dot(rayDir, forward);
    if (fabs(denom) < 1e-8f) {
        // Rayo paralelo al plano, usar posición por defecto
        Cursor3DposX = pivotPos.x;
        Cursor3DposY = pivotPos.y;
        Cursor3DposZ = pivotPos.z;
        return;
    }

    float t = Dot(pivotPos - camPos, forward) / denom;
    if (t < 0) {
        // Intersección detrás de la cámara, usar posición por defecto
        Cursor3DposX = pivotPos.x;
        Cursor3DposY = pivotPos.y;
        Cursor3DposZ = pivotPos.z;
        return;
    }

    Vec3 cursorPos = camPos + rayDir * t;

    Cursor3DposX = cursorPos.x;
    Cursor3DposY = cursorPos.y;
    Cursor3DposZ = cursorPos.z;*/
}

void Viewport3D::Aceptar() {
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    // Mostrar el cursor
    #if SDL_MAJOR_VERSION == 2
        SDL_ShowCursor(SDL_ENABLE);
    #elif SDL_MAJOR_VERSION == 3
        SDL_ShowCursor();
    #endif
    //si no hay objetos
    if (SceneCollection->Childrens.empty()){return;}

    if ( InteractionMode == ObjectMode ){
        if (estado != editNavegacion){
            UndoTransformConfirmar(); // Ctrl+Z: el transform se ACEPTO -> pushea el undo pendiente
            estado = editNavegacion;
        }
    } else if (InteractionMode == EditMode && EditXformActivo()){
        EditXformConfirmar(); // fija el transform de malla (recalcula bordes+normales)
    }
    NumInputReset(); // termina el transform -> limpia el valor numerico tipeado
    ViewPortClickDown = false;
}
#endif

void Viewport3D::button_left(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    if (estado == translacion || estado == EditScale || estado == rotacion){
        Aceptar();
    }
    else {
        GuardarMousePos();
        SetCursor3D();
    }
}
#endif

#ifndef W3D_SYMBIAN
void Viewport3D::mouse_button_up(SDL_Event &e){
    ViewPortClickDown = false;
}
#endif

void Viewport3D::event_mouse_motion(int mx, int my){
    // el viewport 3D bajo el mouse pasa a ser el ACTIVO (multi-viewport)
    Viewport3DActive = this;

#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    // si no estamos rotando, se re-captura el angulo del trackball al volver.
    // la linea punteada solo vale en rotar/escalar (no translate ni navegacion).
    if (estado != rotacion) gTrackballCap = false;
    if (estado != rotacion && estado != EditScale) gLineaValida = false;

    //boton del medio del mouse
    #ifdef __ANDROID__
        if (middleMouseDown || leftMouseDown) {
    #else
        if (middleMouseDown) {
    #endif
        ViewPortClickDown = true;
        // Chequear si Shift está presionado
        #if SDL_MAJOR_VERSION == 2
            const Uint8* state = SDL_GetKeyboardState(NULL);
        #elif SDL_MAJOR_VERSION == 3
            const bool* state = SDL_GetKeyboardState(NULL);
        #endif
        bool shiftHeld = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];

        if (shiftHeld) {
            /*float radY = rotY * M_PI / 180.0f; // Yaw
            float radX = rotX * M_PI / 180.0f; // Pitch

            float factor = 0.01f;

            float cosX = cos(radX);
            float sinX = sin(radX);
            float cosY = cos(radY);
            float sinY = sin(radY);

            PivotZ -= dy * factor * cosY;
            PivotX += dx * factor * cosX - dy * factor * sinY * sinX;
            PivotY += dx * factor * sinX + dy * factor * sinY * cosX;*/
            Pan();
            LShiftPressed = false;

        }
        else {
            RotateOrbit();
        }
    }
    else if (estado == translacion || estado == rotacion || estado == EditScale){
        // PRIMER motion de un transform nuevo: el delta dx/dy todavia es el del frame
        // anterior (CheckWarpMouseInViewport lo recalcula DESPUES de esto) -> lo ignoro
        // para que el transform arranque en CERO y no pegue un salto.
        // El flag se consume SOLO con un delta REAL (dx/dy != 0). Una motion ESPURIA de delta 0
        // (pasa a veces tras el click del menu de extrude) NO debe limpiarlo, sino el primer move
        // de verdad (que arrastra el delta viejo del menu al viewport) salta. (Bug intermitente Dante.)
        if (g_xformPrimerMov) {
            if (dx != 0 || dy != 0) g_xformPrimerMov = false;
            dx = 0; dy = 0; // este motion arranca SIEMPRE en cero
        }
        // si el usuario esta tipeando un valor EXACTO, el mouse NO pisa el transform
        if (NumInputActivo()) { ActualizarLineaTransform(mx, my); return; }
        // Ocultar el cursor
        //SDL_HideCursor();
        // Edit Mode: el transform actua sobre los VERTICES seleccionados (no el
        // objeto). Mismo eje/orientacion/pivot, otro target (modulo en LayoutInput).
        const bool edit = (InteractionMode == EditMode && EditXformActivo());
        switch (estado) {
            case translacion:
                if (edit) EditXformTraslacion(dx, dy, 0.01f);
                else      SetTranslacionObjetos(dx, dy, 0.01f);
                break;
            case rotacion:
                if (axisSelect == ViewAxis) {
                    RotarDesdeVista(mx, my); // trackball (ramifica a edit adentro)
                } else if (axisSelect == OrbitalAxis) {
                    gTrackballCap = false;   // orbital: izq/der=camUp, arr/ab=camRight
                    if (edit) EditXformRotOrbital(dx, dy);
                    else      RotarOrbital(dx, dy);
                } else {
                    gTrackballCap = false;   // constreñido a un eje: incremental
                    if (edit) EditXformRotEje(dx, dy);
                    else      SetRotacion(dx, dy);
                }
                break;
            case EditScale:
                if (edit) EditXformScale(dx, dy, 0.001f);
                else      SetScale(dx, dy, 0.001f);
                break;
            default:
                // por si no coincide con nada
                break;
        }
        // linea punteada VERDE del pivot al mouse (rotar/escalar, no translate)
        ActualizarLineaTransform(mx, my);
    }
}
#endif

void Viewport3D::TeclaDerecha(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    //mueve el mouse
    /*if (mouseVisible){
        mouseX++;
        if (mouseX > iScreenWidth-11){mouseX = iScreenWidth-11;};
    }*/

    //rotX -= fixedMul( 1, aDeltaTimeSecs );
    if (estado == editNavegacion){
        if (navegacionMode == Orbit){
            if (CameraActive && ViewFromCameraActive && CameraToView){
                Object& obj = *CameraActive;
                // Convertir el angulo de rotX a radianes
                /*GLfloat radRotX = obj.rotX * M_PI / 180.0;

                obj.posX-= 30 * cos(radRotX);
                obj.posY+= 30 * sin(radRotX);*/
            }
            else {
                if (ViewFromCameraActive){
                    SetViewFromCameraActive(false);
                }
                //rotX+= 0.5;
            }
        }
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            // Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
            GLfloat leftX = cos(radRotX);
            GLfloat leftY = sin(radRotX);*/

            // Mover hacia la izquierda
            //PivotX -= 30 * leftX;
            //PivotY -= 30 * leftY;
        }
    }
    else if (estado == translacion){
        SetTranslacionObjetos(5, 0, 0.01f);
    }
    else if (estado == rotacion){
        if (axisSelect == OrbitalAxis) RotarOrbital(20, 0); // orbital: yaw (camUp)
        else SetRotacion(-20, 0); // ~2 grados por toque
    }
    else if (estado == EditScale){
        SetScale(2,0);
    }
    else if (estado == timelineMove){
        CurrentFrame++;
        if (!PlayAnimation){
            ReloadAnimation();
        }
    }
}
#endif

void Viewport3D::TeclaIzquierda(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    //mueve el mouse
    if (mouseVisible){
        mouseX--;
        if (mouseX < 0){mouseX = 0;};
    }

    //rotX += fixedMul( 0.1, aDeltaTimeSecs );
    if (estado == editNavegacion){
        if (navegacionMode == Orbit){
            if (CameraActive && ViewFromCameraActive && CameraToView){
                Object& obj = *CameraActive;
                // Convertir el angulo de rotX a radianes
                /*GLfloat radRotX = obj.rotX * M_PI / 180.0;

                obj.posX+= 30 * cos(radRotX);
                obj.posY-= 30 * sin(radRotX);*/
            }
            else {
                if (ViewFromCameraActive){
                    SetViewFromCameraActive(false);
                }
                //rotX-= 0.5;
            }
        }
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            // Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
            GLfloat leftX = cos(radRotX);
            GLfloat leftY = sin(radRotX);*/

            // Mover hacia la izquierda
            //PivotX += 30 * leftX;
            //PivotY += 30 * leftY;
        }
    }
    else if (estado == translacion){
        SetTranslacionObjetos(-5, 0, 0.01f);
    }
    else if (estado == rotacion){
        if (axisSelect == OrbitalAxis) RotarOrbital(-20, 0); // orbital: yaw (camUp)
        else SetRotacion(20, 0); // ~2 grados por toque
    }
    else if (estado == EditScale){
        SetScale(-2,0);
    }
    else if (estado == timelineMove){
        CurrentFrame--;
        if (CurrentFrame < StartFrame){
            StartFrame = EndFrame;
        }
        if (!PlayAnimation){
            ReloadAnimation();
        }
    }
}
#endif

void Viewport3D::TeclaArriba(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    //mueve el mouse
    if (mouseVisible){
        mouseY--;
        if (mouseY < 0){mouseY = 0;};
    }

    if (estado == editNavegacion){
        if (navegacionMode == Orbit){
            if (CameraActive && ViewFromCameraActive && CameraToView){
                Object& obj = *CameraActive;
                // Convertir el angulo de rotX a radianes
                /*GLfloat radRotX = obj.rotX * M_PI / 180.0;
                GLfloat radRotY = obj.rotY * M_PI / 180.0;
                //GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

                obj.posX+= 30 * sin(radRotX);
                //obj.posY-= 30 * cos(radRotX);
                obj.posZ+= 30 * cos(radRotY);*/
            }
            else {
                if (ViewFromCameraActive){
                    SetViewFromCameraActive(false);
                }
                //rotY-= 0.5;
            }
        }
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            PivotY+= 30 * cos(radRotX);
            PivotX-= 30 * sin(radRotX);*/
        }
    }
    else if (estado == EditScale){
        SetScale(2,0);
    }
    else if (estado == rotacion && axisSelect == OrbitalAxis){
        RotarOrbital(0, -20); // orbital: pitch (camRight) hacia arriba
    }
    else if (estado == translacion){
        SetTranslacionObjetos(0, -5, 0.01f);
    }
}
#endif

void Viewport3D::TeclaAbajo(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    //mueve el mouse
    /*if (mouseVisible){
        mouseY++;
        if (mouseY > iScreenHeight-17){mouseY = iScreenHeight-17;};
    }*/

    if (estado == editNavegacion){
        if (navegacionMode == Orbit){
            if (CameraActive && ViewFromCameraActive && CameraToView){
                Object& obj = *CameraActive;
                // Convertir el angulo de rotX a radianes
                //GLfloat radRotX = obj.rotX * M_PI / 180.0;
                //GLfloat radRotY = obj.rotY * M_PI / 180.0;
                //GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

                /*obj.posX-= 30 * sin(radRotX);
                //obj.posY-= 30 * cos(radRotX);
                obj.posZ-= 30 * cos(radRotY);*/
            }
            else {
                if (ViewFromCameraActive){
                    SetViewFromCameraActive(false);
                }
                //rotY+= 0.5;
            }
        }
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            PivotY-= 30 * cos(radRotX);
            PivotX+= 30 * sin(radRotX);*/
        }
    }
    else if (estado == EditScale){
        SetScale(-2,0);
    }
    else if (estado == rotacion && axisSelect == OrbitalAxis){
        RotarOrbital(0, 20); // orbital: pitch (camRight) hacia abajo
    }
    else if (estado == translacion){
        SetTranslacionObjetos(0, 5, 0.01f);
    }
}
#endif

void Viewport3D::SetEje(int eje){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    if (estado != editNavegacion){
        ReestablecerEstado(false);
        axisSelect = eje;
    }
}
#endif

void Viewport3D::SetViewFromCameraActive(bool value){
    if (!CameraActive) return;

    if (value){
        /*LastPosX = posX;
        LastPosY = posY;
        LastPosZ = posZ;
        LastZoom = zoom;*/
    }
    else {
        /*posX = LastPosX;
        posY = LastPosY;
        posZ = LastPosZ;
        zoom = LastZoom;*/
    }
    ViewFromCameraActive = value;
}

#ifndef W3D_SYMBIAN
void Viewport3D::event_key_down(SDL_Event &e){
    #if SDL_MAJOR_VERSION == 2
        SDL_Keycode key = e.key.keysym.sym; //SDL2
    #elif SDL_MAJOR_VERSION == 3
        SDL_Keycode key = e.key.key; // SDL3
    #endif
    // Los menus que abren EN EL CURSOR (Shift+A, U, X, W, Ctrl+R...) usan lastMouseX/Y, que solo se
    // refrescaban en el CLICK -> el menu salia de la ULTIMA posicion clickeada, no del mouse. Aca
    // (con SDL_GetMouseState = posicion ACTUAL) se ponen donde esta el mouse. No durante un transform.
#ifndef W3D_SYMBIAN
    if (estado == editNavegacion) GuardarMousePos();
#endif
    // las FLECHAS se repiten al mantenerlas (mover/orbitar continuo); el resto
    // de las teclas ignora el auto-repeat.
    bool esFlecha = (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT || key == SDLK_RIGHT);
    if (esFlecha && e.key.repeat != 0) {
        if (key == SDLK_RIGHT)     TeclaDerecha();
        else if (key == SDLK_LEFT) TeclaIzquierda();
        else if (key == SDLK_UP)   TeclaArriba();
        else                       TeclaAbajo();
        return;
    }
    if (e.key.repeat == 0) {
        switch (key) {
            case SDLK_LSHIFT:
                ShiftCount = 0;
                LShiftPressed = true;
                break;
            case SDLK_LALT:
                LAltPressed = true;
                break;
            case SDLK_RETURN:  // Enter
                key_down_return();
                break;
            case SDLK_RIGHT:   // Flecha derecha
                TeclaDerecha();
                break;
            case SDLK_LEFT:    // Flecha izquierda
                TeclaIzquierda();
                break;
            case SDLK_UP:
                TeclaArriba();
                break;
            case SDLK_DOWN:
                TeclaAbajo();
                break;
            case SDLK_A:
                // simple: las funciones ya saben si es object o edit mode
                if (LCtrlPressed) {                     // Ctrl+A: menu Apply (Location/Rotation/Scale/All) en object
                    if (InteractionMode == ObjectMode) LayoutApplyMenu(lastMouseX, lastMouseY);
                }
                else if (LShiftPressed) {               // Shift+A: menu Add en el cursor (object)
                    if (InteractionMode == ObjectMode) LayoutMenuAdd(lastMouseX, lastMouseY);
                }
                else if (LAltPressed) DeseleccionarTodo();   // Alt+A: deseleccionar todo
                else SeleccionarTodoForzado();          // A: seleccionar todo
                break;
            case SDLK_I:
                if (LCtrlPressed) InvertirSeleccion();  // Ctrl+I: invertir seleccion
                break;
            case SDLK_D: {
                if (LAltPressed){
                    NewInstance();
                }
                else if (LShiftPressed){
                    // Edit Mode: duplica la seleccion de malla; si no, los objetos
                    if (InteractionMode == EditMode && g_editMesh) LayoutDuplicarEdit();
                    else DuplicatedObject();
                }
                break;
            }
            case SDLK_F:
                // F: "New Edge/Face from Vertices" (solo Edit Mode con malla)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutNewFaceEdit();
                break;
            case SDLK_N:
                // Shift+N: Recalculate Normals (Edit Mode), igual que el menu Face
                if (LShiftPressed && estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutRecalcNormales();
                break;
            case SDLK_L:
                // L: Select Linked (la isla conectada bajo el mouse)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutSelectLinked(lastMouseX, lastMouseY);
                break;
            case SDLK_U:
                // Edit Mode: U abre el menu UV (Mark Seam + proyecciones).
                // Object Mode: U NO hace nada (la "limpieza de pantalla" / Show Overlays se maneja
                // SOLO desde el checkbox del menu Overlays, no por tecla).
                if (InteractionMode == EditMode && g_editMesh && estado == editNavegacion)
                    LayoutMenuUV(lastMouseX, lastMouseY);
                break;
            case SDLK_J:
                if (LCtrlPressed && estado == editNavegacion && InteractionMode == ObjectMode)
                    JoinObjetos();        // Ctrl+J: une las mallas seleccionadas en el objeto activo
                else
                    ChangeViewType();
                break;
            case SDLK_H:
                ChangeVisibilityObj();
                break;
            case SDLK_K:
                SetShowOverlays(!showOverlays);
                break;
            case SDLK_X:
                // durante un transform: X = eje X (re-apretar cicla
                // Global->Local->View->libre); Shift+X = plano (mueve en Y,Z).
                if (estado != editNavegacion){
                    if (LShiftPressed) CiclarPlanoTransform(X);
                    else CiclarEjeTransform(X);
                    if (EditXformActivo()) EditXformReiniciar(); // restaura y re-aplica el nuevo eje
                }
                // Edit Mode: menu Delete cerca del cursor; Object Mode: POPUP de confirmar borrado
                else if (!LayoutDeleteEdit(lastMouseX, lastMouseY)) AbrirConfirmarBorrado();
                break;
            case SDLK_BACKSPACE:
            case SDLK_DELETE:
                if (estado == editNavegacion && !LayoutDeleteEdit(lastMouseX, lastMouseY))
                    AbrirConfirmarBorrado();
                break;
            case SDLK_Y:
                if (estado != editNavegacion){
                    if (LShiftPressed) CiclarPlanoTransform(Y);
                    else CiclarEjeTransform(Y);
                    if (EditXformActivo()) EditXformReiniciar();
                }
                break;
            case SDLK_Z:
                if (estado != editNavegacion){
                    if (LShiftPressed) CiclarPlanoTransform(Z);
                    else CiclarEjeTransform(Z);
                    if (EditXformActivo()) EditXformReiniciar();
                }
                break;
            case SDLK_R:
                // Ctrl+R: Loop Cut and Slide (Edit Mode con malla)
                if (LCtrlPressed && estado == editNavegacion && InteractionMode == EditMode && g_editMesh){
                    LoopCutIniciar(lastMouseX, lastMouseY);
                    break;
                }
                // R arranca la rotacion (trackball); R de nuevo alterna
                // trackball <-> orbital/gimbal (sin tener que ciclar X/Y/Z).
                // En Edit Mode el transform actua sobre los vertices seleccionados.
                if (estado == rotacion) ToggleRotacionOrbital();
                else { // EditXformStart en Edit Mode CAPTURA el undo (Ctrl+Z); en Object Mode -> SetRotacion
                    valorRotacion = 0;
                    if (!EditXformStart(rotacion, ViewAxis)) SetRotacion();
                }
                break;
            case SDLK_G:
                // EditXformStart (no EditXformIniciar directo) -> en Edit Mode CAPTURA el undo del move (Ctrl+Z)
                if (!EditXformStart(translacion, ViewAxis)) SetPosicion();
                break;
            case SDLK_S:
                if (!EditXformStart(EditScale, XYZ)) SetEscala();
                break;
            case SDLK_E:
                // Edit Mode: extrude de las caras seleccionadas (arranca el move por
                // la normal). En Object Mode no hace nada.
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutExtrudeFaces();
                break;
            case SDLK_V:
                // Edit Mode: RIP -> separa la malla a lo largo de la seleccion (loop de bordes / verts / caras)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutRipEdit();
                break;
            case SDLK_W:
                // Edit Mode: menu Mark/Clear Sharp en el cursor (bordes filosos)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutMenuSharp(lastMouseX, lastMouseY);
                break;
            // Numpad (Ctrl = la vista OPUESTA, como Blender)
            case SDLK_KP_1: SetViewpoint(LCtrlPressed ? Viewpoint::back : Viewpoint::front); break;
            //case SDLK_KP_2: numpad('2'); break;
            case SDLK_KP_3: SetViewpoint(LCtrlPressed ? Viewpoint::left : Viewpoint::right); break;
            case SDLK_KP_4: {
                RollOrbit(-15);
                break;
            }
            case SDLK_KP_5: {
                ChangePerspective();
                break;
            };
            case SDLK_KP_6: {
                RollOrbit(15);
                break;
            }
            case SDLK_KP_7: SetViewpoint(LCtrlPressed ? Viewpoint::bottom : Viewpoint::top); break;
            case SDLK_KP_8: BuscarVertexAnimation(); break;
            case SDLK_KP_9: abrir(); break;
            case SDLK_KP_0:
                if (LCtrlPressed) SetActiveObjectAsCamera();          // Ctrl+Num 0: activo -> camara activa (SIN cambiar la vista)
                else SetViewFromCameraActive(!ViewFromCameraActive);  // Num 0: ver desde la camara (toggle)
                break;
            case SDLK_KP_PERIOD: {
                EnfocarObject();
                break;
            }
            // si querés, agregá más teclas aquí
            case SDLK_ESCAPE:  // Esc
                // loop cut en curso: lo descarta (restaura la geometria pre-corte)
                if (LoopCutActivo()) LoopCutCancelar();
                // Edit Mode con transform de malla en curso: descarta (restaura el
                // snapshot de vertices); si no, cancela el transform de objetos.
                else if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
                else Cancelar();
                NumInputReset();
                break;
        }
    }
    else {
        // Evento repetido por mantener apretada
        switch (key) {
            case SDLK_RETURN:  // Enter
                key_down_return();
                break;
            case SDLK_RIGHT:   // Flecha derecha
                TeclaDerecha();
                break;
            case SDLK_LEFT:    // Flecha izquierda
                TeclaIzquierda();
                break;
            case SDLK_UP:
                TeclaArriba();
                break;
            case SDLK_DOWN:
                TeclaAbajo();
                break;
            case SDLK_A:
                if (LAltPressed) DeseleccionarTodo();
                else SeleccionarTodoForzado();
                break;
            // Numpad (Ctrl = la vista OPUESTA, como Blender)
            case SDLK_KP_1: {
                SetViewpoint(LCtrlPressed ? Viewpoint::back : Viewpoint::front);
                break;
            }
            //case SDLK_KP_2: numpad('2'); break;
            case SDLK_KP_3: {
                SetViewpoint(LCtrlPressed ? Viewpoint::left : Viewpoint::right);
                break;
            }
            case SDLK_KP_7: {
                SetViewpoint(LCtrlPressed ? Viewpoint::bottom : Viewpoint::top);
                break;
            }
            case SDLK_KP_8: BuscarVertexAnimation(); break;
            case SDLK_KP_9: abrir(); break;
            //case SDLK_KP_0: numpad('0'); break;
            case SDLK_KP_PERIOD: {
                EnfocarObject();
                break;
            }
            // si querés, agregá más teclas aquí
            case SDLK_ESCAPE:  // Esc
                // Edit Mode con transform de malla en curso: descarta (restaura el
                // snapshot de vertices); si no, cancela el transform de objetos.
                if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
                else Cancelar();
                NumInputReset();
                break;
        }
    }
}
#endif

#ifndef W3D_SYMBIAN
void Viewport3D::event_key_up(SDL_Event &e){
    #if SDL_MAJOR_VERSION == 2
        SDL_Keycode key = e.key.keysym.sym; //SDL2
    #elif SDL_MAJOR_VERSION == 3
        SDL_Keycode key = e.key.key; // SDL3
    #endif
    switch (key) {
        case SDLK_LSHIFT:
            if (ShiftCount < 20){
                changeSelect(SelectMode::NextSingle);
            }
            LShiftPressed = false;
            break;
        case SDLK_LALT:
            LAltPressed = false;
            break;
    }
}
#endif

void Viewport3D::key_down_return(){
    Aceptar();
}

Viewport3D* Viewport3DActive = NULL;

//precalculos
bool recalcularCamara = true;
GLfloat radY = 0.0f;
GLfloat radX = 0.0f;

//GLfloat factor = 0.03f;

GLfloat cosX = 0.0f;
GLfloat sinX = 0.0f;
GLfloat cosY = 0.0f;
GLfloat sinY = 0.0f;

#ifndef W3D_SYMBIAN // el gamepad usa SDL: PC/Android
void Gamepad::Update() {
    if (!target || !Viewport3DActive || !CameraActive || !targetAnim) return;

    // --- 1. CONFIGURACIÓN DE VECTORES DE MOVIMIENTO ---
    Vector3 camForward = CameraActive->forwardVector;
    Vector3 camRight   = CameraActive->rightVector;

    // Proyectar los vectores al plano XZ (el piso) y normalizar.
    Vector3 Fwd_XZ = Vector3(camForward.x, 0.0f, camForward.z).Normalized();
    Vector3 Rgt_XZ = Vector3(camRight.x, 0.0f, camRight.z).Normalized();

    float inputY = axisState[SDL_CONTROLLER_AXIS_LEFTY]; // Adelante/Atrás
    float inputX = axisState[SDL_CONTROLLER_AXIS_LEFTX]; // Lateral/Giro

    bool botonSalto = SDL_GameControllerGetButton(
        controller,
        SDL_CONTROLLER_BUTTON_A
    );

    // Inversión de ejes para el control intuitivo
    float correctedInputY = -inputY;
    float correctedInputX = -inputX;

    // Umbral para ignorar el ruido del stick
    const float DeadZone = 0.1f;

    bool grounded = (target->pos.y <= piso + 0.001f);

    bool justLanded = (!wasGrounded && grounded);
    wasGrounded = grounded;

    // =========================
    // 1. INPUT → HORIZONTAL VELOCITY (INERCIA)
    // =========================

    Vector3 desiredMove(0,0,0);

    if (std::abs(inputY) > DeadZone)
        desiredMove += Fwd_XZ * (-inputY);

    if (std::abs(inputX) > DeadZone)
        desiredMove += Rgt_XZ * (-inputX);

    float accel = velocidad;

    // inercia suave
    velocity.x += desiredMove.x * accel;
    velocity.z += desiredMove.z * accel;

    // freno rápido (clave)
    velocity.x *= 0.85f;
    velocity.z *= 0.85f;

    // =========================
    // 2. GRAVEDAD
    // =========================

    velocity.y -= gravedad;

    // clamp caída
    if (velocity.y < -velocidadMaximaCaida)
        velocity.y = -velocidadMaximaCaida;

    // =========================
    // 3. SALTO
    // =========================

    if (grounded && botonSalto) {
        velocity.y = potenciaSalto;
    }

    // =========================
    // 4. APLICAR MOVIMIENTO
    // =========================

    target->pos += velocity;

    // =========================
    // 5. PISO
    // =========================

    if (target->pos.y <= piso) {
        target->pos.y = piso;
        velocity.y = 0;
        grounded = true;
    }

    // =========================
    // 6. LIMITES XZ
    // =========================

    if (target->pos.x < limiteIzquierdo) target->pos.x = limiteIzquierdo;
    if (target->pos.x > limiteDerecho)   target->pos.x = limiteDerecho;
    if (target->pos.z < limiteFrente)    target->pos.z = limiteFrente;
    if (target->pos.z > limiteFondo)     target->pos.z = limiteFondo;

    // =========================
    // 7. ROTACIÓN SOLO SI ESTÁ EN TIERRA
    // =========================

    Vector3 horizontalVel = velocity;
    horizontalVel.y = 0;

    bool moving = horizontalVel.LengthSq() > 0.001f;

    if (moving) {
        Vector3 dir = horizontalVel.Normalized();

        Quaternion rot = Quaternion::FromDirection(dir, Vector3(0,1,0));
        target->rot = Quaternion::Slerp(target->rot, rot, 0.2f);

        if (grounded){
            targetAnim->nextAnim = 1; // correr
        }
        else if (targetAnim->nextAnim != 2){
            targetAnim->nextAnim = 3; // gira en el aire
        }
    }
    else if (justLanded) {
        targetAnim->nextAnim = 4;
    }
    else if (!grounded && targetAnim->nextAnim != 3) {
        targetAnim->nextAnim = 2; // caer
    }
    else if (grounded && targetAnim->nextAnim != 4){
        targetAnim->nextAnim = 0; // idle
    }
}
#endif // !W3D_SYMBIAN
