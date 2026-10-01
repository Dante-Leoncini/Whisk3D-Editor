// ============================================================================
//  PropsHitbox.cpp — la tarjeta "Hitbox" del panel de propiedades (pestania 2
//  del objeto Hitbox, objects/Hitbox.h). Vive aparte de Properties.cpp: el panel
//  solo la ARMA en su lugar (PropsHitboxConstruir, en ConstruirGrupos) y le pide
//  el bindeo por frame (PropsHitboxActualizar, en ActualizarPestanias).
//
//  Los numeros y checks bindean DIRECTO a los campos del hitbox activo (NULL
//  fuera de la pestania: la fila se oculta y Resize no toca punteros viejos).
//  Etiqueta y filtro son texto con commit EN VIVO: lo tipeado se escribe en el
//  hitbox a cada tecla (patron LOD / Mirror: sync por frame).
// ============================================================================
#include "ViewPorts/Properties.h"
#include "objects/Hitbox.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema

static W3dHitboxBase* HbActivo() {
    return (ObjActivo && ObjActivo->getType() == ObjectType::hitbox) ? (W3dHitboxBase*)ObjActivo : NULL;
}

void PropsHitboxConstruir(Properties* p) {
    if (!p) return;
    p->propHitbox = new GroupPropertie(T("Hitbox"));
    p->propHbActivo = new PropBool(T("Active"));
    p->propHitbox->properties.push_back(p->propHbActivo);
    const char* ejes[3] = { "Size X", "Size Y", "Size Z" };
    for (int k = 0; k < 3; k++) {
        p->propHbTam[k] = new PropFloat(T(ejes[k]), "m");
        p->propHbTam[k]->SetRango(0.0f, 100000.0f);
        p->propHbTam[k]->stepFino = 0.05f; p->propHbTam[k]->stepGrueso = 0.5f;
        p->propHitbox->properties.push_back(p->propHbTam[k]);
    }
    const char* cen[3] = { "Center X", "Center Y", "Center Z" };
    for (int k = 0; k < 3; k++) {
        p->propHbCentro[k] = new PropFloat(T(cen[k]), "m");
        p->propHbCentro[k]->SetRango(-100000.0f, 100000.0f);
        p->propHbCentro[k]->stepFino = 0.05f; p->propHbCentro[k]->stepGrueso = 0.5f;
        p->propHitbox->properties.push_back(p->propHbCentro[k]);
    }
    p->propHbEtiqueta = new PropText(T("Tag"), "");
    p->propHitbox->properties.push_back(p->propHbEtiqueta);
    p->propHbFiltro = new PropText(T("Filter"), "");
    p->propHitbox->properties.push_back(p->propHbFiltro);
    p->propHbCuerpos = new PropBool(T("Detect bodies"));
    p->propHitbox->properties.push_back(p->propHbCuerpos);
    p->GroupProperties.push_back(p->propHitbox);
}

// un campo de TEXTO del hitbox: lo tipeado se aplica en vivo; sin foco muestra el valor real
// (cambio de objeto, undo, un script que lo cambio)
static void SincronizarTexto(PropText* pt, std::string* destino, std::string& ultimo) {
    if (!pt) return;
    const bool foco = TextFieldEnVivo(&pt->field);   // (en vivo: un Esc re-escribe el texto de antes)
    if (foco && destino && pt->field.text != ultimo) {
        *destino = pt->field.text;
        ultimo = pt->field.text;
        g_redraw = true;
    }
    if (!foco) {
        ultimo.clear();
        if (destino && pt->field.text != *destino) { pt->field.SetText(*destino); g_redraw = true; }
    }
}

void PropsHitboxActualizar(Properties* p, bool visible) {
    if (!p || !p->propHitbox) return;
    W3dHitboxBase* h = visible ? HbActivo() : NULL;
    p->propHitbox->visible = (h != NULL);
    if (p->propHbActivo)  p->propHbActivo->value  = h ? &h->activo : NULL;
    if (p->propHbCuerpos) p->propHbCuerpos->value = h ? &h->detectarCuerpos : NULL;
    for (int k = 0; k < 3; k++) {
        if (p->propHbTam[k])    p->propHbTam[k]->value    = h ? &h->tam[k] : NULL;
        if (p->propHbCentro[k]) p->propHbCentro[k]->value = h ? &h->centro[k] : NULL;
    }
    if (p->propHbEtiqueta) p->propHbEtiqueta->oculto = (h == NULL);
    if (p->propHbFiltro)   p->propHbFiltro->oculto   = (h == NULL);
    static std::string ultEtiqueta, ultFiltro;
    SincronizarTexto(p->propHbEtiqueta, h ? &h->etiqueta : NULL, ultEtiqueta);
    SincronizarTexto(p->propHbFiltro,   h ? &h->filtro   : NULL, ultFiltro);
}
