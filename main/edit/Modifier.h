#ifndef MODIFIER_H
#define MODIFIER_H

#include <string>

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
    enum Enum { Screw = 0, Mirror, Array, SubdivisionSurface, Boolean, Armature };
};

class Modifier {
    public:
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

        Modifier(int t, const std::string& n)
            : tipo(t), nombre(n),
              mostrarViewport(true), mostrarEdit(true),
              ejeX(true), ejeY(false), ejeZ(false), target(NULL),
              merge(true), mergeDist(0.001f), clipping(true),
              subLevel(1.0f), subRenderLevel(2.0f), subSimple(false),
              screwAngle(360.0f), screwHeight(0.0f), screwSteps(16.0f), screwRenderSteps(32.0f),
              screwAxis(1), screwStretchU(true), screwStretchV(true),
              screwSmooth(true), screwMerge(true), screwFlip(false),
              cacheAnim(false), cacheSkip(0.0f) {}
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
    }
    return "Modifier";
}

#endif // MODIFIER_H
