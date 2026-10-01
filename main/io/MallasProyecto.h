#ifndef W3D_MALLAS_PROYECTO_H
#define W3D_MALLAS_PROYECTO_H
// ============================================================================
//  MallasProyecto — el lado del EDITOR de las mallas 3D como recurso
//  (libs/Whisk3DCore/objects/MallaRecurso.h tiene el modelo y el contrato).
//
//  Lo que el Core no puede hacer solo porque necesita al editor:
//   - SERIALIZAR una malla editada (triangulacion + Forsyth del index buffer
//     canonico, reposo de las vertex anims) y PUBLICARLA en su recurso: editar
//     la malla de un objeto edita el recurso, y todos sus usuarios la ven.
//   - "Hacer unica", elegir otro recurso, Alt+D (duplicado vinculado) y Shift+D
//     (copia con recurso propio), con sus pasos de undo.
//   - El TICK del editor: materializa la edicion de la malla ACTIVA (Properties,
//     editor UV), la suelta cuando deja de serlo y publica la malla que sale de
//     Edit Mode / weight / vertex paint.
//   - El GUARDADO: agrupa por contenido (dedup) las mallas sueltas que vinieron de
//     un archivo viejo, decide que recurso
//     conserva su nombre, escribe cada entrada UNA vez (.w3db, o .w3dm con el
//     formato de texto; copia la entrada vieja si no cambio) y el registro
//     "mallas" del proyecto.json. Ver formato/mallas-registro.md.
//  Motor generico: aca no hay nombres de ningun juego.
// ============================================================================
#include <string>
#include <vector>

class Mesh;
class Material;
class W3dContenedorEscritor;
struct MallaRecurso;

// la malla como .w3db (en reposo, con el index buffer canonico), con 'nombre' en INFO.
// false = no se puede (no cargo / requiere un bloque desconocido / no entra en 16 bits /
// su edicion no se pudo leer); 'avisos' dice por que.
bool W3dMallaSerializar(Mesh* m, const std::string& nombre, std::string& out,
                        std::vector<std::string>* avisos = 0);

// PUBLICA la geometria de 'm' en su recurso: si cambio, el recurso toma estos datos y
// TODOS sus usuarios se re-apuntan (y los que tienen modificadores se regeneran). La malla
// que publica queda con su edicion (y, fuera de edicion, vuelve a compartir los arrays).
// true = el recurso cambio.
bool W3dMallaPublicar(Mesh* m);

// un recurso NUEVO (en memoria) con la geometria actual de 'm', vinculado a 'm' (conserva su
// edicion). 'nombre' = el que se pide (se uniquifica). NULL si la malla no se pudo serializar.
MallaRecurso* W3dMallaCrearRecurso(Mesh* m, const std::string& nombre);
// idem para una malla SUELTA desde la UI (New Copy / Rename / Folder de la tarjeta, Alt+D, el
// selector), con 'carpeta' (se normaliza) y un paso de UNDO: Ctrl+Z la vuelve suelta (con lo
// que tenia) y Ctrl+Y le devuelve el recurso. Si ya tiene recurso devuelve ese (sin paso).
MallaRecurso* W3dMallaCrearRecursoConUndo(Mesh* m, const std::string& nombre, const std::string& carpeta);

// HACER UNICA (single user): 'm' pasa a un recurso NUEVO, copia del suyo, con nombre libre
// ("Arbol" -> "Arbol.001"). Si ya era el unico usuario no hace nada. Una malla suelta pasa a
// tener su propio recurso. Con undo (Ctrl+Z la devuelve al recurso anterior).
MallaRecurso* W3dMallaHacerUnica(Mesh* m);

// "New Copy" del selector: 'm' pasa a un recurso NUEVO copia del suyo aunque sea el unico
// usuario (con undo). Una malla suelta pasa a tener su propio recurso.
MallaRecurso* W3dMallaNuevaCopia(Mesh* m);
// el selector de la tarjeta "Malla 3D": 'm' pasa a usar 'r' (con undo). false = no se pudo.
bool W3dMallaAsignar(Mesh* m, MallaRecurso* r);

// Alt+D: una malla NUEVA que usa el MISMO recurso (misma pos/rot/escala, materiales,
// modificadores, scripts...). Una malla suelta primero pasa a tener su recurso.
Mesh* W3dMallaDuplicarVinculado(Mesh* src);
// Shift+D de una malla con recurso: la copia usa un recurso NUEVO (copia del de 'src', con
// nombre libre). Devuelve la copia; NULL si 'src' no tiene recurso (el camino de siempre).
Mesh* W3dMallaDuplicarConRecursoPropio(Mesh* src);

// TODA MALLA ES UN RECURSO (la biblioteca): las mallas de las raices vivas que no tienen recurso (una
// primitiva, un import, un Separate, un script, un archivo viejo) pasan a tener el suyo, con el nombre
// del objeto ("Cubo", el siguiente "Cubo.001"); las SUELTAS de un archivo viejo iguales comparten uno.
// Sin undo (es el estado natural de una malla). Barato si nadie nacio ni se desvinculo desde la ultima
// vez. Lo llaman el tick del editor, la carga y los listados. Devuelve cuantas vinculo.
int W3dMallasAsegurarRecursos();
// "Duplicate" de la biblioteca: una copia del recurso sin usuarios que se conserva (con undo)
MallaRecurso* W3dMallaDuplicarRecurso(MallaRecurso* r);

// "la malla es de un recurso y hay otros objetos que lo usan": las operaciones que son del
// OBJETO y no de la malla (Apply transform, Set Origin, Join) la hacen unica antes.
bool W3dMallaCompartida(const Mesh* m);

// EL TICK DEL EDITOR (lo llama ActualizarEditMeshActivo, una vez por frame y en cada cambio
// de modo): materializa la edicion de la malla ACTIVA (y la suelta cuando deja de serlo,
// publicando antes lo que tuviera sin publicar) y publica la malla que sale de edicion (Edit
// Mode, Weight Paint, Vertex Paint) o que una operacion edito afuera de ella.
void W3dMallasTickEditor();
// el proyecto se cierra (ReiniciarEscena): el tick suelta sus punteros a mallas del arbol que se
// destruye (la malla en edicion, la activa que se miraba)
void W3dMallasEditorOlvidar();
// al terminar de ABRIR un proyecto: las mallas que abrieron SUELTAS (sin recurso del registro:
// un archivo viejo) se pueden juntar por contenido al guardar (Mesh::dedupPorContenido)
void W3dMallasMarcarSueltasDelArchivo();
// publica YA lo pendiente, sin salir de Edit Mode (lo pide el guardado: guarda lo que se ve)
void W3dMallasSincronizar();

// ---------------------------------------------------------------------------
//  EL GUARDADO (GuardarW3D, los dos formatos: .w3db o .w3dm por malla)
// ---------------------------------------------------------------------------
// arma el plan: agrupa las mallas de la escena por contenido y mete en el contenedor UNA
// entrada .w3db por grupo (+ los huerfanos). false = alguna malla no se puede guardar (los
// mismos frenos de siempre: no cargo, requiere, edicion ilegible): ya se aviso.
bool W3dMallasGuardarPreparar(W3dContenedorEscritor* esc);
// el recurso con que se escribe el objeto ("malla": <nombre>); "" = la malla no entra al
// plan (va por el camino de siempre, "geometria" propia: mas de 65535 vertices con indices
// de 16 bits, o el guardado del esquema viejo del harness, g_w3dMallasLegado)
std::string W3dMallasGuardarNombreDe(const Mesh* m);
// el bloque "mallas": [...] del proyecto.json (con coma final si hay algo)
void W3dMallasGuardarRegistro(std::string& s);
// los materiales de los recursos HUERFANOS (nadie en la escena los registra al recorrerla)
void W3dMallasGuardarMateriales(std::vector<Material*>& out);
// el guardado salio: la memoria sigue al archivo (entradas, nombres, recursos nuevos,
// objetos re-vinculados). Consume el plan.
void W3dMallasGuardarConfirmar();
// el guardado no salio (o termino): se descarta el plan
void W3dMallasGuardarDescartar();

#endif
