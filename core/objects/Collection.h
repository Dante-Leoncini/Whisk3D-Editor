#ifndef COLLECTION_H
#define COLLECTION_H


#include "./UI/icons.h"
#include "./Objects.h"

class Collection : public Object {
public:
    // Constructor
    Collection(Object* parent = nullptr, Vector3 pos = Vector3(0,0,0));

    // Métodos
    ObjectType getType() override;

    // Destructor
    ~Collection();
};

#endif