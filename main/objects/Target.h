#ifndef TARGET_H
#define TARGET_H

#include <string>
#ifndef W3D_SYMBIAN
#include <iostream>
#endif

#include "objects/Objects.h"

class Target {
    public:
        std::string targetName;
        Object* target;

        Target() : target(NULL) {}

        void SetTarget(const std::string& NewValue);

        bool IsMyAncestor(Object* node, Object* possibleAncestor);

        void ReloadTarget(Object* me);
};

#endif