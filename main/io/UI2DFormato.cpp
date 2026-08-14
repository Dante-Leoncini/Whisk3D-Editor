// ============================================================================
//  UI2DFormato.cpp — ver UI2DFormato.h.
//
//  El JSON se escribe y se parsea A MANO (C++03, sin dependencias): el formato
//  es chico y controlado. El parser es un JSON minimo (objetos, listas, string,
//  numero, bool) que alcanza y sobra para estos archivos.
// ============================================================================
#include "io/UI2DFormato.h"
#include "io/JsonW3d.h"
#include "W3dPaletas.h"          // paletas del PROYECTO: se BAKEAN al guardar y se ADOPTAN al cargar
#include "script/W3dScript.h"    // script lua + refs expuestas (se guardan con el arbol)
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Elemento2D.h"
#include "objects/Texto2D.h"
#include "objects/Imagen2D.h"
#include "objects/Rect2D.h"
#include "objects/Contenedor2D.h"
#include "objects/Slice9.h"
#include "objects/Boton2D.h"
#include "objects/Expandir2D.h"
#include "objects/Video2D.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>

// FORMATO v4 (ver UI2DFormato.h). En el runtime de los juegos quedan asi para
// siempre: sin contenedor, las rutas se resuelven exactamente como siempre.
bool         g_w3dRefsEntradas = false;
std::string  g_w3dDirProyecto;
W3dRefEmitFn g_w3dRefEmit      = NULL;
W3dRefExtFn  g_w3dRefExtMarcar = NULL;

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
// ESTA es LA regla, para todo el arbol: GuardarW3D.cpp tenia su propia copia (BajoCarpeta)
// que NO absorbia separadores repetidos ni cubria la base terminada en separador -con
// dir="/p" y ruta="/p//a.png" devolvia "/a.png", que al releerse es ABSOLUTA-, y de esa
// duplicacion ya habia salido una perdida de assets. Vive aca (y no en GuardarW3D.cpp)
// porque este .cpp lo compila TAMBIEN el runtime de los juegos y GuardarW3D.cpp no.
bool W3dRutaBajoCarpeta(const std::string& ruta, const std::string& dir, std::string& rel) {
    if (dir.empty() || ruta.size() <= dir.size()) return false;
    if (ruta.compare(0, dir.size(), dir) != 0) return false;
    size_t i = dir.size();
    char ult = dir[dir.size() - 1];
    // la base puede ser una RAIZ y venir ya con el separador ("/" o "C:/"): ahi el
    // corte es justo el largo de la base
    if (ult != '/' && ult != '\\' && ruta[i] != '/' && ruta[i] != '\\') return false;
    while (i < ruta.size() && (ruta[i] == '/' || ruta[i] == '\\')) i++;
    if (i >= ruta.size()) return false;   // "base/" pelado: no hay nada relativo
    rel = ruta.substr(i);
    return true;
}
static std::string RutaParaGuardar(std::string& ruta, const std::string& base) {
    // v4: el asset se mete ADENTRO del contenedor y lo que va al archivo es su
    // nombre de ENTRADA (o "ext:..." si el usuario lo quiso afuera / no existe)
    if (g_w3dRefEmit) return g_w3dRefEmit(ruta);
    if (ruta.empty() || base.empty()) return ruta;
    std::string rel;
    if (W3dRutaBajoCarpeta(ruta, base, rel)) return rel;
    return ruta;
}
static std::string RutaAlCargar(const std::string& guardada, const std::string& base) {
    if (guardada.empty()) return guardada;
    // "ext:" = referencia EXTERNA deliberada: se saca el prefijo y se resuelve
    // contra la carpeta del PROYECTO (no la del .w3dui). Queda anotada para que
    // el proximo guardado la vuelva a escribir externa.
    if (guardada.size() > 4 && guardada.compare(0, 4, "ext:") == 0) {
        std::string r = guardada.substr(4);
        std::string res = r;
        if (!(r[0] == '/' || (r.size() > 1 && r[1] == ':'))) {
            const std::string& b = g_w3dDirProyecto.empty() ? base : g_w3dDirProyecto;
            res = b.empty() ? r : (b + "/" + r);
        }
        if (g_w3dRefExtMarcar) g_w3dRefExtMarcar(res);
        return res;
    }
    // absoluta (unix "/x", windows "C:\") queda tal cual; el resto cuelga de la base
    if (guardada[0] == '/' || (guardada.size() > 1 && guardada[1] == ':')) return guardada;
    // v4: es un nombre de ENTRADA del contenedor. Se devuelve TAL CUAL: en memoria
    // la "ruta" de un asset interno ES su entrada y ReadFileBytes la resuelve por
    // el montaje (colgarla de la carpeta del .w3dui daria "escenas/texturas/x.png").
    if (g_w3dRefsEntradas) return guardada;
    // base vacia (el .w3dui es un asset "pelado" sin carpeta, ej. Android): la ruta
    // relativa queda tal cual, SIN anteponer "/" (eso la volveria absoluta y fallaria)
    if (base.empty()) return guardada;
    return base + "/" + guardada;
}

// ---------------------------------------------------------------------------
//  ESCRITURA
// ---------------------------------------------------------------------------
static void JsonEscapar(FILE* f, const std::string& s) {
    fputc('"', f);
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
        else if (c == '\n') fputs("\\n", f);
        else fputc(c, f);
    }
    fputc('"', f);
}
static void Sangria(FILE* f, int n) { for (int i = 0; i < n; i++) fputs("  ", f); }
// NUMEROS CON ROUND-TRIP EXACTO: el "%g" pelado escribia 6 cifras significativas, o
// sea que TODO float del .w3dui (posiciones, tamanos, anclas, colores) se redondeaba
// al guardar y volvia distinto. El helper (JsonW3d.h, el mismo que usa el .w3d) escribe
// el minimo de digitos que RELEIDOS CON ESTE MISMO PARSER dan el mismo float32. Vale
// tambien para el runtime: el .w3dui es el formato que lee el juego compilado.
static void CampoF(FILE* f, int ind, const char* k, float v, bool coma = true) {
    Sangria(f, ind); fprintf(f, "\"%s\": %s%s\n", k, JsonNumTexto(v).c_str(), coma ? "," : "");
}
static void CampoI(FILE* f, int ind, const char* k, int v, bool coma = true) {
    Sangria(f, ind); fprintf(f, "\"%s\": %d%s\n", k, v, coma ? "," : "");
}
static void CampoB(FILE* f, int ind, const char* k, bool v, bool coma = true) {
    Sangria(f, ind); fprintf(f, "\"%s\": %s%s\n", k, v ? "true" : "false", coma ? "," : "");
}
static void CampoS(FILE* f, int ind, const char* k, const std::string& v, bool coma = true) {
    Sangria(f, ind); fprintf(f, "\"%s\": ", k); JsonEscapar(f, v); fprintf(f, "%s\n", coma ? "," : "");
}
static void CampoColor(FILE* f, int ind, const char* k, const float* c, bool coma = true) {
    Sangria(f, ind);
    fprintf(f, "\"%s\": [%s, %s, %s, %s]%s\n", k,
            JsonNumTexto(c[0]).c_str(), JsonNumTexto(c[1]).c_str(),
            JsonNumTexto(c[2]).c_str(), JsonNumTexto(c[3]).c_str(), coma ? "," : "");
}

static void EscribirElemento(FILE* f, Object* o, int ind, const std::string& base);

// los campos que COMPARTEN los elementos y el UI (layout de hijos + overflow)
// los SCRIPTS lua del objeto (puede tener varios) + lo asignado en el editor
static void EscribirScript(FILE* f, int ind, Object* o, const std::string& base) {
    if (!o->scriptDatos || o->scriptDatos->scripts.empty()) return;
    Sangria(f, ind); fputs("\"scripts\": [\n", f);
    for (size_t e = 0; e < o->scriptDatos->scripts.size(); e++) {
        W3dScriptEntrada& ent = o->scriptDatos->scripts[e];   // NO const: ver RutaParaGuardar
        Sangria(f, ind + 1); fputs("{ \"ruta\": ", f);
        JsonEscapar(f, RutaParaGuardar(ent.ruta, base));
        fputs(", \"refs\": {", f);
        for (size_t i = 0; i < ent.refs.size(); i++) {
            fprintf(f, "%s ", i ? "," : "");
            JsonEscapar(f, ent.refs[i].first);
            fputs(": ", f);
            JsonEscapar(f, ent.refs[i].second);
        }
        fprintf(f, " } }%s\n", (e + 1 < o->scriptDatos->scripts.size()) ? "," : "");
    }
    Sangria(f, ind); fputs("],\n", f);
}

static void EscribirCamposHijos(FILE* f, int ind, float padIzq, float padDer,
                                float padArr, float padAba, int layoutHijos,
                                int layoutAjuste, int layoutAlign, int distribucion,
                                float gap,
                                bool padGapPx, bool recortaX, bool recortaY,
                                bool conScroll, float scrollX, float scrollY) {
    CampoF(f, ind, "padIzq", padIzq);
    CampoF(f, ind, "padDer", padDer);
    CampoF(f, ind, "padArr", padArr);
    CampoF(f, ind, "padAba", padAba);
    CampoI(f, ind, "layoutHijos", layoutHijos);      // 0 libre, 1 filas, 2 columnas
    CampoI(f, ind, "layoutAjuste", layoutAjuste);    // 0 estirar, 1 minimo
    CampoI(f, ind, "layoutAlign", layoutAlign);      // 0 inicio, 1 centro, 2 fin
    // reparto css del sobrante (solo con ajuste minimo): 0 gap, 1 between, 2 around,
    // 3 evenly. Los archivos viejos no lo traen -> 0 (comportamiento de siempre).
    CampoI(f, ind, "distribucion", distribucion);
    CampoF(f, ind, "gap", gap);
    CampoB(f, ind, "padGapPx", padGapPx);            // false: proporcional al lado menor
    CampoB(f, ind, "overflowX", recortaX);
    CampoB(f, ind, "overflowY", recortaY);
    CampoB(f, ind, "scroll", conScroll);
    CampoF(f, ind, "scrollX", scrollX);
    CampoF(f, ind, "scrollY", scrollY);
}

static void EscribirHijos(FILE* f, Object* o, int ind, const std::string& base) {
    Sangria(f, ind); fputs("\"hijos\": [\n", f);
    bool primero = true;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        ObjectType t = h->getType();
        if (t != ObjectType::texto2d && t != ObjectType::imagen2d &&
            t != ObjectType::rect2d && t != ObjectType::cont2d &&
            t != ObjectType::slice9 && t != ObjectType::boton2d &&
            t != ObjectType::expandir2d && t != ObjectType::video2d)
            continue;
        if (!primero) fputs(",\n", f);
        primero = false;
        EscribirElemento(f, h, ind + 1, base);
    }
    fputc('\n', f);
    Sangria(f, ind); fputs("]\n", f);
}

static void EscribirElemento(FILE* f, Object* o, int ind, const std::string& base) {
    Elemento2D* e = (Elemento2D*)o;
    Sangria(f, ind); fputs("{\n", f);
    int i2 = ind + 1;
    const char* tipo = "contenedor";
    if (o->getType() == ObjectType::texto2d)  tipo = "texto";
    if (o->getType() == ObjectType::imagen2d) tipo = "imagen";
    if (o->getType() == ObjectType::rect2d)   tipo = "rect";
    if (o->getType() == ObjectType::slice9)   tipo = "slice9";
    if (o->getType() == ObjectType::boton2d)  tipo = "boton";
    if (o->getType() == ObjectType::expandir2d) tipo = "expandir";
    if (o->getType() == ObjectType::video2d)  tipo = "video";
    CampoS(f, i2, "tipo", tipo);
    CampoS(f, i2, "nombre", o->name);
    CampoB(f, i2, "visible", o->visible);
    Sangria(f, i2); fprintf(f, "\"pos\": [%s, %s, %s],\n",
                            JsonNumTexto(o->pos.x).c_str(), JsonNumTexto(o->pos.y).c_str(),
                            JsonNumTexto(o->pos.z).c_str());
    CampoI(f, i2, "ancla", e->ancla);                // 0 centro, 1..4 bordes, 5..8 esquinas
    CampoF(f, i2, "rotacion", e->rot2d);
    CampoF(f, i2, "opacidad", e->opacidad);
    CampoF(f, i2, "peso", e->peso);                  // reparto en filas/columnas del padre
    CampoF(f, i2, "ancho", e->ancho);
    CampoF(f, i2, "alto", e->alto);
    // la unidad del tamano: tamPx se sigue escribiendo por RETROCOMPAT (los editores
    // viejos leen px vs fraccion); tamModo es la verdad (0 fraccion, 1 px, 2 escalado)
    // y si esta presente manda al cargar.
    // GARANTIA DE DIBUJO (fix "UI aplastada" 2026-08): la SEMANTICA de tamModo no
    // cambio, pero ahora TODOS los caminos que dibujan (runtime w3drun, Editor 2D
    // y el HUD del viewport 3D del editor) mapean lienzo->pantalla con UNA sola
    // escala para X e Y (lienzo = pantalla del juego, o letterbox min()): un
    // elemento de W x H px conserva SU proporcion en cualquier resolucion/aspecto,
    // y tamModo 2 crece con min(w,h)/480 de la pantalla REAL (en apaisado, la
    // ALTURA / 480: mas resolucion = mas grande, nunca deformado, como en PS1).
    // El unico camino que estiraba el lienzo al viewport con factores distintos
    // por eje era Viewport3D::RenderUI; ver el bloque "PANTALLA DEL JUEGO" ahi.
    CampoB(f, i2, "tamPx", e->tamModo == TAM2D_PX);
    CampoI(f, i2, "tamModo", e->tamModo);
    CampoB(f, i2, "expandir", e->expandir);          // absorbe el sobrante en la fila/columna
    // margen exterior (unidad: la del padre, px o proporcional como su gap)
    CampoF(f, i2, "margIzq", e->margIzq);
    CampoF(f, i2, "margDer", e->margDer);
    CampoF(f, i2, "margArr", e->margArr);
    CampoF(f, i2, "margAba", e->margAba);
    CampoB(f, i2, "margUni", e->margUni);            // el panel: un valor o por lado
    CampoB(f, i2, "padUni", e->padUni);
    // PALETA elegida por este elemento, POR NOMBRE (ausente/"" = hereda del
    // padre). Los indices pal* se resuelven contra la paleta EFECTIVA.
    if (!o->paleta.empty()) CampoS(f, i2, "paleta", o->paleta);

    if (o->getType() == ObjectType::texto2d) {
        Texto2D* t = (Texto2D*)o;
        CampoS(f, i2, "texto", t->texto);
        CampoF(f, i2, "tam", t->tam);
        CampoI(f, i2, "alignX", t->alignH);          // 0 izq, 1 centro, 2 der
        CampoI(f, i2, "alignY", t->alignV);
        CampoColor(f, i2, "color", t->color);
        CampoS(f, i2, "fuente", RutaParaGuardar(t->fuente, base));   // "" = la de Whisk3D
        // fuente BITMAP (png + json de glifos al lado): manda sobre "fuente" si carga.
        // En v4 el png entra al contenedor por RutaParaGuardar; el json HERMANO se
        // emite aparte (mismo nombre, .json) para que viaje junto: el lector lo deriva
        // de la ruta del png y lo encuentra como entrada.
        if (!t->fuenteBitmap.empty()) {
            // el hermano .json se deriva ANTES: RutaParaGuardar reescribe fuenteBitmap
            // en memoria con el nombre de ENTRADA del contenedor (v4) y el hermano de
            // una entrada ya ingerida no existe en disco (no se podria ingerir).
            size_t pt = t->fuenteBitmap.find_last_of('.');
            std::string js = (pt != std::string::npos) ? t->fuenteBitmap.substr(0, pt) + ".json"
                                                       : std::string();
            CampoS(f, i2, "fuenteBitmap", RutaParaGuardar(t->fuenteBitmap, base));
            if (!js.empty()) RutaParaGuardar(js, base);   // solo para INGERIRLO en el v4
        }
        CampoI(f, i2, "tipoContenido", t->tipo);     // 0 string, 1 number, 2 float
        CampoF(f, i2, "decimales", t->decimales);
        CampoI(f, i2, "lineas", t->lineas);          // 0 una, 1 por palabras, 2 donde sea
        CampoB(f, i2, "autoTam", t->autoTam);
        CampoI(f, i2, "palColor", t->palColor);      // -1 = propio; sino indice de la paleta
    } else if (o->getType() == ObjectType::imagen2d) {
        Imagen2D* im = (Imagen2D*)o;
        CampoS(f, i2, "textura", RutaParaGuardar(im->textura, base));
        CampoI(f, i2, "modo", im->modo);             // 0 estirar, 1 ajustar, 2 cover
        CampoColor(f, i2, "tinte", im->color);
        CampoB(f, i2, "usarAlpha", im->usarAlpha);
        CampoB(f, i2, "filtrado", im->filtrado);
        CampoI(f, i2, "palTinte", im->palTinte);
    } else if (o->getType() == ObjectType::rect2d) {
        CampoColor(f, i2, "color", ((Rect2D*)o)->color);
        CampoI(f, i2, "palColor", ((Rect2D*)o)->palColor);
    } else if (o->getType() == ObjectType::slice9) {
        Slice9* s9 = (Slice9*)o;
        CampoS(f, i2, "textura", RutaParaGuardar(s9->textura, base));
        CampoF(f, i2, "bordeX", s9->bordeX);         // px del archivo
        CampoF(f, i2, "bordeY", s9->bordeY);
        CampoF(f, i2, "escalaBorde", s9->escalaBorde);
        CampoColor(f, i2, "tinte", s9->color);
        CampoB(f, i2, "filtrado", s9->filtrado);
        CampoI(f, i2, "palTinte", s9->palTinte);
    } else if (o->getType() == ObjectType::video2d) {
        Video2D* vd = (Video2D*)o;
        CampoS(f, i2, "video", RutaParaGuardar(vd->video, base));
        CampoI(f, i2, "modo", vd->modo);             // 0 estirar, 1 ajustar, 2 cover
        CampoB(f, i2, "loop", vd->loop);
        CampoB(f, i2, "usarAlpha", vd->usarAlpha);
        CampoB(f, i2, "filtrado", vd->filtrado);
    } else if (o->getType() == ObjectType::boton2d) {
        Boton2D* b = (Boton2D*)o;
        CampoS(f, i2, "texto", b->texto);
        CampoS(f, i2, "icono", RutaParaGuardar(b->icono, base));
        CampoS(f, i2, "fuente", RutaParaGuardar(b->fuente, base));
        CampoF(f, i2, "tam", b->tam);
        CampoF(f, i2, "pad", b->pad);
        CampoColor(f, i2, "colorFondo", b->colorFondo);
        CampoColor(f, i2, "colorTexto", b->colorTexto);
        CampoColor(f, i2, "colorBorde", b->colorBorde);
        CampoColor(f, i2, "colorHover", b->colorHover);   // borde de mouse-over (default verde accent)
        CampoI(f, i2, "palFondo", b->palFondo);
        CampoI(f, i2, "palTexto", b->palTexto);
        CampoI(f, i2, "palBorde", b->palBorde);
        CampoI(f, i2, "palHover", b->palHover);
        CampoS(f, i2, "texturaFondo", RutaParaGuardar(b->texturaFondo, base));
        CampoF(f, i2, "bordeTexX", b->bordeTexX);
        CampoF(f, i2, "bordeTexY", b->bordeTexY);
        CampoF(f, i2, "escalaBordeTex", b->escalaBordeTex);
    }

    EscribirScript(f, i2, o, base);
    EscribirCamposHijos(f, i2, e->padIzq, e->padDer, e->padArr, e->padAba,
                        e->layoutHijos, e->layoutAjuste, e->layoutAlign,
                        e->distribucion, e->gap, e->padGapPx,
                        e->recortaX, e->recortaY, e->conScroll, e->scrollX, e->scrollY);
    EscribirHijos(f, o, i2, base);
    Sangria(f, ind); fputc('}', f);
}

bool UI2DGuardar(UI* u, const std::string& ruta) {
    return UI2DGuardar(u, ruta, w3dFileSystem::ParentPath(ruta));
}

bool UI2DGuardar(UI* u, const std::string& ruta, const std::string& baseRel) {
    if (!u) return false;
    // ESCRITURA ATOMICA: primero a "<ruta>.tmp" (misma carpeta) y recien al final
    // el rename encima. Un fallo a mitad (disco lleno) ya no destruye la escena
    // anterior. Mismo patron que LuaCompilar y que el .w3d.
    std::string tmp = ruta + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { w3dLogfE("UI2D: no pude escribir %s", tmp.c_str()); return false; }
    std::string base = baseRel;

    fputs("{\n", f);
    CampoI(f, 1, "version", 1);
    CampoS(f, 1, "nombre", u->name);
    Sangria(f, 1); fputs("\"ventana\": {\n", f);
    CampoB(f, 2, "igualQueRender", u->igualQueRender);
    CampoF(f, 2, "ancho", u->ancho);
    CampoF(f, 2, "alto", u->alto);
    CampoF(f, 2, "escalaGlobal", u->escalaGlobal);
    CampoColor(f, 2, "color", u->color, false);
    Sangria(f, 1); fputs("},\n", f);
    // las PALETAS: la fuente de verdad es el PROYECTO (W3dPaletas); se BAKEAN
    // aca para que el runtime standalone cargue el .w3dui SIN el .w3d y cada
    // escena sea autocontenida. Fallback: las locales del UI (guardado suelto
    // de una UI vieja sin proyecto con paletas).
    std::vector<Paleta>& psProy = W3dPaletas();
    std::vector<Paleta>& paletas = psProy.empty() ? u->paletas : psProy;
    // la SELECCION de la raiz, por nombre ("" = hereda/nada; se escribe siempre:
    // su presencia dice "formato nuevo, no derivar de paletaActiva")
    CampoS(f, 1, "paleta", u->paleta);
    // RETROCOMPAT lectores viejos: paletaActiva = el indice de esa seleccion
    // dentro de lo bakeado (o la activa legacy del UI si no hay seleccion)
    {
        int act = -1;
        for (size_t i = 0; i < paletas.size(); i++)
            if (!u->paleta.empty() && paletas[i].nombre == u->paleta) { act = (int)i; break; }
        if (act < 0 && u->paleta.empty() &&
            u->paletaActiva >= 0 && u->paletaActiva < (int)paletas.size())
            act = u->paletaActiva;
        CampoI(f, 1, "paletaActiva", act);
    }
    Sangria(f, 1); fputs("\"paletas\": [\n", f);
    for (size_t pIdx = 0; pIdx < paletas.size(); pIdx++) {
        Paleta& pa = paletas[pIdx];
        Sangria(f, 2); fputs("{ \"nombre\": ", f);
        JsonEscapar(f, pa.nombre);
        fputs(", \"colores\": [\n", f);
        for (size_t i = 0; i < pa.colores.size(); i++) {
            Sangria(f, 3);
            fputs("{ \"nombre\": ", f);
            JsonEscapar(f, pa.colores[i].nombre);
            fprintf(f, ", \"color\": [%s, %s, %s, %s] }%s\n",
                    JsonNumTexto(pa.colores[i].rgba[0]).c_str(), JsonNumTexto(pa.colores[i].rgba[1]).c_str(),
                    JsonNumTexto(pa.colores[i].rgba[2]).c_str(), JsonNumTexto(pa.colores[i].rgba[3]).c_str(),
                    (i + 1 < pa.colores.size()) ? "," : "");
        }
        Sangria(f, 2);
        fprintf(f, "] }%s\n", (pIdx + 1 < paletas.size()) ? "," : "");
    }
    Sangria(f, 1); fputs("],\n", f);
    CampoB(f, 1, "padUni", u->padUni);
    EscribirScript(f, 1, u, base);
    EscribirCamposHijos(f, 1, u->padIzq, u->padDer, u->padArr, u->padAba,
                        u->layoutHijos, u->layoutAjuste, u->layoutAlign,
                        u->distribucion, u->gap, u->padGapPx,
                        u->recortaX, u->recortaY, u->conScroll, u->scrollX, u->scrollY);
    EscribirHijos(f, u, 1, base);
    fputs("}\n", f);
    if (fclose(f) != 0) {
        remove(tmp.c_str());
        w3dLogfE("UI2D: escritura incompleta de %s (queda la version anterior)", ruta.c_str());
        return false;
    }
#ifdef _WIN32
    remove(ruta.c_str());   // Windows: rename() no pisa el destino (como LuaCompilar)
#endif
    if (rename(tmp.c_str(), ruta.c_str()) != 0) {
        remove(tmp.c_str());
        w3dLogfE("UI2D: no pude renombrar %s -> %s", tmp.c_str(), ruta.c_str());
        return false;
    }
    w3dLogf("UI2D: guardado %s", ruta.c_str());
    return true;
}

// el PARSER JSON ahora es compartido (io/JsonW3d.h): tambien lo usa el .w3d

// los refs {prop: valor} de un objeto json -> una entrada
static void LeerRefsDe(JVal* jrefs, W3dScriptEntrada* ent) {
    if (!jrefs || jrefs->tipo != 4) return;
    for (std::map<std::string, JVal*>::iterator r = jrefs->obj.begin();
         r != jrefs->obj.end(); ++r)
        if (r->second->tipo == 2)
            ent->refs.push_back(std::make_pair(r->first, r->second->str));
}
// los scripts + lo asignado; tolerante: sin campos = sin scripts. Retrocompat: los
// archivos viejos con "script"/"scriptRefs" (uno solo) se leen como una entrada.
static void LeerScript(JVal* j, Object* o, const std::string& base) {
    std::map<std::string, JVal*>::iterator it = j->obj.find("scripts");
    if (it != j->obj.end() && it->second->tipo == 5) {
        for (size_t i = 0; i < it->second->lista.size(); i++) {
            JVal* je = it->second->lista[i];
            if (!je || je->tipo != 4) continue;
            W3dScriptEntrada ent;
            ent.ruta = RutaAlCargar(JS(je, "ruta", ""), base);
            if (ent.ruta.empty()) continue;
            std::map<std::string, JVal*>::iterator jr = je->obj.find("refs");
            if (jr != je->obj.end()) LeerRefsDe(jr->second, &ent);
            if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
            o->scriptDatos->scripts.push_back(ent);
        }
        return;
    }
    std::string ruta = JS(j, "script", "");   // formato VIEJO (un solo script)
    if (ruta.empty()) return;
    W3dScriptEntrada ent;
    ent.ruta = RutaAlCargar(ruta, base);
    std::map<std::string, JVal*>::iterator jr = j->obj.find("scriptRefs");
    if (jr != j->obj.end()) LeerRefsDe(jr->second, &ent);
    if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
    o->scriptDatos->scripts.push_back(ent);
}

static void LeerCamposHijos(JVal* j, float* padIzq, float* padDer, float* padArr,
                            float* padAba, int* layoutHijos, int* layoutAjuste,
                            int* layoutAlign, int* distribucion, float* gap,
                            bool* padGapPx, bool* recortaX, bool* recortaY,
                            bool* conScroll, float* scrollX, float* scrollY) {
    // "padding" viejo (uniforme) = los 4 lados; los campos nuevos lo pisan si vienen
    float unif = JF(j, "padding", -1.0f);
    if (unif >= 0.0f) { *padIzq = *padDer = *padArr = *padAba = unif; }
    *padIzq = JF(j, "padIzq", *padIzq);
    *padDer = JF(j, "padDer", *padDer);
    *padArr = JF(j, "padArr", *padArr);
    *padAba = JF(j, "padAba", *padAba);
    *layoutHijos = JI(j, "layoutHijos", *layoutHijos);
    *layoutAjuste = JI(j, "layoutAjuste", *layoutAjuste);
    *layoutAlign = JI(j, "layoutAlign", *layoutAlign);
    // archivos viejos sin "distribucion" -> 0 (gap clasico), no cambia nada
    *distribucion = JI(j, "distribucion", *distribucion);
    if (*distribucion < 0 || *distribucion > 3) *distribucion = 0;
    *gap = JF(j, "gap", *gap);
    *padGapPx = JB(j, "padGapPx", *padGapPx);
    // "recortaX/recortaY" es el alias que usan algunos .w3dui escritos a mano (el
    // nombre del miembro); el canonico de guardado es "overflowX/overflowY". Se
    // aceptan los dos para no perder el dato al reabrir/re-guardar esos archivos.
    *recortaX = JB(j, "overflowX", JB(j, "recortaX", *recortaX));
    *recortaY = JB(j, "overflowY", JB(j, "recortaY", *recortaY));
    *conScroll = JB(j, "scroll", *conScroll);
    *scrollX = JF(j, "scrollX", *scrollX);
    *scrollY = JF(j, "scrollY", *scrollY);
}

static void CargarElemento(JVal* j, Object* padre, const std::string& base);

static void CargarHijos(JVal* j, Object* padre, const std::string& base) {
    JVal* hijos = JHijo(j, "hijos", 5);
    if (!hijos) return;
    for (size_t i = 0; i < hijos->lista.size(); i++)
        if (hijos->lista[i]->tipo == 4)
            CargarElemento(hijos->lista[i], padre, base);
}

static void CargarElemento(JVal* j, Object* padre, const std::string& base) {
    std::string tipo = JS(j, "tipo", "contenedor");
    Vector3 pos(0, 0, 0);
    { JVal* jp = JHijo(j, "pos", 5);
      if (jp) {
          if (jp->lista.size() > 0 && jp->lista[0]->tipo == 1) pos.x = (float)jp->lista[0]->num;
          if (jp->lista.size() > 1 && jp->lista[1]->tipo == 1) pos.y = (float)jp->lista[1]->num;
          if (jp->lista.size() > 2 && jp->lista[2]->tipo == 1) pos.z = (float)jp->lista[2]->num;
      } }

    Elemento2D* e = NULL;
    if (tipo == "texto") {
        Texto2D* t = new Texto2D(padre, pos);
        t->texto = JS(j, "texto", t->texto);
        t->tam = JF(j, "tam", t->tam);
        t->alignH = JI(j, "alignX", t->alignH);
        t->alignV = JI(j, "alignY", t->alignV);
        JColor(j, "color", t->color);
        t->fuente = RutaAlCargar(JS(j, "fuente", ""), base);
        if (JS(j, "fuente", "").empty()) t->fuente = "";   // "" = la fuente de Whisk3D
        t->fuenteBitmap = RutaAlCargar(JS(j, "fuenteBitmap", ""), base);
        if (JS(j, "fuenteBitmap", "").empty()) t->fuenteBitmap = "";
        t->tipo = JI(j, "tipoContenido", t->tipo);
        t->decimales = JF(j, "decimales", t->decimales);
        t->lineas = JI(j, "lineas", t->lineas);
        t->autoTam = JB(j, "autoTam", t->autoTam);
        t->palColor = JI(j, "palColor", t->palColor);
        e = t;
    } else if (tipo == "imagen") {
        Imagen2D* im = new Imagen2D(padre, pos);
        im->textura = RutaAlCargar(JS(j, "textura", ""), base);
        im->modo = JI(j, "modo", im->modo);
        JColor(j, "tinte", im->color);
        im->usarAlpha = JB(j, "usarAlpha", im->usarAlpha);
        im->filtrado = JB(j, "filtrado", im->filtrado);
        im->palTinte = JI(j, "palTinte", im->palTinte);
        e = im;
    } else if (tipo == "rect") {
        Rect2D* r = new Rect2D(padre, pos);
        JColor(j, "color", r->color);
        r->palColor = JI(j, "palColor", r->palColor);
        e = r;
    } else if (tipo == "boton") {
        Boton2D* b = new Boton2D(padre, pos);
        b->texto = JS(j, "texto", b->texto);
        b->icono = RutaAlCargar(JS(j, "icono", ""), base);
        if (JS(j, "icono", "").empty()) b->icono = "";
        b->fuente = RutaAlCargar(JS(j, "fuente", ""), base);
        if (JS(j, "fuente", "").empty()) b->fuente = "";
        b->tam = JF(j, "tam", b->tam);
        b->pad = JF(j, "pad", b->pad);
        JColor(j, "colorFondo", b->colorFondo);
        JColor(j, "colorTexto", b->colorTexto);
        JColor(j, "colorBorde", b->colorBorde);
        JColor(j, "colorHover", b->colorHover);   // opcional: sin el campo queda el verde accent
        b->palFondo = JI(j, "palFondo", b->palFondo);
        b->palTexto = JI(j, "palTexto", b->palTexto);
        b->palBorde = JI(j, "palBorde", b->palBorde);
        b->palHover = JI(j, "palHover", b->palHover);   // archivos viejos: -1 (propio)
        b->texturaFondo = RutaAlCargar(JS(j, "texturaFondo", ""), base);
        if (JS(j, "texturaFondo", "").empty()) b->texturaFondo = "";
        b->bordeTexX = JF(j, "bordeTexX", b->bordeTexX);
        b->bordeTexY = JF(j, "bordeTexY", b->bordeTexY);
        b->escalaBordeTex = JF(j, "escalaBordeTex", b->escalaBordeTex);
        e = b;
    } else if (tipo == "video") {
        Video2D* vd = new Video2D(padre, pos);
        vd->video = RutaAlCargar(JS(j, "video", ""), base);
        if (JS(j, "video", "").empty()) vd->video = "";
        vd->modo = JI(j, "modo", vd->modo);
        vd->loop = JB(j, "loop", vd->loop);
        vd->usarAlpha = JB(j, "usarAlpha", vd->usarAlpha);
        vd->filtrado = JB(j, "filtrado", vd->filtrado);
        e = vd;
    } else if (tipo == "expandir") {
        e = new Expandir2D(padre, pos);
    } else if (tipo == "slice9") {
        Slice9* s9 = new Slice9(padre, pos);
        s9->textura = RutaAlCargar(JS(j, "textura", ""), base);
        s9->bordeX = JF(j, "bordeX", s9->bordeX);
        s9->bordeY = JF(j, "bordeY", s9->bordeY);
        s9->escalaBorde = JF(j, "escalaBorde", s9->escalaBorde);
        JColor(j, "tinte", s9->color);
        s9->filtrado = JB(j, "filtrado", s9->filtrado);
        s9->palTinte = JI(j, "palTinte", s9->palTinte);
        e = s9;
    } else {
        e = new Contenedor2D(padre, pos);
    }

    // CARGA: nombre CRUDO (el .w3dui puede traer duplicados). Al MERGEAR un .w3dui en
    // la escena (Add > Whisk3D UI) el caller repara con W3dNombresRepararEscena, que
    // ademas arrastra las refs de los scripts.
    e->SetNameCrudo(JS(j, "nombre", e->name));
    e->visible = JB(j, "visible", true);
    e->ancla = JI(j, "ancla", e->ancla);
    e->rot2d = JF(j, "rotacion", e->rot2d);
    e->opacidad = JF(j, "opacidad", e->opacidad);
    e->peso = JF(j, "peso", e->peso);
    e->ancho = JF(j, "ancho", e->ancho);
    e->alto = JF(j, "alto", e->alto);
    // unidad del tamano, con RETROCOMPAT total: el bool viejo tamPx se lee igual
    // (px o fraccion); si el archivo trae "tamModo" (0 fraccion, 1 px, 2 escalado),
    // ese manda.
    e->tamModo = JB(j, "tamPx", e->tamModo == TAM2D_PX) ? TAM2D_PX : TAM2D_FRACCION;
    e->tamModo = JI(j, "tamModo", e->tamModo);
    if (e->tamModo < 0 || e->tamModo > 2) e->tamModo = TAM2D_PX;
    e->expandir = JB(j, "expandir", e->expandir);
    e->margIzq = JF(j, "margIzq", e->margIzq);
    e->margDer = JF(j, "margDer", e->margDer);
    e->margArr = JF(j, "margArr", e->margArr);
    e->margAba = JF(j, "margAba", e->margAba);
    e->margUni = JB(j, "margUni", e->margUni);
    e->paleta = JS(j, "paleta", "");   // seleccion de paleta por NOMBRE ("" = hereda)
    LeerCamposHijos(j, &e->padIzq, &e->padDer, &e->padArr, &e->padAba,
                    &e->layoutHijos, &e->layoutAjuste, &e->layoutAlign,
                    &e->distribucion, &e->gap, &e->padGapPx,
                    &e->recortaX, &e->recortaY, &e->conScroll, &e->scrollX, &e->scrollY);
    // archivos sin "padUni": si el padding por lado difiere, NO editar con un solo
    // valor (el sincronizador del panel pisaria los lados con el izquierdo)
    e->padUni = JB(j, "padUni", e->padIzq == e->padDer && e->padIzq == e->padArr &&
                                e->padIzq == e->padAba);
    LeerScript(j, e, base);
    CargarHijos(j, e, base);
}

UI* UI2DCargar(const std::string& ruta) {
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(ruta, datos) || datos.empty()) {
        w3dLogfE("UI2D: no pude leer %s", ruta.c_str());
        return NULL;
    }
    JParser parser((const char*)&datos[0], datos.size());
    JVal* raiz = parser.Valor();
    if (parser.error || raiz->tipo != 4) {
        w3dLogfE("UI2D: %s no parsea como JSON", ruta.c_str());
        delete raiz;
        return NULL;
    }
    std::string base = w3dFileSystem::ParentPath(ruta);

    // durante la carga los nombres entran crudos (ver SetNameCrudo mas abajo)
    struct CargaGuard { bool prev; CargaGuard(){ prev = W3dNombresCargando; W3dNombresCargando = true; }
                        ~CargaGuard(){ W3dNombresCargando = prev; } } cargaGuard;
    UI* u = new UI(NULL, Vector3(0, 0, 0));
    u->SetNameCrudo(JS(raiz, "nombre", u->name));   // idem: crudo + reparacion del caller
    // recordar de QUE archivo vino (solo el nombre): GuardarW3D re-escribe ese
    // mismo .w3dui hermano al guardar el proyecto (ver UI::archivoW3dui)
    { size_t s = ruta.find_last_of("/\\");
      u->archivoW3dui = (s == std::string::npos) ? ruta : ruta.substr(s + 1); }
    // las paletas guardadas REEMPLAZAN la default (si el archivo las trae)
    { JVal* jps = JHijo(raiz, "paletas", 5);
      if (jps && !jps->lista.empty()) {
          u->paletas.clear();
          u->paletaActiva = 0;
          for (size_t pIdx = 0; pIdx < jps->lista.size() && pIdx < 8; pIdx++) {
              JVal* jp = jps->lista[pIdx];
              if (jp->tipo != 4) continue;
              u->NuevaPaleta(JS(jp, "nombre", "Paleta"));
              u->Colores().clear();
              JVal* jc = JHijo(jp, "colores", 5);
              if (jc)
                  for (size_t i = 0; i < jc->lista.size() && i < 32; i++) {
                      JVal* e = jc->lista[i];
                      if (e->tipo != 4) continue;
                      float c[4] = { 1, 1, 1, 1 };
                      JColor(e, "color", c);
                      u->AgregarPaleta(JS(e, "nombre", "Color"), c);
                  }
          }
          u->paletaActiva = JI(raiz, "paletaActiva", 0);
          if (u->paletaActiva >= (int)u->paletas.size()) u->paletaActiva = 0;
      }
      // formato viejo: una sola "paleta" (una LISTA de colores; no confundir
      // con la "paleta" string nueva: la seleccion por nombre, ver abajo)
      JVal* jp = JHijo(raiz, "paleta", 5);
      if (jp && !jp->lista.empty() && (!jps || jps->lista.empty())) {
          u->paletas.clear();
          u->paletaActiva = 0;
          u->NuevaPaleta("Whisk3D");
          u->Colores().clear();
          for (size_t i = 0; i < jp->lista.size() && i < 32; i++) {
              JVal* e = jp->lista[i];
              if (e->tipo != 4) continue;
              float c[4] = { 1, 1, 1, 1 };
              JColor(e, "color", c);
              u->AgregarPaleta(JS(e, "nombre", "Color"), c);
          }
      } }
    // la SELECCION de paleta de la raiz, por NOMBRE (formato nuevo; el JHijo
    // tipo 2 = string NO matchea la lista "paleta" del formato viejo). Si el
    // archivo no la trae, se DERIVA de la paletaActiva de siempre: la activa
    // legacy pasa a ser la seleccion explicita de la raiz (mismo resultado).
    {
        JVal* jsel = JHijo(raiz, "paleta", 2);
        if (jsel) u->paleta = jsel->str;
        else if (u->paletaActiva >= 0 && u->paletaActiva < (int)u->paletas.size() &&
                 !u->paletas[u->paletaActiva].colores.empty())
            u->paleta = u->paletas[u->paletaActiva].nombre;
    }
    // ADOPTAR las paletas bakeadas al PROYECTO (merge por nombre, la primera
    // gana): en el editor conviven con las del .w3d; en el runtime standalone
    // este es el unico camino que las carga. Ver W3dPaletas.h.
    W3dPaletasAdoptar(u->paletas);
    JVal* v = JHijo(raiz, "ventana", 4);
    if (v) {
        u->igualQueRender = JB(v, "igualQueRender", u->igualQueRender);
        u->ancho = JF(v, "ancho", u->ancho);
        u->alto = JF(v, "alto", u->alto);
        u->escalaGlobal = JF(v, "escalaGlobal", u->escalaGlobal);
        JColor(v, "color", u->color);
    }
    LeerCamposHijos(raiz, &u->padIzq, &u->padDer, &u->padArr, &u->padAba,
                    &u->layoutHijos, &u->layoutAjuste, &u->layoutAlign,
                    &u->distribucion, &u->gap, &u->padGapPx,
                    &u->recortaX, &u->recortaY, &u->conScroll, &u->scrollX, &u->scrollY);
    // archivos sin "padUni": no pisar un padding por-lado con el sincronizador del panel
    u->padUni = JB(raiz, "padUni", u->padIzq == u->padDer && u->padIzq == u->padArr &&
                                   u->padIzq == u->padAba);
    LeerScript(raiz, u, base);
    CargarHijos(raiz, u, base);
    delete raiz;
    w3dLogf("UI2D: cargado %s", ruta.c_str());
    return u;
}
