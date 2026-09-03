#ifndef WEIGHTPAINT_H
#define WEIGHTPAINT_H

#include <string>
#include <vector> // mascara de caras seleccionadas (PincelAplicar)
#include "edit/W3dFalloff.h" // la curva de caida del pincel (y de cualquier herramienta con radio)

class Mesh;
class Object;

// ============================================================================
//  WEIGHT PAINT (Fase 2): pincel reutilizable + escritura de pesos.
//  - El PINCEL (BrushEstado) es estado GLOBAL separado de los pesos: a futuro lo
//    reusan pintura de texturas / escultura (mismo circulo, misma toolbar).
//  - Los PESOS se escriben por CONTROL-POINT (VertexGroup::verts, sparse), el
//    mismo indexado que usan el skinning (SkinearMesh) y ConstruirColorPeso.
//    El mapeo render-vert -> control-point es Mesh::vertCtrlPoint; en mallas del
//    editor lo puebla GenerarRender (identidad por posicion) o, si todavia esta
//    vacio, WeightPaintAsegurarMapa (lazy).
//  - PincelAplicar es AGNOSTICO del viewport: recibe un PROYECTOR (callback que
//    da la posicion en PANTALLA de cada render-vert) -> el MISMO codigo pinta en
//    el viewport 3D (ProyectarPunto) y en el editor UV (UVtoScreen).
// ============================================================================

// ---- MODO del pincel: que le hace al peso que el vertice YA tiene. ----
// Es un ENUM y no el 'bool sumar' de antes a proposito: al aparecer el tercer modo, un bool
// en cada llamada habria seguido compilando con el significado cambiado (true -> 1 -> restar).
// Con el enum el compilador OBLIGA a revisar cada llamador (bool no convierte a enum).
enum WPModo {
    WPSumar   = 0,  // "+": w = w + valor * falloff
    WPRestar  = 1,  // "-": w = w - valor * falloff
    WPIgualar = 2   // "=": w = valor EXACTO, sin falloff (ver PincelAplicar)
};

// ---- MARCAS de pintura: los cuadraditos que muestran QUE se puede pintar. ----
// Con las marcas prendidas el pincel deja de trabajar "sobre la malla" y pasa a trabajar
// sobre ESOS puntos: pinta los que caen adentro del circulo, al valor de la barra y SIN
// falloff (o lo tocaste o no), y tocar cualquier otro lado no hace nada.
// Los dos modos dibujan el MISMO cuadrado; lo que cambia es DONDE cae:
enum W3dMarcasModo {
    MarcasOff     = 0,
    MarcasVertice = 1,  // uno por CONTROL-POINT, EXACTAMENTE sobre el vertice. Es el de los PESOS:
                        // el skinning lee un peso por vertice, asi que el punto pintable es el vertice.
    MarcasCorner  = 2   // uno por FACE CORNER (render-vert), metido hacia el centro de SU cara para
                        // que los 3 corners que comparten una esquina se vean y se puedan tocar por
                        // separado. Es el que necesita el VERTEX COLOR (que sí es por corner).
};

// ---- PINCEL compartido (radio / fuerza / modo). C++03: struct simple. ----
struct BrushEstado {
    float radioPx;  // radio del circulo en px de pantalla (default 40)
    float fuerza;   // 0..1 (default 1). En +/- es CUANTO peso aplica una pasada en el centro;
                    // en "=" es el VALOR al que queda el peso. La barra de abajo lo muestra
                    // como "valor: 100%" justamente porque en "=" no es una fuerza, es el destino.
    int   modo;     // un WPModo (se guarda int: es estado de UI, el boton lo CICLA +/-/=)
    int   marcas;   // un W3dMarcasModo: OFF = pincel clasico; el boton de la barra prende el que
                    // corresponda al contexto (pesos -> VERTICE; vertex color -> CORNER).
    // ---- VERTEX COLOR: con que color pinta (el "valor" de la barra sigue siendo la INTENSIDAD) ----
    float color[4]; // color libre RGBA 0..1. Lo edita el ColorPicker de siempre (el cuadradito
                    // de color de la barra lo abre), asi que las pestanias RGB/HSV/Hex ya andan.
    int   palIdx;   // -1 = color libre; >= 0 = INDICE de la paleta efectiva del objeto. Lo setea
                    // la pestania "Pal" del mismo ColorPicker -> elegir paleta o "no usar paleta"
                    // sale gratis, y lo pintado con indice sigue a la paleta (palette-swap).
    W3dFalloff falloff; // como cae la intensidad del centro al borde (el menu Falloff de la barra)
    BrushEstado() : radioPx(40.0f), fuerza(1.0f), modo(WPSumar), marcas(MarcasOff), palIdx(-1) {
        color[0] = 0.8f; color[1] = 0.15f; color[2] = 0.15f; color[3] = 1.0f; // un rojo, para que se vea
    }
};

// El falloff que el pincel USA REALMENTE en este momento. Con las marcas prendidas es
// CONSTANTE (1 en todo el radio): el punto se toca o no se toca, no hay medias tintas.
// Sale de aca -- y no de cada llamador -- para que el 3D y el editor UV no puedan
// divergir en esa regla.
const W3dFalloff& BrushFalloffEfectivo();
BrushEstado& BrushGet(); // el estado global del pincel (unico)

// dibuja el CIRCULO del pincel en pantalla: 48 segmentos de linea, blanco con halo
// oscuro (se ve sobre cualquier fondo, como el cursor del UV). Asume proyeccion 2D
// LOCAL del viewport ya seteada (Ortho 0..w, 0..h, y hacia abajo). Solo dibuja.
void BrushDibujarCirculo(float cx, float cy, float radioPx);

// ---- API de escritura de PESOS (por CONTROL-POINT) ----
// peso del control-point 'cp' en el grupo 'grupo' (0 si no tiene entrada / rango invalido)
float PesoDe(Mesh* m, int grupo, int cp);
// asigna el peso: clamp a 0..1; crea la entrada sparse si falta; con w <= 0 BORRA la
// entrada (no acumula basura de peso cero en el grupo)
void  PesoAsignar(Mesh* m, int grupo, int cp, float w);

// ---- API de PESOS del UV GROUP (por RENDER-VERT / CORNER; ver UVGroup en Mesh.h) ----
// Los escribe SOLO el pincel del editor UV y los leen SOLO el skinning 2D (Armature2DAplicar)
// + el relleno de color del UV. Asi se pesan los 4 corners de UNA cara del cubo sin tocar las
// otras caras que comparten esos vertices 3D. Mismas reglas que PesoDe/PesoAsignar (clamp 0..1,
// w <= 0 borra la entrada sparse). Son OTRA entidad: no se bakean desde los vertex groups.
float PesoUVDe(Mesh* m, int uvGrupo, int rv);
void  PesoUVAsignar(Mesh* m, int uvGrupo, int rv, float w);
// deja el UV group SIN pesos (la lista queda, el binding por nombre con el hueso 2D sobrevive)
void  UVGroupLimpiarPesos(Mesh* m, int uvGrupo);

// PROYECTOR: escribe en (sx,sy) la posicion en PANTALLA (px, coords LOCALES del
// viewport) del render-vert i. Devuelve false si el vert NO se pinta (detras de la
// camara / back-facing). 'ctx' es el contexto del caller (viewport + malla).
typedef bool (*WPProyector)(void* ctx, int i, float& sx, float& sy);

// dibuja las MARCAS (los cuadraditos): un cuadrado relleno con el color del punto y un
// BORDE NEGRO alrededor, en cada punto pintable. 'modo' es un W3dMarcasModo; 'colorRV' es
// el color POR RENDER-VERT con el que rellenarlos (weightPaintColor al pintar pesos, el
// vertex color cuando sea eso; NULL = gris neutro). Igual que BrushDibujarCirculo: asume la
// proyeccion 2D LOCAL del viewport ya seteada y SOLO dibuja -- de donde salen las posiciones
// en pantalla lo decide el PROYECTOR, asi el mismo codigo sirve en el viewport 3D y en el UV.
void BrushDibujarMarcas(Mesh* m, int modo, float ladoPx, const unsigned char* colorRV,
                        WPProyector proy, void* ctx);

// OSCURECE las caras que el pincel NO puede tocar. Con la mascara "solo lo seleccionado"
// prendida, pintar afuera de la seleccion no hace NADA y eso no se veia: la malla se veia
// igual en todos lados y el pincel simplemente no respondia. Esto les tira un velo negro al
// 40%, asi la zona pintable se lee de un vistazo.
// Va por el MISMO proyector que el pincel (2D de pantalla, como las marcas) y no por el hook
// de render: en modo pintura la malla se dibuja por un camino propio del Core que retorna
// ANTES de los overlays. Ademas, yendo por el proyector, lo que se oscurece es exactamente lo
// que el pincel descarta -- misma cuenta, no pueden divergir.
// 'soloCaras' = la seleccion por cara que aporte el caller (NULL = derivarla del edit mesh).
void BrushDibujarCarasBloqueadas(Mesh* m, WPProyector proy, void* ctx,
                                 const std::vector<char>* soloCaras = NULL);

// ============================================================================
//  PINCEL DE VERTEX COLOR: la MISMA pasada que la de pesos (mismo circulo, mismo
//  falloff, misma mascara "solo lo seleccionado", mismas marcas) pero lo que escribe
//  es COLOR en la capa 'capa' de la malla (Mesh::colorLayers).
//
//  La unidad es el CORNER, porque asi guarda la capa (SIEMPRE por corner; 'porVertice'
//  es un toggle no-destructivo que COLAPSA al hornear, ver AplicarCapasAlRender). Con
//  la capa en porVertice el pincel pinta TODOS los corners de la posicion tocada, que
//  es lo que hace que un vertice se vea de un color solo: "un vertice puede tener
//  varios face corner".
//
//  CUANTO entra: a = valor01 * falloff(d/radio), y el color se MEZCLA con el que habia
//  (a=1 lo reemplaza). Con las marcas prendidas el falloff es constante -> el color
//  entra al valor de la barra, parejo en todo el circulo.
//
//  POR INDICE DE PALETA (capa porIndice): un indice NO se puede mezclar -- o el corner
//  es del color 3 de la paleta o no lo es. Entonces el falloff decide QUIEN se pinta
//  (a >= 0.5) y el que se pinta toma el indice ENTERO y su color exacto. 'palIdx' < 0
//  pinta color libre y borra el indice de esos corners.
//
//  Devuelve true si algo cambio (y ahi el caller re-hornea con AplicarCapasAlRender).
// ============================================================================
bool PincelAplicarColor(Mesh* m, int capa, float centroX, float centroY, float radioPx,
                        float valor01, const unsigned char* rgba, int palIdx,
                        WPProyector proy, void* ctx,
                        const std::vector<char>* soloCaras = NULL,
                        const W3dFalloff* falloff = NULL);

// FUSIONA por vertice: cada grupo de corners que comparte posicion queda con el PROMEDIO de
// sus colores. Es lo que hace de verdad el paso Per-Corner -> Per-Vertex: antes el flag solo
// cambiaba como se HORNEA (se mostraba el color del primer corner y los demas seguian guardados
// intactos), asi que no se perdia nada... pero tampoco se mezclaba, que es lo que uno espera al
// unificar. Ahora los promedia y los escribe: por eso el cambio pide confirmacion.
// Devuelve true si algun color cambio.
bool VertexColorFusionarPorVertice(Mesh* m, int capa);

// re-hornea los colores de la capa 'capa' desde sus INDICES contra la paleta EFECTIVA de
// 'dueno' (W3dPaletaEfectiva). Es LO QUE HACE el palette-swap: cambiar la paleta de un
// padre y que todos sus herederos se re-pinten. No hace nada si la capa no es porIndice.
bool VertexColorResolverIndices(Mesh* m, int capa, Object* dueno);



// una pasada del pincel: para cada control-point toca su peso segun la distancia del
// vert (proyectado) al centro del circulo. La caida la decide el FALLOFF ('falloff'):
//   f = falloff->Eval(d / radio)          (f=1 en el centro, 0 en el borde)
// falloff = NULL usa SMOOTH, que es la formula que estaba clavada antes de que el
// falloff fuera elegible -- asi ningun llamador viejo cambia de comportamiento.
// delta = fuerza01 * f (sumar o restar), con clamp 0..1. Los render-verts que
// comparten control-point (splits por costura) NO acumulan doble: se usa el falloff
// MAXIMO entre ellos. Devuelve true si algun peso cambio (y ahi invalida el CSR de
// skinning para que la pose refleje los pesos nuevos).
// MODO WPIgualar ("="): el peso queda EXACTAMENTE en fuerza01 -- el falloff decide QUE
// vertices toca el pincel (los de adentro del circulo), no CUANTO les toca. Todo el
// circulo queda en el mismo valor: pintar al 32% deja esos vertices en 32%, se hayan
// pintado antes o no. Por eso "=" es el unico modo que acepta valor 0 (borra a cero
// exacto); en +/- un delta de 0 no hace nada y se corta antes.
// Con WeightPaintSoloSel() ON solo pinta control-points de CARAS SELECCIONADAS en edit
// mode; 'soloCaras' (opcional) es la seleccion por cara LOGICA (faces3d) que aporta el
// caller (el UV editor fuera de sync pasa la suya) - NULL = derivarla de la edit mesh
// (faceSel via faceSrc, la seleccion de edit mode). Sin el toggle, 'soloCaras' se ignora.
bool PincelAplicar(Mesh* m, int grupo, float centroX, float centroY, float radioPx,
                   float fuerza01, WPModo modo, WPProyector proy, void* ctx,
                   const std::vector<char>* soloCaras = NULL,
                   const W3dFalloff* falloff = NULL);

// PINCEL DEL EDITOR UV: la MISMA pasada, pero la unidad es el RENDER-VERT (corner) y escribe
// en el UV GROUP 'uvGrupo'. No colapsa a control-point (los 4 corners de UNA cara se pesan sin
// tocar las caras vecinas que comparten esos vertices 3D) y NO toca los vertex groups, asi que
// el skinning 3D y el export GLB no se enteran. Entrada DEDICADA a proposito: la funcion del
// editor UV es por corner y punto (no hay flag ni modo que decidir).
bool PincelAplicarUV(Mesh* m, int uvGrupo, float centroX, float centroY, float radioPx,
                     float fuerza01, WPModo modo, WPProyector proy, void* ctx,
                     const std::vector<char>* soloCaras = NULL,
                     const W3dFalloff* falloff = NULL);

// ---- ASSIGN / REMOVE / SELECT / DESELECT de grupos (tarjetas Vertex Groups / UV Groups) ----
// El camino "seleccionar y asignar" que faltaba: sin esto un grupo SOLO se podia armar pintando.
// Viven aca (y no en Properties.cpp) porque son operaciones de PESOS, del mismo palo que el
// pincel, y las comparten el panel y el harness de tests.
//   asignar=true  -> peso 1.0 a lo seleccionado; asignar=false -> BORRA su entrada del grupo.
// Un solo paso de undo (UndoPesosIniciar/Confirmar). Devuelven cuantos elementos tocaron.
//
// LAS DOS ENTIDADES SON DISTINTAS (ver VertexGroup/UVGroup en Mesh.h) y cada una opera SU
// seleccion, sin bakear nada de la otra:
//   VertexGroup* -> CONTROL-POINTS, seleccion de EDIT MODE del viewport 3D.
//   UVGroup*     -> RENDER-VERTS (corners), seleccion EFECTIVA del editor UV
//                   (UVVertsSelEfectivos: la propia del UV si la hay, si no la del 3D expandida).
int VertexGroupAsignarSel(Mesh* m, bool asignar);
int UVGroupAsignarSel(Mesh* m, bool asignar);
// SELECT / DESELECT: marca (o desmarca) lo que el grupo activo pesa (peso > 0).
// El vertex group escribe la seleccion de EDIT MODE; el UV group escribe Mesh::uvSelVert.
int VertexGroupSeleccionar(Mesh* m, bool sel);
int UVGroupSeleccionar(Mesh* m, bool sel);

// ---- "EDITAR SOLO LO SELECCIONADO" (mascara de seleccion para la pintura) ----
// Toggle GLOBAL compartido por la toolbar del viewport 3D (modo Weight Paint) y la del UV
// editor (modo pintura), rol TBR_SoloSel (UVEditor.h). ON: el pincel SOLO pinta vertices
// que pertenecen a caras seleccionadas en Edit Mode (la seleccion se hace ahi y PERSISTE
// al cambiar a Weight Paint); sin caras seleccionadas no pinta nada. OFF (default): pinta
// todo (comportamiento historico).
bool& WeightPaintSoloSel();

// puebla Mesh::vertCtrlPoint IDENTIDAD-POR-POSICION (posRep) si esta VACIO (malla del
// editor que todavia no paso por GenerarRender). No pisa un mapeo existente (importadas).
void WeightPaintAsegurarMapa(Mesh* m);

// ---- TOPE POR TRAZO: lo que hace que la fuerza y el falloff se entiendan ----
// Cada movimiento del mouse dispara UNA PASADA del pincel. Pasando despacio son decenas de
// pasadas sobre el mismo vertice, asi que con fuerza 100% todo saturaba al toque y el falloff
// no se notaba: el borde del circulo terminaba igual que el centro. La regla es la de cualquier
// programa de pintura: DENTRO DE UN TRAZO, un elemento no puede recibir mas de lo que le da UNA
// pasada (valor * falloff). Volver a pasar por el mismo lugar no suma; para poner mas, se
// suelta y se pinta otro trazo. Asi el falloff se ve y la fuerza significa algo.
// Lo llaman los tres arranques de trazo (pesos 3D, pesos UV y color).
void BrushTrazoResetear();

// ---- TRAZO (undo): UN paso de undo por trazo, no por movimiento de mouse ----
// al mouse-down: snapshot de los DOS grupos (UndoPesosIniciar guarda vertexGroups + uvGroups) y,
// si la malla no tiene NINGUN grupo de la entidad que toca, crea uno automaticamente (aviso en el
// log) y lo deja activo. Devuelven el indice del grupo destino, o -1 si no se puede pintar.
int  WeightPaintTrazoIniciar(Mesh* m);    // VIEWPORT 3D: vertex groups (grupoActivo); crea "Group"
// EDITOR UV: uv groups (uvGrupoActivo). El grupo automatico toma el nombre del HUESO 2D activo
// si la malla tiene armature 2D (asi el binding por nombre queda hecho); sino "UV Group".
int  WeightPaintTrazoIniciarUV(Mesh* m);
// al soltar: commitea el trazo al stack de undo (descarta si no cambio nada)
void WeightPaintTrazoFin();
bool WeightPaintTrazoActivo();

// ---- MENUS del pincel para la toolbar (compartidos 3D / UV editor) ----
// abren un desplegable que crece hacia ARRIBA desde la toolbar (sx = x del boton,
// syTop = borde superior de la barra), como el menu Orient de la toolbar del 3D.
// (los desplegables de RADIO y FUERZA se fueron: ahora esos dos valores viven en la FILA DE
//  BARRAS deslizables arriba de la toolbar -- BrushBar*, ToolbarBase.cpp -- que se arrastra
//  directo sin abrir nada. Dejarlos habria sido dos caminos para el mismo valor.)
// UN desplegable POR ENTIDAD (no se mezclan ni hay items que bakeen de una a la otra):
void WeightPaintMenuGrupo(Mesh* m, int sx, int syTop);   // 3D: vertex groups + Add Vertex Group
// VERTEX PAINT: las CAPAS DE COLOR de la malla (a cual se pinta) + crear una nueva. Es el
// equivalente del desplegable de vertex groups: la unidad que elegis antes de pintar.
void WeightPaintMenuCapaColor(Mesh* m, int sx, int syTop);
void WeightPaintMenuUVGroup(Mesh* m, int sx, int syTop); // UV: uv groups + Add UV Group + Clear

// labels de los botones de la toolbar ("+"/"-"/"=" y el nombre del grupo activo). 'tam' y
// 'fuerza' salen formateados igual que antes ("40px" / "100%") pero ya no los pinta un boton:
// los usa la FILA DE BARRAS (BrushBar*) para su texto "radio: 40px" / "valor: 100%".
// La variante UV es identica salvo el ultimo (el nombre sale del UV group activo).
void WeightPaintLabels(Mesh* m, std::string& tam, std::string& fuerza,
                       std::string& modo, std::string& grupo);
void WeightPaintLabelsUV(Mesh* m, std::string& tam, std::string& fuerza,
                         std::string& modo, std::string& grupo);

#endif // WEIGHTPAINT_H
