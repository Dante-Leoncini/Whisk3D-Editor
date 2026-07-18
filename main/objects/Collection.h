#ifndef COLLECTION_H
#define COLLECTION_H


#include "objects/Objects.h"
#ifndef W3D_SYMBIAN
    #include "WhiskUI/draw/icons.h"
#endif

class Collection : public Object {
public:
    // Constructor
    Collection(Object* parent = NULL, Vector3 pos = Vector3(0,0,0));

    // Métodos
    ObjectType getType() override;

    // Destructor
    ~Collection();
};

#endif