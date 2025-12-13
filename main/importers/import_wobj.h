#ifndef IMPORTWOBJ_H
#define IMPORTWOBJ_H

#include "importers/import_obj.h"

Mesh* LeerWOBJ(std::ifstream& file, const std::string& filename, Object* parent, bool NoMerge);
Mesh* ImportWOBJ(const std::string& filepath, Object* parent, bool NoMerge);

#endif