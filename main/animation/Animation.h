#ifndef ANIMATION_H
#define ANIMATION_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <iostream>

#include <iostream>

#include "objects/Objects.h"

// Clases de shape key
class ShapeKeyVertex { 
public:
    int index;
    GLshort vertexX;
    GLshort vertexY;
    GLshort vertexZ;
    GLbyte normalX;
    GLbyte normalY;
    GLbyte normalZ;
};

class ShapeKey { 
public:
    std::vector<ShapeKeyVertex> Vertex;
};

// Clase de animación
class Animation { 
public:
    std::vector<ShapeKey> Frames;
    int MixSpeed;
};

// Variables globales
extern bool PlayAnimation;
extern int StartFrame;
extern int EndFrame;
extern int CurrentFrame;

extern Uint32 millisecondsPerFrame;
extern int FrameRate;

extern Uint32 lastAnimTime;
extern Uint32 lastRenderTime;

// Funciones de animación
void CalculateMillisecondsPerFrame(int aFPS);

// Constantes de animación
enum { AnimPosition, AnimRotation, AnimScale };

// Keyframe
class keyFrame { 
public:
    int frame;
    GLfloat valueX;
    GLfloat valueY;
    GLfloat valueZ;
    int Interpolation;
};

// Funciones auxiliares
void Swap(keyFrame& a, keyFrame& b);
int Partition(std::vector<keyFrame>& arr, int low, int high);
void QuickSort(std::vector<keyFrame>& arr, int low, int high);
bool compareKeyFrames(const keyFrame& a, const keyFrame& b);

// Propiedad de animación
class AnimProperty { 
public:
    int Property;
    int firstFrameIndex;
    int lastFrameIndex;
    std::vector<keyFrame> keyframes;

    void SortKeyFrames();
};

// Animación de objeto
class AnimationObject { 
public:
    Object* obj; 
    int FirstKeyFrame;
    int LastKeyFrame;
    std::vector<AnimProperty> Propertys;

    void UpdateFirstLastFrame();
};

// Variables globales de objetos animados
extern std::vector<AnimationObject> AnimationObjects;

// Funciones de búsqueda
int BuscarAnimacionObj();
int BuscarAnimProperty(int indice, int propertySelect);
int BuscarShapeKeyAnimation(Object* obj, bool mostrarError);

// Función para recargar animación
void ReloadAnimation();

#endif