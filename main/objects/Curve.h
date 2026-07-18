#ifndef CURVE_H
#define CURVE_H

#ifdef _WIN32
    #define NOMINMAX
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#include <string>
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // portable: iconos compartidos
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
    #ifndef _WIN32
    #include <GL/glext.h>
    #endif
    #include "WhiskUI/theme/colores.h"
    //para leer el archivo de texto
    #include <fstream>
    #include <sstream>
    #include <iostream>
#endif

struct KDNode {
    int index;        // indice del vertice original
    Vector3 point;

    KDNode* left;
    KDNode* right;

    KDNode() : index(0), left(NULL), right(NULL) {}
};

class Curve : public Object { 
    public:
        int vertexSize;   // (inicializados en el constructor: C++03)
        GLfloat* vertex;
        GLushort* indices;

        Curve(Object* parent = NULL, Vector3 pos = Vector3(0,0,0));

        ~Curve();

        ObjectType getType() override;

        void RenderObject() override;

        bool LoadFromFile(const std::string& filepath);

        KDNode* kdRoot;

        Vector3 GetPoint(int i) const;

        void BuildKDTree();
        KDNode* BuildKDTreeRecursive(std::vector<int>& indices, int depth);
        int FindNearest(const Vector3& target) const;
};

#endif