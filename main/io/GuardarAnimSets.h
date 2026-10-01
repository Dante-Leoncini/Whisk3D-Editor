#ifndef GUARDAR_ANIMSETS_H
#define GUARDAR_ANIMSETS_H

// ============================================================================
//  EL GUARDADO DE LOS ANIMSETS (los clips de esqueleto en .w3da, ver
//  animation/W3dAnimSet.h). Lo llama GuardarW3D en cuatro puntos:
//
//    1. GuardarAnimSetsPreparar, ANTES de recorrer la escena: arma el plan
//       (que animset le toca a cada armature y que clips usa de el),
//       DEDUPLICA POR CLIP y agrega las entradas animaciones/<slug>.w3da.
//    2. GuardarAnimSetsDe, desde el nodo de cada armature: "anims" + "clips".
//    3. GuardarAnimSetsRegistro: el bloque raiz "animsets".
//    4. GuardarAnimSetsConfirmar, SOLO si el archivo quedo escrito: el registro
//       en memoria pasa a ser el guardado y los recursos cargados se re-nombran
//       a sus entradas nuevas (o se desenganchan si la entrada cambio).
//
//  EL PLAN (determinista: re-guardar sin cambios da los mismos bytes):
//    - un animset es la biblioteca de clips de UN esqueleto (la lista de nombres
//      de sus huesos). Los armatures con el mismo esqueleto comparten biblioteca:
//      diez enemigos del mismo rig = una entrada con la UNION de sus clips, y
//      cada clip que se repite (el mismo "caminar" en los diez) va UNA vez.
//    - un armature que vino de un animset y no se toco sigue en ese animset (y
//      con su nombre); uno que se edito (copy-on-write) vuelve al suyo si el
//      esqueleto es el mismo: lo que no cambio cae en los mismos clips.
//    - el orden de los clips de cada biblioteca es el de su primera aparicion
//      recorriendo la escena en preorden.
//    - un animset del registro que ya no usa nadie (HUERFANO) se conserva tal
//      cual (sus bytes del contenedor anterior), con su nombre y su carpeta.
//
//  FRENO: un armature cuyo animset no se pudo leer al abrir (animSetNoCargo)
//  frena el guardado entero (guardar encima lo dejaria sin animaciones).
// ============================================================================
#include <string>
#include <vector>

class Object;
class Armature;
class W3dContenedorEscritor;

// 1. false = el guardado se frena (ya avisado en pantalla y en el log)
bool GuardarAnimSetsPreparar(W3dContenedorEscritor* esc, Object* raiz);   // raiz NULL = todas las del proyecto
// 2. false = el armature no tiene clips en el plan (no se escribe "anims"; o van INLINE)
bool GuardarAnimSetsDe(const Armature* a, std::string& animset, std::vector<int>& clips);
// 2b. la biblioteca de CLIPS DE JERARQUIA de un objeto raiz ("clipsJerarquia" de su nodo; ver
//     animation/W3dAnimSet.h). false = no tiene. Las bibliotecas de jerarquia van al mismo registro
//     "animsets" y a entradas .w3da como las de esqueleto (bloques JCLP), cada una ENTERA desde su
//     recurso en memoria; dos iguales quedan en una (sus usuarios pasan a la primera al confirmar).
bool GuardarAnimSetsJerDe(const Object* o, std::string& animset);
// 3. el bloque raiz (con su coma y salto de linea), o nada si no hay animsets
void GuardarAnimSetsRegistro(std::string& s);
// 4. el archivo quedo escrito
void GuardarAnimSetsConfirmar();

// HARNESS: forzar el formato INLINE de antes (los clips adentro de proyecto.json), para
// tener archivos "viejos" con los que probar la lectura y la migracion. El editor nunca.
extern bool g_w3dAnimsInline;

// lo que hizo el ultimo GuardarAnimSetsPreparar (para el harness y el log)
struct GuardarAnimSetsInfo {
    int animsets;        // bibliotecas escritas (sin los huerfanos)
    int huerfanos;       // animsets del registro conservados sin usuarios
    int armatures;       // armatures con clips
    int clipsArmatures;  // clips de todos los armatures (con repetidos)
    int clipsEscritos;   // clips distintos escritos (despues del dedup)
    long bytes;          // bytes de todas las entradas .w3da escritas
    int raicesJer;       // objetos raiz con biblioteca de clips de jerarquia
    int clipsJer;        // clips de jerarquia escritos (despues del dedup de bibliotecas)
    GuardarAnimSetsInfo() : animsets(0), huerfanos(0), armatures(0), clipsArmatures(0), clipsEscritos(0), bytes(0),
                            raicesJer(0), clipsJer(0) {}
};
const GuardarAnimSetsInfo& GuardarAnimSetsUltimo();

// ============================================================================
//  LOS ANIMSETS COMO RECURSOS DEL PROYECTO: lo que necesita el outliner por
//  recursos (vista Animaciones) para listarlos, renombrarlos, ordenarlos en
//  carpetas y purgar los huerfanos. Operan sobre el REGISTRO (W3dAnimSetsRegistro,
//  animation/W3dAnimSet.h) y los armatures de la escena 'raiz'; el guardado
//  siguiente escribe lo que quede (nombres, carpetas, entradas).
//
//  USUARIOS de un animset = los armatures con clips que lo usan, con el mismo criterio
//  que el plan del guardado: los colgados de su recurso cuyo esqueleto calza, y los que
//  ya pasaron a clips propios (copy-on-write) o siguen colgados con un esqueleto que ya
//  no calza pero traen su nombre en animSetNombre (el guardado los pone ahi y se lo
//  anota). Sin usuarios = HUERFANO.
// ============================================================================
// cuantos usuarios tiene el animset 'nombre' (y cuales, si 'out' no es NULL)
// (tambien son usuarios los objetos raiz que la usan como biblioteca de CLIPS DE JERARQUIA: van en 'raicesJer')
int  W3dAnimSetUsuarios(Object* raiz, const std::string& nombre, std::vector<Armature*>* out,
                        std::vector<Object*>* raicesJer = 0);   // raiz NULL = todas
// renombra el animset: la fila del registro, el recurso cargado y el animSetNombre de sus
// usuarios. El nombre se normaliza y se hace unico entre los del registro (regla .NNN);
// 'final' recibe el que quedo. La entrada del contenedor sigue siendo la misma hasta el
// proximo guardado (que la deriva del nombre nuevo). false = no hay un animset 'viejo'.
bool W3dAnimSetRenombrar(Object* raiz, const std::string& viejo, const std::string& nuevo, std::string* final);
// la carpeta (COSMETICA, la del outliner) del animset 'nombre'. false = no esta en el registro.
bool W3dAnimSetFijarCarpeta(const std::string& nombre, const std::string& carpeta);
// "Purgar huerfanos": saca del registro los animsets sin usuarios (el proximo guardado ya no
// los escribe). Devuelve cuantos saco; 'nombres' (opcional) recibe cuales.
int  W3dAnimSetsPurgarHuerfanos(Object* raiz, std::vector<std::string>* nombres);

#endif // GUARDAR_ANIMSETS_H
