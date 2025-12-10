#ifndef OBJECT_H
#define OBJECT_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "math/Quaternion.h"
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <functional>
#include <sstream>
#include <GL/gl.h>

#include "UI/icons.h"
#include "UI/text.h"
#include "variables.h"

class Viewport3D;
extern Viewport3D* Viewport3DActive;

enum class ObjectType {
    scene, mesh, camera, light, empty, armature, curve,
    collection, baseObject, mirror, gamepad, instance, constraint
};

// Forward declarations
class Object;
extern Object* SceneCollection;
extern std::vector<Object*> ObjSelects;
extern Object* CollectionActive;
extern Object* ObjActivo;

void DeseleccionarTodo(bool IncluirColecciones = false);

class Object {
    public:
        Object* Parent = nullptr;
        std::vector<Object*> Childrens;
        bool visible = true;
        bool desplegado = true;
        bool select = true;
        Text* name = nullptr;
        size_t IconType = 0;

        //GLfloat M[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        Matrix4 M;

        Vector3 pos;    
        Quaternion rot; // NUEVO: reemplazamos rotX/Y/Z por cuaternión
        Vector3 scale;

        virtual ObjectType getType() { return ObjectType::baseObject; }

        Object(
            Object* parent, 
            const std::string& nombre = "Objeto", 
            Vector3 pos = Vector3(0,0,0), 
            Vector3 Rot = Vector3(0.0f, 0.0f, 0.0f),
            Vector3 scale = Vector3(1.0f, 1.0f, 1.0f));
        virtual ~Object();

        void SetNameObj(const std::string& nombre);

        void EliminarObjetosSeleccionados(bool IncluirCollecciones = false);

        std::string SetName(const std::string& baseName);

        void Seleccionar();
        void Deseleccionar();
        void DeseleccionarCompleto(bool IncluirColecciones = false);
        bool EstaSeleccionado(bool IncluirColecciones = false);
        bool SeleccionarCompleto(bool IncluirColecciones = false);

        //todo lo nuevo para tranformaciones locales/globales y quaternion
        void GetMatrix(Matrix4& out) const;
        void RotateLocal(float pitch, float yaw, float roll);
        Vector3 GetGlobalPosition() const;

        virtual void Reload();
        void ReloadAll();

        virtual void RenderObject();
        void Render();
};

// Funciones auxiliares
Object* FindObjectByName(Object* node, const std::string& name);
bool DetectLoop(Object* node,
                std::unordered_set<Object*>& visited,
                std::unordered_set<Object*>& stack,
                int depth = 0);
void SearchLoop();
size_t GetIndexInParent(Object* obj);
Object* GetDeepestDFS(Object* node);
Object* GetPrevDFS(Object* current);
Object* GetNextDFS(Object* current);
bool IsSelectable(Object* obj, bool IncluirColecciones = false);

enum class SelectMode { NextSingle, PrevSingle, NextAdd, PrevAdd };
Object* GetFirstDFS();
void changeSelect(SelectMode mode, bool IncluirColecciones = false);

class SaveState {
    public:
        Object* obj;
        Vector3 pos;
        Quaternion rot;  
        Vector3 scale;
};
extern std::vector<SaveState> estadoObjetos;

void ApagarLucesHijas(Object* obj);
void SetDesplegado(bool desplegado);
void ChangeVisibilityObj();
void SeleccionarTodo(bool IncluirColecciones = false);
bool HayObjetosSeleccionados(bool IncluirColecciones = false);

#endif