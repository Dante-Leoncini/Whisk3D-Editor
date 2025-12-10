#include "Collection.h"

// Constructor
Collection::Collection(Object* parent, Vector3 pos)
    : Object(parent, "Collection", pos){
    IconType = static_cast<size_t>(IconType::archive);
}

// Método getType
ObjectType Collection::getType() {
    return ObjectType::collection;
}

// Destructor
Collection::~Collection() {
    delete name;
}