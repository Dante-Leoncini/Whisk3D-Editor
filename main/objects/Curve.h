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
    #include "WhiskUI/theme/colores.h"
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

class Curve : public Object {
    public:
        int vertexSize;   // (inicializados en el constructor: C++03)
        GLfloat* vertex;
        GLushort* indices;
        std::string origen; // archivo del que se cargo (LoadFromFile lo setea; lo
                            // guarda el .w3d para poder recargar la curva al abrir)

        // ---- CANALES OPCIONALES POR NODO (riel de camara) --------------------
        // Un .cap puede intercalar, DESPUES de cada "p x y z", dos lineas mas:
        //     r <pitch> <yaw> <roll>   grados
        //     f <fov>                  grados, vertical
        // Es la MIRADA AUTORAL que el juego original grababa nodo por nodo (docs/04
        // 6). Un .cap sin ellas deja los dos punteros en NULL y todo se comporta como
        // siempre; un motor viejo las ignora (ya se saltea toda linea que no sea 'p').
        // Los consume Camera cuando esta en modo "mirada del riel".
        GLfloat* rotNodo;   // 3 por nodo (pitch, yaw, roll) o NULL
        GLfloat* fovNodo;   // 1 por nodo o NULL

        // ---- CONVENCION DE EJES DEL ARCHIVO ----------------------------------
        // Un .cap puede declarar en que mano viene autoreado, con una linea
        // OPCIONAL despues del `count`:
        //     ejes x y z      -> ya viene en el espacio del motor (signoZ = +1)
        //     ejes x y -z     -> viene con la Z al reves (signoZ = -1)
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
        // CONVENCION DEL CANAL 'r' (en el espacio del motor, o sea con signoZ = +1):
        //     forward = ( -sin(yaw)*cos(pitch),  sin(pitch),  -cos(yaw)*cos(pitch) )
        //     roll    = giro sobre ese eje
        // es decir: yaw 0 mira hacia -Z, yaw +90 hacia -X, y pitch positivo levanta.
        float signoZ;       // +1 = espacio del motor; -1 = Z espejada

        // ---- ENCUADRE AUTORAL DEL RIEL ---------------------------------------
        // Cabecera OPCIONAL (va suelta, como `ejes`):
        //     aspecto <ancho/alto>     ej: aspecto 1.481481  (40:27)
        // Es el ASPECTO DE IMAGEN con el que se autoreo el recorrido (el mismo
        // numero con el que se hornean las listas de visibilidad por nodo). Si la
        // camara que viaja por este riel no declara un `aspecto:` propio, el modo
        // juego encuadra el render a ESTE aspecto (letterbox/pillarbox) en vez de
        // usar el del render/ventana. 0 = el .cap no lo declara (todo como antes).
        float aspecto;

        Curve(Object* parent = NULL, Vector3 pos = Vector3(0,0,0));

        ~Curve();

        ObjectType getType() override;

        void RenderObject() override;

        bool LoadFromFile(const std::string& filepath);

        KDNode* kdRoot;

        Vector3 GetPoint(int i) const;

        void BuildKDTree();
        KDNode* BuildKDTreeRecursive(std::vector<int>& indices, int depth);
        int FindNearest(const Vector3& target) const;

        // ---- LISTA DE CARGA (streaming por riel, io/W3dRecursos.h) ----------
        // El sidecar `<riel>.cargas.json` (formato/cargas-json.md) trae, por
        // DIRECCION, que recursos adquiere cada nodo al cruzarse. Se declara:
        //     Curve { filePath: "riel.cap"  ListaCarga { filePath: "riel.cargas.json" } }
        // CargarListaCarga parsea el JSON y registra las filas en el almacen;
        // el juego avisa los cruces con W3dCargasCruzarNodo(cargasHandle, ...).
        int         cargasHandle;   // -1 = sin lista de carga
        std::string cargasArchivo;  // el sidecar declarado (para el guardado)
        bool CargarListaCarga(const std::string& ruta);

        // true si el .cap traia el canal de rotacion por nodo
        bool TieneRotacion() const { return rotNodo != NULL && vertexSize > 0; }

        // Rotacion (y fov) del riel en el indice fraccionario 'indice', interpolando
        // entre nodo y nodo POR ARCO CORTO (los 0/360 del yaw cruzan seguido: un lerp
        // crudo hace girar la camara entera en un frame). 'fovOut' puede ser NULL.
        // Devuelve false si la curva no trae rotacion (y no toca las salidas).
        bool RotacionEnNodo(float indice, Vector3* pitchYawRoll, float* fovOut) const;
};

#endif