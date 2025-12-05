#ifndef MODIFIER_H
#define MODIFIER_H

#include <string>
#include <iostream>

#include "UI/icons.h"
#include "Objects.h"

class Modifier {
public:
    std::string targetName = "";
    Object* target = nullptr;

    void SetTarget(const std::string& NewValue);

    bool IsMyAncestor(Object* node, Object* possibleAncestor);

    void ReloadTarget(Object* me);
};

#endif