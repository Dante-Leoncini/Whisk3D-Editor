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

// Guarda SOLO posiciones de vértices para un frame
struct VertexFrame {
    // puntero a array de floats [x,y,z,x,y,z,...]
    const GLfloat* positions = nullptr;
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

        // Frames de la animación (solo posiciones)
        std::vector<VertexFrame*> frames;

        VertexAnimation() = default;
        VertexAnimation(Mesh* tgt, const std::string& animName);

        void AddFrame(VertexFrame* frame);
    
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
    float blendT
);

// Copia directa de un frame a la malla target (sin mezcla)
void ApplyVertexFrame(
    const VertexAnimation& anim,
    size_t frameIndex
);

#endif