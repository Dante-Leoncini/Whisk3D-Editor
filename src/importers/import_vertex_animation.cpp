#include "importers/import_vertex_animation.h"

bool ImportVertexAnimation(const std::string& filepath) {
    return false;
    /*
    // Revisar extensión
    if (filepath.size() < 4 || filepath.substr(filepath.size() - 4) != ".txt") {
        std::cerr << "Error: El archivo seleccionado no tiene la extensión .txt" << std::endl;
        return false;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error al abrir: " << filepath << std::endl;
        return false;
    }

    // si no hay objetos
    if (!ObjActivo) { file.close(); return false; }
    Object& obj = *ObjActivo;
    if (!obj.select) { file.close(); return false; }
    // si no es un mesh
    bool esMesh = false;

    Mesh meshInstance;    // Crea un objeto válido
    Mesh& pMesh = meshInstance; // referencia por defecto
    if (obj.type == mesh) {
        pMesh = Meshes[obj.Id];
        esMesh = true;
    }

    std::string rawline;
    int SAIndex = -1;
    int AnimID = 0;
    int FrameIndex = 0;

    keyFrame defaultKey{};
    defaultKey.Interpolation = 0;
    defaultKey.valueX = defaultKey.valueY = defaultKey.valueZ = 0;
    defaultKey.frame = 0;

    // Para parsing de anim objeto
    int animIndex = -1;
    int propIndex = -1;

    while (std::getline(file, rawline)) {
        // eliminar CR si existe
        if (!rawline.empty() && rawline.back() == '\r') rawline.pop_back();
        // trim left (solo espacios)
        size_t startpos = rawline.find_first_not_of(" \t");
        if (startpos != std::string::npos) rawline = rawline.substr(startpos);
        if (rawline.empty()) continue;

        // --- ShapeKey animation import ---
        if (rawline.rfind("new_animation_ShapeKey", 0) == 0) {
            if (esMesh) {
                SAIndex = BuscarShapeKeyAnimation(&obj, false);
                if (SAIndex < 0) {
                    SAIndex = (int)ShapeKeyAnimations.size();
                    ShapeKeyAnimation newAnim{};
                    newAnim.obj = &obj;
                    newAnim.ChangeAnimation = -1;
                    newAnim.LastFrame = newAnim.NextFrame = 0;
                    newAnim.LastAnimation = newAnim.NextAnimation = 0;
                    newAnim.Mix = 0;
                    newAnim.Normals = true;
                    newAnim.Faces = false;
                    newAnim.Interpolacion = false;
                    ShapeKeyAnimations.push_back(std::move(newAnim));
                }

                AnimID = (int)ShapeKeyAnimations[SAIndex].Animations.size();
                Animation newAnimation{};
                newAnimation.MixSpeed = 1;
                ShapeKeyAnimations[SAIndex].Animations.push_back(std::move(newAnimation));

                // Parsear parámetros adicionales (si los hay)
                std::istringstream iss(rawline.substr(22));
                int param;
                int count = 0;
                while (iss >> param) {
                    switch (count) {
                        case 0: ShapeKeyAnimations[SAIndex].Animations[AnimID].MixSpeed = param; break;
                        case 1: if (param == 1) ShapeKeyAnimations[SAIndex].Interpolacion = true; break;
                        case 2: if (param == 0) ShapeKeyAnimations[SAIndex].Normals = false; break;
                        case 3: if (param == 1) ShapeKeyAnimations[SAIndex].Faces = true; break;
                    }
                    ++count;
                }
            }
            // si no es mesh, ignorar los parámetros
        }
        else if (rawline.rfind("ShapeKey", 0) == 0) {
            if (esMesh) {
                ShapeKey newShapeKey;
                ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames.push_back(std::move(newShapeKey));
                FrameIndex = (int)ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames.size() - 1;
                // Reservar espacio para los vértices
                ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames[FrameIndex].Vertex.reserve(pMesh.vertexSize);
                if (ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames.size() > 1 &&
                    ShapeKeyAnimations[SAIndex].NextFrame == 0) {
                    ShapeKeyAnimations[SAIndex].NextFrame = 1;
                }
            }
        }
        else if (rawline.rfind("akf ", 0) == 0) {
            // akf x y z nx ny nz
            std::istringstream iss(rawline.substr(4));
            double num;
            ShapeKeyVertex newVertex{};
            // Default values (in case less than 6 tokens)
            newVertex.vertexX = newVertex.vertexY = newVertex.vertexZ = 0;
            newVertex.normalX = newVertex.normalY = newVertex.normalZ = 0;
            for (int i = 0; i < 6; ++i) {
                if (!(iss >> num)) break;
                switch (i) {
                    case 0: // Z
                        newVertex.vertexZ = static_cast<GLshort>(-num * 2000.0);
                        break;
                    case 1: // Y
                        newVertex.vertexY = static_cast<GLshort>(num * 2000.0);
                        break;
                    case 2: // X
                        newVertex.vertexX = static_cast<GLshort>(num * 2000.0);
                        break;
                    case 3: { // normalZ
                        double v = -(((num + 1.0) / 2.0) * 255.0 - 128.0);
                        v = std::clamp(v, -128.0, 127.0);
                        newVertex.normalZ = static_cast<GLbyte>(v);
                        break;
                    }
                    case 4: { // normalY
                        double v = ((num + 1.0) / 2.0) * 255.0 - 128.0;
                        v = std::clamp(v, -128.0, 127.0);
                        newVertex.normalY = static_cast<GLbyte>(v);
                        break;
                    }
                    case 5: { // normalX
                        double v = ((num + 1.0) / 2.0) * 255.0 - 128.0;
                        v = std::clamp(v, -128.0, 127.0);
                        newVertex.normalX = static_cast<GLbyte>(v);
                        break;
                    }
                }
            }
            if (esMesh) {
            	ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames[FrameIndex].Vertex.push_back(newVertex);
            }
        }
        // --- Animacion de objetos: rotacion / r / locacion / l / escala / s ---
        else if (rawline.rfind("rotacion ", 0) == 0) {
            // Crear/obtener anim del objeto
            animIndex = BuscarAnimacionObj();
            if (animIndex < 0) {
                AnimationObject newAnim;
                AnimationObjects.push_back(std::move(newAnim));
                animIndex = (int)AnimationObjects.size() - 1;
            }
            AnimationObject& anim = AnimationObjects[animIndex];
            anim.obj = SelectActivo;

            propIndex = BuscarAnimProperty(animIndex, AnimRotation);
            if (propIndex < 0) {
                AnimProperty propNew;
                anim.Propertys.push_back(std::move(propNew));
                propIndex = (int)anim.Propertys.size() - 1;
            }
            AnimProperty& prop = anim.Propertys[propIndex];
            prop.Property = AnimRotation;

            // Leer parámetros (por ejemplo cantidad de keyframes para reservar)
            std::istringstream iss(rawline.substr(9));
            int number;
            int count = 0;
            while (iss >> number && count < 2) {
                if (count == 0 && number > 0) {
                    prop.keyframes.reserve(number);
                }
                ++count;
            }
        }
        else if (rawline.rfind("r ", 0) == 0) {
            if (animIndex < 0 || propIndex < 0) {
                // no hay contexto, ignorar
                continue;
            }
            AnimationObject& anim = AnimationObjects[animIndex];
            AnimProperty& prop = anim.Propertys[propIndex];
            // append default key then fill
            prop.keyframes.push_back(defaultKey);
            int idxKey = (int)prop.keyframes.size() - 1;

            std::istringstream iss(rawline.substr(2));
            int number;
            int count = 0;
            while (iss >> number && count < 4) {
                switch (count) {
                    case 0: prop.keyframes[idxKey].frame = number; break;
                    case 1: prop.keyframes[idxKey].valueX = static_cast<float>(number); break;
                    case 2: prop.keyframes[idxKey].valueZ = static_cast<float>(number); break;
                    case 3: prop.keyframes[idxKey].valueY = static_cast<float>(number); break;
                }
                ++count;
            }
        }
        else if (rawline.rfind("locacion ", 0) == 0) {
            animIndex = BuscarAnimacionObj();
            if (animIndex < 0) {
                AnimationObject newAnim;
                AnimationObjects.push_back(std::move(newAnim));
                animIndex = (int)AnimationObjects.size() - 1;
            }
            AnimationObject& anim = AnimationObjects[animIndex];
            anim.obj = SelectActivo;

            propIndex = BuscarAnimProperty(animIndex, AnimPosition);
            if (propIndex < 0) {
                AnimProperty propNew;
                anim.Propertys.push_back(std::move(propNew));
                propIndex = (int)anim.Propertys.size() - 1;
            }
            AnimProperty& prop = anim.Propertys[propIndex];
            prop.Property = AnimPosition;

            std::istringstream iss(rawline.substr(9));
            int number;
            int count = 0;
            while (iss >> number && count < 2) {
                if (count == 0 && number > 0) {
                    prop.keyframes.reserve(number);
                }
                ++count;
            }
        }
        else if (rawline.rfind("l ", 0) == 0) {
            if (animIndex < 0 || propIndex < 0) continue;
            AnimationObject& anim = AnimationObjects[animIndex];
            AnimProperty& prop = anim.Propertys[propIndex];
            prop.keyframes.push_back(defaultKey);
            int idxKey = (int)prop.keyframes.size() - 1;

            std::istringstream iss(rawline.substr(2));
            int number;
            int count = 0;
            while (iss >> number && count < 4) {
                switch (count) {
                    case 0: prop.keyframes[idxKey].frame = number; break;
                    case 1: prop.keyframes[idxKey].valueX = static_cast<float>(number); break;
                    case 2: prop.keyframes[idxKey].valueY = static_cast<float>(number); break;
                    case 3: prop.keyframes[idxKey].valueZ = static_cast<float>(number); break;
                }
                ++count;
            }
        }
        else if (rawline.rfind("escala ", 0) == 0) {
            animIndex = BuscarAnimacionObj();
            if (animIndex < 0) {
                AnimationObject newAnim;
                AnimationObjects.push_back(std::move(newAnim));
                animIndex = (int)AnimationObjects.size() - 1;
            }
            AnimationObject& anim = AnimationObjects[animIndex];
            anim.obj = SelectActivo;

            propIndex = BuscarAnimProperty(animIndex, AnimScale);
            if (propIndex < 0) {
                AnimProperty propNew;
                anim.Propertys.push_back(std::move(propNew));
                propIndex = (int)anim.Propertys.size() - 1;
            }
            AnimProperty& prop = anim.Propertys[propIndex];
            prop.Property = AnimScale;

            std::istringstream iss(rawline.substr(7));
            int number;
            int count = 0;
            while (iss >> number && count < 2) {
                if (count == 0 && number > 0) {
                    prop.keyframes.reserve(number);
                }
                ++count;
            }
        }
        else if (rawline.rfind("s ", 0) == 0) {
            if (animIndex < 0 || propIndex < 0) continue;
            AnimationObject& anim = AnimationObjects[animIndex];
            AnimProperty& prop = anim.Propertys[propIndex];
            prop.keyframes.push_back(defaultKey);
            int idxKey = (int)prop.keyframes.size() - 1;

            std::istringstream iss(rawline.substr(2));
            int number;
            int count = 0;
            while (iss >> number && count < 4) {
                switch (count) {
                    case 0: prop.keyframes[idxKey].frame = number; break;
                    case 1: prop.keyframes[idxKey].valueX = static_cast<float>(number); break;
                    case 2: prop.keyframes[idxKey].valueY = static_cast<float>(number); break;
                    case 3: prop.keyframes[idxKey].valueZ = static_cast<float>(number); break;
                }
                ++count;
            }
        }
        // else: línea no reconocida -> ignorar
    }

    file.close();
    std::cout << "Animación importada correctamente desde " << filepath << std::endl;
    return true;*/
}