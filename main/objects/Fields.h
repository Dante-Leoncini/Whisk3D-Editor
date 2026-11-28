#ifndef W3D_FIELDS_H
#define W3D_FIELDS_H

// ============================================================================
//  LOS CAMPOS DE LOS OBJETOS, en UNA tabla: cada uno con su nombre ("fog.density"), lo que es y donde esta en el
//  objeto. Antes cada campo se escribia a mano en cinco lugares (guardar, cargar, duplicar, la tarjeta de Properties y
//  la tabla de las rutinas): uno nuevo se agrega aca y lo toman las tarjetas declaradas (ui/panels/Panels.h).
//  El nombre es "tipo.campo": el tipo es el del .w3d (W3dTypeName) y el campo, su clave en el .w3d.
// ============================================================================
class Object;

enum W3dFieldKind {
    FieldBool,    // bool
    FieldInt,     // int (un modo, una opcion de una lista)
    FieldFloat,   // float
    FieldColor,   // float[4]
    FieldVec3,    // Vector3 (x, y, z)
    FieldImage    // Texture* (una IMAGEN: la fila muestra su archivo y un boton la cambia; la retiene el objeto)
};

struct W3dField {
    const char* name;            // "fog.density"
    W3dFieldKind kind;
    int objType;                 // el ObjectType que lo tiene
    void* (*at)(Object* o);      // donde esta en un objeto de ese tipo
    bool (*valid)(Object* o);    // (o NULL) ademas del tipo: la raiz de la escena dice ser una coleccion y no lo es
};

// el campo por nombre (NULL = no hay)
const W3dField* W3dFieldFind(const char* name);
// donde esta el campo en ese objeto (NULL si el objeto no es de su tipo)
void* W3dFieldAt(const W3dField* f, Object* o);
// el tipo de objeto por su nombre del .w3d ("fog"): -1 = no hay
int W3dObjectTypeOf(const char* name);
// copia TODOS los campos de la tabla de 'src' a 'dst' (del mismo tipo): duplicar un objeto cuyos campos estan todos aca
// es esta linea, sin repetir la lista
void W3dFieldsCopy(Object* src, Object* dst);

#endif
