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
    enum Enum { Screw = 0, Mirror, Array, SubdivisionSurface, Boolean };
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

        Modifier(int t, const std::string& n)
            : tipo(t), nombre(n),
              mostrarViewport(true), mostrarEdit(true),
              ejeX(true), ejeY(false), ejeZ(false), target(NULL),
              merge(true), mergeDist(0.001f), clipping(true) {}
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
    }
    return "Modifier";
}

#endif // MODIFIER_H
