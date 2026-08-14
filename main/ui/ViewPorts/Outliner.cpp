#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#ifdef _WIN32
    #include <windows.h>
#endif

#include "Outliner.h"
#include "WhiskUI/draw/glesdraw.h"
#include "render/OpcionesRender.h" // g_redraw (CentrarSeleccion pide redibujar)
#include "objects/ObjectMode.h" // reparent del drag&drop
#include "objects/Instance.h"   // IconoDeObjeto: array/mirror/instance segun el modo
#include "objects/Mesh.h"       // Mesh::armatures2d (una fila VIRTUAL azul por rig 2D bajo la malla)
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (confirmar antes de borrar)
#include "ViewPorts/Notificaciones.h"       // Notificar (renames del modo mover)
#include "Undo.h"                           // UndoCapturarRenames (1 solo paso al confirmar el mover)
#include <cstdio>                           // sprintf (el contador del aviso)
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    extern int W3dPantallaAlto;  // alto de pantalla (flip de Y; glesdraw.cpp)
    extern int ShiftCount;
    extern bool middleMouseDown;
    extern bool MouseWheel;
#else
    #include <GL/gl.h>
#endif

// El icono del objeto se DERIVA de su tipo (Fase D: el core ya no guarda iconos de UI).
// Mapeo tipo de objeto -> IconType (el catalogo de iconos vive en la UI, que el editor SI ve).
size_t IconoDeObjeto(Object* o) {
    switch (o->getType().v) {
        case ObjectType::mesh:       return (size_t)IconType::mesh;
        case ObjectType::light:      return (size_t)IconType::light;
        case ObjectType::camera:     return (size_t)IconType::camera;
        case ObjectType::collection: return (size_t)IconType::archive;
        case ObjectType::empty:      return (size_t)IconType::empty;
        case ObjectType::armature:   return (size_t)IconType::armature;
        case ObjectType::curve:      return (size_t)IconType::curve;
        case ObjectType::mirror:     return (size_t)IconType::mirror;
        case ObjectType::script:     return (size_t)IconType::gamepad;
        case ObjectType::instance: { // array / mirror / instance segun el modo
            Instance* in = (Instance*)o;
            if (in->mirror)    return (size_t)IconType::mirror;
            if (in->count > 1) return (size_t)IconType::array;
            return (size_t)IconType::instance;
        }
        case ObjectType::lod:        return (size_t)IconType::array;     // LOD: niveles de detalle
        case ObjectType::culling:    return (size_t)IconType::visible;   // Culling: que se ve / que se corta
        case ObjectType::particulas: return (size_t)IconType::circle;    // Particulas: emisor (puntitos redondos)
        case ObjectType::ui:         return (size_t)IconType::textura;   // interfaz 2D
        case ObjectType::imagen2d:   return (size_t)IconType::foto;      // elemento imagen 2D
        case ObjectType::rect2d:     return (size_t)IconType::plane;     // elemento rectangulo 2D
        case ObjectType::cont2d:     return (size_t)IconType::carpeta;   // contenedor 2D (ordena hijos)
        case ObjectType::slice9:     return (size_t)IconType::cuadricula; // imagen con bordes fijos
        case ObjectType::boton2d:    return (size_t)IconType::object;     // boton de interfaz
        case ObjectType::expandir2d: return (size_t)IconType::arrowRight; // resorte de layout
        case ObjectType::video2d:    return (size_t)IconType::camera;     // video (sin sonido)
        default:                     return (size_t)IconType::archive;
    }
}

// ===================================================================================================
//  FILAS VIRTUALES "Armature 2D": los huesos 2D NO son objetos de escena, viven DENTRO de la malla
//  (Mesh::armatures2d). El outliner lo AVISA con UNA fila informativa POR ARMATURE 2D (una malla
//  puede tener varios rigs 2D independientes) colgando de la malla y en
//  AZUL (el mismo azul con el que el editor UV dibuja los huesos), para dejar claro que el rig 2D es
//  parte del mesh. Es una fila INERTE: no se selecciona, no se arrastra, no es destino de drop, no
//  se puede desemparentar (no es un objeto) y NO se despliega -- los huesos se listan y se editan en
//  la pestania "Armature 2D" del panel Properties y en el propio editor UV, que es donde se los ve
//  dibujados (tenerlos DOS veces era ruido: decision del dueno).
//  TODOS los recorridos (nombres, ojos, fila<->objeto, alto del contenido) se alinean con la MISMA
//  cuenta Arm2DCant(obj): no hay conteo de filas duplicado que mantener.
// ===================================================================================================
static const float kArm2DAzul[3] = { 0.28f, 0.55f, 1.00f }; // = el azul de los huesos del UV editor

// la malla con armatures 2D de este objeto (NULL = no aplica; tambien es "suma filas virtuales?")
static Mesh* Arm2DDe(Object* o){
    if (!o || o->getType() != ObjectType::mesh) return NULL;
    Mesh* m = (Mesh*)o;
    return m->TieneArm2D() ? m : NULL;
}
// CUANTAS filas virtuales suma este objeto (= cuantos armatures 2D tiene la malla; 0 = ninguna)
static int Arm2DCant(Object* o){ Mesh* m = Arm2DDe(o); return m ? (int)m->armatures2d.size() : 0; }
// 'o' tiene algo que desplegar? (hijos objeto O filas virtuales de armature 2D)
static bool OutTieneHijos(Object* o){ return (o->Childrens.size() >= 1) || (Arm2DCant(o) > 0); }

// resultado de ubicar una fila del arbol: un objeto REAL, o la fila VIRTUAL del armature 2D
struct OutFila {
    Object* obj;    // objeto real (NULL si la fila es virtual)
    Mesh*   arm2d;  // != NULL -> fila virtual "Armature 2D" de esta malla
    int     arm2dIdx; // indice del armature 2D de esa fila (solo si arm2d != NULL)
    int     prof;   // nivel de sangria
    OutFila() : obj(NULL), arm2d(NULL), arm2dIdx(-1), prof(0) {}
};

// Constructor
static Object* W3dObjetoEnFila(int fila, int* profOut = 0); // profOut (opcional) = profundidad del objeto
static bool W3dFilaEnArbol(int fila, OutFila& out);         // fila -> objeto real o fila virtual (false = vacio)
static int W3dFilaDe(Object* objetivo);                     // fila visible de un objeto (-1 si esta plegado)

Outliner::Outliner() : ViewportBase() {
    Renglon = new Rec2D();
    CantidadRenglones = 5;
    lastContentRows = -1;
    hoverFila = -1;
    dragObjeto = NULL;
    dragging = false;
    dragY0 = 0;
    dropFila = -1;
    dropZona = -2;
    dropProf = 0;
    moviendo = false;
    moverObj = NULL;
    moverPadreOrig = NULL;
    moverAnteriorOrig = NULL;
    BarCrear();
}

//para hacer el calculo si o si hay que hacerlo de forma recursiva
void Outliner::CalcularRenglon(Object* obj, int* MaxPosXtemp, int* MaxPosYtemp){
    int rowWidth = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS;
    *MaxPosYtemp -= RenglonHeightGS;
    int textWidth = obj->name.size() * LetterWidthGS;
    rowWidth += textWidth + gapGS;

    // guardar ancho máximo
    if (rowWidth > *MaxPosXtemp) *MaxPosXtemp = rowWidth;

    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (!OutTieneHijos(obj) || !obj->desplegado) return;
    *MaxPosYtemp -= RenglonHeightGS * Arm2DCant(obj); // una fila virtual por armature 2D

    //std::cout << "textWidth: " << textWidth << " rowWidth: " << rowWidth << std::endl;
    for (size_t o = 0; o < obj->Childrens.size(); o++) {
        CalcularRenglon(obj->Childrens[o], MaxPosXtemp, MaxPosYtemp);
        /*int rowWidthObj = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS;
        *MaxPosYtemp -= RenglonHeightGS;

        // texto del objeto
        int textWidthObj = reinterpret_cast<Text*>(obj->Childrens[o]->name->data)->letters.size() * LetterWidthGS;
        rowWidthObj += textWidthObj + gapGS;

        if (rowWidthObj > *MaxPosXtemp) *MaxPosXtemp = rowWidthObj;
        //std::cout << "caracteres obj: " << rowWidthObj << std::endl;*/
    }
}

void Outliner::Resize(int newW, int newH){
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);

    // (el ancho de las franjas se ajusta al final, cuando ya se sabe
    // si existe la barra vertical)

    // Calcular cuántos renglones entran en la altura
    // (ceil portable: std::ceil es ambiguo en RVCT)
    CantidadRenglones = (size_t)((height + RenglonHeightGS - 1) / RenglonHeightGS);

    int MaxPosXtemp = 0;
    int MaxPosYtemp = 0;

    if (!SceneCollection) {
        ResizeScrollbar(newW, newH, 0, 0, BarTopOffset());
        Renglon->SetSize(0, 0, (GLshort)width, RenglonHeightGS);
        return;
    }
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        CalcularRenglon(SceneCollection->Childrens[c], &MaxPosXtemp, &MaxPosYtemp);
    }
    //este es el gap para la barra de desplazamiento de abajo
    MaxPosYtemp -= marginGS;
    //std::cout << "MaxPosXtemp: " << MaxPosXtemp << " width: " << width << std::endl;
    //std::cout << "MaxPosYtemp: "<< MaxPosYtemp << std::endl;
    //std::cout << "Ancho: " << newW << " Alto: "<< newH << std::endl;
    ResizeScrollbar(newW, newH, MaxPosXtemp, MaxPosYtemp, BarTopOffset());

    // las franjas le dejan el "scrollbar area" SOLO si hay barra
    // vertical; sin barra el espacio no se desperdicia
    int reservaV = scrollY ? (borderGS + GlobalScale * 9 + 2) : 0;
    Renglon->SetSize(0, 0, (GLshort)(width - reservaV), RenglonHeightGS);
}

void Outliner::Render(){
    // AUTO-REFRESH del scrollbar: si cambio la cantidad de FILAS VISIBLES (importar/agregar/borrar/desplegar) se
    // recalcula el rango de scroll. Antes solo se recalculaba al REDIMENSIONAR el viewport -> tras importar objetos
    // el scrollbar quedaba viejo y no se podia scrollear hasta cambiar el tamanio de un viewport a mano.
    if (SceneCollection){
        struct C { static int rec(Object* o){ int n = 1;
            if (OutTieneHijos(o) && o->desplegado){
                n += Arm2DCant(o);                        // una fila virtual por armature 2D
                for (size_t i = 0; i < o->Childrens.size(); i++) n += rec(o->Childrens[i]);
            }
            return n; } };
        int filas = 0;
        for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) filas += C::rec(SceneCollection->Childrens[c]);
        if (filas != lastContentRows){ lastContentRows = filas; Resize(width, height); }
    }
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // arbol con origen ARRIBA-izquierda (4 OS): GL quiere abajo-izquierda
    const int glY = W3dPantallaAlto - y - height;
    if (!SceneCollection) return;

    // Limpiar pantalla
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, width, height); // igual a tu viewport
    w3dEngine::ClearColor(ListaColores[static_cast<int>(ColorID::background)][0],
        ListaColores[static_cast<int>(ColorID::background)][1],
        ListaColores[static_cast<int>(ColorID::background)][2],
        ListaColores[static_cast<int>(ColorID::background)][3]);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    w3dEngine::Disable(w3dEngine::ScissorTest);

    w3dEngine::Viewport(x, glY, width, height); // x, y, ancho, alto
    w3dEngine::Ortho(0, width, height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::ColorMaterial);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    //de aca en adelante es como antes
    w3dEngine::PushMatrix();
    size_t RenglonesY = 0;
    w3dEngine::Translatef(0, PosY + borderGS + BarTopOffset(), 0);
    for (size_t i = 0; i < CantidadRenglones; i++) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(0, RenglonesY, 0);
        RenglonesY += RenglonHeightGS;
        // Renglón Seleccionado
        if (dragging && dropZona == 1 && (int)i == dropFila) {
            // vista previa: este seria el futuro PADRE del drop
            w3dEngine::Color4ub(ListaColoresUbyte[static_cast<int>(ColorID::accentDark)][0],
                       ListaColoresUbyte[static_cast<int>(ColorID::accentDark)][1],
                       ListaColoresUbyte[static_cast<int>(ColorID::accentDark)][2], 255);
        }
        else if ((int)i == hoverFila) {
            // hover: feedback antes de hacer click
            w3dEngine::Color4ub(ListaColoresUbyte[static_cast<int>(ColorID::headerColor)][0], ListaColoresUbyte[static_cast<int>(ColorID::headerColor)][1], ListaColoresUbyte[static_cast<int>(ColorID::headerColor)][2], 255);
        }
        else if (i % 2 == 0) {
            w3dEngine::Color4ub(ListaColoresUbyte[static_cast<int>(ColorID::gris)][0], ListaColoresUbyte[static_cast<int>(ColorID::gris)][1], ListaColoresUbyte[static_cast<int>(ColorID::gris)][2], 255);
        }
        else {
            // Renglón impar
            w3dEngine::Color4ub(ListaColoresUbyte[static_cast<int>(ColorID::background)][0], ListaColoresUbyte[static_cast<int>(ColorID::background)][1], ListaColoresUbyte[static_cast<int>(ColorID::background)][2], 255);
        }
        //RenderObject2D(*Renglon);
        Renglon->Render(false);
        w3dEngine::PopMatrix();
    }
    w3dEngine::PopMatrix();

    w3dEngine::BindTexture(Textures[0]->iID);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
#ifndef W3D_SYMBIAN
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    SetColorID(ColorID::grisUI);

    //esto es para recortar y que no se ponga el texto encima de los ojos de la derecha
    w3dEngine::Enable(w3dEngine::ScissorTest);
    if (scrollX){
        w3dEngine::Scissor(x, glY + marginGS, width - 2*IconSizeGS - gapGS - marginGS - borderGS - gapGS - (scrollY ? (GlobalScale*9 + gapGS) : 0), height - marginGS); // - ojos+camaras+barra
    }
    else {
        w3dEngine::Scissor(x, glY, width - 2*IconSizeGS - gapGS - marginGS - borderGS - gapGS - (scrollY ? (GlobalScale*9 + gapGS) : 0), height); // - ojos+camaras+barra
    }

    RenglonesY = 0;
    cullBaseY = PosY + borderGS + BarTopOffset(); filaDFS = 0; // culling: Y de la 1er fila del recorrido de NOMBRES
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(marginGS + PosX, PosY + borderGS + BarTopOffset(), 0);
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++){
        DibujarRenglon(SceneCollection->Childrens[c], !SceneCollection->Childrens[c]->visible);
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
    }
    w3dEngine::PopMatrix();

    SetColorID(ColorID::grisUI);
    RenglonesY = 0;

    w3dEngine::PushMatrix();
    //no usa PosX porque los ojos siempre estan en la misma posicion en X. al borde
    int reservaBarra = scrollY ? (GlobalScale * 9 + gapGS) : 0;   // solo el ancho de la barra + un gap chico
    w3dEngine::Translatef(width - 2*IconSizeGS - gapGS - marginGS - borderGS - reservaBarra, GlobalScale + PosY + borderGS + BarTopOffset(), 0);

    if (scrollX){
        w3dEngine::Scissor(x, glY + marginGS, width - marginGS - borderGS, height - marginGS); // igual a tu viewport - los ojos
    }
    else {
        w3dEngine::Scissor(x, glY, width - marginGS - borderGS, height); // igual a tu viewport - los ojos
    }

    cullBaseY = GlobalScale + PosY + borderGS + BarTopOffset(); filaDFS = 0; // culling: Y de la 1er fila del recorrido de OJOS
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        DibujarOjos(SceneCollection->Childrens[c], !SceneCollection->Childrens[c]->visible,
                    !SceneCollection->Childrens[c]->renderizable);
    }
    w3dEngine::PopMatrix();
    w3dEngine::Disable(w3dEngine::ScissorTest);

    // vista previa del drag: LINEA VERDE donde se va a insertar
    if (dragging && (dropZona == 0 || dropZona == 2)) {
        static Rec2D* linea = NULL;
        if (!linea) linea = new Rec2D();
        int lineY = borderGS + PosY + BarTopOffset()
                    + (dropFila + (dropZona == 2 ? 1 : 0)) * RenglonHeightGS;
        w3dEngine::Disable(w3dEngine::Texture2D);
        SetColorID(ColorID::accent);
        // la linea se INDENTA al nivel del destino: si el objeto va a quedar emparentado (nivel > 0)
        // arranca mas a la derecha y es mas angosta; a la raiz (nivel 0) ocupa todo el ancho.
        int indent = dropProf * (IconSizeGS + gapGS);
        linea->SetSize((GLshort)(borderGS + indent), (GLshort)(lineY - GlobalScale),
                       (GLshort)(width - bordersGS - indent), (GLshort)(2 * GlobalScale));
        linea->RenderObject(false);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }

    RenderBar();
    DibujarBordes(this);
    DibujarScrollbar(this);
#ifdef W3D_SYMBIAN
    w3dEngine::EnableArray(w3dEngine::NormalArray); // baseline que asume la escena
#endif
}

void Outliner::DibujarRenglon(Object* obj, bool hidden){
    // CULLING: solo se DIBUJA la fila si cae en el area visible. El traversal de hijos (mas abajo) avanza la matriz
    // igual, asi que las filas visibles quedan bien ubicadas. Margen de 1 fila arriba/abajo (no cortar filas al borde).
    int myY = cullBaseY + (int)filaDFS * (int)RenglonHeightGS; filaDFS++;
    bool filaVisible = (myY + (int)RenglonHeightGS * 2 > 0) && (myY < (int)height + (int)RenglonHeightGS);
    if (filaVisible) {
    w3dEngine::PushMatrix();
    GLfloat opacityRow = hidden ? 0.5f : 1.0f;

    if (moviendo && obj == moverObj){
        // MODO MOVER con teclado: el objeto que se esta moviendo se resalta (accent),
        // para que en el N95 (sin mouse) se vea claro cual se esta reordenando.
        SetColorID(ColorID::accent, opacityRow);
    }
    else if (dragging){
        // mientras se ARRASTRA el outliner entero se ve deseleccionado;
        // solo el objeto arrastrado queda marcado (la seleccion vuelve
        // a verse normal al soltar)
        if (obj == dragObjeto){
            SetColorID(ColorID::accent, opacityRow);
        } else {
            SetColorID(ColorID::grisUI, opacityRow);
        }
    }
    else if (obj == ObjActivo){
        //std::cout << "Objeto activo en el outliner: " << reinterpret_cast<Text*>(SceneCollection->Childrens[c]->name->data)->value << "\n";
        if (obj->select){
            SetColorID(ColorID::accent, opacityRow);
        }
        else {
            SetColorID(ColorID::blanco, opacityRow);
        }
    }
    else if (obj->select){
        SetColorID(ColorID::accentDark, opacityRow);
    }
    else {
        SetColorID(ColorID::grisUI, opacityRow);
    }

    //icono desplegar (si no tiene hijos: flecha a la derecha). El armature 2D VIRTUAL tambien
    //cuenta como "hijo" (una malla sin hijos objeto pero con huesos 2D se puede desplegar).
    if (!OutTieneHijos(obj) || !obj->desplegado){
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::arrowRight)]->uvs);
    }
    else {
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::arrow)]->uvs);
    }

    //icono de la coleccion
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    W3dDrawStrip4(IconMesh, IconsUV[IconoDeObjeto(obj)]->uvs);

    //texto render
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    RenderBitmapText(obj->name);

    w3dEngine::PopMatrix();
    } // fin del DRAW de la fila (culling); el traversal de hijos de abajo corre siempre

    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (!OutTieneHijos(obj) || !obj->desplegado) return;
    Mesh* virt = Arm2DDe(obj); // NULL = la malla no tiene armatures 2D (no suma filas)
    const int nArm = Arm2DCant(obj);

    //linea
    w3dEngine::PushMatrix();
    for (int k = 0; k < nArm; k++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
    }
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
    }
    w3dEngine::PopMatrix();

    //flechas
    w3dEngine::PushMatrix();
    DibujarLineaDesplegada(obj);
    w3dEngine::PopMatrix();

    //renglon normal
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    for (int k = 0; k < nArm; k++){       // los ARMATURES 2D van primero (son DATO de la malla)
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        DibujarArm2D(virt, k, hidden);
    }
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        DibujarRenglon(obj->Childrens[o],
            hidden ? true : !obj->Childrens[o]->visible);
    }
    w3dEngine::Translatef(-IconSizeGS - gapGS, 0, 0);
}

// fila VIRTUAL de UN armature 2D de la malla: una fila informativa (icono de armature + nombre del
// armature), en AZUL (= parte del mesh, no un objeto de escena) y SIN flechita (no se despliega:
// los huesos se listan en la pestania "Armature 2D" del panel). Ocupa 1 renglon y no mueve la
// matriz. El armature ACTIVO va en azul PLENO y los demas apagados, como en el editor UV.
void Outliner::DibujarArm2D(Mesh* m, int idx, bool hidden){
    if (!m || idx < 0 || idx >= (int)m->armatures2d.size()) return;
    const GLfloat op = (hidden || idx != m->armature2dActivo) ? 0.5f : 1.0f; // el ACTIVO, pleno
    int myY = cullBaseY + (int)filaDFS * (int)RenglonHeightGS; filaDFS++;
    if (!((myY + (int)RenglonHeightGS * 2 > 0) && (myY < (int)height + (int)RenglonHeightGS))) return;
    w3dEngine::PushMatrix();
    w3dEngine::Color4f(kArm2DAzul[0], kArm2DAzul[1], kArm2DAzul[2], op);
    // la columna de la flechita queda VACIA (no hay nada que desplegar): solo se saltea para que
    // el icono y el texto queden alineados con los de las filas de objetos
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::armature)]->uvs);
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    RenderBitmapText(m->armatures2d[idx] ? m->armatures2d[idx]->nombre : std::string("Armature 2D"));
    w3dEngine::PopMatrix();
}

void Outliner::DibujarLineaDesplegada(Object* obj){
    for (int k = 0, n = Arm2DCant(obj); k < n; k++){ // cada fila virtual lleva su linea
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
    }
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
        DibujarLineaDesplegada(obj->Childrens[o]);
    }
}

void Outliner::DibujarOjos(Object* obj, bool hidden, bool noRender){
    // CULLING: mismo criterio que DibujarRenglon (el Translatef de avance de abajo corre siempre para ubicar a los hijos)
    int myY = cullBaseY + (int)filaDFS * (int)RenglonHeightGS; filaDFS++;
    if ((myY + (int)RenglonHeightGS * 2 > 0) && (myY < (int)height + (int)RenglonHeightGS)) {
        // OJO (visible): ocultado por si mismo o por un PADRE oculto -> tenue
        SetColorID(ColorID::grisUI, hidden ? 0.5f : 1.0f);
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(obj->visible ? IconType::visible : IconType::hidden)]->uvs);
        // CAMARA (renderizar) a la DERECHA del ojo: si el objeto NO se renderiza -> camara_off;
        // si SI pero un PADRE no se renderiza -> camara encendida pero SEMI-TRANSPARENTE (igual que el ojo)
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
        SetColorID(ColorID::grisUI, noRender ? 0.5f : 1.0f);
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(obj->renderizable ? IconType::camera : IconType::camera_off)]->uvs);
        w3dEngine::PopMatrix();
    }
    w3dEngine::Translatef(0, RenglonHeightGS, 0);

    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (!OutTieneHijos(obj) || !obj->desplegado) return;

    // las filas VIRTUALES de armature 2D no tienen ojo ni camara (no son objetos), pero SI ocupan
    // fila -> hay que avanzar el contador de culling y la matriz para no desalinear los ojos.
    for (int k = 0, n = Arm2DCant(obj); k < n; k++){ filaDFS++; w3dEngine::Translatef(0, RenglonHeightGS, 0); }

    for (size_t o = 0; o < obj->Childrens.size(); o++){
        DibujarOjos(obj->Childrens[o],
                    hidden   ? true : !obj->Childrens[o]->visible,
                    noRender ? true : !obj->Childrens[o]->renderizable);
    }
}

void Outliner::button_left(){
    if (mouseOverScrollY){
        mouseOverScrollYpress = true;
    }
}

#ifndef W3D_SYMBIAN
void Outliner::mouse_button_up(int boton){
    ViewPortClickDown = false;
    if (boton == W3dMB_IZQ) {
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
    //else if (boton == W3dMB_MEDIO) {
    //    middleMouseDown = false;
    //}
    FindMouseOver(lastMouseX,lastMouseY);
}
#endif

#ifndef W3D_SYMBIAN
void Outliner::event_mouse_wheel(float dy, int mx, int my){
    {
      if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; } // sobre la barra -> horizontal
    MouseWheel = true;
    ScrollY(dy*6*GlobalScale);
    MouseWheel = false;
}
#endif

void Outliner::FindMouseOver(int mx, int my){
    // (el hover de las barras lo calcula LayoutInput con la zona del
    // agarre; la llamada vieja a ScrollMouseOver pisaba ese estado)
}

// TOUCH: arrastrar 1 dedo sobre el CONTENIDO = scroll (v/h). La barra la maneja el gesto lockeado.
bool Outliner::event_finger_scroll(int px, int py, int dx, int dy){
    ScrollByTouch(dx, dy);
    return true;
}

void Outliner::event_mouse_motion(int mx, int my) {
    // hover: fila bajo el mouse (feedback antes del click)
    hoverFila = (my - y - borderGS - PosY - BarTopOffset()) / RenglonHeightGS;
    // el "scrollbar area" esta reservada: ahi no hay hover de renglon
    if (mouseOverScrollY || mouseOverScrollX) {
        hoverFila = -1;
    }
    if (leftMouseDown && dragObjeto) {
        int d = my - dragY0;
        if (d < 0) d = -d;
        if (!dragging && d > RenglonHeightGS / 2) dragging = true;
        if (dragging) {
            // vista previa del drop: linea de insercion o futuro padre
            dropZona = -2;
            if (Contains(mx, my)) {
                int rel = my - y - borderGS - PosY - BarTopOffset();
                if (rel >= 0) {
                    dropFila = rel / RenglonHeightGS;
                    int resto = rel % RenglonHeightGS;
                    int f = dropFila;
                    OutFila hitF;
                    bool hay = W3dFilaEnArbol(f, hitF);
                    // fila VIRTUAL del armature 2D: NO es destino de drop (no es un objeto; el
                    // armature 2D no se puede re-emparentar desde el outliner)
                    if (hay && hitF.arm2d) { dropZona = -2; return; }
                    Object* destino = hay ? hitF.obj : NULL;
                    dropProf = hay ? hitF.prof : 0; // profundidad del destino: la linea se indenta ahi
                    if (!destino) dropZona = -1; // al vacio: a la raiz
                    else if (destino == dragObjeto) dropZona = -2;
                    else if (resto < RenglonHeightGS / 4) dropZona = 0;
                    else if (resto > (RenglonHeightGS * 3) / 4) dropZona = 2;
                    else dropZona = 1;
                }
            }
            return; // mientras se arrastra no scrollea
        }
    }
    if (middleMouseDown || leftMouseDown) {
        ViewPortClickDown = true;

        ScrollX(dx);
        ScrollY(dy);
        return;
    }
    //si no se esta haciendo click. entonces miras si el mouse esta encima de algo
    else if (scrollY){
        FindMouseOver(mx, my);
    }
}

#ifndef W3D_SYMBIAN
void Outliner::event_key_down(int tecla, bool repeticion){
    const int key = tecla;
    if (repeticion == 0) {
        // MODO MOVER: las flechas reordenan/reparentan en vez de navegar; OK confirma; C/backspace/Esc cancela.
        if (moviendo) {
            switch (key) {
                case W3dK_UP:    MoverPaso(0); return;
                case W3dK_DOWN:  MoverPaso(1); return;
                case W3dK_LEFT:  MoverPaso(2); return; // izquierda = SACAR (unparent)
                case W3dK_RIGHT: MoverPaso(3); return; // derecha = METER (parent bajo el hermano anterior)
                case W3dK_RETURN: case W3dK_KP_ENTER: MoverConfirmar(); return;
                case W3dK_ESCAPE: case W3dK_BACKSPACE: case W3dK_C: MoverCancelar(); return;
                default: return; // en modo mover se traga el resto
            }
        }
        switch (key) {
            case W3dK_G: // g = entrar en modo MOVER (reordenar / reparentar el objeto activo)
                MoverIniciar();
                break;
            case W3dK_A:
                SeleccionarTodo(true);
                break;
            case W3dK_H:
                ChangeVisibilityObj();
                break;
            case W3dK_X:
            case W3dK_DELETE:   // Supr borra igual que X
                if (estado == editNavegacion){
                    AbrirConfirmarBorrado(true); // popup de confirmacion (incluye colecciones); Si -> borra con undo
                }
                break;
            case W3dK_LEFT:
                SetDesplegado(false);
                break;
            case W3dK_RIGHT:
                SetDesplegado(true);
                break;
            case W3dK_UP:
                changeSelect(SelectMode::PrevSingle, true);
                AsegurarVisible();
                break;
            case W3dK_DOWN:
                changeSelect(SelectMode::NextSingle, true);
                AsegurarVisible();
                break;
            case W3dK_KP_PERIOD: // numpad "." = centrar la seleccion (llega por hover, como todas
                CentrarSeleccion(); // las teclas por-viewport: el ruteo manda al viewport bajo el mouse)
                break;
        };
    }
}
#endif

#ifndef W3D_SYMBIAN
void Outliner::event_key_up(int tecla){
    const int key = tecla;
    switch (key) {
        case W3dK_LSHIFT:
            if (ShiftCount < 20){
                changeSelect(SelectMode::NextSingle, true);
                AsegurarVisible();
            }
            ShiftCount = 0;
            LShiftPressed = false;
            break;
        case W3dK_LALT:
            LAltPressed = false;
            break;
    }
}
#endif

// fila visible N (respetando desplegado) -> objeto REAL o fila VIRTUAL del armature 2D.
// Devuelve true si la fila cayo en este subarbol (out queda cargado).
static bool W3dFilaVisibleRec(Object* obj, int& fila, int prof, OutFila& out) {
    if (fila == 0) { out.obj = obj; out.arm2d = NULL; out.prof = prof; return true; }
    fila--;
    if (obj->desplegado) {
        Mesh* am = Arm2DDe(obj);
        for (int k = 0, n = Arm2DCant(obj); k < n; k++) { // una fila por armature 2D (sin hijos)
            if (fila == 0) { out.obj = NULL; out.arm2d = am; out.arm2dIdx = k; out.prof = prof + 1; return true; }
            fila--;
        }
        for (size_t o = 0; o < obj->Childrens.size(); o++)
            if (W3dFilaVisibleRec(obj->Childrens[o], fila, prof + 1, out)) return true;
    }
    return false;
}

// fila N del arbol completo: objeto real o fila virtual. false = la fila esta en el VACIO
// (debajo del ultimo renglon) -> ahi el drop desemparenta a la raiz, como siempre.
static bool W3dFilaEnArbol(int fila, OutFila& out) {
    if (!SceneCollection) return false;
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++)
        if (W3dFilaVisibleRec(SceneCollection->Childrens[c], fila, 0, out)) return true;
    return false;
}

// objeto en la fila visible N (+ su profundidad en profOut). NULL = vacio o fila VIRTUAL
// (la fila del armature 2D no es un objeto: no se selecciona ni es destino de drop).
static Object* W3dObjetoEnFila(int fila, int* profOut) {
    OutFila f;
    if (!W3dFilaEnArbol(fila, f)) return NULL;
    if (profOut) *profOut = f.prof;
    return f.obj;
}

// numero de fila visible de un objeto (-1 si no esta a la vista)
static bool W3dBuscarFila(Object* obj, Object* objetivo, int* fila) {
    if (obj == objetivo) return true;
    (*fila)++;
    if (obj->desplegado) {
        (*fila) += Arm2DCant(obj);    // las filas virtuales de armature 2D corren la numeracion
        for (size_t o = 0; o < obj->Childrens.size(); o++) {
            if (W3dBuscarFila(obj->Childrens[o], objetivo, fila)) return true;
        }
    }
    return false;
}

static int W3dFilaDe(Object* objetivo) {
    if (!objetivo || !SceneCollection) return -1;
    int fila = 0;
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        if (W3dBuscarFila(SceneCollection->Childrens[c], objetivo, &fila)) {
            return fila;
        }
    }
    return -1;
}

// NUMPAD "." = centrar la SELECCION en la vista (el analogo del Frame Selected de Blender,
// por diseno). Scrollea PosY para que la fila objetivo quede en el MEDIO del area de
// contenido. Con varios seleccionados se centra el PROMEDIO de sus filas VISIBLES (un objeto
// dentro de un padre plegado no tiene fila: no aporta); sin seleccion visible cae al objeto
// ACTIVO. Solo scroll vertical: no toca PosX, la seleccion ni el desplegado.
void Outliner::CentrarSeleccion() {
    if (!SceneCollection) return;
    long suma = 0; int n = 0;
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        int f = W3dFilaDe(ObjSelects[i]);
        if (f >= 0) { suma += f; n++; }
    }
    if (n == 0 && ObjActivo) {
        int f = W3dFilaDe(ObjActivo);
        if (f >= 0) { suma = f; n = 1; }
    }
    if (n == 0) return;                     // nada seleccionado a la vista: no hay que encuadrar
    int fila = (int)(suma / n);
    // geometria de las filas (la misma cuenta que Render/ClickSeleccionar):
    //   y_local(fila) = borderGS + PosY + BarTopOffset() + fila * RenglonHeightGS
    // se pide y_local(fila) + RenglonHeightGS/2 == centro del area de contenido.
    int top = BarTopOffset();
    int centro = top + (height - top) / 2;
    int nuevo = centro - (int)RenglonHeightGS / 2 - borderGS - top - fila * (int)RenglonHeightGS;
    if (nuevo > 0) nuevo = 0;               // no scrollear "antes" del inicio
    if (nuevo < MaxPosY) nuevo = MaxPosY;   // ni mas alla del final (MaxPosY es <= 0)
    if (nuevo != PosY) { PosY = nuevo; g_redraw = true; }
}

// AUTO-SCROLL de la navegacion por teclado: el scroll MINIMO para que la fila del
// objeto ACTIVO entre entera en el area de contenido. Si ya se ve, no toca nada (por
// eso no reusa CentrarSeleccion: centrar en cada flecha, con 315 objetos, marea).
// Antes no habia auto-scroll de ningun tipo: el unico encuadre era el numpad ".", asi
// que bajando con la flecha el highlight se iba abajo de todo y no se veia mas.
void Outliner::AsegurarVisible() {
    if (!SceneCollection || !ObjActivo) return;
    int fila = W3dFilaDe(ObjActivo);
    if (fila < 0) return;                   // adentro de una rama plegada: no tiene fila
    const int top = BarTopOffset();
    const int alto = (int)RenglonHeightGS;
    // y_local(fila) = borderGS + PosY + top + fila * alto   (la misma cuenta de Render)
    const int yFila = borderGS + PosY + top + fila * alto;
    int nuevo = PosY;
    if (yFila < top)                  nuevo = PosY + (top - yFila);              // se fue por ARRIBA
    else if (yFila + alto > height)   nuevo = PosY - (yFila + alto - height);    // se fue por ABAJO
    if (nuevo > 0) nuevo = 0;               // no scrollear "antes" del inicio
    if (nuevo < MaxPosY) nuevo = MaxPosY;   // ni mas alla del final (MaxPosY es <= 0)
    if (nuevo != PosY) { PosY = nuevo; g_redraw = true; }
}

// suelta el arrastre de una fila: reordena (bordes de la fila destino),
// emparenta (centro de la fila, manteniendo la transformacion) o manda
// a la raiz (soltar en el vacio)
void Outliner::SoltarDrag(int mx, int my) {
    Object* obj = dragObjeto;
    bool estaba = dragging;
    dragObjeto = NULL;
    dragging = false;
    dropZona = -2;
    if (!obj || !estaba) return;
    if (!Contains(mx, my)) return;
    int rel = my - y - borderGS - PosY - BarTopOffset();
    if (rel < 0) return;
    int fila = rel / RenglonHeightGS;
    int resto = rel % RenglonHeightGS;
    int f = fila;
    OutFila hitF;
    bool hay = W3dFilaEnArbol(f, hitF);
    if (hay && hitF.arm2d) return;   // fila VIRTUAL del armature 2D: no acepta drops
    Object* destino = hay ? hitF.obj : NULL;
    if (!destino) {
        // al vacio: desemparenta hacia la raiz, sin moverse del lugar
        ReparentKeepTransform(obj, SceneCollection);
    } else if (destino != obj) {
        if (resto < RenglonHeightGS / 4) {
            MoverJuntoA(obj, destino, false); // reordenar: antes
        } else if (resto > (RenglonHeightGS * 3) / 4) {
            MoverJuntoA(obj, destino, true); // reordenar: despues
        } else {
            // al centro: pasa a ser HIJO del destino (keep transform)
            ReparentKeepTransform(obj, destino);
            destino->desplegado = true;
        }
    }
    Resize(width, height);
}

// ---- MODO MOVER con teclado (reordenar / reparentar sin mouse) ----
// NOMBRES: meter un objeto ADENTRO de otra escena (dir 3) le cambia el ESPACIO DE NOMBRES
// y el embudo del reparent lo puede renumerar (ver W3dAdjuntarA en ObjectMode.cpp).
//
// EL MODAL SUPRIME LOS RENAMES Y LOS RESUELVE AL CONFIRMAR. Motivo: cada paso que cruzaba
// de escena renumeraba el subarbol Y empujaba su propio MultiRenameUndo. Cancelar
// restauraba los nombres a mano, pero esos pasos QUEDABAN en el historial convertidos en
// no-ops (sus valores viejos ya eran los actuales): N Ctrl+Z que no hacen nada. Y no hay
// forma de sacarlos del stack (el modulo de undo no expone un "pop"). Entonces se invierte
// el orden: durante el modal se levanta W3dNombresCargando -LA MISMA puerta que usan los
// loaders, que hace que W3dAdjuntarA no renumere ni capture undo- y la uniquificacion se
// hace UNA sola vez al CONFIRMAR, con UN solo paso de undo. Cancelar ya no tiene nada que
// sacar del historial (y el snapshot de nombres queda igual, como red de seguridad).
// Los nombres pueden verse repetidos MIENTRAS dura el modal: es transitorio y se resuelve
// al soltar, igual que la posicion del objeto.
static std::vector<std::pair<Object*, std::string> > gMoverNombres;
static void MoverJuntarNombres(Object* o) {
    if (!o) return;
    gMoverNombres.push_back(std::make_pair(o, o->name));
    for (size_t i = 0; i < o->Childrens.size(); i++) MoverJuntarNombres(o->Childrens[i]);
}
// PRE-ORDEN (el mismo recorrido que usa la busqueda por nombre y que junta W3dAdjuntarA):
// asi el que conserva el nombre pelado es el mismo objeto en los dos caminos.
static void MoverJuntarSubarbol(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) MoverJuntarSubarbol(o->Childrens[i], out);
}
// al CONFIRMAR: re-uniquifica el subarbol movido en su scope definitivo, en UN paso de
// undo (misma receta que W3dAdjuntarA, que aca quedo cortocircuitado por el guard).
static void MoverUniquificarSubarbol(Object* raiz) {
    if (!raiz) return;
    std::vector<Object*> sub;
    MoverJuntarSubarbol(raiz, sub);
    bool hayCambio = false;
    for (size_t i = 0; i < sub.size() && !hayCambio; i++)
        if (sub[i]->NombreLibre(sub[i]->name) != sub[i]->name) hayCambio = true;
    if (!hayCambio) return;
    // Object::name = destino 'Directo' legitimo (ver W3dRenameDest en Undo.h)
    std::vector<W3dRenameDest> puntas;
    for (size_t i = 0; i < sub.size(); i++) puntas.push_back(W3dDestNombre(&sub[i]->name));
    UndoCapturarRenames(puntas);   // TODAS las puntas en UN comando
    int n = 0; std::string primeros;
    for (size_t i = 0; i < sub.size(); i++) {
        const std::string viejo = sub[i]->name;
        sub[i]->SetNameObj(viejo);
        if (sub[i]->name == viejo) continue;
        n++;
        if (n <= 3) { if (!primeros.empty()) primeros += ", "; primeros += viejo + " -> " + sub[i]->name; }
    }
    if (n > 0) {
        char cant[32]; sprintf(cant, "%d", n);
        Notificar(std::string(cant) + " nombre(s) renumerados al cambiar de escena (" +
                  primeros + (n > 3 ? ", ...)" : ")") + ". Revisa tus scripts lua.", true);
    }
}

// El objeto que se mueve es el ACTIVO. Guarda su posicion original para poder cancelar.
void Outliner::MoverIniciar() {
    if (moviendo) return;
    if (!ObjActivo || !SceneCollection) return;
    moverObj = ObjActivo;
    gMoverNombres.clear();
    MoverJuntarNombres(moverObj);
    // UNDO: los pasos de adentro del modal NO capturan (W3dNombresCargando esta levantado, ver
    // MoverPaso). El estado PREVIO se toma ACA, una sola vez, y se empuja al CONFIRMAR: asi el
    // modal deja EXACTAMENTE un paso -mudanza + renumeracion- y el cancelar no deja ninguno.
    UndoReparentIniciar(moverObj);
    moverPadreOrig = moverObj->Parent;   // NULL = raiz
    moverAnteriorOrig = NULL;
    Object* padre = moverObj->Parent ? moverObj->Parent : SceneCollection;
    for (size_t i = 0; i < padre->Childrens.size(); i++)
        if (padre->Childrens[i] == moverObj) { if (i > 0) moverAnteriorOrig = padre->Childrens[i-1]; break; }
    moviendo = true;
}

// dir: 0=arriba 1=abajo (reordena entre hermanos) 2=afuera(unparent) 3=adentro(parent bajo el hermano anterior)
void Outliner::MoverPaso(int dir) {
    if (!moviendo || !moverObj || !SceneCollection) return;
    Object* padre = moverObj->Parent ? moverObj->Parent : SceneCollection;
    int idx = -1;
    for (size_t i = 0; i < padre->Childrens.size(); i++)
        if (padre->Childrens[i] == moverObj) { idx = (int)i; break; }
    if (idx < 0) return;
    // los renames (y su undo) se posponen al MoverConfirmar: ver el bloque de arriba
    struct MoverGuard { bool prev; MoverGuard(){ prev = W3dNombresCargando; W3dNombresCargando = true; }
                        ~MoverGuard(){ W3dNombresCargando = prev; } } moverGuard;
    if (dir == 0) {                         // ARRIBA: antes del hermano anterior
        if (idx > 0) { Object* ref = padre->Childrens[idx-1]; MoverJuntoA(moverObj, ref, false); }
    } else if (dir == 1) {                  // ABAJO: despues del hermano siguiente
        if (idx + 1 < (int)padre->Childrens.size()) { Object* ref = padre->Childrens[idx+1]; MoverJuntoA(moverObj, ref, true); }
    } else if (dir == 2) {                  // AFUERA (unparent): al abuelo, justo despues del padre
        if (padre != SceneCollection) {
            Object* viejoPadre = padre;
            Object* abuelo = padre->Parent ? padre->Parent : SceneCollection;
            ReparentKeepTransform(moverObj, abuelo);
            MoverJuntoA(moverObj, viejoPadre, true);
        }
    } else if (dir == 3) {                  // ADENTRO (parent): hijo del hermano anterior
        if (idx > 0) {
            Object* nuevoPadre = padre->Childrens[idx-1];
            ReparentKeepTransform(moverObj, nuevoPadre);
            nuevoPadre->desplegado = true;
        }
    }
    Resize(width, height); // recalcular el scroll
}

void Outliner::MoverConfirmar() {
    if (moviendo && moverObj) {
        UndoGrupoIniciar();          // la mudanza y los nombres, en UN SOLO Ctrl+Z
        UndoReparentConfirmar();     // el paso del ARBOL (capturado en MoverIniciar) va PRIMERO
        MoverUniquificarSubarbol(moverObj); // recien ACA se renumera (y empuja su paso)
        UndoGrupoFin();
    } else {
        UndoReparentCancelar();
    }
    moviendo = false; moverObj = NULL; gMoverNombres.clear();
}

void Outliner::MoverCancelar() {
    UndoReparentCancelar();   // el objeto vuelve solo: el modal no deja NINGUN paso (test 'nombresmover')
    if (moviendo && moverObj && SceneCollection) {
        // el regreso tampoco tiene que renumerar ni empujar undo (los pasos ya no lo hicieron)
        struct MoverGuard { bool prev; MoverGuard(){ prev = W3dNombresCargando; W3dNombresCargando = true; }
                            ~MoverGuard(){ W3dNombresCargando = prev; } } moverGuard;
        Object* padreOrig = moverPadreOrig ? moverPadreOrig : SceneCollection;
        ReparentKeepTransform(moverObj, padreOrig); // restaura el padre + el transform local (el mundo se mantuvo)
        if (moverAnteriorOrig) {
            MoverJuntoA(moverObj, moverAnteriorOrig, true); // justo despues del hermano que estaba antes
        } else {
            for (size_t i = 0; i < padreOrig->Childrens.size(); i++) // era el PRIMERO: al frente
                if (padreOrig->Childrens[i] != moverObj) { MoverJuntoA(moverObj, padreOrig->Childrens[i], false); break; }
        }
        // ...y los NOMBRES (el reparent los pudo renumerar al cruzar de escena)
        for (size_t i = 0; i < gMoverNombres.size(); i++)
            if (gMoverNombres[i].first) gMoverNombres[i].first->SetNameObj(gMoverNombres[i].second);
        Resize(width, height);
    }
    moviendo = false; moverObj = NULL; gMoverNombres.clear();
}

void Outliner::ClickSeleccionar(int mx, int my) {
    if (!SceneCollection) return;
    int rel = my - y - borderGS - PosY - BarTopOffset();
    if (rel < 0) return;   // franja del borde: la division de un negativo chico daba fila 0
    int fila = rel / RenglonHeightGS;
    int filaClick = fila; // (el walk de abajo consume "fila")
    // columnas de la derecha: CAMARA (renderizar) y OJO (a su izquierda), corridas
    // por la barra de scroll vertical si esta presente
    int reservaBarra = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    bool enCamara = (mx - x) >= (width - IconSizeGS - marginGS - borderGS - reservaBarra) &&
                    (mx - x) <  (width - marginGS - borderGS - reservaBarra);
    bool enOjo = !enCamara && (mx - x) >= (width - 2*IconSizeGS - gapGS - marginGS - borderGS - reservaBarra) &&
                 (mx - x) < (width - IconSizeGS - gapGS - marginGS - borderGS - reservaBarra);
    // ---- fila VIRTUAL del armature 2D: INERTE. No se selecciona, no se arrastra, no se empareja,
    // no tiene ojo/camara y no se despliega (los huesos se listan y se editan en la pestania
    // "Armature 2D" del panel Properties). El click se COME aca para que no caiga en la malla.
    {
        int fv = fila; OutFila hv;
        if (W3dFilaEnArbol(fv, hv) && hv.arm2d) return;
    }
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        int prof = 0;
        OutFila hf;
        Object* hit = W3dFilaVisibleRec(SceneCollection->Childrens[c], fila, 0, hf) ? hf.obj : NULL;
        prof = hf.prof;
        if (hit) {
            // la FLECHA de desplegar (primer icono, segun la profundidad)
            int xFlecha = x + marginGS + PosX + prof * (IconSizeGS + gapGS);
            if (!enOjo && !enCamara && mx >= xFlecha && mx < xFlecha + IconSizeGS &&
                !hit->Childrens.empty()) {
                hit->desplegado = !hit->desplegado;
                Resize(width, height); // recalcular el scroll
                return;
            }
            if (!enOjo && !enCamara) {
                // el click puede convertirse en ARRASTRE (reordenar /
                // emparentar): se confirma al moverse con el boton
                dragObjeto = hit;
                dragging = false;
                dragY0 = my;
            }
            if (enCamara) {
                hit->renderizable = !hit->renderizable;
            } else if (enOjo) {
                hit->visible = !hit->visible;
            } else if (LShiftPressed) {
                // shift+click: RANGO desde el activo hasta la fila
                // clickeada (1 y 4 -> 1,2,3,4), sumando a la seleccion
                int desde = W3dFilaDe(ObjActivo);
                if (desde < 0) {
                    hit->Seleccionar();
                } else {
                    int a = desde < filaClick ? desde : filaClick;
                    int b = desde < filaClick ? filaClick : desde;
                    for (int f = a; f <= b; f++) {
                        Object* o = W3dObjetoEnFila(f);
                        // el rango tiene que quedar en ObjSelects, no solo con select=true:
                        // operaciones como Join arman su lista desde ObjSelects pero borran por el
                        // flag select -> un rango select=true fuera de ObjSelects se borra SIN unirse
                        // (las mallas intermedias "desaparecen"). Mantener ambos consistentes.
                        if (o && !o->select) { o->select = true; ObjSelects.push_back(o); }
                    }
                    hit->Seleccionar(); // el clickeado queda activo
                }
            } else if (LCtrlPressed) {
                // ctrl+click: agregar/sacar UNO de la seleccion (Deseleccionar lo saca de ObjSelects,
                // no solo select=false -> sino quedaba en ObjSelects y ops como Join lo procesaban igual)
                if (hit->select) { hit->Deseleccionar(); if (ObjActivo == hit) ObjActivo = NULL; }
                else { hit->Seleccionar(); }
            } else if (!enOjo && !enCamara) {
                DeseleccionarTodo();
                hit->Seleccionar();
            }
            return;
        }
    }
}

void Outliner::key_down_return(){
}

Outliner::~Outliner() {
    delete Renglon;
}