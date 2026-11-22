#ifndef EDITMESH_H
#define EDITMESH_H

#ifdef _WIN32
    #define NOMINMAX
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#include <string>
#include <set>
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif
#include "crossplatform.h" // MeshIndex (16 bits N95 / 32 bits escritorio)

class Mesh;

// LA SELECCION EXACTA de la malla de edicion, por POSICION: vertices, aristas, caras (las tres) y el activo. Sobrevive a
// un GenerateRender (que rearma la malla de edicion y re-numera todo) cuando la op NO mueve vertices: marcar afilado,
// costuras, shade smooth / flat, recalcular / dar vuelta normales. (Restaurar desde los VERTICES -RebuildEditSelByPos-
// sumaba aristas y caras que no estaban elegidas: dos aristas de una cara traian la tercera.)
struct EditSelSnap {
    std::set<std::vector<int> > v, e, f;   // las claves (posiciones redondeadas) de lo elegido
    std::vector<int> active;               // la clave del activo (del modo de seleccion de ese momento)
    int activeMode;
    bool has;
    EditSelSnap() : activeMode(0), has(false) {}
};

// ===================================================
//  EditMesh: la malla de EDICION (separada de la malla de render de Whisk3DCore).
// ===================================================
// Tiene UN vertice por POSICION unica (no le importan los duplicados: no hay
// normales, ni UV, ni iluminacion, ni texturas). Guarda la seleccion (por vertice
// y por arista) y buffers PRECALCULADOS con vertex color para renderizar rapido en
// GL ES 1.1: 1 sola pasada, glDrawArrays (glDrawElements GL_LINES no anda en el
// N95). Se recolorea SOLO al (de)seleccionar, nunca por frame.
class EditMesh {
    public:
        Mesh* src; // la malla de render de la que se construyo (lee su geometria)

        std::vector<int> editVerts;          // pos[k] <-> src->vertex[editVerts[k]] (GPU)
        std::vector<unsigned char> vertSel;  // 1 = vertice editable k seleccionado
        std::vector<unsigned char> edgeSel;  // 1 = arista e seleccionada (modo edge)

        std::vector<unsigned char> faceSel;  // 1 = la cara f esta seleccionada (modo face)
        std::vector<GLfloat> pos;            // posiciones (xyz) de los vertices editables
        std::vector<GLubyte> col;            // color RGBA por vertice (puntos)
        std::vector<GLushort> lineIdx;       // pares de indices (en pos) de cada arista
        // lineas EXPANDIDAS (2 vertices por arista) para glDrawArrays(GL_LINES)
        std::vector<GLfloat> linePos;        // posiciones expandidas
        std::vector<GLubyte> lineCol;        // color expandido (vertex color o por-arista)
        // CARAS (1 por face3d): poligono de indices editables + sus aristas + centro
        std::vector<std::vector<int> > faces;     // faces[f] = indices editables del poligono
        std::vector<int> faceSrc;                 // faceSrc[f] = indice en src->faces3d (NO es 1:1: hay caras degeneradas que se saltean). Lo usa el borrado.
        std::vector<std::vector<int> > faceEdges; // faceEdges[f] = indices de arista del poligono
        std::vector<GLfloat> faceCenter;          // centro xyz de cada cara (los puntitos)
        std::vector<GLubyte> facePtCol;           // color del puntito por cara (negro/verde/blanco)
        std::vector<unsigned char> closedFace;    // (telefono) la cara es de un pedazo CERRADO de la malla: si mira para
                                                  // atras, la tapa la propia malla (se calcula al dibujar, una vez)
        std::vector<MeshIndex> faceTriDyn;         // triangulos de las caras SELECCIONADAS (fill)
        int activeIdx;                            // ultimo sub-elemento seleccionado (activo) o -1
        // skinGeomVersion de src con el que se lleno pos[]. Si src->skinGeomVersion difiere,
        // algo movio vertex[] POR FUERA de la edicion (tipico: una vertex anim parada en un
        // frame del timeline -> EvalVertexAnim deforma vertex[] y bumpea la version). Entonces
        // el cage quedo en la pose vieja y hay que re-derivarlo con SyncPos. Editar
        // verts (PushPositions) NO bumpea la version -> la edicion en curso no se re-pisa.
        unsigned syncedGeomVersion;
        // (las capas de datos -UV maps / vertex colors / vertex groups- viven PERSISTENTES
        //  en el Mesh, no aca: ver Mesh::uvMaps/colorLayers/vertexGroups + Mesh::GenerateRender)

        EditMesh();

        void Build(Mesh* m);  // arma todo desde la malla de render (1 vez)
        // empuja las posiciones EDITABLES (pos[], autoritativas durante la edicion) al
        // render IN-PLACE: cada vert GPU toma la pos del editVert de su grupo (posRep).
        // Sin realloc ni cambio de topologia. Lo usa Mesh::RefreshRender (mover verts).
        void PushPositions();
        void RefreshOverlay();  // rearma lineas + centros de cara desde pos[] (sin leer render)
        void Recolor();         // recolorea todo segun EditSelectMode + seleccion + activo
        // re-lee las posiciones desde src->vertex y rearma linePos + faceCenter (sin
        // tocar la topologia ni la seleccion). Lo usa el transform de malla en vivo.
        void SyncPos();
        void Render();            // dibuja caras (semitransp) + lineas + vertices/puntos-cara

        // seleccion del sub-elemento ACTIVO (EditSelectMode: vertex/edge/face)
        void SelectAll(bool sel);
        void SaveSelection(EditSelSnap& s) const;      // (ver EditSelSnap)
        void RestoreSelection(const EditSelSnap& s);   // + Recolor
        // al CAMBIAR el modo de seleccion (vertex/edge/face) la seleccion PASA al modo nuevo, como en Blender: las caras
        // dan sus aristas / sus vertices, las aristas sus puntas, y hacia arriba quedan las aristas / caras que tienen
        // TODO adentro (un quad elegido -> sus 4 aristas -> sus 4 vertices, y de vuelta el mismo quad)
        void ConvertSelection(int fromMode, int toMode);
        void Invert();
        void ToggleVert(int k, bool onlyThis); // pick de vertice (onlyThis=sin shift)
        void ToggleEdge(int e, bool onlyThis); // pick de arista
        void ToggleFace(int f, bool onlyThis); // pick de cara
        // L: Select Linked — selecciona la isla conexa que contiene al elemento k (en
        // 'modo' SelVertex/Edge/Face). onlyThis reemplaza; si no, agrega.
        void SelectLinked(int k, int mode, bool onlyThis);
        // Shift+Alt+Click (modo cara): Loop Select de caras desde el borde edgeK
        // (propaga por quads, perpendicular al borde). onlyThis reemplaza; si no, agrega.
        void SelectLoopFace(int edgeK, bool onlyThis);
        // Modo BORDE — dos tipos de loop (como Blender):
        //  EDGE LOOP: el anillo que SIGUE la linea del borde (el circulo de un cilindro);
        //    cruza cada vertice regular (4 bordes) al borde que NO comparte cara.
        //  EDGE RING: los travesanos PERPENDICULARES (camina por quads como el loop de caras,
        //    pero junta los bordes opuestos). onlyThis reemplaza; si no, agrega.
        void SelectLoopEdge(int edgeK, bool onlyThis);
        void SelectLoopEdgeVerts(int edgeK, bool onlyThis); // modo VERTICE: verts del edge loop (sin acumular edgeSel)
        void SelectRingEdge(int edgeK, bool onlyThis);
        // Pick Shortest Path (Ctrl+Click): camino mas corto desde el ACTIVO hasta toIdx, en el
        // modo activo (vert/edge/face). fillRegion=false -> un solo caminito; true -> rellena TODA
        // la region (todos los elementos en ALGUN camino minimo: distA+distB==total; en un grid =
        // el rectangulo). AGREGA a la seleccion (no reemplaza); toIdx queda activo.
        void SelectShortestPath(int toIdx, bool fillRegion);
        // centro LOCAL de la seleccion del sub-elemento activo (para enfocar con ".").
        // false si no hay nada seleccionado.
        bool SelectionCenter(float& cx, float& cy, float& cz) const;
        // ademas del centro LOCAL, el radio LOCAL (distancia mas lejana al centro) de la seleccion,
        // para que el foco '.' ajuste el zoom a lo seleccionado. false si no hay nada seleccionado.
        bool SelectionCenterRadius(float& cx, float& cy, float& cz, float& rLocal) const;

        int NumVerts() const { return (int)editVerts.size(); }
        int NumEdges() const { return (int)(lineIdx.size() / 2); }
        int NumFaces() const { return (int)faces.size(); }
};

#endif // EDITMESH_H
