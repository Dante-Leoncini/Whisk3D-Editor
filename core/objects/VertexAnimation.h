#ifndef VERTEXANIMATION_H
#define VERTEXANIMATION_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <GL/gl.h>
#include <GL/glext.h>

#include "UI/icons.h"
#include "Mesh.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>

#include "importers/import_obj.h"

static void ParseFace(const std::string& line, Face& f) {
    std::istringstream ss(line.substr(2));
    std::string tok;

    while (ss >> tok) {
        FaceCorner fc;

        int v = -1, t = -1, n = -1;

        if (tok.find("//") != std::string::npos) {
            sscanf(tok.c_str(), "%d//%d", &v, &n);
        } else {
            sscanf(tok.c_str(), "%d/%d/%d", &v, &t, &n);
        }

        fc.vertex = v - 1;
        fc.normal = n - 1;

        f.corner.push_back(fc);
    }
}

static GLbyte* BuildVertexNormals(
    size_t vertexCount,
    const std::vector<GLbyte>& tempNormals,
    const std::vector<Face>& faces
) {
    GLbyte* out = new GLbyte[vertexCount * 3];

    // default
    for (size_t i = 0; i < vertexCount * 3; i++)
        out[i] = 127;

    for (const Face& f : faces) {
        if (f.corner.size() < 3) continue;

        for (size_t i = 1; i < f.corner.size() - 1; i++) {
            const FaceCorner tri[3] = {
                f.corner[0],
                f.corner[i],
                f.corner[i + 1]
            };

            for (int k = 0; k < 3; k++) {
                const FaceCorner& fc = tri[k];
                if (fc.vertex < 0 || fc.normal < 0) continue;

                size_t v = fc.vertex * 3;
                size_t n = fc.normal * 3;

                out[v + 0] = tempNormals[n + 0];
                out[v + 1] = tempNormals[n + 1];
                out[v + 2] = tempNormals[n + 2];
            }
        }
    }

    return out;
}

// Guarda SOLO posiciones de vértices para un frame
struct VertexFrame {
    // puntero a array de floats [x,y,z,x,y,z,...]
    const GLfloat* positions = nullptr;
    const GLbyte* normals = nullptr;
};

class VertexAnimation {
    public:
        // Nombre de la animación
        std::string name;

        // Declaración (desde .w3d)
        std::string basePath;
        int frameCount = 0;
        int padding = 0;

        // Runtime
        Mesh* target = nullptr;

        bool UseNormals = false;

        // Frames de la animación (solo posiciones)
        std::vector<VertexFrame*> frames;

        VertexAnimation() = default;
        VertexAnimation(Mesh* tgt, const std::string& animName, bool useNormals = false);
    
        // Cargar animaciones desde archivos .obj
        bool LoadFrames();

        size_t FrameCount() const { return frames.size(); }
};

// Mezcla dos animaciones y escribe en la malla target
// - fromAnim: animación actual (ej: idle)
// - toAnim: animación destino (ej: run)
// - fromFrame: frame actual en fromAnim
// - blendT: 0..1 (0 = solo fromAnim, 1 = solo toAnim)
// - toFrame: frame en toAnim (usualmente 0 al comenzar la mezcla)
void BlendVertexAnimations(
    const VertexAnimation& fromAnim,
    const VertexAnimation& toAnim,
    size_t fromFrame,
    size_t toFrame,
    float blendT,
    Mesh* mesh
);

// Copia directa de un frame a la malla target (sin mezcla)
void ApplyVertexFrame(
    const VertexAnimation& anim,
    size_t frameIndex
);

#endif