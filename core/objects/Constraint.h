#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include <string>
#include <iostream>

#include "Objects.h"
#include "Target.h"
#include "UI/icons.h"

class Constraint: public Object, public Target {
    public:
        Constraint(Object* parent);

        ObjectType getType() override;
        void Reload() override;
        void RenderObject() override;

        ~Constraint();
};

#endif