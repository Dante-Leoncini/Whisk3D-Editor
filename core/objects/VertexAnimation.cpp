#include "VertexAnimation.h"

VertexAnimation::VertexAnimation(Mesh* tgt, const std::string& animName, bool useNormals, float Speed): 
      target(tgt), name(animName), UseNormals(useNormals), speed(Speed) {}

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
        std::vector<GLbyte>  tempNormals;
        std::vector<Face>    faces;

        std::string line;

        auto conv = [](double v) -> GLbyte {
            v = ((v + 1.0) * 0.5) * 255.0 - 128.0;
            if (v > 127) v = 127;
            if (v < -128) v = -128;
            return (GLbyte)v;
        };

        // ---------- PARSE OBJ ----------
        while (std::getline(file, line)) {

            if (line.rfind("v ", 0) == 0) {
                GLfloat x, y, z;
                sscanf(line.c_str(), "v %f %f %f", &x, &y, &z);
                verts.push_back(x);
                verts.push_back(y);
                verts.push_back(z);
            }
            else if (UseNormals && line.rfind("vn ", 0) == 0) {
                double nx, ny, nz;
                sscanf(line.c_str(), "vn %lf %lf %lf", &nx, &ny, &nz);

                tempNormals.push_back(conv(nx));
                tempNormals.push_back(conv(ny));
                tempNormals.push_back(conv(nz));
            }
            else if (UseNormals && line.rfind("f ", 0) == 0) {
                Face f;
                ParseFace(line, f);
                faces.push_back(f);
            }
        }

        // ---------- VALIDACIÓN ----------
        if ((int)verts.size() != target->vertexSize) {
            std::cerr << "[Anim] Vertex count mismatch en "
                      << path.str() << "\n";
            return false;
        }

        // ---------- CONSTRUIR NORMALES ----------
        GLbyte* finalNormals = nullptr;

        if (UseNormals) {
            size_t vcount = verts.size() / 3;
            finalNormals = BuildVertexNormals(vcount, tempNormals, faces);
        }

        // ---------- CREAR FRAME ----------
        VertexFrame* frame = new VertexFrame;

        GLfloat* pos = new GLfloat[verts.size()];
        std::memcpy(pos, verts.data(), verts.size() * sizeof(GLfloat));
        frame->positions = pos;

        frame->normals = finalNormals; // puede ser nullptr

        frames.push_back(frame);
    }

    return true;
}

void ApplyVertexFrame(const VertexAnimation& anim, size_t frameIndex) {
    assert(anim.target);

    if (frameIndex >= anim.frames.size()) {
        frameIndex = anim.frames.size() - 1;
    }

    Mesh* mesh = anim.target;
    const VertexFrame* frame = anim.frames[frameIndex];

    // ---------- posiciones ----------
    const GLfloat* srcPos = frame->positions;
    for (size_t i = 0; i < mesh->vertexSize; ++i) {
        mesh->vertex[i] = srcPos[i];
    }

    // ---------- normales (opcional) ----------
    if (anim.UseNormals && frame->normals && mesh->normals) {
        const GLbyte* srcNrm = frame->normals;
        for (size_t i = 0; i < mesh->vertexSize; ++i) {
            mesh->normals[i] = srcNrm[i];
        }
    }
}

static inline float NrmByteToFloat(GLbyte v) {
    return (float(v) + 128.0f) / 255.0f * 2.0f - 1.0f;
}

static inline GLbyte NrmFloatToByte(float v) {
    v = ((v + 1.0f) * 0.5f) * 255.0f - 128.0f;
    if (v > 127) v = 127;
    if (v < -128) v = -128;
    return (GLbyte)v;
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

    // Clamp rápido
    if (blendT <= 0.0f) {
        ApplyVertexFrame(fromAnim, fromFrame);
        return;
    }
    if (blendT >= 1.0f) {
        ApplyVertexFrame(toAnim, toFrame);
        return;
    }

    const VertexFrame* A = fromAnim.frames[fromFrame];
    const VertexFrame* B = toAnim.frames[toFrame];

    // ---------- posiciones ----------
    for (size_t i = 0; i < mesh->vertexSize; ++i) {
        mesh->vertex[i] =
            A->positions[i] * (1.0f - blendT) +
            B->positions[i] * blendT;
    }

    // ---------- normales (solo si ambas animaciones las usan) ----------
    if (fromAnim.UseNormals && toAnim.UseNormals &&
        A->normals && B->normals && mesh->normals) {

        for (size_t i = 0; i < mesh->vertexSize; ++i) {
            float na = NrmByteToFloat(A->normals[i]);
            float nb = NrmByteToFloat(B->normals[i]);

            float n = na * (1.0f - blendT) + nb * blendT;

            mesh->normals[i] = NrmFloatToByte(n);
        }
    }
}