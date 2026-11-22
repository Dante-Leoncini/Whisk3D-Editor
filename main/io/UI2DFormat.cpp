// ============================================================================
//  UI2DFormat.cpp — ver UI2DFormat.h.
//
//  El JSON se escribe y se parsea A MANO (C++03, sin dependencias): el formato
//  es chico y controlado. El parser es un JSON minimo (objetos, listas, string,
//  numero, bool) que alcanza y sobra para estos archivos.
// ============================================================================
#include "io/UI2DFormat.h"
#include "io/JsonW3d.h"
#include "W3dPalettes.h"         // paletas del PROYECTO: se BAKEAN al guardar y se ADOPTAN al cargar
#include "script/W3dScript.h"    // script lua + refs expuestas (se guardan con el arbol)
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Element2D.h"
#include "objects/Text2D.h"
#include "objects/Image2D.h"
#include "objects/Rect2D.h"
#include "objects/Container2D.h"
#include "objects/Slice9.h"
#include "objects/Button2D.h"
#include "objects/Expand2D.h"
#include "objects/Video2D.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>

// FORMATO v4 (ver UI2DFormat.h). En el runtime de los juegos quedan asi para
// siempre: sin contenedor, las rutas se resuelven exactamente como siempre.
bool         g_w3dRefsEntries = false;
std::string  g_w3dDirProject;
W3dRefEmitFn g_w3dRefEmit    = NULL;
W3dRefExtFn  g_w3dRefExtMark = NULL;
W3dLoadPathFn g_w3dPathLibHook = NULL;   // (una escena UI de una libreria: ver UI2DFormat.h)

// ---------------------------------------------------------------------------
//  RUTAS relativas: si 'ruta' esta adentro de 'base' (la carpeta del .w3dui),
//  se guarda relativa; si no, absoluta tal cual.
//
//  'ruta' NO es const a proposito: guardando un proyecto v4, el hook g_w3dRefEmit
//  (el editor lo apunta al escritor del contenedor) mete el asset ADENTRO del
//  .w3d y deja el campo del widget con su NOMBRE DE ENTRADA. En el runtime de los
//  juegos el hook es NULL y esto se comporta como siempre.
// ---------------------------------------------------------------------------
// true si 'ruta' cuelga de la carpeta 'dir': el prefijo tiene que terminar EN un
// separador. Sin eso, una carpeta HERMANA con el mismo prefijo ("/home/d/proj-x"
// vs la base "/home/d/proj") daba una relativa corrupta ("-x/a.png") y el asset
// no se encontraba mas.
// ESTA es LA regla, para todo el arbol: SaveW3D.cpp tenia su propia copia (BajoCarpeta)
// que NO absorbia separadores repetidos ni cubria la base terminada en separador -con
// dir="/p" y ruta="/p//a.png" devolvia "/a.png", que al releerse es ABSOLUTA-, y de esa
// duplicacion ya habia salido una perdida de assets. Vive aca (y no en SaveW3D.cpp)
// porque este .cpp lo compila TAMBIEN el runtime de los juegos y SaveW3D.cpp no.
bool W3dPathUnderFolder(const std::string& path, const std::string& dir, std::string& rel) {
    if (dir.empty() || path.size() <= dir.size()) return false;
    if (path.compare(0, dir.size(), dir) != 0) return false;
    size_t i  = dir.size();
    char last = dir[dir.size() - 1];
    // la base puede ser una RAIZ y venir ya con el separador ("/" o "C:/"): ahi el
    // corte es justo el largo de la base
    if (last != '/' && last != '\\' && path[i] != '/' && path[i] != '\\') return false;
    while (i < path.size() && (path[i] == '/' || path[i] == '\\')) i++;
    if (i >= path.size()) return false;   // "base/" pelado: no hay nada relativo
    rel = path.substr(i);
    return true;
}
static std::string PathToSave(std::string& path, const std::string& base) {
    // v4: el asset se mete ADENTRO del contenedor y lo que va al archivo es su
    // nombre de ENTRADA (o "ext:..." si el usuario lo quiso afuera / no existe)
    if (g_w3dRefEmit) return g_w3dRefEmit(path);
    if (path.empty() || base.empty()) return path;
    std::string rel;
    if (W3dPathUnderFolder(path, base, rel)) return rel;
    return path;
}
static std::string PathOnLoad(const std::string& saved, const std::string& base) {
    if (saved.empty()) return saved;
    // CONTENIDO DE UNA LIBRERIA externa: la resuelve el lector del proyecto en el contexto de esa libreria
    if (g_w3dPathLibHook) return g_w3dPathLibHook(saved);
    // "ext:" = referencia EXTERNA deliberada: se saca el prefijo y se resuelve
    // contra la carpeta del PROYECTO (no la del .w3dui). Queda anotada para que
    // el proximo guardado la vuelva a escribir externa.
    if (saved.size() > 4 && saved.compare(0, 4, "ext:") == 0) {
        std::string r = saved.substr(4);
        std::string res = r;
        if (!(r[0] == '/' || (r.size() > 1 && r[1] == ':'))) {
            const std::string& b = g_w3dDirProject.empty() ? base : g_w3dDirProject;
            res = b.empty() ? r : (b + "/" + r);
        }
        if (g_w3dRefExtMark) g_w3dRefExtMark(res);
        return res;
    }
    // absoluta (unix "/x", windows "C:\") queda tal cual; el resto cuelga de la base
    if (saved[0] == '/' || (saved.size() > 1 && saved[1] == ':')) return saved;
    // v4: es un nombre de ENTRADA del contenedor. Se devuelve TAL CUAL: en memoria
    // la "ruta" de un asset interno ES su entrada y ReadFileBytes la resuelve por
    // el montaje (colgarla de la carpeta del .w3dui daria "scenes/textures/x.png").
    if (g_w3dRefsEntries) return saved;
    // base vacia (el .w3dui es un asset "pelado" sin carpeta, ej. Android): la ruta
    // relativa queda tal cual, SIN anteponer "/" (eso la volveria absoluta y fallaria)
    if (base.empty()) return saved;
    return base + "/" + saved;
}

// ---------------------------------------------------------------------------
//  ESCRITURA
// ---------------------------------------------------------------------------
static void JsonEscape(FILE* f, const std::string& s) {
    fputc('"', f);
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
        else if (c == '\n') fputs("\\n", f);
        else fputc(c, f);
    }
    fputc('"', f);
}
static void Indent(FILE* f, int n) { for (int i = 0; i < n; i++) fputs("  ", f); }
// NUMEROS CON ROUND-TRIP EXACTO: el "%g" pelado escribia 6 cifras significativas, o
// sea que TODO float del .w3dui (posiciones, tamanos, anclas, colores) se redondeaba
// al guardar y volvia distinto. El helper (JsonW3d.h, el mismo que usa el .w3d) escribe
// el minimo de digitos que RELEIDOS CON ESTE MISMO PARSER dan el mismo float32. Vale
// tambien para el runtime: el .w3dui es el formato que lee el juego compilado.
static void FieldF(FILE* f, int ind, const char* k, float v, bool comma = true) {
    Indent(f, ind); fprintf(f, "\"%s\": %s%s\n", k, JsonNumText(v).c_str(), comma ? "," : "");
}
static void FieldI(FILE* f, int ind, const char* k, int v, bool comma = true) {
    Indent(f, ind); fprintf(f, "\"%s\": %d%s\n", k, v, comma ? "," : "");
}
static void FieldB(FILE* f, int ind, const char* k, bool v, bool comma = true) {
    Indent(f, ind); fprintf(f, "\"%s\": %s%s\n", k, v ? "true" : "false", comma ? "," : "");
}
static void FieldS(FILE* f, int ind, const char* k, const std::string& v, bool comma = true) {
    Indent(f, ind); fprintf(f, "\"%s\": ", k); JsonEscape(f, v); fprintf(f, "%s\n", comma ? "," : "");
}
static void FieldColor(FILE* f, int ind, const char* k, const float* c, bool comma = true) {
    Indent(f, ind);
    fprintf(f, "\"%s\": [%s, %s, %s, %s]%s\n", k,
            JsonNumText(c[0]).c_str(), JsonNumText(c[1]).c_str(),
            JsonNumText(c[2]).c_str(), JsonNumText(c[3]).c_str(), comma ? "," : "");
}

static void WriteElement(FILE* f, Object* o, int ind, const std::string& base);

// los campos que COMPARTEN los elementos y el UI (layout de hijos + overflow)
// los SCRIPTS lua del objeto (puede tener varios) + lo asignado en el editor
static void WriteScript(FILE* f, int ind, Object* o, const std::string& base) {
    if (!o->scriptData || o->scriptData->scripts.empty()) return;
    Indent(f, ind); fputs("\"scripts\": [\n", f);
    for (size_t e = 0; e < o->scriptData->scripts.size(); e++) {
        W3dScriptEntry& ent = o->scriptData->scripts[e];   // NO const: ver PathToSave
        Indent(f, ind + 1); fputs("{ \"path\": ", f);
        JsonEscape(f, PathToSave(ent.path, base));
        if (ent.off) fputs(", \"off\": true", f);   // el ojo de la lista de scripts
        fputs(", \"refs\": {", f);
        for (size_t i = 0; i < ent.refs.size(); i++) {
            fprintf(f, "%s ", i ? "," : "");
            JsonEscape(f, ent.refs[i].first);
            fputs(": ", f);
            JsonEscape(f, ent.refs[i].second);
        }
        fprintf(f, " } }%s\n", (e + 1 < o->scriptData->scripts.size()) ? "," : "");
    }
    Indent(f, ind); fputs("],\n", f);
}

static void WriteFieldsChildren(FILE* f, int ind, float padLeft, float padRight,
                                float padArr, float padBottom, int layoutChildren,
                                int layoutFit, int layoutAlign, int distribution,
                                float gap,
                                bool padGapPx, bool cropsX, bool cropsY,
                                bool withScroll, float scrollX, float scrollY) {
    FieldF(f, ind, "padLeft", padLeft);
    FieldF(f, ind, "padRight", padRight);
    FieldF(f, ind, "padArr", padArr);
    FieldF(f, ind, "padBottom", padBottom);
    FieldI(f, ind, "layoutChildren", layoutChildren);    // 0 libre, 1 filas, 2 columnas
    FieldI(f, ind, "layoutFit", layoutFit);        // 0 estirar, 1 minimo
    FieldI(f, ind, "layoutAlign", layoutAlign);       // 0 inicio, 1 centro, 2 fin
    // reparto css del sobrante (solo con ajuste minimo): 0 gap, 1 between, 2 around,
    // 3 evenly. Los archivos viejos no lo traen -> 0 (comportamiento de siempre).
    FieldI(f, ind, "distribution", distribution);
    FieldF(f, ind, "gap", gap);
    FieldB(f, ind, "padGapPx", padGapPx);            // false: proporcional al lado menor
    FieldB(f, ind, "overflowX", cropsX);
    FieldB(f, ind, "overflowY", cropsY);
    FieldB(f, ind, "scroll", withScroll);
    FieldF(f, ind, "scrollX", scrollX);
    FieldF(f, ind, "scrollY", scrollY);
}

static void WriteChildren(FILE* f, Object* o, int ind, const std::string& base) {
    Indent(f, ind); fputs("\"children\": [\n", f);
    bool first = true;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        ObjectType t = h->getType();
        if (t != ObjectType::text2d && t != ObjectType::image2d &&
            t != ObjectType::rect2d && t != ObjectType::cont2d &&
            t != ObjectType::slice9 && t != ObjectType::button2d &&
            t != ObjectType::expand2d && t != ObjectType::video2d)
            continue;
        if (!first) fputs(",\n", f);
        first = false;
        WriteElement(f, h, ind + 1, base);
    }
    fputc('\n', f);
    Indent(f, ind); fputs("]\n", f);
}

static void WriteElement(FILE* f, Object* o, int ind, const std::string& base) {
    Element2D* e = (Element2D*)o;
    Indent(f, ind); fputs("{\n", f);
    int i2 = ind + 1;
    const char* type = "container";
    if (o->getType() == ObjectType::text2d)  type = "text";
    if (o->getType() == ObjectType::image2d) type = "image";
    if (o->getType() == ObjectType::rect2d)   type = "rect";
    if (o->getType() == ObjectType::slice9)   type = "slice9";
    if (o->getType() == ObjectType::button2d)  type = "button";
    if (o->getType() == ObjectType::expand2d) type = "expand";
    if (o->getType() == ObjectType::video2d)  type = "video";
    FieldS(f, i2, "type", type);
    FieldS(f, i2, "name", o->name);
    FieldB(f, i2, "visible", o->visible);
    Indent(f, i2); fprintf(f, "\"pos\": [%s, %s, %s],\n",
                            JsonNumText(o->pos.x).c_str(), JsonNumText(o->pos.y).c_str(),
                            JsonNumText(o->pos.z).c_str());
    FieldI(f, i2, "anchor", e->anchor);                // 0 centro, 1..4 bordes, 5..8 esquinas
    FieldF(f, i2, "rotation", e->rot2d);
    FieldF(f, i2, "opacity", e->opacity);
    FieldF(f, i2, "weight", e->weight);                  // reparto en filas/columnas del padre
    FieldF(f, i2, "width", e->width);
    FieldF(f, i2, "height", e->height);
    // la unidad del tamano: tamPx se sigue escribiendo por RETROCOMPAT (los editores
    // viejos leen px vs fraccion); sizeMode es la verdad (0 fraccion, 1 px, 2 escalado)
    // y si esta presente manda al cargar.
    // GARANTIA DE DIBUJO (fix "UI aplastada" 2026-08): la SEMANTICA de sizeMode no
    // cambio, pero ahora TODOS los caminos que dibujan (runtime w3drun, Editor 2D
    // y el HUD del viewport 3D del editor) mapean canvas->screen con UNA sola
    // escala para X e Y (lienzo = pantalla del juego, o letterbox min()): un
    // elemento de W x H px conserva SU proporcion en cualquier resolucion/aspecto,
    // y sizeMode 2 crece con min(w,h)/480 de la pantalla REAL (en apaisado, la
    // ALTURA / 480: mas resolucion = mas grande, nunca deformado, como en PS1).
    // El unico camino que estiraba el lienzo al viewport con factores distintos
    // por eje era Viewport3D::RenderUI; ver el bloque "PANTALLA DEL JUEGO" ahi.
    FieldB(f, i2, "sizePx", e->sizeMode == TAM2D_PX);
    FieldI(f, i2, "sizeMode", e->sizeMode);
    FieldB(f, i2, "expand", e->expand);          // absorbe el sobrante en la fila/columna
    // margen exterior (unidad: la del padre, px o proporcional como su gap)
    FieldF(f, i2, "marginLeft", e->marginLeft);
    FieldF(f, i2, "marginRight", e->marginRight);
    FieldF(f, i2, "marginTop", e->marginTop);
    FieldF(f, i2, "marginBottom", e->marginBottom);
    FieldB(f, i2, "marginAll", e->marginUni);            // el panel: un valor o por lado
    FieldB(f, i2, "padUni", e->padUni);
    // PALETA elegida por este elemento, POR NOMBRE (ausente/"" = hereda del
    // padre). Los indices pal* se resuelven contra la paleta EFECTIVA.
    if (!o->palette.empty()) FieldS(f, i2, "palette", o->palette);

    if (o->getType() == ObjectType::text2d) {
        Text2D* t = (Text2D*)o;
        FieldS(f, i2, "text", t->text);
        FieldF(f, i2, "size", t->size);
        FieldI(f, i2, "alignX", t->alignH);          // 0 izq, 1 centro, 2 der
        FieldI(f, i2, "alignY", t->alignV);
        FieldColor(f, i2, "color", t->color);
        FieldS(f, i2, "font", PathToSave(t->source, base));   // "" = la de Whisk3D
        // fuente BITMAP (png + json de glifos al lado): manda sobre "fuente" si carga.
        // En v4 el png entra al contenedor por PathToSave; el json HERMANO se
        // emite aparte (mismo nombre, .json) para que viaje junto: el lector lo deriva
        // de la ruta del png y lo encuentra como entrada.
        if (!t->fontBitmap.empty()) {
            // el hermano .json se deriva ANTES: PathToSave reescribe fontBitmap
            // en memoria con el nombre de ENTRADA del contenedor (v4) y el hermano de
            // una entrada ya ingerida no existe en disco (no se podria ingerir).
            size_t pt = t->fontBitmap.find_last_of('.');
            std::string js = (pt != std::string::npos) ? t->fontBitmap.substr(0, pt) + ".json"
                                                       : std::string();
            FieldS(f, i2, "bitmapFont", PathToSave(t->fontBitmap, base));
            if (!js.empty()) PathToSave(js, base);   // solo para INGERIRLO en el v4
        }
        FieldI(f, i2, "contentType", t->type);     // 0 string, 1 number, 2 float
        FieldF(f, i2, "decimals", t->decimals);
        FieldI(f, i2, "lines", t->lines);          // 0 una, 1 por palabras, 2 donde sea
        FieldB(f, i2, "autoSize", t->autoSize);
        FieldI(f, i2, "palColor", t->palColor);      // -1 = propio; sino indice de la paleta
    } else if (o->getType() == ObjectType::image2d) {
        Image2D* im = (Image2D*)o;
        FieldS(f, i2, "texture", PathToSave(im->texture, base));
        FieldI(f, i2, "mode", im->mode);             // 0 estirar, 1 ajustar, 2 cover
        FieldColor(f, i2, "tint", im->color);
        FieldB(f, i2, "useAlpha", im->useAlpha);
        if (im->mix != 1) FieldI(f, i2, "mix", im->mix);   // solo si no es la normal (alpha)
        FieldB(f, i2, "filtered", im->filtered);
        FieldI(f, i2, "palTint", im->palTint);
        if (im->IsFlipbook()) {   // flipbook: solo si anima (el resto de las imagenes queda igual)
            FieldI(f, i2, "flipFrames", im->flip.frames);
            FieldF(f, i2, "flipFps", im->flip.fps);
            FieldI(f, i2, "flipCols", im->flip.cols);
            FieldI(f, i2, "flipRows", im->flip.rows);
            FieldI(f, i2, "flipOffset", im->flipPlay.offset);
            // sub-tira en el atlas unico: solo si difiere de la textura entera
            if (im->flip.stripWidth != 1.0f) FieldF(f, i2, "flipWidth", im->flip.stripWidth);
            if (im->flip.stripHeight  != 1.0f) FieldF(f, i2, "flipHeight",  im->flip.stripHeight);
            if (im->flip.stripU0 != 0.0f)    FieldF(f, i2, "flipU0",    im->flip.stripU0);
            if (im->flip.stripV0 != 0.0f)    FieldF(f, i2, "flipV0",    im->flip.stripV0);
        }
        // imagen FIJA en el atlas unico: su recorte (solo si difiere de entera)
        if (im->uvRect[0] != 0.0f || im->uvRect[1] != 0.0f ||
            im->uvRect[2] != 1.0f || im->uvRect[3] != 1.0f) {
            FieldF(f, i2, "uvU0", im->uvRect[0]);
            FieldF(f, i2, "uvV0", im->uvRect[1]);
            FieldF(f, i2, "uvU1", im->uvRect[2]);
            FieldF(f, i2, "uvV1", im->uvRect[3]);
        }
    } else if (o->getType() == ObjectType::rect2d) {
        FieldColor(f, i2, "color", ((Rect2D*)o)->color);
        FieldI(f, i2, "palColor", ((Rect2D*)o)->palColor);
    } else if (o->getType() == ObjectType::slice9) {
        Slice9* s9 = (Slice9*)o;
        FieldS(f, i2, "texture", PathToSave(s9->texture, base));
        FieldF(f, i2, "borderX", s9->borderX);         // px del archivo
        FieldF(f, i2, "borderY", s9->borderY);
        FieldF(f, i2, "borderScale", s9->borderScale);
        FieldColor(f, i2, "tint", s9->color);
        FieldB(f, i2, "filtered", s9->filtered);
        FieldI(f, i2, "palTint", s9->palTint);
    } else if (o->getType() == ObjectType::video2d) {
        Video2D* vd = (Video2D*)o;
        FieldS(f, i2, "video", PathToSave(vd->video, base));
        FieldI(f, i2, "mode", vd->mode);             // 0 estirar, 1 ajustar, 2 cover
        FieldB(f, i2, "loop", vd->loop);
        FieldB(f, i2, "useAlpha", vd->useAlpha);
        FieldB(f, i2, "filtered", vd->filtered);
    } else if (o->getType() == ObjectType::button2d) {
        Button2D* b = (Button2D*)o;
        FieldS(f, i2, "text", b->text);
        FieldS(f, i2, "icon", PathToSave(b->icon, base));
        FieldS(f, i2, "font", PathToSave(b->source, base));
        FieldF(f, i2, "size", b->size);
        FieldF(f, i2, "pad", b->pad);
        FieldColor(f, i2, "backgroundColor", b->backgroundColor);
        FieldColor(f, i2, "textColor", b->textColor);
        FieldColor(f, i2, "borderColor", b->borderColor);
        FieldColor(f, i2, "colorHover", b->colorHover);   // borde de mouse-over (default verde accent)
        FieldI(f, i2, "palBackground", b->palBackground);
        FieldI(f, i2, "palText", b->palText);
        FieldI(f, i2, "palBorder", b->palBorder);
        FieldI(f, i2, "palHover", b->palHover);
        FieldS(f, i2, "textureBackground", PathToSave(b->textureBackground, base));
        FieldF(f, i2, "borderTexX", b->borderTexX);
        FieldF(f, i2, "borderTexY", b->borderTexY);
        FieldF(f, i2, "borderTexScale", b->borderScaleTex);
    }

    WriteScript(f, i2, o, base);
    WriteFieldsChildren(f, i2, e->padLeft, e->padRight, e->padArr, e->padBottom,
                        e->layoutChildren, e->layoutFit, e->layoutAlign,
                        e->distribution, e->gap, e->padGapPx,
                        e->cropsX, e->cropsY, e->withScroll, e->scrollX, e->scrollY);
    WriteChildren(f, o, i2, base);
    Indent(f, ind); fputc('}', f);
}

bool UI2DSave(UI* u, const std::string& path) {
    return UI2DSave(u, path, w3dFileSystem::ParentPath(path));
}

bool UI2DSave(UI* u, const std::string& path, const std::string& baseRel) {
    if (!u) return false;
    // ESCRITURA ATOMICA: primero a "<ruta>.tmp" (misma carpeta) y recien al final
    // el rename encima. Un fallo a mitad (disco lleno) ya no destruye la escena
    // anterior. Mismo patron que LuaCompilar y que el .w3d.
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { w3dLogfE("UI2D: no pude escribir %s", tmp.c_str()); return false; }
    std::string base = baseRel;

    fputs("{\n", f);
    FieldI(f, 1, "version", 1);
    FieldS(f, 1, "name", u->name);
    Indent(f, 1); fputs("\"window\": {\n", f);
    FieldB(f, 2, "sameAsRender", u->sameAsRender);
    FieldB(f, 2, "inTree", u->inTree);
    FieldF(f, 2, "width", u->width);
    FieldF(f, 2, "height", u->height);
    FieldF(f, 2, "globalScale", u->scaleGlobal);
    FieldB(f, 2, "equalScaleEditor", u->equalScaleEditor);
    FieldColor(f, 2, "color", u->color, false);
    Indent(f, 1); fputs("},\n", f);
    // las PALETAS: la fuente de verdad es el PROYECTO (W3dPalettes); se BAKEAN
    // aca para que el runtime standalone cargue el .w3dui SIN el .w3d y cada
    // escena sea autocontenida. Fallback: las locales del UI (guardado suelto
    // de una UI vieja sin proyecto con paletas).
    std::vector<Palette>& psProj = W3dPalettes();
    std::vector<Palette>& palettes = psProj.empty() ? u->palettes : psProj;
    // la SELECCION de la raiz, por nombre ("" = hereda/nada; se escribe siempre:
    // su presencia dice "formato nuevo, no derivar de activePalette")
    FieldS(f, 1, "palette", u->palette);
    // RETROCOMPAT lectores viejos: activePalette = el indice de esa seleccion
    // dentro de lo bakeado (o la activa legacy del UI si no hay seleccion)
    {
        int act = -1;
        for (size_t i = 0; i < palettes.size(); i++)
            if (!u->palette.empty() && palettes[i].name == u->palette) { act = (int)i; break; }
        if (act < 0 && u->palette.empty() &&
            u->activePalette >= 0 && u->activePalette < (int)palettes.size())
            act = u->activePalette;
        FieldI(f, 1, "activePalette", act);
    }
    Indent(f, 1); fputs("\"palettes\": [\n", f);
    for (size_t pIdx = 0; pIdx < palettes.size(); pIdx++) {
        Palette& pa = palettes[pIdx];
        Indent(f, 2); fputs("{ \"name\": ", f);
        JsonEscape(f, pa.name);
        fputs(", \"colors\": [\n", f);
        for (size_t i = 0; i < pa.colors.size(); i++) {
            Indent(f, 3);
            fputs("{ \"name\": ", f);
            JsonEscape(f, pa.colors[i].name);
            fprintf(f, ", \"color\": [%s, %s, %s, %s] }%s\n",
                    JsonNumText(pa.colors[i].rgba[0]).c_str(), JsonNumText(pa.colors[i].rgba[1]).c_str(),
                    JsonNumText(pa.colors[i].rgba[2]).c_str(), JsonNumText(pa.colors[i].rgba[3]).c_str(),
                    (i + 1 < pa.colors.size()) ? "," : "");
        }
        Indent(f, 2);
        fprintf(f, "] }%s\n", (pIdx + 1 < palettes.size()) ? "," : "");
    }
    Indent(f, 1); fputs("],\n", f);
    FieldB(f, 1, "padUni", u->padUni);
    WriteScript(f, 1, u, base);
    WriteFieldsChildren(f, 1, u->padLeft, u->padRight, u->padArr, u->padBottom,
                        u->layoutChildren, u->layoutFit, u->layoutAlign,
                        u->distribution, u->gap, u->padGapPx,
                        u->cropsX, u->cropsY, u->withScroll, u->scrollX, u->scrollY);
    WriteChildren(f, u, 1, base);
    fputs("}\n", f);
    if (fclose(f) != 0) {
        remove(tmp.c_str());
        w3dLogfE("UI2D: escritura incompleta de %s (queda la version anterior)", path.c_str());
        return false;
    }
    if (!w3dFileSystem::ReplaceFile(tmp, path)) {   // (tambien donde rename() no pisa: Windows, Symbian, la 3DS)
        remove(tmp.c_str());
        w3dLogfE("UI2D: no pude renombrar %s -> %s", tmp.c_str(), path.c_str());
        return false;
    }
    w3dLogf("UI2D: guardado %s", path.c_str());
    return true;
}

// el PARSER JSON ahora es compartido (io/JsonW3d.h): tambien lo usa el .w3d

// los refs {prop: valor} de un objeto json -> una entrada
static void ReadRefsOf(JVal* jrefs, W3dScriptEntry* ent) {
    if (!jrefs || jrefs->type != 4) return;
    for (std::map<std::string, JVal*>::iterator r = jrefs->obj.begin();
         r != jrefs->obj.end(); ++r)
        if (r->second->type == 2)
            ent->refs.push_back(std::make_pair(r->first, r->second->str));
}
// los scripts + lo asignado; tolerante: sin campos = sin scripts. Retrocompat: los
// archivos viejos con "script"/"scriptRefs" (uno solo) se leen como una entrada.
static void ReadScript(JVal* j, Object* o, const std::string& base) {
    std::map<std::string, JVal*>::iterator it = j->obj.find("scripts");
    if (it != j->obj.end() && it->second->type == 5) {
        for (size_t i = 0; i < it->second->list.size(); i++) {
            JVal* je = it->second->list[i];
            if (!je || je->type != 4) continue;
            W3dScriptEntry ent;
            ent.path = PathOnLoad(JS(je, "path", ""), base);
            if (ent.path.empty()) continue;
            ent.off = JB(je, "off", false);
            std::map<std::string, JVal*>::iterator jr = je->obj.find("refs");
            if (jr != je->obj.end()) ReadRefsOf(jr->second, &ent);
            if (!o->scriptData) o->scriptData = new W3dScriptData();
            o->scriptData->scripts.push_back(ent);
        }
        return;
    }
    std::string path = JS(j, "script", "");   // formato VIEJO (un solo script)
    if (path.empty()) return;
    W3dScriptEntry ent;
    ent.path = PathOnLoad(path, base);
    std::map<std::string, JVal*>::iterator jr = j->obj.find("scriptRefs");
    if (jr != j->obj.end()) ReadRefsOf(jr->second, &ent);
    if (!o->scriptData) o->scriptData = new W3dScriptData();
    o->scriptData->scripts.push_back(ent);
}

static void ReadFieldsChildren(JVal* j, float* padLeft, float* padRight, float* padArr,
                            float* padBottom, int* layoutChildren, int* layoutFit,
                            int* layoutAlign, int* distribution, float* gap,
                            bool* padGapPx, bool* cropsX, bool* cropsY,
                            bool* withScroll, float* scrollX, float* scrollY) {
    // "padding" viejo (uniforme) = los 4 lados; los campos nuevos lo pisan si vienen
    float unif = JF(j, "padding", -1.0f);
    if (unif >= 0.0f) { *padLeft = *padRight = *padArr = *padBottom = unif; }
    *padLeft   = JF(j, "padLeft", *padLeft);
    *padRight  = JF(j, "padRight", *padRight);
    *padArr    = JF(j, "padArr", *padArr);
    *padBottom = JF(j, "padBottom", *padBottom);
    *layoutChildren = JI(j, "layoutChildren", *layoutChildren);
    *layoutFit = JI(j, "layoutFit", *layoutFit);
    *layoutAlign = JI(j, "layoutAlign", *layoutAlign);
    // archivos viejos sin "distribucion" -> 0 (gap clasico), no cambia nada
    *distribution = JI(j, "distribution", *distribution);
    if (*distribution < 0 || *distribution > 3) *distribution = 0;
    *gap = JF(j, "gap", *gap);
    *padGapPx = JB(j, "padGapPx", *padGapPx);
    // "cropsX/cropsY" es el alias que usan algunos .w3dui escritos a mano (el
    // nombre del miembro); el canonico de guardado es "overflowX/overflowY". Se
    // aceptan los dos para no perder el dato al reabrir/re-guardar esos archivos.
    *cropsX = JB(j, "overflowX", JB(j, "clipX", *cropsX));
    *cropsY = JB(j, "overflowY", JB(j, "clipY", *cropsY));
    *withScroll = JB(j, "scroll", *withScroll);
    *scrollX = JF(j, "scrollX", *scrollX);
    *scrollY = JF(j, "scrollY", *scrollY);
}

static void LoadElement(JVal* j, Object* parent, const std::string& base);

static void LoadChildren(JVal* j, Object* parent, const std::string& base) {
    JVal* children = JChild(j, "children", 5);
    if (!children) return;
    for (size_t i = 0; i < children->list.size(); i++)
        if (children->list[i]->type == 4)
            LoadElement(children->list[i], parent, base);
}

static void LoadElement(JVal* j, Object* parent, const std::string& base) {
    std::string type = JS(j, "type", "container");
    Vector3 pos(0, 0, 0);
    { JVal* jp = JChild(j, "pos", 5);
      if (jp) {
          if (jp->list.size() > 0 && jp->list[0]->type == 1) pos.x = (float)jp->list[0]->num;
          if (jp->list.size() > 1 && jp->list[1]->type == 1) pos.y = (float)jp->list[1]->num;
          if (jp->list.size() > 2 && jp->list[2]->type == 1) pos.z = (float)jp->list[2]->num;
      } }

    Element2D* e = NULL;
    if (type == "text") {
        Text2D* t = new Text2D(parent, pos);
        t->text = JS(j, "text", t->text);
        t->size = JF(j, "size", t->size);
        t->alignH = JI(j, "alignX", t->alignH);
        t->alignV = JI(j, "alignY", t->alignV);
        JColor(j, "color", t->color);
        t->source = PathOnLoad(JS(j, "font", ""), base);
        if (JS(j, "font", "").empty()) t->source = "";   // "" = la fuente de Whisk3D
        t->fontBitmap = PathOnLoad(JS(j, "bitmapFont", ""), base);
        if (JS(j, "bitmapFont", "").empty()) t->fontBitmap = "";
        t->type = JI(j, "contentType", t->type);
        t->decimals = JF(j, "decimals", t->decimals);
        t->lines = JI(j, "lines", t->lines);
        t->autoSize = JB(j, "autoSize", t->autoSize);
        t->palColor = JI(j, "palColor", t->palColor);
        e = t;
    } else if (type == "image") {
        Image2D* im = new Image2D(parent, pos);
        im->texture = PathOnLoad(JS(j, "texture", ""), base);
        im->mode = JI(j, "mode", im->mode);
        JColor(j, "tint", im->color);
        im->useAlpha = JB(j, "useAlpha", im->useAlpha);
        im->mix = JI(j, "mix", im->mix);
        im->filtered = JB(j, "filtered", im->filtered);
        im->palTint  = JI(j, "palTint", im->palTint);
        int _fc = JI(j, "flipFrames", 0);
        if (_fc > 0)   // engancha el flipbook (configura el asset + registra el player en el motor)
            im->FlipbookConfig(_fc, JF(j, "flipFps", 30.0f),
                               JI(j, "flipCols", 1), JI(j, "flipRows", 1),
                               JI(j, "flipOffset", 0),
                               JF(j, "flipWidth", 1.0f), JF(j, "flipHeight", 1.0f),
                               JF(j, "flipU0", 0.0f), JF(j, "flipV0", 0.0f));
        // imagen FIJA en el atlas unico: su recorte (ausente = textura entera)
        im->uvRect[0] = JF(j, "uvU0", im->uvRect[0]);
        im->uvRect[1] = JF(j, "uvV0", im->uvRect[1]);
        im->uvRect[2] = JF(j, "uvU1", im->uvRect[2]);
        im->uvRect[3] = JF(j, "uvV1", im->uvRect[3]);
        e = im;
    } else if (type == "rect") {
        Rect2D* r = new Rect2D(parent, pos);
        JColor(j, "color", r->color);
        r->palColor = JI(j, "palColor", r->palColor);
        e = r;
    } else if (type == "button") {
        Button2D* b = new Button2D(parent, pos);
        b->text = JS(j, "text", b->text);
        b->icon = PathOnLoad(JS(j, "icon", ""), base);
        if (JS(j, "icon", "").empty()) b->icon = "";
        b->source = PathOnLoad(JS(j, "font", ""), base);
        if (JS(j, "font", "").empty()) b->source = "";
        b->size = JF(j, "size", b->size);
        b->pad  = JF(j, "pad", b->pad);
        JColor(j, "backgroundColor", b->backgroundColor);
        JColor(j, "textColor", b->textColor);
        JColor(j, "borderColor", b->borderColor);
        JColor(j, "colorHover", b->colorHover);   // opcional: sin el campo queda el verde accent
        b->palBackground = JI(j, "palBackground", b->palBackground);
        b->palText       = JI(j, "palText", b->palText);
        b->palBorder     = JI(j, "palBorder", b->palBorder);
        b->palHover      = JI(j, "palHover", b->palHover);   // archivos viejos: -1 (propio)
        b->textureBackground = PathOnLoad(JS(j, "textureBackground", ""), base);
        if (JS(j, "textureBackground", "").empty()) b->textureBackground = "";
        b->borderTexX = JF(j, "borderTexX", b->borderTexX);
        b->borderTexY = JF(j, "borderTexY", b->borderTexY);
        b->borderScaleTex = JF(j, "borderTexScale", b->borderScaleTex);
        e = b;
    } else if (type == "video") {
        Video2D* vd = new Video2D(parent, pos);
        vd->video = PathOnLoad(JS(j, "video", ""), base);
        if (JS(j, "video", "").empty()) vd->video = "";
        vd->mode = JI(j, "mode", vd->mode);
        vd->loop = JB(j, "loop", vd->loop);
        vd->useAlpha = JB(j, "useAlpha", vd->useAlpha);
        vd->filtered = JB(j, "filtered", vd->filtered);
        e = vd;
    } else if (type == "expand") {
        e = new Expand2D(parent, pos);
    } else if (type == "slice9") {
        Slice9* s9 = new Slice9(parent, pos);
        s9->texture = PathOnLoad(JS(j, "texture", ""), base);
        s9->borderX = JF(j, "borderX", s9->borderX);
        s9->borderY = JF(j, "borderY", s9->borderY);
        s9->borderScale = JF(j, "borderScale", s9->borderScale);
        JColor(j, "tint", s9->color);
        s9->filtered = JB(j, "filtered", s9->filtered);
        s9->palTint  = JI(j, "palTint", s9->palTint);
        e = s9;
    } else {
        e = new Container2D(parent, pos);
    }

    // CARGA: nombre CRUDO (el .w3dui puede traer duplicados). Al MERGEAR un .w3dui en
    // la escena (Add > Whisk3D UI) el caller repara con W3dNamesRepairScene, que
    // ademas arrastra las refs de los scripts.
    e->SetRawName(JS(j, "name", e->name));
    e->visible = JB(j, "visible", true);
    e->anchor = JI(j, "anchor", e->anchor);
    e->rot2d  = JF(j, "rotation", e->rot2d);
    e->opacity = JF(j, "opacity", e->opacity);
    e->weight = JF(j, "weight", e->weight);
    e->width = JF(j, "width", e->width);
    e->height = JF(j, "height", e->height);
    // unidad del tamano, con RETROCOMPAT total: el bool viejo tamPx se lee igual
    // (px o fraccion); si el archivo trae "sizeMode" (0 fraccion, 1 px, 2 escalado),
    // ese manda.
    e->sizeMode = JB(j, "sizePx", e->sizeMode == TAM2D_PX) ? TAM2D_PX : TAM2D_FRACTION;
    e->sizeMode = JI(j, "sizeMode", e->sizeMode);
    if (e->sizeMode < 0 || e->sizeMode > 2) e->sizeMode = TAM2D_PX;
    e->expand = JB(j, "expand", e->expand);
    e->marginLeft   = JF(j, "marginLeft", e->marginLeft);
    e->marginRight  = JF(j, "marginRight", e->marginRight);
    e->marginTop    = JF(j, "marginTop", e->marginTop);
    e->marginBottom = JF(j, "marginBottom", e->marginBottom);
    e->marginUni    = JB(j, "marginAll", e->marginUni);
    e->palette = JS(j, "palette", "");   // seleccion de paleta por NOMBRE ("" = hereda)
    ReadFieldsChildren(j, &e->padLeft, &e->padRight, &e->padArr, &e->padBottom,
                    &e->layoutChildren, &e->layoutFit, &e->layoutAlign,
                    &e->distribution, &e->gap, &e->padGapPx,
                    &e->cropsX, &e->cropsY, &e->withScroll, &e->scrollX, &e->scrollY);
    // archivos sin "padUni": si el padding por lado difiere, NO editar con un solo
    // valor (el sincronizador del panel pisaria los lados con el izquierdo)
    e->padUni = JB(j, "padUni", e->padLeft == e->padRight && e->padLeft == e->padArr &&
                                e->padLeft == e->padBottom);
    ReadScript(j, e, base);
    LoadChildren(j, e, base);
}

UI* UI2DLoad(const std::string& path) {
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(path, data) || data.empty()) {
        w3dLogfE("UI2D: no pude leer %s", path.c_str());
        return NULL;
    }
    JParser parser((const char*)&data[0], data.size());
    JVal* root = parser.Value();
    if (parser.error || root->type != 4) {
        w3dLogfE("UI2D: %s no parsea como JSON", path.c_str());
        delete root;
        return NULL;
    }
    std::string base = w3dFileSystem::ParentPath(path);

    // durante la carga los nombres entran crudos (ver SetRawName mas abajo)
    struct LoadGuard { bool prev; LoadGuard(){ prev = W3dNamesLoading; W3dNamesLoading = true; }
                        ~LoadGuard(){ W3dNamesLoading = prev; } } loadGuard;
    UI* u = new UI(NULL, Vector3(0, 0, 0));
    u->SetRawName(JS(root, "name", u->name));   // idem: crudo + reparacion del caller
    // recordar de QUE archivo vino (solo el nombre): SaveW3D re-escribe ese
    // mismo .w3dui hermano al guardar el proyecto (ver UI::fileW3dui)
    { size_t s = path.find_last_of("/\\");
      u->fileW3dui = (s == std::string::npos) ? path : path.substr(s + 1); }
    // las paletas guardadas REEMPLAZAN la default (si el archivo las trae)
    { JVal* jps = JChild(root, "palettes", 5);
      if (jps && !jps->list.empty()) {
          u->palettes.clear();
          u->activePalette = 0;
          for (size_t pIdx = 0; pIdx < jps->list.size() && pIdx < 8; pIdx++) {
              JVal* jp = jps->list[pIdx];
              if (jp->type != 4) continue;
              u->NewPalette(JS(jp, "name", "Palette"));
              u->Colors().clear();
              JVal* jc = JChild(jp, "colors", 5);
              if (jc)
                  for (size_t i = 0; i < jc->list.size() && i < 32; i++) {
                      JVal* e = jc->list[i];
                      if (e->type != 4) continue;
                      float c[4] = { 1, 1, 1, 1 };
                      JColor(e, "color", c);
                      u->AddPalette(JS(e, "name", "Color"), c);
                  }
          }
          u->activePalette = JI(root, "activePalette", 0);
          if (u->activePalette >= (int)u->palettes.size()) u->activePalette = 0;
      }
      // formato viejo: una sola "paleta" (una LISTA de colores; no confundir
      // con la "paleta" string nueva: la seleccion por nombre, ver abajo)
      JVal* jp = JChild(root, "palette", 5);
      if (jp && !jp->list.empty() && (!jps || jps->list.empty())) {
          u->palettes.clear();
          u->activePalette = 0;
          u->NewPalette("Whisk3D");
          u->Colors().clear();
          for (size_t i = 0; i < jp->list.size() && i < 32; i++) {
              JVal* e = jp->list[i];
              if (e->type != 4) continue;
              float c[4] = { 1, 1, 1, 1 };
              JColor(e, "color", c);
              u->AddPalette(JS(e, "name", "Color"), c);
          }
      } }
    // la SELECCION de paleta de la raiz, por NOMBRE (formato nuevo; el JChild
    // tipo 2 = string NO matchea la lista "paleta" del formato viejo). Si el
    // archivo no la trae, se DERIVA de la activePalette de siempre: la activa
    // legacy pasa a ser la seleccion explicita de la raiz (mismo resultado).
    {
        JVal* jsel = JChild(root, "palette", 2);
        if (jsel) u->palette = jsel->str;
        else if (u->activePalette >= 0 && u->activePalette < (int)u->palettes.size() &&
                 !u->palettes[u->activePalette].colors.empty())
            u->palette = u->palettes[u->activePalette].name;
    }
    // ADOPTAR las paletas bakeadas al PROYECTO (merge por nombre, la primera
    // gana): en el editor conviven con las del .w3d; en el runtime standalone
    // este es el unico camino que las carga. Ver W3dPalettes.h.
    W3dPalettesAdopt(u->palettes);
    JVal* v = JChild(root, "window", 4);
    if (v) {
        u->sameAsRender = JB(v, "sameAsRender", u->sameAsRender);
        u->inTree = JB(v, "inTree", false);   // (sin la clave: un proyecto de antes -> al final, como se veia)
        u->width = JF(v, "width", u->width);
        u->height = JF(v, "height", u->height);
        u->scaleGlobal = JF(v, "globalScale", u->scaleGlobal);
        u->equalScaleEditor = JB(v, "equalScaleEditor", u->equalScaleEditor);
        JColor(v, "color", u->color);
    }
    ReadFieldsChildren(root, &u->padLeft, &u->padRight, &u->padArr, &u->padBottom,
                    &u->layoutChildren, &u->layoutFit, &u->layoutAlign,
                    &u->distribution, &u->gap, &u->padGapPx,
                    &u->cropsX, &u->cropsY, &u->withScroll, &u->scrollX, &u->scrollY);
    // archivos sin "padUni": no pisar un padding por-lado con el sincronizador del panel
    u->padUni = JB(root, "padUni", u->padLeft == u->padRight && u->padLeft == u->padArr &&
                                   u->padLeft == u->padBottom);
    ReadScript(root, u, base);
    LoadChildren(root, u, base);
    delete root;
    w3dLogf("UI2D: cargado %s", path.c_str());
    return u;
}
