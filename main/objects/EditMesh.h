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
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif
#include "crossplatform.h" // MeshIndex (16 bits N95 / 32 bits escritorio)

class Mesh;

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
        std::vector<MeshIndex> faceTriDyn;         // triangulos de las caras SELECCIONADAS (fill)
        int activeIdx;                            // ultimo sub-elemento seleccionado (activo) o -1
        // (las capas de datos -UV maps / vertex colors / vertex groups- viven PERSISTENTES
        //  en el Mesh, no aca: ver Mesh::uvMaps/colorLayers/vertexGroups + Mesh::GenerarRender)

        EditMesh();

        void Construir(Mesh* m);  // arma todo desde la malla de render (1 vez)
        // empuja las posiciones EDITABLES (pos[], autoritativas durante la edicion) al
        // render IN-PLACE: cada vert GPU toma la pos del editVert de su grupo (posRep).
        // Sin realloc ni cambio de topologia. Lo usa Mesh::RefrescarRender (mover verts).
        void EmpujarPosiciones();
        void RefrescarOverlay();  // rearma lineas + centros de cara desde pos[] (sin leer render)
        void Recolorear();        // recolorea todo segun EditSelectMode + seleccion + activo
        // re-lee las posiciones desde src->vertex y rearma linePos + faceCenter (sin
        // tocar la topologia ni la seleccion). Lo usa el transform de malla en vivo.
        void SincronizarPos();
        void Render();            // dibuja caras (semitransp) + lineas + vertices/puntos-cara

        // seleccion del sub-elemento ACTIVO (EditSelectMode: vertex/edge/face)
        void SeleccionarTodo(bool sel);
        void Invertir();
        void TogglearVert(int k, bool soloEste); // pick de vertice (soloEste=sin shift)
        void TogglearEdge(int e, bool soloEste); // pick de arista
        void TogglearFace(int f, bool soloEste); // pick de cara
        // L: Select Linked — selecciona la isla conexa que contiene al elemento k (en
        // 'modo' SelVertex/Edge/Face). soloEste reemplaza; si no, agrega.
        void SeleccionarLinked(int k, int modo, bool soloEste);
        // Shift+Alt+Click (modo cara): Loop Select de caras desde el borde edgeK
        // (propaga por quads, perpendicular al borde). soloEste reemplaza; si no, agrega.
        void SeleccionarLoopFace(int edgeK, bool soloEste);
        // Modo BORDE — dos tipos de loop (como Blender):
        //  EDGE LOOP: el anillo que SIGUE la linea del borde (el circulo de un cilindro);
        //    cruza cada vertice regular (4 bordes) al borde que NO comparte cara.
        //  EDGE RING: los travesanos PERPENDICULARES (camina por quads como el loop de caras,
        //    pero junta los bordes opuestos). soloEste reemplaza; si no, agrega.
        void SeleccionarLoopEdge(int edgeK, bool soloEste);
        void SeleccionarLoopEdgeVerts(int edgeK, bool soloEste); // modo VERTICE: verts del edge loop (sin acumular edgeSel)
        void SeleccionarRingEdge(int edgeK, bool soloEste);
        // Pick Shortest Path (Ctrl+Click): camino mas corto desde el ACTIVO hasta toIdx, en el
        // modo activo (vert/edge/face). fillRegion=false -> un solo caminito; true -> rellena TODA
        // la region (todos los elementos en ALGUN camino minimo: distA+distB==total; en un grid =
        // el rectangulo). AGREGA a la seleccion (no reemplaza); toIdx queda activo.
        void SeleccionarShortestPath(int toIdx, bool fillRegion);
        // centro LOCAL de la seleccion del sub-elemento activo (para enfocar con ".").
        // false si no hay nada seleccionado.
        bool CentroSeleccion(float& cx, float& cy, float& cz) const;
        // ademas del centro LOCAL, el radio LOCAL (distancia mas lejana al centro) de la seleccion,
        // para que el foco '.' ajuste el zoom a lo seleccionado. false si no hay nada seleccionado.
        bool CentroRadioSeleccion(float& cx, float& cy, float& cz, float& rLocal) const;

        int NumVerts() const { return (int)editVerts.size(); }
        int NumEdges() const { return (int)(lineIdx.size() / 2); }
        int NumFaces() const { return (int)faces.size(); }
};

#endif // EDITMESH_H
