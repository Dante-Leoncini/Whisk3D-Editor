#include "ViewPorts/ViewPort3D.h"
#include <cmath>

// Variables globales
GLfloat LastRotX = 0;
GLfloat LastRotY = 0;  
GLfloat LastPivotX = 0;
GLfloat LastPivotY = 0;
GLfloat LastPivotZ = 0;

Viewport3D::Viewport3D()
    : orthographic(false), ViewFromCameraActive(false), showOverlays(true), ShowUi(true),
      showFloor(true), showYaxis(true), showXaxis(true), CameraToView(false),
      showOrigins(true), show3DCursor(true), ShowRelantionshipsLines(true),
      view(RenderType::MaterialPreview), nearClip(0.01f), farClip(1000.0f),
      cameraDistance(10.0f), aspect(1.0f), posX(0.0f), posY(0.0f), posZ(0.0f),
      rotX(23.5f), rotY(20.0f), PivotX(0.0f), PivotY(0.0f), PivotZ(0.0f) 
{}

Viewport3D::~Viewport3D() {};

// Ejemplo de implementación de un método
void Viewport3D::ReloadLights() {
    ::view = view;
    ::showOverlayGlobal = showOverlays;
    Viewport3DActive = this;
    
    for(size_t l = 0; l < Lights.size(); l++) {
        glDisable(Lights[l]->LightID);
    }
    
    switch(view){
        case RenderType::MaterialPreview:
            glEnable(GL_LIGHT0);
            glLightfv(GL_LIGHT0, GL_DIFFUSE,  MaterialPreviewDiffuse);
            glLightfv(GL_LIGHT0, GL_AMBIENT,  MaterialPreviewAmbient);
            glLightfv(GL_LIGHT0, GL_SPECULAR, MaterialPreviewSpecular);
            glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
            glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.0f);
            glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.0f);
            break;

        case RenderType::ZBuffer:
            glDisable(GL_LIGHT0);
            break;

        default:
            break;
    }
}

// Cambia entre MaterialPreview y Rendered
void Viewport3D::ChangeViewType(){
    if (view == RenderType::MaterialPreview) {
        view = RenderType::Rendered;
    } else {
        view = RenderType::MaterialPreview;
    }
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
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

#ifdef ANDROID
    // OpenGL ES 1.1 no tiene gluPerspective, usamos función propia
    if (orthographic) {
        float size = 90.0f;
        glOrtho(-size * aspect, size * aspect,
                -size, size,
                nearClip, farClip);
    } 
    else {
        gluPerspectivef(fovDeg, aspect, nearClip, farClip);
    }
#else
    // OpenGL desktop sí tiene gluPerspective
    if (orthographic) {
        float size = 90.0f;
        glOrtho(-size * aspect, size * aspect,
                -size, size,
                nearClip, farClip);
    } 
    else {
        gluPerspective(fovDeg, aspect, nearClip, farClip);
    }
#endif

    // Matriz de modelo
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Limpiar pantalla
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);

    if (view == RenderType::ZBuffer) {
        glEnable(GL_FOG);
        glFogf(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, nearClip);
        glFogf(GL_FOG_END, farClip);
        GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        glFogfv(GL_FOG_COLOR, fogColor);
    } else {
        glDisable(GL_FOG);
        glClearColor(ListaColores[static_cast<int>(ColorID::background)][0], ListaColores[static_cast<int>(ColorID::background)][1],
                     ListaColores[static_cast<int>(ColorID::background)][2], ListaColores[static_cast<int>(ColorID::background)][3]);
    }

    if (scene->limpiarPantalla) {
        if (view == RenderType::ZBuffer) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        } else if (view == RenderType::Rendered) {
            glClearColor(scene->backgroundColor[0], scene->backgroundColor[1],
                         scene->backgroundColor[2], scene->backgroundColor[3]);
        } else {
            glClearColor(ListaColores[static_cast<int>(ColorID::background)][0], ListaColores[static_cast<int>(ColorID::background)][1],
                         ListaColores[static_cast<int>(ColorID::background)][2], ListaColores[static_cast<int>(ColorID::background)][3]);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(x, y, width, height);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_COLOR_MATERIAL);

    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    if (ViewFromCameraActive) {
        RecalcViewPos();
    }

    if (view == RenderType::Solid || view == RenderType::MaterialPreview) {
        glLightfv(GL_LIGHT0, GL_POSITION, MaterialPreviewPosition);
    }

    glTranslatef(posX, posZ, -cameraDistance + posY);

    glRotatef(rotY, 1, 0, 0);
    glRotatef(rotX, 0, 1, 0);
    glTranslatef(PivotX, PivotZ, PivotY);

    // Dibujar overlays
    if (showOverlays) {
#ifdef ANDROID
        glMaterialxv(GL_FRONT_AND_BACK, GL_DIFFUSE,  ListaColoresX[negro]);
        glMaterialxv(GL_FRONT_AND_BACK, GL_AMBIENT,  ListaColoresX[negro]);
        glMaterialxv(GL_FRONT_AND_BACK, GL_SPECULAR, ListaColoresX[negro]);
#else
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  ListaColores[static_cast<int>(ColorID::negro)]);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  ListaColores[static_cast<int>(ColorID::negro)]);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, ListaColores[static_cast<int>(ColorID::negro)]);
#endif

        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
        glDisable(GL_BLEND);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_TEXTURE_2D);

        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);

        if (showFloor || showXaxis || showYaxis) RenderFloor();
    }

    glEnable(GL_DEPTH_TEST);

    // Renderiza la escena recursivamente
    SceneCollection->Render();

    if (showOverlays) RenderOverlay();
    if (ShowUi) RenderUI();
}

void Viewport3D::RenderFloor() {
    glEnable(GL_FOG);
    glFogf(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, nearClip);
    glFogf(GL_FOG_END, 25.0f);

    if (view == RenderType::Rendered) {
        glFogfv(GL_FOG_COLOR, scene->backgroundColor); 
    } else {
#ifdef ANDROID
        glFogxv(GL_FOG_COLOR, ListaColoresX[background]);
#else
        glFogfv(GL_FOG_COLOR, ListaColores[static_cast<int>(ColorID::background)]);
#endif
    }

    glLineWidth(1);	 
    glVertexPointer(3, GL_FLOAT, 0, objVertexdataFloor);

    glColor4f(
        ListaColores[static_cast<int>(ColorID::LineaPiso)][0],
        ListaColores[static_cast<int>(ColorID::LineaPiso)][1],
        ListaColores[static_cast<int>(ColorID::LineaPiso)][2],
        ListaColores[static_cast<int>(ColorID::LineaPiso)][3]
    );
    glDrawElements(GL_LINES, objFacesFloor, GL_UNSIGNED_SHORT, objFacedataFloor);

    // Linea Roja
    if (showXaxis) {
        glLineWidth(2);
        glColor4f(
            ListaColores[static_cast<int>(ColorID::LineaPisoRoja)][0],
            ListaColores[static_cast<int>(ColorID::LineaPisoRoja)][1],
            ListaColores[static_cast<int>(ColorID::LineaPisoRoja)][2],
            ListaColores[static_cast<int>(ColorID::LineaPisoRoja)][3]
        );
        glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo);
        glLineWidth(1);
    } else if (showFloor) {
        glColor4f(
            ListaColores[static_cast<int>(ColorID::LineaPiso)][0],
            ListaColores[static_cast<int>(ColorID::LineaPiso)][1],
            ListaColores[static_cast<int>(ColorID::LineaPiso)][2],
            ListaColores[static_cast<int>(ColorID::LineaPiso)][3]
        );
        glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo);
    }

    // Linea Verde
    if (showYaxis) {
        glLineWidth(2);
        glColor4f(
            ListaColores[static_cast<int>(ColorID::LineaPisoVerde)][0],
            ListaColores[static_cast<int>(ColorID::LineaPisoVerde)][1],
            ListaColores[static_cast<int>(ColorID::LineaPisoVerde)][2],
            ListaColores[static_cast<int>(ColorID::LineaPisoVerde)][3]
        );
        glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde);
        glLineWidth(1);
    } else if (showFloor) {
        glColor4f(
            ListaColores[static_cast<int>(ColorID::LineaPiso)][0],
            ListaColores[static_cast<int>(ColorID::LineaPiso)][1],
            ListaColores[static_cast<int>(ColorID::LineaPiso)][2],
            ListaColores[static_cast<int>(ColorID::LineaPiso)][3]
        );
        glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde);
    }

    glDisable(GL_FOG);
}

void Viewport3D::RenderAllAxisTransform() { 
    glVertexPointer(3, GL_FLOAT, 0, objVertexdataFloor);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(3);

    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        if (RenderAxisTransform(SceneCollection->Childrens[c])) break;
    }
}

void Viewport3D::RenderOverlay() {
#ifdef ANDROID
    glMaterialxv(GL_FRONT_AND_BACK, GL_DIFFUSE,  ListaColoresX[negro]);
    glMaterialxv(GL_FRONT_AND_BACK, GL_AMBIENT,  ListaColoresX[negro]);
    glMaterialxv(GL_FRONT_AND_BACK, GL_SPECULAR, ListaColoresX[negro]);
#else
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  ListaColores[static_cast<int>(ColorID::negro)]);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  ListaColores[static_cast<int>(ColorID::negro)]);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, ListaColores[static_cast<int>(ColorID::negro)]);
#endif

    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glDisableClientState(GL_COLOR_ARRAY);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    if (!SceneCollection->Childrens.empty()) {
        glLineWidth(1);

        if (ShowRelantionshipsLines) RenderRelantionshipsLines();
        RenderIcons3D();

        glDisable(GL_DEPTH_TEST);

        if (showOrigins) RenderOrigins();

        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);

        if (!SceneCollection->Childrens.empty() && 
            (estado == translacion || estado == rotacion || estado == EditScale))
            RenderAllAxisTransform();
    }

    if (show3DCursor) Render3Dcursor();
}

void Viewport3D::RenderRelantionshipsLines() {
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glTexCoordPointer(2, GL_FLOAT, 0, lineUV);
    glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1], 
              ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    glBindTexture(GL_TEXTURE_2D, Textures[3]->iID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    RenderLinkLines(SceneCollection);

    glDepthMask(GL_TRUE);
}

void Viewport3D::Render3Dcursor() {
    glDisable(GL_DEPTH_TEST);
    glPushMatrix();
    glTranslatef(Cursor3DposX, Cursor3DposZ, Cursor3DposY);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glEnable(GL_POINT_SPRITE);
    glPointSize(32);
    glVertexPointer(3, GL_SHORT, 0, pointVertex);
    glBindTexture(GL_TEXTURE_2D, Textures[2]->iID);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1], 
              ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
    glDrawArrays(GL_POINTS, 0, 1);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_POINT_SPRITE);
    glDisable(GL_BLEND);

    glVertexPointer(3, GL_FLOAT, 0, Cursor3DVertices);
    glLineWidth(2);
    glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0], ListaColores[static_cast<int>(ColorID::grisUI)][1], 
              ListaColores[static_cast<int>(ColorID::grisUI)][2], ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    glDrawElements(GL_LINES, Cursor3DEdgesSize, GL_UNSIGNED_SHORT, Cursor3DEdges);

    glPopMatrix();
}

void Viewport3D::RenderUI() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glViewport(x, y, width, height);
    glOrtho(0, width, height, 0, -1, 1);

    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glDisableClientState(GL_COLOR_ARRAY);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, Textures[0]->iID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    DibujarBordes(this);
}

void Viewport3D::EnfocarObject() {
    if (!ObjSelects.empty()) {
        SetTransformPivotPoint();
        PivotX = 0.0f - TransformPivotPointFloat[0];
        PivotY = 0.0f - TransformPivotPointFloat[1];
        PivotZ = 0.0f - TransformPivotPointFloat[2];
    }
}

void Viewport3D::RecalcViewPos() {
    Object& obj = *CameraActive;
    rotX = -obj.rotZ + 90;
    rotY = -obj.rotY;
    PivotX = -obj.posX;
    PivotY = -obj.posY;
    PivotZ = -obj.posZ;
}

void Viewport3D::SetViewpoint(int opcion) {
    switch (opcion) {
        case top:
            rotX = -180.0;
            rotY = 90.0;
            ViewFromCameraActive = false;
            CameraToView = false;
            break;
        case front:
            rotX = -180.0;
            rotY = 0.0;
            ViewFromCameraActive = false;
            CameraToView = false;
            break;
        case right:
            rotX = 90.0;
            rotY = 0.0;
            ViewFromCameraActive = false;
            CameraToView = false;
            break;
        case cameraView:
            if (CameraActive && !ViewFromCameraActive) {
                LastRotX = rotX;
                LastRotY = rotY;
                LastPivotX = PivotX;
                LastPivotY = PivotY;
                LastPivotZ = PivotZ;
                RecalcViewPos();
                ViewFromCameraActive = true;
            }
            break;
    }
}

void Viewport3D::RestaurarViewport(){
    ViewFromCameraActive = false;
    rotX = LastRotX;
    rotY = LastRotY;	
    PivotX = LastPivotX;
    PivotY = LastPivotY;
    PivotZ = LastPivotZ;
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
    Vec3 camPos = pivotPos - forward * cameraDistance;

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
            estado = editNavegacion;
        }
    }
    ViewPortClickDown = false;
}

void Viewport3D::button_left(){
    if (estado == translacion || estado == EditScale || estado == rotacion){
        Aceptar();
    }
    else {
        GuardarMousePos();
        SetCursor3D();
    }
}

void Viewport3D::mouse_button_up(SDL_Event &e){

}

void Viewport3D::event_mouse_wheel(SDL_Event &e){
    posY+= e.wheel.y;
}

void Viewport3D::event_mouse_motion(int mx, int my){
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
            float radY = rotY * M_PI / 180.0f; // Yaw
            float radX = rotX * M_PI / 180.0f; // Pitch

            float factor = 0.01f;

            float cosX = cos(radX);
            float sinX = sin(radX);
            float cosY = cos(radY);
            float sinY = sin(radY);

            PivotZ -= dy * factor * cosY;
            PivotX += dx * factor * cosX - dy * factor * sinY * sinX;
            PivotY += dx * factor * sinX + dy * factor * sinY * cosX;
            LShiftPressed = false;
        } 
        else {
            // 🚀 ROTAR cámara
            rotX += dx * 0.2f;  
            rotY += dy * 0.2f;  

            // Limitar rotY para evitar giros extremos
            if(rotY > 180.0f) rotY -= 360.0f;
            if(rotY < -180.0f) rotY += 360.0f;
            if(rotX > 180.0f) rotX -= 360.0f;
            if(rotX < -180.0f) rotX += 360.0f;
            UpdatePrecalculos();
        }
    }
    else if (estado == translacion || estado == rotacion || estado == EditScale){
        // Ocultar el cursor
        //SDL_HideCursor();
        switch (estado) {
            case translacion:
                SetTranslacionObjetos(dx, dy, 0.01f);
                break;
            case rotacion:
                SetRotacion(dx, dy);
                break;
            case EditScale:
                SetScale(dx, dy);
                break;
            default:
                // por si no coincide con nada
                break;
        }
    }
}

void Viewport3D::TeclaDerecha(){
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
                GLfloat radRotX = obj.rotX * M_PI / 180.0;

                obj.posX-= 30 * cos(radRotX);
                obj.posY+= 30 * sin(radRotX);
            }
            else {
                if (ViewFromCameraActive){
                    RestaurarViewport();
                }
                rotX+= 0.5;	
            }		
        }
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            GLfloat radRotX = rotX * M_PI / 180.0;

            // Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
            GLfloat leftX = cos(radRotX);
            GLfloat leftY = sin(radRotX);

            // Mover hacia la izquierda
            PivotX -= 30 * leftX;
            PivotY -= 30 * leftY;
        }
    }
    else if (estado == translacion){
        SetTranslacionObjetos(30, 0);		
    }
    else if (estado == rotacion){
        SetRotacion(-1, 0);
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

void Viewport3D::TeclaIzquierda(){
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
                GLfloat radRotX = obj.rotX * M_PI / 180.0;

                obj.posX+= 30 * cos(radRotX);
                obj.posY-= 30 * sin(radRotX);
            }
            else {
                if (ViewFromCameraActive){
                    RestaurarViewport();
                }
                rotX-= 0.5;
            }
        }
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            GLfloat radRotX = rotX * M_PI / 180.0;

            // Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
            GLfloat leftX = cos(radRotX);
            GLfloat leftY = sin(radRotX);

            // Mover hacia la izquierda
            PivotX += 30 * leftX;
            PivotY += 30 * leftY;
        }	
    }
    else if (estado == translacion){	
        SetTranslacionObjetos(-30, 0);		
    }
    else if (estado == rotacion){
        SetRotacion(1, 0);
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

void Viewport3D::TeclaArriba(){
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
                GLfloat radRotX = obj.rotX * M_PI / 180.0;
                GLfloat radRotY = obj.rotY * M_PI / 180.0;
                //GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

                obj.posX+= 30 * sin(radRotX);
                //obj.posY-= 30 * cos(radRotX);
                obj.posZ+= 30 * cos(radRotY);
            }
            else {
                if (ViewFromCameraActive){
                    RestaurarViewport();
                }
                rotY-= 0.5;	
            }			
        }
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            GLfloat radRotX = rotX * M_PI / 180.0;

            PivotY+= 30 * cos(radRotX);
            PivotX-= 30 * sin(radRotX);
        }		
    }
    else if (estado == EditScale){
        SetScale(2,0);	
    }
    else if (estado == translacion){
        SetTranslacionObjetos(0, -30);
    }
}

void Viewport3D::TeclaAbajo(){
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
                GLfloat radRotX = obj.rotX * M_PI / 180.0;
                GLfloat radRotY = obj.rotY * M_PI / 180.0;
                //GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

                obj.posX-= 30 * sin(radRotX);
                //obj.posY-= 30 * cos(radRotX);
                obj.posZ-= 30 * cos(radRotY);
            }
            else {
                if (ViewFromCameraActive){
                    RestaurarViewport();
                }
                rotY+= 0.5;	
            }		
        }
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            GLfloat radRotX = rotX * M_PI / 180.0;

            PivotY-= 30 * cos(radRotX);
            PivotX+= 30 * sin(radRotX);
        }
    }
    else if (estado == EditScale){
        SetScale(-2,0);	
    }
    else if (estado == translacion){
        SetTranslacionObjetos(0, 30);		
    }
}

// Método para actualizar cache
void Viewport3D::UpdatePrecalculos() {
    precalculado.radY = rotY * M_PI / 180.0f;
    precalculado.radX = rotX * M_PI / 180.0f;

    precalculado.cosX = cos(precalculado.radX);
    precalculado.sinX = sin(precalculado.radX);
    precalculado.cosY = cos(precalculado.radY);
    precalculado.sinY = sin(precalculado.radY);
}

void Viewport3D::ClickD(){
    if (LAltPressed){
        UpdatePrecalculos();
        NewInstance();
    }
    else if (LShiftPressed){
        UpdatePrecalculos();
        DuplicatedObject();
    }
}

void Viewport3D::ClickE(){
    posZ-= 1.0;
}

void Viewport3D::ClickQ(){
    posZ+= 1.0;  
}

void Viewport3D::SetEje(int eje){
    if (estado != editNavegacion){
        ReestablecerEstado(false);
        axisSelect = eje;
    }	
}

void Viewport3D::event_key_down(SDL_Event &e){
    #if SDL_MAJOR_VERSION == 2
        SDL_Keycode key = e.key.keysym.sym; //SDL2            
    #elif SDL_MAJOR_VERSION == 3
        SDL_Keycode key = e.key.key; // SDL3
    #endif
    if (e.key.repeat == 0) { 
        switch (key) {
            case SDLK_LSHIFT:
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
                SeleccionarTodo();
                break;
            case SDLK_D:
                ClickD();
                break;
            case SDLK_U:
                scene->SetLimpiarPantalla();
                break;
            case SDLK_J:
                ChangeViewType();
                break;    
            case SDLK_H:
                ChangeVisibilityObj();
                break;    
            case SDLK_K:          
                SetShowOverlays(!showOverlays);
                break;          
            case SDLK_X:   
                if (estado != editNavegacion){
                    if (axisSelect != X){
                        SetEje(X);
                    }
                    else {
                        SetEje(ViewAxis);
                    }
                } 
                else {
                    Eliminar();
                }
                break;
            case SDLK_Y:   
                if (estado != editNavegacion){
                    if (axisSelect != Y){
                        SetEje(Y);
                    }
                    else {
                        SetEje(ViewAxis);
                    }
                }
                break;
            case SDLK_Z:   
                if (estado != editNavegacion){
                    if (axisSelect != Z){
                        SetEje(Z);
                    }
                    else {
                        SetEje(ViewAxis);
                    }
                }
                break;
            case SDLK_R:    
                SetRotacion();
                break;
            case SDLK_G:  
                // Para activar el cursor de mover/arrastrar
                UpdatePrecalculos();
                SetPosicion();
                break;			
            case SDLK_S:   
                SetEscala();
                break;
            // Numpad
            case SDLK_KP_1: SetViewpoint(front); break;
            //case SDLK_KP_2: numpad('2'); break;
            case SDLK_KP_3: SetViewpoint(right); break;
            case SDLK_KP_5: {
                ChangePerspective(); 
                break;
            };
            case SDLK_KP_7: SetViewpoint(top); break;
            case SDLK_KP_8: BuscarVertexAnimation(); break;
            case SDLK_KP_9: abrir(); break;
            //case SDLK_KP_0: numpad('0'); break;
            case SDLK_KP_PERIOD: {
                EnfocarObject(); 
                break;
            }
            // si querés, agregá más teclas aquí
            case SDLK_ESCAPE:  // Esc                    
                Cancelar();
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
            case SDLK_UP:   // Flecha derecha
                TeclaArriba();
                break;
            case SDLK_DOWN:    // Flecha izquierda
                TeclaAbajo();
                break;
            case SDLK_A:   // Flecha derecha
                SeleccionarTodo();
                break;
            case SDLK_E:   // Flecha derecha
                ClickE();
                break;
            case SDLK_Q:    // Flecha izquierda
                ClickQ();
                break;
            // Numpad
            case SDLK_KP_1: {
                SetViewpoint(front);
                break;
            }
            //case SDLK_KP_2: numpad('2'); break;
            case SDLK_KP_3: {
                SetViewpoint(right);
                break;
            }
            case SDLK_KP_7: {
                SetViewpoint(top);
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
                Cancelar();
                break;
        }
    }
}

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
            ShiftCount = 0;
            LShiftPressed = false;
            break;
        case SDLK_LALT:
            LAltPressed = false;
            break;
    }
}

void Viewport3D::key_down_return(){
    Aceptar();
}

Viewport3D* Viewport3DActive = nullptr;

//precalculos
bool recalcularCamara = true;
GLfloat radY = 0.0f;
GLfloat radX = 0.0f;

GLfloat factor = 0.01f;

GLfloat cosX = 0.0f;
GLfloat sinX = 0.0f;
GLfloat cosY = 0.0f;
GLfloat sinY = 0.0f;

void Gamepad::Update(){
    if (!target || !Viewport3DActive) return;

    //Viewport3DActive->rotX   += axisState[SDL_CONTROLLER_AXIS_RIGHTX] * 1.0f;
    //Viewport3DActive->rotY   += axisState[SDL_CONTROLLER_AXIS_RIGHTY] * 1.0f;

    target->rotX += axisState[SDL_CONTROLLER_AXIS_RIGHTX] * 0.3f;
    target->rotZ += axisState[SDL_CONTROLLER_AXIS_RIGHTY] * 0.3f;

    // Limitar rotY para evitar giros extremos
    if(Viewport3DActive->rotY > 180.0f) Viewport3DActive->rotY -= 360.0f;
    if(Viewport3DActive->rotY < -180.0f) Viewport3DActive->rotY += 360.0f;
    if(Viewport3DActive->rotX > 180.0f) Viewport3DActive->rotX -= 360.0f;
    if(Viewport3DActive->rotX < -180.0f) Viewport3DActive->rotX += 360.0f;

    if (recalcularCamara || axisState[SDL_CONTROLLER_AXIS_RIGHTX] != 0.0f || axisState[SDL_CONTROLLER_AXIS_RIGHTY] != 0.0f ){
        //precalculos
        radY = Viewport3DActive->rotY * M_PI / 180.0f; // Yaw
        radX = Viewport3DActive->rotX * M_PI / 180.0f; // Pitch

        cosX = cos(radX);
        sinX = sin(radX);
        cosY = cos(radY);
        sinY = sin(radY);
        recalcularCamara = false;
    }

    // Movimiento cámara según sticks y gatillos
    /*Viewport3DActive->PivotZ += axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * cosY;
    Viewport3DActive->PivotX -= axisState[SDL_CONTROLLER_AXIS_LEFTX] * factor * cosX - axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * sinY * sinX;
    Viewport3DActive->PivotY += axisState[SDL_CONTROLLER_AXIS_LEFTX] * factor * sinX + axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * sinY * cosX;*/

    target->posZ += axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * cosY;
    target->posX -= axisState[SDL_CONTROLLER_AXIS_LEFTX] * factor * cosX - axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * sinY * sinX;
    target->posY += axisState[SDL_CONTROLLER_AXIS_LEFTX] * factor * sinX + axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * sinY * cosX;

    //std::cout << "PivotX: " << PivotX << " PivotY: " << PivotY << " PivotZ: " << PivotZ << std::endl;
    //std::cout << "rotY: " << rotY << std::endl;

    //Viewport3DActive->posY += (axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] - axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]) * 0.1f;
    target->scaleX += (axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] - axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]) * 0.02f;
    target->scaleY += (axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] - axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]) * 0.02f;
    target->scaleZ += (axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] - axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]) * 0.02f;

    //target->posY += axisState[SDL_CONTROLLER_AXIS_LEFTX] * velocidad;
    //target->posX += axisState[SDL_CONTROLLER_AXIS_LEFTY] * velocidad; 
}