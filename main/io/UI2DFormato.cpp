// ============================================================================
//  UI2DFormato.cpp — ver UI2DFormato.h.
//
//  El JSON se escribe y se parsea A MANO (C++03, sin dependencias): el formato
//  es chico y controlado. El parser es un JSON minimo (objetos, listas, string,
//  numero, bool) que alcanza y sobra para estos archivos.
// ============================================================================
#include "io/UI2DFormato.h"
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
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>

// ---------------------------------------------------------------------------
//  RUTAS relativas: si 'ruta' esta adentro de 'base' (la carpeta del .w3dui),
//  se guarda relativa; si no, absoluta tal cual.
// ---------------------------------------------------------------------------
static std::string RutaParaGuardar(const std::string& ruta, const std::string& base) {
    if (ruta.empty() || base.empty()) return ruta;
    if (ruta.size() > base.size() && ruta.compare(0, base.size(), base) == 0) {
        size_t i = base.size();
        while (i < ruta.size() && (ruta[i] == '/' || ruta[i] == '\\')) i++;
        return ruta.substr(i);
    }
    return ruta;
}
static std::string RutaAlCargar(const std::string& guardada, const std::string& base) {
    if (guardada.empty()) return guardada;
    // absoluta (unix "/x", windows "C:\") queda tal cual; el resto cuelga de la base
    if (guardada[0] == '/' || (guardada.size() > 1 && guardada[1] == ':')) return guardada;
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
static void CampoF(FILE* f, int ind, const char* k, float v, bool coma = true) {
    Sangria(f, ind); fprintf(f, "\"%s\": %g%s\n", k, v, coma ? "," : "");
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
    fprintf(f, "\"%s\": [%g, %g, %g, %g]%s\n", k, c[0], c[1], c[2], c[3], coma ? "," : "");
}

static void EscribirElemento(FILE* f, Object* o, int ind, const std::string& base);

// los campos que COMPARTEN los elementos y el UI (layout de hijos + overflow)
static void EscribirCamposHijos(FILE* f, int ind, float padIzq, float padDer,
                                float padArr, float padAba, int layoutHijos,
                                int layoutAjuste, int layoutAlign, float gap,
                                bool padGapPx, bool recortaX, bool recortaY,
                                bool conScroll, float scrollX, float scrollY) {
    CampoF(f, ind, "padIzq", padIzq);
    CampoF(f, ind, "padDer", padDer);
    CampoF(f, ind, "padArr", padArr);
    CampoF(f, ind, "padAba", padAba);
    CampoI(f, ind, "layoutHijos", layoutHijos);      // 0 libre, 1 filas, 2 columnas
    CampoI(f, ind, "layoutAjuste", layoutAjuste);    // 0 estirar, 1 minimo
    CampoI(f, ind, "layoutAlign", layoutAlign);      // 0 inicio, 1 centro, 2 fin
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
            t != ObjectType::expandir2d)
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
    CampoS(f, i2, "tipo", tipo);
    CampoS(f, i2, "nombre", o->name);
    CampoB(f, i2, "visible", o->visible);
    Sangria(f, i2); fprintf(f, "\"pos\": [%g, %g, %g],\n", o->pos.x, o->pos.y, o->pos.z);
    CampoI(f, i2, "ancla", e->ancla);                // 0 centro, 1..4 bordes, 5..8 esquinas
    CampoF(f, i2, "rotacion", e->rot2d);
    CampoF(f, i2, "opacidad", e->opacidad);
    CampoF(f, i2, "peso", e->peso);                  // reparto en filas/columnas del padre
    CampoF(f, i2, "ancho", e->ancho);
    CampoF(f, i2, "alto", e->alto);
    CampoB(f, i2, "tamPx", e->tamPx);                // false: relativo al rect del padre

    if (o->getType() == ObjectType::texto2d) {
        Texto2D* t = (Texto2D*)o;
        CampoS(f, i2, "texto", t->texto);
        CampoF(f, i2, "tam", t->tam);
        CampoI(f, i2, "alignX", t->alignH);          // 0 izq, 1 centro, 2 der
        CampoI(f, i2, "alignY", t->alignV);
        CampoColor(f, i2, "color", t->color);
        CampoS(f, i2, "fuente", RutaParaGuardar(t->fuente, base));   // "" = la de Whisk3D
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
        CampoI(f, i2, "palFondo", b->palFondo);
        CampoI(f, i2, "palTexto", b->palTexto);
        CampoI(f, i2, "palBorde", b->palBorde);
        CampoS(f, i2, "texturaFondo", RutaParaGuardar(b->texturaFondo, base));
        CampoF(f, i2, "bordeTexX", b->bordeTexX);
        CampoF(f, i2, "bordeTexY", b->bordeTexY);
        CampoF(f, i2, "escalaBordeTex", b->escalaBordeTex);
    }

    EscribirCamposHijos(f, i2, e->padIzq, e->padDer, e->padArr, e->padAba,
                        e->layoutHijos, e->layoutAjuste, e->layoutAlign,
                        e->gap, e->padGapPx,
                        e->recortaX, e->recortaY, e->conScroll, e->scrollX, e->scrollY);
    EscribirHijos(f, o, i2, base);
    Sangria(f, ind); fputc('}', f);
}

bool UI2DGuardar(UI* u, const std::string& ruta) {
    if (!u) return false;
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) { w3dLogfE("UI2D: no pude escribir %s", ruta.c_str()); return false; }
    std::string base = w3dFileSystem::ParentPath(ruta);

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
    // las PALETAS ("temas"): los elementos referencian por indice contra la ACTIVA
    CampoI(f, 1, "paletaActiva", u->paletaActiva);
    Sangria(f, 1); fputs("\"paletas\": [\n", f);
    for (size_t pIdx = 0; pIdx < u->paletas.size(); pIdx++) {
        Paleta& pa = u->paletas[pIdx];
        Sangria(f, 2); fputs("{ \"nombre\": ", f);
        JsonEscapar(f, pa.nombre);
        fputs(", \"colores\": [\n", f);
        for (size_t i = 0; i < pa.colores.size(); i++) {
            Sangria(f, 3);
            fputs("{ \"nombre\": ", f);
            JsonEscapar(f, pa.colores[i].nombre);
            fprintf(f, ", \"color\": [%g, %g, %g, %g] }%s\n",
                    pa.colores[i].rgba[0], pa.colores[i].rgba[1],
                    pa.colores[i].rgba[2], pa.colores[i].rgba[3],
                    (i + 1 < pa.colores.size()) ? "," : "");
        }
        Sangria(f, 2);
        fprintf(f, "] }%s\n", (pIdx + 1 < u->paletas.size()) ? "," : "");
    }
    Sangria(f, 1); fputs("],\n", f);
    EscribirCamposHijos(f, 1, u->padIzq, u->padDer, u->padArr, u->padAba,
                        u->layoutHijos, u->layoutAjuste, u->layoutAlign,
                        u->gap, u->padGapPx,
                        u->recortaX, u->recortaY, u->conScroll, u->scrollX, u->scrollY);
    EscribirHijos(f, u, 1, base);
    fputs("}\n", f);
    fclose(f);
    w3dLogf("UI2D: guardado %s", ruta.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  PARSER JSON minimo: valores tipados en un arbol chico.
// ---------------------------------------------------------------------------
struct JVal {
    // 0 = null, 1 = numero, 2 = string, 3 = bool, 4 = objeto, 5 = lista
    int tipo;
    double num;
    bool b;
    std::string str;
    std::map<std::string, JVal*> obj;
    std::vector<JVal*> lista;
    JVal() : tipo(0), num(0), b(false) {}
    ~JVal() {
        for (std::map<std::string, JVal*>::iterator it = obj.begin(); it != obj.end(); ++it)
            delete it->second;
        for (size_t i = 0; i < lista.size(); i++) delete lista[i];
    }
};

struct JParser {
    const char* p;
    const char* fin;
    bool error;
    JParser(const char* ini, size_t n) : p(ini), fin(ini + n), error(false) {}
    void Blancos() { while (p < fin && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool Es(char c) { Blancos(); return p < fin && *p == c; }
    bool Comer(char c) { if (!Es(c)) { error = true; return false; } p++; return true; }
    JVal* Valor() {
        Blancos();
        if (p >= fin) { error = true; return new JVal(); }
        if (*p == '{') return Objeto();
        if (*p == '[') return Lista();
        if (*p == '"') { JVal* v = new JVal(); v->tipo = 2; v->str = Cadena(); return v; }
        if (!strncmp(p, "true", 4))  { p += 4; JVal* v = new JVal(); v->tipo = 3; v->b = true;  return v; }
        if (!strncmp(p, "false", 5)) { p += 5; JVal* v = new JVal(); v->tipo = 3; v->b = false; return v; }
        if (!strncmp(p, "null", 4))  { p += 4; return new JVal(); }
        // numero
        char* despues = NULL;
        double n = strtod(p, &despues);
        if (despues == p) { error = true; return new JVal(); }
        p = despues;
        JVal* v = new JVal(); v->tipo = 1; v->num = n;
        return v;
    }
    std::string Cadena() {
        std::string s;
        if (!Comer('"')) return s;
        while (p < fin && *p != '"') {
            if (*p == '\\' && p + 1 < fin) {
                p++;
                if (*p == 'n') s += '\n'; else s += *p;
            } else s += *p;
            p++;
        }
        if (p < fin) p++;   // la comilla de cierre
        return s;
    }
    JVal* Objeto() {
        JVal* v = new JVal(); v->tipo = 4;
        Comer('{');
        if (Es('}')) { p++; return v; }
        for (;;) {
            std::string k = Cadena();
            Comer(':');
            v->obj[k] = Valor();
            if (error) break;
            if (Es(',')) { p++; continue; }
            Comer('}');
            break;
        }
        return v;
    }
    JVal* Lista() {
        JVal* v = new JVal(); v->tipo = 5;
        Comer('[');
        if (Es(']')) { p++; return v; }
        for (;;) {
            v->lista.push_back(Valor());
            if (error) break;
            if (Es(',')) { p++; continue; }
            Comer(']');
            break;
        }
        return v;
    }
};

// lecturas comodas con default (los archivos viejos no traen los campos nuevos)
static float JF(JVal* o, const char* k, float def) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->tipo == 1) ? (float)it->second->num : def;
}
static int JI(JVal* o, const char* k, int def) { return (int)JF(o, k, (float)def); }
static bool JB(JVal* o, const char* k, bool def) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->tipo == 3) ? it->second->b : def;
}
static std::string JS(JVal* o, const char* k, const std::string& def) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->tipo == 2) ? it->second->str : def;
}
static void JColor(JVal* o, const char* k, float* c) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    if (it == o->obj.end() || it->second->tipo != 5) return;
    for (size_t i = 0; i < 4 && i < it->second->lista.size(); i++)
        if (it->second->lista[i]->tipo == 1) c[i] = (float)it->second->lista[i]->num;
}
static JVal* JHijo(JVal* o, const char* k, int tipo) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->tipo == tipo) ? it->second : NULL;
}

static void LeerCamposHijos(JVal* j, float* padIzq, float* padDer, float* padArr,
                            float* padAba, int* layoutHijos, int* layoutAjuste,
                            int* layoutAlign, float* gap,
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
    *gap = JF(j, "gap", *gap);
    *padGapPx = JB(j, "padGapPx", *padGapPx);
    *recortaX = JB(j, "overflowX", *recortaX);
    *recortaY = JB(j, "overflowY", *recortaY);
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
        b->palFondo = JI(j, "palFondo", b->palFondo);
        b->palTexto = JI(j, "palTexto", b->palTexto);
        b->palBorde = JI(j, "palBorde", b->palBorde);
        b->texturaFondo = RutaAlCargar(JS(j, "texturaFondo", ""), base);
        if (JS(j, "texturaFondo", "").empty()) b->texturaFondo = "";
        b->bordeTexX = JF(j, "bordeTexX", b->bordeTexX);
        b->bordeTexY = JF(j, "bordeTexY", b->bordeTexY);
        b->escalaBordeTex = JF(j, "escalaBordeTex", b->escalaBordeTex);
        e = b;
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

    e->name = JS(j, "nombre", e->name);
    e->visible = JB(j, "visible", true);
    e->ancla = JI(j, "ancla", e->ancla);
    e->rot2d = JF(j, "rotacion", e->rot2d);
    e->opacidad = JF(j, "opacidad", e->opacidad);
    e->peso = JF(j, "peso", e->peso);
    e->ancho = JF(j, "ancho", e->ancho);
    e->alto = JF(j, "alto", e->alto);
    e->tamPx = JB(j, "tamPx", e->tamPx);
    LeerCamposHijos(j, &e->padIzq, &e->padDer, &e->padArr, &e->padAba,
                    &e->layoutHijos, &e->layoutAjuste, &e->layoutAlign,
                    &e->gap, &e->padGapPx,
                    &e->recortaX, &e->recortaY, &e->conScroll, &e->scrollX, &e->scrollY);
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

    UI* u = new UI(NULL, Vector3(0, 0, 0));
    u->name = JS(raiz, "nombre", u->name);
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
      // formato viejo: una sola "paleta"
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
                    &u->gap, &u->padGapPx,
                    &u->recortaX, &u->recortaY, &u->conScroll, &u->scrollX, &u->scrollY);
    CargarHijos(raiz, u, base);
    delete raiz;
    w3dLogf("UI2D: cargado %s", ruta.c_str());
    return u;
}
