#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include <string>
#include <iostream>

#include "Objects.h"
#include "Target.h"
#include "UI/icons.h"
#include "variables.h" 

class Constraint: public Object, public Target {
    public:
        bool useHorizontal;
        bool usePitch;

        Constraint(Object* parent, 
            bool Horizontal = true, bool Pitch = false, 
            Vector3 pos = Vector3(0,0,0));

        ObjectType getType() override;
        void Reload() override;
        void RenderObject() override;

        ~Constraint();
};

#endif