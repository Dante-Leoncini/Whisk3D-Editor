// ============================================================================
//  PropsHitbox.cpp — la tarjeta "Hitbox" del panel de propiedades (pestania 2
//  del objeto Hitbox, objects/Hitbox.h). Vive aparte de Properties.cpp: el panel
//  solo la ARMA en su lugar (PropsHitboxBuild, en BuildGroups) y le pide
//  el bindeo por frame (PropsHitboxUpdate, en UpdateTabs).
//
//  Los numeros y checks bindean DIRECTO a los campos del hitbox activo (NULL
//  fuera de la pestania: la fila se oculta y Resize no toca punteros viejos).
//  Etiqueta y filtro son texto con commit EN VIVO: lo tipeado se escribe en el
//  hitbox a cada tecla (patron LOD / Mirror: sync por frame).
// ============================================================================
#include "ViewPorts/Properties.h"
#include "objects/Hitbox.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema

static W3dHitboxBase* HbActive() {
    return (ActiveObj && ActiveObj->getType() == ObjectType::hitbox) ? (W3dHitboxBase*)ActiveObj : NULL;
}

void PropsHitboxBuild(Properties* p) {
    if (!p) return;
    p->propHitbox = new GroupPropertie(T("Hitbox"));
    p->propHbActive = new PropBool(T("Active"));
    p->propHitbox->properties.push_back(p->propHbActive);
    const char* axes[3] = { "Size X", "Size Y", "Size Z" };
    for (int k = 0; k < 3; k++) {
        p->propHbSize[k] = new PropFloat(T(axes[k]), "m");
        p->propHbSize[k]->SetRange(0.0f, 100000.0f);
        p->propHbSize[k]->fineStep = 0.05f; p->propHbSize[k]->thickStep = 0.5f;
        p->propHitbox->properties.push_back(p->propHbSize[k]);
    }
    const char* center[3] = { "Center X", "Center Y", "Center Z" };
    for (int k = 0; k < 3; k++) {
        p->propHbCenter[k] = new PropFloat(T(center[k]), "m");
        p->propHbCenter[k]->SetRange(-100000.0f, 100000.0f);
        p->propHbCenter[k]->fineStep = 0.05f; p->propHbCenter[k]->thickStep = 0.5f;
        p->propHitbox->properties.push_back(p->propHbCenter[k]);
    }
    p->propHbLabel = new PropText(T("Tag"), "");
    p->propHitbox->properties.push_back(p->propHbLabel);
    p->propHbFilter = new PropText(T("Filter"), "");
    p->propHitbox->properties.push_back(p->propHbFilter);
    p->propHbBodies = new PropBool(T("Detect bodies"));
    p->propHitbox->properties.push_back(p->propHbBodies);
    p->GroupProperties.push_back(p->propHitbox);
}

// un campo de TEXTO del hitbox: lo tipeado se aplica en vivo; sin foco muestra el valor real
// (cambio de objeto, undo, un script que lo cambio)
static void SyncText(PropText* pt, std::string* destination, std::string& last) {
    if (!pt) return;
    const bool focus = TextFieldLive(&pt->field);   // (en vivo: un Esc re-escribe el texto de antes)
    if (focus && destination && pt->field.text != last) {
        *destination = pt->field.text;
        last = pt->field.text;
        g_redraw = true;
    }
    if (!focus) {
        last.clear();
        if (destination && pt->field.text != *destination) { pt->field.SetText(*destination); g_redraw = true; }
    }
}

void PropsHitboxUpdate(Properties* p, bool visible) {
    if (!p || !p->propHitbox) return;
    W3dHitboxBase* h = visible ? HbActive() : NULL;
    p->propHitbox->visible = (h != NULL);
    if (p->propHbActive)  p->propHbActive->value  = h ? &h->active : NULL;
    if (p->propHbBodies) p->propHbBodies->value = h ? &h->detectBodies : NULL;
    // Z-ARRIBA, como la tarjeta Transformar y Lua (desde el 2026-11-11): el campo "Y" es la z interna y el "Z" la y
    static const int kAxis[3] = { 0, 2, 1 };
    for (int k = 0; k < 3; k++) {
        if (p->propHbSize[k])    p->propHbSize[k]->value    = h ? &h->size[kAxis[k]] : NULL;
        if (p->propHbCenter[k]) p->propHbCenter[k]->value = h ? &h->center[kAxis[k]] : NULL;
    }
    if (p->propHbLabel) p->propHbLabel->hidden = (h == NULL);
    if (p->propHbFilter)   p->propHbFilter->hidden   = (h == NULL);
    static std::string lastLabel, lastFilter;
    SyncText(p->propHbLabel, h ? &h->label : NULL, lastLabel);
    SyncText(p->propHbFilter,   h ? &h->filter   : NULL, lastFilter);
}
