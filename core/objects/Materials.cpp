#include "Materials.h"
#include <iostream>

// Variables globales
std::vector<Material*> Materials;
Material* MaterialDefecto = nullptr;

// ===================================================
// Implementación de Material
// ===================================================
Material::Material(const std::string& nombre, bool MaterialDefectoFlag, bool TieneVertexColor) {
    name = new Text(nombre);
    if (!MaterialDefectoFlag){
        Materials.push_back(this);
    }
    vertexColor = TieneVertexColor;
    interpolacion = lineal;
}

Material::~Material() {
    delete name;
}

// ===================================================
// Funciones auxiliares
// ===================================================
Material* BuscarMaterialPorNombre(const std::string& name) {
    for (size_t i = 0; i < Materials.size(); ++i){
        if (Materials[i]->name->value == name) return Materials[i];
    }
    return nullptr;
}

int DuplicateMaterial(int srcId) {
    return -1;
    /*
    if (srcId < 0 || srcId >= (int)Materials.size()) return -1;

    Material copy = Materials[srcId]; // copia completa
    Materials.push_back(copy);

    return (int)Materials.size() - 1; // devuelve el índice del duplicado
    */
}