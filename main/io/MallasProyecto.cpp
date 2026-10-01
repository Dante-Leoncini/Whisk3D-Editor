// ============================================================================
//  MallasProyecto.cpp — el lado del EDITOR de las mallas 3D como recurso.
//  Ver MallasProyecto.h y el contrato en libs/Whisk3DCore/objects/MallaRecurso.h.
// ============================================================================
#include "W3dRaices.h"          // las mallas de TODAS las raices (escenas/prefabs), en orden de registro
#include "io/MallasProyecto.h"
#include "io/W3dContenedor.h"          // el escritor del contenedor + W3dSlugEntrada
#include "io/GuardarW3D.h"             // g_w3dIndices16Simulado
#include "io/W3dMalla.h"               // W3dMallaBloqueQueFalta / W3dMallaNombreSano
#include "io/W3dMallaBin.h"
#include "objects/MallaRecurso.h"
#include "objects/Mesh.h"
#include "objects/Objects.h"
#include "objects/ObjectMode.h"        // W3dDuplicarUno
#include "objects/Primitivas.h"        // W3dMallaNacioHook: el Add de una primitiva nace con su malla 3D
#include "animation/VertexAnimation.h" // W3dReposoVertexAnim: la malla se serializa EN REPOSO
#include "edit/MeshEdit.h"             // W3dMallaIndicesCanonicos: el index buffer que arma el .w3dm al abrir
#include "undo/Undo.h"                 // UndoPushExterno: los pasos de undo de los vinculos
#include "base/W3dInteractionState.h"  // InteractionMode (Edit / Weight / Vertex paint)
#include "W3dAviso.h"                  // W3dAvisof / W3dNombreCorto
#include "W3dNombres.h"                // LA regla de nombres unicos (el nombre de una malla nueva)
#include "io/W3dRecursos.h"            // el id de un recurso en el almacen cambia con su entrada
#include "importers/import_w3d.h"      // W3dFormatoMallas (texto/binario) + la malla de trabajo fuera de la escena
#include "io/Librerias.h"              // W3dLibRefDe: una malla vacia porque su libreria no esta
#include "w3dlog.h"
#include <map>
#include <set>
#include <algorithm>

extern bool g_redraw;
extern Object* g_editMesh;

// EL ESTADO DEL EDITOR (lo resetea W3dMallasEditorOlvidar al cerrar el proyecto: son punteros a
// mallas del arbol que se destruye)
static Mesh* gEnEdicion = NULL;     // la malla (de un recurso) que esta en edicion ahora
// LA ACTIVA QUE SE MIRA: la malla activa con su edicion en memoria (sus listas en Properties, el
// editor UV) y la firma de esa edicion al materializarla ("" = no la materializo el tick: al
// soltarla se compara contra el recurso)
static Mesh* gActivaVista = NULL;
static std::string gFirmaVista;

// ============================================================================
//  SERIALIZAR / PUBLICAR
// ============================================================================
bool W3dMallaSerializar(Mesh* m, const std::string& nombre, std::string& out, std::vector<std::string>* avisos) {
    out.clear();
    if (!m) return false;
    // los mismos frenos que el guardado de siempre (EscribirMallaW3dm): nada se hornea a medias
    if (m->w3dmAjenos.noCargo) {
        if (avisos) avisos->push_back("su geometria no se pudo leer al abrir");
        return false;
    }
    const std::string falta = W3dMallaBloqueQueFalta(m);
    if (!falta.empty()) {
        if (avisos) avisos->push_back("su malla necesita el bloque '" + falta + "' de una version mas nueva");
        return false;
    }
    const int nRV = m->vertex ? m->vertexSize : 0;
    if (!W3dMallaBinIndicesAlcanzan(nRV) || (g_w3dIndices16Simulado && nRV > 65535)) {
        if (avisos) avisos->push_back("tiene mas de 65535 vertices y esta plataforma usa indices de 16 bits");
        return false;
    }
    if (!m->edicionPendiente.empty() && !W3dMallaBinMaterializarEdicion(m)) {
        if (avisos) avisos->push_back("no pude leer sus caras");
        return false;
    }
    // el reposo del skinning 2D es dato (ver EscribirMallaW3dm)
    if (m->TieneArm2D()) m->Armature2DRestCapturar();
    std::string b;
    {
        W3dReposoVertexAnim reposo(m);   // la malla EN REPOSO, no en la pose del playhead
        m->ReposarUVAnimTira();
        W3dMallaBinIndices canon;
        W3dMallaIndicesCanonicos(m, canon);
        if (!W3dMallaBinEscribir(m, &canon, b, avisos)) return false;
    }
    if (!W3dMallaBinRenombrar(b, nombre, out)) out.swap(b);
    return true;
}

// el contenido de dos .w3db sin mirar el nombre (INFO): lo que se compara para el dedup
static std::string ClaveContenido(const std::string& b) {
    std::string k;
    if (!W3dMallaBinRenombrar(b, "", k)) return b;
    return k;
}

// LOS BYTES DEL RECURSO TAL COMO ES HOY: los de su entrada (o en memoria) con el nombre de
// material de cada parte (PART) puesto al dia. El material es dato de la malla y cambiarlo en
// la tarjeta no reescribe los bytes (W3dMallaRecursoCambiarMaterial): sin esto, comparar una
// malla serializada contra su recurso daria "distinta" solo por el material.
static bool BytesDelRecurso(const MallaRecurso* r, std::string& out) {
    if (!W3dMallaRecursoLeerBytes(r, out)) return false;
    if (r->materiales.size() != r->partes.size() || r->partes.empty()) return true;
    std::vector<std::string> nombres, enArchivo;
    for (size_t k = 0; k < r->materiales.size(); k++)
        nombres.push_back((r->materiales[k] && !r->materiales[k]->name.empty())
                          ? W3dMallaNombreSano(r->materiales[k]->name, NULL, "de un material") : std::string());
    std::string t;
    if (W3dMallaBinMateriales(out, enArchivo) && enArchivo != nombres && W3dMallaBinCambiarMateriales(out, nombres, t))
        out.swap(t);
    return true;
}

// el material de cada parte de la malla (lo que el recurso guarda por puntero)
static void MaterialesDe(const Mesh* m, std::vector<Material*>& out) {
    out.clear();
    for (size_t k = 0; k < m->materialsGroup.size(); k++) out.push_back(m->materialsGroup[k].material);
}

// la malla tiene su EDICION en memoria (caras, o geometria suelta: aristas / vertices sin caras) y es la del
// recurso con el que se vincula (una copia de la de su original): se conserva al vincular
static bool TieneEdicion(const Mesh* m) {
    return m->edicionPendiente.empty() && (!m->faces3d.empty() || !m->looseEdges.empty() || !m->looseVerts.empty());
}

// la malla esta viva y vinculada? (sin desreferenciar un puntero que pudo morir: se busca en
// las listas de usuarios del registro, que ~Mesh mantiene al dia)
static bool UsuarioVivo(const Mesh* m) {
    if (!m) return false;
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    for (size_t i = 0; i < reg.size(); i++)
        for (size_t k = 0; k < reg[i]->usuarios.size(); k++) if (reg[i]->usuarios[k] == m) return true;
    // (y las mallas de las LIBRERIAS, que no estan en el registro del proyecto. Sin esto el Ctrl+Y de un "New Copy" de
    //  una malla de una libreria -tambien el de desvincularla- no hacia nada. Solo se COMPARAN punteros: 'm' puede ser
    //  uno viejo)
    const std::vector<MallaRecurso*>& lib = W3dMallasLib();
    for (size_t i = 0; i < lib.size(); i++)
        for (size_t k = 0; k < lib[i]->usuarios.size(); k++) if (lib[i]->usuarios[k] == m) return true;
    return false;
}

static bool Esta(const std::vector<Mesh*>& v, const Mesh* m) {
    return std::find(v.begin(), v.end(), m) != v.end();
}

// los usuarios con modificadores geometricos se regeneran (el Core no puede)
static void RegenerarUsuarios(MallaRecurso* r, const std::vector<Mesh*>& exceptos) {
    std::vector<Mesh*> us = r->usuarios;
    for (size_t i = 0; i < us.size(); i++)
        if (us[i] && !Esta(exceptos, us[i]) && !us[i]->modificadores.empty()) us[i]->GenerarMallaModificada();
}

// los usuarios que se quedan con LO SUYO cuando 'r' toma otros datos: el que publica y la malla
// en EDIT MODE (su jaula y sus capas son suyas: re-apuntarla las tiraria en medio de la edicion)
static void ExceptuadosDe(const MallaRecurso* r, Mesh* publica, std::vector<Mesh*>& out) {
    out.clear();
    if (publica) out.push_back(publica);
    if (gEnEdicion && gEnEdicion != publica && UsuarioVivo(gEnEdicion) && gEnEdicion->malla == r)
        out.push_back(gEnEdicion);
}

// LAS VERTEX ANIMS son POR OBJETO e indexan los render-verts de su malla: si lo que se publica
// cambia cuantos hay, re-apuntar a los OTROS usuarios animados a la topologia nueva las dejaria
// muertas para siempre (sus cuadros miden la vieja: EvalVertexAnim no las evalua) y asi se
// guardarian. Esos usuarios se APARTAN a una copia del recurso con el contenido VIEJO (con un
// aviso): siguen animando igual y los demas ven la edicion. Antes de los recursos, editar un
// objeto nunca tocaba la geometria de otro; esto conserva eso para lo que no se puede remapear.
static void ApartarAnimados(MallaRecurso* r, int nRVNuevo, const std::vector<Mesh*>& exceptos) {
    if (!r || !r->Cargada() || nRVNuevo == r->vertexSize) return;
    std::vector<Mesh*> animados;
    for (size_t i = 0; i < r->usuarios.size(); i++) {
        Mesh* u = r->usuarios[i];
        if (u && !Esta(exceptos, u) && !u->animations.empty()) animados.push_back(u);
    }
    if (animados.empty()) return;
    std::string viejos;
    if (!BytesDelRecurso(r, viejos)) {
        w3dLogfW("[mallas] no pude leer '%s' para apartar a sus objetos con vertex anims", r->nombre.c_str());
        return;
    }
    MallaRecurso* copia = W3dMallaRecursoNuevo(r->nombre, "", r->carpeta);
    std::string b2;
    if (W3dMallaBinRenombrar(viejos, copia->nombre, b2)) viejos.swap(b2);
    W3dMallaRecursoPonerBytes(copia, viejos);
    copia->materiales = r->materiales;
    int n = 0;
    for (size_t i = 0; i < animados.size(); i++) {
        Mesh* u = animados[i];
        if (!W3dMallaVincular(u, copia, false)) continue;   // RePosar le devuelve su cuadro
        if (!u->modificadores.empty()) u->GenerarMallaModificada();
        n++;
    }
    w3dLogfW("[mallas] '%s' cambio de topologia: %d objeto(s) con vertex anims quedan con la malla anterior ('%s')",
             r->nombre.c_str(), n, copia->nombre.c_str());
    if (n > 0)
        W3dAvisof(false, "%d objeto(s) con vertex anims siguen con la malla anterior: '%s'",
                  n, W3dNombreCorto(copia->nombre).c_str());
}

// el recurso toma los datos 'nuevos' que publica 'm' (ya serializados y distintos): los animados
// que no calzan se apartan, todos los demas usuarios se re-apuntan y los que tienen modificadores
// se regeneran
static bool PublicarContenido(MallaRecurso* r, Mesh* m, const std::string& nuevos) {
    std::vector<Mesh*> ex;
    ExceptuadosDe(r, m, ex);
    ApartarAnimados(r, m->vertex ? m->vertexSize : 0, ex);
    std::vector<Material*> mats;
    MaterialesDe(m, mats);
    if (!W3dMallaRecursoAplicarBytes(r, nuevos, ex, &mats)) return false;
    RegenerarUsuarios(r, ex);
    return true;
}

// se puede re-apuntar la malla a los arrays del recurso SIN cambiar lo que se ve? No si la
// sesion la tiene POSADA: una vertex anim (vertex[] con la pose del playhead), un flipbook (uv[]
// corrido al cuadro actual) o un rig 2D (uv[] deformado). Esas se quedan con su copia (siguen
// siendo usuarias del recurso); guardar o publicar no puede mover lo que muestra el viewport.
static bool SePuedeRevincular(const Mesh* m) {
    return m && m->animations.empty() && !m->flipbook && !m->TieneArm2D();
}

// la malla que publica, FUERA de edicion, vuelve a compartir los arrays del recurso (conserva
// su edicion: acaba de salir de ella y es la misma malla)
static void VolverACompartir(Mesh* m) {
    if (!m || !m->malla || m == (Mesh*)g_editMesh) return;
    if (!SePuedeRevincular(m)) return;
    if (m->datosComp) return;   // ya comparte
    const bool conserva = m->edicionPendiente.empty();
    W3dMallaVincular(m, m->malla, conserva);
    if (!m->modificadores.empty()) m->GenerarMallaModificada();
}

bool W3dMallaPublicar(Mesh* m) {
    if (!m || !m->malla) return false;
    MallaRecurso* r = m->malla;
    if (W3dMallaLimpia(m)) return false;   // nada propio que pueda haber cambiado
    std::string nuevos;
    std::vector<std::string> avisos;
    if (!W3dMallaSerializar(m, r->nombre, nuevos, &avisos)) {
        w3dLogfW("[mallas] no pude publicar '%s' en la malla '%s': %s", m->name.c_str(), r->nombre.c_str(),
                 avisos.empty() ? "?" : avisos[0].c_str());
        return false;
    }
    std::string viejos;
    if (BytesDelRecurso(r, viejos) && ClaveContenido(viejos) == ClaveContenido(nuevos)) {
        VolverACompartir(m);   // no cambio nada: vuelve a compartir (memoria)
        return false;
    }
    if (!PublicarContenido(r, m, nuevos)) {
        w3dLogfE("[mallas] la malla '%s' no acepto la edicion de '%s'", r->nombre.c_str(), m->name.c_str());
        return false;
    }
    VolverACompartir(m);
    w3dLogf("[mallas] '%s' publicada desde '%s' (%d usuario(s))", r->nombre.c_str(), m->name.c_str(),
            (int)r->usuarios.size());
    g_redraw = true;
    return true;
}

MallaRecurso* W3dMallaCrearRecurso(Mesh* m, const std::string& nombre) {
    if (!m) return NULL;
    std::string b;
    if (!W3dMallaSerializar(m, nombre, b, NULL)) return NULL;
    MallaRecurso* r = W3dMallaRecursoNuevo(nombre, "", m->malla ? m->malla->carpeta : std::string());
    std::string b2;
    if (W3dMallaBinRenombrar(b, r->nombre, b2)) b.swap(b2);
    W3dMallaRecursoPonerBytes(r, b);
    MaterialesDe(m, r->materiales);
    // la malla SERIALIZADA es esta: su edicion coincide con el recurso
    if (!W3dMallaVincular(m, r, m->edicionPendiente.empty())) return NULL;
    return r;
}

// ============================================================================
//  UNDO DE LOS VINCULOS (Hacer unica / elegir otra malla / una malla SUELTA que pasa a tener
//  su recurso): el paso guarda la malla y el OTRO estado (un recurso, o NULL = suelta);
//  aplicar = intercambiar. El paso RETIENE al recurso que guarda (refsUndo): la purga de los
//  recursos de la sesion no lo borra mientras el paso exista, asi que su puntero es estable;
//  la malla se suelta si se libera (el gancho de ~Object la pone en NULL).
// ============================================================================
struct VinculoUndoDato { Mesh* m; MallaRecurso* otro; };

// la malla deja su recurso y queda SUELTA con su copia de todo (lo que ve hoy: su edicion y sus
// arrays). El Ctrl+Z de "una malla suelta paso a tener su recurso" (New Copy / Rename / Folder /
// Alt+D / el selector sobre una suelta).
static bool VolverSuelta(Mesh* m) {
    if (!m || !m->malla) return true;
    if (!m->edicionPendiente.empty() && !W3dMallaBinMaterializarEdicion(m)) return false;
    m->DesinstanciarDatos(W3DMD_TODO, true);
    W3dMallaDesvincular(m);
    return true;
}

static void VinculoUndoAplicar(void* d) {
    VinculoUndoDato* v = (VinculoUndoDato*)d;
    Mesh* m = v->m;
    if (!m) return;
    MallaRecurso* cur = m->malla;   // NULL = la malla esta suelta
    if (cur && !UsuarioVivo(m)) return;
    if (v->otro) {
        if (!W3dMallaVincular(m, v->otro, false)) return;
    } else if (!cur || !VolverSuelta(m)) return;
    if (!m->modificadores.empty()) m->GenerarMallaModificada();
    // el paso ahora guarda el OTRO: retiene al que deja la malla y suelta al que ella tomo
    W3dMallaRecursoUndoRetener(cur);
    W3dMallaRecursoUndoSoltar(v->otro);
    v->otro = cur;
    g_redraw = true;
}
static void VinculoUndoDesvincular(void* d, Object* borrado) {
    VinculoUndoDato* v = (VinculoUndoDato*)d;
    if ((Object*)v->m == borrado) v->m = NULL;
}
static void VinculoUndoLiberar(void* d) {
    VinculoUndoDato* v = (VinculoUndoDato*)d;
    W3dMallaRecursoUndoSoltar(v->otro);   // si quedo sin usuarios, lo purga el proximo tick
    delete v;
}
// 'anterior' = el recurso que la malla tenia (NULL = estaba SUELTA: el Ctrl+Z la vuelve suelta)
static void PushVinculoUndo(Mesh* m, MallaRecurso* anterior) {
    if (!m) return;
    VinculoUndoDato* v = new VinculoUndoDato();
    v->m = m; v->otro = anterior;
    W3dMallaRecursoUndoRetener(anterior);
    UndoExterno f; f.aplicar = VinculoUndoAplicar; f.desvincular = VinculoUndoDesvincular; f.liberar = VinculoUndoLiberar;
    UndoPushExterno(f, v);
}

MallaRecurso* W3dMallaCrearRecursoConUndo(Mesh* m, const std::string& nombre, const std::string& carpeta) {
    if (!m) return NULL;
    if (m->malla) return m->malla;
    MallaRecurso* r = W3dMallaCrearRecurso(m, nombre);
    if (!r) return NULL;
    r->carpeta = W3dMallaCarpetaNormalizar(carpeta);
    PushVinculoUndo(m, NULL);   // Ctrl+Z: la malla vuelve a estar suelta
    g_redraw = true;
    return r;
}

bool W3dMallaCompartida(const Mesh* m) {
    return m && m->malla && W3dMallaRecursoUsuariosEnEscena(m->malla) > 1;
}

// 'm' pasa a una COPIA de su recurso (nombre libre), con undo
static MallaRecurso* CopiarRecursoPara(Mesh* m) {
    MallaRecurso* viejo = m->malla;
    // los datos: los del recurso si la malla no se toco; si no, la malla como esta
    std::string b;
    bool ok = W3dMallaLimpia(m) && BytesDelRecurso(viejo, b);
    if (!ok) ok = W3dMallaSerializar(m, viejo->nombre, b, NULL);
    if (!ok) return NULL;
    // (la copia de una malla de una LIBRERIA externa es del PROYECTO: sin el prefijo de la libreria ni su carpeta, que
    //  son de alla; asi se edita y se guarda como cualquier otra)
    std::string base = viejo->nombre, carpeta = viejo->carpeta;
    if (!viejo->libreria.empty()) {
        const size_t barra = base.find('/');
        if (barra != std::string::npos && barra + 1 < base.size()) base = base.substr(barra + 1);
        carpeta.clear();
    }
    MallaRecurso* nuevo = W3dMallaRecursoNuevo(base, "", carpeta);
    std::string b2;
    if (W3dMallaBinRenombrar(b, nuevo->nombre, b2)) b.swap(b2);
    W3dMallaRecursoPonerBytes(nuevo, b);
    MaterialesDe(m, nuevo->materiales);
    if (!W3dMallaVincular(m, nuevo, m->edicionPendiente.empty())) return NULL;
    if (!m->modificadores.empty()) m->GenerarMallaModificada();
    PushVinculoUndo(m, viejo);
    g_redraw = true;
    return nuevo;
}

MallaRecurso* W3dMallaHacerUnica(Mesh* m) {
    if (!m || !m->malla) return NULL;          // una malla suelta ya es unica
    if (W3dMallaRecursoUsuariosEnEscena(m->malla) <= 1) return m->malla;
    return CopiarRecursoPara(m);
}

MallaRecurso* W3dMallaNuevaCopia(Mesh* m) {
    if (!m) return NULL;
    // una malla SUELTA pasa a tener su recurso (con su paso de undo: Ctrl+Z la vuelve suelta)
    if (!m->malla) return W3dMallaCrearRecursoConUndo(m, m->name, std::string());
    return CopiarRecursoPara(m);
}

bool W3dMallaAsignar(Mesh* m, MallaRecurso* r) {
    if (!m || !r) return false;
    if (m->malla == r) return true;
    // lo que la malla tenga SIN PUBLICAR (un grupo nuevo, una capa) es de SU recurso: se publica
    // antes de irse (editar la malla de un objeto edita el recurso). Sin esto el cambio se perdia
    // y el Ctrl+Z la devolvia sin el.
    if (m->malla && m != gEnEdicion && !W3dMallaLimpia(m)) W3dMallaPublicar(m);
    // UN Ctrl+Z: una malla suelta primero pasa a tener SU recurso (su propio paso: asi el Ctrl+Z
    // la vuelve suelta, con lo que tenia) y despues elige el otro
    UndoGrupoIniciar();
    if (!m->malla && !W3dMallaCrearRecursoConUndo(m, m->name, std::string())) { UndoGrupoFinSecuencial(); return false; }
    MallaRecurso* anterior = m->malla;
    if (!W3dMallaVincular(m, r, false)) {
        UndoGrupoFinSecuencial();
        W3dAvisof(true, "No pude cargar la malla '%s'", W3dNombreCorto(r->nombre).c_str());
        return false;
    }
    if (!m->modificadores.empty()) m->GenerarMallaModificada();
    PushVinculoUndo(m, anterior);
    UndoGrupoFinSecuencial();
    g_redraw = true;
    return true;
}

// ============================================================================
//  TODA MALLA ES UN RECURSO (la biblioteca: nunca hay mallas "sin recurso")
// ============================================================================
static unsigned gAsegNacidos = 0, gAsegDesv = 0;
static int gAsegRaices = -1;
static std::set<unsigned int> gAsegFallaron;   // seriales que no se pudieron serializar (no se reintenta)
static void JuntarSueltas(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::proxy) continue;   // (lo que genera un proxy es de su libreria: no del proyecto)
        // (una malla VACIA porque su malla es de una libreria que no esta no es suelta: tiene su referencia, ver
        //  io/Librerias.h; darle un recurso propio vacio la perderia)
        if (h->getType() == ObjectType::mesh && !((Mesh*)h)->malla && !gAsegFallaron.count(h->serial) &&
            !W3dLibRefDe(h, W3D_LIBREF_MALLA)) out.push_back((Mesh*)h);
        JuntarSueltas(h, out);
    }
}
int W3dMallasAsegurarRecursos() {
    // (jugando no: lo que un script cree durante el Play se va con el Stop)
    extern bool SimActiva();
    if (SimActiva()) return 0;
    std::vector<Object*> raices;
    W3dRaicesVivas(raices);
    const unsigned nac = W3dObjetosNacidos(), des = W3dMallasDesvinculadas();
    if (nac == gAsegNacidos && des == gAsegDesv && (int)raices.size() == gAsegRaices) return 0;
    std::vector<Mesh*> sueltas;
    for (size_t i = 0; i < raices.size(); i++) JuntarSueltas(raices[i], sueltas);
    gAsegNacidos = nac; gAsegDesv = des; gAsegRaices = (int)raices.size();
    int n = 0;
    // las SUELTAS DE UN ARCHIVO VIEJO iguales comparten UN recurso (mil arboles -> una malla)
    std::map<std::string, MallaRecurso*> porContenido;
    for (size_t i = 0; i < sueltas.size(); i++) {
        Mesh* m = sueltas[i];
        if (m->malla) continue;
        std::string b;
        std::vector<std::string> avisos;
        if (!W3dMallaSerializar(m, m->name, b, &avisos)) {
            gAsegFallaron.insert(m->serial);
            w3dLogfW("[mallas] '%s' no pudo tener su malla 3D: %s", m->name.c_str(), avisos.empty() ? "?" : avisos[0].c_str());
            continue;
        }
        const std::string clave = m->dedupPorContenido ? ClaveContenido(b) : std::string();
        if (!clave.empty()) {
            std::map<std::string, MallaRecurso*>::iterator it = porContenido.find(clave);
            if (it != porContenido.end() && W3dMallaVincular(m, it->second, TieneEdicion(m))) {
                if (!m->modificadores.empty()) m->GenerarMallaModificada();
                n++;
                continue;
            }
        }
        MallaRecurso* r = W3dMallaRecursoNuevo(m->name, "", std::string());
        std::string b2;
        if (W3dMallaBinRenombrar(b, r->nombre, b2)) b.swap(b2);
        W3dMallaRecursoPonerBytes(r, b);
        MaterialesDe(m, r->materiales);
        if (!W3dMallaVincular(m, r, m->edicionPendiente.empty())) {
            gAsegFallaron.insert(m->serial);
            continue;
        }
        if (!clave.empty()) porContenido[clave] = r;
        n++;
    }
    if (n > 0) { w3dLogf("[mallas] %d malla(s) sin recurso pasaron a tener el suyo", n); g_redraw = true; }
    return n;
}

// el Add de una primitiva (objects/Primitivas.cpp): su malla 3D en el acto, no recien en el proximo cuadro
static void MallaNacio() { W3dMallasAsegurarRecursos(); }
namespace { struct EngancharMallaNacio { EngancharMallaNacio() { W3dMallaNacioHook = MallaNacio; } } gEngancharMallaNacio; }

// "Duplicate" de la biblioteca: una copia del recurso, sin usuarios, que se CONSERVA (un huerfano
// pedido). Con undo: deshacer la saca del registro y rehacer la vuelve a crear con esos bytes.
struct UndoMallaCreada {
    MallaRecurso* r;
    std::string nombre, carpeta, bytes;
    std::vector<Material*> mats;
    bool viva;
};
static void UndoMallaCreadaAplicar(void* d) {
    UndoMallaCreada* u = (UndoMallaCreada*)d;
    if (u->viva) {
        if (!u->r || !W3dMallaRecursoBorrar(u->r)) return;   // (alguien la empezo a usar: se queda)
        u->r = NULL;
        u->viva = false;
    } else {
        MallaRecurso* r = W3dMallaRecursoNuevo(u->nombre, "", u->carpeta);
        W3dMallaRecursoPonerBytes(r, u->bytes);
        r->materiales = u->mats;
        r->conservar = true;
        u->r = r;
        u->viva = true;
    }
    g_redraw = true;
}
static void UndoMallaCreadaLiberar(void* d) { delete (UndoMallaCreada*)d; }
MallaRecurso* W3dMallaDuplicarRecurso(MallaRecurso* r) {
    if (!r) return NULL;
    std::string b;
    // lo que el recurso ES hoy: si un usuario lo esta editando sin publicar, se publica antes
    for (size_t i = 0; i < r->usuarios.size(); i++)
        if (r->usuarios[i] && r->usuarios[i] != gEnEdicion && !W3dMallaLimpia(r->usuarios[i])) { W3dMallaPublicar(r->usuarios[i]); break; }
    if (!BytesDelRecurso(r, b)) return NULL;
    MallaRecurso* n = W3dMallaRecursoNuevo(r->nombre, "", r->carpeta);
    std::string b2;
    if (W3dMallaBinRenombrar(b, n->nombre, b2)) b.swap(b2);
    W3dMallaRecursoPonerBytes(n, b);
    n->materiales = r->materiales;
    n->conservar = true;
    UndoMallaCreada* u = new UndoMallaCreada();
    u->r = n; u->nombre = n->nombre; u->carpeta = n->carpeta; u->bytes = b; u->mats = n->materiales; u->viva = true;
    UndoExterno f; f.aplicar = UndoMallaCreadaAplicar; f.liberar = UndoMallaCreadaLiberar;
    UndoPushExterno(f, u);
    g_redraw = true;
    return n;
}

// ============================================================================
//  DUPLICAR (Alt+D vinculado / Shift+D con recurso propio)
// ============================================================================
Mesh* W3dMallaDuplicarVinculado(Mesh* src) {
    if (!src) return NULL;
    // una malla SUELTA pasa a tener su recurso (con su paso: el que llama lo junta con la
    // creacion de la copia en un Ctrl+Z, que saca la copia y la vuelve suelta)
    if (!src->malla && !W3dMallaCrearRecursoConUndo(src, src->name, std::string())) return NULL;
    // lo que src muestra tiene que ser lo del recurso (una edicion sin publicar se publica)
    if (!W3dMallaLimpia(src)) W3dMallaPublicar(src);
    Mesh* d = (Mesh*)W3dDuplicarUno(src);   // copia todo lo del OBJETO (y la geometria de paso)
    if (!d) return NULL;
    // ...y la geometria pasa a ser la del recurso (comparte arrays y VBO). Su edicion es una
    // copia de la de src, que coincide con el recurso: se conserva si la hay.
    d->noEditable = src->noEditable;
    if (!W3dMallaVincular(d, src->malla, TieneEdicion(d))) {
        W3dAvisof(true, "No pude vincular la copia de '%s' a su malla", W3dNombreCorto(src->name).c_str());
    }
    if (!d->modificadores.empty()) d->GenerarMallaModificada();
    return d;
}

Mesh* W3dMallaDuplicarConRecursoPropio(Mesh* src) {
    if (!src || !src->malla) return NULL;
    MallaRecurso* r = src->malla;
    std::string b;
    bool ok = W3dMallaLimpia(src) && BytesDelRecurso(r, b);
    if (!ok) ok = W3dMallaSerializar(src, r->nombre, b, NULL);
    Mesh* d = (Mesh*)W3dDuplicarUno(src);
    if (!d || !ok) return d;   // sin bytes: la copia queda suelta (el guardado le da recurso)
    MallaRecurso* nuevo = W3dMallaRecursoNuevo(r->nombre, "", r->carpeta);
    std::string b2;
    if (W3dMallaBinRenombrar(b, nuevo->nombre, b2)) b.swap(b2);
    W3dMallaRecursoPonerBytes(nuevo, b);
    MaterialesDe(src, nuevo->materiales);
    d->noEditable = src->noEditable;
    W3dMallaVincular(d, nuevo, TieneEdicion(d));
    if (!d->modificadores.empty()) d->GenerarMallaModificada();
    return d;
}

// ============================================================================
//  EL TICK DEL EDITOR
// ============================================================================
void W3dMallasEditorOlvidar() {
    gEnEdicion = NULL;
    gActivaVista = NULL;
    gFirmaVista.clear();
}

static Mesh* ContextoDeEdicion() {
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return NULL;
    Mesh* a = (Mesh*)ObjActivo;
    if (!a->malla) return NULL;   // una malla suelta no tiene a quien publicar
    if (g_editMesh == ObjActivo) return a;
    if (InteractionMode == WeightPaint || InteractionMode == VertexPaint) return a;
    return NULL;
}

// la malla ACTIVA si es de un recurso (NULL = no hay, o es suelta: la suelta tiene su edicion)
static Mesh* ActivaConRecurso() {
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return NULL;
    Mesh* a = (Mesh*)ObjActivo;
    return a->malla ? a : NULL;
}

// la edicion de la malla esta en memoria DE VERDAD? (materializada, no una que fallo al leerse
// y quedo vacia: esa no se puede publicar ni comparar)
static bool EdicionEnMemoria(const Mesh* m) {
    return m->edicionPendiente.empty() && (!m->faces3d.empty() || m->facesSize == 0);
}

// LA FIRMA DE LA EDICION materializada (caras, capas con su nombre y tamano, marcas) y de lo que
// comparte con el recurso: si al soltarla da lo mismo que al materializarla, nada suyo cambio y
// se suelta sin serializar. Barata (conteos y nombres): corre una vez por cambio de activa.
static void FirmaNum(std::string& s, long v) { char b[24]; snprintf(b, sizeof(b), "%ld;", v); s += b; }
static std::string FirmaEdicion(const Mesh* m) {
    std::string s;
    FirmaNum(s, (long)m->datosComp);
    FirmaNum(s, (long)m->faces3d.size());
    FirmaNum(s, (long)m->looseEdges.size()); FirmaNum(s, (long)m->looseVerts.size());
    FirmaNum(s, (long)m->sharpEdges.size()); FirmaNum(s, (long)m->seamEdges.size());
    FirmaNum(s, (long)m->cornerNormal.size());
    FirmaNum(s, (long)m->uvMapActivo);
    for (size_t i = 0; i < m->uvMaps.size(); i++) {
        if (!m->uvMaps[i]) { s += "-;"; continue; }
        s += m->uvMaps[i]->nombre; s += '\x1f'; FirmaNum(s, (long)m->uvMaps[i]->uv.size());
    }
    FirmaNum(s, (long)m->colorActivo);
    for (size_t i = 0; i < m->colorLayers.size(); i++) {
        if (!m->colorLayers[i]) { s += "-;"; continue; }
        s += m->colorLayers[i]->nombre; s += '\x1f';
        FirmaNum(s, (long)m->colorLayers[i]->color.size()); FirmaNum(s, m->colorLayers[i]->porVertice ? 1L : 0L);
    }
    return s;
}

// LA ACTIVA QUE SE MIRABA dejo de serlo: su edicion vuelve a quedar PENDIENTE en su recurso (la
// memoria que se ahorra con los recursos no se pierde por haber seleccionado). Si algo suyo
// cambio y no se publico (una lista de Properties, una capa, un grupo nuevo), antes se PUBLICA:
// editar la malla de un objeto edita el recurso, y asi ningun otro usuario lo pisa despues.
static void SoltarVista(Mesh* v, const std::string& firma) {
    if (!v || !v->malla || v == gEnEdicion || v == (Mesh*)g_editMesh) return;
    if (!v->edicionPendiente.empty()) return;   // ya estaba pendiente
    const bool igual = !firma.empty() && FirmaEdicion(v) == firma && W3dMallaExtrasComoRecurso(v);
    if (!igual) {
        if (!EdicionEnMemoria(v)) return;        // una edicion que no se pudo leer no se publica
        W3dMallaPublicar(v);
    }
    W3dMallaSoltarEdicion(v);   // (no-op si algo suyo sigue siendo propio: una pose, el flipbook)
}

void W3dMallasTickEditor() {
    // 0) los recursos de la sesion que ya nadie usa ni nombra (ver W3dMallasPurgarSinUso)
    W3dMallasPurgarSinUso();
    // ...y TODA malla con el suyo (la que nacio en este cuadro: un Add, un import, un Separate)
    W3dMallasAsegurarRecursos();
    Mesh* ctx = ContextoDeEdicion();
    Mesh* act = ActivaConRecurso();
    // 1) SALIR DE EDICION: la malla que sale se publica en su recurso (la ven todos)
    if (gEnEdicion != ctx && gEnEdicion) {
        Mesh* sale = gEnEdicion;
        gEnEdicion = NULL;
        if (UsuarioVivo(sale)) W3dMallaPublicar(sale);
    }
    // 2) LA ACTIVA QUE SE MIRABA dejo de serlo: se suelta (publicando antes lo que tenga). ANTES
    //    de entrar a editar otra: la que entra re-apunta a lo publicado y lo ve
    if (gActivaVista && gActivaVista != act) {
        Mesh* v = gActivaVista;
        const std::string firma = gFirmaVista;
        gActivaVista = NULL; gFirmaVista.clear();
        if (UsuarioVivo(v)) SoltarVista(v, firma);
    }
    // 3) ENTRAR A EDICION: la malla que se va a editar tiene su edicion en memoria y SUS arrays
    //    (copia): ninguna operacion de edicion puede escribir la geometria de los demas usuarios
    if (gEnEdicion != ctx && ctx) {
        gEnEdicion = ctx;
        if (!ctx->edicionPendiente.empty()) W3dMallaBinMaterializarEdicion(ctx);
        ctx->DesinstanciarDatos(W3DMD_TODO, true);
    }
    // 4) LAS MALLAS QUE UNA OPERACION EDITO FUERA DE EDICION (Apply Modifier, Shade,
    //    un Ctrl+Z de geometria, una lista de Properties...): se publican ya
    std::vector<Mesh*> ed;
    W3dMallasTomarEditadas(ed);
    for (size_t i = 0; i < ed.size(); i++) {
        Mesh* m = ed[i];
        if (!m || m == gEnEdicion || !m->malla) continue;   // la de edicion se publica al salir
        W3dMallaPublicar(m);
    }
    // 5) LA ACTIVA (y SOLO ella) necesita su edicion: sus listas en Properties (UV maps, capas de
    //    color) y el editor UV. Lo demas seleccionado se MIRA con la topologia compartida del
    //    recurso (contorno, snap, seleccion por caja): materializar a todo lo seleccionado no se
    //    soltaba nunca (Select All sobre mil arboles = +90 MB para siempre).
    if (act && act != gEnEdicion && act != gActivaVista) {
        if (!act->edicionPendiente.empty()) {
            if (W3dMallaBinMaterializarEdicion(act)) { gActivaVista = act; gFirmaVista = FirmaEdicion(act); }
        } else if (EdicionEnMemoria(act)) {
            // ya estaba en memoria (acaba de salir de edicion, un Ctrl+Z): sin firma, soltarla la
            // compara contra el recurso
            gActivaVista = act; gFirmaVista.clear();
        }
    }
}

void W3dMallasSincronizar() {
    if (gEnEdicion && UsuarioVivo(gEnEdicion) && gEnEdicion->malla) {
        // publicar SIN salir de edicion: la malla se queda con lo suyo (su jaula no se toca)
        Mesh* m = gEnEdicion;
        MallaRecurso* r = m->malla;
        if (!W3dMallaLimpia(m)) {
            std::string nuevos, viejos;
            if (W3dMallaSerializar(m, r->nombre, nuevos, NULL) &&
                !(BytesDelRecurso(r, viejos) && ClaveContenido(viejos) == ClaveContenido(nuevos)))
                PublicarContenido(r, m, nuevos);
        }
    }
    std::vector<Mesh*> ed;
    W3dMallasTomarEditadas(ed);
    for (size_t i = 0; i < ed.size(); i++)
        if (ed[i] && ed[i] != gEnEdicion && ed[i]->malla) W3dMallaPublicar(ed[i]);
}

// ============================================================================
//  EL GUARDADO
// ============================================================================
namespace {
struct Grupo {
    std::string nombre;          // el nombre con que se escribe
    std::string carpeta;
    std::string entrada;         // la entrada en el contenedor NUEVO
    MallaRecurso* rec;           // el recurso que conserva su identidad (NULL = uno nuevo)
    bool contenidoDelRecurso;    // los bytes son los de 'rec' (no se tocaron)
    std::string bytes;           // los bytes a escribir ("" = los de 'rec', a leer o copiar)
    std::string clave;           // contenido sin nombre (dedup); "" = todavia no se calculo
    std::vector<Mesh*> mallas;   // los objetos que lo usan (en el orden de la escena)
    std::vector<Material*> mats; // material de cada parte
    int primero;                 // orden de la escena del primer objeto (orden del registro)
    bool huerfano;
    const MallaRecurso* origen;  // un recurso NUEVO que se separo de este (usuario tocado)
    // nacio de una malla SUELTA que vino de un archivo viejo (Mesh::dedupPorContenido): solo con
    // estos grupos se juntan por contenido las demas sueltas de archivo
    bool juntable;
    // (huerfano) los nombres de material que dicen los bytes de su PART
    std::vector<std::string> partNombres;
    // (huerfano) su entrada no se pudo leer (falta en el contenedor o esta rota): no se escribe
    // ni se registra, y el proyecto se guarda igual (con un aviso)
    bool perdido;
    Grupo() : rec(0), contenidoDelRecurso(false), primero(0), huerfano(false), origen(0),
              juntable(false), perdido(false) {}
};
struct PorPrimero {
    const std::vector<Grupo>* gs;
    bool operator()(size_t a, size_t b) const { return (*gs)[a].primero < (*gs)[b].primero; }
};
}

static std::vector<Grupo> gPlan;
static std::vector<size_t> gOrden;                 // gPlan en el orden del registro
static std::map<const Mesh*, size_t> gPlanDe;      // malla -> su grupo
static bool gPlanListo = false;
static bool gPlanTexto = false;                    // el plan escribio .w3dm (formato de texto)
// LOS BORRADOS QUE EL UNDO RETIENE (MallaRecurso::borrado) y todavia nombran su entrada del .w3d que este
// guardado reemplaza: no se escriben, pero un Ctrl+Z despues de guardar les devuelve sus objetos. Sus bytes se
// leen del contenedor VIEJO al preparar (serial del recurso -> su .w3db; vacio = ya los tenia en memoria) y al
// confirmar quedan en memoria sin entrada: sin esto el recurso nombraba una entrada que ya no estaba y ningun
// guardado posterior salia ("no pude leer la malla")
static std::vector<std::pair<int, std::string> > gDesalojados;

void W3dMallasGuardarDescartar() {
    gPlan.clear(); gOrden.clear(); gPlanDe.clear();
    gDesalojados.clear();
    gPlanListo = false;
}

static void JuntarMallas(Object* o, std::vector<Mesh*>& out) {
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        // lo que genera un PROXY es de su libreria (no se guarda); y una malla del proyecto que usa una malla de una
        // LIBRERIA la nombra (GuardarW3D): ninguna de las dos entra al plan
        if (h->getType() == ObjectType::proxy) continue;
        // (ni la que quedo vacia porque su malla es de una libreria que NO ESTA: tambien la nombra)
        if (h->getType() == ObjectType::mesh && !(((Mesh*)h)->malla && !((Mesh*)h)->malla->libreria.empty()) &&
            !(!((Mesh*)h)->malla && W3dLibRefDe(h, W3D_LIBREF_MALLA)))
            out.push_back((Mesh*)h);
        JuntarMallas(h, out);
    }
}

// el nombre de material que el escritor pone en PART para esta parte
static std::string NombreMaterialParte(const Material* mat) {
    if (!mat || mat->name.empty()) return std::string();
    return W3dMallaNombreSano(mat->name, NULL, "de un material");
}

// la clave de contenido de un grupo (se calcula la primera vez que se compara)
static const std::string& ClaveDe(Grupo& g) {
    if (g.clave.empty()) {
        if (!g.bytes.empty()) g.clave = ClaveContenido(g.bytes);
        else if (g.rec) {
            std::string b;
            if (BytesDelRecurso(g.rec, b)) g.clave = ClaveContenido(b);
        }
        if (g.clave.empty()) g.clave = "\x01";   // ilegible: no coincide con nada
    }
    return g.clave;
}

// ---------------------------------------------------------------------------
//  EL FORMATO DE TEXTO ("formatoMallas": "texto"): la misma entrada por malla, en .w3dm
// ---------------------------------------------------------------------------
// ancla de los render-verts (W3dMallaEscribir): si ALGUN objeto de la malla tiene vertex anims,
// el orden de memoria es dato de todos (sus keyframes lo indexan)
static int AnclaDe(const Grupo& g) {
    for (size_t k = 0; k < g.mallas.size(); k++) if (g.mallas[k] && !g.mallas[k]->animations.empty()) return 1;
    return -1;
}
// el .w3dm desde un OBJETO del grupo: la malla tal como se ve (su edicion, en reposo). La linea
// "nombre" del archivo es la de la MALLA (el recurso), no la del objeto.
static bool TextoDeMalla(Mesh* m, const std::string& nombre, int ancla, std::string& txt,
                         std::vector<std::string>* avisos) {
    // una malla de un recurso con la edicion pendiente se materializa para escribirla y despues
    // la suelta (igual que el camino de siempre, EscribirMallaW3dm)
    const bool prestada = m->malla && !m->edicionPendiente.empty();
    if (!m->edicionPendiente.empty() && !W3dMallaBinMaterializarEdicion(m)) {
        if (avisos) avisos->push_back("no pude leer sus caras");
        return false;
    }
    if (m->TieneArm2D()) m->Armature2DRestCapturar();   // el reposo 2D es dato (ver EscribirMallaW3dm)
    const std::string nomObj = m->name;
    m->name = nombre;
    bool ok;
    {
        W3dReposoVertexAnim reposo(m);   // EN REPOSO, no en la pose del playhead
        m->ReposarUVAnimTira();
        ok = W3dMallaEscribir(m, txt, avisos, ancla);
    }
    m->name = nomObj;
    if (prestada) W3dMallaSoltarEdicion(m);
    return ok;
}
// el .w3dm desde los BYTES (una malla huerfana: ningun objeto la usa): una malla de trabajo
// fuera de la escena la lee entera del .w3db y la escribe en texto
static bool TextoDeBytes(const std::string& b, const std::string& nombre, const std::vector<Material*>& mats,
                         std::string& txt, std::vector<std::string>* avisos) {
    Mesh* t = W3dMallaTemporalNueva();
    t->name = nombre;
    W3dMallaInfo info;
    W3dMallaBinOpciones op;   // con la edicion: caras, capas, marcas
    bool ok = !b.empty() && W3dMallaBinLeer((const unsigned char*)b.data(), b.size(), t, &info, op);
    if (ok) {
        for (size_t k = 0; k < t->materialsGroup.size(); k++)
            t->materialsGroup[k].material = (k < mats.size() && mats[k]) ? mats[k]
                                          : (k < info.materiales.size() ? BuscarMaterialPorNombre(info.materiales[k]) : NULL);
        ok = W3dMallaEscribir(t, txt, avisos);
    } else if (avisos) avisos->push_back("no pude leer la malla");
    W3dMallaTemporalBorrar(t);
    return ok;
}

static bool NombreTomado(const std::string& n, const MallaRecurso* excepto) {
    for (size_t i = 0; i < gPlan.size(); i++) if (gPlan[i].nombre == n) return true;
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    for (size_t i = 0; i < reg.size(); i++) if (reg[i] != excepto && reg[i]->nombre == n) return true;
    return false;
}
static bool NombreTomadoFn(const std::string& n, void* ctx) { return NombreTomado(n, (const MallaRecurso*)ctx); }

bool W3dMallasGuardarPreparar(W3dContenedorEscritor* esc) {
    W3dMallasGuardarDescartar();
    if (!esc || !SceneCollection) return false;
    // lo que se ve es lo que se guarda: la malla en edicion y las editadas se publican antes
    W3dMallasSincronizar();
    W3dMallasPurgarSinUso();   // (los de la sesion sin usuarios ni undo no se escriben)

    // las mallas de TODAS las raices del proyecto (la escena activa y las escenas/prefabs que no se
    // estan mirando: el guardado las escribe a todas, W3dRaices.h), en el orden del REGISTRO: el
    // orden del registro "mallas" no puede depender de que escena se estaba mirando al guardar
    std::vector<Mesh*> mallas;
    {
        std::vector<Object*> raices;
        W3dRaicesEnOrden(raices);
        for (size_t i = 0; i < raices.size(); i++) JuntarMallas(raices[i], mallas);
    }
    bool ok = true;

    // PASADA A: los recursos que tienen al menos un usuario SIN TOCAR conservan su identidad
    // y su contenido (el grupo nace de ellos, en el orden del primer usuario)
    std::map<const MallaRecurso*, size_t> grupoDe;
    std::map<const MallaRecurso*, std::string> claveRec;   // contenido de cada recurso (se lee una vez)
    std::set<const MallaRecurso*> enTexto;   // recursos de mallas que van por el camino de texto (16 bits)
    std::vector<char> resuelta(mallas.size(), 0);
    for (size_t i = 0; i < mallas.size(); i++) {
        Mesh* m = mallas[i];
        if (!m->malla || !W3dMallaLimpia(m)) continue;
        // una malla que no entra en el .w3db de ESTA plataforma (indices de 16 bits) va por el
        // camino de texto aunque su recurso este sin tocar (ver EscribirMallaW3dm)
        if (!W3dMallaBinIndicesAlcanzan(m->vertexSize) || (g_w3dIndices16Simulado && m->vertexSize > 65535)) {
            enTexto.insert(m->malla);
            continue;
        }
        std::map<const MallaRecurso*, size_t>::iterator it = grupoDe.find(m->malla);
        size_t gi;
        if (it == grupoDe.end()) {
            Grupo g;
            g.nombre = m->malla->nombre; g.carpeta = m->malla->carpeta;
            g.rec = m->malla; g.contenidoDelRecurso = true;
            g.mats = m->malla->materiales;
            g.primero = (int)i;
            gPlan.push_back(g);
            gi = gPlan.size() - 1;
            grupoDe[m->malla] = gi;
        } else gi = it->second;
        gPlan[gi].mallas.push_back(m);
        gPlanDe[m] = gi;
        resuelta[i] = 1;
    }
    // PASADA B: las tocadas y las sueltas se serializan y se agrupan POR CONTENIDO (dedup: mil
    // arboles iguales de un archivo viejo -> una sola malla)
    for (size_t i = 0; i < mallas.size(); i++) {
        if (resuelta[i]) continue;
        Mesh* m = mallas[i];
        std::string b;
        std::vector<std::string> avisos;
        const int nRV = m->vertex ? m->vertexSize : 0;
        // una malla que no entra en el .w3db de esta plataforma va por el camino de TEXTO
        // (EscribirMallaW3dm, "geometria" propia): no entra al plan
        if (!m->w3dmAjenos.noCargo && W3dMallaBloqueQueFalta(m).empty() &&
            (!W3dMallaBinIndicesAlcanzan(nRV) || (g_w3dIndices16Simulado && nRV > 65535))) {
            if (m->malla) enTexto.insert(m->malla);
            continue;
        }
        if (!W3dMallaSerializar(m, m->malla ? m->malla->nombre : m->name, b, &avisos)) {
            // los frenos de siempre: el proyecto no se guarda y se dice por que
            w3dLogfE("[W3D] '%s': %s: NO guardo encima", m->name.c_str(), avisos.empty() ? "?" : avisos[0].c_str());
            W3dAvisof(true, "No guardo '%s': %s", W3dNombreCorto(m->name).c_str(),
                      avisos.empty() ? "su malla no se puede escribir" : avisos[0].c_str());
            ok = false;
            continue;
        }
        for (size_t k = 0; k < avisos.size(); k++) w3dLogfW("[W3D] '%s': %s", m->name.c_str(), avisos[k].c_str());
        if (!avisos.empty())
            W3dAvisof(true, "'%s': %s%s", W3dNombreCorto(m->name).c_str(), avisos[0].c_str(),
                      avisos.size() > 1 ? " (y mas: ver whisk3d.log)" : "");
        const std::string clave = ClaveContenido(b);
        size_t gi = (size_t)-1;
        MallaRecurso* r = m->malla;
        if (r) {
            // UNA MALLA CON RECURSO se queda con SU recurso (no se mezcla con otro igual: un
            // Shift+D o un "Hacer unica" son a proposito). Igual a su recurso -> su grupo;
            // distinta (se toco sin publicarse) -> el recurso la adopta si nadie mas lo usa sin
            // tocar, o se separa en un recurso nuevo con el nombre siguiente (Arbol.001).
            std::map<const MallaRecurso*, std::string>::iterator ic = claveRec.find(r);
            if (ic == claveRec.end()) {
                std::string rb;
                ic = claveRec.insert(std::make_pair((const MallaRecurso*)r,
                                                   BytesDelRecurso(r, rb) ? ClaveContenido(rb) : std::string("\x01"))).first;
            }
            const bool igual = (ic->second == clave);
            std::map<const MallaRecurso*, size_t>::iterator ig = grupoDe.find(r);
            if (igual && ig != grupoDe.end() && gPlan[ig->second].contenidoDelRecurso) gi = ig->second;
            else if (ig == grupoDe.end()) {
                Grupo g;
                g.primero = (int)i;
                g.bytes.swap(b);
                g.clave = clave;
                MaterialesDe(m, g.mats);
                g.rec = r; g.nombre = r->nombre; g.carpeta = r->carpeta;
                g.contenidoDelRecurso = igual;
                grupoDe[r] = gPlan.size();
                gPlan.push_back(g);
                gi = gPlan.size() - 1;
            } else {
                // otro usuario tocado del MISMO recurso con el mismo contenido: juntos, sea con
                // el que ADOPTO el recurso (su grupo es el de 'r', con el contenido nuevo) o con
                // uno que se separo antes. Sin mirar el que adopto, dos usuarios con el mismo
                // cambio terminaban en dos recursos identicos (Arbol y Arbol.001).
                for (size_t k = 0; k < gPlan.size() && gi == (size_t)-1; k++)
                    if ((gPlan[k].origen == r || (gPlan[k].rec == r && !gPlan[k].contenidoDelRecurso)) &&
                        ClaveDe(gPlan[k]) == clave) gi = k;
                if (gi == (size_t)-1) {
                    Grupo g;
                    g.primero = (int)i;
                    g.bytes.swap(b);
                    g.clave = clave;
                    MaterialesDe(m, g.mats);
                    g.nombre = W3dNombreUnico(r->nombre, "Malla", NombreTomadoFn, (void*)0);
                    g.carpeta = r->carpeta;
                    g.origen = r;
                    gPlan.push_back(g);
                    gi = gPlan.size() - 1;
                }
            }
        } else {
            // UNA MALLA SUELTA. Si vino asi de un ARCHIVO VIEJO (una geometria por objeto,
            // Mesh::dedupPorContenido) se agrupa POR CONTENIDO con las otras sueltas de archivo
            // iguales: mil arboles iguales -> una sola malla. Una malla NUEVA de la sesion (una
            // primitiva, un Shift+D de una suelta, un import) tiene SU recurso: dos cubos iguales
            // no quedan vinculados porque si al guardar (editar uno no puede mover al otro).
            if (m->dedupPorContenido)
                for (size_t k = 0; k < gPlan.size() && gi == (size_t)-1; k++)
                    if (gPlan[k].juntable && ClaveDe(gPlan[k]) == clave) gi = k;
            if (gi == (size_t)-1) {
                Grupo g;
                g.primero = (int)i;
                g.bytes.swap(b);
                g.clave = clave;
                MaterialesDe(m, g.mats);
                g.nombre = W3dNombreUnico(m->name, "Malla", NombreTomadoFn, (void*)0);
                g.juntable = m->dedupPorContenido;
                gPlan.push_back(g);
                gi = gPlan.size() - 1;
            }
        }
        gPlan[gi].mallas.push_back(m);
        gPlanDe[m] = gi;
        // el ORDEN del registro es el del primer usuario en la escena: una tocada (la activa, con
        // su edicion materializada) que se suma al grupo que armo en la PASADA A un usuario
        // POSTERIOR adelanta el grupo. Sin esto el orden dependia de que objeto estaba activo y
        // re-guardar sin cambios podia dar otro proyecto.json
        if ((int)i < gPlan[gi].primero) gPlan[gi].primero = (int)i;
    }
    if (!ok) { W3dMallasGuardarDescartar(); return false; }

    // LOS HUERFANOS: los del ARCHIVO (tienen entrada) se conservan aunque nadie los use; los
    // creados en esta sesion que se quedaron sin usuarios (un Shift+D deshecho, un "Hacer
    // unica" que se volvio a asignar) no se escriben
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    const int base = (int)mallas.size();
    for (size_t i = 0; i < reg.size(); i++) {
        MallaRecurso* r = reg[i];
        if (grupoDe.find(r) != grupoDe.end()) continue;
        if (r->borrado) {
            // (el usuario lo borro: vive solo para el undo y no se escribe). Si nombra una entrada de este
            // .w3d, sus bytes se leen AHORA: el contenedor nuevo ya no la va a tener (ver gDesalojados)
            if (!r->entrada.empty()) {
                std::string b;
                if (r->bytes.empty() && !W3dMallaRecursoLeerBytes(r, b))
                    w3dLogfW("[W3D] la malla borrada '%s' (%s) no se pudo leer: un Ctrl+Z ya no la puede devolver",
                             r->nombre.c_str(), r->entrada.c_str());
                else gDesalojados.push_back(std::make_pair(r->serial, b));
            }
            continue;
        }
        // un huerfano PEDIDO (el "Duplicate" de la biblioteca) todavia sin entrada: se escribe de sus bytes
        if (r->entrada.empty() && !r->conservar) continue;
        if (enTexto.count(r)) continue;   // su objeto se guarda en texto: no es un huerfano
        Grupo g;
        g.nombre = r->nombre; g.carpeta = r->carpeta; g.rec = r;
        g.contenidoDelRecurso = true; g.huerfano = true;
        g.primero = base + (int)i;
        // EL MATERIAL DE CADA PARTE aunque nunca se haya cargado en la sesion (sus PART lo
        // nombran): asi va al bloque "materiales" (sin esto, un material que solo usaba el
        // huerfano se perdia del proyecto) y un rename de la sesion llega a su PART
        if (W3dMallaRecursoMateriales(r, &g.partNombres)) g.mats = r->materiales;
        else {
            // SU ENTRADA NO SE PUEDE LEER (falta en el contenedor o esta rota): no hay nada que
            // conservar. No frena el guardado (no habria forma de salir: el huerfano no se puede
            // quitar); se avisa y el proyecto se guarda sin esa malla.
            g.perdido = true;
            w3dLogfW("[W3D] la malla huerfana '%s' (%s) no se pudo leer: no se guarda", r->nombre.c_str(), r->entrada.c_str());
            W3dAvisof(true, "La malla '%s' (sin objetos) no se pudo leer: se guarda sin ella",
                      W3dNombreCorto(r->nombre).c_str());
        }
        gPlan.push_back(g);
    }

    // ORDEN DEL REGISTRO: el de la escena (primer usuario), despues los huerfanos. Determinista.
    for (size_t i = 0; i < gPlan.size(); i++) gOrden.push_back(i);
    PorPrimero cmp; cmp.gs = &gPlan;
    std::stable_sort(gOrden.begin(), gOrden.end(), cmp);

    // LAS ENTRADAS: una por grupo, "mallas/<slug(nombre)>.w3db" (-2, -3 si el slug choca); en el
    // formato de TEXTO la misma entrada en .w3dm (el registro y la dedup son los mismos)
    gPlanTexto = (W3dFormatoMallas() == W3D_MALLAS_TEXTO);
    const char* ext = gPlanTexto ? ".w3dm" : ".w3db";
    for (size_t o = 0; o < gOrden.size(); o++) {
        Grupo& g = gPlan[gOrden[o]];
        if (g.perdido) continue;
        const std::string base = W3dSlugEntrada(g.nombre.empty() ? std::string("malla") : g.nombre);
        std::string nom = "mallas/" + base + ext;
        for (int k = 2; esc->Tiene(nom); k++) {
            char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
            nom = "mallas/" + base + sf + ext;
        }
        g.entrada = nom;
        // los NOMBRES que el archivo tiene que llevar adentro: el del recurso (INFO) y el de
        // cada material (PART, por nombre: un material renombrado en la sesion)
        std::vector<std::string> matsNom;
        for (size_t k = 0; k < g.mats.size(); k++) {
            // (un huerfano cuyo material no existe en esta escena conserva el nombre de su PART)
            if (!g.mats[k] && g.huerfano && k < g.partNombres.size()) matsNom.push_back(g.partNombres[k]);
            else matsNom.push_back(NombreMaterialParte(g.mats[k]));
        }
        // COPIA TAL CUAL: un recurso sin tocar, sin cambios de nombre, que ya estaba en el
        // contenedor con la misma entrada (no se lee ni se reescribe nada)
        if (g.contenidoDelRecurso && g.rec && !g.rec->modificado && g.rec->entrada == nom && g.bytes.empty()) {
            // un HUERFANO: lo que dicen sus bytes contra el nombre vivo de cada material (un
            // material renombrado en la sesion reescribe su PART); sin cargar y con usuarios no
            // puede pasar (un usuario lo carga)
            bool mismos = g.huerfano ? (g.partNombres == matsNom) : !g.rec->Cargada();
            if (!mismos && !g.huerfano) {
                mismos = (g.rec->partes.size() == matsNom.size());
                for (size_t k = 0; k < matsNom.size() && mismos; k++)
                    if (g.rec->partes[k].material != matsNom[k]) mismos = false;
            }
            if (mismos && esc->CopiarDelViejo(nom)) {
                // el nombre de INFO lo dice el registro; una copia sin tocar lo conserva
                continue;
            }
        }
        if (gPlanTexto) {
            // TEXTO: desde un objeto del grupo (la malla tal como se ve) o, si es huerfana, desde
            // sus bytes. La copia BINARIA del grupo (si la hay) queda para la memoria: al confirmar
            // es el cache de la conversion del .w3dm recien escrito (no se vuelve a convertir).
            std::string txt, b;
            std::vector<std::string> avisos;
            bool okT;
            if (!g.mallas.empty()) okT = TextoDeMalla(g.mallas[0], g.nombre, AnclaDe(g), txt, &avisos);
            else okT = (!g.bytes.empty() ? (b = g.bytes, true) : (g.rec && BytesDelRecurso(g.rec, b))) &&
                       TextoDeBytes(b, g.nombre, g.mats, txt, &avisos);
            for (size_t k = 0; k < avisos.size(); k++) w3dLogfW("[W3D] malla '%s': %s", g.nombre.c_str(), avisos[k].c_str());
            if (!okT && g.huerfano) {
                // (un huerfano ilegible: igual que en binario, se avisa y se guarda sin el)
                g.perdido = true;
                w3dLogfW("[W3D] la malla huerfana '%s' no se pudo escribir en texto: no se guarda", g.nombre.c_str());
                W3dAvisof(true, "La malla '%s' (sin objetos) no se pudo leer: se guarda sin ella",
                          W3dNombreCorto(g.nombre).c_str());
                continue;
            }
            if (!okT) {
                w3dLogfE("[W3D] la malla '%s' no se pudo escribir en texto", g.nombre.c_str());
                W3dAvisof(true, "No guardo: la malla '%s' no se pudo escribir (%s)", W3dNombreCorto(g.nombre).c_str(),
                          avisos.empty() ? "ver whisk3d.log" : avisos[0].c_str());
                ok = false;
                break;
            }
            if (!avisos.empty())
                W3dAvisof(true, "'%s': %s%s", W3dNombreCorto(g.nombre).c_str(), avisos[0].c_str(),
                          avisos.size() > 1 ? " (y mas: ver whisk3d.log)" : "");
            if (!g.bytes.empty()) {
                std::string t;
                if (W3dMallaBinRenombrar(g.bytes, g.nombre, t)) g.bytes.swap(t);
                std::vector<std::string> enArchivo;
                if (W3dMallaBinMateriales(g.bytes, enArchivo) && enArchivo.size() == matsNom.size() && enArchivo != matsNom &&
                    W3dMallaBinCambiarMateriales(g.bytes, matsNom, t)) g.bytes.swap(t);
            }
            if (!esc->AgregarBytes(nom, txt, false)) {
                w3dLogfE("GuardarW3D: no pude meter %s adentro del .w3d", nom.c_str());
                ok = false;
                break;
            }
            continue;
        }
        // si no, los bytes (del recurso o serializados), con los nombres al dia
        std::string b;
        if (!g.bytes.empty()) b = g.bytes;
        else if (g.huerfano && !BytesDelRecurso(g.rec, b)) {
            // (un huerfano ilegible: se avisa y se guarda sin el, ver arriba)
            g.perdido = true;
            w3dLogfW("[W3D] la malla huerfana '%s' no se pudo leer: no se guarda", g.nombre.c_str());
            W3dAvisof(true, "La malla '%s' (sin objetos) no se pudo leer: se guarda sin ella",
                      W3dNombreCorto(g.nombre).c_str());
            continue;
        }
        else if (!g.rec || !BytesDelRecurso(g.rec, b)) {
            w3dLogfE("[W3D] la malla '%s' no se pudo leer para guardarla", g.nombre.c_str());
            W3dAvisof(true, "No guardo: no pude leer la malla '%s'", W3dNombreCorto(g.nombre).c_str());
            ok = false;
            break;
        }
        std::string t;
        if (W3dMallaBinRenombrar(b, g.nombre, t)) b.swap(t);
        std::vector<std::string> enArchivo;
        if (W3dMallaBinMateriales(b, enArchivo) && enArchivo.size() == matsNom.size() && enArchivo != matsNom &&
            W3dMallaBinCambiarMateriales(b, matsNom, t)) b.swap(t);
        g.bytes.swap(b);
        if (!esc->AgregarBytes(nom, g.bytes, false)) {
            w3dLogfE("GuardarW3D: no pude meter %s adentro del .w3d", nom.c_str());
            ok = false;
            break;
        }
    }
    if (!ok) { W3dMallasGuardarDescartar(); return false; }
    gPlanListo = true;
    return true;
}

std::string W3dMallasGuardarNombreDe(const Mesh* m) {
    std::map<const Mesh*, size_t>::const_iterator it = gPlanDe.find(m);
    if (!gPlanListo || it == gPlanDe.end()) return std::string();
    return gPlan[it->second].nombre;
}

// un string JSON (mismo escape que el resto del guardado: comillas, barra y control)
static void JsonTexto(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const unsigned char c = (unsigned char)v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += (char)c; }
        else if (c == '\n') s += "\\n";
        else if (c == '\r') s += "\\r";
        else if (c == '\t') s += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", (unsigned)c); s += b; }
        else s += (char)c;
    }
    s += '"';
}

void W3dMallasGuardarRegistro(std::string& s) {
    if (!gPlanListo) return;
    std::vector<const Grupo*> gs;   // (sin los huerfanos que no se pudieron leer)
    for (size_t o = 0; o < gOrden.size(); o++) if (!gPlan[gOrden[o]].perdido) gs.push_back(&gPlan[gOrden[o]]);
    if (gs.empty()) return;
    s += "  \"mallas\": [\n";
    for (size_t o = 0; o < gs.size(); o++) {
        const Grupo& g = *gs[o];
        s += "    {\"nombre\": "; JsonTexto(s, g.nombre);
        s += ", \"entrada\": "; JsonTexto(s, g.entrada);
        if (!g.carpeta.empty()) { s += ", \"carpeta\": "; JsonTexto(s, W3dRutaCosmeticaJson(g.carpeta)); }
        s += "}";
        s += (o + 1 < gs.size()) ? ",\n" : "\n";
    }
    s += "  ],\n";
}

void W3dMallasGuardarMateriales(std::vector<Material*>& out) {
    out.clear();
    if (!gPlanListo) return;
    for (size_t o = 0; o < gOrden.size(); o++) {
        const Grupo& g = gPlan[gOrden[o]];
        if (!g.huerfano || g.perdido) continue;
        for (size_t k = 0; k < g.mats.size(); k++) if (g.mats[k]) out.push_back(g.mats[k]);
    }
}

// LA CLAVE TEMPORAL de un recurso cargado mientras las entradas se mudan (no choca con ninguna
// entrada del contenedor ni con las "*<serial>" de los recursos sin entrada)
static std::string ClaveMudando(const MallaRecurso* r) {
    char b[32]; snprintf(b, sizeof(b), "#mudando:%d", r->serial);
    return b;
}

// LAS ENTRADAS NUEVAS de los recursos que siguen ("" para los huerfanos que no se pudieron leer:
// ya no estan en el archivo), con su clave en el almacen. EN DOS PASOS: primero todos los
// cargados a una clave temporal y recien despues cada uno a la suya. De a uno, un recurso chocaba
// con la entrada VIEJA de otro que todavia no se habia mudado (dos mallas que intercambian
// nombres, un slug que cambia el orden): el cambio fallaba en silencio, el recurso quedaba con un
// descriptor ajeno y el proximo vinculo lo volvia a cargar ENCIMA de sus usuarios.
static void MudarEntradas() {
    std::vector<MallaRecurso*> mudan;
    std::vector<std::string> nuevas;
    for (size_t o = 0; o < gOrden.size(); o++) {
        const Grupo& g = gPlan[gOrden[o]];
        if (!g.rec) continue;
        const std::string e = g.perdido ? std::string() : g.entrada;
        if (g.rec->entrada == e) continue;
        mudan.push_back(g.rec);
        nuevas.push_back(e);
    }
    // (los BORRADOS que retiene el undo se quedan sin entrada: el archivo nuevo no la tiene)
    for (size_t i = 0; i < gDesalojados.size(); i++) {
        MallaRecurso* r = W3dMallaRecursoPorSerial(gDesalojados[i].first);
        if (!r || r->entrada.empty()) continue;
        mudan.push_back(r);
        nuevas.push_back(std::string());
    }
    for (size_t i = 0; i < mudan.size(); i++)
        if (mudan[i]->rec) W3dRecursoMover(W3DREC_MALLA, mudan[i]->rec->id, ClaveMudando(mudan[i]));
    for (size_t i = 0; i < mudan.size(); i++) mudan[i]->entrada = nuevas[i];
    for (size_t i = 0; i < mudan.size(); i++) {
        MallaRecurso* r = mudan[i];
        // (la nueva solo puede estar ocupada por un descriptor SIN dato -una ListaCarga que pedia
        // esa entrada-, que se absorbe; si la ocupa un dato vivo el recurso sigue andando por su
        // descriptor, que el vinculo usa por puntero)
        if (r->rec && !W3dRecursoMover(W3DREC_MALLA, r->rec->id, r->IdAlmacen()))
            w3dLogfE("[mallas] la clave '%s' del almacen esta ocupada: la malla '%s' sigue como '%s'",
                     r->IdAlmacen().c_str(), r->nombre.c_str(), r->rec->id.c_str());
    }
}

void W3dMallasGuardarConfirmar() {
    if (!gPlanListo) { W3dMallasGuardarDescartar(); return; }
    // la malla en edicion: el puntero del tick se valida (pudo quedar de un proyecto cerrado si
    // ningun tick corrio en el medio: sin viewport 3D en el layout, el harness)
    if (gEnEdicion && !UsuarioVivo(gEnEdicion)) gEnEdicion = NULL;
    Mesh* enEd = gEnEdicion ? gEnEdicion : (Mesh*)g_editMesh;
    MudarEntradas();
    // los BORRADOS que retiene el undo viven desde ahora de sus bytes en memoria (lo unico que queda de ellos).
    // Si un Ctrl+Z les devuelve sus objetos vuelven a ser del proyecto como un huerfano del archivo (se conservan
    // y el proximo guardado los escribe); si no, la purga los borra cuando el undo los suelta
    for (size_t i = 0; i < gDesalojados.size(); i++) {
        MallaRecurso* r = W3dMallaRecursoPorSerial(gDesalojados[i].first);
        if (!r) continue;
        if (!gDesalojados[i].second.empty()) r->bytes.swap(gDesalojados[i].second);
        r->bytesDeTexto = false;   // (la conversion de un .w3dm ya no es un cache: su entrada no esta)
        r->modificado = true;
        r->conservar = true;
    }
    for (size_t o = 0; o < gOrden.size(); o++) {
        const size_t gi = gOrden[o];
        Grupo& g = gPlan[gi];
        if (g.perdido) continue;   // (un huerfano ilegible: sin entrada, lo purga el proximo tick)
        MallaRecurso* r = g.rec;
        if (!r) {
            // un recurso NUEVO: sus datos estan en la entrada recien escrita
            r = W3dMallaRecursoNuevo(g.nombre, g.entrada, g.carpeta);
            r->materiales = g.mats;
        } else {
            if (!g.contenidoDelRecurso) {
                // el recurso ADOPTA el contenido de un usuario tocado: sus usuarios pasan a los
                // datos nuevos. Se quedan con lo suyo la malla en edicion (hasta salir) y los
                // usuarios que el plan manda a OTRO recurso (se re-vinculan cuando les toque:
                // re-apuntarlos aca les cambiaba lo que muestran por el contenido adoptado)
                std::vector<Mesh*> ex;
                if (enEd) ex.push_back(enEd);
                for (size_t k = 0; k < r->usuarios.size(); k++) {
                    std::map<const Mesh*, size_t>::const_iterator ip = gPlanDe.find(r->usuarios[k]);
                    if (ip != gPlanDe.end() && ip->second != gi) ex.push_back(r->usuarios[k]);
                }
                std::vector<Material*> mats = g.mats;
                W3dMallaRecursoAplicarBytes(r, g.bytes, ex, &mats);
                RegenerarUsuarios(r, ex);
            }
            if (!r->nombre.empty() && r->nombre != g.nombre) r->nombre = g.nombre;
        }
        if (gPlanTexto) {
            // TEXTO: la entrada es un .w3dm. Los bytes binarios que ya estan en memoria (los del
            // grupo, o los que el recurso tenia) son el CACHE de su conversion: no se vuelve a leer
            // el texto para dibujar ni para materializar.
            if (!g.bytes.empty()) r->bytes = g.bytes;
            r->bytesDeTexto = !r->bytes.empty();
        } else {
            // lo que se escribio ES el recurso: los bytes en memoria sobran (la entrada los tiene)
            r->bytes.clear();
            r->bytesDeTexto = false;
        }
        r->modificado = false;
        if (!g.mats.empty() && g.mats.size() == r->partes.size()) r->materiales = g.mats;
        // los objetos del grupo pasan a usar ESTE recurso (los sueltos y los tocados comparten
        // desde ya; la malla en edicion sigue con lo suyo hasta salir)
        for (size_t k = 0; k < g.mallas.size(); k++) {
            Mesh* m = g.mallas[k];
            if (!m) continue;
            if (m->malla == r && W3dMallaLimpia(m)) continue;
            if (m == (Mesh*)g_editMesh || m == gEnEdicion || !SePuedeRevincular(m)) {
                // la malla en EDICION (o pintando, o POSADA por una animacion) no se re-vincula:
                // su jaula, sus capas y lo que muestra siguen siendo suyos. Si su grupo es OTRO
                // recurso (una malla suelta, un usuario tocado que se separo), el vinculo se muda
                // sin tocar sus datos.
                if (m->malla != r) W3dMallaMudar(m, r);
                continue;
            }
            const bool conserva = m->edicionPendiente.empty() && !m->faces3d.empty();
            if (W3dMallaVincular(m, r, conserva) && !m->modificadores.empty()) m->GenerarMallaModificada();
        }
    }
    W3dMallasGuardarDescartar();
}

// LAS MALLAS SUELTAS DE UN ARCHIVO: todo lo que abrio sin un recurso del registro (una
// "geometria" por objeto, un .obj, un GLB, el texto viejo) se puede juntar por contenido al
// guardar (ver Mesh::dedupPorContenido)
static void MarcarSueltas(Object* o) {
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::mesh && !((Mesh*)h)->malla) ((Mesh*)h)->dedupPorContenido = true;
        MarcarSueltas(h);
    }
}
void W3dMallasMarcarSueltasDelArchivo() {
    if (SceneCollection) MarcarSueltas(SceneCollection);
}
