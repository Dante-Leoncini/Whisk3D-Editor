#include "Collection.h"
#include "WhiskUI/icons.h" // el icono compartido del outliner

// Constructor
Collection::Collection(Object* parent, Vector3 pos)
    : Object(parent, "Collection", pos){
}

// Método getType
ObjectType Collection::getType() {
    return ObjectType::collection;
}

// Destructor
Collection::~Collection() {
}