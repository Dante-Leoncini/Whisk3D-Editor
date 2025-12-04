#include "Objects/Collection.h"

// Constructor
Collection::Collection(Object* parent)
    : Object(parent, "Collection"){
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