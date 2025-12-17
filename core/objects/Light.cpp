#include "Light.h"
#include <cstdio>

// Inicialización de variables estáticas
std::vector<Light*> Lights;

// Constructor privado
Light::Light(Object* parent, GLfloat x, GLfloat y, GLfloat z)
: Object(parent, "Light", {x, y, z}), LightID(GL_LIGHT0) {
    IconType = static_cast<size_t>(IconType::light);

    position[0] = 0;
    position[1] = 0;
    position[2] = 0;
    position[3] = 1.0f;

    ambient[0] = ambient[1] = ambient[2] = 0.0f; ambient[3] = 1.0f;
    diffuse[0] = diffuse[1] = diffuse[2] = 1.0f; diffuse[3] = 1.0f;
    specular[0] = specular[1] = specular[2] = 0.0f; specular[3] = 1.0f;
}

// Método Create
Light* Light::Create(Object* parent, GLfloat x, GLfloat y, GLfloat z) {
    if (Lights.size() >= MAX_LIGHTS) {
        printf("WARNING: Máximo de luces alcanzado.\n");
        return nullptr;
    }

    Light* l = new Light(parent, x, y, z);
    Lights.push_back(l);
    return l;
}

// Destructor
Light::~Light() {
}

// getType
ObjectType Light::getType() {
    return ObjectType::light;
}

// SetDiffuse
void Light::SetDiffuse(GLfloat r, GLfloat g, GLfloat b) {
    diffuse[0] = r;
    diffuse[1] = g;
    diffuse[2] = b;
}

// SetLightID
void Light::SetLightID(GLenum ID) {
    LightID = ID;
}

// RenderObject
void Light::RenderObject() {
    if (view == RenderType::Rendered){
        glEnable(LightID);
        glLightfv(LightID, GL_POSITION, position);
        glLightfv(LightID, GL_DIFFUSE,  diffuse);
        glLightfv(LightID, GL_AMBIENT,  ambient);
        glLightfv(LightID, GL_SPECULAR, specular);

        // ACTIVAR ATENUACIÓN:
        glLightf(LightID, GL_CONSTANT_ATTENUATION, 0.5f);
        glLightf(LightID, GL_LINEAR_ATTENUATION,   0.1f);
        glLightf(LightID, GL_QUADRATIC_ATTENUATION, 0.02f);
    }

    if (!showOverlayGlobal) return;

    // Color según selección
    if (ObjActivo == this && select) {
        glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0],
                  ListaColores[static_cast<int>(ColorID::accent)][1],
                  ListaColores[static_cast<int>(ColorID::accent)][2],
                  ListaColores[static_cast<int>(ColorID::accent)][3]);
    } else if (select) {
        glColor4f(ListaColores[static_cast<int>(ColorID::accentDark)][0],
                  ListaColores[static_cast<int>(ColorID::accentDark)][1],
                  ListaColores[static_cast<int>(ColorID::accentDark)][2],
                  ListaColores[static_cast<int>(ColorID::accentDark)][3]);
    } else {
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0],
                  ListaColores[static_cast<int>(ColorID::grisUI)][1],
                  ListaColores[static_cast<int>(ColorID::grisUI)][2],
                  ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D); 
    glDisable(GL_BLEND);
    glLineWidth(1);

    // 1. Matriz global del objeto
    GetMatrix(M);

    // 2. Posición global de la lámpara
    Vector3 worldPos = M * Vector3(0, 0, 0);

    // 3. Guardarlo como vértice 1 de la línea
    //LineaLightVertex[4] = 0;
    //LineaLightVertex[5] = 0;
    LineaLightVertex[6] = -worldPos.z;

    glVertexPointer(3, GL_FLOAT, 0, LineaLightVertex);
    glDrawElements(GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge);
    //glPopMatrix();
}