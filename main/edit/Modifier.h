#ifndef MODIFIER_H
#define MODIFIER_H

#include <string>
#include <vector>
#include "objects/VisSet.h" // metodo "celdas" del Culling por triangulo (dato .w3dvis)

class Object; // el target del Mirror puede ser CUALQUIER objeto de la escena (su world matrix define el plano)

// ============================================================================
//  MODIFICADOR — concepto del EDITOR (Whisk3D), NO del core (Whisk3DCore).
//  Un modificador es una operacion que, sobre la malla ORIGINAL editable, GENERA una malla de RENDER nueva.
//  El ORDEN importa (se aplican en secuencia, uno sobre el resultado del anterior). Se EDITA la malla original;
//  en el RENDER se ve la malla generada. Whisk3DCore solo recibe la malla final lista para renderizar.
//  Por ahora SOLO el tipo + nombre (la UI y el stack). Los parametros y la generacion vienen despues.
//  El Mesh (core) guarda esto como Modifier* FORWARD-DECLARADO: el core no conoce esta clase ni la procesa.
// ============================================================================
struct ModifierType {
    // AL FINAL a proposito (los ids viejos no se mueven): CullingTri = culling de la malla
    // POR TRIANGULO, sector a sector, con datos PRECALCULADOS (PVS estilo PS1). Ver abajo.
    enum Enum { Screw = 0, Mirror, Array, SubdivisionSurface, Boolean, Armature, CullingTri };
};

// ============================================================================
//  IDENTIDAD ESTABLE DE UN MODIFICADOR (el mismo problema que Object::serial, Objects.h)
//
//  Un Modifier vive en el heap y el stack lo borra con delete: la direccion se RECICLA y
//  cualquier estado que se haya guardado "el modificador que esta en 0x1234" cae sobre
//  OTRO modificador, en silencio. Lo sufre el undo del borrado de objetos (RefEntry::mod
//  en main/undo/Undo.cpp): guarda el Modifier* cuyo target hay que restaurar, y con la
//  direccion reciclada escribia el target ENCIMA del modificador de otra malla (test
//  'modtargetuaf'). Por eso cada Modifier nace con un numero MONOTONO que no se reusa nunca.
//
//  El serial NO se copia: copiar un modificador (duplicar un objeto, Separate) da OTRO
//  modificador, y darle el mismo numero seria justo el reciclaje que esto evita. Va en un
//  tipo propio para que el copy-constructor IMPLICITO de Modifier -que es el que se usa
//  en ObjectMode.cpp- haga lo correcto solo: si fuera un unsigned int pelado habria que
//  acordarse de pisarlo en cada sitio que clona, y olvidarse no da ni un aviso.
// ============================================================================
struct W3dModSerial {
    unsigned int v;
    W3dModSerial();                        // v = el proximo del contador
    W3dModSerial(const W3dModSerial&);     // una COPIA es otro modificador -> numero NUEVO
    W3dModSerial& operator=(const W3dModSerial&) { return *this; } // idem: no se pisa
};

class Modifier {
    public:
        W3dModSerial serial; // identidad estable (ver arriba). 'serial.v' nunca es 0.
        int tipo;            // ModifierType::Enum
        std::string nombre;  // nombre visible (editable a futuro; arranca con el nombre del tipo)

        // --- VISIBILIDAD (todos los modificadores) ---
        bool mostrarViewport; // OFF = NUNCA se calcula (se saltea en el pipeline; el siguiente opera sobre el estado previo)
        bool mostrarEdit;     // OFF = no se calcula EN Edit Mode (edicion rapida N95; al salir de Edit se recalcula)

        // --- MIRROR (por ahora solo este modificador tiene params; cuando haya mas -> subclases) ---
        bool  ejeX, ejeY, ejeZ; // ejes a espejar (default X). El espejo es secuencial: X, luego Y sobre el
                                // resultado, luego Z (como Blender).
        Object* target;         // NULL = espejo desde el ORIGEN del objeto; si no, desde el origen del TARGET
                                // (su posicion + ROTACION en el mundo definen el plano; la escala no importa).
        bool  merge;            // "soldar" (visual) los verts que tocan el plano: los snapea al plano + alinea
                                // sus normales (media sobre el eje) -> una media esfera se ve esfera perfecta.
        float mergeDist;        // distancia del merge (default 0.001 m)
        bool  clipping;         // (edit-time) clampea los verts al plano al moverlos y, una vez pegados, los
                                // deja pegados a esa pared por el resto del transform (solo deslizan por el plano).
                                // ARRANCA EN ON (el flujo tipico de Mirror lo quiere activado).

        // --- SUBDIVISION SURFACE --- (level como float para bindear directo a PropFloat entero; se castea a int)
        float subLevel;         // niveles en el VIEWPORT (default 1)
        float subRenderLevel;   // niveles en el RENDER (default 2)
        bool  subSimple;        // false = Catmull-Clark (suaviza), true = Simple (subdivide sin mover verts)

        // --- SCREW --- barre el perfil (aristas) alrededor de un eje. steps como float (PropFloat entero).
        float screwAngle;       // grados que gira del primero al ultimo (default 360)
        float screwHeight;      // cuanto SUBE por el eje del primero al ultimo (el "screw"; default 0 = torno)
        float screwSteps;       // copias del perfil en el VIEWPORT (default 16)
        float screwRenderSteps; // copias en el RENDER (default 32)
        int   screwAxis;        // 0=X, 1=Y, 2=Z. Default Y: en este motor (Y arriba) un perfil dibujado en la vista
                                // frontal lathea vertical alrededor de Y (como una botella parada).
        bool  screwStretchU;    // genera U (a lo largo del giro) para textura cilindrica
        bool  screwStretchV;    // genera V (a lo largo del perfil)
        bool  screwSmooth;      // normales SUAVES (perfil redondito) en vez de facetado plano. Default ON (un lathe casi
                                // siempre se quiere suave).
        bool  screwMerge;       // suelda verts coincidentes (polos donde el perfil toca el eje + costura del torno 360)
                                // -> topologia limpia y sin costura. Default ON.
        bool  screwFlip;        // invierte el winding (normales para el otro lado) si la superficie quedo del reves.

        // --- ARMATURE: cache de vertex-animation (bakea el skinning por frame -> reproduccion sin recomputar) ---
        bool  cacheAnim;        // Cache Animation: guarda las poses deformadas en memoria (default OFF)
        float cacheSkip;        // Frame Skip: 0=todos los frames; N=guarda cada N+1 e interpola (menos memoria). float para PropFloat entero.

        // --- CULLING (por triangulo, "CullingTri"): PVS precalculado estilo PS1 ---
        // La malla se parte en SECTORES; cada sector lista QUE triangulos se dibujan desde
        // ahi. Los datos vienen PRECALCULADOS del juego original en un sidecar junto al
        // .obj de origen de la malla:
        //     <modelo>.pvs.json = { "sectores": M, "tris": [[t0, t1, ...], ...] }
        // con cada t = indice de TRIANGULO 0-based en el ORDEN DE CARAS del OBJ. El
        // modificador precalcula UN index buffer por sector (W3dPVSAplicarSector) y en
        // runtime se dibuja SOLO el del sector activo: cambiar de sector = swap del index
        // buffer (re-subida del IBO), NUNCA trabajo por frame. La malla original queda
        // INTACTA y editable (el modificador no toca vertex/faces; solo elige indices).
        // El lua del juego cambia el sector con setSector(obj, s) al pasar de nodo/camara.
        // metodoPVS 0 = "Triangulos (PVS)": sectores del sidecar `.pvs.json` (arriba).
        // metodoPVS 1 = "Celdas (VisSet)": el dato es un `.w3dvis` binario (formato/
        //   w3dvis.md, cargado en visSet): celdas de a miles, listas CON ORDEN y deltas
        //   entre celdas vecinas. Reemplaza al viejo "BSP" (que nunca corto nada); un
        //   .w3d viejo que declare "bsp" cae aca y, sin `.w3dvis` que cargar, sigue
        //   dibujando la malla completa (mismo comportamiento observable que el BSP).
        // En los DOS metodos sectorPVS es la celda/sector ACTIVO (1-based; 0 = completa)
        // y el resultado es el override Mesh::pvsFaces/pvsGroups/pvsVersion: ese es el
        // contrato con el render (el core dibuja, esto decide).
        int  metodoPVS;
        int  sectorPVS;    // sector/celda ACTIVO, 1-based. 0 o fuera de rango = malla completa (fallback).
        // FALLBACK DE CELDA VACIA (declarado en el archivo, "sectorFallback:"):
        // que se dibuja cuando la celda/sector ACTIVO existe pero su lista quedo
        // SIN TRIANGULOS. Sin declarar (0) una celda vacia dibuja NADA, que es lo
        // correcto cuando el dato dice "desde aca no se ve" (oclusion real). Pero
        // cuando la celda la elige OTRO criterio que el que horneo las listas
        // (p.ej. un LOD por distancia sobre listas horneadas por nodo: el hijo
        // elegido puede caer en una celda que para ESA variante esta vacia), una
        // celda vacia se volvia un POZO NEGRO: ni esta malla ni su pareja se
        // dibujaban. El archivo elige el paracaidas:
        //   sectorFallback: "completa"  (-1) -> celda vacia = malla COMPLETA
        //   sectorFallback: N           (>0) -> celda vacia = la lista de la celda N
        //   (ausente / 0)                    -> celda vacia = nada (como siempre)
        int  sectorFallback;
        // NOMBRE REAL del sidecar, tal como quedo en el contenedor (o en disco). Se
        // guarda en el .w3d y se INGIERE al empaquetar: derivarlo de <origen>.pvs.json
        // funcionaba solo con el pipeline del proyecto desplegado en disco. Al mandar
        // un .w3d v4 a otra maquina el .obj entra como "modelos/x.obj" y el sidecar no
        // entraba a ningun lado -> el PVS desaparecia SIN AVISO. Vacio = derivar del
        // origen, como antes (compatibilidad con los .w3d ya guardados).
        std::string pvsArchivo;
        // NOMBRE REAL del `.w3dvis` (metodo "celdas"), mismo contrato que pvsArchivo:
        // vacio = derivar de <origen sin extension>.w3dvis / extra/<base>.w3dvis.
        std::string visArchivo;
        // -- runtime (NO se serializa) --
        std::vector< std::vector<int> > pvsSectores; // tris por sector, del sidecar
        VisSet visSet;     // metodo "celdas": el `.w3dvis` cargado (por VALOR: copiar el
                           // modificador copia el dato y no hay doble-free con el copy-ctor
                           // implicito que usa la duplicacion de objetos)
        std::vector<unsigned> visLista; // la lista de la celda APLICADA, en el orden del archivo
        bool pvsCargado;   // ya se intento leer el sidecar (evita re-lecturas por frame)
        int  pvsAplicado;  // sector cuyo index buffer esta armado en la malla (-1 = ninguno)
        int  pvsFacesBase; // facesSize con el que se armo (si la malla cambio, se rearma y revalida)

        Modifier(int t, const std::string& n)
            : tipo(t), nombre(n),
              mostrarViewport(true), mostrarEdit(true),
              ejeX(true), ejeY(false), ejeZ(false), target(NULL),
              merge(true), mergeDist(0.001f), clipping(true),
              subLevel(1.0f), subRenderLevel(2.0f), subSimple(false),
              screwAngle(360.0f), screwHeight(0.0f), screwSteps(16.0f), screwRenderSteps(32.0f),
              screwAxis(1), screwStretchU(true), screwStretchV(true),
              screwSmooth(true), screwMerge(true), screwFlip(false),
              cacheAnim(false), cacheSkip(0.0f),
              metodoPVS(0), sectorPVS(0), sectorFallback(0),
              pvsCargado(false), pvsAplicado(-1), pvsFacesBase(0) {}
        virtual ~Modifier() {}
};

// nombre por defecto de cada tipo (para la lista + el menu "Add")
inline const char* NombreTipoModificador(int t) {
    switch (t) {
        case ModifierType::Screw:              return "Screw";
        case ModifierType::Mirror:             return "Mirror";
        case ModifierType::Array:              return "Array";
        case ModifierType::SubdivisionSurface: return "Subdivision Surface";
        case ModifierType::Boolean:            return "Boolean";
        case ModifierType::Armature:           return "Armature";
        case ModifierType::CullingTri:         return "Culling";
    }
    return "Modifier";
}

#endif // MODIFIER_H
