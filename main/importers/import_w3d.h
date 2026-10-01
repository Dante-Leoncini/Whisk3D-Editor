#ifndef IMPORTW3D_H
#define IMPORTW3D_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#include <map>
#include <string>
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "objects/Objects.h"
#include "objects/Mirror.h"
#include "objects/Curve.h"
#include "animation/VertexAnimation.h"
#include "objects/Collection.h"
#ifndef W3D_SYMBIAN
#include "controles.h"   // input SDL de escritorio; import_w3d no usa sus simbolos (en el telefono se guarda)
#endif

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"

#include "importers/import_wobj.h"

// ----------------------------- helpers -----------------------------
float GetFloatOrDefault(const std::map<std::string,std::string>& props, const std::string& k, float def=0.0f);
int GetIntOrDefault(const std::map<std::string,std::string>& props, const std::string& k, int def=0);
GLenum GetLightIDOrDefault(std::string name, GLenum defaultLight = GL_LIGHT0);
std::string Unquote(const std::string& s);

// ----------------------------- Tokenizer -----------------------------
std::vector<std::string> Tokenize(const std::string& src);

// ----------------------------- Node & Find -----------------------------
struct Node {
    std::string type;
    std::map<std::string,std::string> props;
    std::vector<Node*> children;
};

Node* Find(Node* root, const std::string& type);
Node* ParseNode(std::vector<std::string>& tk, size_t& i);

ViewportBase* BuildLayout(Node* n);
void ApplyViewport3DProps(Viewport3D* v, const std::map<std::string,std::string>& p);
void ApplyCommonProps(Object* obj, const std::map<std::string,std::string>& p);
Object* CreateObjectFromNode(Node* n, Object* parent);
void BuildObjectRecursive(Node* n, Object* parent);
void BuildScene(Node* root);

// ----------------------------- Open W3D -----------------------------
// entrada UNICA de lectura de proyectos: detecta el formato POR CONTENIDO
// ('{' = JSON plano nuevo, 'PK' = zip v2 viejo, 'Whisk3D' = texto viejo) y
// rutea. Nunca crashea: cualquier problema deja aviso en el log y defaults.
void AbrirW3D(const std::string& ruta);
// compat: abre w3dPath (la global). Es AbrirW3D(w3dPath).
void OpenW3D();

// ----------------------------- Malla de trabajo -----------------------------
// una Mesh FUERA de la escena (sin padre, sin nombre buscado, sin tocar la seleccion): para
// convertir una malla del registro entre texto (.w3dm) y binario (.w3db) al cargar o guardar.
// Se libera con W3dMallaTemporalBorrar.
class Mesh;
Mesh* W3dMallaTemporalNueva();
void  W3dMallaTemporalBorrar(Mesh* t);

// ----------------------------- Escena 3D del JUEGO -----------------------------
// LA ESCENA 3D EN UN JUEGO COMPILADO (la usa el runtime, w3drun): carga el
// proyecto.json que "Compilar juego" dejo al lado del binario CON EL MISMO lector
// que el editor, y despues resuelve targets/constraints y termina de cargar las
// texturas. Ver el comentario largo en import_w3d.cpp.
// Existe en los DOS builds (con y sin editor): el harness de pruebas lo usa para
// verificar que el juego lee exactamente lo mismo que el Play.
bool W3dProyectoCargarEscena3D(const void* datos, size_t n);

// ----------------------------- Anexar en RUNTIME -----------------------------
// ANEXA un .w3d (JSON v3 plano) a la escena EN CALIENTE: objetos + materiales +
// sus animaciones de escena, sin limpiar nada del proyecto abierto. Lo usa el
// bind lua importarW3D() (streaming del modo juego). Devuelve el primer objeto
// raiz creado, o NULL. Ver el comentario largo en import_w3d.cpp.
Object* W3dImportarW3DAnexo(const std::string& ruta);

// ----------------------------- Medicion de la carga -----------------------------
// REPARTO DEL TIEMPO DE APERTURA POR FASE (lo lee el comando `cargabench` del harness,
// y es la linea base contra la que se mide todo lo que acelere la carga).
// El reloj es un PUNTERO a proposito: este lector es compartido (editor, juego
// compilado, Symbian) y no puede depender de un reloj de plataforma. NULL (lo normal)
// = no se mide nada y no cuesta nada; el que mide lo apunta a un reloj de PARED en
// milisegundos mientras dura la medicion, y pone los acumuladores en cero con Reset().
struct W3dCargaFases {
    double montajeMs;    // montar el contenedor (indice del zip) + leer proyecto.json (o el archivo)
    double jsonMs;       // parsear el proyecto: proyecto.json (el arbol JVal) o el texto viejo (Tokenize+ParseNode)
    double objetosMs;    // TODOS los objetos del arbol (incluye mallas y armatures)
    double mallasMs;     // objetos malla: leer su geometria y dejarla lista para dibujar
    double mallaLeerMs;  //   de eso: leer la entrada .w3dm (contenedor/disco) o el .obj del texto viejo
    double mallaParseMs; //   de eso: parsear el .w3dm / el .obj (texto -> arrays)
    double mallaDerivadosMs; // de eso: derivar al cargar (index buffer por parte, bordes, capas al render, mapa de puntos)
    double clipsMs;      // armatures: huesos + clips de animacion
    double animEscenaMs; // curvas de las animaciones de escena (bloque "animaciones")
    double precargaMs;   // precarga del pie de la apertura (texturas 3D/2D + sonidos)
    int    mallas;       // mallas construidas
    int    mallasBin;    //   de esas: las que salieron de un .w3db (binario, sin derivar nada)
    int    mallasDerivadas; // las que pagaron el cierre clasico (Reagrupar/Forsyth/CalcularBordes/
                            // AplicarCapasAlRender): un .w3dm de texto, o un .w3db que no traia algo
    int    armatures;    // armatures construidos
    int    clips;        // clips de esqueleto (suma de todos los armatures)
    W3dCargaFases() { Reset(); }
    void Reset() {
        montajeMs = jsonMs = objetosMs = mallasMs = clipsMs = animEscenaMs = precargaMs = 0.0;
        mallaLeerMs = mallaParseMs = mallaDerivadosMs = 0.0;
        mallas = mallasBin = mallasDerivadas = armatures = clips = 0;
    }
};
extern W3dCargaFases g_w3dCargaFases;
extern double (*g_w3dCargaReloj)();

// ============================================================================
//  FORMATO DE LAS MALLAS AL GUARDAR (opcion del PROYECTO: "formatoMallas" en el
//  proyecto.json). BINARIO (.w3db, io/W3dMallaBin.h) por defecto: abrir es un
//  memcpy. TEXTO (.w3dm) para quien quiera diffs legibles. El lector lo fija al
//  abrir (ausente = binario) y ReiniciarEscena lo vuelve al default.
//  g_w3dFormatoMallasForzado (-1 = no) PISA al del proyecto para TODO el proceso:
//  lo usa el harness (los tests del .w3dm fijan texto aunque abran otros .w3d).
// ============================================================================
enum { W3D_MALLAS_BINARIO = 0, W3D_MALLAS_TEXTO = 1 };
extern int g_w3dFormatoMallasProyecto;
extern int g_w3dFormatoMallasForzado;
// el formato con el que se guardan HOY las mallas (el forzado si hay, si no el del proyecto)
inline int W3dFormatoMallas() {
    return g_w3dFormatoMallasForzado >= 0 ? g_w3dFormatoMallasForzado : g_w3dFormatoMallasProyecto;
}

// ============================================================================
//  LAS MALLAS .w3db COMO LAS LEE EL JUEGO COMPILADO: solo los bloques de RENDER; los
//  de edicion quedan pendientes (Mesh::edicionPendiente) y se materializan solo si el
//  stack de la malla GENERA malla (W3dStackGeneraMalla) o alguien pide sus poligonos
//  (ConstruirPolyMesh). En el juego (W3D_SIN_EDITOR) es SIEMPRE asi; en el editor lo
//  prende el harness (w3dbjuego) para probar esa carga sin compilar un juego. Una malla
//  cargada asi NO se puede guardar (no tiene caras): el harness la cierra al terminar.
// ============================================================================
#ifndef W3D_SIN_EDITOR
extern bool g_w3dMallasComoJuego;
#endif

#endif