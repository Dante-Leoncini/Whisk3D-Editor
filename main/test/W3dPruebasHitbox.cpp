// ============================================================================
//  W3dPruebasHitbox.cpp — comandos de harness del HITBOX y de los eventos lua.
//  Ver W3dPruebasHitbox.h. Los llama W3dPruebasRecursosCmd antes que los suyos.
//
//  Comandos:
//    hitboxnuevo <nombre> [padre N] [pos x y z] [rot x y z] [escala x y z] [tam x y z]
//                [centro x y z] [etiqueta S] [filtro S] [cuerpos 0|1] [activo 0|1] [visible 0|1]
//        crea un Hitbox ('-' = texto vacio). Sin padre cuelga de la escena.
//    hitboxinfo <nombre|*> [activo 0|1] [tam x y z] [centro x y z] [etiqueta S] [filtro S]
//               [cuerpos 0|1] [padre N|-] [duenio N] [tocando 0|1] [dentro N]
//        informa un hitbox ('*' = el activo) y asserta lo pedido (tolerancia 1e-4).
//    hitboxstats [hitboxes N] [cuerpos N] [pares N] [contactos N]
//        las cantidades del ULTIMO paso del motor (W3dHitboxEstadisticas).
//    hitboxadd [objeto|-]
//        Add > Hitbox del menu (la MISMA accion): con objeto, lo deja activo antes (el hitbox
//        nace hijo suyo, del tamano de sus mallas); con '-', sin activo (suelto en el cursor).
//    hitboxdup <nombre>
//        W3dDuplicarUno (el nucleo de Shift+D): la copia tiene los mismos campos y otro nombre.
//    hitboxprops <nombre>
//        la tarjeta "Hitbox" del primer panel de propiedades: visible y bindeada con el hitbox
//        activo, el texto de etiqueta/filtro con commit en vivo, el icono de la pestania y
//        el del outliner; con otro objeto activo la tarjeta se oculta y suelta los punteros.
//    compartidonum <clave> <min> [max]
//        el valor NUMERICO de compartido(clave) esta en [min, max] (max default = min).
//    hitboxpaso [n] / hitboxlimpiar
//        el motor de hitbox SOLO (sin Play ni scripts): n pasos / olvidar los pares.
//    nombreactivo <nombre>
//        renombra el objeto activo (el nombre de una primitiva depende del idioma).
//    posobjeto <nombre> x y z [tol]
//        la posicion LOCAL del objeto (tolerancia default 1e-3): lo que restaura el Stop.
//    hitboxgrilla <n> [grande 0|1] [cizalla 0|1] [maxms X]
//        escena NUEVA con n hitbox al azar (determinista: tamanos, rotaciones y escalas
//        distintas) y opcionalmente uno ENORME; un paso del motor contra la fuerza bruta
//        (SAT de todos los pares): tienen que dar los MISMOS pares. Informa ms del paso.
//        Con cizalla, uno de cada 5 cuelga de un vacio con escala NO uniforme y rotado (su
//        caja en mundo es un paralelepipedo): la esfera/grilla no tiene que perder pares.
//    hitboxsat <n> [semilla S]
//        el SAT (W3dObbSolapan) contra una referencia EXACTA independiente (aristas de cada
//        caja recortadas contra la otra en sus coordenadas afines) en n pares al azar de
//        cajas y PARALELEPIPEDOS (cizalla): ni falsos negativos ni falsos positivos.
//    hitboxvista <nombre> [overlays 0|1] [tipo 0|1] [queda 0|1] [dibuja 0|1] [pick 0|1] [ver 0|1]
//        lo que el viewport 3D VE y CLICKEA del hitbox: pone Show Overlays / Objects > Hitbox
//        del viewport (queda 1 = no los repone al final), dibuja y cuenta los pixeles del verde
//        del hitbox, hace un click (ScenePick3D) en el medio de una arista, y asserta: dibuja
//        (hay alambre), pick (el click lo elige) y ver (el overlay de debug verHitboxes).
//    ordenhijo <nombre> <indice>
//        mueve el objeto a esa posicion entre sus hermanos (lo que hace arrastrar en el
//        outliner): el orden del ARBOL deja de ser el de creacion (serial).
//    juegohitboxmin <carpeta>
//        el juego 3D minimo (juego3dmin) + hitbox, cuerpos rigidos y una animacion de escena,
//        con scripts que cuentan los eventos y un JUEZ que deja el veredicto en compartido
//        ("juez") y en el log, y pide salir() cuando termina.
//    juegohitboxlog <carpeta> <nombre>
//        corre el binario compilado y lee el veredicto del juez en su whisk3d.log.
// ============================================================================
#include "test/W3dPruebasHitbox.h"
#include "test/W3dScript.h"            // W3dRunCommand("juego3dmin ...")
#include "objects/Objects.h"
#include "objects/Hitbox.h"
#include "objects/Empty.h"
#include "objects/ObjectMode.h"        // W3dDuplicarUno
#include "physics/W3dHitbox.h"
#include "physics/W3dRigido.h"
#include "script/W3dScript.h"
#include "animation/Animation.h"       // la animacion de escena del juego de prueba
#include "config/W3dProfile.h"         // W3dNowMs
#include "ViewPorts/ViewPorts.h"       // rootViewport / IconoDeObjeto
#include "ViewPorts/Properties.h"
#include "WhiskUI/draw/icons.h"
#include "ViewPorts/ViewPort3D.h"     // hitboxvista: los flags de overlays del viewport
#include "ViewPorts/Pick3D.h"         // hitboxvista: el click de verdad (ScenePick3D)
#include "w3dGraphics.h"
#include "w3dFilesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>

extern void ReiniciarEscena();

// ---------------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------------
static Object* HbBuscar(const std::string& n) {
    if (n == "*") return ObjActivo;
    return SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
}
static std::string HbTexto(const std::string& s) { return s == "-" ? std::string() : s; }
static bool HbCerca(float a, float b) { return fabsf(a - b) <= 1e-4f; }

static void HbNodos(ViewportBase* n, std::vector<ViewportBase*>& hojas) {
    if (!n) return;
    if (n->isLeaf()) { hojas.push_back(n); return; }
    if (n->ContainerKind() == 1) { HbNodos(((ViewportRow*)n)->childA, hojas); HbNodos(((ViewportRow*)n)->childB, hojas); }
    else { HbNodos(((ViewportColumn*)n)->childA, hojas); HbNodos(((ViewportColumn*)n)->childB, hojas); }
}
static ViewportBase* HbPrimeraHoja(int kind) {
    std::vector<ViewportBase*> hojas;
    HbNodos(rootViewport, hojas);
    for (size_t i = 0; i < hojas.size(); i++) if (hojas[i]->ViewportKind() == kind) return hojas[i];
    return NULL;
}

static bool HbCompartido(const std::string& clave, std::string* valor) {
    const int nc = W3dScriptCompartidoCantidad();
    for (int i = 0; i < nc; i++) {
        std::string k, v;
        if (W3dScriptCompartidoPar(i, &k, &v) && k == clave) { if (valor) *valor = v; return true; }
    }
    return false;
}

static bool HbEscribir(const std::string& ruta, const char* texto) {
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f) return false;
    fputs(texto, f);
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxnuevo
// ---------------------------------------------------------------------------
static bool CmdHitboxNuevo(std::istringstream& ss, std::string& err) {
    std::string nombre; ss >> nombre;
    if (nombre.empty()) { err = "hitboxnuevo: uso: hitboxnuevo <nombre> [clave valor]..."; return false; }
    Object* padre = NULL;
    Vector3 pos(0, 0, 0), rot(0, 0, 0), escala(1, 1, 1);
    float tam[3] = { 1, 1, 1 }, centro[3] = { 0, 0, 0 };
    std::string etiqueta, filtro;
    int cuerpos = 0, activo = 1, visible = 1;
    std::string clave;
    while (ss >> clave) {
        if (clave == "padre") {
            std::string pn; ss >> pn;
            padre = HbBuscar(pn);
            if (!padre) { err = "hitboxnuevo: no existe el padre '" + pn + "'"; return false; }
        } else if (clave == "pos" || clave == "rot" || clave == "escala" || clave == "tam" || clave == "centro") {
            float x = 0, y = 0, z = 0;
            if (!(ss >> x >> y >> z)) { err = "hitboxnuevo: '" + clave + "' pide x y z"; return false; }
            if (clave == "pos") pos = Vector3(x, y, z);
            else if (clave == "rot") rot = Vector3(x, y, z);
            else if (clave == "escala") escala = Vector3(x, y, z);
            else if (clave == "tam") { tam[0] = x; tam[1] = y; tam[2] = z; }
            else { centro[0] = x; centro[1] = y; centro[2] = z; }
        } else if (clave == "etiqueta") { std::string v; ss >> v; etiqueta = HbTexto(v); }
        else if (clave == "filtro")     { std::string v; ss >> v; filtro = HbTexto(v); }
        else if (clave == "cuerpos")    ss >> cuerpos;
        else if (clave == "activo")     ss >> activo;
        else if (clave == "visible")    ss >> visible;
        else { err = "hitboxnuevo: clave desconocida '" + clave + "'"; return false; }
    }
    Hitbox* h = new Hitbox(padre, pos);
    h->SetNameObj(nombre);
    h->SetRotEuler(rot);
    h->scale = escala;
    for (int k = 0; k < 3; k++) { h->tam[k] = tam[k]; h->centro[k] = centro[k]; }
    h->etiqueta = etiqueta; h->filtro = filtro;
    h->detectarCuerpos = cuerpos != 0; h->activo = activo != 0; h->visible = visible != 0;
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [hitboxnuevo] '%s' padre=%s tam=(%g,%g,%g)\n", h->name.c_str(),
           padre ? padre->name.c_str() : "(escena)", h->tam[0], h->tam[1], h->tam[2]);
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxinfo
// ---------------------------------------------------------------------------
static bool CmdHitboxInfo(std::istringstream& ss, std::string& err) {
    std::string nombre; ss >> nombre;
    Object* o = HbBuscar(nombre);
    if (!o || o->getType() != ObjectType::hitbox) { err = "hitboxinfo: '" + nombre + "' no es un hitbox"; return false; }
    W3dHitboxBase* h = (W3dHitboxBase*)o;
    std::vector<Object*> dentro;
    W3dHitboxDentro(h, &dentro);
    Object* du = W3dHitboxDuenio(h);
    const bool padreEsEscena = !h->Parent || h->Parent == SceneCollection;
    printf("      [hitboxinfo] '%s' activo=%d tam=(%g,%g,%g) centro=(%g,%g,%g) etiqueta='%s' filtro='%s' "
           "cuerpos=%d padre=%s duenio=%s tocando=%d dentro=%d\n",
           h->name.c_str(), h->activo ? 1 : 0, h->tam[0], h->tam[1], h->tam[2],
           h->centro[0], h->centro[1], h->centro[2], h->etiqueta.c_str(), h->filtro.c_str(),
           h->detectarCuerpos ? 1 : 0, padreEsEscena ? "-" : h->Parent->name.c_str(),
           du ? du->name.c_str() : "?", W3dHitboxTocaAlgo(h) ? 1 : 0, (int)dentro.size());
    for (size_t i = 0; i < dentro.size(); i++) printf("         adentro: %s\n", dentro[i]->name.c_str());
    std::string clave;
    while (ss >> clave) {
        char b[256];
        if (clave == "tam" || clave == "centro") {
            float x = 0, y = 0, z = 0; ss >> x >> y >> z;
            const float* v = (clave == "tam") ? h->tam : h->centro;
            if (!HbCerca(v[0], x) || !HbCerca(v[1], y) || !HbCerca(v[2], z)) {
                snprintf(b, sizeof(b), "hitboxinfo: %s = (%g,%g,%g), se esperaba (%g,%g,%g)", clave.c_str(),
                         v[0], v[1], v[2], x, y, z);
                err = b; return false;
            }
        } else if (clave == "activo" || clave == "cuerpos" || clave == "tocando" || clave == "dentro") {
            int v = 0; ss >> v;
            int real = (clave == "activo") ? (h->activo ? 1 : 0) : (clave == "cuerpos") ? (h->detectarCuerpos ? 1 : 0)
                     : (clave == "tocando") ? (W3dHitboxTocaAlgo(h) ? 1 : 0) : (int)dentro.size();
            if (real != v) { snprintf(b, sizeof(b), "hitboxinfo: %s = %d, se esperaba %d", clave.c_str(), real, v); err = b; return false; }
        } else if (clave == "etiqueta" || clave == "filtro") {
            std::string v; ss >> v; v = HbTexto(v);
            const std::string& real = (clave == "etiqueta") ? h->etiqueta : h->filtro;
            if (real != v) { err = "hitboxinfo: " + clave + " = '" + real + "', se esperaba '" + v + "'"; return false; }
        } else if (clave == "padre") {
            std::string v; ss >> v;
            const std::string real = padreEsEscena ? std::string("-") : h->Parent->name;
            if (real != v) { err = "hitboxinfo: padre = '" + real + "', se esperaba '" + v + "'"; return false; }
        } else if (clave == "duenio") {
            std::string v; ss >> v;
            if (!du || du->name != v) { err = "hitboxinfo: duenio = '" + (du ? du->name : std::string("?")) + "', se esperaba '" + v + "'"; return false; }
        } else { err = "hitboxinfo: assert desconocido '" + clave + "'"; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxstats
// ---------------------------------------------------------------------------
static bool CmdHitboxStats(std::istringstream& ss, std::string& err) {
    const W3dHitboxStats st = W3dHitboxEstadisticas();
    printf("      [hitboxstats] hitboxes=%d cuerpos=%d probados=%d pares=%d contactos=%d\n",
           st.hitboxes, st.cuerpos, st.probados, st.pares, st.contactos);
    std::string clave;
    while (ss >> clave) {
        int v = 0;
        if (!(ss >> v)) { err = "hitboxstats: falta el valor de '" + clave + "'"; return false; }
        int real = -1;
        if (clave == "hitboxes") real = st.hitboxes;
        else if (clave == "cuerpos") real = st.cuerpos;
        else if (clave == "pares") real = st.pares;
        else if (clave == "contactos") real = st.contactos;
        else { err = "hitboxstats: assert desconocido '" + clave + "'"; return false; }
        if (real != v) {
            char b[160]; snprintf(b, sizeof(b), "hitboxstats: %s = %d, se esperaba %d", clave.c_str(), real, v);
            err = b; return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxadd / hitboxdup
// ---------------------------------------------------------------------------
static bool CmdHitboxAdd(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    DeseleccionarTodo();
    ObjActivo = NULL;
    if (!n.empty() && n != "-") {
        Object* o = HbBuscar(n);
        if (!o) { err = "hitboxadd: no existe '" + n + "'"; return false; }
        o->Seleccionar();
    }
    extern void AddHitbox();
    AddHitbox();
    Object* nuevo = ObjActivo;
    if (!nuevo || nuevo->getType() != ObjectType::hitbox) { err = "hitboxadd: Add > Hitbox no dejo un hitbox activo"; return false; }
    W3dHitboxBase* h = (W3dHitboxBase*)nuevo;
    printf("      [hitboxadd] '%s' padre=%s tam=(%g,%g,%g) centro=(%g,%g,%g)\n", h->name.c_str(),
           (h->Parent && h->Parent != SceneCollection) ? h->Parent->name.c_str() : "-",
           h->tam[0], h->tam[1], h->tam[2], h->centro[0], h->centro[1], h->centro[2]);
    return true;
}

static bool CmdHitboxDup(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Object* o = HbBuscar(n);
    if (!o || o->getType() != ObjectType::hitbox) { err = "hitboxdup: '" + n + "' no es un hitbox"; return false; }
    W3dHitboxBase* s = (W3dHitboxBase*)o;
    Object* d0 = W3dDuplicarUno(o);
    if (!d0 || d0->getType() != ObjectType::hitbox) { err = "hitboxdup: la copia no es un hitbox"; return false; }
    W3dHitboxBase* d = (W3dHitboxBase*)d0;
    bool igual = d->activo == s->activo && d->etiqueta == s->etiqueta && d->filtro == s->filtro &&
                 d->detectarCuerpos == s->detectarCuerpos && d->Parent == s->Parent &&
                 d->pos.x == s->pos.x && d->pos.y == s->pos.y && d->pos.z == s->pos.z &&
                 d->scale.x == s->scale.x && d->scale.y == s->scale.y && d->scale.z == s->scale.z;
    for (int k = 0; k < 3; k++) igual = igual && d->tam[k] == s->tam[k] && d->centro[k] == s->centro[k];
    const bool conScripts = (s->scriptDatos != NULL) == (d->scriptDatos != NULL);
    printf("      [hitboxdup] '%s' -> '%s' campos %s, scripts %s\n", s->name.c_str(), d->name.c_str(),
           igual ? "IGUALES" : "DISTINTOS", conScripts ? "copiados" : "PERDIDOS");
    if (!igual || !conScripts) { err = "hitboxdup: la copia no tiene los mismos campos"; return false; }
    if (d->name == s->name) { err = "hitboxdup: la copia tiene el MISMO nombre"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxprops
// ---------------------------------------------------------------------------
static bool CmdHitboxProps(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    Object* o = HbBuscar(n);
    if (!o || o->getType() != ObjectType::hitbox) { err = "hitboxprops: '" + n + "' no es un hitbox"; return false; }
    W3dHitboxBase* h = (W3dHitboxBase*)o;
    Properties* pr = (Properties*)HbPrimeraHoja(3);
    if (!pr || !pr->propHitbox) { err = "hitboxprops: no hay panel de propiedades con la tarjeta Hitbox"; return false; }
    DeseleccionarTodo();
    o->Seleccionar();
    pr->pestaniaActiva = 2;
    pr->ActualizarPestanias();
    bool ok = pr->propHitbox->visible && pr->propHbActivo->value == &h->activo &&
              pr->propHbCuerpos->value == &h->detectarCuerpos;
    for (int k = 0; k < 3; k++)
        ok = ok && pr->propHbTam[k]->value == &h->tam[k] && pr->propHbCentro[k]->value == &h->centro[k];
    const bool iconoTab = pr->BarTabs.size() >= 3 && pr->BarTabs[2]->visible && pr->BarTabs[2]->icon == (int)IconType::hitbox;
    const bool iconoOut = IconoDeObjeto(o) == (size_t)IconType::hitbox;
    // texto con commit EN VIVO: lo tipeado va al hitbox; sin foco el campo muestra el valor real
    const std::string etiqueta0 = h->etiqueta, filtro0 = h->filtro;
    g_textFieldActivo = &pr->propHbEtiqueta->field;
    pr->propHbEtiqueta->field.SetText("tipeada");
    pr->ActualizarPestanias();
    const bool vivo = (h->etiqueta == "tipeada");
    g_textFieldActivo = &pr->propHbFiltro->field;
    pr->propHbFiltro->field.SetText("otro");
    pr->ActualizarPestanias();
    const bool vivoFiltro = (h->filtro == "otro");
    g_textFieldActivo = NULL;
    h->etiqueta = "desde lua";
    pr->ActualizarPestanias();
    const bool muestra = (pr->propHbEtiqueta->field.text == "desde lua");
    h->etiqueta = etiqueta0; h->filtro = filtro0;
    pr->ActualizarPestanias();
    // con OTRO objeto activo la tarjeta se oculta y suelta los punteros
    Empty* otro = new Empty(NULL, Vector3(0, 0, 0));
    otro->Seleccionar();
    pr->ActualizarPestanias();
    const bool suelta = !pr->propHitbox->visible && pr->propHbActivo->value == NULL && pr->propHbTam[0]->value == NULL;
    DeseleccionarTodo(); ObjActivo = NULL;
    std::vector<Object*>& hs = SceneCollection->Childrens;   // sacarlo de la escena ANTES de liberarlo
    for (size_t i = 0; i < hs.size(); i++) if (hs[i] == otro) { hs.erase(hs.begin() + i); break; }
    delete otro;
    printf("      [hitboxprops] tarjeta %s | texto en vivo %s/%s | muestra el valor real %s | icono pestania %s | "
           "icono outliner %s | otro activo la oculta %s\n",
           ok ? "BINDEADA" : "MAL", vivo ? "OK" : "MAL", vivoFiltro ? "OK" : "MAL", muestra ? "OK" : "MAL",
           iconoTab ? "OK" : "MAL", iconoOut ? "OK" : "MAL", suelta ? "OK" : "MAL");
    if (!ok || !vivo || !vivoFiltro || !muestra || !iconoTab || !iconoOut || !suelta) {
        err = "hitboxprops: la tarjeta Hitbox no se comporta (ver arriba)"; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  compartidonum
// ---------------------------------------------------------------------------
static bool CmdCompartidoNum(std::istringstream& ss, std::string& err) {
    std::string clave; double mn = 0, mx = 0;
    ss >> clave >> mn;
    if (!(ss >> mx)) mx = mn;
    std::string v;
    if (!HbCompartido(clave, &v)) { err = "compartidonum: no existe la clave '" + clave + "'"; return false; }
    const double x = atof(v.c_str());
    printf("      [compartidonum] %s = %s (rango %g..%g)\n", clave.c_str(), v.c_str(), mn, mx);
    if (x < mn - 1e-6 || x > mx + 1e-6) {
        char b[200]; snprintf(b, sizeof(b), "compartidonum: %s = %s, fuera de [%g, %g]", clave.c_str(), v.c_str(), mn, mx);
        err = b; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  posobjeto <nombre> x y z [tol]: la posicion LOCAL de un objeto (lo que restaura Stop)
// ---------------------------------------------------------------------------
static bool CmdPosObjeto(std::istringstream& ss, std::string& err) {
    std::string n; float x = 0, y = 0, z = 0, tol = 1e-3f;
    ss >> n >> x >> y >> z; ss >> tol;
    Object* o = HbBuscar(n);
    if (!o) { err = "posobjeto: no existe '" + n + "'"; return false; }
    const float d = sqrtf((o->pos.x - x) * (o->pos.x - x) + (o->pos.y - y) * (o->pos.y - y) + (o->pos.z - z) * (o->pos.z - z));
    printf("      [posobjeto] '%s' en (%.4f, %.4f, %.4f)\n", n.c_str(), o->pos.x, o->pos.y, o->pos.z);
    if (d > tol) {
        char b[200]; snprintf(b, sizeof(b), "posobjeto: '%s' esta a %.4f de (%g, %g, %g)", n.c_str(), d, x, y, z);
        err = b; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxpaso [n] / hitboxlimpiar: el motor SOLO (sin Play ni scripts), n pasos de 1/30 s
// ---------------------------------------------------------------------------
static bool CmdHitboxPaso(std::istringstream& ss, std::string& err) {
    (void)err;
    int n = 1; ss >> n; if (n < 1) n = 1;
    for (int i = 0; i < n; i++) W3dHitboxPaso(1.0f / 30.0f);
    const W3dHitboxStats st = W3dHitboxEstadisticas();
    printf("      [hitboxpaso] %d paso(s): hitboxes=%d pares=%d\n", n, st.hitboxes, st.pares);
    { extern bool g_redraw; g_redraw = true; }
    return true;
}

// ---------------------------------------------------------------------------
//  nombreactivo <nombre>: renombra el objeto activo (los nombres de las primitivas
//  dependen del idioma del sistema: "Cube"/"Cubo")
// ---------------------------------------------------------------------------
static bool CmdNombreActivo(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (!ObjActivo || n.empty()) { err = "nombreactivo: uso: nombreactivo <nombre> (con un objeto activo)"; return false; }
    ObjActivo->SetNameObj(n);
    if (ObjActivo->name != n) { err = "nombreactivo: el nombre '" + n + "' ya estaba usado"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxgrilla: el broadphase contra la fuerza bruta
// ---------------------------------------------------------------------------
struct HbCajaPrueba { Vector3 c, u[3], h; };
static HbCajaPrueba HbCajaMundo(W3dHitboxBase* hb) {
    HbCajaPrueba r;
    Matrix4 M; hb->GetWorldMatrix(M);
    const Vector3 col[3] = { Vector3(M.m[0], M.m[1], M.m[2]), Vector3(M.m[4], M.m[5], M.m[6]), Vector3(M.m[8], M.m[9], M.m[10]) };
    float hh[3];
    for (int k = 0; k < 3; k++) {
        const float len = col[k].Length();
        r.u[k] = col[k] * (1.0f / len);
        hh[k] = 0.5f * fabsf(hb->tam[k]) * len;
        if (hh[k] < 1e-4f) hh[k] = 1e-4f;
    }
    r.h = Vector3(hh[0], hh[1], hh[2]);
    r.c = M * Vector3(hb->centro[0], hb->centro[1], hb->centro[2]);
    return r;
}
static unsigned gHbSemilla = 12345u;
static float HbAzar() { gHbSemilla = gHbSemilla * 1664525u + 1013904223u; return (float)((gHbSemilla >> 8) & 0xFFFF) / 65535.0f; }

static bool CmdHitboxGrilla(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 2) { err = "hitboxgrilla: uso: hitboxgrilla <n> [grande 0|1] [cizalla 0|1] [maxms X]"; return false; }
    int grande = 0, cizalla = 0; double maxms = -1.0;
    std::string clave;
    while (ss >> clave) {
        if (clave == "grande") ss >> grande;
        else if (clave == "cizalla") ss >> cizalla;
        else if (clave == "maxms") ss >> maxms;
        else { err = "hitboxgrilla: clave desconocida '" + clave + "'"; return false; }
    }
    ReiniciarEscena();
    gHbSemilla = 12345u;
    const float lado = 1.2f * cbrtf((float)n);
    std::vector<W3dHitboxBase*> hbs;
    int cizallados = 0;
    W3dNombresCargando = true;   // nombres crudos (uniquificar uno a uno es O(n^2))
    for (int i = 0; i < n; i++) {
        const Vector3 p(HbAzar() * lado, HbAzar() * lado, HbAzar() * lado);
        Hitbox* h = NULL;
        if (cizalla && i % 5 == 0) {
            // CIZALLA: un vacio con escala NO uniforme y rotado, y el hitbox rotado adentro: su
            // matriz de mundo no tiene columnas perpendiculares (la caja es un paralelepipedo)
            Empty* pa = new Empty(NULL, p);
            char np[32]; snprintf(np, sizeof(np), "Pa%d", i);
            pa->SetNameCrudo(np);
            pa->SetRotEuler(Vector3(HbAzar() * 360.0f, HbAzar() * 360.0f, HbAzar() * 360.0f));
            pa->scale = Vector3(0.4f + 2.0f * HbAzar(), 0.4f + 2.0f * HbAzar(), 0.4f + 2.0f * HbAzar());
            h = new Hitbox(pa, Vector3(0, 0, 0));
            cizallados++;
        } else {
            h = new Hitbox(NULL, p);
        }
        char nb[32]; snprintf(nb, sizeof(nb), "Hb%d", i);
        h->SetNameCrudo(nb);
        for (int k = 0; k < 3; k++) h->tam[k] = 0.2f + HbAzar();
        h->SetRotEuler(Vector3(HbAzar() * 360.0f, HbAzar() * 360.0f, HbAzar() * 360.0f));
        if (i % 7 == 0) h->scale = Vector3(1.5f, 0.5f, 1.0f);     // escala de mundo aplicada
        if (i % 11 == 0) { h->centro[0] = 0.3f; h->centro[2] = -0.2f; }
        hbs.push_back(h);
    }
    if (grande) {
        Hitbox* g = new Hitbox(NULL, Vector3(lado * 0.5f, lado * 0.5f, lado * 0.5f));
        g->SetNameCrudo("Enorme");
        g->tam[0] = lado * 0.6f; g->tam[1] = lado * 0.4f; g->tam[2] = lado * 0.5f;
        g->SetRotEuler(Vector3(10.0f, 25.0f, 0.0f));
        hbs.push_back(g);
    }
    W3dNombresCargando = false;
    DeseleccionarTodo(); ObjActivo = NULL;
    // fuerza bruta: el SAT en TODOS los pares
    std::vector<HbCajaPrueba> cajas;
    for (size_t i = 0; i < hbs.size(); i++) cajas.push_back(HbCajaMundo(hbs[i]));
    const double tb0 = W3dNowMs();
    int bruta = 0;
    for (size_t i = 0; i < cajas.size(); i++)
        for (size_t j = i + 1; j < cajas.size(); j++)
            if (W3dObbSolapan(cajas[i].c, cajas[i].u, cajas[i].h, cajas[j].c, cajas[j].u, cajas[j].h)) bruta++;
    const double msBruta = W3dNowMs() - tb0;
    W3dHitboxLimpiar();
    const double t0 = W3dNowMs();
    W3dHitboxPaso(1.0f / 30.0f);
    const double ms = W3dNowMs() - t0;
    const W3dHitboxStats st = W3dHitboxEstadisticas();
    // segundo paso: los mismos pares (todos "siguen") y sin allocations nuevas
    const double t1 = W3dNowMs();
    W3dHitboxPaso(1.0f / 30.0f);
    const double ms2 = W3dNowMs() - t1;
    const W3dHitboxStats st2 = W3dHitboxEstadisticas();
    printf("      [hitboxgrilla] n=%d%s%s | pares motor=%d (2do paso %d) fuerza bruta=%d | SAT probados=%d de %d pares posibles"
           " | paso %.2f ms (2do %.2f ms) | fuerza bruta %.2f ms\n",
           (int)hbs.size(), grande ? " (+1 enorme)" : "", cizallados ? " (con cizalla)" : "", st.pares, st2.pares, bruta, st.probados,
           (int)(hbs.size() * (hbs.size() - 1) / 2), ms, ms2, msBruta);
    W3dHitboxLimpiar();
    if (st.hitboxes != (int)hbs.size()) { err = "hitboxgrilla: el motor no recolecto todos los hitbox"; return false; }
    if (st.pares != bruta || st2.pares != bruta) { err = "hitboxgrilla: el broadphase PERDIO o INVENTO pares (no coincide con la fuerza bruta)"; return false; }
    if (maxms > 0.0 && ms2 > maxms) {
        char b[160]; snprintf(b, sizeof(b), "hitboxgrilla: el paso tardo %.2f ms (tope %.2f)", ms2, maxms);
        err = b; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxsat: el SAT contra una referencia EXACTA e independiente
// ---------------------------------------------------------------------------
// una caja (o paralelepipedo) en doble precision: centro y semi-aristas (h_k * u_k)
struct HbParalelepipedo { double c[3], e[3][3]; };   // e[k] = semiarista k (vector)

static HbParalelepipedo HbParDe(const Vector3& c, const Vector3* u, const Vector3& h, double f) {
    HbParalelepipedo p;
    const double hh[3] = { h.x * f, h.y * f, h.z * f };
    p.c[0] = c.x; p.c[1] = c.y; p.c[2] = c.z;
    for (int k = 0; k < 3; k++) { p.e[k][0] = u[k].x * hh[k]; p.e[k][1] = u[k].y * hh[k]; p.e[k][2] = u[k].z * hh[k]; }
    return p;
}

// coordenadas AFINES de un punto en la caja q (q = cubo [-1,1]^3 en esas coordenadas)
static bool HbAfin(const HbParalelepipedo& q, const double pw[3], double t[3]) {
    const double (*m)[3] = q.e;   // columnas = semiaristas
    // inversa de la matriz de columnas e0 e1 e2 por cofactores
    const double a = m[0][0], b = m[1][0], c = m[2][0];
    const double d = m[0][1], e = m[1][1], f = m[2][1];
    const double g = m[0][2], h = m[1][2], i = m[2][2];
    const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (fabs(det) < 1e-12) return false;
    const double r[3] = { pw[0] - q.c[0], pw[1] - q.c[1], pw[2] - q.c[2] };
    t[0] = ((e * i - f * h) * r[0] + (c * h - b * i) * r[1] + (b * f - c * e) * r[2]) / det;
    t[1] = ((f * g - d * i) * r[0] + (a * i - c * g) * r[1] + (c * d - a * f) * r[2]) / det;
    t[2] = ((d * h - e * g) * r[0] + (b * g - a * h) * r[1] + (a * e - b * d) * r[2]) / det;
    return true;
}

// el segmento a-b (coordenadas afines) toca el cubo [-1,1]^3? (Liang-Barsky)
static bool HbSegmentoEnCubo(const double a[3], const double b[3]) {
    double t0 = 0.0, t1 = 1.0;
    for (int k = 0; k < 3; k++) {
        const double d = b[k] - a[k];
        const double ps[2] = { -d, d };
        const double qs[2] = { a[k] + 1.0, 1.0 - a[k] };
        for (int s2 = 0; s2 < 2; s2++) {
            if (ps[s2] == 0.0) { if (qs[s2] < 0.0) return false; continue; }
            const double r = qs[s2] / ps[s2];
            if (ps[s2] < 0.0) { if (r > t0) t0 = r; } else { if (r < t1) t1 = r; }
            if (t0 > t1) return false;
        }
    }
    return true;
}

// alguna ARISTA de p toca el solido q? Dos poliedros convexos se tocan si y solo si alguna arista
// de uno toca al otro (todo vertice de la interseccion esta en una arista de alguno de los dos)
static bool HbAristasTocan(const HbParalelepipedo& p, const HbParalelepipedo& q) {
    double v[8][3];
    for (int i = 0; i < 8; i++) {
        double w[3];
        for (int a = 0; a < 3; a++)
            w[a] = p.c[a] + ((i & 1) ? 1 : -1) * p.e[0][a] + ((i & 2) ? 1 : -1) * p.e[1][a] + ((i & 4) ? 1 : -1) * p.e[2][a];
        if (!HbAfin(q, w, v[i])) return false;
    }
    for (int i = 0; i < 8; i++)
        for (int bit = 1; bit < 8; bit <<= 1)
            if (!(i & bit) && HbSegmentoEnCubo(v[i], v[i | bit])) return true;
    return false;
}

static bool HbReferenciaTocan(const HbParalelepipedo& p, const HbParalelepipedo& q) {
    return HbAristasTocan(p, q) || HbAristasTocan(q, p);
}

// una rotacion al azar (tres angulos) como matriz 3x3 de filas
static void HbRotAzar(float r[3][3]) {
    const float a = HbAzar() * 6.2831853f, b = HbAzar() * 6.2831853f, c = HbAzar() * 6.2831853f;
    const float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b), cc = cosf(c), sc = sinf(c);
    // Rz(c) * Ry(b) * Rx(a)
    r[0][0] = cc * cb; r[0][1] = cc * sb * sa - sc * ca; r[0][2] = cc * sb * ca + sc * sa;
    r[1][0] = sc * cb; r[1][1] = sc * sb * sa + cc * ca; r[1][2] = sc * sb * ca - cc * sa;
    r[2][0] = -sb;     r[2][1] = cb * sa;                r[2][2] = cb * ca;
}

// una caja al azar como la arma el motor: matriz de mundo R1 * S * R2 (un padre rotado con escala
// no uniforme y un hijo rotado); columnas normalizadas = ejes, largo * tam / 2 = semiejes
static void HbCajaAzar(bool cizalla, float lado, Vector3* c, Vector3 u[3], Vector3* h) {
    float r1[3][3], r2[3][3], m[3][3];
    HbRotAzar(r1); HbRotAzar(r2);
    const float s[3] = { cizalla ? 0.3f + 2.7f * HbAzar() : 1.0f,
                         cizalla ? 0.3f + 2.7f * HbAzar() : 1.0f,
                         cizalla ? 0.3f + 2.7f * HbAzar() : 1.0f };
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float acc = 0.0f;
            for (int k = 0; k < 3; k++) acc += r1[i][k] * s[k] * r2[k][j];
            m[i][j] = acc;
        }
    float hh[3];
    for (int k = 0; k < 3; k++) {
        Vector3 col(m[0][k], m[1][k], m[2][k]);
        const float len = col.Length();
        u[k] = col * (1.0f / len);
        hh[k] = 0.5f * (0.2f + 1.3f * HbAzar()) * len;
    }
    *h = Vector3(hh[0], hh[1], hh[2]);
    *c = Vector3((HbAzar() - 0.5f) * lado, (HbAzar() - 0.5f) * lado, (HbAzar() - 0.5f) * lado);
}

static bool CmdHitboxSat(std::istringstream& ss, std::string& err) {
    int n = 0; ss >> n;
    if (n < 1) { err = "hitboxsat: uso: hitboxsat <n> [semilla S]"; return false; }
    unsigned semilla = 777u;
    std::string clave;
    while (ss >> clave) {
        if (clave == "semilla") ss >> semilla;
        else { err = "hitboxsat: clave desconocida '" + clave + "'"; return false; }
    }
    gHbSemilla = semilla;
    int tocan = 0, noTocan = 0, dudosos = 0, falsosNeg = 0, falsosPos = 0, conCizalla = 0;
    for (int i = 0; i < n; i++) {
        // un tercio cajas de verdad contra cajas, el resto con al menos un paralelepipedo
        const bool cA = (i % 3) != 0, cB = (i % 3) == 2;
        Vector3 ca, ua[3], ha, cb, ub[3], hb;
        HbCajaAzar(cA, 3.0f, &ca, ua, &ha);
        HbCajaAzar(cB, 3.0f, &cb, ub, &hb);
        if (cA || cB) conCizalla++;
        const bool sat = W3dObbSolapan(ca, ua, ha, cb, ub, hb);
        // la referencia con las cajas un poco MAS CHICAS (si tocan, seguro se solapan) y un poco
        // MAS GRANDES (si no tocan, seguro no): lo que queda entre las dos es un roce, no se juzga
        const bool seguroSi = HbReferenciaTocan(HbParDe(ca, ua, ha, 0.999), HbParDe(cb, ub, hb, 0.999));
        const bool quizas = HbReferenciaTocan(HbParDe(ca, ua, ha, 1.001), HbParDe(cb, ub, hb, 1.001));
        if (seguroSi) { tocan++; if (!sat) falsosNeg++; }
        else if (!quizas) { noTocan++; if (sat) falsosPos++; }
        else dudosos++;
    }
    printf("      [hitboxsat] %d pares (%d con cizalla): se tocan %d, no %d, roce %d | falsos negativos %d, "
           "falsos positivos %d\n", n, conCizalla, tocan, noTocan, dudosos, falsosNeg, falsosPos);
    if (falsosNeg || falsosPos) { err = "hitboxsat: el SAT no coincide con la referencia exacta"; return false; }
    if (tocan < n / 10 || noTocan < n / 10) { err = "hitboxsat: la muestra no tiene casos de los dos lados"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  hitboxvista: lo que el viewport 3D VE y CLICKEA de un hitbox
// ---------------------------------------------------------------------------
static bool CmdHitboxVista(std::istringstream& ss, std::string& err) {
    std::string nombre; ss >> nombre;
    Object* o = HbBuscar(nombre);
    if (!o || o->getType() != ObjectType::hitbox) { err = "hitboxvista: '" + nombre + "' no es un hitbox"; return false; }
    W3dHitboxBase* hb = (W3dHitboxBase*)o;
    int ovl = -1, tipo = -1, queda = 0, espDibuja = -1, espPick = -1, espVer = -1;
    std::string clave;
    while (ss >> clave) {
        int v = 0;
        if (!(ss >> v)) { err = "hitboxvista: falta el valor de '" + clave + "'"; return false; }
        if (clave == "overlays") ovl = v;
        else if (clave == "tipo") tipo = v;
        else if (clave == "queda") queda = v;
        else if (clave == "dibuja") espDibuja = v;
        else if (clave == "pick") espPick = v;
        else if (clave == "ver") espVer = v;
        else { err = "hitboxvista: clave desconocida '" + clave + "'"; return false; }
    }
    namespace gfx = w3dEngine;
    extern Viewport3D* Viewport3DActive;
    extern int W3dPantallaAlto;
    if (!rootViewport) { err = "hitboxvista: no hay layout de viewports"; return false; }
    DeseleccionarTodo(); ObjActivo = NULL;   // seleccionado cambia el color del alambre
    { extern void CargarTexturasPendientes(); CargarTexturasPendientes(); }
    rootViewport->Render();                   // puebla Viewport3DActive
    Viewport3D* vp = Viewport3DActive;
    if (!vp) { err = "hitboxvista: no hay viewport 3D"; return false; }
    const bool ovl0 = vp->showOverlays, tipo0 = vp->showHitbox;
    if (ovl >= 0) vp->showOverlays = (ovl != 0);
    if (tipo >= 0) vp->showHitbox = (tipo != 0);
    rootViewport->Render();                   // publica los flags (w3dRenderOverlays, g_showHitbox)
    vp->Render();                             // el render REAL de la vista
    gfx::Finish();
    // el alambre: pixeles del VERDE del hitbox activo (0.25, 0.90, 0.35)
    const int vpGLY = W3dPantallaAlto - vp->y - vp->height;
    std::vector<unsigned char> buf((size_t)vp->width * vp->height * 4);
    gfx::ReadPixelsRGBA(vp->x, vpGLY, vp->width, vp->height, &buf[0]);
    int verdes = 0;
    for (size_t i = 0; i + 3 < buf.size(); i += 4)
        if (abs((int)buf[i] - 64) <= 10 && abs((int)buf[i + 1] - 229) <= 10 && abs((int)buf[i + 2] - 89) <= 10) verdes++;
    const bool dibuja = verdes >= 20;
    // el click: en el MEDIO de una arista de la caja (la de arriba-adelante, a lo largo de X)
    Matrix4 M; hb->GetWorldMatrix(M);
    const Vector3 medio = M * Vector3(hb->centro[0], hb->centro[1] + 0.5f * fabsf(hb->tam[1]),
                                      hb->centro[2] + 0.5f * fabsf(hb->tam[2]));
    vp->BindVista();
    float sx = 0, sy = 0;
    bool pick = false;
    const bool enVista = vp->ProyectarPunto(medio, sx, sy) && sx >= 0 && sy >= 0 &&
                         sx < (float)vp->width && sy < (float)vp->height;
    if (enVista) {
        ScenePick3D(vp->x + (int)sx, vp->y + (int)sy, vp->x, vp->y, vp->width, vp->height, W3dPantallaAlto);
        pick = hb->select;
        DeseleccionarTodo(); ObjActivo = NULL;
    }
    if (!queda) { vp->showOverlays = ovl0; vp->showHitbox = tipo0; }
    rootViewport->Render();                   // deja los flags publicados como quedo el viewport
    printf("      [hitboxvista] '%s' overlays=%d tipo=%d | pixeles del alambre=%d (%s) | click en (%d,%d) %s | "
           "verHitboxes=%d\n", hb->name.c_str(), (ovl >= 0 ? ovl : (int)ovl0), (tipo >= 0 ? tipo : (int)tipo0),
           verdes, dibuja ? "SE VE" : "no se ve", (int)sx, (int)sy,
           !enVista ? "FUERA DE LA VISTA" : (pick ? "LO ELIGE" : "no lo elige"), g_w3dHitboxVer ? 1 : 0);
    char b[200];
    if (espDibuja >= 0 && (dibuja ? 1 : 0) != espDibuja) {
        snprintf(b, sizeof(b), "hitboxvista: dibuja = %d (%d pixeles), se esperaba %d", dibuja ? 1 : 0, verdes, espDibuja);
        err = b; return false;
    }
    if (espPick >= 0 && !enVista) { err = "hitboxvista: la arista queda fuera de la vista"; return false; }
    if (espPick >= 0 && (pick ? 1 : 0) != espPick) {
        snprintf(b, sizeof(b), "hitboxvista: pick = %d, se esperaba %d", pick ? 1 : 0, espPick);
        err = b; return false;
    }
    if (espVer >= 0 && (g_w3dHitboxVer ? 1 : 0) != espVer) {
        snprintf(b, sizeof(b), "hitboxvista: verHitboxes = %d, se esperaba %d", g_w3dHitboxVer ? 1 : 0, espVer);
        err = b; return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  ordenhijo <nombre> <indice>: reordena entre hermanos (como arrastrar en el outliner)
// ---------------------------------------------------------------------------
static bool CmdOrdenHijo(std::istringstream& ss, std::string& err) {
    std::string n; int idx = -1;
    ss >> n >> idx;
    Object* o = HbBuscar(n);
    if (!o || idx < 0) { err = "ordenhijo: uso: ordenhijo <nombre> <indice>"; return false; }
    Object* padre = o->Parent ? o->Parent : SceneCollection;
    std::vector<Object*>& h = padre->Childrens;
    size_t i = 0;
    while (i < h.size() && h[i] != o) i++;
    if (i == h.size()) { err = "ordenhijo: '" + n + "' no esta entre los hijos de su padre"; return false; }
    if ((size_t)idx >= h.size()) { err = "ordenhijo: indice fuera de rango"; return false; }
    h.erase(h.begin() + i);
    h.insert(h.begin() + idx, o);
    printf("      [ordenhijo] '%s' pasa a la posicion %d de '%s' (serial %u)\n", o->name.c_str(), idx,
           padre->name.c_str(), o->serial);
    return true;
}

// ---------------------------------------------------------------------------
//  juegohitboxmin / juegohitboxlog: lo mismo en el Play y en el juego compilado
// ---------------------------------------------------------------------------
// el contador generico: alEntrar/alQuedarse/alSalir (y alTocar) en compartido("<prefijo>_...")
static const char* kLuaContador =
    "-- contador de eventos de hitbox / contactos (prueba del motor)\n"
    "propiedades = { prefijo = \"x\" }\n"
    "local p = \"x\"\n"
    "local function suma(c) local k = p .. \"_\" .. c; setCompartido(k, (compartido(k) or 0) + 1) end\n"
    "local function nom(o) if o then return nombre(o) end return \"nil\" end\n"
    "function inicio()\n"
    "  p = propiedad(\"prefijo\")\n"
    "  setCompartido(p .. \"_entrar\", 0); setCompartido(p .. \"_quedarse\", 0)\n"
    "  setCompartido(p .. \"_salir\", 0);  setCompartido(p .. \"_tocar\", 0)\n"
    "end\n"
    "function alEntrar(otro, mio, suyo)\n"
    "  suma(\"entrar\")\n"
    "  setCompartido(p .. \"_otro\", nom(otro)); setCompartido(p .. \"_mio\", nom(mio)); setCompartido(p .. \"_suyo\", nom(suyo))\n"
    "end\n"
    "function alQuedarse(otro, mio, suyo) suma(\"quedarse\") end\n"
    "function alSalir(otro, mio, suyo) suma(\"salir\"); setCompartido(p .. \"_salio\", nom(otro)) end\n"
    "function alTocar(otro, impulso, x, y, z, nx, ny, nz)\n"
    "  suma(\"tocar\"); setCompartido(p .. \"_tocado\", nom(otro)); setCompartido(p .. \"_ny\", ny)\n"
    "end\n";
// un objeto que avanza en X un paso FIJO por tick (determinista aunque el dt varie)
static const char* kLuaMover =
    "-- avanza en X un paso fijo por tick (x = x0 + paso * n)\n"
    "propiedades = { x0 = 0, paso = 0.4, y = 0 }\n"
    "local n = 0\n"
    "function actualizar(dt)\n"
    "  n = n + 1\n"
    "  setPosicion(yo(), propiedad(\"x0\") + propiedad(\"paso\") * n, propiedad(\"y\"), 0)\n"
    "end\n";
// la caja que cae: los dinamicos arrancan DORMIDOS, se la despierta con una velocidad
static const char* kLuaDespertar =
    "-- despierta el cuerpo rigido (los dinamicos arrancan dormidos)\n"
    "function inicio() fisicaVel(yo(), 0, -0.5, 0) end\n";
// arranca la animacion de escena (one-shot)
static const char* kLuaAnim =
    "function inicio() animEscena(\"Subir\", false) end\n";
// EL JUEZ: mira todo y deja el veredicto (compartido + log); en el juego pide salir(). Los
// contadores tienen que dar EXACTO (un alTocar por lado, como en el Play) y, una vez que se
// cumple todo, se sigue mirando 1 s MAS (mucho mas que la gracia de 0,15 s de los contactos): un
// segundo golpe falso (dt variable del juego compilado) o cualquier otro evento de mas es FALTA.
static const char* kLuaJuez =
    "-- veredicto de la prueba de hitbox del juego compilado\n"
    "local t, seg = 0, 0\n"
    "local listo = false\n"
    "local okDesde = nil\n"
    "local function c(k) return compartido(k) or -1 end\n"
    "function actualizar(dt)\n"
    "  if listo then return end\n"
    "  t = t + 1; seg = seg + dt\n"
    "  local _, ay = posicion(buscar(\"Ascensor\"))\n"
    "  local ok = c(\"puerta_entrar\") == 1 and c(\"puerta_quedarse\") == 7 and c(\"puerta_salir\") == 1\n"
    "     and c(\"zona_entrar\") == 1 and c(\"zona_salir\") == 1 and c(\"caja_entrar\") == 1 and c(\"caja_salir\") == 1\n"
    "     and c(\"piso_tocar\") == 1 and c(\"caja_tocar\") == 1 and ay > 4.9\n"
    "  if ok and not okDesde then okDesde = seg end\n"
    "  local fin = (okDesde and (not ok or seg - okDesde >= 1.0)) or seg > 20 or t > 100000\n"
    "  if fin then\n"
    "    ok = ok and okDesde ~= nil\n"
    "    listo = true\n"
    "    local linea = string.format(\"[juegohitbox] %s puerta=%d/%d/%d zona=%d/%d caja=%d/%d tocar=%d/%d ascensor=%.2f ticks=%d\",\n"
    "      ok and \"OK\" or \"FALTA\", c(\"puerta_entrar\"), c(\"puerta_quedarse\"), c(\"puerta_salir\"),\n"
    "      c(\"zona_entrar\"), c(\"zona_salir\"), c(\"caja_entrar\"), c(\"caja_salir\"), c(\"piso_tocar\"), c(\"caja_tocar\"), ay, t)\n"
    "    info(linea)\n"
    "    setCompartido(\"juez\", ok and \"OK\" or \"FALTA\")\n"
    "    salir()\n"
    "  end\n"
    "end\n";

static void HbScript(Object* o, const char* ruta, const char* prop, const char* valor) {
    if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
    W3dScriptEntrada e; e.ruta = ruta;
    if (prop) e.refs.push_back(std::make_pair(std::string(prop), std::string(valor)));
    o->scriptDatos->scripts.push_back(e);
}
static void HbRef(Object* o, const char* prop, const char* valor) {
    o->scriptDatos->scripts.back().refs.push_back(std::make_pair(std::string(prop), std::string(valor)));
}
static Empty* HbEmpty(const char* nombre, const Vector3& pos) {
    Empty* e = new Empty(NULL, pos);
    e->SetNameObj(nombre);
    return e;
}

static bool CmdJuegoHitboxMin(std::istringstream& ss, std::string& err) {
    std::string dir; ss >> dir;
    if (dir.empty()) { err = "juegohitboxmin: uso: juegohitboxmin <carpeta>"; return false; }
    std::string e2;
    if (!W3dRunCommand("juego3dmin " + dir, e2)) { err = "juegohitboxmin: " + e2; return false; }
    if (!HbEscribir(dir + "/hb_contador.lua", kLuaContador) || !HbEscribir(dir + "/hb_mover.lua", kLuaMover) ||
        !HbEscribir(dir + "/hb_despertar.lua", kLuaDespertar) || !HbEscribir(dir + "/hb_anim.lua", kLuaAnim) ||
        !HbEscribir(dir + "/hb_juez.lua", kLuaJuez)) {
        err = "juegohitboxmin: no pude escribir los .lua en " + dir; return false;
    }
    // --- PUERTA: un hitbox hijo de la puerta y un objeto que la atraviesa a paso fijo ---
    Empty* puerta = HbEmpty("Puerta", Vector3(0, 10, 0));
    HbScript(puerta, "hb_contador.lua", "prefijo", "puerta");
    Hitbox* zp = new Hitbox(puerta, Vector3(0, 0, 0));
    zp->SetNameObj("ZonaPuerta");
    zp->tam[0] = zp->tam[1] = zp->tam[2] = 2.0f;
    zp->filtro = "jugador";
    Empty* movil = HbEmpty("Movil", Vector3(-5.05f, 10, 0));
    HbScript(movil, "hb_mover.lua", "x0", "-5.05"); HbRef(movil, "y", "10");
    Hitbox* cm = new Hitbox(movil, Vector3(0, 0, 0));
    cm->SetNameObj("CajaMovil");
    cm->etiqueta = "jugador";
    // --- ZONA que detecta un CUERPO RIGIDO que cae y aterriza en un piso estatico ---
    Empty* piso = HbEmpty("Piso", Vector3(20, -0.5f, 0));
    piso->fisica = new W3dRigidoDef();
    piso->fisica->tipo = 0; piso->fisica->caja[0] = 10; piso->fisica->caja[1] = 1; piso->fisica->caja[2] = 10;
    HbScript(piso, "hb_contador.lua", "prefijo", "piso");
    Empty* caja = HbEmpty("Caja", Vector3(20, 4, 0));
    caja->fisica = new W3dRigidoDef();
    HbScript(caja, "hb_contador.lua", "prefijo", "caja");
    HbScript(caja, "hb_despertar.lua", NULL, NULL);
    Hitbox* zona = new Hitbox(NULL, Vector3(20, 2.5f, 0));
    zona->SetNameObj("Zona");
    zona->tam[0] = 3; zona->tam[1] = 1; zona->tam[2] = 3;
    zona->detectarCuerpos = true;
    HbScript(zona, "hb_contador.lua", "prefijo", "zona");
    // --- ANIMACION DE ESCENA pedida por lua (animEscena): el Ascensor sube de 0 a 5 ---
    Empty* asc = HbEmpty("Ascensor", Vector3(-20, 0, 0));
    HbScript(asc, "hb_anim.lua", NULL, NULL);
    InitSceneAnimations();
    const int activa = SceneAnimActiva;
    const int idx = NuevaEscena();
    RenombrarEscenaActiva("Subir");
    SceneAnimations[idx]->startFrame = 1; SceneAnimations[idx]->endFrame = 30; SceneAnimations[idx]->fps = 30;
    { AnimationObject ao; ao.obj = asc;
      AnimProperty& py = PropertyDeLista(ao.Propertys, AnimPosition, AnimY);
      SetKeyCurva(py, 1, 0.0f); SetKeyCurva(py, 30, 5.0f);
      ao.UpdateFirstLastFrame();
      AnimationObjects.push_back(ao); }
    SetEscenaActiva(activa);
    // --- el JUEZ ---
    Empty* juez = HbEmpty("Juez", Vector3(0, -10, 0));
    HbScript(juez, "hb_juez.lua", NULL, NULL);
    DeseleccionarTodo(); ObjActivo = NULL;
    printf("      [juegohitboxmin] juego3dmin + puerta/zona/caja/piso/ascensor/juez en '%s'\n", dir.c_str());
    return true;
}

static bool CmdJuegoHitboxLog(std::istringstream& ss, std::string& err) {
    std::string dir, nombre; ss >> dir >> nombre;
    if (dir.empty() || nombre.empty()) { err = "juegohitboxlog: uso: juegohitboxlog <carpeta> <nombre>"; return false; }
    const std::string carpeta = dir + "/build/linux";
    const std::string bin = carpeta + "/" + nombre;
    if (!w3dFileSystem::FileExists(bin)) { err = "juegohitboxlog: no existe el binario compilado '" + bin + "'"; return false; }
    const std::string log = carpeta + "/whisk3d.log";
    remove(log.c_str());
    char cmdRun[2200];
    // el juez pide salir() al terminar; el timeout es la red de seguridad
    snprintf(cmdRun, sizeof(cmdRun), "cd \"%s\" && timeout 120 ./%s > /dev/null 2>&1", carpeta.c_str(), nombre.c_str());
    const int r = system(cmdRun);
    FILE* f = fopen(log.c_str(), "rb");
    if (!f) { err = "juegohitboxlog: el juego no dejo whisk3d.log (se compilo en modo debug?)"; return false; }
    std::string linea, veredicto;
    char buf[2048];
    while (fgets(buf, sizeof(buf), f)) {
        const char* p = strstr(buf, "[juegohitbox]");
        if (p) { veredicto = p; }
    }
    fclose(f);
    while (!veredicto.empty() && (veredicto[veredicto.size() - 1] == '\n' || veredicto[veredicto.size() - 1] == '\r'))
        veredicto.erase(veredicto.size() - 1);
    printf("      [juegohitboxlog] salida=%d | %s\n", r, veredicto.empty() ? "(sin veredicto del juez)" : veredicto.c_str());
    if (veredicto.empty()) { err = "juegohitboxlog: el juez no escribio su veredicto (los scripts no corrieron?)"; return false; }
    // el juez ya exige los valores exactos; ademas se leen de su linea (un juez viejo o roto que
    // diga OK con otros numeros no pasa)
    if (veredicto.find("[juegohitbox] OK") == std::string::npos ||
        veredicto.find("puerta=1/7/1") == std::string::npos || veredicto.find("tocar=1/1") == std::string::npos) {
        err = "juegohitboxlog: en el juego compilado los eventos/rigidos/animacion NO dan lo mismo que en el Play";
        return false;
    }
    return true;
}

// ============================================================================
//  el despachador
// ============================================================================
bool W3dPruebasHitboxCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "hitboxnuevo")    return CmdHitboxNuevo(ss, err);
    if (cmd == "hitboxinfo")     return CmdHitboxInfo(ss, err);
    if (cmd == "hitboxstats")    return CmdHitboxStats(ss, err);
    if (cmd == "hitboxadd")      return CmdHitboxAdd(ss, err);
    if (cmd == "hitboxdup")      return CmdHitboxDup(ss, err);
    if (cmd == "hitboxprops")    return CmdHitboxProps(ss, err);
    if (cmd == "compartidonum")  return CmdCompartidoNum(ss, err);
    if (cmd == "hitboxgrilla")   return CmdHitboxGrilla(ss, err);
    if (cmd == "hitboxsat")      return CmdHitboxSat(ss, err);
    if (cmd == "hitboxvista")    return CmdHitboxVista(ss, err);
    if (cmd == "ordenhijo")      return CmdOrdenHijo(ss, err);
    if (cmd == "posobjeto")      return CmdPosObjeto(ss, err);
    if (cmd == "nombreactivo")   return CmdNombreActivo(ss, err);
    if (cmd == "hitboxpaso")     return CmdHitboxPaso(ss, err);
    if (cmd == "hitboxlimpiar")  { W3dHitboxLimpiar(); return true; }
    if (cmd == "juegohitboxmin") return CmdJuegoHitboxMin(ss, err);
    if (cmd == "juegohitboxlog") return CmdJuegoHitboxLog(ss, err);
    manejado = false;
    return false;
}
