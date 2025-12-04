#include "Animation.h"

// Variables globales
bool PlayAnimation = true;
int StartFrame = 1;
int EndFrame = 250;
int CurrentFrame = 1;

Uint32 millisecondsPerFrame = 67;  // animación (ej: 15 FPS)
int FrameRate = 60;                // render (ej: 60 FPS)

Uint32 lastAnimTime = SDL_GetTicks();
Uint32 lastRenderTime = SDL_GetTicks();

// Funciones
void CalculateMillisecondsPerFrame(int aFPS) {
    FrameRate = aFPS;
    millisecondsPerFrame = 1000 / aFPS;
}

// Funciones de keyframes
void Swap(keyFrame& a, keyFrame& b) {
    keyFrame temp = a;
    a = b;
    b = temp;
}

int Partition(std::vector<keyFrame>& arr, int low, int high) {
    int pivot = arr[high].frame;
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j].frame < pivot) {
            i++;
            Swap(arr[i], arr[j]);
        }
    }
    Swap(arr[i + 1], arr[high]);
    return i + 1;
}

void QuickSort(std::vector<keyFrame>& arr, int low, int high) {
    if (low < high) {
        int pi = Partition(arr, low, high);
        QuickSort(arr, low, pi - 1);
        QuickSort(arr, pi + 1, high);
    }
}

bool compareKeyFrames(const keyFrame& a, const keyFrame& b) {
    return a.frame < b.frame;
}

// AnimProperty
void AnimProperty::SortKeyFrames() {
    QuickSort(keyframes, 0, keyframes.size() - 1);
}

// AnimationObject
void AnimationObject::UpdateFirstLastFrame() {
    FirstKeyFrame = 100000000;
    LastKeyFrame = 0;
    for(size_t pr = 0; pr < Propertys.size(); pr++) {    
        for(size_t kf = 0; kf < Propertys[pr].keyframes.size(); kf++) {    
            if (Propertys[pr].keyframes[kf].frame > LastKeyFrame){
                LastKeyFrame = Propertys[pr].keyframes[kf].frame;    
            }
            if (Propertys[pr].keyframes[kf].frame < FirstKeyFrame){
                FirstKeyFrame = Propertys[pr].keyframes[kf].frame;    
            }
        }    
    }
}

// Vector global de objetos animados
std::vector<AnimationObject> AnimationObjects;

// Funciones de búsqueda (implementa según necesites)
int BuscarAnimacionObj() {
    int index = -1;
    return index;
}

int BuscarAnimProperty(int indice, int propertySelect) {
    int index = -1;
    return index;
}

int BuscarShapeKeyAnimation(Object* obj, bool mostrarError) {
    return -1;
}

void ReloadAnimation() {	
    // ShapeKeyAnimation
    /*for (size_t ska = 0; ska < ShapeKeyAnimations.size(); ska++) {
        Mesh& pMesh = Meshes[ShapeKeyAnimations[ska].Id];
        ShapeKeyAnimation& animState = ShapeKeyAnimations[ska];

        animState.Mix++;
        if (animState.Mix >= animState.Animations[animState.LastAnimation].MixSpeed) {
            animState.Mix = 0;
            animState.LastAnimation = animState.NextAnimation;
            animState.LastFrame = animState.NextFrame;

            if (animState.ChangeAnimation < 0) {
                animState.NextFrame++;
                if (animState.NextFrame >= (int)animState.Animations[animState.NextAnimation].Frames.size()) {
                    animState.NextFrame = 0;
                }
            } else {
                animState.NextAnimation = animState.ChangeAnimation;
                animState.ChangeAnimation = -1;
                animState.NextFrame = 0;
            }
        }

        Animation& LasAnim = animState.Animations[animState.LastAnimation];
        Animation& NexAnim = animState.Animations[animState.NextAnimation];
        ShapeKey& LastFrame = LasAnim.Frames[animState.LastFrame];
        ShapeKey& NextFrame = NexAnim.Frames[animState.NextFrame];

        // Calcular el porcentaje de mezcla
        GLfloat mixFactor = static_cast<GLfloat>(animState.Mix) / LasAnim.MixSpeed;

        if (!animState.Interpolacion) {
            for (size_t v = 0; v < LastFrame.Vertex.size(); v++) {
                pMesh.vertex[v*3]     = LastFrame.Vertex[v].vertexX;
                pMesh.vertex[v*3 + 1] = LastFrame.Vertex[v].vertexY;
                pMesh.vertex[v*3 + 2] = LastFrame.Vertex[v].vertexZ;
                if (animState.Normals) {
                    pMesh.normals[v*3]     = LastFrame.Vertex[v].normalX;
                    pMesh.normals[v*3 + 1] = LastFrame.Vertex[v].normalY;
                    pMesh.normals[v*3 + 2] = LastFrame.Vertex[v].normalZ;
                }
            }
        } else {
            for (size_t v = 0; v < NextFrame.Vertex.size(); v++) {
                // Interpolación lineal de los vértices
                pMesh.vertex[v*3] = static_cast<GLshort>(
                    LastFrame.Vertex[v].vertexX +
                    mixFactor * (NextFrame.Vertex[v].vertexX - LastFrame.Vertex[v].vertexX)
                );
                pMesh.vertex[v*3 + 1] = static_cast<GLshort>(
                    LastFrame.Vertex[v].vertexY +
                    mixFactor * (NextFrame.Vertex[v].vertexY - LastFrame.Vertex[v].vertexY)
                );
                pMesh.vertex[v*3 + 2] = static_cast<GLshort>(
                    LastFrame.Vertex[v].vertexZ +
                    mixFactor * (NextFrame.Vertex[v].vertexZ - LastFrame.Vertex[v].vertexZ)
                );

                // Interpolación lineal de las normales (si aplica)
                if (animState.Normals) {
                    pMesh.normals[v*3] = static_cast<GLshort>(
                        LastFrame.Vertex[v].normalX +
                        mixFactor * (NextFrame.Vertex[v].normalX - LastFrame.Vertex[v].normalX)
                    );
                    pMesh.normals[v*3 + 1] = static_cast<GLshort>(
                        LastFrame.Vertex[v].normalY +
                        mixFactor * (NextFrame.Vertex[v].normalY - LastFrame.Vertex[v].normalY)
                    );
                    pMesh.normals[v*3 + 2] = static_cast<GLshort>(
                        LastFrame.Vertex[v].normalZ +
                        mixFactor * (NextFrame.Vertex[v].normalZ - LastFrame.Vertex[v].normalZ)
                    );
                }
            }
        }
    }

    // Animación de objetos (posición, rotación, escala, etc.)
    for (size_t a = 0; a < AnimationObjects.size(); a++) {
        for (size_t p = 0; p < AnimationObjects[a].Propertys.size(); p++) {
            AnimProperty& anim = AnimationObjects[a].Propertys[p];
            if (anim.keyframes.size() > 0) {
                GLfloat valueX = 0, valueY = 0, valueZ = 0;
                int firstFrameIndex = 0, lastFrameIndex = 0;

                // Encontrar el primer y último frame relevante
                for (size_t f = 0; f < anim.keyframes.size(); f++) {
                    if (anim.keyframes[f].frame <= CurrentFrame) {
                        firstFrameIndex = (int)f;
                    }
                    if (anim.keyframes[f].frame >= CurrentFrame) {
                        lastFrameIndex = (int)f;
                        break;
                    }
                }

                // Si CurrentFrame está fuera de los límites de los keyframes
                if (CurrentFrame <= anim.keyframes[firstFrameIndex].frame) {
                    valueX = anim.keyframes[firstFrameIndex].valueX;
                    valueY = anim.keyframes[firstFrameIndex].valueY;
                    valueZ = anim.keyframes[firstFrameIndex].valueZ;
                } else if (CurrentFrame >= anim.keyframes[lastFrameIndex].frame) {
                    valueX = anim.keyframes.back().valueX;
                    valueY = anim.keyframes.back().valueY;
                    valueZ = anim.keyframes.back().valueZ;
                } else {
                    // Interpolación entre keyframes
                    int frame1 = anim.keyframes[firstFrameIndex].frame;
                    int frame2 = anim.keyframes[lastFrameIndex].frame;

                    GLfloat valueX1 = anim.keyframes[firstFrameIndex].valueX;
                    GLfloat valueY1 = anim.keyframes[firstFrameIndex].valueY;
                    GLfloat valueZ1 = anim.keyframes[firstFrameIndex].valueZ;

                    GLfloat valueX2 = anim.keyframes[lastFrameIndex].valueX;
                    GLfloat valueY2 = anim.keyframes[lastFrameIndex].valueY;
                    GLfloat valueZ2 = anim.keyframes[lastFrameIndex].valueZ;

                    GLfloat t  = (CurrentFrame - frame1) / (GLfloat)(frame2 - frame1);
                    GLfloat t2 = 0;

                    switch (anim.keyframes[firstFrameIndex].Interpolation) {
                        case Constant:
                            valueX = valueX1;
                            valueY = valueY1;
                            valueZ = valueZ1;
                            break;
                        case Linear:
                            valueX = valueX1 + (valueX2 - valueX1) * t;
                            valueY = valueY1 + (valueY2 - valueY1) * t;
                            valueZ = valueZ1 + (valueZ2 - valueZ1) * t;
                            break;
                        case EaseIn:
                            t2 = t * t;
                            valueX = valueX1 + (valueX2 - valueX1) * t2;
                            valueY = valueY1 + (valueY2 - valueY1) * t2;
                            valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
                            break;
                        case EaseOut:
                            t2 = 1 - (1 - t) * (1 - t);
                            valueX = valueX1 + (valueX2 - valueX1) * t2;
                            valueY = valueY1 + (valueY2 - valueY1) * t2;
                            valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
                            break;
                        case EaseInOut:
                            if (t < 0.5f)
                                t2 = 2 * t * t;
                            else
                                t2 = 1 - 2 * (1 - t) * (1 - t);
                            valueX = valueX1 + (valueX2 - valueX1) * t2;
                            valueY = valueY1 + (valueY2 - valueY1) * t2;
                            valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
                            break;
                        default:
                            valueX = valueX1;
                            valueY = valueY1;
                            valueZ = valueZ1;
                            break;
                    }
                }

                // Asignar valor a la propiedad
                switch (anim.Property) {
                    case AnimPosition:
                        AnimationObjects[a].obj->posX = valueX;
                        AnimationObjects[a].obj->posY = valueY;
                        AnimationObjects[a].obj->posZ = valueZ;
                        break;
                    case AnimRotation:
                        AnimationObjects[a].obj->rotX = valueX;
                        AnimationObjects[a].obj->rotY = valueY;
                        AnimationObjects[a].obj->rotZ = valueZ;
                        break;
                    case AnimScale:
                        AnimationObjects[a].obj->scaleX = valueX;
                        AnimationObjects[a].obj->scaleY = valueY;
                        AnimationObjects[a].obj->scaleZ = valueZ;
                        break;
                    default:
                        break;
                }
            }
        }
    }*/
}