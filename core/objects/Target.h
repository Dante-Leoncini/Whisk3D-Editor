#ifndef TARGET_H
#define TARGET_H

#include <string>
#include <iostream>

#include "Objects.h"

class Target {
    public:
        std::string targetName = "";
        Object* target = nullptr;

        void SetTarget(const std::string& NewValue);

        bool IsMyAncestor(Object* node, Object* possibleAncestor);

        void ReloadTarget(Object* me);
};

#endif