#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <vector>
#include "render/OpcionesRender.h"   // RenderType / g_redraw: son del editor
#ifndef W3D_SYMBIAN
#include <SDL2/SDL.h>
#endif

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif

#ifndef W3D_SYMBIAN
#include "variables.h"
#endif
#ifndef W3D_SYMBIAN
#include "sdl_key_compat.h"
#endif
#include "ViewPorts.h"
#include "ScrollBar.h"
#include "WithBorder.h"

#include "objects/Objects.h"
#include "objects/ObjectMode.h"

#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Mesh.h"

#include "WhiskUI/Propieties/PropertieBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropGap.h"
#include "WhiskUI/Propieties/PropList.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropColor.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropLabel.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropSeparator.h"
#include "WhiskUI/Propieties/PropButtonRow.h"

void DibujarTitulo(Object* obj, int maxPixels);

// "Load Texture": cada plataforma lo cablea (PC = browser compartido). Carga
// una imagen y la asigna al material 'mat' (async: puede abrir un modal).
class Material;
extern void (*DialogoCargarTextura)(Material* mat);
// Crea un material nuevo (Material, Material.001, ...) en el mesh part indicado y lo devuelve. NULL si el indice no
// existe. Lo usa el Add > Reference para dejar el material listo y abrirle el selector de textura enseguida.
Material* NuevoMaterialEnMeshPart(Mesh* mesh, int idx);
// "Load Texture" del normal map: prende este flag y usa el MISMO DialogoCargarTextura (browser compartido 4 OS);
// el callback de carga de cada plataforma asigna a mat->normalTexture en vez de mat->texture cuando esta en true.
extern bool gCargarTexturaComoNormal;

class Properties;
// el panel de propiedades con el que se INTERACTUO por ultima vez: las
// acciones globales (menus de material/textura, rebind) operan sobre el
extern Properties* PropsActivo;
// lleva el panel ACTIVO a la pestania "ARMATURE 2D" (tab 6, huesos 2D del mesh). La llama el editor
// UV al ENTRAR a la edicion de huesos (Tab, menu Add > Armature 2D, selector de Modo): el dueno
// reporto que "no veia" el panel del rig 2D porque vivia mezclado en la pestania de datos del mesh
// y habia que ir a buscarlo. No-op sin panel activo (headless) o si el tab no corresponde.
void PropsIrAArmature2D();

// une los campos "Path" + "File name" de las tarjetas EXPORT y RENDER en la ruta final.
// Es un envoltorio finito de W3dRutaEnCarpeta (FileBrowser.h), la UNICA implementacion de
// "carpeta + nombre" del editor: carpeta vacia -> salida por defecto, carpeta RELATIVA ->
// contra el proyecto abierto. 'nombreDefecto' se usa si el campo del nombre esta vacio.
std::string W3dRutaSalidaCampos(const std::string& dir, const std::string& nombre,
                                const char* nombreDefecto);

class Properties : public ViewportBase, public WithBorder, public Scrollable {
    public:
        Scrollable* ComoScrollable() { return this; }
        Properties();
        ~Properties() W3D_OVERRIDE;

        // grupos PROPIOS del panel: cada viewport de propiedades es
        // INDEPENDIENTE (seleccion, scroll de la lista, tamanos). Los
        // nombres pisan a los viejos globales a proposito: los metodos
        // siguen compilando igual.
        std::vector<GroupPropertie*> GroupProperties;
        GroupPropertie* propTransform;
        PropBool* propObjVisible;   // visible en el viewport (animable)
        PropBool* propObjRender;    // sale en el render final (animable)
        PropBool* propObjRelLines;  // dibujar la linea al padre. Va en la tarjeta GENERICA:
                                    // es una opcion UNIVERSAL de todos los objetos.
        // tarjeta "Animacion" del OBJETO (pestania Objeto): animaciones del objeto
        // (contenedores; hoy vertex anims de la malla) — lista + New + Delete
        GroupPropertie* propObjAnim;
        PropListMeshParts* propListObjAnims;
        GroupPropertie* propMeshParts; // tarjeta: selector de parte + gestion (New/Assign/Select/.../Rename)
        GroupPropertie* propMaterial;  // tarjeta APARTE: el material del mesh part seleccionado
        PropButtonRow* propRowPartOps; // fila: Assign | Select | Deselect
        PropButtonRow* propRowDelRen;  // fila: Delete | Rename (Delete oculto si hay 1 sola parte)
        PropButtonRow* propRowPartMove;// fila: Move Up | Move Down (oculta si hay 1 sola parte; orden de dibujado)
        // props del material como MEMBERS (Rebind los setea por nombre, NO por indice -> reordenar la
        // tarjeta no rompe nada): 8 checkboxes + 3 colores + shininess.
        PropBool*  propMatChk[11]; // Filtering, Transparent, VertexColor, Lighting, Repeat, Culling, DepthTest, Smooth, Chrome, Equirect360, NormalMapping
        PropColor* propMatCol[3];  // Base Color, Specular, Emission
        PropFloat* propMatShin;    // Shininess
        // ---- CALCOMANIAS / MEZCLA / PROFUNDIDAD -----------------------------------
        // Hasta hoy `decal`, `mezcla`, `sesgoProfundidad` y `ordenPasada` solo se podian
        // poner desde el .mtl o el .w3d: en el editor no habia forma de ver como se porta
        // una sombra pegada al piso sin salir a editar un archivo de texto. Ahora estan
        // en la tarjeta Material: DOS DESPLEGABLES (mezcla y test de profundidad) + el
        // sesgo, el orden de pasada y un tilde "Decal" que aplica la receta entera.
        PropBool*  propMatDecal;   // preset DECAL (transparente + sin escribir z + sesgo + pasada 1)
        PropFloat* propMatSesgo;   // sesgoProfundidad (glPolygonOffset; negativo = hacia el ojo)
        PropFloat* propMatOrden;   // ordenPasada 0 opaco / 1 decal / 2 transparente
        PropBool*  propMatLineas;      // LINEAS: dibujar las aristas de la malla con el material
        PropFloat* propMatGrosorLinea; // grosor de esas lineas en px (glLineWidth); visible con Lines ON
        GroupPropertie* propLight;   // pestania de luz: TODAS las propiedades editables de la luz GL
        // tarjeta del elemento TEXTO 2D (se edita en el Editor 2D)
        GroupPropertie* propTexto2D;
        PropText*   propT2dNombre;   // nombre del objeto (arriba de todo; el tab Objeto no se muestra)
        PropFloat*  propT2dPosX;     // posicion (X/Y en el lienzo, Z = profundidad)
        PropFloat*  propT2dPosY;
        PropFloat*  propT2dPosZ;
        PropFloat*  propT2dOpac;     // opacidad 0..1 (multiplica a la de los padres)
        PropBool*   propT2dPosAbs;   // posicion en PIXELES (off = relativa al tamano de la UI)
        PropFloat*  propT2dPeso;     // peso en el reparto de filas/columnas del padre
        PropButton* propT2dLineas;   // desplegable: una linea / por palabras / cualquier parte
        PropBool*   propT2dAutoTam;  // ajustar el tamano de fuente al area disponible
        PropText*   propT2dTexto;    // que dice ("Texto" por defecto)
        PropButton* propT2dTipo;     // desplegable: string / number / float
        PropFloat*  propT2dDec;      // decimales del float (0 = no se ven)
        PropFloat*  propT2dTam;      // alto de fuente, en px del lienzo
        PropButton* propT2dAlignH;   // desplegable: izquierda / centro / derecha
        PropButton* propT2dAlignV;   // desplegable: arriba / centro / abajo
        PropColor*  propT2dColor;    // swatch -> ColorPicker
        PropButton* propT2dPal;      // desplegable: color propio o de la paleta
        PropButton* propT2dFuente;   // desplegable: Whisk3D / cargar un .ttf
        PropButton* propT2dAncla;    // desplegable: desde donde se mide la posicion (ancla)
        PropFloat*  propT2dRot;      // rotacion en grados
        // tarjeta del elemento IMAGEN 2D (una textura con modo de ajuste)
        GroupPropertie* propImagen2D;
        PropButton* propImgTextura;  // elegir el archivo de imagen (file browser)
        PropFloat*  propImgAncho;    // el rectangulo del elemento, en px de lienzo
        PropFloat*  propImgAlto;
        PropFloat*  propImgRot;      // rotacion en grados
        PropButton* propImgModo;     // desplegable: estirar / ajustar / cover
        PropButton* propImgAncla;    // desplegable de ancla (mismo esquema que el texto)
        PropText*   propImgNombre;   // nombre del objeto (arriba de todo)
        PropFloat*  propImgPosX;
        PropFloat*  propImgPosY;
        PropFloat*  propImgPosZ;
        PropFloat*  propImgOpac;     // opacidad 0..1
        PropBool*   propImgPosAbs;   // posicion en pixeles (off = relativa)
        PropFloat*  propImgPeso;
        PropButton* propImgUnidad;   // desplegable: unidad del ancho/alto (fraccion / px / escalado)
        PropColor*  propImgColor;    // tinte de la textura
        PropButton* propImgPal;
        PropBool*   propImgAlpha;    // usar el canal alpha de la textura
        PropBool*   propImgFiltro;   // filtrado de textura (off = pixel-perfect)
        // tarjeta del elemento RECTANGULO 2D (color solido o transparente: acomoda hijos)
        GroupPropertie* propRect2D;
        PropText*   propRectNombre;
        PropFloat*  propRectPosX;
        PropFloat*  propRectPosY;
        PropFloat*  propRectPosZ;
        PropBool*   propRectPosAbs;
        PropFloat*  propRectAncho;
        PropFloat*  propRectAlto;
        PropFloat*  propRectRot;
        PropButton* propRectAncla;
        PropFloat*  propRectOpac;
        PropColor*  propRectColor;
        PropButton* propRectPal;
        PropFloat*  propRectPeso;
        PropButton* propRectUnidad;  // desplegable: unidad del tamano
        // tarjeta del elemento CONTENEDOR (rectangulo invisible: solo ordena a sus hijos)
        GroupPropertie* propCont2D;
        PropText*   propContNombre;
        PropFloat*  propContPosX;
        PropFloat*  propContPosY;
        PropFloat*  propContPosZ;
        PropBool*   propContPosAbs;
        PropFloat*  propContPeso;
        PropFloat*  propContAncho;
        PropFloat*  propContAlto;
        PropFloat*  propContRot;
        PropButton* propContAncla;
        PropFloat*  propContOpac;
        PropButton* propContUnidad;  // desplegable: unidad del tamano
        // tarjeta del elemento SLICE 9 (imagen con bordes fijos)
        GroupPropertie* propS9card;
        PropText*   propS9Nombre;
        PropFloat*  propS9PosX;
        PropFloat*  propS9PosY;
        PropFloat*  propS9PosZ;
        PropBool*   propS9PosAbs;
        PropFloat*  propS9Peso;
        PropButton* propS9Textura;
        PropFloat*  propS9Ancho;
        PropFloat*  propS9Alto;
        PropButton* propS9Unidad;    // desplegable: unidad del tamano
        PropFloat*  propS9BordeX;    // grosor del borde EN la textura (px del archivo), por eje
        PropFloat*  propS9BordeY;
        PropFloat*  propS9EscBorde;  // grosor dibujado = borde * esto
        PropFloat*  propS9Rot;
        PropButton* propS9Ancla;
        PropFloat*  propS9Opac;
        // tarjeta del BOTON 2D
        GroupPropertie* propBtn2D;
        PropText*   propBtnNombre;
        PropFloat*  propBtnPosX;
        PropFloat*  propBtnPosY;
        PropFloat*  propBtnPosZ;
        PropBool*   propBtnPosAbs;
        PropFloat*  propBtnPeso;
        PropText*   propBtnTexto;
        PropButton* propBtnIcono;    // elegir el png del icono
        PropFloat*  propBtnTam;
        PropFloat*  propBtnPad;
        PropButton* propBtnAncla;
        PropFloat*  propBtnRot;
        PropFloat*  propBtnOpac;
        PropColor*  propBtnColFondo;
        PropColor*  propBtnColTexto;
        PropColor*  propBtnColBorde;
        PropColor*  propBtnColHover; // borde de mouse-over (verde accent por defecto)
        PropButton* propBtnPalFondo; // desplegable: color propio o uno de la paleta
        PropButton* propBtnPalTexto;
        PropButton* propBtnPalBorde;
        PropButton* propBtnPalHover;
        PropButton* propBtnTex;      // textura de fondo (9 pedazos) opcional
        PropFloat*  propBtnTexBX;
        PropFloat*  propBtnTexBY;
        PropFloat*  propBtnTexEsc;
        // tarjeta del VIDEO 2D
        GroupPropertie* propVid2D;
        PropText*   propVidNombre;
        PropFloat*  propVidPosX;
        PropFloat*  propVidPosY;
        PropFloat*  propVidPosZ;
        PropBool*   propVidPosAbs;
        PropFloat*  propVidPeso;
        PropButton* propVidArchivo;  // elegir el .mp4/.webm/.gif
        PropFloat*  propVidAncho;
        PropFloat*  propVidAlto;
        PropButton* propVidUnidad;   // desplegable: unidad del tamano
        PropButton* propVidModo;     // estirar / ajustar / cover
        PropBool*   propVidLoop;
        PropBool*   propVidAlpha;    // usar la transparencia del video
        PropBool*   propVidPlay;     // ver la animacion en el editor
        PropBool*   propVidFiltro;
        PropButton* propVidAncla;
        PropFloat*  propVidRot;
        PropFloat*  propVidOpac;
        // tarjeta MARGEN (del elemento 2D activo): aire alrededor en filas/columnas +
        // el checkbox "expandir" (absorbe el sobrante como el elemento Expandir)
        GroupPropertie* propMargen;
        PropBool*   propMargExp;     // expandirse en la fila/columna
        PropBool*   propMargUni;     // un solo valor para los 4 lados
        PropFloat*  propMargTodos;   // el valor unico (con uniforme prendido)
        PropFloat*  propMargIzq;     // por lado (con uniforme apagado)
        PropFloat*  propMargDer;
        PropFloat*  propMargArr;
        PropFloat*  propMargAba;
        // tarjeta del EXPANDIR
        GroupPropertie* propExp2D;
        PropText*   propExpNombre;
        PropFloat*  propExpPeso;
        PropColor*  propS9Color;     // tinte del arte (blanco = tal cual)
        PropButton* propS9Pal;
        PropBool*   propS9Filtro;    // filtrado de textura (off = pixel-perfect)
        // tarjeta del objeto UI (la raiz de la interfaz)
        GroupPropertie* propUIcard;
        PropBool*   propUIver3D;     // ver los elementos con su profundidad en la escena 3D
        PropBool*   propUIigualRender; // lienzo = tamano del render (default); apagado = responsive
        PropFloat*  propUIancho;     // el lienzo propio (solo en responsive)
        PropFloat*  propUIalto;
        PropButton* propUIres;       // desplegable de resoluciones (4k .. 240p)
        PropButton* propUIaspecto;   // desplegable de aspecto (16:9 / 4:3 / 1:1)
        PropButton* propUIrotar;     // boton: intercambia ancho y alto
        PropColor*  propUIcolor;     // fondo de la ventana (transparente por defecto)
        PropFloat*  propUIescala;    // escala global del contenido (x1 N95 ... x4 pantallas grandes)
        PropButton* propUIexport;    // guardar el arbol como .w3dui (JSON)
        // el objeto SCRIPT/CONTROL: una tarjeta "Control" (nombre + visible + agregar)
        // y UNA TARJETA POR SCRIPT (archivo + propiedades expuestas + quitar).
        // Invisible = sus scripts no se ejecutan.
        GroupPropertie* propControl;
        PropListMeshParts* propListScripts;  // la LISTA de scripts (modo 12). El orden = orden de ejecucion.
        PropButtonRow* propRowScript;        // Add | Remove
        PropButtonRow* propRowScriptMove;    // Move Up | Move Down (oculta con < 2)
        enum { kMaxScriptCards = 8 };
        GroupPropertie* propScriptCards[kMaxScriptCards];
        int scriptFirma;             // rebuild de las tarjetas al cambiar de script/objeto
        // tarjeta PALETAS (del PROYECTO, pestania 0): filas dinamicas
        // nombre+color por entrada; la gestion respeta los invariantes
        // (misma cantidad en todas; borrar corrige referencias)
        GroupPropertie* propPaleta;
        int paletaFilas;             // firma de lo construido (rebuild al cambiar)
        PropButton* propPaletaSel;   // desplegable: cual paleta EDITA la tarjeta / crear / borrar
        PropText*   propPaletaNombre; // nombre editable de la paleta en edicion (renombra en vivo)
        // tarjeta "Paleta" del OBJETO (cualquier objeto, 3D o 2D): la seleccion
        // heredable ("Igual que el padre" o una paleta del proyecto por nombre)
        GroupPropertie* propPaletaObj;
        PropButton* propPaletaObjSel;
        PropText*   propUInombre;    // nombre del objeto UI
        PropFloat*  propUIopac;      // opacidad de la interfaz ENTERA
        // tarjeta "Children": lo que afecta a los HIJOS del seleccionado (padding = encoge el
        // area donde se enganchan sus anclas; la linea se ve en el Editor 2D con el UI elegido)
        GroupPropertie* propHijos;
        PropBool*   propHijosPadUni;   // un solo valor de padding para los 4 lados
        PropFloat*  propHijosPadTodos; // el valor unico (con uniforme prendido)
        PropFloat*  propHijosPadIzq;   // el padding, POR LADO
        PropFloat*  propHijosPadDer;
        PropFloat*  propHijosPadArr;
        PropFloat*  propHijosPadAba;
        PropButton* propHijosLayout; // desplegable: libremente / filas / columnas
        PropFloat*  propHijosGap;    // espacio entre hijos (solo con filas/columnas)
        PropBool*   propHijosPx;     // padding y gap en px (default) o proporcionales
        PropButton* propHijosAjuste; // desplegable: estirar (100%) / minimo (tamano natural)
        PropButton* propHijosAlign;  // desplegable: inicio / centro / fin (con ajuste minimo)
        PropButton* propHijosDistrib;// desplegable: gap / space-between / space-around /
                                     // space-evenly (reparto css; solo con ajuste minimo)
        PropBool*   propHijosClipX;  // overflow: recortar lo que se sale, por eje
        PropBool*   propHijosClipY;
        PropBool*   propHijosScroll; // permitir scrollear el contenido recortado
        PropFloat*  propHijosScrollX;
        PropFloat*  propHijosScrollY;
        PropBool*   propLightDir;     // Directional (w=0) vs puntual/spot
        PropFloat*  propLightGL;      // numero de GL light (0..7, entero)
        PropColor*  propLightDiffuse; // color difuso
        PropColor*  propLightAmbient; // color ambiente
        PropColor*  propLightSpecular;// color especular
        PropFloat*  propLightAttC;    // atenuacion constante / lineal / cuadratica
        PropFloat*  propLightAttL;
        PropFloat*  propLightAttQ;
        PropFloat*  propLightSpotCut; // spot: angulo del cono
        PropFloat*  propLightSpotExp; // spot: concentracion del haz
        GroupPropertie* propCamera;  // pestania de camara: lente (fov/orto) + target (look-at)
        PropBool*   propCamOrtho;    // proyeccion ortografica vs perspectiva
        PropFloat*  propCamFov;      // campo de vision (grados), animable (AnimFov)
        PropFloat*  propCamNear;     // distancia minima de dibujado, animable (AnimClip/X)
        PropFloat*  propCamFar;      // distancia maxima de dibujado, animable (AnimClip/Y)
        GroupPropertie* propInstance;// pestania de instance/array/mirror: target
        // pestania del objeto LOD: umbrales de distancia como texto "20, 45, 90"
        // (commit en vivo, mismo patron que el texto del elemento 2D)
        GroupPropertie* propLOD;
        PropText*   propLodDist;
        PropBool*   propLodSoloCam;   // medir desde la camara ACTIVA (igual que el Culling)
        // pestania del objeto Culling: desde que camara se corta + distancia maxima
        GroupPropertie* propCulling;
        PropBool*   propCullActivo;    // interruptor del recorte (demo A/B en vivo)
        PropBool*   propCullSoloCam;
        PropFloat*  propCullDistMax;   // culling por distancia (0 = sin limite)
        // pestania de la Collection: orden de dibujo para transparentes
        GroupPropertie* propCollection;
        PropBool*   propCollOrdenCam;
        PropBool*   propCollOrdenUnaVez;
        // pestania del objeto Particulas: el emisor (textura + config del cono).
        // Los numeros/checks bindean DIRECTO a los campos del activo; los dos de
        // TEXTO (textura y color "r, g, b, a") van con el patron del LOD (sync por frame).
        GroupPropertie* propParticulas;
        PropText*   propPartTextura;    // ruta del PNG (con alpha)
        PropFloat*  propPartCantidad;   // particulas/seg (0 = solo rafagas emitir())
        PropFloat*  propPartVida;       // segundos
        PropFloat*  propPartTam;        // lado del billboard (unidades de mundo)
        PropFloat*  propPartVel;        // velocidad inicial (unidades/seg)
        PropFloat*  propPartDispersion; // apertura del cono (grados)
        PropFloat*  propPartGravedad;   // + cae / - sube
        PropBool*   propPartAditivo;    // mezcla aditiva vs alpha
        PropBool*   propPartSustractivo;// mezcla sustractiva dst-src (humo/polvo); gana sobre aditiva
        PropFloat*  propPartVariacion;  // 0..1: jitter por particula sobre vel/vida/tam
        PropFloat*  propPartTurbulencia;// deriva azarosa suave por particula (unid/s^2)
        PropBool*   propPartRotacion;   // nace con angulo azaroso 0..360 fijo (rotz del original)
        PropFloat*  propPartVelRot;     // giro continuo (grados/s), signo azaroso por particula
        PropText*   propPartColor;      // tinte + alpha inicial ("r, g, b, a")
        PropBool*   propPartDesvanecer; // alpha -> 0 con la vida
        PropBool*   propPartActivo;     // false = no emite
        // tarjeta ARCHIVO (pestania Render, arriba de todo): abrir/guardar el
        // proyecto .w3d (v3: JSON plano) con ruta/nombre. Los assets del proyecto son
        // SIEMPRE archivos externos en la carpeta (el checkbox "Empaquetar
        // assets" viejo se elimino; empaquetar es del juego COMPILADO).
        GroupPropertie* propArchivo;
        PropText*   propProyCarpeta;  // carpeta del proyecto
        PropText*   propProyNombre;   // nombre.w3d
        PropButton* propProyAbrir;
        PropButton* propProyGuardar;
        PropButton* propProyVersion; // "Guardar version vN" (guardado por versiones; N real segun versiones/)
        PropButton* propProyComo;
        PropButton* propProyExtraer; // "Extraer assets": saca lo de adentro del .w3d a una carpeta
        GroupPropertie* propRender;  // pestania RENDER: tarjeta "Render" (output)
        GroupPropertie* propAnimation; // pestania RENDER: tarjeta "Animation" (selector + New/Delete + Render Animation)
        GroupPropertie* propKeyframe;  // tarjeta "Keyframe": el keyframe elegido en el editor de curvas, con numeros exactos
        PropButton* propBtnAnimSel;    // dropdown: animacion ACTIVA (Scene(s) / clips del armature seleccionado)
        PropButtonRow* propRowAnimNewDel; // fila: New | Delete (Delete oculto si no hay nada que borrar)
        PropButton* propBtnAnimRename; // "Rename" de la animacion activa (escena o clip)
        PropButton* propBtnAnimRender; // "Render Animation" (gris si no hay animaciones)
        // tarjeta "Juego" (debajo de Animacion): compilar + cache del viaje en el tiempo
        GroupPropertie* propJuego;
        PropButton* propJuegoPlat;      // desplegable: Linux .deb / AppImage / WebGL
        PropButton* propJuegoModoVent;  // desplegable: Ventana / Pantalla completa / Sin bordes
        PropButton* propJuegoOrient;    // desplegable: Todas / Solo vertical / Solo horizontal
        PropButton* propJuegoIcono;     // elegir el PNG (maxima definicion) del icono del juego
        PropButton* propJuegoAssets;    // desplegable: Sueltos (editables) / Empaquetados (protegidos)
        PropBool*   propJuegoFisica;    // checkbox: compilar CON el motor de fisica (default si)
        PropBool*   propJuegoSonido;    // checkbox: compilar CON el audio (default si)
        PropBool*   propJuegoDebug;     // checkbox: modo debug W3D_DEV_LOG=1 (default NO = produccion)
        PropButton* propJuegoCompilar;
        PropBool*   propAnimConservar; // play no pisa lo grabado: lo reproduce
        GroupPropertie* propExport;  // pestania RENDER: tarjeta "Export" (.obj)
        // pestania TRANSFORMAR (tab 5, icono del modo de seleccion; SOLO Edit Mode con malla activa):
        // 2 tarjetas. "Transform Mesh" = X/Y/Z del centro de la seleccion (antes en la pestania
        // Vertices); "Transform UV" = centro X/Y de los UVs de la MISMA seleccion (editarla mueve
        // los UVs con el undo liviano de UV).
        GroupPropertie* propEditItem;   // tarjeta "Transform Mesh" (X/Y/Z del centro/pivote de la seleccion)
        float editPosX, editPosY, editPosZ; // posicion del centro de la seleccion (convencion Z-up del panel; se
                                            // recalcula cada frame; editarla traslada rigido lo seleccionado)
        GroupPropertie* propUVTransform; // tarjeta "Transform UV" (centro X/Y de los UVs de la seleccion)
        float uvPosU, uvPosV;            // centro UV (posiciones unicas); editarlo traslada rigido los UVs
        // pestania VERTICES (icono mesh): 3 tarjetas. Las listas REUSAN PropListMeshParts (modo 1/2).
        GroupPropertie* propUVMaps;     // tarjeta "UV Maps" (lista de UV maps)
        GroupPropertie* propColorLayers;// tarjeta "Color" (lista de capas + modo per-vertex/corner)
        GroupPropertie* propVertexGroups;// tarjeta "Vertex Groups" (pesos por CONTROL-POINT: huesos del rig 3D)
        GroupPropertie* propUVGroups;   // tarjeta "UV Groups" (pesos por CORNER: huesos del armature 2D del UV)
        GroupPropertie* propModifiers;     // pestania "Modifiers" (mesh): selector del stack + Add/Remove/Move
        PropListMeshParts* propListModifiers; // selector del stack de modificadores (modo 3)
        PropButtonRow* propRowMod;         // fila Add | Remove (Remove oculto si no hay modificadores)
        PropButtonRow* propRowModMove;     // fila Move Up | Move Down (oculta si hay < 2)
        GroupPropertie* propModifierProps; // tarjeta con las props del modificador SELECCIONADO
        // --- props del modificador MIRROR (se bindean al Modifier activo en ActualizarPestanias) ---
        PropBool*  propModVerViewport; PropBool* propModVerEdit; // display en viewport / en edit mode (todos los mods)
        PropLabel* propModVacio;   // "(no properties yet)" para tipos sin params todavia
        PropBool*  propMirX; PropBool* propMirY; PropBool* propMirZ; // ejes
        PropButton* propMirTarget; // "Mirror Object" (dropdown: cualquier objeto)
        PropButton* propArmTarget; // "Target" del modificador Armature (dropdown: solo esqueletos)
        PropButton* propBtnOptVG;  // "Optimize Vertex Groups" (1 hueso/vertice) del modificador Armature (destructivo, con confirm)
        PropBool*   propArmCache;      // "Cache Animation" del modificador Armature (bakea el skinning por frame)
        PropFloat*  propArmCacheSkip;  // "Frame Skip" del cache (0=todos; N=cada N+1 e interpola -> menos memoria)
        PropBool*  propMirMerge; PropFloat* propMirDist; PropBool* propMirClip; // merge + distancia + clipping
        // Subdivision Surface: modo (Catmull-Clark/Simple) + niveles viewport/render
        PropBool*  propSubSimple; PropFloat* propSubLevel; PropFloat* propSubRender;
        // Screw: angle, screw(height), steps viewport/render, eje (dropdown X/Y/Z), stretch U/V
        PropFloat* propScrewAngle; PropFloat* propScrewHeight; PropFloat* propScrewSteps; PropFloat* propScrewRender;
        PropButton* propScrewAxis; PropBool* propScrewStretchU; PropBool* propScrewStretchV;
        PropBool* propScrewSmooth; PropBool* propScrewMerge; PropBool* propScrewFlip; // suave / soldar / invertir normales
        // Culling (PVS por triangulo): metodo (desplegable Triangulos/BSP) + Recalcular + info del sidecar
        PropButton* propPvsMetodo;
        PropButton* propPvsRecalc;
        PropLabel*  propPvsInfo;
        PropButton* propBtnApplyMod; // "Apply Modifier": hornea la malla generada en malla real editable
        // ===== pestania "Constraints" (tab 7, la ULTIMA): 2 tarjetas, calcadas de Modifiers =====
        // El stack NO es de la malla: lo tiene cualquier objeto 3D (una luz, una camara, una
        // coleccion). Por eso la lista va por PropListMeshParts::obj (modo 11) y NUNCA por 'mesh'.
        GroupPropertie* propConstraints;       // tarjeta 1: la lista + Add/Remove + Move Up/Down
        PropListMeshParts* propListConstraints;// selector del stack (modo 11: Object::constraints)
        PropButtonRow* propRowCon;             // fila Add | Remove (Remove oculto si no hay ninguno)
        PropButtonRow* propRowConMove;         // fila Move Up | Move Down (oculta si hay < 2)
        GroupPropertie* propConstraintProps;   // tarjeta 2: props del constraint SELECCIONADO
        PropBool*   propConActivo;
        PropBool*   propConVerEdit;           // "Show in Edit Mode" del constraint (default OFF)      // "Enabled" (el ojito de la lista)
        PropButton* propConFuente;      // "Source": desplegable de objetos + la opcion "la vista"
        PropLabel*  propConAvisoFuente; // aviso: sin fuente el constraint no hace nada
        PropFloat*  propConInfluencia;  // "Influence" 0..100 con "%"
        PropBool*   propConEjeX;        // "Copy X/Y/Z" (Copy Location / Copy Rotation)
        PropBool*   propConEjeY;
        PropBool*   propConEjeZ;
        PropLabel*  propConAvisoEjes;   // aviso: sin ningun eje tildado el constraint no hace nada
        PropButton* propConBBModo;      // "Mode" del billboard (tipo arbol / mira a la camara)
        PropLabel*  propConAvisoBB;     // aviso: el giro de 180 con influencia intermedia
        PropListMeshParts* propListUV;    // lista de UV maps (modo=1)
        PropListMeshParts* propListColor; // lista de capas de color (modo=2)
        PropListMeshParts* propListVertGroups; // lista de grupos de vertices / huesos 3D (modo=4)
        PropListMeshParts* propListUVGroups;   // lista de UV groups / huesos 2D (modo=9)
        // pestania ARMATURE (icono esqueleto): tarjeta "Animation" (lista de clips + Add / Rename / Delete / Move).
        // REUSA PropListMeshParts en modo 5 (lee arm->animations), igual que la lista de vertex groups.
        GroupPropertie* propArmAnim;       // tarjeta "Animation" (clips del esqueleto)
        PropListMeshParts* propListAnims;  // lista de clips de animacion (modo=5)
        PropButton* propBtnRenameAnim;     // "Rename" del clip activo (nombre unico por armature)
        PropButton* propBtnDupAnim;        // "Duplicate" del clip activo (se oculta sin clips)
        PropButtonRow* propRowAnimOps;     // fila Delete | Move Up | Move Down de clips
        // tarjeta "Bones": lista de huesos (modo=6) + nombre editable + padre desplegable + transform del hueso activo
        GroupPropertie* propArmBones;
        PropListMeshParts* propListBones;  // lista de huesos (modo=6)
        PropText* propBoneNombre;          // fila "Name": cuadro editable inline; al confirmar renombra hueso + vertex group
        PropButton* propBoneParent;        // fila "Parent": desplegable (Ninguno + huesos validos); elegir re-parenta
        PropBool* propBoneConectado;       // fila "Connected": el head esta SOLDADO al tail del padre (flag 'conectado').
                                           // Tildarlo MUEVE el hueso para pegarlo; destildarlo = Alt+P > Disconnect Bone.
                                           // Se oculta si el hueso no tiene padre (no tendria sentido).
        // tarjeta "Armature 2D" (huesos 2D del MESH, Mesh::armatures2d): SOLO mientras un editor UV
        // esta en Edit Bones/Pose. Lista de ARMATURES 2D (modo=10, con Add/Rename/Delete: una malla
        // puede tener varios rigs 2D independientes) + lista de huesos DEL ACTIVO (modo=8) + Name
        // inline (renombra hueso + UV group homonimo) + Parent desplegable + Pos de lo seleccionado
        // (hueso entero = centro; solo head o solo tail = esa punta) y, en POSE, la rotacion/escala
        // 2D del hueso activo.
        GroupPropertie* propBones2D;
        PropListMeshParts* propListArm2Ds;  // lista de ARMATURES 2D del mesh (modo=10)
        PropButton* propBtnRenameArm2D;     // renombra el armature 2D activo
        PropListMeshParts* propListBones2D; // lista de huesos 2D del armature ACTIVO (modo=8)
        PropText* propBone2DNombre;
        PropButton* propBone2DParent;
        PropBool* propBone2DConectado;     // fila "Connected" del hueso 2D (mismo flag/semantica que el 3D)
        PropFloat* propBone2DPosX;         // fila "Pos X" del hueso 2D (rest de lo seleccionado, o traslacion en Pose)
        PropFloat* propBone2DRot;          // fila "Rotation" del hueso 2D: SOLO posando (value=NULL la oculta)
        PropButton* propBtnColorMode;   // toggle Per-Vertex / Per-Corner color
        PropText* propRenderPath;    // campo editable "Path" del render (carpeta de salida)
        PropText* propRenderOutput;  // campo editable "File name" del render (solo el nombre + .png)
        PropFloat* propRenderW; PropFloat* propRenderH;         // ancho/alto del render en pixeles (editable)
        PropBool*  propRenderZbuffer; PropBool* propRenderNormal; PropBool* propRenderAlpha; // pases extra
        PropColor* propRenderBg;     // color de fondo del render (global g_renderBg, solo pase Rendered)
        float renderW; float renderH;          // valores del render (default 640 x 480)
        bool  renderZbuffer; bool renderNormal; bool renderAlpha; // pases extra tildados (el beauty siempre)
        PropText* propExportPath;    // campo editable "Path" del export (carpeta de salida)
        PropText* propExportName;    // campo editable "File name" del export (solo el nombre + extension del formato)
        PropButton* propExportFormat;// dropdown de formato de export (OBJ / FBX / glTF / GLB)
        PropButton* propBtnRenameMat; // boton "Rename Material" (se oculta si el material es el por defecto;
                                      // al renombrar se vuelve input via Button::editField)
        PropButton* propBtnRenameUV;    // "Rename" de la UV map activa (tab Vertices)
        PropButton* propBtnRenameColor; // "Rename" de la capa de color activa (tab Vertices)
        PropButton* propBtnRenameGroup; // "Rename" del grupo de vertices activo (tab Vertices)
        PropButton* propBtnRenameUVGroup; // "Rename" del UV group activo (tab Vertices)
        PropButtonRow* propRowUVOps;    // fila Delete | Move Up | Move Down de UV maps (oculta con 1 sola)
        PropButtonRow* propRowColorOps; // fila Delete | Move Up | Move Down de capas de color (oculta con 1 sola)
        PropButtonRow* propRowGroupOps; // fila Delete | Move Up | Move Down de grupos de vertices
        PropButtonRow* propRowUVGroupOps; // fila Delete | Move Up | Move Down de UV groups
        // filas Assign | Remove y Select | Deselect de los DOS grupos de pesos (mismo patron que
        // la fila Assign|Select|Deselect de Mesh Parts). Sin ellas un grupo SOLO se podia armar
        // pintando con el pincel: no habia forma de asignar a mano lo que estaba seleccionado.
        PropButtonRow* propRowGroupAsig;   // Vertex Groups: Assign | Remove
        PropButtonRow* propRowGroupSel;    // Vertex Groups: Select | Deselect
        PropButtonRow* propRowUVGroupAsig; // UV Groups: Assign | Remove
        PropButtonRow* propRowUVGroupSel;  // UV Groups: Select | Deselect
        PropText*   propNameObj;        // campo "Name" del OBJETO activo (tab Objeto): se ve el nombre y se edita al clickear
        int  exportFormat;           // formato de export: 0=OBJ 1=FBX 2=glTF 3=GLB (dropdown propExportFormat)
        bool exportSelectedOnly;     // checkbox "Selected only"
        bool exportApplyModifiers;   // checkbox "Apply Modifiers" (default ON): exporta la malla generada por los mods
        bool exportApplyTransforms;  // checkbox "Apply Transforms" (default ON): hornea el transform en el .obj
        // ultimo activo (para el nombre por defecto). Por SERIAL (Object::serial), no por
        // puntero: la direccion se recicla al borrar y crear, y el campo se quedaba pegado.
        unsigned int exportLastSerial;
        PropButton* propBtnCamTarget;
        PropButton* propBtnInstTarget;
        PropButton* propBtnNewMaterial;
        PropButton* propBtnTextura;
        PropButton* propBtnNormalTex; // selector de la textura del normal map (visible si Normal Mapping ON)
        PropButton* propBtnReflectMode; // dropdown del MODO de Reflection (Matcap/Sphere Map/Equirect; visible si Reflection ON)
        PropButton* propBtnMezcla;      // dropdown del MODO DE MEZCLA (Alpha/Aditivo/Multiply/...; visible si Transparent ON)
        PropButton* propBtnProfundidad; // dropdown del TEST DE PROFUNDIDAD (test + escritura de z, en 4 combinaciones)
        PropButton* propRotMode; // selector del modo de rotacion (Euler/Quat/Axis)
        PropLabel* propMsgDefault; // aviso "material por defecto no editable" (1 label WRAP multilinea)
        PropSeparator* propSepMat; // separador del card Material (se oculta con el material por defecto)

        void ConstruirGrupos(); // arma los grupos (en el constructor)
        void Rebind();          // re-bindea el material del part activo

        int pestaniaActiva;     // 0 = Render/Archivo, 1 = Objeto (transforms), 2 = contextual
                                // (Mesh/Light/Camera/...), 3 = Vertices, 4 = Modifiers,
                                // 5 = Transformar (Edit Mode: Transform Mesh + Transform UV),
                                // 6 = Armature 2D (solo editando/posando los huesos 2D del mesh),
                                // 7 = Constraints (cualquier objeto 3D)
                                // *** ESTOS NUMEROS SON LITERALES EN ~40 LUGARES (y en los tests,
                                //     que indexan BarTabs[1]/[5]/[6]/[7]): una pestania nueva va
                                //     SIEMPRE AL FINAL, nunca intercalada. ***
        // foco del teclado en las PESTAÑAS (no en las propiedades): se entra
        // apretando arriba en la 1ra fila; izq/der cambian de pestaña; abajo/
        // enter vuelve a las propiedades. (navegacion sin mouse, Symbian)
        bool focoEnTabs;
        void CambiarTab(int dir);   // -1/+1 entre las pestañas visibles
        void EntrarPrimerGrupoVisible(); // foco al 1er grupo VISIBLE de la pestaña
        void EntrarUltimoGrupoVisible(); // foco a la ULTIMA propiedad (arriba en las pestañas -> wrap)
        void LimpiarSeleccionGrupos();   // sin nada resaltado (foco en pestañas)
        // setea el rect en pantalla del boton de la fila seleccionada para que
        // su desplegable abra alineado al navegar por TECLADO (el mouse lo hace
        // en ClickEn). Mismo recorrido que CentrarSeleccion.
        void SetRectFilaSeleccionada();
        void ActualizarPestanias(); // visibilidad de tabs/grupos segun el objeto
        void ClickTab(int mx, int my); // click en una pestania de la barra

        Object* target;
        int maxPixelsTitle;

        int selectIndex;
        void NextSelect();
        void PrevSelect();
        void SetOpenGroup(bool open);

        bool editando;
        int ViewportKind() const { return 3; } // (menu de tipo)
        void ClearHover(); // apaga el hover de los botones de fila
        void ResetButtonHovers(); // apaga el hover de TODOS los botones (filas)
        void EnterPropertieSelect();

        void Resize(int newW, int newH) W3D_OVERRIDE;
        void Render() W3D_OVERRIDE;

        void RefreshTargetProperties();
        void RefreshPropMeshParts();
        void CentrarSeleccion(); // centrar la opcion al navegar

        void button_left() W3D_OVERRIDE;
        void button_right() W3D_OVERRIDE;
        void button_up() W3D_OVERRIDE;
        void button_down() W3D_OVERRIDE;
        void Cancel();
        void FindMouseOver(int mx, int my);
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE;
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // touch: arrastrar = scroll vertical
        // tarjeta "Ajustes" (pestania Render): el config.ini editable desde adentro del programa.
        // FUERA del #ifndef: son DATOS de la tarjeta, no metodos de SDL. La tarjeta se arma igual en el telefono
        // -- de hecho ahi es donde mas sirve, que es donde no hay un editor de texto para tocar el .ini a mano.
        GroupPropertie* propAjustes;
        PropButton* propAjIdioma;   // dropdown de idioma
        PropBool*   propAjAntialias;
        PropButton* propAjBackend;  // dropdown del backend grafico
        PropButton* propAjSkin;     // dropdown del skin

#ifndef W3D_SYMBIAN
        void mouse_button_up(int boton) W3D_OVERRIDE;
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
        void event_key_down(int tecla, bool repeticion) W3D_OVERRIDE;
        void event_key_up(int tecla) W3D_OVERRIDE;
#endif
        // click: plegar/desplegar el grupo cuyo titulo este bajo el mouse
        // (compartido; en PC se cablea a mouse_button_up)
        void ClickEn(int mx, int my);

        // touch: (mx,my) cae sobre el VALUE BOX de un PropFloat? -> ahi el arrastre horizontal EDITA (slider);
        // en el label / otras filas el arrastre SCROLLEA. Lo usa el ruteo tactil en controles.cpp.
        bool PuntoEnCampoNumerico(int mx, int my) W3D_OVERRIDE;
        bool TouchSliderArmar(int mx, int my) W3D_OVERRIDE;
        void TouchSliderMover(int dx) W3D_OVERRIDE;
        void TouchSliderSoltar() W3D_OVERRIDE;
        // PropFloat cuyo value box cae bajo (mx,my), o NULL (recorrido de filas comun a lo de arriba)
        PropFloat* PropFloatEnValueBox(int mx, int my);
        // mini-listado (PropListMeshParts) bajo la coordenada py, o NULL (para el scroll tactil de la lista)
        PropListMeshParts* ListaBajoY(int py);

        void key_down_return();
};

// limpia el latch del scroll tactil de listas (lo llama controles.cpp al soltar el dedo). Definida en Properties.cpp.
void PropertiesTouchScrollFin();

#endif