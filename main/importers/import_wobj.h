#ifndef IMPORTWOBJ_H
#define IMPORTWOBJ_H

#include "importers/import_obj.h"
#ifndef W3D_SYMBIAN
#include <filesystem>   // (no se usa hoy; RVCT/Symbian no tiene C++17 filesystem)
#endif

// vertToCP (opcional): por cada vertice de RENDER creado, el indice de la linea 'v'
// del OBJ de la que salio (dominio CONTROL-POINT). Lo pide ImportWOBJ cuando hay un
// sidecar de grupos <modelo>.groups.json (con noMerge el mapeo es la identidad).
Mesh* ReadWOBJ(std::istream& file, const std::string& filename, Object* parent, bool NoMerge,
               std::vector<int>* vertToCP = NULL, size_t totalBytes = 0);
// noEditable: malla de escenario CERRADA a edicion -> se saltea ComputeBorders
// (posRep/edges/bordersBuf) y solo se calcula el AABB (ComputeAABBOnly). Carga
// mas rapido y no gasta memoria de edicion; el Tab a Edit Mode la ignora.
Mesh* ImportWOBJ(const std::string& filepath, Object* parent, bool NoMerge, bool noEditable = false);

#endif