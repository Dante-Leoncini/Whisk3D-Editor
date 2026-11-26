#ifndef CURVE_H
#define CURVE_H

#ifdef _WIN32
    #define NOMINMAX
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#include <string>
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // portable: iconos compartidos
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
    #ifndef _WIN32
    #include <GL/glext.h>
    #endif
    #include "WhiskUI/theme/colors.h"
    //para leer el archivo de texto
    #include <fstream>
    #include <sstream>
    #include <iostream>
#endif

struct KDNode {
    int index;        // indice del vertice original
    Vector3 point;

    KDNode* left;
    KDNode* right;

    KDNode() : index(0), left(NULL), right(NULL) {}
};

class Mesh;   // la malla de edicion interna (miembro 'edicion', ver abajo)
class Material; // el material de la geometria generada (tubo / cinta)

// ============================================================================
//  SPLINES (Bezier / poly): la curva AUTORADA con manijas, como las de Blender.
//
//  Una Curve puede ser de dos clases, y NUNCA las dos a la vez:
//   - CURVA DE NODOS (la de siempre): 'splines' VACIO. Los nodos son 'vertex' (+ 'edges' si
//     es un grafo con ramas); vienen de un .cap ('origin') o inline del .w3d ("points").
//   - CURVA DE SPLINES: 'splines' con al menos una. Los puntos de control y sus manijas son
//     la VERDAD, y 'vertex'/'vertexSize' (+ 'edges') pasan a ser su TESELACION en espacio
//     local (Tessellate). Asi todo lo que ya consumia la polilinea -el riel de la camara,
//     FindNearest, el modificador Oclusion, un constraint que sigue el camino- anda igual
//     sobre una Bezier sin enterarse de que existen las manijas.
//
//  Las manijas son POSICIONES ABSOLUTAS (como handle_left/handle_right de Blender), en el
//  mismo espacio local que el punto. Los tipos tambien son los de Blender: Free, Aligned
//  (las dos opuestas, cada una con su largo), Vector (apunta a 1/3 del vecino) y Auto (las
//  calcula la curva: CalcHandles, con la MISMA cuenta que Blender para que una curva traida
//  de alla no cambie de forma al editarla aca).
// ============================================================================
enum { W3dHandleFree = 0, W3dHandleAligned = 1, W3dHandleVector = 2, W3dHandleAuto = 3 };
enum { W3dSplinePoly = 0, W3dSplineBezier = 1 };
// seleccion de Edit Mode de un punto (bits): manija izquierda, el punto, manija derecha. No se
// guarda en el .w3d; vive en el punto para que el undo y el rearmado del proxy la conserven.
enum { W3dCurveSelL = 1, W3dCurveSelP = 2, W3dCurveSelR = 4 };

struct CurvePoint {
    Vector3 co;               // el punto de control (espacio LOCAL de la curva)
    Vector3 left, right;      // manijas: posiciones ABSOLUTAS en el mismo espacio (solo Bezier)
    unsigned char typeL, typeR;   // W3dHandle*
    unsigned char sel;        // W3dCurveSel* (Edit Mode)
    float radius;             // escala del perfil del bevel en este punto (1 = el bevelDepth tal cual)
    float tilt;               // giro del perfil alrededor de la tangente, en RADIANES (como Blender)
    CurvePoint() : typeL(W3dHandleAuto), typeR(W3dHandleAuto), sel(0), radius(1.0f), tilt(0.0f) {}
};

struct CurveSpline {
    std::vector<CurvePoint> points;
    int  type;                // W3dSplinePoly / W3dSplineBezier
    bool cyclic;              // cerrada: el ultimo punto vuelve al primero
    int  resolution;          // segmentos por tramo Bezier; 0 = el de la curva (Curve::resolution)
    CurveSpline() : type(W3dSplineBezier), cyclic(false), resolution(0) {}
};

class Curve : public Object {
    public:
        int vertexSize;   // (inicializados en el constructor: C++03)
        GLfloat* vertex;
        GLushort* indices;
        std::string origin; // archivo del que se cargo (LoadFromFile lo setea; lo
                            // guarda el .w3d para poder recargar la curva al abrir)

        // ---- CANALES OPCIONALES POR NODO (riel de camara) --------------------
        // Un .cap puede intercalar, DESPUES de cada "p x y z", dos lineas mas:
        //     r <pitch> <yaw> <roll>   grados
        //     f <fov>                  grados, vertical
        // Es la MIRADA AUTORAL que el juego original grababa nodo por nodo (docs/04
        // 6). Un .cap sin ellas deja los dos punteros en NULL y todo se comporta como
        // siempre; un motor viejo las ignora (ya se saltea toda linea que no sea 'p').
        // Los consume Camera cuando esta en modo "mirada del riel".
        GLfloat* rotNode;   // 3 por nodo (pitch, yaw, roll) o NULL
        GLfloat* fovNode;   // 1 por nodo o NULL

        // ---- CONVENCION DE EJES DEL ARCHIVO ----------------------------------
        // Un .cap puede declarar en que mano viene autoreado, con una linea
        // OPCIONAL despues del `count`:
        //     ejes x y z      -> ya viene en el espacio del motor (signZ = +1)
        //     ejes x y -z     -> viene con la Z al reves (signZ = -1)
        // Sin esa linea el default es -1 POR COMPATIBILIDAD: el cargador negaba la
        // Z de todo nodo incondicionalmente, sin forma de apagarlo, porque el unico
        // productor historico de .cap autoreaba en un espacio de mano contraria.
        // Un proyecto que autoree su curva en el espacio del motor pone
        // `ejes x y z` y deja de estar espejado.
        //
        // NO ES SOLO EL CARGADOR: el forward que arma Camera a partir del canal 'r'
        // (pitch/yaw/roll -> direccion de mirada) tiene que sufrir EL MISMO espejo,
        // o la camara queda mirando al reves. Por eso el signo vive en UN dato del
        // archivo y las dos formulas lo leen de aca, en vez de ser un acuerdo tacito
        // entre dos cuentas escritas en dos archivos distintos.
        //
        // CONVENCION DEL CANAL 'r' (en el espacio del motor, o sea con signZ = +1):
        //     forward = ( -sin(yaw)*cos(pitch),  sin(pitch),  -cos(yaw)*cos(pitch) )
        //     roll    = giro sobre ese eje
        // es decir: yaw 0 mira hacia -Z, yaw +90 hacia -X, y pitch positivo levanta.
        float signZ;       // +1 = espacio del motor; -1 = Z espejada

        // ---- ENCUADRE AUTORAL DEL RIEL ---------------------------------------
        // Cabecera OPCIONAL (va suelta, como `ejes`):
        //     aspecto <ancho/alto>     ej: aspecto 1.481481  (40:27)
        // Es el ASPECTO DE IMAGEN con el que se autoreo el recorrido (el mismo
        // numero con el que se hornean las listas de visibilidad por nodo). Si la
        // camara que viaja por este riel no declara un `aspecto:` propio, el modo
        // juego encuadra el render a ESTE aspecto (letterbox/pillarbox) en vez de
        // usar el del render/ventana. 0 = el .cap no lo declara (todo como antes).
        float aspect;

        Curve(Object* parent = NULL, Vector3 pos = Vector3(0,0,0));

        ~Curve();

        ObjectType getType() W3D_OVERRIDE;

        void RenderObject() W3D_OVERRIDE;

        // FOCO/ENCUADRE ('.' / Frame Selected): centro y radio del bounding de los NODOS.
        // Sin esto el encuadre usaba el origen del objeto (radio 0) y un riel de 1000
        // unidades "se encuadraba" a la nada (queja del dueno: "funciona muy mal").
        Vector3 PointFocus() const W3D_OVERRIDE;
        float   FocusRadius() const W3D_OVERRIDE;

        // ---- GRAFO DE RAMAS (pedido del dueno: "el riel puede tener ramificaciones... todos
        // los path de crash en un solo objeto") -------------------------------------------
        // aristas: pares de indices de nodo. VACIO = polilinea implicita 0-1-2-... (los .cap
        // de siempre). Con aristas, el path es un GRAFO: extrudir varias veces desde un nodo
        // crea ramas, borrar un nodo separa sub-paths. Una RAMA = componente conexa; el
        // modificador Oclusion puede filtrar que ramas participan del nodo-mas-cercano (la
        // rama del bonus no sirve al nivel principal y viceversa).
        std::vector<GLushort> edges;
        std::vector<int> nodeBranch;   // (derivado, no se serializa) componente conexa de cada nodo
        int nBranches;                 // cantidad de ramas (componentes)
        // MALLA DE EDICION INTERNA (parte de la curva, NO un objeto de escena: no se puede
        // seleccionar, ni borrar, ni aparece en el outliner -- muere con la curva). Al entrar
        // a Edit Mode sobre la curva, g_editMesh apunta ACA y toda la maquinaria de edicion
        // de mallas (G/E, vertices/aristas) opera sobre ella; al salir, los nodos vuelven.
        Mesh* edit;                        // NULL hasta la primera edicion; delete en ~Curve
        void RecalculateBranches();        // rehace nodeBranch/nBranches (llamar tras cargar/editar)
        // nodo mas cercano CONSIDERANDO solo las ramas habilitadas (branchesOn[rama] != 0);
        // NULL o vacio = todas. Lineal (~1000 nodos = trivial, tambien en el N95).
        int FindNearestFiltered(const Vector3& target, const std::vector<char>* branchesOn) const;

        bool LoadFromFile(const std::string& filepath);

        KDNode* kdRoot;

        Vector3 GetPoint(int i) const;

        void BuildKDTree();
        KDNode* BuildKDTreeRecursive(std::vector<int>& indices, int depth);
        int FindNearest(const Vector3& target) const;

        // ---- LISTA DE CARGA (streaming por riel, io/W3dResources.h) ----------
        // El sidecar `<riel>.cargas.json` (formato/cargas-json.md) trae, por
        // DIRECCION, que recursos adquiere cada nodo al cruzarse. Se declara:
        //     Curve { filePath: "riel.cap"  ListaCarga { filePath: "riel.cargas.json" } }
        // LoadLoadList parsea el JSON y registra las filas en el almacen;
        // el juego avisa los cruces con W3dLoadsCrossNode(loadsHandle, ...).
        int         loadsHandle;  // -1 = sin lista de carga
        std::string loadsFile;    // el sidecar declarado (para el guardado)
        bool LoadLoadList(const std::string& path);

        // true si el .cap traia el canal de rotacion por nodo
        bool HasRotation() const { return rotNode != NULL && vertexSize > 0; }

        // Rotacion (y fov) del riel en el indice fraccionario 'indice', interpolando
        // entre nodo y nodo POR ARCO CORTO (los 0/360 del yaw cruzan seguido: un lerp
        // crudo hace girar la camara entera en un frame). 'fovOut' puede ser NULL.
        // Devuelve false si la curva no trae rotacion (y no toca las salidas).
        bool RotationInNode(float index, Vector3* pitchYawRoll, float* fovOut) const;

        // ==================================================================
        //  SPLINES (ver el bloque de arriba de la clase)
        // ==================================================================
        std::vector<CurveSpline> splines;   // VACIO = curva de nodos de siempre
        // Los enteros de la forma (resolucion, lados del tubo) se guardan en float a proposito: los
        // edita la tarjeta declarada de Properties (ui/panels), que solo sabe de floats.
        float resolution;       // segmentos por tramo Bezier de las splines sin resolucion propia (12)
        // GEOMETRIA (como el Bevel / Extrude de Blender): un TUBO redondo de radio bevelDepth (los
        // cables), una CINTA de ancho 2*extrude, o las dos juntas (un tubo estirado). 0 y 0 = solo
        // la linea (un riel / un camino). Vale tambien para una curva de nodos.
        float bevelDepth;       // radio del tubo (0 = sin tubo)
        float bevelResolution;  // lados del tubo = 4 + 2 * bevelResolution, como Blender (entero; 4 = 12 lados)
        float extrude;          // media altura de la cinta (0 = sin cinta)
        bool  fillCaps;         // tapas en las puntas de un tubo abierto
        Material* material;     // el de la geometria (NULL = el por defecto)
        // la MALLA GENERADA: interna de la curva (no esta en el arbol, no se guarda, no la ve el
        // outliner). Su Parent es la curva (sin estar en Childrens) para que su matriz de MUNDO sea
        // la de la curva: el pase de transparentes, las sombras y el trazado de rayos la ubican
        // solos. NULL sin bevel ni extrude.
        Mesh* geom;
        // la esta editando el Edit Mode (lo prende/apaga el editor, LayoutInput.cpp): dibuja las manijas y
        // la geometria no escribe profundidad (si no, el tubo tapa los puntos que van por adentro)
        bool editing;
        // (derivado de Tessellate) cada spline ocupa vertex[splineFirst[s] .. +splineCount[s]-1]
        std::vector<int> splineFirst, splineCount;
        std::vector<float> evalRadius, evalTilt;   // radio / giro por vertice teselado
        unsigned tessVersion;   // sube con cada Tessellate (la geometria se rehace si cambio)
        // (lo que tenia la forma la ultima vez que se armo 'geom': si nada cambio no se rehace)
        float geomKey[6];
        const GLfloat* geomVertexPtr;
        bool geomHad;

        bool HasSplines() const { return !splines.empty(); }
        // manijas AUTO y VECTOR (y la ALIGNED que hace pareja con una de esas) recalculadas desde los
        // puntos, con la cuenta de Blender (calchandleNurb). La seleccion decide el orden de las
        // Aligned: la manija ELEGIDA manda (la que el usuario esta moviendo).
        void CalcHandles();
        void CalcHandlesSpline(int s);
        // splines -> vertex/vertexSize/indices/edges (+ ramas). 'full' = ademas el arbol KD y la
        // geometria; un arrastre en vivo lo deja para cuando se suelta (no imprime ni aloca de mas).
        void Tessellate(bool full = true);
        // (re)arma 'geom' si cambio la teselacion o un parametro de la forma; barato si no cambio nada
        void UpdateGeometry();
        // arma la malla de la forma (tubo / cinta) en 'out' (una malla vacia). false = no hay forma
        // (sin bevel ni extrude). La usan la geometria interna y "Convert to Mesh".
        bool BuildShapeMesh(Mesh* out) const;

        // ---- EVALUACION (espacio LOCAL de la curva) ----
        // Bezier cubica: p0 = punto, p1 = su manija derecha, p2 = la manija izquierda del siguiente, p3 = el siguiente
        static Vector3 BezierPoint(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
        static Vector3 BezierDerivative(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
        int  SegmentCount(int s) const;   // tramos de la spline s (cerrada: uno mas)
        // el tramo 'seg' de la spline s en u = 0..1, EXACTO (no sale de la teselacion)
        bool EvalSegment(int s, int seg, float u, Vector3* point, Vector3* tangent) const;
        // t = 0..1 sobre la spline s, repartido parejo POR TRAMO (como el parametro de Blender). En una
        // curva de nodos, sobre la polilinea (s se ignora). tangent sale normalizada (puede ser NULL).
        bool Evaluate(float t, Vector3* point, Vector3* tangent, int s = 0) const;
        // largo de la teselacion: de la spline s, o de toda la curva (s < 0)
        float Length(int s = -1) const;
        // el punto a 'dist' metros del principio de la spline s, por LARGO de arco (velocidad pareja)
        bool EvaluateAtLength(float dist, Vector3* point, Vector3* tangent, int s = 0) const;
};

// DUPLICAR (Shift+D, ObjectMode.cpp): una curva nueva con los mismos nodos o splines, forma y material
Curve* CurveDuplicate(Curve* src);

#endif