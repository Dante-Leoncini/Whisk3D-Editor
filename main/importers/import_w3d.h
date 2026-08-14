#ifndef IMPORTW3D_H
#define IMPORTW3D_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#include <map>
#include <string>
#include <GL/gl.h>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "objects/Objects.h"
#include "objects/Mirror.h"
#include "objects/Curve.h"
#include "animation/VertexAnimation.h"
#include "objects/Collection.h"
#include "controles.h"

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"

#include "importers/import_wobj.h"

// ----------------------------- helpers -----------------------------
float GetFloatOrDefault(const std::map<std::string,std::string>& props, const std::string& k, float def=0.0f);
int GetIntOrDefault(const std::map<std::string,std::string>& props, const std::string& k, int def=0);
GLenum GetLightIDOrDefault(std::string name, GLenum defaultLight = GL_LIGHT0);
std::string Unquote(const std::string& s);

// ----------------------------- Tokenizer -----------------------------
std::vector<std::string> Tokenize(const std::string& src);

// ----------------------------- Node & Find -----------------------------
struct Node {
    std::string type;
    std::map<std::string,std::string> props;
    std::vector<Node*> children;
};

Node* Find(Node* root, const std::string& type);
Node* ParseNode(std::vector<std::string>& tk, size_t& i);

ViewportBase* BuildLayout(Node* n);
void ApplyViewport3DProps(Viewport3D* v, const std::map<std::string,std::string>& p);
void ApplyCommonProps(Object* obj, const std::map<std::string,std::string>& p);
Object* CreateObjectFromNode(Node* n, Object* parent);
void BuildObjectRecursive(Node* n, Object* parent);
void BuildScene(Node* root);

// ----------------------------- Open W3D -----------------------------
// entrada UNICA de lectura de proyectos: detecta el formato POR CONTENIDO
// ('{' = JSON plano nuevo, 'PK' = zip v2 viejo, 'Whisk3D' = texto viejo) y
// rutea. Nunca crashea: cualquier problema deja aviso en el log y defaults.
void AbrirW3D(const std::string& ruta);
// compat: abre w3dPath (la global). Es AbrirW3D(w3dPath).
void OpenW3D();

// ----------------------------- Escena 3D del JUEGO -----------------------------
// LA ESCENA 3D EN UN JUEGO COMPILADO (la usa el runtime, w3drun): carga el
// proyecto.json que "Compilar juego" dejo al lado del binario CON EL MISMO lector
// que el editor, y despues resuelve targets/constraints y termina de cargar las
// texturas. Ver el comentario largo en import_w3d.cpp.
// Existe en los DOS builds (con y sin editor): el harness de pruebas lo usa para
// verificar que el juego lee exactamente lo mismo que el Play.
bool W3dProyectoCargarEscena3D(const void* datos, size_t n);

#endif