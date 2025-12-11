#include "ViewPorts/ViewPort3D.h"
#include <cmath>

Viewport3D::Viewport3D(Vector3 pos){
    RecalcOrbitPosition();
}

Viewport3D::~Viewport3D() {};

void Viewport3D::event_mouse_wheel(SDL_Event &e) {
    if (!ViewFromCameraActive) {
        Zoom(e.wheel.y* 2.0f); //podria multiplciarse por un valor por sensibilidad  * 1.0f
    }
}

void Viewport3D::EnfocarObject() {
    if (!ObjSelects.empty()) {
        SetTransformPivotPoint();
        pivot = TransformPivotPoint;
        RecalcOrbitPosition();
    }
}

void Viewport3D::Zoom(float delta){
    orbitDistance -= delta * 0.5f;

    if (orbitDistance < 0.1f)
        orbitDistance = 0.1f;

    RecalcOrbitPosition();
}

void Viewport3D::UpdateViewOrbit() {
    Matrix4 view;

    if(ViewFromCameraActive && CameraActive) {
        // Tomar la rotación y posición de la cámara
        Quaternion camRotInv = CameraActive->rot.Inverted();
        Matrix4 R = camRotInv.ToMatrix();

        Matrix4 T;
        T.Identity();
        T.m[12] = -CameraActive->pos.x;
        T.m[13] = -CameraActive->pos.y;
        T.m[14] = -CameraActive->pos.z;

        view = R * T;
    } 
    else {
        // La View Matrix es la inversa de la Transformación de Cámara.
        // Para rotación pura, la inversa es el conjugado.
        Quaternion viewRotInverse = viewRot.Inverted(); 
        Matrix4 R = viewRotInverse.ToMatrix();

        Matrix4 T;
        T.Identity();
        // Movemos el mundo en dirección opuesta a la cámara
        T.m[12] = -viewPos.x;
        T.m[13] = -viewPos.y;
        T.m[14] = -viewPos.z;

        // Orden estándar ViewMatrix: Rotacion * Traslacion
        view = R * T;
    }

    glLoadMatrixf(view.m);
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

void Viewport3D::Pan(){
    const float speed = orbitDistance * 0.002f;

    // mover en el plano de la cámara
    Vector3 right = viewRot * Vector3(1,0,0);
    Vector3 up    = viewRot * Vector3(0,1,0);

    pivot = pivot - right * (dx * speed);
    pivot = pivot + up    * (dy * speed);

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
    ::rotGlobal = viewRot;
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

    if (orthographic) {
        float size = 5.0f;
        glOrtho(-size * aspect, size * aspect,
                -size, size,
                nearClip, farClip);
    } 
    else {
        #ifdef ANDROID
            // OpenGL ES 1.1 no tiene gluPerspective, usamos función propia
            gluPerspectivef(fovDeg, aspect, nearClip, farClip);
        #else
            // OpenGL desktop sí tiene gluPerspective
            gluPerspective(fovDeg, aspect, nearClip, farClip);
        #endif
    }

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

    if (view == RenderType::Solid || view == RenderType::MaterialPreview) {
        glLightfv(GL_LIGHT0, GL_POSITION, MaterialPreviewPosition);
    }

    UpdateViewOrbit();

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
    glTranslatef(cursor3D.pos.x, cursor3D.pos.z, cursor3D.pos.y);

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
                SetScale(dx, dy, 0.001f);
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
    else if (estado == translacion){
        SetTranslacionObjetos(0, 30);		
    }
}

void Viewport3D::SetEje(int eje){
    if (estado != editNavegacion){
        ReestablecerEstado(false);
        axisSelect = eje;
    }	
}

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
            case SDLK_D: {
                if (LAltPressed){
                    NewInstance();
                }
                else if (LShiftPressed){
                    DuplicatedObject();
                }
                break;
            }
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
                SetPosicion();
                break;			
            case SDLK_S:   
                SetEscala();
                break;
            // Numpad
            case SDLK_KP_1: SetViewpoint(Viewpoint::front); break;
            //case SDLK_KP_2: numpad('2'); break;
            case SDLK_KP_3: SetViewpoint(Viewpoint::right); break;
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
            case SDLK_KP_7: SetViewpoint(Viewpoint::top); break;
            case SDLK_KP_8: BuscarVertexAnimation(); break;
            case SDLK_KP_9: abrir(); break;
            case SDLK_KP_0: SetViewFromCameraActive(!ViewFromCameraActive); break;
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
            // Numpad
            case SDLK_KP_1: {
                SetViewpoint(Viewpoint::front);
                break;
            }
            //case SDLK_KP_2: numpad('2'); break;
            case SDLK_KP_3: {
                SetViewpoint(Viewpoint::right);
                break;
            }      
            case SDLK_KP_7: {
                SetViewpoint(Viewpoint::top);
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

    /*target->rotX += axisState[SDL_CONTROLLER_AXIS_RIGHTX] * 0.3f;
    target->rotZ += axisState[SDL_CONTROLLER_AXIS_RIGHTY] * 0.3f;*/

    // Limitar rotY para evitar giros extremos
    /*if(Viewport3DActive->rotY > 180.0f) Viewport3DActive->rotY -= 360.0f;
    if(Viewport3DActive->rotY < -180.0f) Viewport3DActive->rotY += 360.0f;
    if(Viewport3DActive->rotX > 180.0f) Viewport3DActive->rotX -= 360.0f;
    if(Viewport3DActive->rotX < -180.0f) Viewport3DActive->rotX += 360.0f;*/

    if (recalcularCamara || axisState[SDL_CONTROLLER_AXIS_RIGHTX] != 0.0f || axisState[SDL_CONTROLLER_AXIS_RIGHTY] != 0.0f ){
        //precalculos
        /*radY = Viewport3DActive->rotY * M_PI / 180.0f; // Yaw
        radX = Viewport3DActive->rotX * M_PI / 180.0f; // Pitch*/

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

    target->pos.z += axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * cosY;
    target->pos.x -= axisState[SDL_CONTROLLER_AXIS_LEFTX] * factor * cosX - axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * sinY * sinX;
    target->pos.y += axisState[SDL_CONTROLLER_AXIS_LEFTX] * factor * sinX + axisState[SDL_CONTROLLER_AXIS_LEFTY] * factor * sinY * cosX;

    //std::cout << "PivotX: " << PivotX << " PivotY: " << PivotY << " PivotZ: " << PivotZ << std::endl;
    //std::cout << "rotY: " << rotY << std::endl;

    //Viewport3DActive->posY += (axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] - axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]) * 0.1f;
    /*target->scaleX += (axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] - axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]) * 0.02f;
    target->scaleY += (axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] - axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]) * 0.02f;
    target->scaleZ += (axisState[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] - axisState[SDL_CONTROLLER_AXIS_TRIGGERLEFT]) * 0.02f;*/

    //target->posY += axisState[SDL_CONTROLLER_AXIS_LEFTX] * velocidad;
    //target->posX += axisState[SDL_CONTROLLER_AXIS_LEFTY] * velocidad; 
}