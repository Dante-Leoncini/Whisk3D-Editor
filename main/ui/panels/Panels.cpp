#include "ui/panels/Panels.h"
#include "objects/Fields.h"
#include "objects/Objects.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropColor.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"     // IconIndex: el icono de una opcion por su nombre
#include "io/ProjectChanges.h"      // W3dTouchedChanges
#include "config/W3dLang.h"
#include "base/w3dlog.h"
#include "objects/Textures.h"       // la fila IMAGEN: el archivo de la textura (si sigue cargada)
#include <string>
#include <string.h>
#include <stdlib.h>

extern const char* const kW3dPanelsText;               // ui/panels/PanelsText.cpp
extern bool g_redraw;
void PropsOpenMenuUnder(PopupMenu* menu, Button* button);   // ViewPorts/Properties.cpp
void PropsLayoutDirty();
void W3dPanelsImagePick(Object* o);   // ViewPorts/LayoutInput.cpp: la fila IMAGEN abre el selector para ese objeto

namespace {
enum { WhenNone, WhenEq, WhenNe };
// una fila: el control, el campo al que apunta y cuando se ve
struct PanelRow {
    PropertieBase* prop;
    const W3dField* field;
    int comp;                           // (vec3) que componente: 0 x, 1 y, 2 z
    const W3dField* whenField;
    int whenOp;
    float whenValue;
    std::vector<std::string> options;   // (choice) el texto de cada valor, ya traducido
    std::vector<int> icons;             // (choice) el icono de cada valor (-1 = sin)
    std::vector<int> order;             // (choice) en que orden van en el menu
};
struct PanelCard {
    GroupPropertie* group;
    int tab, objType;
    std::vector<PanelRow> rows;
};
}
struct W3dPanels { std::vector<PanelCard> cards; };

namespace {
std::vector<W3dPanels*> gAll;   // los de los paneles vivos (el desplegable busca ahi la fila de su boton)

// ---- EL TEXTO: una linea son palabras y "textos entre comillas"; '#' es un comentario ----
struct Words {
    std::vector<std::string> w;
    explicit Words(const std::string& line) {
        size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
            if (i >= line.size() || line[i] == '#') break;
            // (las comillas valen tambien adentro de una palabra: clave="con espacios")
            std::string t;
            bool quoted = false;
            while (i < line.size() && (quoted || (line[i] != ' ' && line[i] != '\t'))) {
                if (line[i] == '"') { quoted = !quoted; i++; continue; }
                t += line[i++];
            }
            w.push_back(t);
        }
    }
};
// el valor de 'clave=valor' entre las palabras desde 'from' ("" si no esta)
std::string Option(const Words& ws, size_t from, const char* key) {
    const size_t n = strlen(key);
    for (size_t i = from; i < ws.w.size(); i++)
        if (ws.w[i].size() > n && ws.w[i].compare(0, n, key) == 0 && ws.w[i][n] == '=') return ws.w[i].substr(n + 1);
    return std::string();
}
void Fail(int line, const char* what) { w3dLogfE("[tarjetas] linea %d del texto: %s", line, what); }
// when=<campo>==<n> | when=<campo>!=<n>
void ParseWhen(const std::string& s, PanelRow& r, int line) {
    size_t op = s.find("==");
    r.whenOp = WhenEq;
    if (op == std::string::npos) { op = s.find("!="); r.whenOp = WhenNe; }
    if (op != std::string::npos) r.whenField = W3dFieldFind(s.substr(0, op).c_str());
    if (op == std::string::npos || !r.whenField ||
        (r.whenField->kind != FieldInt && r.whenField->kind != FieldBool && r.whenField->kind != FieldFloat)) {
        r.whenOp = WhenNone; Fail(line, "when=<campo>==<n> o !=<n> (un tilde, un entero o un numero)"); return;
    }
    r.whenValue = (float)atof(s.c_str() + op + 2);
}

// ---- EL DESPLEGABLE de una fila 'choice': sus opciones; la elegida escribe su numero en el campo ----
PopupMenu* gChoiceMenu = 0;
const PanelRow* gChoiceRow = 0;   // la fila del desplegable abierto
void ChoicePicked(int id) {
    if (!gChoiceRow || !ActiveObj || id < 0 || id >= (int)gChoiceRow->options.size()) return;
    int* v = (int*)W3dFieldAt(gChoiceRow->field, ActiveObj);
    if (!v) return;
    *v = id;
    W3dTouchedChanges();   // (el proyecto tiene cambios sin guardar)
    // (el texto del boton y las filas que dependen del valor -when- cambian ya)
    for (size_t i = 0; i < gAll.size(); i++) W3dPanelsBind(gAll[i], ActiveObj);
    PropsLayoutDirty();
    g_redraw = true;
}
// LA FILA IMAGEN: el boton abre el selector de imagen para el objeto activo (ViewPorts/LayoutInput.cpp: el Vacio de la
// referencia). El selector es asincronico: la imagen llega despues y el objeto la retiene
void ImageOpen() { if (ActiveObj) W3dPanelsImagePick(ActiveObj); }
// el NOMBRE del archivo de la imagen ("" = no hay, o ya no esta cargada: no se toca un puntero viejo)
std::string ImageName(Texture* t) {
    if (!t) return std::string();
    bool alive = false;
    for (size_t i = 0; i < Textures.size() && !alive; i++) alive = (Textures[i] == t);
    if (!alive) return std::string();
    const size_t s = t->path.find_last_of("/\\");
    return s == std::string::npos ? t->path : t->path.substr(s + 1);
}
void ChoiceOpen() {
    PropButton* b = PropButton::running;
    gChoiceRow = 0;
    for (size_t i = 0; i < gAll.size() && !gChoiceRow; i++)
        for (size_t c = 0; c < gAll[i]->cards.size() && !gChoiceRow; c++)
            for (size_t r = 0; r < gAll[i]->cards[c].rows.size(); r++)
                if (gAll[i]->cards[c].rows[r].prop == b) { gChoiceRow = &gAll[i]->cards[c].rows[r]; break; }
    if (!gChoiceRow || !b) return;
    if (!gChoiceMenu) { gChoiceMenu = new PopupMenu(); gChoiceMenu->action = ChoicePicked; }
    gChoiceMenu->Clear();
    const int* now = ActiveObj ? (const int*)W3dFieldAt(gChoiceRow->field, ActiveObj) : 0;
    for (size_t k = 0; k < gChoiceRow->order.size(); k++) {
        const int i = gChoiceRow->order[k];
        gChoiceMenu->Add(gChoiceRow->options[(size_t)i], i, gChoiceRow->icons[(size_t)i])->green = (now && *now == i);
    }
    PropsOpenMenuUnder(gChoiceMenu, b->button);
}

// los pasos de un numero: steps=<fino>,<grueso>,<arrastre>
void Steps(PropFloat* f, const std::string& s) {
    if (s.empty()) return;
    float v[3] = { f->fineStep, f->thickStep, f->dragStep };
    const char* c = s.c_str();
    for (int i = 0; i < 3 && *c; i++) { v[i] = (float)strtod(c, (char**)&c); if (*c == ',') c++; }
    f->fineStep = v[0]; f->thickStep = v[1]; f->dragStep = v[2];
}
// un numero (o un componente de un vec3) con su unidad, su rango y sus pasos
PropFloat* NewFloat(const std::string& label, const Words& ws) {
    PropFloat* f = new PropFloat(label, Option(ws, 3, "unit"));
    const std::string mn = Option(ws, 3, "min"), mx = Option(ws, 3, "max");
    if (!mn.empty() || !mx.empty())
        f->SetRange(mn.empty() ? -1e30f : (float)atof(mn.c_str()), mx.empty() ? 1e30f : (float)atof(mx.c_str()));
    Steps(f, Option(ws, 3, "steps"));
    return f;
}
// una fila del texto -> su control (NULL si no corresponde al campo)
PropertieBase* NewRow(const Words& ws, PanelRow& r, int line) {
    const std::string& k = ws.w[0];
    const std::string label = T(ws.w[1].c_str());
    const W3dFieldKind kind = r.field->kind;
    if (k == "bool" && kind == FieldBool) return new PropBool(label);
    if (k == "image" && kind == FieldImage) {   // el nombre a la izquierda; el boton (el archivo, o "Cargar imagen") a la derecha
        PropButton* b = new PropButton(label);
        b->withLabel = true; b->inColumn = true;
        b->action = ImageOpen;
        return b;
    }
    if (k == "color" && kind == FieldColor) return new PropColor(label);
    if (k == "float" && kind == FieldFloat) return NewFloat(label, ws);
    if (k == "choice" && kind == FieldInt && ws.w.size() >= 4) {
        const std::string& o = ws.w[3];
        for (size_t a = 0; a <= o.size();) {
            size_t z = o.find('|', a);
            if (z == std::string::npos) z = o.size();
            std::string op = o.substr(a, z - a);
            const size_t colon = op.find(':');
            r.icons.push_back(colon == std::string::npos ? -1 : IconIndex(op.substr(colon + 1).c_str()));
            if (colon != std::string::npos) op = op.substr(0, colon);
            r.options.push_back(T(op.c_str()));
            a = z + 1;
        }
        const std::string ord = Option(ws, 4, "order");
        for (const char* c = ord.c_str(); *c;) {
            char* e = 0; const long i = strtol(c, &e, 10);
            if (e == c) break;
            if (i >= 0 && i < (long)r.options.size()) r.order.push_back((int)i);
            c = (*e == ',') ? e + 1 : e;
        }
        if (r.order.size() != r.options.size()) {   // (sin order, o uno incompleto: el de los numeros)
            r.order.clear();
            for (size_t i = 0; i < r.options.size(); i++) r.order.push_back((int)i);
        }
        PropButton* b = new PropButton(label);
        b->withLabel = true;   // el nombre a la izquierda, el desplegable a la derecha
        b->inColumn = true;    // (alineados con los numeros: la etiqueta en su columna, el desplegable en la de valores)
        b->button->dropdown = true;
        b->action = ChoiceOpen;
        return b;
    }
    Fail(line, "el control no corresponde al campo (bool/float/color/choice/image)");
    return 0;
}
}

W3dPanels* W3dPanelsBuild(std::vector<GroupPropertie*>& cards) {
    W3dPanels* p = new W3dPanels();
    const char* s = kW3dPanelsText;
    for (int line = 1; *s; line++) {
        const char* e = strchr(s, '\n');
        if (!e) e = s + strlen(s);
        const Words ws(std::string(s, e));
        s = *e ? e + 1 : e;
        if (ws.w.empty()) continue;
        if (ws.w[0] == "card") {
            PanelCard c;
            c.objType = (ws.w.size() >= 4) ? W3dObjectTypeOf(ws.w[2].c_str()) : -1;
            if (c.objType < 0) { Fail(line, "card <pestania> <tipo de objeto> \"<titulo>\""); continue; }
            c.tab = atoi(ws.w[1].c_str());
            c.group = new GroupPropertie(T(ws.w[3].c_str()));
            const std::string width = Option(ws, 4, "width");
            if (!width.empty()) c.group->valueWidth = (float)atof(width.c_str());
            // after=<titulo>: va despues de esa tarjeta (asi las tarjetas de un objeto quedan en su orden aunque unas sean
            // declaradas y otras no); sin after, al final
            const std::string after = Option(ws, 4, "after");
            size_t at = cards.size();
            if (!after.empty()) {
                const std::string t = T(after.c_str());
                for (size_t g = 0; g < cards.size(); g++) if (cards[g] && cards[g]->name == t) at = g + 1;
            }
            cards.insert(cards.begin() + (long)at, c.group);
            p->cards.push_back(c);
            continue;
        }
        if (p->cards.empty()) { Fail(line, "una fila antes de la primera card"); continue; }
        if (ws.w.size() < 3) { Fail(line, "<control> \"<etiqueta>\" <campo>"); continue; }
        PanelRow r;
        r.prop = 0; r.whenField = 0; r.whenOp = WhenNone; r.whenValue = 0.0f;
        r.field = W3dFieldFind(ws.w[2].c_str());
        if (!r.field) { Fail(line, "campo desconocido (ver objects/Fields.cpp)"); continue; }
        r.comp = 0;
        const std::string when = Option(ws, 3, "when");
        if (!when.empty()) ParseWhen(when, r, line);
        PanelCard& c = p->cards.back();
        if (ws.w[0] == "vec3") {   // tres numeros: "<etiqueta> X", "Y", "Z"
            if (r.field->kind != FieldVec3) { Fail(line, "vec3 de un campo que no es un Vector3"); continue; }
            static const char* const kAxis[3] = { "X", "Y", "Z" };
            const bool full = Option(ws, 3, "axes") == "full";   // (axes=full: la etiqueta entera en los tres)
            for (int a = 0; a < 3; a++) {
                r.comp = a;
                r.prop = NewFloat((a == 0 || full) ? std::string(T(ws.w[1].c_str())) + " " + kAxis[a] : std::string(kAxis[a]), ws);
                c.group->properties.push_back(r.prop);
                c.rows.push_back(r);
            }
            continue;
        }
        r.prop = NewRow(ws, r, line);
        if (!r.prop) continue;
        c.group->properties.push_back(r.prop);
        c.rows.push_back(r);
    }
    gAll.push_back(p);
    return p;
}

void W3dPanelsFree(W3dPanels* p) {
    if (!p) return;
    for (size_t i = 0; i < gAll.size(); i++) if (gAll[i] == p) { gAll.erase(gAll.begin() + (long)i); break; }
    for (size_t c = 0; c < p->cards.size(); c++)
        for (size_t r = 0; r < p->cards[c].rows.size(); r++)
            if (gChoiceRow == &p->cards[c].rows[r]) gChoiceRow = 0;
    delete p;
}

bool W3dPanelsBind(W3dPanels* p, Object* active) {
    if (!p) return false;
    bool changed = false;
    for (size_t c = 0; c < p->cards.size(); c++)
        for (size_t i = 0; i < p->cards[c].rows.size(); i++) {
            PanelRow& r = p->cards[c].rows[i];
            void* at = W3dFieldAt(r.field, active);
            if (at && r.whenOp != WhenNone) {
                const void* w = W3dFieldAt(r.whenField, active);
                const float v = !w ? 0.0f : r.whenField->kind == FieldBool ? (*(const bool*)w ? 1.0f : 0.0f)
                              : r.whenField->kind == FieldInt ? (float)*(const int*)w : *(const float*)w;
                if (!w || (r.whenOp == WhenEq) != (v == r.whenValue)) at = 0;
            }
            switch (r.field->kind) {
                case FieldBool:  { PropBool* b = (PropBool*)r.prop; changed |= !b->value != !at; b->value = (bool*)at; break; }
                case FieldFloat: { PropFloat* f = (PropFloat*)r.prop; changed |= !f->value != !at; f->value = (float*)at; break; }
                case FieldColor: { PropColor* k = (PropColor*)r.prop; changed |= !k->value != !at; k->value = (float*)at; break; }
                case FieldVec3:  { PropFloat* f = (PropFloat*)r.prop; changed |= !f->value != !at;
                                   f->value = at ? &((float*)at)[r.comp] : (float*)0; break; }
                case FieldImage: {
                    PropButton* b = (PropButton*)r.prop;
                    changed |= b->hidden != !at;
                    b->hidden = !at;
                    const std::string n = at ? ImageName(*(Texture**)at) : std::string();
                    const std::string txt = n.empty() ? std::string(T("Load image")) : n;
                    if (b->button->text != txt) { b->button->text = txt; changed = true; }
                    b->button->icon = -1;
                    break;
                }
                case FieldInt: {
                    PropButton* b = (PropButton*)r.prop;
                    changed |= b->hidden != !at;
                    b->hidden = !at;
                    const int v = at ? *(const int*)at : -1;
                    const bool ok = v >= 0 && v < (int)r.options.size();
                    b->button->text = ok ? r.options[(size_t)v] : std::string("?");
                    b->button->icon = ok ? r.icons[(size_t)v] : -1;
                    break;
                }
            }
        }
    return changed;
}

namespace {
// la tarjeta es de este objeto: su pestania, su tipo y al menos una fila con a que apuntar (la raiz de la escena es de
// tipo coleccion, pero ningun campo de una coleccion vale para ella)
bool CardFor(const PanelCard& c, int tab, Object* active) {
    if (!active || c.tab != tab || c.objType != (int)active->getType()) return false;
    for (size_t r = 0; r < c.rows.size(); r++) if (W3dFieldAt(c.rows[r].field, active)) return true;
    return false;
}
}

void W3dPanelsShow(W3dPanels* p, int tab, Object* active) {
    if (!p) return;
    for (size_t c = 0; c < p->cards.size(); c++) p->cards[c].group->visible = CardFor(p->cards[c], tab, active);
}

bool W3dPanelsHasCard(const W3dPanels* p, int tab, Object* active) {
    if (!p) return false;
    for (size_t c = 0; c < p->cards.size(); c++) if (CardFor(p->cards[c], tab, active)) return true;
    return false;
}
