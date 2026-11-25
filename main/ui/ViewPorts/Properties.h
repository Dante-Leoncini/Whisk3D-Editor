#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <vector>
#include "render/RenderOptions.h"   // RenderType / g_redraw: son del editor
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

void DrawTitle(Object* obj, int maxPixels);

// "Load Texture": cada plataforma lo cablea (PC = browser compartido). Carga
// una imagen y la asigna al material 'mat' (async: puede abrir un modal).
class Material;
extern void (*DialogLoadTexture)(Material* mat);
// Crea un material nuevo (Material, Material.001, ...) en el mesh part indicado y lo devuelve. NULL si el indice no
// existe. Lo usa el Add > Reference para dejar el material listo y abrirle el selector de textura enseguida.
Material* NewMaterialInMeshPart(Mesh* mesh, int idx);
// "Load Texture" del normal map: prende este flag y usa el MISMO DialogLoadTexture (browser compartido 4 OS);
// el callback de carga de cada plataforma asigna a mat->normalTexture en vez de mat->texture cuando esta en true.
extern bool gLoadTextureAsNormal;

class Properties;
// el panel de propiedades con el que se INTERACTUO por ultima vez: las
// acciones globales (menus de material/textura, rebind) operan sobre el
extern Properties* PropsActive;
// lleva el panel ACTIVO a la pestania "ARMATURE 2D" (tab 6, huesos 2D del mesh). La llama el editor
// UV al ENTRAR a la edicion de huesos (Tab, menu Add > Armature 2D, selector de Modo): el dueno
// reporto que "no veia" el panel del rig 2D porque vivia mezclado en la pestania de datos del mesh
// y habia que ir a buscarlo. No-op sin panel activo (headless) o si el tab no corresponde.
void PropsGoToArmature2D();
// lleva el panel activo (o el primero del layout) a la pestania "RECURSO" (10): lo que se eligio en
// una vista de recursos del outliner (material -> su tarjeta Material; textura -> vista previa + ruta;
// malla / animset -> datos y usuarios). No-op sin panel o sin recurso activo.
void PropsGoToResource();
// el proyecto se CERRO (RestartScene libero los objetos): cada panel de propiedades suelta lo que
// apuntaba al objeto activo (su 'target' y la lista de partes de la malla) y re-bindea con el proximo
void PropsForgetScene();
class PropImage;

// une los campos "Path" + "File name" de las tarjetas EXPORT y RENDER en la ruta final.
// Es un envoltorio finito de W3dPathInFolder (FileBrowser.h), la UNICA implementacion de
// "carpeta + nombre" del editor: carpeta vacia -> salida por defecto, carpeta RELATIVA ->
// contra el proyecto abierto. 'defaultName' se usa si el campo del nombre esta vacio.
std::string W3dOutputPathFields(const std::string& dir, const std::string& name,
                                const char* defaultName);

class Properties : public ViewportBase, public WithBorder, public Scrollable {
    public:
        Scrollable* AsScrollable() { return this; }
        Properties();
        ~Properties() W3D_OVERRIDE;

        // grupos PROPIOS del panel: cada viewport de propiedades es
        // INDEPENDIENTE (seleccion, scroll de la lista, tamanos). Los
        // nombres pisan a los viejos globales a proposito: los metodos
        // siguen compilando igual.
        std::vector<GroupPropertie*> GroupProperties;
        GroupPropertie* propTransform;
        PropBool* propObjVisible;  // visible en el viewport (animable)
        PropBool* propObjRender;   // sale en el render final (animable)
        PropBool* propObjStatic;   // ESTATICO vs DINAMICO (culling por grilla): true=celda cacheada / false=por-frame
        PropBool* propObjProjShadow;  // SOMBRAS EN TIEMPO REAL: proyecta (tapa la luz) / recibe. Solo en las mallas
        PropBool* propObjRecShadow;
        PropBool* propObjBounces;      // LA LUZ PRECALCULADA: rebota la luz (la indirecta) / captador de sombras (solo las recibe)
        PropBool* propObjPicker;
        PropBool* propObjInProbes;    // lo ven las sondas (de reflejo y de luz): apagado, la sonda que va adentro no lo ve
        PropBool* propObjRelLines;  // dibujar la linea al padre. Va en la tarjeta GENERICA:
                                    // es una opcion UNIVERSAL de todos los objetos.
        // tarjeta "Animacion" del OBJETO (pestania Objeto): animaciones del objeto
        // (contenedores; hoy vertex anims de la malla) — lista + New + Delete
        GroupPropertie* propObjAnim;
        PropListMeshParts* propListObjAnims;
        // la BIBLIOTECA de CLIPS DE JERARQUIA del objeto (animation/W3dAnimSet.h): el desplegable "Clips"
        // de la tarjeta Animacion (ninguna / una del proyecto / una nueva)
        PropButton* propHierLib;
        // el RETARGET PROPIO del objeto (animRetarget de lua, guardado en su nodo): un armature, el de sus clips de
        // esqueleto (Complete / Rotations only); cualquier otro, el de sus clips de jerarquia (+ "Clip default")
        PropButton* propObjRetarget;
        GroupPropertie* propMeshParts; // tarjeta: selector de parte + gestion (New/Assign/Select/.../Rename)
        GroupPropertie* propMaterial;  // tarjeta APARTE: el material del mesh part seleccionado
        PropButtonRow* propRowPartOps; // fila: Assign | Select | Deselect
        PropButtonRow* propRowDelRow;  // fila: Delete | Rename (Delete oculto si hay 1 sola parte)
        PropButtonRow* propRowPartMove;// fila: Move Up | Move Down (oculta si hay 1 sola parte; orden de dibujado)
        // props del material como MEMBERS (Rebind los setea por nombre, NO por indice -> reordenar la
        // tarjeta no rompe nada): 8 checkboxes + 3 colores + shininess.
        PropBool*  propMatChk[11]; // Filtering, Transparent, VertexColor, Lighting, Repeat, Culling, DepthTest, Smooth, Chrome, Equirect360, NormalMapping
        // ---- CALCOMANIAS / MEZCLA / PROFUNDIDAD -----------------------------------
        // Hasta hoy `decal`, `mezcla`, `sesgoProfundidad` y `ordenPasada` solo se podian
        // poner desde el .mtl o el .w3d: en el editor no habia forma de ver como se porta
        // una sombra pegada al piso sin salir a editar un archivo de texto. Ahora estan
        // en la tarjeta Material: DOS DESPLEGABLES (mezcla y test de profundidad) + el
        // sesgo, el orden de pasada y un tilde "Decal" que aplica la receta entera.
        PropFloat* propMatBias;    // sesgoProfundidad (glPolygonOffset; negativo = hacia el ojo)
        PropFloat* propMatBiasM;   // sesgoMetros (sesgo de profundidad en metros: + aleja, - acerca)
        PropButton* propMatOrder;  // EL PASE del material (desplegable): opacos / calcomanias / transparentes
        PropBool*  propMatLines;         // LINEAS: dibujar las aristas de la malla con el material
        PropFloat* propMatThicknessLine; // grosor de esas lineas en px (glLineWidth); visible con Lines ON
        PropFloat* propMatAlpha;         // ALPHA TEST: Material::alphaTest 0..1 (0 = apagado; clave "alfaCorte" del .w3d)
        PropButton* propBtnTexB;         // MEZCLA DE TEXTURAS: la textura B (Material::textureB, desplegable como el normal map)
        PropFloat*  propMatMixB;         // ... y cuanto de B (Material::mixB 0..1; clave "mixB" del .w3d)
        GroupPropertie* propLight;
        PropBool* propLightIgnOrder;   // "Ignorar orden del arbol" (pre-pase) vs solo lo que sigue en el arbol   // pestania de luz: TODAS las propiedades editables de la luz GL
        // tarjeta del elemento TEXTO 2D (se edita en el Editor 2D)
        GroupPropertie* propText2D;
        PropText*   propT2dName;   // nombre del objeto (arriba de todo; el tab Objeto no se muestra)
        PropFloat*  propT2dPosX;   // posicion (X/Y en el lienzo, Z = profundidad)
        PropFloat*  propT2dPosY;
        PropFloat*  propT2dPosZ;
        PropFloat*  propT2dOpac;      // opacidad 0..1 (multiplica a la de los padres)
        PropBool*   propT2dPosAbs;    // posicion en PIXELES (off = relativa al tamano de la UI)
        PropFloat*  propT2dWeight;    // peso en el reparto de filas/columnas del padre
        PropButton* propT2dLines;     // desplegable: una linea / por palabras / cualquier parte
        PropBool*   propT2dAutoSize;  // ajustar el tamano de fuente al area disponible
        PropText*   propT2dText;      // que dice ("Texto" por defecto)
        PropButton* propT2dType;      // desplegable: string / number / float
        PropFloat*  propT2dDec;       // decimales del float (0 = no se ven)
        PropFloat*  propT2dSize;      // alto de fuente, en px del lienzo
        PropButton* propT2dAlignH;    // desplegable: izquierda / centro / derecha
        PropButton* propT2dAlignV;    // desplegable: arriba / centro / abajo
        PropColor*  propT2dColor;     // swatch -> ColorPicker
        PropButton* propT2dPal;       // desplegable: color propio o de la paleta
        PropButton* propT2dFont;      // desplegable: Whisk3D / cargar un .ttf
        PropButton* propT2dAnchor;    // desplegable: desde donde se mide la posicion (ancla)
        PropFloat*  propT2dRot;       // rotacion en grados
        // tarjeta del elemento IMAGEN 2D (una textura con modo de ajuste)
        GroupPropertie* propImage2D;
        PropButton* propImgTexture;  // elegir el archivo de imagen (file browser)
        PropFloat*  propImgWidth;    // el rectangulo del elemento, en px de lienzo
        PropFloat*  propImgHeight;
        PropFloat*  propImgRot;      // rotacion en grados
        PropButton* propImgMode;     // desplegable: estirar / ajustar / cover
        PropButton* propImgAnchor;   // desplegable de ancla (mismo esquema que el texto)
        PropText*   propImgName;     // nombre del objeto (arriba de todo)
        PropFloat*  propImgPosX;
        PropFloat*  propImgPosY;
        PropFloat*  propImgPosZ;
        PropFloat*  propImgOpac;     // opacidad 0..1
        PropBool*   propImgPosAbs;   // posicion en pixeles (off = relativa)
        PropFloat*  propImgWeight;
        PropButton* propImgUnit;    // desplegable: unidad del ancho/alto (fraccion / px / escalado)
        PropColor*  propImgColor;   // tinte de la textura
        PropButton* propImgPal;
        PropBool*   propImgAlpha;    // usar el canal alpha de la textura
        PropButton* propImgMix;      // modo de mezcla (Alpha / Aditiva / ...)
        PropBool*   propImgFilter;   // filtrado de textura (off = pixel-perfect)
        // tarjeta del elemento RECTANGULO 2D (color solido o transparente: acomoda hijos)
        GroupPropertie* propRect2D;
        PropText*   propRectName;
        PropFloat*  propRectPosX;
        PropFloat*  propRectPosY;
        PropFloat*  propRectPosZ;
        PropBool*   propRectPosAbs;
        PropFloat*  propRectWidth;
        PropFloat*  propRectHeight;
        PropFloat*  propRectRot;
        PropButton* propRectAnchor;
        PropFloat*  propRectOpac;
        PropColor*  propRectColor;
        PropButton* propRectPal;
        PropFloat*  propRectWeight;
        PropButton* propRectUnit;  // desplegable: unidad del tamano
        // tarjeta del elemento CONTENEDOR (rectangulo invisible: solo ordena a sus hijos)
        GroupPropertie* propCont2D;
        PropText*   propContName;
        PropFloat*  propContPosX;
        PropFloat*  propContPosY;
        PropFloat*  propContPosZ;
        PropBool*   propContPosAbs;
        PropFloat*  propContWeight;
        PropFloat*  propContWidth;
        PropFloat*  propContHeight;
        PropFloat*  propContRot;
        PropButton* propContAnchor;
        PropFloat*  propContOpac;
        PropButton* propContUnit;  // desplegable: unidad del tamano
        // tarjeta del elemento SLICE 9 (imagen con bordes fijos)
        GroupPropertie* propS9card;
        PropText*   propS9Name;
        PropFloat*  propS9PosX;
        PropFloat*  propS9PosY;
        PropFloat*  propS9PosZ;
        PropBool*   propS9PosAbs;
        PropFloat*  propS9Weight;
        PropButton* propS9Texture;
        PropFloat*  propS9Width;
        PropFloat*  propS9Height;
        PropButton* propS9Unit;       // desplegable: unidad del tamano
        PropFloat*  propS9BorderX;    // grosor del borde EN la textura (px del archivo), por eje
        PropFloat*  propS9BorderY;
        PropFloat*  propS9EscBorder;  // grosor dibujado = borde * esto
        PropFloat*  propS9Rot;
        PropButton* propS9Anchor;
        PropFloat*  propS9Opac;
        // tarjeta del BOTON 2D
        GroupPropertie* propBtn2D;
        PropText*   propBtnName;
        PropFloat*  propBtnPosX;
        PropFloat*  propBtnPosY;
        PropFloat*  propBtnPosZ;
        PropBool*   propBtnPosAbs;
        PropFloat*  propBtnWeight;
        PropText*   propBtnText;
        PropButton* propBtnIcon;    // elegir el png del icono
        PropFloat*  propBtnSize;
        PropFloat*  propBtnPad;
        PropButton* propBtnAnchor;
        PropFloat*  propBtnRot;
        PropFloat*  propBtnOpac;
        PropColor*  propBtnColBackground;
        PropColor*  propBtnColText;
        PropColor*  propBtnColBorder;
        PropColor*  propBtnColHover;      // borde de mouse-over (verde accent por defecto)
        PropButton* propBtnPalBackground; // desplegable: color propio o uno de la paleta
        PropButton* propBtnPalText;
        PropButton* propBtnPalBorder;
        PropButton* propBtnPalHover;
        PropButton* propBtnTex;      // textura de fondo (9 pedazos) opcional
        PropFloat*  propBtnTexBX;
        PropFloat*  propBtnTexBY;
        PropFloat*  propBtnTexEsc;
        // tarjeta del VIDEO 2D
        GroupPropertie* propVid2D;
        PropText*   propVidName;
        PropFloat*  propVidPosX;
        PropFloat*  propVidPosY;
        PropFloat*  propVidPosZ;
        PropBool*   propVidPosAbs;
        PropFloat*  propVidWeight;
        PropButton* propVidFile;  // elegir el .mp4/.webm/.gif
        PropFloat*  propVidWidth;
        PropFloat*  propVidHeight;
        PropButton* propVidUnit;   // desplegable: unidad del tamano
        PropButton* propVidMode;   // estirar / ajustar / cover
        PropBool*   propVidLoop;
        PropBool*   propVidAlpha;    // usar la transparencia del video
        PropBool*   propVidPlay;     // ver la animacion en el editor
        PropBool*   propVidFilter;
        PropButton* propVidAnchor;
        PropFloat*  propVidRot;
        PropFloat*  propVidOpac;
        // tarjeta MARGEN (del elemento 2D activo): aire alrededor en filas/columnas +
        // el checkbox "expandir" (absorbe el sobrante como el elemento Expandir)
        GroupPropertie* propMargin;
        PropBool*   propMarginExp;    // expandirse en la fila/columna
        PropBool*   propMarginUni;    // un solo valor para los 4 lados
        PropFloat*  propMarginAll;    // el valor unico (con uniforme prendido)
        PropFloat*  propMarginLeft;   // por lado (con uniforme apagado)
        PropFloat*  propMarginRight;
        PropFloat*  propMarginTop;
        PropFloat*  propMarginBottom;
        // tarjeta del EXPANDIR
        GroupPropertie* propExp2D;
        PropText*   propExpName;
        PropFloat*  propExpWeight;
        PropColor*  propS9Color;     // tinte del arte (blanco = tal cual)
        PropButton* propS9Pal;
        PropBool*   propS9Filter;    // filtrado de textura (off = pixel-perfect)
        // tarjeta del objeto UI (la raiz de la interfaz)
        GroupPropertie* propUIcard;
        PropBool*   propUIShow3D;     // ver los elementos con su profundidad en la escena 3D
        PropBool*   propUIEqualRender; // lienzo = tamano del render (default); apagado = responsive
        PropBool*   propUIInTree;      // la UI se dibuja en su lugar del outliner (UI::inTree)
        PropFloat*  propUIWidth;     // el lienzo propio (solo en responsive)
        PropFloat*  propUIHeight;
        PropButton* propUIres;      // desplegable de resoluciones (4k .. 240p)
        PropButton* propUIAspect;   // desplegable de aspecto (16:9 / 4:3 / 1:1)
        PropButton* propUIRotate;   // boton: intercambia ancho y alto
        PropColor*  propUIcolor;    // fondo de la ventana (transparente por defecto)
        PropFloat*  propUIScale;    // escala global del contenido (x1 N95 ... x4 pantallas grandes)
        PropBool*   propUIScaleEqual; // escala = GlobalScale del editor (por plataforma); tildado oculta el valor manual
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
        int scriptSignature;             // rebuild de las tarjetas al cambiar de script/objeto
        // tarjeta PALETAS (del PROYECTO, pestania 0): filas dinamicas
        // nombre+color por entrada; la gestion respeta los invariantes
        // (misma cantidad en todas; borrar corrige referencias)
        GroupPropertie* propPalette;
        int paletteRows;              // firma de lo construido (rebuild al cambiar)
        PropButton* propPaletteSel;   // desplegable: cual paleta EDITA la tarjeta / crear / borrar
        PropText*   propPaletteName; // nombre editable de la paleta en edicion (renombra en vivo)
        // tarjeta "Paleta" del OBJETO (cualquier objeto, 3D o 2D): la seleccion
        // heredable ("Igual que el padre" o una paleta del proyecto por nombre)
        GroupPropertie* propPaletteObj;
        PropButton* propPaletteObjSel;
        PropText*   propUIName;    // nombre del objeto UI
        PropFloat*  propUIopac;    // opacidad de la interfaz ENTERA
        // tarjeta "Children": lo que afecta a los HIJOS del seleccionado (padding = encoge el
        // area donde se enganchan sus anclas; la linea se ve en el Editor 2D con el UI elegido)
        GroupPropertie* propChildren;
        PropBool*   propChildrenPadUni;  // un solo valor de padding para los 4 lados
        PropFloat*  propChildrenPadAll;  // el valor unico (con uniforme prendido)
        PropFloat*  propChildrenPadLeft; // el padding, POR LADO
        PropFloat*  propChildrenPadRight;
        PropFloat*  propChildrenPadTop;
        PropFloat*  propChildrenPadBottom;
        PropButton* propChildrenLayout; // desplegable: libremente / filas / columnas
        PropFloat*  propChildrenGap;    // espacio entre hijos (solo con filas/columnas)
        PropBool*   propChildrenPx;     // padding y gap en px (default) o proporcionales
        PropButton* propChildrenFit; // desplegable: estirar (100%) / minimo (tamano natural)
        PropButton* propChildrenAlign;  // desplegable: inicio / centro / fin (con ajuste minimo)
        PropButton* propChildrenDistrib;// desplegable: gap / space-between / space-around /
                                     // space-evenly (reparto css; solo con ajuste minimo)
        PropBool*   propChildrenClipX;  // overflow: recortar lo que se sale, por eje
        PropBool*   propChildrenClipY;
        PropBool*   propChildrenScroll; // permitir scrollear el contenido recortado
        PropFloat*  propChildrenScrollX;
        PropFloat*  propChildrenScrollY;
        PropButton* propLightType;    // Tipo: Punto / Sol / Foco (direccional + cono)
        PropFloat*  propLightGL;      // numero de GL light (0..7, entero)
        PropColor*  propLightDiffuse; // color difuso
        PropColor*  propLightAmbient; // color ambiente
        PropColor*  propLightSpecular;// color especular
        PropFloat*  propLightAttC;    // atenuacion constante / lineal / cuadratica
        PropFloat*  propLightAttL;
        PropFloat*  propLightAttQ;
        PropFloat*  propLightIntens;   // la intensidad (multiplica el color)
        PropFloat*  propLightDist;     // la distancia en metros (hasta donde alumbra; el largo del cono)
        PropFloat*  propLightSpotCut;  // spot: angulo del cono
        PropFloat*  propLightSpotExp;  // spot: concentracion del haz
        PropFloat*  propLightRTRadius; // trazado de rayos: tamano de la lampara (0 = puntual, sombra dura)
        PropFloat*  propLightRTRays;   // trazado de rayos: rayos de sombra hacia esta lampara (0 = global)
        GroupPropertie* propCamera;  // pestania de camara: lente (fov/orto) + target (look-at)
        PropBool*   propActiveCamChk; // "Camara activa": la misma pista que la tarjeta Camara activa (animable)
        PropBool*   propCamOrtho;    // proyeccion ortografica vs perspectiva
        PropFloat*  propCamFov;      // campo de vision (grados), animable (AnimFov)
        PropFloat*  propCamNear;     // distancia minima de dibujado, animable (AnimClip/X)
        PropFloat*  propCamFar;      // distancia maxima de dibujado, animable (AnimClip/Y)
        // PROFUNDIDAD DE CAMPO de la camara (Camera::dof): el tilde, la apertura, la distancia de foco (animable:
        // AnimCamFocus) y el objeto al que sigue el foco (desplegable; si hay uno, la distancia se esconde)
        PropBool* propCamDof; PropFloat* propCamFStop; PropFloat* propCamFocus; PropButton* propBtnCamFocusObj;
        GroupPropertie* propInstance;// pestania de instance/array/mirror: target
        // pestania del objeto LOD: umbrales de distancia como texto "20, 45, 90"
        // (commit en vivo, mismo patron que el texto del elemento 2D)
        GroupPropertie* propLOD;
        PropText*   propLodDist;
        PropBool*   propLodOnlyCam;   // medir desde la camara ACTIVA (igual que el Culling)
        // pestania del objeto Culling UNIFICADO: selector de metodo + campos comunes + campos del metodo Grid
        GroupPropertie* propCulling;
        PropButton* propCullMethod;    // dropdown: Frustum / Grid / Triangulo / BSP
        PropBool*   propCullActive;    // interruptor del recorte (demo A/B en vivo)
        PropBool*   propCullOnlyCam;   // medir desde la camara ACTIVA (calc una vez para todas las vistas)
        PropFloat*  propCullDistMax;   // culling por distancia (0 = sin limite)
        PropBool*   propCullOrderAlpha;// translucido: ordena back->forward (alpha)
        PropBool*   propCullOrderNear;// opaco: forward->back puro (sin agrupar por material)
        PropFloat*  propCullCellSize;  // metodo Grid: lado de la celda (unidades de mundo)
        PropBool*   propCullMode3D;    // metodo Grid: grilla 3D (default 2D en XZ)
        PropButton* propCullRecalc;    // BOTON "Recalcular": rearma la grilla del metodo Grid
        struct W3dPanels* panels;   // LAS TARJETAS DECLARADAS (ui/panels/PanelsText.cpp): Niebla, Vacio, ...
        // SONIDO (objects/Sound.h): el archivo, como suena, desde cuando (segundos), el recorte, el bucle y el 3D
        GroupPropertie* propSound;
        PropButton* propSoundFile;    // desplegable: los sonidos del proyecto
        PropBool*   propSoundActive;
        PropFloat*  propSoundVol;         // animable (AnimSound X)
        PropFloat*  propSoundTone;        // animable (AnimSound Y): pitch
        PropFloat*  propSoundReverb;      // animable (AnimSound Z): envio a la reverb
        PropFloat*  propSoundPan;         // solo sin 3D
        PropFloat*  propSoundStart;       // segundos de la escena
        PropFloat*  propSoundRecStart;    // recorte del principio (s)
        PropFloat*  propSoundRecEnd;      // recorte del final (s)
        PropBool*   propSoundLoop;        // se repite hasta el infinito
        PropBool*   propSoundPos;         // sonido 3D
        PropFloat*  propSoundRadius;      // 3D: hasta aca entero
        PropFloat*  propSoundRange;       // 3D: desde aca no se oye
        // objetos ClearZ / Recorte (composicion de la pantalla en el arbol)
        GroupPropertie* propClip;
        PropBool*   propRcActive;
        PropFloat*  propRcX;
        PropFloat*  propRcY;
        PropFloat*  propRcWidth;
        PropFloat*  propRcHeight;
        PropButton* propRcCamera;      // dropdown: (ninguna) + las camaras de la escena
        PropBool*   propRcClearZ;
        PropBool*   propRcBackground;
        PropColor*  propRcColor;
        // tarjeta RUTINA (objects/Routine.h): PropsRoutineBuild / PropsRoutineUpdate (ViewPorts/PropsRoutine.cpp)
        GroupPropertie* propRoutine;
        PropButton* propRuList;             // "Render mode": la lista que se edita (All / Solid / ...)
        PropFloat*  propRuRepeat;           // BUCLE DE RUTINA: cuantas veces corre la lista (1 = una rutina comun)
        PropButton* propRuUse;              // que lista usa ese modo (desplegable)
        PropListMeshParts* propRuSteps;     // los pasos (modo 14)
        PropButtonRow* propRuRowAdd;        // Add (desplegable agrupado) | Remove
        PropButton* propRuRef;              // la malla / textura / objeto / rutina (desplegable por carpetas)
        PropButton* propRuRef2;             // "Test de visibilidad": el objeto que mueve la caja (opcional)
        PropText*   propRuText;             // "Llamar Lua": el nombre de la funcion
        PropLabel*  propRuInfo[5];          // en filas: triangulos / indices / y lo que APUNTAN los punteros en ese paso
        PropLabel*  propRuNotice;           // primitiva simulada / el formato del array
        PropColor*  propRuColor;            // los pasos de color: el selector de color de Whisk3D
        PropBool*   propRuColorNum;         // ...o los 4 numeros (para usar memorias)
        PropButton* propRuLight;            // GL_LIGHT0..7 (desplegable)
        PropButton* propRuOption;           // la opcion del paso: modo de niebla, funcion z... (desplegable)
        PropBool*   propRuOn;
        PropBool*   propRuManual;            // Draw mesh: rango MANUAL de triangulos
        PropText*   propRuArray;             // Draw mesh sin rango manual: el array (memoria) o nada (las partes)
        PropText*   propRuInteger;           // la parte / los pasos a saltear
        PropText*   propRuNum[5];
        PropButtonRow* propRuMax;           // Draw mesh manual: el ultimo triangulo = el ultimo de la malla
        PropBool*   propRuBuffer[3];        // Clear: que buffers limpia (color, profundidad, estencil)
        PropLabel*  propRuReason;           // por que esta en rojo
        // tarjeta HITBOX (objects/Hitbox.h): la arman, la bindean y sincronizan sus textos
        // PropsHitboxBuild / PropsHitboxUpdate (ViewPorts/PropsHitbox.cpp)
        GroupPropertie* propHitbox;
        // LA SONDA DE LUZ (ViewPorts/PropsLightProbe.cpp): tamano, puntos, rayos, "Bake" y el estado
        GroupPropertie* propProbe;
        // PRECALCULAR LA ILUMINACION (ViewPorts/PropsBake.cpp): debajo de la tarjeta Material
        GroupPropertie* propBake;
        PropButton* propBakeTex;
        PropButton* propBakeUV;
        PropFloat*  propBakeSamples;
        PropBool*   propBakeIndirect;
        PropFloat*  propBakeBounces;
        PropBool*   propBakeDenoiser;
        PropButton* propBakeButton;
        PropButton* propBakeBar;
        PropButton* propBakeCancel;      // Cancelar (la cruz), abajo de la barra mientras hornea
        PropLabel*  propBakeNoticeTex;   // la textura elegida ya tiene la luz de OTRO objeto (hornear ahi pisa lo que se cruce)
        PropLabel*  propBakeNotice;
        PropFloat*  propSlSize[3];
        PropFloat*  propSlPoints[3];
        PropFloat*  propSlRays;
        PropButton* propSlBake;
        PropButton* propSlBar;        // el progreso mientras hornea (como el horneado de la luz)
        PropButton* propSlCancel;     // Cancelar, abajo de la barra
        PropLabel*  propSlState;
        // LA SONDA DE REFLEJO (PropsReflectionProbe.cpp, objects/ReflectionProbe.h)
        GroupPropertie* propProbeRef;
        PropButton* propSrType; PropFloat* propSrImportance; PropFloat* propSrIntensity; PropBool* propSrBox;
        PropFloat* propSrSize[3]; PropFloat* propSrOrigin[3]; PropButton* propSrRes;
        PropFloat* propSrNear; PropFloat* propSrEach; PropButton* propSrImage;
        PropFloat* propSrSamples; PropFloat* propSrBounces;   // (la horneada: el trazado de rayos)
        PropButton* propSrBake; PropLabel* propSrState;
        PropButton* propSrBar; PropButton* propSrCancel;   // el progreso mientras hornea y Cancelar
        // LAS SOMBRAS EN TIEMPO REAL (ViewPorts/PropsShadows.cpp): debajo de la tarjeta Render
        GroupPropertie* propShadows;
        PropBool*   propShadowOn;
        PropFloat*  propShadowDist;
        PropFloat*  propShadowCascades;
        PropFloat*  propShadowCut[3];
        PropFloat*  propShadowBorder;
        PropFloat*  propShadowBlur;
        PropButton* propShadowRes;
        PropBool*   propShadowSoft;
        PropFloat*  propShadowInt;
        PropColor*  propShadowColor;   // el color de la sombra
        PropFloat*  propShadowBias;
        PropLabel*  propShadowState;
        PropBool*   propHbActive;
        PropFloat*  propHbSize[3];       // ancho X / alto Y / largo Z (locales)
        PropFloat*  propHbCenter[3];
        PropText*   propHbLabel;     // commit en vivo (patron LOD: sync por frame)
        PropText*   propHbFilter;
        PropBool*   propHbBodies;      // detectar cuerpos rigidos
        // tarjeta INSTANCIA DE PREFAB (objects/InstancePrefab.h): la arman y la bindean PropsPrefabBuild /
        // PropsPrefabUpdate (ViewPorts/PropsPrefab.cpp). En una instancia: su prefab (desplegable, acepta un
        // prefab arrastrado de la biblioteca), Edit Prefab, Unpack y Reset Overrides; en un objeto GENERADO por
        // una: de que instancia es y "Select Instance"
        GroupPropertie* propPrefab;
        PropButton* propPfSel;
        PropLabel*  propPfInfo;
        PropButton* propPfEdit;
        PropButton* propPfUnpack;
        PropButton* propPfReset;
        PropButton* propPfInstance;
        // (la misma tarjeta en un PROXY W3D, objects/ProxyW3d.h: la LIBRERIA -desplegable con las vinculadas y
        //  "Add Library..."-; el elemento va en propPfSel)
        PropButton* propPxLib;
        // (y el STREAMING de la instancia o el proxy, io/Streaming.h: COMO se carga -desplegable siempre / por
        //  distancia-, a que DISTANCIA, contra que OBJETIVO, y la VISTA PREVIA del streaming en el editor -opcion del
        //  proyecto-. Con Ctrl+Z: PropsPrefab.cpp los anota al soltar el mouse / dejar de tipear)
        PropButton* propPfLoad;
        PropFloat*  propPfDistance;
        PropText*   propPfTarget;
        PropBool*   propPfView;
        // pestania de la Collection: orden de dibujo para transparentes
        // pestania del objeto Mirror (pedido del dueno: "el mirror no tiene
        // propiedades"): target por NOMBRE + ejes espejados + limites del rect
        GroupPropertie* propMirror;
        PropText*   propMirrorTarget;   // nombre del objeto a espejar (patron LOD: sync por frame)
        PropBool*   propMirrorX;        // ejes que se niegan (X/Y/Z)
        PropBool*   propMirrorY;
        PropBool*   propMirrorZ;
        PropBool*   propMirrorChildren;  // reflejar tambien los hijos del target
        PropBool*   propMirrorLimits;    // recortar al rectangulo del plano (limU/V)
        PropFloat*  propMirrorU0;
        PropFloat*  propMirrorU1;
        PropFloat*  propMirrorV0;
        PropFloat*  propMirrorV1;
        // tarjeta FISICA (cuerpo rigido, physics/W3dRigid.h): aparece para
        // CUALQUIER objeto con definicion (Add > Physics). Los numeros bindean
        // DIRECTO a la definicion; el tipo es un boton que CICLA
        // (Dynamic -> Static -> Character) y hay boton para quitarla.
        GroupPropertie* propPhysics;
        PropButton* propPhysType;
        PropFloat*  propPhysMass;
        PropFloat*  propPhysBoxX;
        PropFloat*  propPhysBoxY;
        PropFloat*  propPhysBoxZ;
        PropFloat*  propPhysCenterX;
        PropFloat*  propPhysCenterY;
        PropFloat*  propPhysCenterZ;
        PropFloat*  propPhysFriction;
        PropFloat*  propPhysBounce;
        PropButton* propPhysRemove;
        // pestania del objeto Particulas: el emisor (textura + config del cono).
        // Los numeros/checks bindean DIRECTO a los campos del activo; los dos de
        // TEXTO (textura y color "r, g, b, a") van con el patron del LOD (sync por frame).
        GroupPropertie* propParticles;
        PropButton* propPartTexture;    // dropdown de texturas cargadas + "Load Texture" (file browser), como el material
        PropFloat*  propPartCount;   // particulas/seg (0 = solo rafagas emitir())
        PropFloat*  propPartLife;       // segundos
        PropButton* propPartMix;     // dropdown del modo de mezcla (Normal/Aditiva/Substractiva/... del motor)
        PropColor*  propPartColor;      // tinte + alpha: swatch -> ColorPicker de Whisk3D (bindea a Particles::color[4])
        PropBool*   propPartFade; // alpha -> 0 con la vida
        PropBool*   propPartActive;     // false = no emite
        PropBool*   propPartFiltering;   // textura LINEAR (suave) vs NEAREST (pixelada)
        // ---- particulas AVANZADAS: tarjetas aparte (Movimiento / Tamanio / Color / Rotacion / Forma) ----
        GroupPropertie* propPartColCard;   // Color y alfa
        GroupPropertie* propPartShapeCard; // Forma y flipbook
        PropFloat* propPartAlphaDecays;
        PropFloat* propPartAlphaDeath;
        PropFloat* propPartAppear;
        PropFloat* propPartFadeNear[2];
        PropFloat* propPartFadeFar[2];
        PropBool*  propPartUseColEnd;
        PropColor* propPartEndColor;
        PropButton* propPartShape;       // dropdown Billboard / Estirada / Linea
        PropFloat* propPartStretch;
        PropFloat* propPartTrailPts;
        PropFloat* propPartTrailStep;
        PropFloat* propPartThickness;
        PropFloat* propPartFlipC;
        PropFloat* propPartFlipCols;
        PropFloat* propPartFlipRows;
        PropFloat* propPartFlipFps;
        PropBool*  propPartFlipOne;
        // espejos float de los campos int (PropFloat bindea float*): se copian del objeto al rebind y
        // vuelven al objeto (redondeados) en el onChange
        float partFlipC, partFlipCols, partFlipRows, partTrailPts;
        // tarjeta ARCHIVO (pestania Render, arriba de todo): abrir/guardar el
        // proyecto .w3d (v3: JSON plano) con ruta/nombre. Los assets del proyecto son
        // SIEMPRE archivos externos en la carpeta (el checkbox "Empaquetar
        // assets" viejo se elimino; empaquetar es del juego COMPILADO).
        GroupPropertie* propFile;
        PropText*   propProjFolder;  // carpeta del proyecto
        PropText*   propProjName;    // nombre.w3d
        PropButton* propNewProj;  // "Nuevo proyecto": la escena de arranque, sin archivo (NewProjectNow)
        PropButton* propProjOpen;
        PropButton* propProjSave;
        PropButton* propProjVersion; // "Guardar version vN" (guardado por versiones; N real segun versions/)
        PropButton* propProjAs;
        // (extraer los assets y el formato de las mallas ya no son del proyecto: son de cada recurso, en su tarjeta
        // Recurso -propRecExtract, propRecMeshText-)
        PropBool*   propProjNet;    // "Network access" (tarjeta JUEGO): el permiso de la red de este archivo (W3dNetPermissions.h)
        GroupPropertie* propRender;  // pestania RENDER: tarjeta "Render" (output)
        GroupPropertie* propAnimation; // pestania ANIMACION (9): tarjeta "Animation" (selector + New/Delete)
        // ===== pestania ANIMACION (9): el MIX de animaciones (capas estilo Maya) =====
        GroupPropertie* propMix;          // tarjeta "Mix": rango + el arbol objetos/escenas -> capas + Add/Remove/Up/Down
        PropListMeshParts* propListMix;   // el arbol (PropListMeshParts modo 13)
        PropButtonRow* propRowMix;        // Agregar | Quitar
        PropButtonRow* propRowMixMove;    // Subir | Bajar
        PropFloat* propMixStart;          // el rango del mix (g_mixStart / g_mixEnd)
        PropFloat* propMixEnd;
        GroupPropertie* propLayer;        // tarjeta "Capa": la capa ELEGIDA en el arbol
        PropButton* propLayerAnim;        // que animacion suena en la capa
        PropFloat*  propLayerInfl;        // influencia %
        PropButton* propLayerMode;        // Mezclar / Sumar / Restar
        PropButton* propLayerBone;        // mascara (solo capas de armature)
        PropFloat*  propLayerFrom;        // frame del mix en que arranca
        PropFloat*  propLayerVel;         // velocidad
        PropBool*   propLayerLoop;
        PropBool*   propLayerVisible;      // el ojo
        // tarjeta RENDER: el rango PROPIO de la secuencia (0 = el del timeline)
        PropFloat* propRenderStart;
        PropFloat* propRenderEnd;
        PropFloat* propRenderFps;
        GroupPropertie* propKeyframe;  // tarjeta "Keyframe": el keyframe elegido en el editor de curvas, con numeros exactos
        // tarjeta "Curva" (pestania Animacion, entre Animacion y Keyframe): las curvas elegidas en el timeline (las que
        // tienen keyframes elegidos): interpolacion y handles de TODOS sus keyframes + el Repetir. Con varias, lo que
        // no coincide se ve "-" y lo que se elige va a todas
        GroupPropertie* propCurve;
        // las filas de las tarjetas Keyframe / Curva / Modificadores de curva de ESTE panel (las acciones y el refresco
        // trabajan sobre unos punteros estaticos que se re-enlazan al panel que se refresca o se usa: con dos paneles,
        // o uno que se libera, no quedan apuntando a filas de otro)
        struct CurveRowsUI* curveRows;
        // pestania Modifiers: los MODIFICADORES DE LA CURVA activa (Cycles / Noise / Stepped / Smooth): la pila y el
        // elegido (como los del objeto). El titulo dice de que curva son ("X Location")
        GroupPropertie* propCurveMods;
        GroupPropertie* propCurveModProps;
        PropListMeshParts* propListCurveMods;
        // tarjeta "Camara activa" (pestania Animacion, animacion de ESCENA): elegir la camara activa + insertar un
        // CORTE (keyframe de la pista SceneAnimation::camTrack) en el frame actual. Los cortes se mueven/borran en el
        // dope sheet (fila "Camara activa", con una camara seleccionada).
        GroupPropertie* propCamActive;
        PropButton* propActiveCamSel;   // dropdown con las camaras de la escena
        PropButton* propActiveCamKey;   // inserta el corte en el frame actual
        PropButton* propBtnAnimSel;    // dropdown: animacion ACTIVA (Scene(s) / clips del armature seleccionado)
        PropButtonRow* propRowAnimNewDel; // fila: New | Delete (Delete oculto si no hay nada que borrar)
        PropButton* propBtnAnimRename; // "Rename" de la animacion activa (escena o clip)
        PropButton* propClipRetarget;  // el retarget por defecto del CLIP DE JERARQUIA elegido (Complete / Rotations only)
        PropButton* propBtnAnimRender; // "Render Animation" (gris si no hay animaciones)
        // CONFIG del flipbook activo (kind 5): en la card Animation, ocultos salvo con flipbook activo
        PropButton* propFlipAtlas;    // dropdown del atlas (textura) del flipbook
        PropFloat*  propFlipFrames;   // cantidad de celdas del ciclo
        PropFloat*  propFlipCols;     // columnas de la grilla del atlas
        PropFloat*  propFlipRows;     // filas de la grilla
        PropFloat*  propFlipFps;      // velocidad (cuadros/seg)
        // tarjeta "Juego" (debajo de Animacion): compilar + cache del viaje en el tiempo
        GroupPropertie* propGame;
        PropButton* propGamePlat;      // desplegable: Linux .deb / AppImage / WebGL / ... / las consolas
        PropButton* propGameBackend;   // desplegable: el backend grafico (los que tenga la plataforma elegida)
        PropButton* propGameStart;     // desplegable: con que escena o juego arranca el juego compilado
        PropButton* propGameVentMode;  // desplegable: Ventana / Pantalla completa / Sin bordes
        PropButton* propGameOrient;    // desplegable: Todas / Solo vertical / Solo horizontal
        PropButton* propGameIcon;      // elegir el PNG (maxima definicion) del icono del juego
        PropButton* propGameAssets;    // desplegable: Sueltos (editables) / Empaquetados (protegidos)
        PropButton* propGameUID;       // genera/muestra el UID3 de Symbian del juego (app propia)
        PropBool*   propGamePhysics;   // checkbox: compilar CON el motor de fisica (default si)
        PropBool*   propGameSound;     // checkbox: compilar CON el audio (default si)
        PropBool*   propGameDebug;     // checkbox: modo debug W3D_DEV_LOG=1 (default NO = produccion)
        PropButton* propGameCompile;
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
        // --- props del modificador MIRROR (se bindean al Modifier activo en UpdateTabs) ---
        PropBool*  propModShowViewport; PropBool* propModShowEdit; // display en viewport / en edit mode (todos los mods)
        PropLabel* propModEmpty;   // "(no properties yet)" para tipos sin params todavia
        PropBool*  propMirX; PropBool* propMirY; PropBool* propMirZ; // ejes
        PropButton* propMirTarget; // "Mirror Object" (dropdown: cualquier objeto)
        // ARRAY: copias, desplazamiento relativo / constante (X/Y/Z de la UI), objeto de desplazamiento y soldar
        PropFloat* propArrCount;
        PropBool*  propArrRelOn; PropFloat* propArrRelX; PropFloat* propArrRelY; PropFloat* propArrRelZ;
        PropBool*  propArrConstOn; PropFloat* propArrConstX; PropFloat* propArrConstY; PropFloat* propArrConstZ;
        PropButton* propArrTarget;
        PropBool*  propArrMerge; PropFloat* propArrMergeDist;
        // Boolean: la operacion (fila Intersect | Union | Difference, la activa en verde) + el objeto
        PropButtonRow* propBoolOp;
        PropButton*    propBoolTarget;
        PropButton* propArmTarget; // "Target" del modificador Armature (dropdown: solo esqueletos)
        PropButton* propBtnOptVG;  // "Optimize Vertex Groups" (1 hueso/vertice) del modificador Armature (destructivo, con confirm)
        PropBool*  propMirMerge; PropFloat* propMirDist; PropBool* propMirClip; // merge + distancia + clipping
        // Subdivision Surface: modo (Catmull-Clark/Simple) + niveles viewport/render
        PropBool*  propSubSimple; PropFloat* propSubLevel; PropFloat* propSubRender;
        // Screw: angle, screw(height), steps viewport/render, eje (dropdown X/Y/Z), stretch U/V
        PropFloat* propScrewAngle; PropFloat* propScrewHeight; PropFloat* propScrewSteps; PropFloat* propScrewRender;
        PropButton* propScrewAxis; PropBool* propScrewStretchU; PropBool* propScrewStretchV;
        PropBool* propScrewSmooth; PropBool* propScrewMerge; PropBool* propScrewFlip; // suave / soldar / invertir normales
        // Culling (PVS por triangulo): metodo (desplegable Triangulos/BSP) + Recalcular + info del sidecar
        PropButton* propPvsMethod;
        PropButton* propPvsRecalc;
        PropLabel*  propPvsInfo;
        GroupPropertie* propMeshEdit;      // card "Edicion" (pestania Mesh): interruptor editable/no-editable
        PropButton* propBtnMeshEditable;   // "Borrar datos para edicion" / "Convertir en mesh editable"
        PropBool* propMeshLockNormals; // casilla "Bloquear normales" (Mesh::lockNormals)
        // card "3D Mesh" (la PRIMERA de la pestania "Malla 3D", la 3): que malla 3D (un RECURSO de la
        // biblioteca, objects/MeshResource.h) usa el objeto. SOLO se elige: el nombre y la carpeta del
        // recurso se editan en la biblioteca (el outliner)
        GroupPropertie* propMesh3D;
        PropButton* propBtnMeshSel;        // desplegable: que malla del proyecto usa el objeto (+ New Copy); '*' = sin guardar
        PropLabel*  propLblMeshUses;       // cuantos objetos de la escena la usan
        PropButton* propBtnMeshUnique;     // Make Single User (solo si la usan varios)
        // una malla de una LIBRERIA externa (o lo que genera un proxy) es de SOLO LECTURA: el aviso de la tarjeta
        // "3D Mesh" (y el de la de modificadores) y los botones que la editan, que se ocultan (las listas se ven)
        PropLabel*  propLblMeshLib;       // "It belongs to a library (read-only)..." (pestania 3)
        PropLabel*  propLblModsLib;       // idem en la tarjeta Modifiers (lo que genera un proxy)
        PropButton* propBtnNewPart;       // "New Mesh Part"
        PropButton* propBtnAddVG;         // "Add Vertex Group"
        PropButton* propBtnAddUVG;        // "Add UV Group"
        PropButton* propBtnAddUV;         // "Add UV Map"
        PropButton* propBtnAddCol;        // "Add Color Layer"
        PropButton* propPvsPath;       // dropdown: el PATH del recorrido (Curve / malla de aristas)
        PropBool*   propPvsOnlyCam;    // OFF = el nodo sigue la vista libre del viewport (demo A/B)
        PropButton* propPvsBranches;   // dropdown-toggles: que RAMAS del path participan del nearest
        PropFloat*  propPvsNode;       // "Nodo actual" (0 = malla completa; jugando lo pisa el motor)
        PropButton* propBtnApplyMod; // "Apply Modifier": hornea la malla generada en malla real editable
        // ===== pestania "Constraints" (tab 7, la ULTIMA): 2 tarjetas, calcadas de Modifiers =====
        // El stack NO es de la malla: lo tiene cualquier objeto 3D (una luz, una camara, una
        // coleccion). Por eso la lista va por PropListMeshParts::obj (modo 11) y NUNCA por 'mesh'.
        GroupPropertie* propConstraints;       // tarjeta 1: la lista + Add/Remove + Move Up/Down
        PropListMeshParts* propListConstraints;// selector del stack (modo 11: Object::constraints)
        PropButtonRow* propRowConstraint;             // fila Add | Remove (Remove oculto si no hay ninguno)
        PropButtonRow* propRowConstraintMove;         // fila Move Up | Move Down (oculta si hay < 2)
        GroupPropertie* propConstraintProps;   // tarjeta 2: props del constraint SELECCIONADO
        PropBool*   propConstraintActive;
        PropBool*   propConstraintShowEdit;           // "Show in Edit Mode" del constraint (default OFF)      // "Enabled" (el ojito de la lista)
        PropButton* propConstraintSource;       // "Source": desplegable de objetos + la opcion "la vista"
        PropLabel*  propConstraintNoticeSource; // aviso: sin fuente el constraint no hace nada
        PropFloat*  propConstraintInfluence;    // "Influence" 0..100 con "%"
        PropBool*   propConstraintAxisX;        // "Copy X/Y/Z" (Copy Location / Copy Rotation)
        PropBool*   propConstraintAxisY;
        PropBool*   propConstraintAxisZ;
        PropLabel*  propConstraintNoticeAxes;   // aviso: sin ningun eje tildado el constraint no hace nada
        PropButton* propConstraintBBMode;       // "Mode" del billboard (tipo arbol / mira a la camara)
        PropLabel*  propConstraintNoticeBB;     // aviso: el giro de 180 con influencia intermedia
        PropButton* propConstraintBone;         // Child Of: "Bone" (desplegable de huesos si la fuente es un armature)
        PropBool*   propConstraintCoLoc;        // Child Of: hereda Location / Rotation / Scale de la fuente
        PropBool*   propConstraintCoRot;
        PropBool*   propConstraintCoEsc;
        PropButtonRow* propRowConstraintInverse; // Child Of: Set Inverse | Clear Inverse
        PropListMeshParts* propListUV;           // lista de UV maps (modo=1)
        PropListMeshParts* propListColor;        // lista de capas de color (modo=2)
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
        PropText* propBoneName;            // fila "Name": cuadro editable inline; al confirmar renombra hueso + vertex group
        PropButton* propBoneParent;        // fila "Parent": desplegable (Ninguno + huesos validos); elegir re-parenta
        PropBool* propBoneConnected;       // fila "Connected": el head esta SOLDADO al tail del padre (flag 'conectado').
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
        PropText* propBone2DName;
        PropButton* propBone2DParent;
        PropBool* propBone2DConnected;     // fila "Connected" del hueso 2D (mismo flag/semantica que el 3D)
        // Pos/Rotation/Scale del hueso 2D: filas DE ESTE PANEL. Antes eran punteros estaticos del
        // .cpp que apuntaban a las filas del ULTIMO panel construido: al liberarse ese panel quedaban
        // colgando y UpdateTabs de otro panel escribia sobre filas ya liberadas (y sus propias
        // filas nunca se refrescaban). Cada panel refresca las suyas.
        PropFloat* propBone2DPosX;         // fila "Pos X" del hueso 2D (rest de lo seleccionado, o traslacion en Pose)
        PropFloat* propBone2DPosY;         // fila "Pos Y" (idem Pos X)
        PropFloat* propBone2DRot;          // fila "Rotation" del hueso 2D: SOLO posando (value=NULL la oculta)
        PropFloat* propBone2DSclX;         // fila "Scale X": SOLO posando
        PropFloat* propBone2DSclY;         // fila "Scale Y": SOLO posando
        PropButton* propBtnColorMode;   // toggle Per-Vertex / Per-Corner color
        PropText* propRenderPath;    // campo editable "Path" del render (carpeta de salida)
        PropText* propRenderOutput;  // campo editable "File name" del render (solo el nombre + .png)
        PropFloat* propRenderW; PropFloat* propRenderH;         // ancho/alto del render en pixeles (editable)
        PropBool*  propRenderZbuffer; PropBool* propRenderNormal; PropBool* propRenderAlpha; // pases extra
        PropButton* propRenderBg;    // el FONDO del render: color o textura 360 (el boton de la derecha cambia)
        PropFloat*  propRenderBgRot[3];          // la rotacion de la textura 360 del fondo (grados)
        PropFloat*  propRenderLightBackground;   // LA LUZ DEL FONDO: cuanto ilumina (0 a 200%: g_renderLightBackground)
        // (las OPCIONES del render no son del panel: son de g_render, render/RenderOptions.h. Las filas las editan)
        PropButton* propRenderFormat; PropFloat* propRenderQuality;
        PropBool* propRenderFinal; PropBool* propRenderFinalAlpha; PropBool* propRenderPost;
        PropBool* propRenderSound; PropButton* propRenderAudioFmt; PropFloat* propRenderAudioKbps;   // el audio
        PropButton* propRenderVideoCodec; PropFloat* propRenderVideoKbps;   // el video: codec y bitrate
        PropButton* propRenderAntialias;   // el antialiasing del render rasterizado (sin trazado de rayos)
        unsigned renderSeenGen;   // la g_renderGen de las filas de la tarjeta Render (otra = se abrio un proyecto: rearmarlas)
        // VECTORES DE MOVIMIENTO (el pase _vectores) y el DESENFOQUE DE MOVIMIENTO del beauty (render/MotionVectors.h)
        PropBool* propRenderVectors; PropBool* propRenderBlur; PropFloat* propRenderShutter;
        PropBool*  propRT;           // "Ray Tracing": el pase Rendered se traza por CPU (render/RayTracer)
        PropFloat* propRTRays; PropFloat* propRTSamples; PropFloat* propRTPasses; // solo visibles con Ray Tracing tildado
        PropFloat* propRTLayers;  // las capas transparentes que atraviesa un rayo
        PropFloat* propRTBounces; // los REBOTES de la luz (luz indirecta): 0 = solo la directa
        PropButton* propRTGpu;
        PropBool* propRTOnlyFinal;   // ver solo la imagen terminada (g_rt.onlyFinal)
        PropBool* propRTSeed;        // otro ruido en cada imagen (g_rt.seedVariable)
        PropBool* propRTBlur; PropFloat* propRTShutter; PropButton* propRTObtPos;   // el desenfoque trazado (g_rt.blur)
        // tarjeta "Post Processing" (pestania Render, debajo de Render): la pila de efectos (render/PostProcess.h), como
        // los modificadores: la lista + Add|Remove + Move Up|Move Down y los parametros del ELEGIDO
        GroupPropertie* propPost;
        PropBool* propPostOn; PropLabel* propPostCPU; PropListMeshParts* propListPost;
        PropButtonRow* propRowPost; PropButtonRow* propRowPostMove;
        PropBool* propPostActive;
        PropFloat* propPostGrainInt; PropFloat* propPostGrainSize; PropBool* propPostGrainColor;
        PropFloat* propPostFocusDist; PropFloat* propPostFocusRange; PropFloat* propPostFocusBlur; PropBool* propPostFocusCam;
        PropFloat* propPostMovObt;
        PropFloat* propPostBloom[6]; PropColor* propPostBloomColor; PropBool* propPostFastBloom;   // (BLOOM: v[0..5])
        PropButton* propRTUpscaler;  // COMO se agranda (g_rt.upscaler: lineal o FSR 1); solo con el reescalado puesto
        PropButton* propRTScale;     // el reescalado del viewport (g_rt.scale: Off, x2, x3, x4)       // DONDE se traza: CPU o la GPU (render/RTGpu.h); solo si hay una GPU con Vulkan
        PropButton* propRTDenoiser;  // el DENOISER (desplegable: los que hay en esta maquina; render/Denoiser.h)
        float rtRays, rtSamples, rtPasses, rtLayers, rtBounces;     // espejos float de g_rt (los PropFloat bindean float*)
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
        PropButtonRow* propRowGroupAssign;   // Vertex Groups: Assign | Remove
        PropButtonRow* propRowGroupSel;      // Vertex Groups: Select | Deselect
        PropButtonRow* propRowUVGroupAssign; // UV Groups: Assign | Remove
        PropButtonRow* propRowUVGroupSel;    // UV Groups: Select | Deselect
        PropText*   propNameObj;        // campo "Name" del OBJETO activo (tab Objeto): se ve el nombre y se edita al clickear
        int  exportFormat;           // formato de export: 0=OBJ 1=FBX 2=glTF 3=GLB 4=STL (dropdown propExportFormat)
        bool exportSelectedOnly;     // checkbox "Selected only"
        bool exportApplyModifiers;   // checkbox "Apply Modifiers" (default ON): exporta la malla generada por los mods
        bool exportApplyTransforms;  // checkbox "Apply Transforms" (default ON): hornea el transform en el .obj
        // ultimo activo (para el nombre por defecto). Por SERIAL (Object::serial), no por
        // puntero: la direccion se recicla al borrar y crear, y el campo se quedaba pegado.
        unsigned int exportLastSerial;
        PropButton* propBtnCamTarget;
        PropButton* propBtnInstTarget;
        PropFloat* propInstCopies;        // cuantas veces dibuja un Array a su target (Instance::count)
        PropButton* propBtnTexture;
        PropButton* propBtnNormalTex; // selector de la textura del normal map (visible si Normal Mapping ON)
        // LOS MAPAS del trazado de rayos / PBR (W3dMatMap EMISION..ALFA; [0] y [1] = NULL: son propBtnTexture y
        // propBtnNormalTex). [nombre] [numero/textura] [UV] [la textura o el numero]. Solo con el trazado de rayos
        PropButton* propMap[8];
        PropFloat*  propMatIOR;      // indice de refraccion (trazado de rayos)
        PropFloat*  propMatTrans;    // TRANSMISION 0..1 (trazado de rayos): lo que es vidrio
        PropBool*   propMatSSR;      // REFLEJOS EN PANTALLA (luz por pixel)
        // SHADER ESCRITO (objects/WrittenShader.h): el tilde, el archivo, Compile (+ el lapiz: editar en el IDE) y el resultado
        PropBool*   propMatShaderOn;
        PropButton* propBtnShader;
        PropButton* propBtnShaderComp;
        PropLabel*  propMatShaderMsg;
        PropButton* propBtnReflectMode; // dropdown del MODO de Reflection (Matcap/Sphere Map/Equirect; visible si Reflection ON)
        PropButton* propBtnShading;
        PropBool*   propMatLightInd;      // "Indirect light": la sonda de luz por vertice en el pipeline fijo (Material::lightIndirect)   // dropdown del SOMBREADO: por vertice / por pixel (solo con shaders y Lighting ON)
        // ---- CAPAS DE TEXTURA (TexLayer): hasta 4 filas de [textura | UV | mezcla | on]. La fila que sigue a la
        //      ultima capa es la de "Add Layer" (elegir una textura ahi crea la capa); "No Texture" la borra. ----
        PropLabel*  propLayersTitle;
        PropListMeshParts* propListLayers;   // las capas de textura, en una lista (como las partes de malla)
        PropListMeshParts* propListLayersMat;   // LAS CAPAS DE MATERIAL del mesh part (arriba de la tarjeta Material)
        PropButton* propLayerMatAdd;            // "New Material" (+): una capa nueva con un material nuevo
        PropButton* propLayerMatChange;         // "Change Material": el material de la capa elegida
        PropButton* propLayerAdd;            // "Add Layer": elegir una textura crea la capa
        PropButton* propLayerTex[4];
        PropButton* propLayerUV[4];
        PropButton* propLayerMix[4];
        PropFloat*  propLayerForce[4];  // el FACTOR de la mezcla de la capa elegida (0 a 100%: TexLayer::fuerza)
        PropBool*   propLayerOn[4];
        PropButton* propBtnMix;   // dropdown del MODO DE MEZCLA (Alpha/Aditivo/Multiply/...; visible si Transparent ON)
        PropButton* propBtnDepth; // dropdown del TEST DE PROFUNDIDAD (test + escritura de z, en 4 combinaciones)
        PropButton* propRotMode; // selector del modo de rotacion (Euler/Quat/Axis)
        PropLabel* propMsgDefault; // aviso "material por defecto no editable" (1 label WRAP multilinea)
        // ---- el RECURSO ACTIVO de la BIBLIOTECA (el outliner): se muestra en la pestania "Malla 3D" (la 3,
        //      sin pestania aparte). Su nombre y su carpeta se editan aca (con undo); sus DATOS POR TIPO
        //      (resolucion/alfa/formato/bytes de una textura, frecuencia/canales/duracion de un sonido...),
        //      dentro/fuera del .w3d, sus acciones; un material ademas usa la tarjeta Material de siempre,
        //      bindeada al material elegido en vez del de un mesh part.
        GroupPropertie* propResource;
        PropText*   propRecName;      // Name: renombrar el recurso (lo ven todos sus usuarios)
        PropText*   propRecFolder;    // Folder: su carpeta cosmetica ("" = la raiz)
        PropLabel*  propRecType;      // (ya no se muestra: los datos van en cajas -Usuarios, Tipo, Guardado-)
        PropText*   propRecUsers;     // Usuarios | N (0 = en ROJO: nadie lo usa)
        PropText*   propRecBoxType;   // Tipo | Textura (y "solo lectura" si es de una libreria)
        PropText*   propRecSaved;     // Guardado | Si (guardado en el proyecto o en su archivo)
        PropButton* propRecSave;      // Guardado | [Guardar] (en vez de la caja, cuando tiene cambios sin guardar)
        PropText*   propRecEntry;     // su entrada del contenedor (una caja de solo lectura: se selecciona y se copia)
        std::vector<PropText*> propRecData;    // los DATOS del tipo, una fila cada uno (Resolucion | 256 x 256), de solo lectura
        PropButton* propRecLocation; // desplegable: "Inside the .w3d" / "External" (los que tienen archivo)
        PropBool*   propRecMeshText; // (malla 3D) "Mesh as text": se guarda .w3dm (texto) o .w3db (binario)
        PropButton* propRecSaveAs;   // "Save As...": una COPIA de su archivo afuera (nombre y carpeta); el recurso no cambia
        PropButton* propRecExtract;    // (ya no se muestra: lo hace Guardar como)
        PropImage* propRecPreview;     // vista previa de una textura
        PropButtonRow* propRecActions; // (ya no se muestra: Seleccionar usuarios y Borrar van en su fila cada uno)
        PropButton* propRecSelUsers;   // "Select Users" (el icono de los usuarios): los objetos que lo usan
        PropButton* propRecDelete;     // "Delete" (el tacho; en uso: avisa cuantos lo usan)
        PropButtonRow* propRecActions2; // sonido: Play | Stop; escena/prefab: Open; script: Open in IDE
        unsigned    recVersionBind;    // W3dActiveResourceVersion con la que se bindeo la tarjeta
        PropSeparator* propSepMat; // separador del card Material (se oculta con el material por defecto)

        void BuildGroups(); // arma los grupos (en el constructor)
        void Rebind();      // re-bindea el material del part activo

        int activeTab;     // 0 = Render/Archivo, 1 = Objeto (transforms), 2 = contextual
                                // (Mesh/Light/Camera/...), 3 = Vertices, 4 = Modifiers,
                                // 5 = Transformar (Edit Mode: Transform Mesh + Transform UV),
                                // 6 = Armature 2D (solo editando/posando los huesos 2D del mesh),
                                // 7 = Constraints (cualquier objeto 3D)
                                // 8 = Scripts, 9 = Animacion (selector + MIX de capas).
                                // La 3 es la pestania "Malla 3D": la malla del objeto, o el RECURSO
                                // elegido en la biblioteca del outliner (no hay pestania aparte)
                                // *** ESTOS NUMEROS SON LITERALES EN ~40 LUGARES (y en los tests,
                                //     que indexan BarTabs[1]/[5]/[6]/[7]): una pestania nueva va
                                //     SIEMPRE AL FINAL, nunca intercalada. ***
        // foco del teclado en las PESTAÑAS (no en las propiedades): se entra
        // apretando arriba en la 1ra fila; izq/der cambian de pestaña; abajo/
        // enter vuelve a las propiedades. (navegacion sin mouse, Symbian)
        bool focusInTabs;
        void ChangeTab(int dir);   // -1/+1 entre las pestañas visibles
        void EnterFirstGroupVisible(); // foco al 1er grupo VISIBLE de la pestaña
        void ResumeSelection();        // volver al panel (tecla verde): la fila donde estaba, si sigue a la vista
        void EnterLastGroupVisible();  // foco a la ULTIMA propiedad (arriba en las pestañas -> wrap)
        void ClearSelectionGroups();   // sin nada resaltado (foco en pestañas)
        // setea el rect en pantalla del boton de la fila seleccionada para que
        // su desplegable abra alineado al navegar por TECLADO (el mouse lo hace
        // en ClickIn). Mismo recorrido que CenterSelection.
        void SetRectRowSelected();
        void UpdateTabs(); // visibilidad de tabs/grupos segun el objeto
        // pestanias GLOBALES (no dependen del objeto seleccionado): 0 Render/Archivo y 9 Animacion; y la 3 con un
        // RECURSO de la biblioteca elegido (su tarjeta vive ahi, y elegirlo suelta la escena: no hay objeto activo)
        bool TabGlobal() const;
        void ClickTab(int mx, int my); // click en una pestania de la barra

        Object* target;
        int maxPixelsTitle;

        int selectIndex;
        void NextSelect();
        void PrevSelect();
        void SetOpenGroup(bool open);

        bool editing;
        int ViewportKind() const { return 3; } // (menu de tipo)
        void ClearHover(); // apaga el hover de los botones de fila
        void ResetButtonHovers(); // apaga el hover de TODOS los botones (filas)
        void EnterPropertieSelect();

        void Resize(int newW, int newH) W3D_OVERRIDE;
        void Render() W3D_OVERRIDE;

        void RefreshTargetProperties();
        void RefreshPropMeshParts();
        void CenterSelection(); // centrar la opcion al navegar

        void button_left() W3D_OVERRIDE;
        void button_right() W3D_OVERRIDE;
        void button_up() W3D_OVERRIDE;
        void button_down() W3D_OVERRIDE;
        void Cancel();
        void FindMouseOver(int mx, int my);
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE;
        bool NoWrapCursor() const W3D_OVERRIDE;   // (arrastrando la barra de un mapa del material: Properties.cpp)
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // touch: arrastrar = scroll vertical
        // tarjeta "Ajustes" (pestania Render): el config.start editable desde adentro del programa.
        // FUERA del #ifndef: son DATOS de la tarjeta, no metodos de SDL. La tarjeta se arma igual en el telefono
        // -- de hecho ahi es donde mas sirve, que es donde no hay un editor de texto para tocar el .ini a mano.
        GroupPropertie* propSettings;
        PropButton* propSettingsLanguage;   // dropdown de idioma
        PropBool*   propSettingsAntialias;
        PropButton* propSettingsBackend;  // dropdown del backend grafico
        PropButton* propSettingsSkin;     // dropdown del skin
        PropButton* propSettingsLogs;      // nivel de los logs del editor (todo / avisos / errores / nada)
        PropText*   propSettingsRepo;     // raiz del repo para Compilar (editor instalado)
        PropText*   propSettingsDevice; // el nombre de este dispositivo en la red (el que ven al buscar partidas)

        void mouse_button_up(int button) W3D_OVERRIDE;   // (Symbian: solo con el dedo)
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
        void event_key_down(int keyCode, bool repeat) W3D_OVERRIDE;
        void event_key_up(int key) W3D_OVERRIDE;
        // click: plegar/desplegar el grupo cuyo titulo este bajo el mouse
        // (compartido; en PC se cablea a mouse_button_up)
        void ClickIn(int mx, int my);
        // la fila bajo (mx,my) (NULL = cabecera / hueco / afuera) y su MENU CONTEXTUAL (mantener apretado o click
        // derecho): restablecer / copiar / pegar y, si es animable, los keyframes. false = ahi no hay una fila de valor
        PropertieBase* PropInPoint(int mx, int my);
        // (menuX/menuY: donde abrir el menu, si no es en el punto: la pulsacion larga lo abre arriba del dedo)
        bool MenuContext(int mx, int my, int menuX = -1, int menuY = -1);
        // el MISMO menu, de la fila elegida con el TECLADO (el OK mantenido del N95): en el medio de esa fila
        bool MenuContextKeyboard();
        // LAS LISTAS CON EL TECLADO (el N95): la lista de la fila elegida (con su y de arriba en pantalla y su tarjeta),
        // el menu de SU fila elegida (derecha / el OK mantenido) y renombrarla (dos OK seguidos)
        PropListMeshParts* ListKeyboard(int* yTop, GroupPropertie** group);
        bool ListMenuKeyboard();
        bool ListRenameKeyboard();

        // touch: (mx,my) cae sobre el VALUE BOX de un PropFloat? -> ahi el arrastre horizontal EDITA (slider);
        // en el label / otras filas el arrastre SCROLLEA. Lo usa el ruteo tactil en controls.cpp.
        bool PointInFieldNumeric(int mx, int my) W3D_OVERRIDE;
        bool TouchSliderBuild(int mx, int my) W3D_OVERRIDE;
        void TouchSliderMove(int dx) W3D_OVERRIDE;
        void TouchSliderRelease() W3D_OVERRIDE;
        // PropFloat cuyo value box cae bajo (mx,my), o NULL (recorrido de filas comun a lo de arriba)
        PropFloat* PropFloatInValueBox(int mx, int my);
        int MapNumberIn(int mx, int my);   // el numero de un mapa del material bajo el punto (su k, o -1)
        // mini-listado (PropListMeshParts) bajo la coordenada py, o NULL (para el scroll tactil de la lista)
        PropListMeshParts* ListUnderY(int py);
        // las LISTAS con operaciones (tacho, menu de la fila, renombrar con doble click, arrastrar): Properties.cpp
        PropListMeshParts* ListIn(int py, int* yTop, int* hRow, GroupPropertie** group);
        bool DoubleClick(int mx, int my);
        bool ListHasRow(int mx, int my);
        bool ListGrab(int mx, int my);
        void ListGrabRelease(int mx, int my, int menuX, int menuY);

        void key_down_return();
};

// limpia el latch del scroll tactil de listas (lo llama controls.cpp al soltar el dedo). Definida en Properties.cpp.
void PropertiesTouchScrollEnd();

#endif