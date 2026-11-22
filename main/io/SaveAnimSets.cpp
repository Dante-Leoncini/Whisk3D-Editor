#include "io/SaveAnimSets.h"
#include "io/W3dContainer.h"           // el escritor del contenedor + W3dSlugEntry
#include "animation/W3dAnimSet.h"      // el formato .w3da, el recurso y el registro
#include "animation/SkeletalAnimation.h"
#include "objects/Armature.h"
#include "objects/Objects.h"
#include "W3dRoots.h"                  // los armatures de TODAS las raices (escenas/prefabs)
#include "io/W3dResources.h"
#include "io/w3dFilesystem.h"          // los HUERFANOS se copian de la entrada del contenedor anterior
#include "base/W3dNames.h"             // W3dUniqueNameInValues: la regla .NNN de siempre
#include "w3dlog.h"
#include "W3dNotice.h"                  // el freno se VE en pantalla, no solo en el log
#include "io/Libraries.h"               // W3dLibRefOf: el animset de una libreria que no esta (se nombra igual)

#include <map>
#include <set>
#include <algorithm>
#include <stdio.h>

bool g_w3dAnimsInline = false;

namespace {

// una biblioteca del plan
struct LibPlan {
    std::string name, entry, folder;
    std::vector<std::string> bones;               // la tabla HUES (el espacio de indices)
    std::vector<std::string> blocks;              // los CLIP serializados, en el orden de la biblioteca
    std::vector<std::string> blocksHier;          // los JCLP (clips de jerarquia), en el orden de su recurso
    std::map<unsigned, std::vector<int> > byFootprint;   // huella del bloque -> candidatos (dedup)
    W3dResource* rec;                              // el recurso del que viene (NULL = nueva)
    const std::vector<W3dAnimSetForeignBlock>* foreign;
    // la HUELLA del .w3da escrito (los bytes ya se le pasaron al escritor): con ella Confirmar
    // sabe si el recurso cargado quedo igual a su entrada nueva
    unsigned footprint1, footprint2;
    size_t   size;
    LibPlan() : rec(0), foreign(0), footprint1(0), footprint2(0), size(0) {}
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
std::map<const Object*, int> gHierLib;
std::set<int> gLibsFromResource;
std::vector<W3dAnimSetRow> gNewRegistry;
// los que usan un animset de una LIBRERIA externa (io/Libraries.h): no entran al plan (no se escriben en el
// proyecto); su nodo lo NOMBRA con su prefijo ("Personajes/Caminar") y los clips que usa de el
std::map<const Armature*, std::pair<std::string, std::vector<int> > > gArmLib;
std::map<const Object*, std::string> gHierLibExt;
// el recurso es de una libreria? (su clave en el almacen es su entrada "lib:<libreria>/...")
bool LibResource(const W3dResource* r) { return r && r->id.compare(0, 4, "lib:") == 0; }
SaveAnimSetsInfo gInfo;

// (lo que genera un PROXY de una libreria no se guarda: sus armatures y sus clips son de la libreria)
void Collect(Object* o, std::vector<Armature*>& out) {
    if (!o || o->getType() == ObjectType::proxy) return;
    if (o->getType() == ObjectType::armature) out.push_back((Armature*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) Collect(o->Childrens[i], out);
}
// los objetos con biblioteca de clips de jerarquia (en preorden, como los armatures)
void CollectHier(Object* o, std::vector<Object*>& out) {
    if (!o || o->getType() == ObjectType::proxy) return;
    if (o->clipsHier && !o->clipsHier->animset.empty()) out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectHier(o->Childrens[i], out);
}
void CollectHierRoot(Object* root, std::vector<Object*>& out) {
    if (root) { CollectHier(root, out); return; }
    std::vector<Object*> roots;
    W3dRootsInOrder(roots);
    for (size_t i = 0; i < roots.size(); i++) CollectHier(roots[i], out);
}
// los armatures de 'raiz' o, con NULL, los de TODAS las raices del proyecto (la activa y las
// escenas/prefabs que no se estan mirando: ver W3dLiveRoots)
void CollectRoot(Object* root, std::vector<Armature*>& out) {
    if (root) { Collect(root, out); return; }
    std::vector<Object*> roots;
    W3dRootsInOrder(roots);   // (en el orden del registro: el guardado no depende de la escena activa)
    for (size_t i = 0; i < roots.size(); i++) Collect(roots[i], out);
}

int LibByName(const std::string& n) {
    for (size_t i = 0; i < gLibs.size(); i++) if (gLibs[i].name == n) return (int)i;
    return -1;
}

std::string FreeName(const std::string& base, const std::vector<std::string>& extra) {
    std::vector<std::string> taken = extra;
    for (size_t i = 0; i < gLibs.size(); i++) taken.push_back(gLibs[i].name);
    return W3dUniqueNameInValues(base, "Animations", taken, -1);
}

std::string RegistryFolder(const std::string& name) {
    const std::vector<W3dAnimSetRow>& r = W3dAnimSetsRegistry();
    for (size_t i = 0; i < r.size(); i++) if (r[i].name == name) return r[i].folder;
    return std::string();
}

unsigned FootprintBlock(const std::string& b) {
    unsigned h1, h2;
    W3dAnimSetFootprint((const unsigned char*)b.data(), b.size(), h1, h2);
    return h1 ^ (h2 * 2654435761u);
}

// el indice del bloque en la biblioteca (lo agrega si es nuevo): el DEDUP POR CLIP
int AddClip(LibPlan& L, const std::string& block) {
    const unsigned h = FootprintBlock(block);
    std::vector<int>& cand = L.byFootprint[h];
    for (size_t i = 0; i < cand.size(); i++) if (L.blocks[cand[i]] == block) return cand[i];
    L.blocks.push_back(block);
    cand.push_back((int)L.blocks.size() - 1);
    return (int)L.blocks.size() - 1;
}

std::string FreeEntry(W3dContainerWriter* esc, const std::string& name, const std::set<std::string>& used) {
    const std::string base = "animations/" + W3dSlugEntry(name);
    std::string e = base + ".w3da";
    for (int k = 2; (esc && esc->Has(e)) || used.count(e); k++) {
        char b[16]; sprintf(b, "-%d", k);
        e = base + b + ".w3da";
    }
    return e;
}

bool RowLower(const W3dAnimSetRow& a, const W3dAnimSetRow& b) { return a.name < b.name; }

// un string del JSON: el mismo escape que JEsc de SaveW3D.cpp (lo que entiende JParser)
void JText(std::string& s, const std::string& v) {
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

const SaveAnimSetsInfo& SaveLastAnimSets() { return gInfo; }

bool SaveAnimSetsPrepare(W3dContainerWriter* esc, Object* root) {
    gLibs.clear(); gArms.clear(); gArmIdx.clear(); gNewRegistry.clear();
    gHierLib.clear(); gLibsFromResource.clear();
    gArmLib.clear(); gHierLibExt.clear();
    gInfo = SaveAnimSetsInfo();
    std::vector<Armature*> arms;
    CollectRoot(root, arms);
    std::vector<Object*> hier;
    CollectHierRoot(root, hier);
    // los que siguen COLGADOS de un animset de una LIBRERIA: lo nombran y salen del plan
    for (size_t i = arms.size(); i-- > 0; ) {
        Armature* a = arms[i];
        if (a->animations.empty() || !W3dArmatureAnimSetFits(a) || !LibResource(W3dArmatureAnimSetResource(a))) continue;
        std::vector<int> idx;
        W3dArmatureAnimsIndices(a, idx);
        gArmLib[a] = std::make_pair(a->animSetName, idx);
        arms.erase(arms.begin() + (long)i);
    }
    // ...y los que quedaron SIN clips porque su animset es de una libreria que NO ESTA (io/Libraries.h): su referencia se
    // escribe tal cual (vuelve a resolverse al vincularla); no son un animset del proyecto ni frenan el guardado
    for (size_t i = arms.size(); i-- > 0; ) {
        const W3dLibRef* ref = arms[i]->animations.empty() ? W3dLibRefOf(arms[i], W3D_LIBREF_ANIMSET) : NULL;
        if (!ref) continue;
        gArmLib[arms[i]] = std::make_pair(ref->name, ref->clips);
        arms.erase(arms.begin() + (long)i);
    }
    for (size_t i = hier.size(); i-- > 0; )
        if (LibResource(hier[i]->clipsHier->rec)) {
            gHierLibExt[hier[i]] = hier[i]->clipsHier->animset;
            hier.erase(hier.begin() + (long)i);
        }
    // ---- FRENO: una biblioteca de clips de jerarquia que no se pudo leer al abrir no se pisa ----
    for (size_t i = 0; i < hier.size(); i++)
        if (!hier[i]->clipsHier->notLoaded.empty()) {
            w3dLogfE("[W3D] '%s': sus clips (biblioteca '%s') no se pudieron leer al abrir: NO guardo encima",
                     hier[i]->name.c_str(), hier[i]->clipsHier->notLoaded.c_str());
            W3dAvisof(true, T("Not saving: the clips of '%s' could not be read when opening and saving would erase them"),
                      W3dShortName(hier[i]->name).c_str());
            return false;
        }
    // ---- FRENO: un animset que no se pudo leer al abrir no se pisa ----
    for (size_t i = 0; i < arms.size(); i++)
        if (!arms[i]->animSetNotLoaded.empty()) {
            w3dLogfE("[W3D] '%s': sus animaciones (animset '%s') no se pudieron leer al abrir: NO guardo encima",
                     arms[i]->name.c_str(), arms[i]->animSetNotLoaded.c_str());
            W3dAvisof(true, T("Not saving: the animations of '%s' could not be read when opening and saving would erase them"),
                      W3dShortName(arms[i]->name).c_str());
            return false;
        }
    // el formato INLINE de antes (solo el harness): nada de animsets, ni siquiera los huerfanos
    if (g_w3dAnimsInline) return true;

    // ---- 1) los armatures que siguen COLGADOS de un animset: se quedan en el (y con su nombre),
    //         salvo que su esqueleto ya no calce con la tabla como al asignarlo (un hueso renombrado
    //         sin tocar los clips): esos van al paso 2 como si sus clips fueran propios ----
    std::vector<char> attached(arms.size(), 0);
    for (size_t i = 0; i < arms.size(); i++) attached[i] = W3dArmatureAnimSetFits(arms[i]) ? 1 : 0;
    for (size_t i = 0; i < arms.size(); i++) {
        Armature* a = arms[i];
        if (a->animations.empty() || !attached[i]) continue;
        W3dAnimSet* set = W3dArmatureAnimSet(a);
        if (!set) continue;
        W3dResource* r = W3dArmatureAnimSetResource(a);
        int li = -1;
        for (size_t k = 0; k < gLibs.size(); k++) if (gLibs[k].rec == r) { li = (int)k; break; }
        if (li < 0) {
            LibPlan L;
            L.name  = FreeName(set->name.empty() ? a->name : set->name, std::vector<std::string>());
            L.bones = set->data.bones;
            L.rec = r;
            L.foreign = &set->data.foreign;
            gLibs.push_back(L);
        }
    }
    // ---- 2) los de clips PROPIOS (editados, importados, de un .w3d viejo): a la biblioteca
    //         de su esqueleto (primero la que traian, despues cualquiera con los mismos
    //         huesos) o a una nueva con el nombre del animset de donde vinieron / del armature ----
    std::vector<int> libOf(arms.size(), -1);
    for (size_t i = 0; i < arms.size(); i++) {
        Armature* a = arms[i];
        if (a->animations.empty()) continue;
        if (attached[i]) {
            W3dResource* r = W3dArmatureAnimSetResource(a);
            for (size_t k = 0; k < gLibs.size(); k++) if (gLibs[k].rec == r) { libOf[i] = (int)k; break; }
            continue;
        }
        std::vector<std::string> bones;
        W3dArmatureBoneNames(a, bones);
        int li = -1;
        if (!a->animSetName.empty()) {
            const int k = LibByName(a->animSetName);
            if (k >= 0 && gLibs[k].bones == bones) li = k;
        }
        for (size_t k = 0; li < 0 && k < gLibs.size(); k++) if (gLibs[k].bones == bones) li = (int)k;
        if (li < 0) {
            // el nombre no pisa a otro animset del REGISTRO (un huerfano se conserva con el suyo),
            // salvo el que este armature traia: es el mismo animset, editado
            std::vector<std::string> busy;
            const std::vector<W3dAnimSetRow>& reg = W3dAnimSetsRegistry();
            for (size_t k = 0; k < reg.size(); k++) if (reg[k].name != a->animSetName) busy.push_back(reg[k].name);
            LibPlan L;
            L.name  = FreeName(a->animSetName.empty() ? a->name : a->animSetName, busy);
            L.bones = bones;
            gLibs.push_back(L);
            li = (int)gLibs.size() - 1;
        }
        libOf[i] = li;
    }
    // ---- 3) los clips, en preorden: cada uno traducido al espacio de huesos de su biblioteca
    //         y deduplicado ahi (el mismo clip en diez armatures = UN bloque) ----
    //  Un armature COLGADO escribe el clip CANONICO del recurso (ya esta en el espacio de la
    //  tabla: mapa identidad), no el que tiene en la mano: si usa una VARIANTE, esa copia tiene
    //  en -1 las pistas de los huesos que a SU esqueleto le faltan, y volver al espacio de la
    //  tabla desde ahi las dejaba sin hueso para siempre (y el clip ya no deduplicaba con el
    //  canonico de los demas usuarios: la biblioteca se duplicaba en cada guardado).
    std::vector<int> identity;
    for (size_t i = 0; i < arms.size(); i++) {
        Armature* a = arms[i];
        if (libOf[i] < 0) continue;
        LibPlan& L = gLibs[libOf[i]];
        std::vector<int> map;
        W3dAnimSetMapFromArmature(L.bones, a, map);
        // colgado: el indice de cada clip en el recurso (-1 = no esta; no deberia pasar)
        const W3dAnimSet* set = (attached[i] && L.rec == W3dArmatureAnimSetResource(a)) ? W3dArmatureAnimSet(a) : 0;
        std::vector<int> idx;
        if (set) {
            W3dArmatureAnimsIndices(a, idx);
            identity.resize(L.bones.size());
            for (size_t h = 0; h < identity.size(); h++) identity[h] = (int)h;
        }
        ArmPlan P;
        P.a = a; P.lib = libOf[i];
        std::string block;
        for (size_t c = 0; c < a->animations.size(); c++) {
            if (!a->animations[c]) continue;
            const int k = (set && c < idx.size()) ? idx[c] : -1;
            if (k >= 0 && k < (int)set->data.clips.size() && set->data.clips[k])
                W3dAnimSetClipBlock(*set->data.clips[k], identity, (int)L.bones.size(), block);
            else
                W3dAnimSetClipBlock(*a->animations[c], map, (int)L.bones.size(), block);
            P.clips.push_back(AddClip(L, block));
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
        std::map<const W3dResource*, int> recLib;          // recurso -> biblioteca del plan
        std::map<std::string, int> byContent;              // bytes de sus clips de jerarquia -> biblioteca
        std::vector<int> identityH;
        for (size_t i = 0; i < hier.size(); i++) {
            Object* o = hier[i];
            W3dResource* r = o->clipsHier->rec;
            const W3dAnimSet* set = (r && r->state == W3DREC_READY) ? (const W3dAnimSet*)r->data : 0;
            if (!set) continue;
            std::map<const W3dResource*, int>::const_iterator already = recLib.find(r);
            if (already != recLib.end()) { gHierLib[o] = already->second; continue; }
            std::vector<std::string> blocksHier(set->data.hierarchies.size());
            std::string content;
            for (size_t c = 0; c < set->data.hierarchies.size(); c++) {
                W3dAnimSetClipHierBlock(*set->data.hierarchies[c], blocksHier[c]);
                content += blocksHier[c];
            }
            // una biblioteca que el plan de los armatures ya tiene (un animset con clips de los dos tipos)
            int li = -1;
            for (size_t k = 0; k < gLibs.size() && li < 0; k++) if (gLibs[k].rec == r) li = (int)k;
            if (li < 0 && set->data.clips.empty() && !content.empty()) {
                std::map<std::string, int>::const_iterator equal = byContent.find(content);
                if (equal != byContent.end()) {
                    recLib[r] = equal->second; gHierLib[o] = equal->second;
                    w3dLogf("[W3D] clips de jerarquia: la biblioteca '%s' es igual a '%s': queda una",
                            set->name.c_str(), gLibs[(size_t)equal->second].name.c_str());
                    continue;
                }
            }
            if (li < 0) {
                LibPlan L;
                L.name  = FreeName(set->name.empty() ? o->name : set->name, std::vector<std::string>());
                L.bones = set->data.bones;
                L.rec = r;
                L.foreign = &set->data.foreign;
                // sus clips de esqueleto (si tiene) tal cual: nadie los esta usando en el plan de arriba
                identityH.resize(L.bones.size());
                for (size_t h = 0; h < identityH.size(); h++) identityH[h] = (int)h;
                std::string block;
                for (size_t c = 0; c < set->data.clips.size(); c++)
                    if (set->data.clips[c]) {
                        W3dAnimSetClipBlock(*set->data.clips[c], identityH, (int)L.bones.size(), block);
                        AddClip(L, block);
                    }
                gLibs.push_back(L);
                li = (int)gLibs.size() - 1;
                gLibsFromResource.insert(li);
            }
            gLibs[(size_t)li].blocksHier.swap(blocksHier);
            if (!content.empty() && set->data.clips.empty()) byContent[content] = li;
            recLib[r] = li;
            gHierLib[o] = li;
            gInfo.clipsHier += (int)gLibs[(size_t)li].blocksHier.size();
        }
        gInfo.rootsHier = (int)gHierLib.size();
    }
    // ---- 4) las entradas ----
    std::set<std::string> used;
    for (size_t k = 0; k < gLibs.size(); k++) {
        LibPlan& L = gLibs[k];
        std::vector<const std::string*> ptrs, ptrsHier;
        for (size_t c = 0; c < L.blocks.size(); c++) ptrs.push_back(&L.blocks[c]);
        for (size_t c = 0; c < L.blocksHier.size(); c++) ptrsHier.push_back(&L.blocksHier[c]);
        std::string bytes;
        W3dAnimSetBuild(L.bones, ptrs, L.foreign, bytes, &ptrsHier);
        W3dAnimSetFootprint((const unsigned char*)bytes.data(), bytes.size(), L.footprint1, L.footprint2);
        L.size = bytes.size();
        L.entry = FreeEntry(esc, L.name, used);
        used.insert(L.entry);
        L.folder = RegistryFolder(L.name);
        if (esc) esc->AddBytes(L.entry, bytes, false);
        W3dAnimSetRow f; f.name = L.name; f.entry = L.entry; f.folder = L.folder;
        gNewRegistry.push_back(f);
        gInfo.animsets++;
        gInfo.writtenClips += (int)L.blocks.size();
        gInfo.bytes += (long)bytes.size();
        // los bloques ya los tiene el escritor: no hace falta tenerlos dos veces hasta el final
        std::vector<std::string>().swap(L.blocks);
        std::vector<std::string>().swap(L.blocksHier);
        L.byFootprint.clear();
    }
    // ---- 5) los HUERFANOS del registro: sin usuarios, se conservan tal cual ----
    const std::vector<W3dAnimSetRow> reg = W3dAnimSetsRegistry();
    for (size_t i = 0; i < reg.size(); i++) {
        if (LibByName(reg[i].name) >= 0) continue;
        bool already = false;
        for (size_t k = 0; k < gNewRegistry.size(); k++) if (gNewRegistry[k].name == reg[i].name) already = true;
        if (already) continue;
        std::vector<unsigned char> d;
        if (!w3dFileSystem::ReadFileBytes(reg[i].entry, d) || d.empty()) {
            w3dLogfW("[W3D] el animset huerfano '%s' (%s) ya no se puede leer: sale del registro",
                     reg[i].name.c_str(), reg[i].entry.c_str());
            continue;
        }
        W3dAnimSetRow f;
        f.name = reg[i].name;
        f.entry = FreeEntry(esc, f.name, used);
        used.insert(f.entry);
        f.folder = reg[i].folder;
        if (esc) esc->AddBytes(f.entry, std::string((const char*)&d[0], d.size()), false);
        gNewRegistry.push_back(f);
        gInfo.orphans++;
        gInfo.bytes += (long)d.size();
    }
    std::sort(gNewRegistry.begin(), gNewRegistry.end(), RowLower);
    if (gInfo.animsets || gInfo.orphans)
        w3dLogf("[W3D] animsets: %d (+%d huerfanos) para %d armatures: %d clips -> %d distintos; %d raices con %d clips de jerarquia; %ld bytes",
                gInfo.animsets, gInfo.orphans, gInfo.armatures, gInfo.clipsArmatures, gInfo.writtenClips,
                gInfo.rootsHier, gInfo.clipsHier, gInfo.bytes);
    return true;
}

bool SaveAnimSetsHierOf(const Object* o, std::string& animset) {
    std::map<const Object*, std::string>::const_iterator ie = gHierLibExt.find(o);
    if (ie != gHierLibExt.end()) { animset = ie->second; return true; }   // (la de una libreria: por su nombre)
    std::map<const Object*, int>::const_iterator it = gHierLib.find(o);
    if (it == gHierLib.end() || it->second < 0 || it->second >= (int)gLibs.size()) return false;
    animset = gLibs[(size_t)it->second].name;
    return true;
}

bool SaveAnimSetsOf(const Armature* a, std::string& animset, std::vector<int>& clips) {
    std::map<const Armature*, std::pair<std::string, std::vector<int> > >::const_iterator il = gArmLib.find(a);
    if (il != gArmLib.end()) { animset = il->second.first; clips = il->second.second; return !clips.empty(); }
    std::map<const Armature*, size_t>::const_iterator it = gArmIdx.find(a);
    if (it == gArmIdx.end()) return false;
    const ArmPlan& P = gArms[it->second];
    if (P.lib < 0 || P.clips.empty()) return false;
    animset = gLibs[P.lib].name;
    clips = P.clips;
    return true;
}

void SaveAnimSetsRegistry(std::string& s) {
    if (gNewRegistry.empty()) return;
    s += "  \"animsets\": [\n";
    for (size_t i = 0; i < gNewRegistry.size(); i++) {
        const W3dAnimSetRow& f = gNewRegistry[i];
        s += "    { \"name\": "; JText(s, f.name);
        s += ", \"entry\": "; JText(s, f.entry);
        s += ", \"folder\": "; JText(s, W3dCosmeticPathJson(f.folder));
        s += " }";
        if (i + 1 < gNewRegistry.size()) s += ",";
        s += "\n";
    }
    s += "  ],\n";
}

void SaveAnimSetsConfirm() {
    W3dAnimSetsRegistrySet(gNewRegistry);
    // ---- los RECURSOS cargados: su id es el nombre de la entrada de la que salieron ----
    //  - biblioteca que se escribio IGUAL a lo que el recurso tiene en memoria: el recurso pasa a
    //    llamarse como su entrada nueva (un armature que se cargue despues lo comparte);
    //  - biblioteca que CAMBIO (se le sumaron clips, se re-nombro contra otra): el recurso se
    //    desengancha a un id que no es una entrada, y el que pida la entrada lee la nueva;
    //  - cualquier otro descriptor con el nombre de una entrada recien escrita, idem (su dato
    //    es el de antes); si NADIE lo referencia y no tiene dato (el FALLO recordado de una
    //    entrada rota que el guardado reescribio) se OLVIDA: correrlo de clave lo dejaba en el
    //    almacen para siempre (ver W3dAnimSetUnhook).
    //  En dos pasadas (primero a ids temporales) para que dos renombres cruzados no choquen.
    //  El id desenganchado se arma sobre la ENTRADA (W3dAnimSetIdEntry): re-desenganchar el
    //  mismo recurso en otro guardado no le acumula prefijos.
    static int nUnhooked = 0;
    std::vector<W3dResource*> all;
    W3dResourcesList(W3DREC_ANIMSET, all);
    std::set<std::string> newValue;
    for (size_t k = 0; k < gNewRegistry.size(); k++) newValue.insert(gNewRegistry[k].entry);
    std::vector<std::pair<W3dResource*, std::string> > destinations;
    std::vector<W3dResource*> forget;
    for (size_t i = 0; i < all.size(); i++) {
        W3dResource* r = all[i];
        if (!r) continue;
        const LibPlan* L = 0;
        for (size_t k = 0; k < gLibs.size() && !L; k++) if (gLibs[k].rec == r) L = &gLibs[k];
        std::string destination;
        if (L) {
            W3dAnimSet* set = (r->state == W3DREC_READY) ? (W3dAnimSet*)r->data : 0;
            // una biblioteca que se escribio ENTERA desde este recurso (la de unos clips de jerarquia, que se
            // editan en el recurso mismo: es la copia viva) es su entrada nueva por construccion: su huella pasa
            // a ser la de lo escrito (si no, se desenganchaba y el proximo que la pidiera leeria otra copia)
            if (set && gLibsFromResource.count((int)(L - &gLibs[0]))) {
                set->footprint1 = L->footprint1; set->footprint2 = L->footprint2; set->fileSize = L->size;
            }
            const bool equal = set && set->fileSize == L->size && set->footprint1 == L->footprint1 && set->footprint2 == L->footprint2;
            if (equal) { destination = L->entry; set->name = L->name; }
        } else if (!newValue.count(r->id)) continue;       // no choca con nada: no se toca
        else if (r->refTotal <= 0 && r->state != W3DREC_READY) { forget.push_back(r); continue; }
        if (destination.empty()) {
            char b[48]; sprintf(b, "#guardado%d:", ++nUnhooked);
            destination = std::string(b) + W3dAnimSetIdEntry(r->id);
        }
        if (destination != r->id) destinations.push_back(std::make_pair(r, destination));
    }
    // (antes de los renombres: la clave que ocupaban queda libre para el que llega)
    for (size_t i = 0; i < forget.size(); i++) W3dAnimSetUnhook(forget[i], "saved", ++nUnhooked);
    for (size_t i = 0; i < destinations.size(); i++) {
        char b[48]; sprintf(b, "#tmp%d:", (int)i);
        W3dResourceRename(W3DREC_ANIMSET, destinations[i].first->id, std::string(b) + destinations[i].first->id);
    }
    for (size_t i = 0; i < destinations.size(); i++)
        if (!W3dResourceRename(W3DREC_ANIMSET, destinations[i].first->id, destinations[i].second))
            w3dLogfW("[W3D] animset: no pude re-nombrar el recurso a '%s'", destinations[i].second.c_str());
    // ---- los armatures con clips PROPIOS quedan anotados en su biblioteca (el proximo guardado
    //      los vuelve a poner ahi aunque en el medio se cambie el esqueleto de otro) ----
    for (size_t i = 0; i < gArms.size(); i++)
        if (gArms[i].a && gArms[i].lib >= 0) gArms[i].a->animSetName = gLibs[gArms[i].lib].name;
    // ---- los objetos raiz de clips de jerarquia: nombran la biblioteca que quedo (renombrada contra otra, o
    //      la PRIMERA de dos iguales: esos pasan a su recurso y sueltan el de la copia) ----
    for (std::map<const Object*, int>::const_iterator it = gHierLib.begin(); it != gHierLib.end(); ++it) {
        Object* o = (Object*)it->first;
        if (!o->clipsHier || it->second < 0 || it->second >= (int)gLibs.size()) continue;
        const LibPlan& L = gLibs[(size_t)it->second];
        if (L.rec && o->clipsHier->rec != L.rec) {
            W3dResourceRetain(L.rec, W3DREC_PERMANENT);
            W3dResource* old = o->clipsHier->rec;
            // las VISTAS de edicion de sus clips (la que se esta editando y las de las raices que no se miran)
            // pasan a los clips equivalentes de la biblioteca que queda (el mismo contenido, el mismo orden): si
            // no, quedaban HUERFANAS y lo que se keyeaba despues en ellas no llegaba a ningun clip
            const W3dAnimSet* sv = (old && old->state == W3DREC_READY) ? (const W3dAnimSet*)old->data : 0;
            const W3dAnimSet* sn = (L.rec->state == W3DREC_READY) ? (const W3dAnimSet*)L.rec->data : 0;
            if (sv && sn) {
                std::vector<std::vector<SceneAnimation*>*> lists;
                W3dRootsListsAnim(lists);
                for (size_t k = 0; k < lists.size(); k++)
                    W3dClipsViewsSwap(*lists[k], o, sv->data.hierarchies, sn->data.hierarchies);
            }
            o->clipsHier->rec = L.rec;
            if (old) W3dResourceRelease(old, W3DREC_PERMANENT);
        }
        o->clipsHier->animset = L.name;
    }
    gLibs.clear(); gArms.clear(); gArmIdx.clear(); gHierLib.clear(); gLibsFromResource.clear();
}

// ============================================================================
//  LOS ANIMSETS COMO RECURSOS DEL PROYECTO (ver SaveAnimSets.h)
// ============================================================================
int W3dAnimSetUsers(Object* root, const std::string& name, std::vector<Armature*>* out, std::vector<Object*>* rootsHier) {
    if (out) out->clear();
    if (rootsHier) rootsHier->clear();
    std::vector<Armature*> arms;
    CollectRoot(root, arms);
    int n = 0;
    // los objetos raiz que la usan como biblioteca de clips de jerarquia (con clips o sin ellos: la nombran)
    std::vector<Object*> hier;
    CollectHierRoot(root, hier);
    for (size_t i = 0; i < hier.size(); i++) {
        if (hier[i]->clipsHier->animset != name) continue;
        n++;
        if (rootsHier) rootsHier->push_back(hier[i]);
    }
    for (size_t i = 0; i < arms.size(); i++) {
        // el MISMO criterio que el plan del guardado (SaveAnimSetsPrepare): sin clips no es
        // usuario de nada; colgado del recurso Y con el esqueleto que calza, su nombre manda; si
        // no (clips propios, o colgado con un hueso renombrado despues: el guardado lo manda a la
        // biblioteca de su esqueleto y Confirmar se la anota en animSetName), animSetName.
        if (arms[i]->animations.empty()) continue;
        const W3dAnimSet* set = W3dArmatureAnimSetFits(arms[i]) ? W3dArmatureAnimSet(arms[i]) : 0;
        const bool uses = set ? (set->name == name) : (arms[i]->animSetName == name);
        if (!uses) continue;
        n++;
        if (out) out->push_back(arms[i]);
    }
    return n;
}

bool W3dAnimSetRename(Object* root, const std::string& old, const std::string& newValue, std::string* final) {
    std::vector<W3dAnimSetRow> reg = W3dAnimSetsRegistry();
    int row = -1;
    for (size_t i = 0; i < reg.size(); i++) if (reg[i].name == old) { row = (int)i; break; }
    if (row < 0) return false;
    // unico entre los OTROS del registro (renombrarlo como estaba no le suma un .001)
    std::vector<std::string> taken;
    for (size_t i = 0; i < reg.size(); i++) if ((int)i != row) taken.push_back(reg[i].name);
    const std::string n = W3dUniqueNameInValues(W3dNameNormalize(newValue, "Animations"), "Animations", taken, -1);
    if (final) *final = n;
    if (n == old) return true;
    // los usuarios ANTES de tocar nada (se buscan por el nombre viejo)
    std::vector<Armature*> users;
    std::vector<Object*> rootsHier;
    W3dAnimSetUsers(root, old, &users, &rootsHier);
    reg[row].name = n;
    W3dAnimSetsRegistrySet(reg);
    // el recurso cargado (su nombre es el que el guardado usa para los armatures colgados)
    std::vector<W3dResource*> all;
    W3dResourcesList(W3DREC_ANIMSET, all);
    for (size_t i = 0; i < all.size(); i++) {
        if (!all[i] || all[i]->state != W3DREC_READY || !all[i]->data) continue;
        W3dAnimSet* set = (W3dAnimSet*)all[i]->data;
        if (set->name == old) set->name = n;
    }
    for (size_t i = 0; i < users.size(); i++) users[i]->animSetName = n;
    for (size_t i = 0; i < rootsHier.size(); i++) rootsHier[i]->clipsHier->animset = n;
    w3dLogf("[W3D] animset '%s' -> '%s' (%d usuarios)", old.c_str(), n.c_str(), (int)users.size());
    return true;
}

bool W3dAnimSetSetFolder(const std::string& name, const std::string& folder) {
    std::vector<W3dAnimSetRow> reg = W3dAnimSetsRegistry();
    for (size_t i = 0; i < reg.size(); i++) {
        if (reg[i].name != name) continue;
        reg[i].folder = folder;
        W3dAnimSetsRegistrySet(reg);
        return true;
    }
    return false;
}

int W3dAnimSetsPurgeOrphans(Object* root, std::vector<std::string>* names) {
    if (names) names->clear();
    const std::vector<W3dAnimSetRow> reg = W3dAnimSetsRegistry();
    std::vector<W3dAnimSetRow> remain;
    int n = 0;
    for (size_t i = 0; i < reg.size(); i++) {
        if (W3dAnimSetUsers(root, reg[i].name, 0, 0) > 0) { remain.push_back(reg[i]); continue; }
        n++;
        if (names) names->push_back(reg[i].name);
        w3dLogf("[W3D] animset huerfano purgado: '%s' (%s)", reg[i].name.c_str(), reg[i].entry.c_str());
    }
    if (n) W3dAnimSetsRegistrySet(remain);
    return n;
}
