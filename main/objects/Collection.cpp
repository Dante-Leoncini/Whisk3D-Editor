#include "Collection.h"
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner

// Constructor
Collection::Collection(Object* parent, Vector3 pos)
    : Object(parent, T("Collection"), pos){
}

// Método getType
ObjectType Collection::getType() {
    return ObjectType::collection;
}

// Destructor
Collection::~Collection() {
}