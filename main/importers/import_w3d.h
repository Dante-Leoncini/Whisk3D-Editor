#ifndef IMPORTW3D_H
#define IMPORTW3D_H

#ifdef _WIN32
    #include <windows.h>
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
#include "objects/Constraint.h"
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
void OpenW3D();

#endif