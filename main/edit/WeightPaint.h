#ifndef WEIGHTPAINT_H
#define WEIGHTPAINT_H

#include <string>
#include <vector> // mascara de caras seleccionadas (PincelAplicar)

class Mesh;

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

// ---- PINCEL compartido (radio / fuerza / modo). C++03: struct simple. ----
struct BrushEstado {
    float radioPx;  // radio del circulo en px de pantalla (default 40)
    float fuerza;   // 0..1 (default 1): cuanto peso aplica una pasada en el centro
    int   modo;     // 0 = sumar, 1 = restar
    BrushEstado() : radioPx(40.0f), fuerza(1.0f), modo(0) {}
};
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

// una pasada del pincel: para cada control-point toca su peso segun la distancia del
// vert (proyectado) al centro del circulo. Falloff SMOOTHSTEP del centro al borde:
//   s = 1 - d/radio ; f = s*s*(3 - 2*s)   (f=1 en el centro, 0 en el borde)
// delta = fuerza01 * f (sumar o restar), con clamp 0..1. Los render-verts que
// comparten control-point (splits por costura) NO acumulan doble: se usa el falloff
// MAXIMO entre ellos. Devuelve true si algun peso cambio (y ahi invalida el CSR de
// skinning para que la pose refleje los pesos nuevos).
// Con WeightPaintSoloSel() ON solo pinta control-points de CARAS SELECCIONADAS en edit
// mode; 'soloCaras' (opcional) es la seleccion por cara LOGICA (faces3d) que aporta el
// caller (el UV editor fuera de sync pasa la suya) - NULL = derivarla de la edit mesh
// (faceSel via faceSrc, la seleccion de edit mode). Sin el toggle, 'soloCaras' se ignora.
bool PincelAplicar(Mesh* m, int grupo, float centroX, float centroY, float radioPx,
                   float fuerza01, bool sumar, WPProyector proy, void* ctx,
                   const std::vector<char>* soloCaras = NULL);

// PINCEL DEL EDITOR UV: la MISMA pasada, pero la unidad es el RENDER-VERT (corner) y escribe
// en el UV GROUP 'uvGrupo'. No colapsa a control-point (los 4 corners de UNA cara se pesan sin
// tocar las caras vecinas que comparten esos vertices 3D) y NO toca los vertex groups, asi que
// el skinning 3D y el export GLB no se enteran. Entrada DEDICADA a proposito: la funcion del
// editor UV es por corner y punto (no hay flag ni modo que decidir).
bool PincelAplicarUV(Mesh* m, int uvGrupo, float centroX, float centroY, float radioPx,
                     float fuerza01, bool sumar, WPProyector proy, void* ctx,
                     const std::vector<char>* soloCaras = NULL);

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
void WeightPaintMenuTam(int sx, int syTop);            // slider del radio (px)
void WeightPaintMenuFuerza(int sx, int syTop);         // slider de la fuerza (0..1)
// UN desplegable POR ENTIDAD (no se mezclan ni hay items que bakeen de una a la otra):
void WeightPaintMenuGrupo(Mesh* m, int sx, int syTop);   // 3D: vertex groups + Add Vertex Group
void WeightPaintMenuUVGroup(Mesh* m, int sx, int syTop); // UV: uv groups + Add UV Group + Clear

// labels de los 4 botones de la toolbar ("40px", "100%", "+"/"-", nombre del grupo activo).
// La variante UV es identica salvo el ultimo (el nombre sale del UV group activo).
void WeightPaintLabels(Mesh* m, std::string& tam, std::string& fuerza,
                       std::string& modo, std::string& grupo);
void WeightPaintLabelsUV(Mesh* m, std::string& tam, std::string& fuerza,
                         std::string& modo, std::string& grupo);

#endif // WEIGHTPAINT_H
