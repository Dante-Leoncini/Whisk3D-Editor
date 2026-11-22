// ============================================================================
//  PostProcessObj.cpp — ver PostProcessObj.h
// ============================================================================
#include "objects/PostProcessObj.h"

void (*g_postInTree)(PostProcess* p) = 0;

PostProcess::PostProcess(Object* parent, Vector3 pos) : Object(parent, "PostProcess", pos) {
    noTransform = true;   // (no se mueve: actua en su lugar del arbol, sin matriz propia)
    opt.on = true;
}

void PostProcess::RenderObject() {
    if (g_postInTree) g_postInTree(this);
}

void W3dPostProcessesOf(Object* root, std::vector<PostProcess*>& out) {
    if (!root || !root->visible) return;
    if (root->getType() == ObjectType::postProcess) out.push_back((PostProcess*)root);
    for (size_t i = 0; i < root->Childrens.size(); i++) W3dPostProcessesOf(root->Childrens[i], out);
}
