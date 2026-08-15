#ifndef IMPORTWOBJ_H
#define IMPORTWOBJ_H

#include "importers/import_obj.h"
#ifndef W3D_SYMBIAN
#include <filesystem>   // (no se usa hoy; RVCT/Symbian no tiene C++17 filesystem)
#endif

// vertToCP (opcional): por cada vertice de RENDER creado, el indice de la linea 'v'
// del OBJ de la que salio (dominio CONTROL-POINT). Lo pide ImportWOBJ cuando hay un
// sidecar de grupos <modelo>.grupos.json (con noMerge el mapeo es la identidad).
Mesh* LeerWOBJ(std::istream& file, const std::string& filename, Object* parent, bool NoMerge,
               std::vector<int>* vertToCP = NULL);
Mesh* ImportWOBJ(const std::string& filepath, Object* parent, bool NoMerge);

#endif