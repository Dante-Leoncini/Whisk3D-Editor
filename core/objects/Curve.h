#ifndef CURVE_H
#define CURVE_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <GL/gl.h>
#include <GL/glext.h>

#include "UI/colores.h"
#include "UI/icons.h"
#include "Objects.h"

//para leer el archivo de texto
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

struct KDNode {
    int index;        // índice del vértice original
    Vector3 point;

    KDNode* left  = nullptr;
    KDNode* right = nullptr;
};

class Curve : public Object { 
    public:
        int vertexSize = 0;
        GLfloat* vertex = nullptr;
        GLushort* indices;

        Curve(Object* parent = nullptr, Vector3 pos = Vector3(0,0,0));

        ~Curve();

        ObjectType getType() override;

        void RenderObject() override;

        bool LoadFromFile(const std::string& filepath);

        KDNode* kdRoot = nullptr;

        Vector3 GetPoint(int i) const;

        void BuildKDTree();
        KDNode* BuildKDTreeRecursive(std::vector<int>& indices, int depth);
        int FindNearest(const Vector3& target) const;
};

#endif