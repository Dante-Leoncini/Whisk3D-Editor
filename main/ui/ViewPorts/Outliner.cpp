#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#ifdef _WIN32
    #include <windows.h>
#endif

#include "Outliner.h"
#include "WhiskUI/draw/glesdraw.h"
#include "objects/ObjectMode.h" // reparent del drag&drop
#include "objects/Instance.h"   // IconoDeObjeto: array/mirror/instance segun el modo
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (confirmar antes de borrar)
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
        case ObjectType::gamepad:    return (size_t)IconType::gamepad;
        case ObjectType::instance: { // array / mirror / instance segun el modo
            Instance* in = (Instance*)o;
            if (in->mirror)    return (size_t)IconType::mirror;
            if (in->count > 1) return (size_t)IconType::array;
            return (size_t)IconType::instance;
        }
        case ObjectType::constraint: return (size_t)IconType::constraint;
        default:                     return (size_t)IconType::archive;
    }
}

// Constructor
static Object* W3dObjetoEnFila(int fila, int* profOut = 0); // profOut (opcional) = profundidad del objeto

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
    if (obj->Childrens.size() < 1 || !obj->desplegado) return;

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
            if (o->Childrens.size() >= 1 && o->desplegado) for (size_t i = 0; i < o->Childrens.size(); i++) n += rec(o->Childrens[i]);
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
        w3dEngine::Scissor(x, glY + marginGS, width - IconSizeGS - marginGS - borderGS - gapGS, height - marginGS); // igual a tu viewport - los ojos
    }
    else {
        w3dEngine::Scissor(x, glY, width - IconSizeGS - marginGS - borderGS - gapGS, height); // igual a tu viewport - los ojos
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
    w3dEngine::Translatef(width - IconSizeGS - marginGS - borderGS, GlobalScale + PosY + borderGS + BarTopOffset(), 0);

    if (scrollX){
        w3dEngine::Scissor(x, glY + marginGS, width - marginGS - borderGS, height - marginGS); // igual a tu viewport - los ojos
    }
    else {
        w3dEngine::Scissor(x, glY, width - marginGS - borderGS, height); // igual a tu viewport - los ojos
    }

    cullBaseY = GlobalScale + PosY + borderGS + BarTopOffset(); filaDFS = 0; // culling: Y de la 1er fila del recorrido de OJOS
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        DibujarOjos(SceneCollection->Childrens[c], !SceneCollection->Childrens[c]->visible);
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

    //icono desplegar (si no tiene hijos: flecha a la derecha)
    if (obj->Childrens.size() < 1 || !obj->desplegado){
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
    if (obj->Childrens.size() < 1 || !obj->desplegado) return;

    //linea
    w3dEngine::PushMatrix();
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
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        DibujarRenglon(obj->Childrens[o],
            hidden ? true : !obj->Childrens[o]->visible);
    }
    w3dEngine::Translatef(-IconSizeGS - gapGS, 0, 0);
}

void Outliner::DibujarLineaDesplegada(Object* obj){
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
        DibujarLineaDesplegada(obj->Childrens[o]);
    }
}

void Outliner::DibujarOjos(Object* obj, bool hidden){
    // CULLING: mismo criterio que DibujarRenglon (el Translatef de avance de abajo corre siempre para ubicar a los hijos)
    int myY = cullBaseY + (int)filaDFS * (int)RenglonHeightGS; filaDFS++;
    if ((myY + (int)RenglonHeightGS * 2 > 0) && (myY < (int)height + (int)RenglonHeightGS)) {
        GLfloat opacityRow = hidden ? 0.5f : 1.0f;
        SetColorID(ColorID::grisUI, opacityRow);
        if (obj->visible){
            W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::visible)]->uvs);
        }
        else {
            W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::hidden)]->uvs);
        }
    }
    w3dEngine::Translatef(0, RenglonHeightGS, 0);

    //si no tiene hijos. o no esta desplegado se ahorra todos los bucles siguentes
    if (obj->Childrens.size() < 1 || !obj->desplegado) return;

    for (size_t o = 0; o < obj->Childrens.size(); o++){
        DibujarOjos(obj->Childrens[o],
                    hidden ? true : !obj->Childrens[o]->visible);
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
                    int prof = 0;
                    Object* destino = W3dObjetoEnFila(f, &prof);
                    dropProf = prof; // profundidad del destino: la linea de insercion se indenta a ese nivel
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
                break;
            case W3dK_DOWN:
                changeSelect(SelectMode::NextSingle, true);
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

// fila visible N (respetando desplegado) -> objeto + profundidad
static Object* W3dFilaVisible(Object* obj, int& fila, int prof, int* profOut) {
    if (fila == 0) { if (profOut) *profOut = prof; return obj; }
    fila--;
    if (obj->desplegado) {
        for (size_t o = 0; o < obj->Childrens.size(); o++) {
            Object* r = W3dFilaVisible(obj->Childrens[o], fila, prof + 1, profOut);
            if (r) return r;
        }
    }
    return NULL;
}

// objeto en la fila visible N del arbol completo (+ su profundidad en profOut, si se pide)
static Object* W3dObjetoEnFila(int fila, int* profOut) {
    if (!SceneCollection) return NULL;
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        Object* r = W3dFilaVisible(SceneCollection->Childrens[c], fila, 0, profOut);
        if (r) return r;
    }
    return NULL;
}

// numero de fila visible de un objeto (-1 si no esta a la vista)
static bool W3dBuscarFila(Object* obj, Object* objetivo, int* fila) {
    if (obj == objetivo) return true;
    (*fila)++;
    if (obj->desplegado) {
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
    Object* destino = W3dObjetoEnFila(f);
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
// El objeto que se mueve es el ACTIVO. Guarda su posicion original para poder cancelar.
void Outliner::MoverIniciar() {
    if (moviendo) return;
    if (!ObjActivo || !SceneCollection) return;
    moverObj = ObjActivo;
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

void Outliner::MoverConfirmar() { moviendo = false; moverObj = NULL; }

void Outliner::MoverCancelar() {
    if (moviendo && moverObj && SceneCollection) {
        Object* padreOrig = moverPadreOrig ? moverPadreOrig : SceneCollection;
        ReparentKeepTransform(moverObj, padreOrig); // restaura el padre + el transform local (el mundo se mantuvo)
        if (moverAnteriorOrig) {
            MoverJuntoA(moverObj, moverAnteriorOrig, true); // justo despues del hermano que estaba antes
        } else {
            for (size_t i = 0; i < padreOrig->Childrens.size(); i++) // era el PRIMERO: al frente
                if (padreOrig->Childrens[i] != moverObj) { MoverJuntoA(moverObj, padreOrig->Childrens[i], false); break; }
        }
        Resize(width, height);
    }
    moviendo = false; moverObj = NULL;
}

void Outliner::ClickSeleccionar(int mx, int my) {
    if (!SceneCollection) return;
    int fila = (my - y - borderGS - PosY - BarTopOffset()) / RenglonHeightGS;
    if (fila < 0) return;
    int filaClick = fila; // (el walk de abajo consume "fila")
    // columna de los OJOS (a la derecha): click = mostrar/ocultar
    bool enOjo = (mx - x) >= (width - IconSizeGS - marginGS - borderGS);
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        int prof = 0;
        Object* hit = W3dFilaVisible(SceneCollection->Childrens[c], fila, 0, &prof);
        if (hit) {
            // la FLECHA de desplegar (primer icono, segun la profundidad)
            int xFlecha = x + marginGS + PosX + prof * (IconSizeGS + gapGS);
            if (!enOjo && mx >= xFlecha && mx < xFlecha + IconSizeGS &&
                !hit->Childrens.empty()) {
                hit->desplegado = !hit->desplegado;
                Resize(width, height); // recalcular el scroll
                return;
            }
            if (!enOjo) {
                // el click puede convertirse en ARRASTRE (reordenar /
                // emparentar): se confirma al moverse con el boton
                dragObjeto = hit;
                dragging = false;
                dragY0 = my;
            }
            if (enOjo) {
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
            } else {
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