#include "VertexAnimation.h"

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

// Guarda SOLO posiciones de vértices para un frame
struct VertexFrame {
    // puntero a array de floats [x,y,z,x,y,z,...]
    const float* positions = nullptr;
};

class VertexAnimation {
    public:
        // Malla objetivo donde se escribe el resultado final
        Mesh* target = nullptr;

        // Frames de la animación (solo posiciones)
        std::vector<VertexFrame*> frames;

        VertexAnimation() = default;
        VertexAnimation(Mesh* tgt);

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

VertexAnimation::VertexAnimation(Mesh* tgt, size_t vcount)
: target(tgt), vertexCount(vcount) {}