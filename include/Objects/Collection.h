#ifndef COLLECTION_H
#define COLLECTION_H


#include "./UI/icons.h"
#include "./Objects.h"

class Collection : public Object {
public:
    // Constructor
    Collection(Object* parent = nullptr);

    // Métodos
    ObjectType getType() override;

    // Destructor
    ~Collection();
};

#endif