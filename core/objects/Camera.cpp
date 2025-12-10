#include "Camera.h"

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

const GLshort CameraFaceActive[3] = {5, 6, 7};

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

Object* CameraActive = nullptr;

// ------------------- MÉTODOS -------------------

Camera::Camera(Object* parent, Vector3 pos, GLfloat rx, GLfloat ry, GLfloat rz, GLfloat rw)
    : Object(parent, "Camera", pos) 
{
    rot.x = rx; rot.y = ry; rot.z = rz; rot.w = rw;
    IconType = static_cast<size_t>(IconType::camera);

    if (!CameraActive){
        CameraActive = this;
    }
}

ObjectType Camera::getType() {
    return ObjectType::camera;
}

void Camera::RenderObject() {
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;

    glPushMatrix();
    //glScalef(scale.x, scale.z, scale.y);

    if (ObjActivo == this && select){
        glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0],
                  ListaColores[static_cast<int>(ColorID::accent)][1],
                  ListaColores[static_cast<int>(ColorID::accent)][2],
                  ListaColores[static_cast<int>(ColorID::accent)][3]);
    }
    else if (select){
        glColor4f(ListaColores[static_cast<int>(ColorID::accentDark)][0],
                  ListaColores[static_cast<int>(ColorID::accentDark)][1],
                  ListaColores[static_cast<int>(ColorID::accentDark)][2],
                  ListaColores[static_cast<int>(ColorID::accentDark)][3]);
    }
    else {
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0],
                  ListaColores[static_cast<int>(ColorID::grisUI)][1],
                  ListaColores[static_cast<int>(ColorID::grisUI)][2],
                  ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D); 
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glLineWidth(1);
    glDisableClientState(GL_NORMAL_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, CameraVertices);
    glDrawElements(GL_LINES, CameraEdgesSize, GL_UNSIGNED_SHORT, CameraEdges);

    if (CameraActive == this){		
        glDisable(GL_CULL_FACE);	
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, CameraFaceActive);	
    }

    glPopMatrix();
}

Camera::~Camera() {
    delete name;
}