#include "objects/Fields.h"
#include "objects/Objects.h"
#include "objects/Fog.h"
#include "objects/Empty.h"
#include "objects/ClipRegion.h"   // ClearZ
#include "objects/Particles.h"
#include "objects/Collection.h"
#include <string.h>

namespace {
// donde esta el miembro Mem en un T: un puntero a miembro como parametro de template (C++98, sin lambdas: el Symbian)
template <class T, class M, M T::*Mem> void* At(Object* o) { return &(static_cast<T*>(o)->*Mem); }
// una coleccion de verdad: la raiz de la escena tambien dice ser de tipo coleccion, pero no tiene padre ni es una Collection
bool RealCollection(Object* o) { return o->Parent != 0; }

const W3dField kFields[] = {
    // NIEBLA (objects/Fog.h)
    { "fog.active",     FieldBool,  ObjectType::fog, &At<Fog, bool, &Fog::active> },
    { "fog.mode",       FieldInt,   ObjectType::fog, &At<Fog, int, &Fog::mode> },
    { "fog.start",      FieldFloat, ObjectType::fog, &At<Fog, float, &Fog::start> },
    { "fog.end",        FieldFloat, ObjectType::fog, &At<Fog, float, &Fog::end> },
    { "fog.density",    FieldFloat, ObjectType::fog, &At<Fog, float, &Fog::density> },
    { "fog.color",      FieldColor, ObjectType::fog, &At<Fog, float[4], &Fog::color> },
    { "fog.background", FieldBool,  ObjectType::fog, &At<Fog, bool, &Fog::background> },
    // VACIO (objects/Empty.h): como se dibuja
    { "empty.shape",    FieldInt,   ObjectType::empty, &At<Empty, int, &Empty::shape> },
    { "empty.drawSize", FieldFloat, ObjectType::empty, &At<Empty, float, &Empty::drawingSize> },
    { "empty.drawRot",  FieldVec3,  ObjectType::empty, &At<Empty, Vector3, &Empty::rotDrawing> },
    // LIMPIAR PROFUNDIDAD (objects/ClipRegion.h)
    { "clearz.active",  FieldBool,  ObjectType::clearz, &At<ClearZ, bool, &ClearZ::active> },
    // PARTICULAS (objects/Particles.h): movimiento, tamanio y rotacion
    { "particles.vel",          FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::vel> },
    { "particles.dispersion",   FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::dispersion> },
    { "particles.velLocal",     FieldVec3,  ObjectType::particles, &At<Particles, float[3], &Particles::velLocal> },
    { "particles.randomVel",    FieldVec3,  ObjectType::particles, &At<Particles, float[3], &Particles::velRandom> },
    { "particles.gravity",      FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::gravity> },
    { "particles.acceleration", FieldVec3,  ObjectType::particles, &At<Particles, float[3], &Particles::acceleration> },
    { "particles.drag",         FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::drag> },
    { "particles.turbulence",   FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::turbulence> },
    { "particles.randomPos",    FieldVec3,  ObjectType::particles, &At<Particles, float[3], &Particles::posRandom> },
    { "particles.size",           FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::size> },
    { "particles.variation",      FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::variation> },
    { "particles.growth",         FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::growth> },
    { "particles.growthBrake",    FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::brakeGrowth> },
    { "particles.rotation",       FieldBool,  ObjectType::particles, &At<Particles, bool, &Particles::rotation> },
    { "particles.initialRot",     FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::initialRot> },
    { "particles.randomRot",      FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::rotRandom> },
    { "particles.rotationSpeed",  FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::velRotation> },
    { "particles.randomRotVel",   FieldFloat, ObjectType::particles, &At<Particles, float, &Particles::velRotRandom> },
    { "particles.randomSpinSign", FieldBool,  ObjectType::particles, &At<Particles, bool, &Particles::turnSignRandom> },
    // COLECCION (objects/Collection.h): el orden de dibujo de sus hijos
    { "collection.sortByCamera", FieldBool, ObjectType::collection, &At<Collection, bool, &Collection::sortByCamera>, &RealCollection },
    { "collection.sortOnce",     FieldBool, ObjectType::collection, &At<Collection, bool, &Collection::sortOnce>, &RealCollection },
};
const int kFieldCount = (int)(sizeof(kFields) / sizeof(kFields[0]));
}

const W3dField* W3dFieldFind(const char* name) {
    for (int i = 0; i < kFieldCount; i++) if (strcmp(kFields[i].name, name) == 0) return &kFields[i];
    return 0;
}

void* W3dFieldAt(const W3dField* f, Object* o) {
    if (!f || !o || (int)o->getType() != f->objType || (f->valid && !f->valid(o))) return 0;
    return f->at(o);
}

void W3dFieldsCopy(Object* src, Object* dst) {
    if (!src || !dst || src->getType() != dst->getType()) return;
    for (int i = 0; i < kFieldCount; i++) {
        void* a = W3dFieldAt(&kFields[i], src);
        void* b = a ? W3dFieldAt(&kFields[i], dst) : 0;
        if (!b) continue;
        switch (kFields[i].kind) {
            case FieldBool:  *(bool*)b = *(const bool*)a; break;
            case FieldInt:   *(int*)b = *(const int*)a; break;
            case FieldFloat: *(float*)b = *(const float*)a; break;
            case FieldColor: memcpy(b, a, sizeof(float) * 4); break;
            case FieldVec3:  *(Vector3*)b = *(const Vector3*)a; break;
        }
    }
}

int W3dObjectTypeOf(const char* name) {
    for (int t = 0; t <= (int)ObjectType::reflectionProbe; t++)
        if (strcmp(W3dTypeName(ObjectType((ObjectType::Enum)t)), name) == 0) return t;
    return -1;
}
