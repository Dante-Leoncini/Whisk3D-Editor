#include "Modifier.h"

// Establece el nombre del target
void Modifier::SetTarget(const std::string& NewValue) {
    targetName = NewValue;
}

// Comprueba si possibleAncestor es un ancestro de node
bool Modifier::IsMyAncestor(Object* node, Object* possibleAncestor) {
    while (node) {
        if (node == possibleAncestor) return true;
        node = node->Parent;
    }
    return false;
}

// Busca y asigna el target según targetName
void Modifier::ReloadTarget(Object* me) {
    // std::cout << "buscando target : " << targetName << std::endl;
    Object* FindTarget = FindObjectByName(SceneCollection, targetName);
    if (!FindTarget) return;

    // 1) Evitar apuntarse a sí mismo
    if (FindTarget == me) {
        std::cout << "ERROR: Target es el mismo objeto → prohibido\n";
        target = nullptr;
        return;
    }

    // 2) Evitar apuntar a ancestros
    if (IsMyAncestor(me, FindTarget)) {
        std::cout << "ERROR: Target es un ancestro → generaría recursion\n";
        target = nullptr;
        return;
    }

    // 3) Asignar correctamente
    target = FindTarget;
    // std::cout << "target seteado en: " << targetName << std::endl;
}