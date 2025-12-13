#include "VertexAnimation.h"

VertexAnimation::VertexAnimation(Mesh* tgt, const std::string& animName): target(tgt), name(animName) {}

void VertexAnimation::AddFrame(VertexFrame* frame) {
    frames.push_back(frame);
}

bool VertexAnimation::LoadFrames() {
    if (!target || !target->vertex || target->vertexSize <= 0)
        return false;

    frames.clear();

    for (int i = 1; i <= frameCount; ++i) {

        std::ostringstream path;
        path << basePath
             << std::setw(padding)
             << std::setfill('0')
             << i
             << ".obj";

        std::ifstream file(path.str());
        if (!file.is_open()) {
            std::cerr << "[Anim] No se pudo abrir " << path.str() << "\n";
            return false;
        }

        std::vector<GLfloat> verts;
        std::string line;

        while (std::getline(file, line)) {
            if (line.rfind("v ", 0) == 0) {
                GLfloat x, y, z;
                std::sscanf(line.c_str(), "v %f %f %f", &x, &y, &z);
                verts.push_back(x);
                verts.push_back(y);
                verts.push_back(z);
                //std::cerr << "["<< name <<"] x: "<< x <<" y: "<< y <<" z: "<< z << "\n";
            }
        }

        // chequeo CLAVE
        if ((int)verts.size() != target->vertexSize) {
            std::cerr << "[Anim] Vertex count mismatch en "
                      << path.str() << "\n";
            return false;
        }

        //std::cerr << "["<< name <<"] Vertex count " << (verts.size()/3) << "\n";

        VertexFrame* f = new VertexFrame;
        f->positions = new float[verts.size()];
        std::memcpy((void*)f->positions, verts.data(),
                    verts.size() * sizeof(float));

        frames.push_back(f);
    }

    return true;
}

void ApplyVertexFrame(const VertexAnimation& anim, size_t frameIndex) {
    assert(anim.target);
    if(frameIndex >= anim.frames.size()){
        frameIndex = anim.frames.size()-1;
    }

    //std::cout << "ApplyVertexFrame "<< anim.name << " frameIndex: " << (frameIndex+1) << "/" << anim.frames.size() << "\n";

    Mesh* mesh = anim.target;
    const float* src = anim.frames[frameIndex]->positions;

    //std::cout << "Vertex Mesh: "<< (mesh->vertexSize) << "\n";

    for (size_t i = 0; i < mesh->vertexSize; ++i) {
        mesh->vertex[i] = src[i];
    }
}

void BlendVertexAnimations(
    const VertexAnimation& fromAnim,
    const VertexAnimation& toAnim,
    size_t fromFrame,
    size_t toFrame,
    float blendT,
    Mesh* mesh
) {
    assert(fromAnim.target);
    assert(fromAnim.target == toAnim.target);
    assert(fromFrame < fromAnim.frames.size());
    assert(toFrame < toAnim.frames.size());
    assert(fromAnim.vertexCount == toAnim.vertexCount);

    // Clamp
    if (blendT <= 0.0f) {
        ApplyVertexFrame(fromAnim, fromFrame);
        return;
    }

    if (blendT >= 1.0f) {
        ApplyVertexFrame(toAnim, toFrame);
        return;
    }

    const float* A = fromAnim.frames[fromFrame]->positions;
    const float* B = toAnim.frames[toFrame]->positions;

    // Mezcla lineal por vértice
    for (size_t i = 0; i < mesh->vertexSize; ++i) {
        mesh->vertex[i] = A[i] * (1.0f - blendT) + B[i] * blendT;
    }
}