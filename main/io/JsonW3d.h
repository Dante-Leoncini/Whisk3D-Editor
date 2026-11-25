#ifndef JSONW3D_H
#define JSONW3D_H

#include <string>
#include <map>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ============================================================================
//  JSON minimo COMPARTIDO (C++03): el parser que nacio en UI2DFormato (.w3dui)
//  ahora lo usan tambien el proyecto .w3d (scene.json) y quien lo necesite.
//  Header-only para no tocar los build de las 4 plataformas.
// ============================================================================

// ---------------------------------------------------------------------------
//  ESCRITURA DE NUMEROS CON ROUND-TRIP EXACTO
//
//  El "%g" pelado escribe 6 cifras significativas, o sea que 3.14159265 vuelve
//  como 3.14159: un error de 2.7e-6 que NADIE ve al guardar y que rompe en
//  silencio todo lo que dependa del valor EXACTO (el valor de un keyframe, el
//  offset de un handle bezier, la posicion de un objeto). Es el mismo problema
//  que el .w3dm ya resolvio en su escritor de texto (ver W3dText.h): se escribe
//  el MINIMO de digitos que, RELEIDO CON ESTE MISMO PARSER, da el mismo float32
//  bit a bit.
//
//  Se valida contra strtod y no contra otro parser a proposito: strtod es lo que
//  usa JParser::Value() mas abajo, o sea el lector REAL de estos archivos. Lo que
//  importa es que cierre el par escritor/lector que existe, no un ideal.
//
//  6..9 digitos: 9 significativas alcanzan SIEMPRE para un float32 (formato
//  IEEE-754 binary32), asi que el bucle termina con round-trip garantizado.
// ---------------------------------------------------------------------------
inline std::string JsonNumText(float v) {
    // NaN e infinitos NO son JSON legal: se sanean (mejor un archivo que abre con
    // un numero raro que uno que no parsea). v != v es el test portable de NaN.
    if (v != v) return std::string("0");
    if (v >  3.4028235e38f) return std::string("3.4e38");
    if (v < -3.4028235e38f) return std::string("-3.4e38");
    char b[48];
    for (int d = 6; d <= 9; d++) {
        snprintf(b, sizeof(b), "%.*g", d, (double)v);
        if ((float)strtod(b, NULL) == v) break;   // relee IGUAL con NUESTRO lector
    }
    return std::string(b);
}

struct JVal {
    // 0 = null, 1 = numero, 2 = string, 3 = bool, 4 = objeto, 5 = lista
    int type;
    double num;
    bool b;
    std::string str;
    std::map<std::string, JVal*> obj;
    std::vector<JVal*> list;
    // FILA COMPACTA: una lista de SOLO numeros que es elemento de OTRA lista (un keyframe
    // [frame,v,i,...], una fila de pesos, una cara de 'topologia') guarda sus numeros ACA y
    // deja 'lista' vacia: un JVal + un vector en vez de un JVal por numero. Sin esto el
    // proyecto grande de prueba (2.5 MB de anims horneadas = ~800 mil nodos) NO ENTRABA en la RAM del
    // N95. Se leen con JRowLen/JRowNum (abajo), que entienden los DOS formatos; las
    // listas sueltas (pos/color/etc., hijas de un objeto) siguen como siempre.
    std::vector<float> nums;
    JVal() : type(0), num(0), b(false) {}
    ~JVal() {
        for (std::map<std::string, JVal*>::iterator it = obj.begin(); it != obj.end(); ++it)
            delete it->second;
        for (size_t i = 0; i < list.size(); i++) delete list[i];
    }
};

// (depth: no mas de 256 niveles de { y [ -un proyecto, un .w3dui o una libreria de internet con unos pocos KB de
//  "[[[[..." agotaba la pila, antes en la PSP y el N95-; tambien acota el borrado recursivo de ~JVal)
struct JParser {
    const char* p;
    const char* end;
    bool error;
    int depth;
    JParser(const char* start, size_t n) : p(start), end(start + n), error(false), depth(0) {}
    struct Level { int& d; explicit Level(int& x) : d(x) { ++d; } ~Level() { --d; } };
    bool TooDeep() { if (depth > 256) error = true; return depth > 256; }
    void Whitespace() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool Is(char c) { Whitespace(); return p < end && *p == c; }
    bool Eat(char c) { if (!Is(c)) { error = true; return false; } p++; return true; }
    JVal* Value() {
        Whitespace();
        if (p >= end) { error = true; return new JVal(); }
        if (*p == '{') return Object();
        if (*p == '[') return List();
        if (*p == '"') { JVal* v = new JVal(); v->type = 2; v->str = ReadString(); return v; }
        // OJO: el buffer NO termina en NUL (son los bytes crudos del archivo):
        // strncmp/strtod directos sobre 'p' podian leer PASADO 'fin' con un
        // archivo truncado. Se acota todo al rango [p, fin).
        if (end - p >= 4 && !strncmp(p, "true", 4))  { p += 4; JVal* v = new JVal(); v->type = 3; v->b = true;  return v; }
        if (end - p >= 5 && !strncmp(p, "false", 5)) { p += 5; JVal* v = new JVal(); v->type = 3; v->b = false; return v; }
        if (end - p >= 4 && !strncmp(p, "null", 4))  { p += 4; return new JVal(); }
        double n = 0.0;
        if (!Number(n)) { error = true; return new JVal(); }
        JVal* v = new JVal(); v->type = 1; v->num = n;
        return v;
    }
    // un NUMERO en 'p' (ya sin blancos) -> 'out', y 'p' queda despues. false = no es un numero ('p' no se mueve).
    // Copia acotada con NUL (un numero JSON nunca llega a 47 chars)
    bool Number(double& out) {
        char nbuf[48];
        size_t nn = (size_t)(end - p); if (nn > sizeof(nbuf) - 1) nn = sizeof(nbuf) - 1;
        memcpy(nbuf, p, nn); nbuf[nn] = 0;
        char* after = NULL;
        const double n = strtod(nbuf, &after);
        if (after == nbuf) return false;
        p += (after - nbuf);
        out = n;
        return true;
    }
    // lo que Value() leeria como NUMERO (lo que no es objeto, lista, string ni true/false/null)?
    bool LooksLikeNumber() const {
        if (p >= end) return false;
        const char c = *p;
        if (c == '{' || c == '[' || c == '"') return false;
        if (end - p >= 4 && (!strncmp(p, "true", 4) || !strncmp(p, "null", 4))) return false;
        if (end - p >= 5 && !strncmp(p, "false", 5)) return false;
        return true;
    }
    std::string ReadString() {
        std::string s;
        if (!Eat('"')) return s;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                p++;
                if (*p == 'n') s += '\n'; else s += *p;
            } else s += *p;
            p++;
        }
        if (p < end) p++;   // la comilla de cierre
        return s;
    }
    JVal* Object() {
        Level level(depth);
        JVal* v = new JVal(); v->type = 4;
        if (TooDeep()) return v;
        Eat('{');
        if (Is('}')) { p++; return v; }
        for (;;) {
            std::string k = ReadString();
            Eat(':');
            JVal*& slot = v->obj[k];
            delete slot;   // (una clave repetida: la de antes no quedaba en ningun lado)
            slot = Value();
            if (error) break;
            if (Is(',')) { p++; continue; }
            Eat('}');
            break;
        }
        return v;
    }
    // un elemento de una lista: si es una sub-lista va por Fila (puede quedar COMPACTA)
    JVal* Element() {
        Whitespace();
        return (p < end && *p == '[') ? Row() : Value();
    }
    JVal* List() {
        Level level(depth);
        JVal* v = new JVal(); v->type = 5;
        if (TooDeep()) return v;
        Eat('[');
        if (Is(']')) { p++; return v; }
        for (;;) {
            v->list.push_back(Element());
            if (error) break;
            if (Is(',')) { p++; continue; }
            Eat(']');
            break;
        }
        return v;
    }
    // una sub-lista (elemento de OTRA lista): si es de SOLO numeros queda COMPACTA en 'nums' (ver JVal). Los
    // numeros se leen DIRECTO a 'nums', sin un JVal por numero: antes cada uno se creaba y se borraba al compactar
    // la fila (8 por keyframe: cientos de miles de reservas chicas que en el N95 hacian lenta la carga y
    // fragmentaban el heap). El arbol que queda es el MISMO: si aparece algo que no es un numero, lo leido pasa a
    // JVal con su double exacto (los primeros DOUBLE_STACK en la pila; los de una fila mas larga, a un vector) y la
    // fila sigue como una lista comun.
    enum { DOUBLE_STACK = 32 };
    JVal* Row() {
        Level level(depth);
        JVal* v = new JVal(); v->type = 5;
        if (TooDeep()) return v;
        Eat('[');
        if (Is(']')) { p++; return v; }
        double stack[DOUBLE_STACK];
        std::vector<double> rest;    // (solo una fila de mas de DOUBLE_STACK numeros)
        size_t count = 0;            // numeros leidos mientras es una fila
        bool onlyNumbers = true;
        for (;;) {
            Whitespace();
            double n = 0.0;
            if (onlyNumbers && LooksLikeNumber() && Number(n)) {
                if (count < DOUBLE_STACK) stack[count] = n; else rest.push_back(n);
                count++;
            } else {
                if (onlyNumbers) {
                    // deja de ser una fila: los numeros leidos pasan a JVal (con su double, como antes)
                    onlyNumbers = false;
                    for (size_t k = 0; k < count; k++) {
                        JVal* x = new JVal(); x->type = 1;
                        x->num = (k < DOUBLE_STACK) ? stack[k] : rest[k - DOUBLE_STACK];
                        v->list.push_back(x);
                    }
                    std::vector<double>().swap(rest);
                }
                v->list.push_back(Element());
            }
            if (error) break;
            if (Is(',')) { p++; continue; }
            Eat(']');
            break;
        }
        if (onlyNumbers && count) {   // UNA reserva con la capacidad justa
            v->nums.reserve(count);
            for (size_t k = 0; k < count; k++) v->nums.push_back((float)(k < DOUBLE_STACK ? stack[k] : rest[k - DOUBLE_STACK]));
        }
        return v;
    }
};

// lecturas comodas con default (los archivos viejos no traen los campos nuevos)
inline float JF(JVal* o, const char* k, float def) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->type == 1) ? (float)it->second->num : def;
}
inline int JI(JVal* o, const char* k, int def) { return (int)JF(o, k, (float)def); }
// un entero SIN SIGNO de 32 bits EXACTO (un serial de objeto): del double del parser, sin pasar por float (JI lo
// redondea desde 2^24: 16777217 volvia como 16777216)
inline unsigned JU(JVal* o, const char* k, unsigned def) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    if (it == o->obj.end() || it->second->type != 1) return def;
    const double v = it->second->num;
    return (v >= 0.0 && v <= 4294967295.0) ? (unsigned)v : def;
}
inline bool JB(JVal* o, const char* k, bool def) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->type == 3) ? it->second->b : def;
}
inline std::string JS(JVal* o, const char* k, const std::string& def) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->type == 2) ? it->second->str : def;
}
inline void JColor(JVal* o, const char* k, float* c) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    if (it == o->obj.end() || it->second->type != 5) return;
    for (size_t i = 0; i < 4 && i < it->second->list.size(); i++)
        if (it->second->list[i]->type == 1) c[i] = (float)it->second->list[i]->num;
}
inline JVal* JChild(JVal* o, const char* k, int type) {
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->type == type) ? it->second : NULL;
}

// --- FILAS (sub-listas de numeros, compactadas o no; ver JVal::nums) ---
// largo de la fila (sirve tambien para una lista comun)
inline size_t JRowLen(JVal* v) { return v->nums.empty() ? v->list.size() : v->nums.size(); }
// el elemento i es un numero? (una fila compacta es toda numeros por construccion)
inline bool JRowIsNum(JVal* v, size_t i) {
    if (!v->nums.empty()) return i < v->nums.size();
    return i < v->list.size() && v->list[i] && v->list[i]->type == 1;
}
// n-esimo numero de la fila ('def' si falta o no es numero)
inline float JRowNum(JVal* v, size_t i, float def) {
    if (!v->nums.empty()) return (i < v->nums.size()) ? v->nums[i] : def;
    return JRowIsNum(v, i) ? (float)v->list[i]->num : def;
}

#endif // JSONW3D_H
