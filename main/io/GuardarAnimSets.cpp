#include "io/GuardarAnimSets.h"
#include "io/W3dContenedor.h"          // el escritor del contenedor + W3dSlugEntrada
#include "animation/W3dAnimSet.h"      // el formato .w3da, el recurso y el registro
#include "animation/SkeletalAnimation.h"
#include "objects/Armature.h"
#include "objects/Objects.h"
#include "W3dRaices.h"                  // los armatures de TODAS las raices (escenas/prefabs)
#include "io/W3dRecursos.h"
#include "io/w3dFilesystem.h"          // los HUERFANOS se copian de la entrada del contenedor anterior
#include "base/W3dNombres.h"           // W3dNombreUnicoEnValores: la regla .NNN de siempre
#include "w3dlog.h"
#include "W3dAviso.h"                  // el freno se VE en pantalla, no solo en el log
#include "io/Librerias.h"               // W3dLibRefDe: el animset de una libreria que no esta (se nombra igual)

#include <map>
#include <set>
#include <algorithm>
#include <stdio.h>

bool g_w3dAnimsInline = false;

namespace {

// una biblioteca del plan
struct LibPlan {
    std::string nombre, entrada, carpeta;
    std::vector<std::string> huesos;              // la tabla HUES (el espacio de indices)
    std::vector<std::string> bloques;             // los CLIP serializados, en el orden de la biblioteca
    std::vector<std::string> bloquesJer;          // los JCLP (clips de jerarquia), en el orden de su recurso
    std::map<unsigned, std::vector<int> > porHuella;   // huella del bloque -> candidatos (dedup)
    W3dRecurso* rec;                              // el recurso del que viene (NULL = nueva)
    const std::vector<W3dAnimSetBloqueAjeno>* ajenos;
    // la HUELLA del .w3da escrito (los bytes ya se le pasaron al escritor): con ella Confirmar
    // sabe si el recurso cargado quedo igual a su entrada nueva
    unsigned huella1, huella2;
    size_t   tam;
    LibPlan() : rec(0), ajenos(0), huella1(0), huella2(0), tam(0) {}
};
struct ArmPlan {
    Armature* a;
    int lib;
    std::vector<int> clips;
    ArmPlan() : a(0), lib(-1) {}
};

std::vector<LibPlan> gLibs;
std::vector<ArmPlan> gArms;
std::map<const Armature*, size_t> gArmIdx;
// CLIPS DE JERARQUIA: la biblioteca que le toca a cada objeto raiz (indice en gLibs) y las bibliotecas
// que se armaron ENTERAS desde su recurso en memoria (Confirmar las deja nombrando su entrada nueva)
std::map<const Object*, int> gJerLib;
std::set<int> gLibsDesdeRecurso;
std::vector<W3dAnimSetFila> gRegistroNuevo;
// los que usan un animset de una LIBRERIA externa (io/Librerias.h): no entran al plan (no se escriben en el
// proyecto); su nodo lo NOMBRA con su prefijo ("Personajes/Caminar") y los clips que usa de el
std::map<const Armature*, std::pair<std::string, std::vector<int> > > gArmLib;
std::map<const Object*, std::string> gJerLibExt;
// el recurso es de una libreria? (su clave en el almacen es su entrada "lib:<libreria>/...")
bool RecursoDeLibreria(const W3dRecurso* r) { return r && r->id.compare(0, 4, "lib:") == 0; }
GuardarAnimSetsInfo gInfo;

// (lo que genera un PROXY de una libreria no se guarda: sus armatures y sus clips son de la libreria)
void Juntar(Object* o, std::vector<Armature*>& out) {
    if (!o || o->getType() == ObjectType::proxy) return;
    if (o->getType() == ObjectType::armature) out.push_back((Armature*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) Juntar(o->Childrens[i], out);
}
// los objetos con biblioteca de clips de jerarquia (en preorden, como los armatures)
void JuntarJer(Object* o, std::vector<Object*>& out) {
    if (!o || o->getType() == ObjectType::proxy) return;
    if (o->clipsJer && !o->clipsJer->animset.empty()) out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarJer(o->Childrens[i], out);
}
void JuntarJerRaiz(Object* raiz, std::vector<Object*>& out) {
    if (raiz) { JuntarJer(raiz, out); return; }
    std::vector<Object*> raices;
    W3dRaicesEnOrden(raices);
    for (size_t i = 0; i < raices.size(); i++) JuntarJer(raices[i], out);
}
// los armatures de 'raiz' o, con NULL, los de TODAS las raices del proyecto (la activa y las
// escenas/prefabs que no se estan mirando: ver W3dRaicesVivas)
void JuntarRaiz(Object* raiz, std::vector<Armature*>& out) {
    if (raiz) { Juntar(raiz, out); return; }
    std::vector<Object*> raices;
    W3dRaicesEnOrden(raices);   // (en el orden del registro: el guardado no depende de la escena activa)
    for (size_t i = 0; i < raices.size(); i++) Juntar(raices[i], out);
}

int LibPorNombre(const std::string& n) {
    for (size_t i = 0; i < gLibs.size(); i++) if (gLibs[i].nombre == n) return (int)i;
    return -1;
}

std::string NombreLibre(const std::string& base, const std::vector<std::string>& extra) {
    std::vector<std::string> tomados = extra;
    for (size_t i = 0; i < gLibs.size(); i++) tomados.push_back(gLibs[i].nombre);
    return W3dNombreUnicoEnValores(base, "Animaciones", tomados, -1);
}

std::string CarpetaDeRegistro(const std::string& nombre) {
    const std::vector<W3dAnimSetFila>& r = W3dAnimSetsRegistro();
    for (size_t i = 0; i < r.size(); i++) if (r[i].nombre == nombre) return r[i].carpeta;
    return std::string();
}

unsigned HuellaBloque(const std::string& b) {
    unsigned h1, h2;
    W3dAnimSetHuella((const unsigned char*)b.data(), b.size(), h1, h2);
    return h1 ^ (h2 * 2654435761u);
}

// el indice del bloque en la biblioteca (lo agrega si es nuevo): el DEDUP POR CLIP
int AgregarClip(LibPlan& L, const std::string& bloque) {
    const unsigned h = HuellaBloque(bloque);
    std::vector<int>& cand = L.porHuella[h];
    for (size_t i = 0; i < cand.size(); i++) if (L.bloques[cand[i]] == bloque) return cand[i];
    L.bloques.push_back(bloque);
    cand.push_back((int)L.bloques.size() - 1);
    return (int)L.bloques.size() - 1;
}

std::string EntradaLibre(W3dContenedorEscritor* esc, const std::string& nombre, const std::set<std::string>& usadas) {
    const std::string base = "animaciones/" + W3dSlugEntrada(nombre);
    std::string e = base + ".w3da";
    for (int k = 2; (esc && esc->Tiene(e)) || usadas.count(e); k++) {
        char b[16]; sprintf(b, "-%d", k);
        e = base + b + ".w3da";
    }
    return e;
}

bool FilaMenor(const W3dAnimSetFila& a, const W3dAnimSetFila& b) { return a.nombre < b.nombre; }

// un string del JSON: el mismo escape que JEsc de GuardarW3D.cpp (lo que entiende JParser)
void JTexto(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const char c = v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += c; }
        else if (c == '\n') s += "\\n";
        else s += c;
    }
    s += '"';
}

} // namespace

const GuardarAnimSetsInfo& GuardarAnimSetsUltimo() { return gInfo; }

bool GuardarAnimSetsPreparar(W3dContenedorEscritor* esc, Object* raiz) {
    gLibs.clear(); gArms.clear(); gArmIdx.clear(); gRegistroNuevo.clear();
    gJerLib.clear(); gLibsDesdeRecurso.clear();
    gArmLib.clear(); gJerLibExt.clear();
    gInfo = GuardarAnimSetsInfo();
    std::vector<Armature*> arms;
    JuntarRaiz(raiz, arms);
    std::vector<Object*> jer;
    JuntarJerRaiz(raiz, jer);
    // los que siguen COLGADOS de un animset de una LIBRERIA: lo nombran y salen del plan
    for (size_t i = arms.size(); i-- > 0; ) {
        Armature* a = arms[i];
        if (a->animations.empty() || !W3dArmatureAnimSetCalza(a) || !RecursoDeLibreria(W3dArmatureAnimSetRecurso(a))) continue;
        std::vector<int> idx;
        W3dArmatureAnimsIndices(a, idx);
        gArmLib[a] = std::make_pair(a->animSetNombre, idx);
        arms.erase(arms.begin() + (long)i);
    }
    // ...y los que quedaron SIN clips porque su animset es de una libreria que NO ESTA (io/Librerias.h): su referencia se
    // escribe tal cual (vuelve a resolverse al vincularla); no son un animset del proyecto ni frenan el guardado
    for (size_t i = arms.size(); i-- > 0; ) {
        const W3dLibRef* ref = arms[i]->animations.empty() ? W3dLibRefDe(arms[i], W3D_LIBREF_ANIMSET) : NULL;
        if (!ref) continue;
        gArmLib[arms[i]] = std::make_pair(ref->nombre, ref->clips);
        arms.erase(arms.begin() + (long)i);
    }
    for (size_t i = jer.size(); i-- > 0; )
        if (RecursoDeLibreria(jer[i]->clipsJer->rec)) {
            gJerLibExt[jer[i]] = jer[i]->clipsJer->animset;
            jer.erase(jer.begin() + (long)i);
        }
    // ---- FRENO: una biblioteca de clips de jerarquia que no se pudo leer al abrir no se pisa ----
    for (size_t i = 0; i < jer.size(); i++)
        if (!jer[i]->clipsJer->noCargo.empty()) {
            w3dLogfE("[W3D] '%s': sus clips (biblioteca '%s') no se pudieron leer al abrir: NO guardo encima",
                     jer[i]->name.c_str(), jer[i]->clipsJer->noCargo.c_str());
            W3dAvisof(true, "No guardo: los clips de '%s' no se pudieron leer al abrir y guardarlo los borraria",
                      W3dNombreCorto(jer[i]->name).c_str());
            return false;
        }
    // ---- FRENO: un animset que no se pudo leer al abrir no se pisa ----
    for (size_t i = 0; i < arms.size(); i++)
        if (!arms[i]->animSetNoCargo.empty()) {
            w3dLogfE("[W3D] '%s': sus animaciones (animset '%s') no se pudieron leer al abrir: NO guardo encima",
                     arms[i]->name.c_str(), arms[i]->animSetNoCargo.c_str());
            W3dAvisof(true, "No guardo: las animaciones de '%s' no se pudieron leer al abrir y guardarlo las borraria",
                      W3dNombreCorto(arms[i]->name).c_str());
            return false;
        }
    // el formato INLINE de antes (solo el harness): nada de animsets, ni siquiera los huerfanos
    if (g_w3dAnimsInline) return true;

    // ---- 1) los armatures que siguen COLGADOS de un animset: se quedan en el (y con su nombre),
    //         salvo que su esqueleto ya no calce con la tabla como al asignarlo (un hueso renombrado
    //         sin tocar los clips): esos van al paso 2 como si sus clips fueran propios ----
    std::vector<char> colgado(arms.size(), 0);
    for (size_t i = 0; i < arms.size(); i++) colgado[i] = W3dArmatureAnimSetCalza(arms[i]) ? 1 : 0;
    for (size_t i = 0; i < arms.size(); i++) {
        Armature* a = arms[i];
        if (a->animations.empty() || !colgado[i]) continue;
        W3dAnimSet* set = W3dArmatureAnimSet(a);
        if (!set) continue;
        W3dRecurso* r = W3dArmatureAnimSetRecurso(a);
        int li = -1;
        for (size_t k = 0; k < gLibs.size(); k++) if (gLibs[k].rec == r) { li = (int)k; break; }
        if (li < 0) {
            LibPlan L;
            L.nombre = NombreLibre(set->nombre.empty() ? a->name : set->nombre, std::vector<std::string>());
            L.huesos = set->datos.huesos;
            L.rec = r;
            L.ajenos = &set->datos.ajenos;
            gLibs.push_back(L);
        }
    }
    // ---- 2) los de clips PROPIOS (editados, importados, de un .w3d viejo): a la biblioteca
    //         de su esqueleto (primero la que traian, despues cualquiera con los mismos
    //         huesos) o a una nueva con el nombre del animset de donde vinieron / del armature ----
    std::vector<int> libDe(arms.size(), -1);
    for (size_t i = 0; i < arms.size(); i++) {
        Armature* a = arms[i];
        if (a->animations.empty()) continue;
        if (colgado[i]) {
            W3dRecurso* r = W3dArmatureAnimSetRecurso(a);
            for (size_t k = 0; k < gLibs.size(); k++) if (gLibs[k].rec == r) { libDe[i] = (int)k; break; }
            continue;
        }
        std::vector<std::string> huesos;
        W3dArmatureNombresHuesos(a, huesos);
        int li = -1;
        if (!a->animSetNombre.empty()) {
            const int k = LibPorNombre(a->animSetNombre);
            if (k >= 0 && gLibs[k].huesos == huesos) li = k;
        }
        for (size_t k = 0; li < 0 && k < gLibs.size(); k++) if (gLibs[k].huesos == huesos) li = (int)k;
        if (li < 0) {
            // el nombre no pisa a otro animset del REGISTRO (un huerfano se conserva con el suyo),
            // salvo el que este armature traia: es el mismo animset, editado
            std::vector<std::string> ocupados;
            const std::vector<W3dAnimSetFila>& reg = W3dAnimSetsRegistro();
            for (size_t k = 0; k < reg.size(); k++) if (reg[k].nombre != a->animSetNombre) ocupados.push_back(reg[k].nombre);
            LibPlan L;
            L.nombre = NombreLibre(a->animSetNombre.empty() ? a->name : a->animSetNombre, ocupados);
            L.huesos = huesos;
            gLibs.push_back(L);
            li = (int)gLibs.size() - 1;
        }
        libDe[i] = li;
    }
    // ---- 3) los clips, en preorden: cada uno traducido al espacio de huesos de su biblioteca
    //         y deduplicado ahi (el mismo clip en diez armatures = UN bloque) ----
    //  Un armature COLGADO escribe el clip CANONICO del recurso (ya esta en el espacio de la
    //  tabla: mapa identidad), no el que tiene en la mano: si usa una VARIANTE, esa copia tiene
    //  en -1 las pistas de los huesos que a SU esqueleto le faltan, y volver al espacio de la
    //  tabla desde ahi las dejaba sin hueso para siempre (y el clip ya no deduplicaba con el
    //  canonico de los demas usuarios: la biblioteca se duplicaba en cada guardado).
    std::vector<int> identidad;
    for (size_t i = 0; i < arms.size(); i++) {
        Armature* a = arms[i];
        if (libDe[i] < 0) continue;
        LibPlan& L = gLibs[libDe[i]];
        std::vector<int> mapa;
        W3dAnimSetMapaDesdeArmature(L.huesos, a, mapa);
        // colgado: el indice de cada clip en el recurso (-1 = no esta; no deberia pasar)
        const W3dAnimSet* set = (colgado[i] && L.rec == W3dArmatureAnimSetRecurso(a)) ? W3dArmatureAnimSet(a) : 0;
        std::vector<int> idx;
        if (set) {
            W3dArmatureAnimsIndices(a, idx);
            identidad.resize(L.huesos.size());
            for (size_t h = 0; h < identidad.size(); h++) identidad[h] = (int)h;
        }
        ArmPlan P;
        P.a = a; P.lib = libDe[i];
        std::string bloque;
        for (size_t c = 0; c < a->animations.size(); c++) {
            if (!a->animations[c]) continue;
            const int k = (set && c < idx.size()) ? idx[c] : -1;
            if (k >= 0 && k < (int)set->datos.clips.size() && set->datos.clips[k])
                W3dAnimSetClipBloque(*set->datos.clips[k], identidad, (int)L.huesos.size(), bloque);
            else
                W3dAnimSetClipBloque(*a->animations[c], mapa, (int)L.huesos.size(), bloque);
            P.clips.push_back(AgregarClip(L, bloque));
        }
        gInfo.armatures++;
        gInfo.clipsArmatures += (int)P.clips.size();
        gArmIdx[a] = gArms.size();
        gArms.push_back(P);
    }
    // ---- 3b) las bibliotecas de CLIPS DE JERARQUIA (las de los objetos raiz): cada una se escribe
    //          ENTERA desde su recurso en memoria (la editan todos sus usuarios: es la copia viva). Dos
    //          bibliotecas con el MISMO contenido (dos objetos que grabaron lo mismo por separado) quedan
    //          en una: los usuarios de la segunda pasan a la primera (dedup por contenido). Una biblioteca
    //          que ademas tiene clips de esqueleto que ningun armature usa los conserva tal cual ----
    {
        std::map<const W3dRecurso*, int> libDeRec;          // recurso -> biblioteca del plan
        std::map<std::string, int> porContenido;            // bytes de sus clips de jerarquia -> biblioteca
        std::vector<int> identidadH;
        for (size_t i = 0; i < jer.size(); i++) {
            Object* o = jer[i];
            W3dRecurso* r = o->clipsJer->rec;
            const W3dAnimSet* set = (r && r->estado == W3DREC_LISTO) ? (const W3dAnimSet*)r->dato : 0;
            if (!set) continue;
            std::map<const W3dRecurso*, int>::const_iterator ya = libDeRec.find(r);
            if (ya != libDeRec.end()) { gJerLib[o] = ya->second; continue; }
            std::vector<std::string> bloquesJer(set->datos.jerarquias.size());
            std::string contenido;
            for (size_t c = 0; c < set->datos.jerarquias.size(); c++) {
                W3dAnimSetClipJerBloque(*set->datos.jerarquias[c], bloquesJer[c]);
                contenido += bloquesJer[c];
            }
            // una biblioteca que el plan de los armatures ya tiene (un animset con clips de los dos tipos)
            int li = -1;
            for (size_t k = 0; k < gLibs.size() && li < 0; k++) if (gLibs[k].rec == r) li = (int)k;
            if (li < 0 && set->datos.clips.empty() && !contenido.empty()) {
                std::map<std::string, int>::const_iterator igual = porContenido.find(contenido);
                if (igual != porContenido.end()) {
                    libDeRec[r] = igual->second; gJerLib[o] = igual->second;
                    w3dLogf("[W3D] clips de jerarquia: la biblioteca '%s' es igual a '%s': queda una",
                            set->nombre.c_str(), gLibs[(size_t)igual->second].nombre.c_str());
                    continue;
                }
            }
            if (li < 0) {
                LibPlan L;
                L.nombre = NombreLibre(set->nombre.empty() ? o->name : set->nombre, std::vector<std::string>());
                L.huesos = set->datos.huesos;
                L.rec = r;
                L.ajenos = &set->datos.ajenos;
                // sus clips de esqueleto (si tiene) tal cual: nadie los esta usando en el plan de arriba
                identidadH.resize(L.huesos.size());
                for (size_t h = 0; h < identidadH.size(); h++) identidadH[h] = (int)h;
                std::string bloque;
                for (size_t c = 0; c < set->datos.clips.size(); c++)
                    if (set->datos.clips[c]) {
                        W3dAnimSetClipBloque(*set->datos.clips[c], identidadH, (int)L.huesos.size(), bloque);
                        AgregarClip(L, bloque);
                    }
                gLibs.push_back(L);
                li = (int)gLibs.size() - 1;
                gLibsDesdeRecurso.insert(li);
            }
            gLibs[(size_t)li].bloquesJer.swap(bloquesJer);
            if (!contenido.empty() && set->datos.clips.empty()) porContenido[contenido] = li;
            libDeRec[r] = li;
            gJerLib[o] = li;
            gInfo.clipsJer += (int)gLibs[(size_t)li].bloquesJer.size();
        }
        gInfo.raicesJer = (int)gJerLib.size();
    }
    // ---- 4) las entradas ----
    std::set<std::string> usadas;
    for (size_t k = 0; k < gLibs.size(); k++) {
        LibPlan& L = gLibs[k];
        std::vector<const std::string*> ptrs, ptrsJer;
        for (size_t c = 0; c < L.bloques.size(); c++) ptrs.push_back(&L.bloques[c]);
        for (size_t c = 0; c < L.bloquesJer.size(); c++) ptrsJer.push_back(&L.bloquesJer[c]);
        std::string bytes;
        W3dAnimSetArmar(L.huesos, ptrs, L.ajenos, bytes, &ptrsJer);
        W3dAnimSetHuella((const unsigned char*)bytes.data(), bytes.size(), L.huella1, L.huella2);
        L.tam = bytes.size();
        L.entrada = EntradaLibre(esc, L.nombre, usadas);
        usadas.insert(L.entrada);
        L.carpeta = CarpetaDeRegistro(L.nombre);
        if (esc) esc->AgregarBytes(L.entrada, bytes, false);
        W3dAnimSetFila f; f.nombre = L.nombre; f.entrada = L.entrada; f.carpeta = L.carpeta;
        gRegistroNuevo.push_back(f);
        gInfo.animsets++;
        gInfo.clipsEscritos += (int)L.bloques.size();
        gInfo.bytes += (long)bytes.size();
        // los bloques ya los tiene el escritor: no hace falta tenerlos dos veces hasta el final
        std::vector<std::string>().swap(L.bloques);
        std::vector<std::string>().swap(L.bloquesJer);
        L.porHuella.clear();
    }
    // ---- 5) los HUERFANOS del registro: sin usuarios, se conservan tal cual ----
    const std::vector<W3dAnimSetFila> reg = W3dAnimSetsRegistro();
    for (size_t i = 0; i < reg.size(); i++) {
        if (LibPorNombre(reg[i].nombre) >= 0) continue;
        bool ya = false;
        for (size_t k = 0; k < gRegistroNuevo.size(); k++) if (gRegistroNuevo[k].nombre == reg[i].nombre) ya = true;
        if (ya) continue;
        std::vector<unsigned char> d;
        if (!w3dFileSystem::ReadFileBytes(reg[i].entrada, d) || d.empty()) {
            w3dLogfW("[W3D] el animset huerfano '%s' (%s) ya no se puede leer: sale del registro",
                     reg[i].nombre.c_str(), reg[i].entrada.c_str());
            continue;
        }
        W3dAnimSetFila f;
        f.nombre = reg[i].nombre;
        f.entrada = EntradaLibre(esc, f.nombre, usadas);
        usadas.insert(f.entrada);
        f.carpeta = reg[i].carpeta;
        if (esc) esc->AgregarBytes(f.entrada, std::string((const char*)&d[0], d.size()), false);
        gRegistroNuevo.push_back(f);
        gInfo.huerfanos++;
        gInfo.bytes += (long)d.size();
    }
    std::sort(gRegistroNuevo.begin(), gRegistroNuevo.end(), FilaMenor);
    if (gInfo.animsets || gInfo.huerfanos)
        w3dLogf("[W3D] animsets: %d (+%d huerfanos) para %d armatures: %d clips -> %d distintos; %d raices con %d clips de jerarquia; %ld bytes",
                gInfo.animsets, gInfo.huerfanos, gInfo.armatures, gInfo.clipsArmatures, gInfo.clipsEscritos,
                gInfo.raicesJer, gInfo.clipsJer, gInfo.bytes);
    return true;
}

bool GuardarAnimSetsJerDe(const Object* o, std::string& animset) {
    std::map<const Object*, std::string>::const_iterator ie = gJerLibExt.find(o);
    if (ie != gJerLibExt.end()) { animset = ie->second; return true; }   // (la de una libreria: por su nombre)
    std::map<const Object*, int>::const_iterator it = gJerLib.find(o);
    if (it == gJerLib.end() || it->second < 0 || it->second >= (int)gLibs.size()) return false;
    animset = gLibs[(size_t)it->second].nombre;
    return true;
}

bool GuardarAnimSetsDe(const Armature* a, std::string& animset, std::vector<int>& clips) {
    std::map<const Armature*, std::pair<std::string, std::vector<int> > >::const_iterator il = gArmLib.find(a);
    if (il != gArmLib.end()) { animset = il->second.first; clips = il->second.second; return !clips.empty(); }
    std::map<const Armature*, size_t>::const_iterator it = gArmIdx.find(a);
    if (it == gArmIdx.end()) return false;
    const ArmPlan& P = gArms[it->second];
    if (P.lib < 0 || P.clips.empty()) return false;
    animset = gLibs[P.lib].nombre;
    clips = P.clips;
    return true;
}

void GuardarAnimSetsRegistro(std::string& s) {
    if (gRegistroNuevo.empty()) return;
    s += "  \"animsets\": [\n";
    for (size_t i = 0; i < gRegistroNuevo.size(); i++) {
        const W3dAnimSetFila& f = gRegistroNuevo[i];
        s += "    { \"nombre\": "; JTexto(s, f.nombre);
        s += ", \"entrada\": "; JTexto(s, f.entrada);
        s += ", \"carpeta\": "; JTexto(s, W3dRutaCosmeticaJson(f.carpeta));
        s += " }";
        if (i + 1 < gRegistroNuevo.size()) s += ",";
        s += "\n";
    }
    s += "  ],\n";
}

void GuardarAnimSetsConfirmar() {
    W3dAnimSetsRegistroFijar(gRegistroNuevo);
    // ---- los RECURSOS cargados: su id es el nombre de la entrada de la que salieron ----
    //  - biblioteca que se escribio IGUAL a lo que el recurso tiene en memoria: el recurso pasa a
    //    llamarse como su entrada nueva (un armature que se cargue despues lo comparte);
    //  - biblioteca que CAMBIO (se le sumaron clips, se re-nombro contra otra): el recurso se
    //    desengancha a un id que no es una entrada, y el que pida la entrada lee la nueva;
    //  - cualquier otro descriptor con el nombre de una entrada recien escrita, idem (su dato
    //    es el de antes); si NADIE lo referencia y no tiene dato (el FALLO recordado de una
    //    entrada rota que el guardado reescribio) se OLVIDA: correrlo de clave lo dejaba en el
    //    almacen para siempre (ver W3dAnimSetDesenganchar).
    //  En dos pasadas (primero a ids temporales) para que dos renombres cruzados no choquen.
    //  El id desenganchado se arma sobre la ENTRADA (W3dAnimSetIdEntrada): re-desenganchar el
    //  mismo recurso en otro guardado no le acumula prefijos.
    static int nDesenganchados = 0;
    std::vector<W3dRecurso*> todos;
    W3dRecursosListar(W3DREC_ANIMSET, todos);
    std::set<std::string> nuevas;
    for (size_t k = 0; k < gRegistroNuevo.size(); k++) nuevas.insert(gRegistroNuevo[k].entrada);
    std::vector<std::pair<W3dRecurso*, std::string> > destinos;
    std::vector<W3dRecurso*> olvidar;
    for (size_t i = 0; i < todos.size(); i++) {
        W3dRecurso* r = todos[i];
        if (!r) continue;
        const LibPlan* L = 0;
        for (size_t k = 0; k < gLibs.size() && !L; k++) if (gLibs[k].rec == r) L = &gLibs[k];
        std::string destino;
        if (L) {
            W3dAnimSet* set = (r->estado == W3DREC_LISTO) ? (W3dAnimSet*)r->dato : 0;
            // una biblioteca que se escribio ENTERA desde este recurso (la de unos clips de jerarquia, que se
            // editan en el recurso mismo: es la copia viva) es su entrada nueva por construccion: su huella pasa
            // a ser la de lo escrito (si no, se desenganchaba y el proximo que la pidiera leeria otra copia)
            if (set && gLibsDesdeRecurso.count((int)(L - &gLibs[0]))) {
                set->huella1 = L->huella1; set->huella2 = L->huella2; set->tamArchivo = L->tam;
            }
            const bool igual = set && set->tamArchivo == L->tam && set->huella1 == L->huella1 && set->huella2 == L->huella2;
            if (igual) { destino = L->entrada; set->nombre = L->nombre; }
        } else if (!nuevas.count(r->id)) continue;       // no choca con nada: no se toca
        else if (r->refTotal <= 0 && r->estado != W3DREC_LISTO) { olvidar.push_back(r); continue; }
        if (destino.empty()) {
            char b[48]; sprintf(b, "#guardado%d:", ++nDesenganchados);
            destino = std::string(b) + W3dAnimSetIdEntrada(r->id);
        }
        if (destino != r->id) destinos.push_back(std::make_pair(r, destino));
    }
    // (antes de los renombres: la clave que ocupaban queda libre para el que llega)
    for (size_t i = 0; i < olvidar.size(); i++) W3dAnimSetDesenganchar(olvidar[i], "guardado", ++nDesenganchados);
    for (size_t i = 0; i < destinos.size(); i++) {
        char b[48]; sprintf(b, "#tmp%d:", (int)i);
        W3dRecursoRenombrar(W3DREC_ANIMSET, destinos[i].first->id, std::string(b) + destinos[i].first->id);
    }
    for (size_t i = 0; i < destinos.size(); i++)
        if (!W3dRecursoRenombrar(W3DREC_ANIMSET, destinos[i].first->id, destinos[i].second))
            w3dLogfW("[W3D] animset: no pude re-nombrar el recurso a '%s'", destinos[i].second.c_str());
    // ---- los armatures con clips PROPIOS quedan anotados en su biblioteca (el proximo guardado
    //      los vuelve a poner ahi aunque en el medio se cambie el esqueleto de otro) ----
    for (size_t i = 0; i < gArms.size(); i++)
        if (gArms[i].a && gArms[i].lib >= 0) gArms[i].a->animSetNombre = gLibs[gArms[i].lib].nombre;
    // ---- los objetos raiz de clips de jerarquia: nombran la biblioteca que quedo (renombrada contra otra, o
    //      la PRIMERA de dos iguales: esos pasan a su recurso y sueltan el de la copia) ----
    for (std::map<const Object*, int>::const_iterator it = gJerLib.begin(); it != gJerLib.end(); ++it) {
        Object* o = (Object*)it->first;
        if (!o->clipsJer || it->second < 0 || it->second >= (int)gLibs.size()) continue;
        const LibPlan& L = gLibs[(size_t)it->second];
        if (L.rec && o->clipsJer->rec != L.rec) {
            W3dRecursoRetener(L.rec, W3DREC_PERMANENTE);
            W3dRecurso* viejo = o->clipsJer->rec;
            // las VISTAS de edicion de sus clips (la que se esta editando y las de las raices que no se miran)
            // pasan a los clips equivalentes de la biblioteca que queda (el mismo contenido, el mismo orden): si
            // no, quedaban HUERFANAS y lo que se keyeaba despues en ellas no llegaba a ningun clip
            const W3dAnimSet* sv = (viejo && viejo->estado == W3DREC_LISTO) ? (const W3dAnimSet*)viejo->dato : 0;
            const W3dAnimSet* sn = (L.rec->estado == W3DREC_LISTO) ? (const W3dAnimSet*)L.rec->dato : 0;
            if (sv && sn) {
                std::vector<std::vector<SceneAnimation*>*> listas;
                W3dRaicesListasAnim(listas);
                for (size_t k = 0; k < listas.size(); k++)
                    W3dClipsVistasRecambiar(*listas[k], o, sv->datos.jerarquias, sn->datos.jerarquias);
            }
            o->clipsJer->rec = L.rec;
            if (viejo) W3dRecursoSoltar(viejo, W3DREC_PERMANENTE);
        }
        o->clipsJer->animset = L.nombre;
    }
    gLibs.clear(); gArms.clear(); gArmIdx.clear(); gJerLib.clear(); gLibsDesdeRecurso.clear();
}

// ============================================================================
//  LOS ANIMSETS COMO RECURSOS DEL PROYECTO (ver GuardarAnimSets.h)
// ============================================================================
int W3dAnimSetUsuarios(Object* raiz, const std::string& nombre, std::vector<Armature*>* out, std::vector<Object*>* raicesJer) {
    if (out) out->clear();
    if (raicesJer) raicesJer->clear();
    std::vector<Armature*> arms;
    JuntarRaiz(raiz, arms);
    int n = 0;
    // los objetos raiz que la usan como biblioteca de clips de jerarquia (con clips o sin ellos: la nombran)
    std::vector<Object*> jer;
    JuntarJerRaiz(raiz, jer);
    for (size_t i = 0; i < jer.size(); i++) {
        if (jer[i]->clipsJer->animset != nombre) continue;
        n++;
        if (raicesJer) raicesJer->push_back(jer[i]);
    }
    for (size_t i = 0; i < arms.size(); i++) {
        // el MISMO criterio que el plan del guardado (GuardarAnimSetsPreparar): sin clips no es
        // usuario de nada; colgado del recurso Y con el esqueleto que calza, su nombre manda; si
        // no (clips propios, o colgado con un hueso renombrado despues: el guardado lo manda a la
        // biblioteca de su esqueleto y Confirmar se la anota en animSetNombre), animSetNombre.
        if (arms[i]->animations.empty()) continue;
        const W3dAnimSet* set = W3dArmatureAnimSetCalza(arms[i]) ? W3dArmatureAnimSet(arms[i]) : 0;
        const bool usa = set ? (set->nombre == nombre) : (arms[i]->animSetNombre == nombre);
        if (!usa) continue;
        n++;
        if (out) out->push_back(arms[i]);
    }
    return n;
}

bool W3dAnimSetRenombrar(Object* raiz, const std::string& viejo, const std::string& nuevo, std::string* final) {
    std::vector<W3dAnimSetFila> reg = W3dAnimSetsRegistro();
    int fila = -1;
    for (size_t i = 0; i < reg.size(); i++) if (reg[i].nombre == viejo) { fila = (int)i; break; }
    if (fila < 0) return false;
    // unico entre los OTROS del registro (renombrarlo como estaba no le suma un .001)
    std::vector<std::string> tomados;
    for (size_t i = 0; i < reg.size(); i++) if ((int)i != fila) tomados.push_back(reg[i].nombre);
    const std::string n = W3dNombreUnicoEnValores(W3dNombreNormalizar(nuevo, "Animaciones"), "Animaciones", tomados, -1);
    if (final) *final = n;
    if (n == viejo) return true;
    // los usuarios ANTES de tocar nada (se buscan por el nombre viejo)
    std::vector<Armature*> usuarios;
    std::vector<Object*> raicesJer;
    W3dAnimSetUsuarios(raiz, viejo, &usuarios, &raicesJer);
    reg[fila].nombre = n;
    W3dAnimSetsRegistroFijar(reg);
    // el recurso cargado (su nombre es el que el guardado usa para los armatures colgados)
    std::vector<W3dRecurso*> todos;
    W3dRecursosListar(W3DREC_ANIMSET, todos);
    for (size_t i = 0; i < todos.size(); i++) {
        if (!todos[i] || todos[i]->estado != W3DREC_LISTO || !todos[i]->dato) continue;
        W3dAnimSet* set = (W3dAnimSet*)todos[i]->dato;
        if (set->nombre == viejo) set->nombre = n;
    }
    for (size_t i = 0; i < usuarios.size(); i++) usuarios[i]->animSetNombre = n;
    for (size_t i = 0; i < raicesJer.size(); i++) raicesJer[i]->clipsJer->animset = n;
    w3dLogf("[W3D] animset '%s' -> '%s' (%d usuarios)", viejo.c_str(), n.c_str(), (int)usuarios.size());
    return true;
}

bool W3dAnimSetFijarCarpeta(const std::string& nombre, const std::string& carpeta) {
    std::vector<W3dAnimSetFila> reg = W3dAnimSetsRegistro();
    for (size_t i = 0; i < reg.size(); i++) {
        if (reg[i].nombre != nombre) continue;
        reg[i].carpeta = carpeta;
        W3dAnimSetsRegistroFijar(reg);
        return true;
    }
    return false;
}

int W3dAnimSetsPurgarHuerfanos(Object* raiz, std::vector<std::string>* nombres) {
    if (nombres) nombres->clear();
    const std::vector<W3dAnimSetFila> reg = W3dAnimSetsRegistro();
    std::vector<W3dAnimSetFila> quedan;
    int n = 0;
    for (size_t i = 0; i < reg.size(); i++) {
        if (W3dAnimSetUsuarios(raiz, reg[i].nombre, 0, 0) > 0) { quedan.push_back(reg[i]); continue; }
        n++;
        if (nombres) nombres->push_back(reg[i].nombre);
        w3dLogf("[W3D] animset huerfano purgado: '%s' (%s)", reg[i].nombre.c_str(), reg[i].entrada.c_str());
    }
    if (n) W3dAnimSetsRegistroFijar(quedan);
    return n;
}
