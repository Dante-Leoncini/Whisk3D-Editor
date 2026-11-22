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
};

#endif