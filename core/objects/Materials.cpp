#include "Materials.h"
#include <iostream>

// Variables globales
std::vector<Material*> Materials;
Material* MaterialDefecto = nullptr;

// ===================================================
// Implementación de Material
// ===================================================
Material::Material(const std::string& nombre, bool MaterialDefectoFlag, bool TieneVertexColor) {
    name = nombre;
    if (!MaterialDefectoFlag){
        Materials.push_back(this);
    }
    vertexColor = TieneVertexColor;
    interpolacion = lineal;
}

Material::~Material() {}

// ===================================================
// Funciones auxiliares
// ===================================================
Material* BuscarMaterialPorNombre(const std::string& name) {
    for (size_t i = 0; i < Materials.size(); ++i){
        if (Materials[i]->name == name) return Materials[i];
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

// ===================================================
// Implementación de Material animado
// ===================================================
void AnimatedMaterial::Update() {
    //std::cout << "Control detectado: " << SDL_GameControllerName(controller) << std::endl;
    tickCounter++;

    if (tickCounter >= frameDurations[frameIndex]) {
        tickCounter = 0;
        frameIndex = (frameIndex + 1) % frameTextures.size();
        for (Material* target : targets) {
            if (target)
                target->texture = frameTextures[frameIndex];
        }
    }
}

std::vector<AnimatedMaterial*> AnimatedMaterials;

void UpdateAnimatedMaterials() {
    for (AnimatedMaterial* anim : AnimatedMaterials) {
        if (anim)
            anim->Update();
    }
}