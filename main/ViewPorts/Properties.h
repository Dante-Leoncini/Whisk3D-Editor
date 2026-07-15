#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <vector>
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
// "Load Texture" del normal map: prende este flag y usa el MISMO DialogoCargarTextura (browser compartido 4 OS);
// el callback de carga de cada plataforma asigna a mat->normalTexture en vez de mat->texture cuando esta en true.
extern bool gCargarTexturaComoNormal;

class Properties;
// el panel de propiedades con el que se INTERACTUO por ultima vez: las
// acciones globales (menus de material/textura, rebind) operan sobre el
extern Properties* PropsActivo;

class Properties : public ViewportBase, public WithBorder, public Scrollable {
    public:
        Properties();
        ~Properties() override;

        // grupos PROPIOS del panel: cada viewport de propiedades es
        // INDEPENDIENTE (seleccion, scroll de la lista, tamanos). Los
        // nombres pisan a los viejos globales a proposito: los metodos
        // siguen compilando igual.
        std::vector<GroupPropertie*> GroupProperties;
        GroupPropertie* propTransform;
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
        GroupPropertie* propLight;   // pestania de luz: TODAS las propiedades editables de la luz GL
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
        GroupPropertie* propCamera;  // pestania de camara: target (look-at)
        GroupPropertie* propInstance;// pestania de instance/array/mirror: target
        GroupPropertie* propRender;  // pestania RENDER: tarjeta "Render" (output)
        GroupPropertie* propAnimation; // pestania RENDER: tarjeta "Animation" (selector + New/Delete + Render Animation)
        PropButton* propBtnAnimSel;    // dropdown: animacion ACTIVA (Scene(s) / clips del armature seleccionado)
        PropButtonRow* propRowAnimNewDel; // fila: New | Delete (Delete oculto si no hay nada que borrar)
        PropButton* propBtnAnimRename; // "Rename" de la animacion activa (escena o clip)
        PropButton* propBtnAnimRender; // "Render Animation" (gris si no hay animaciones)
        GroupPropertie* propExport;  // pestania RENDER: tarjeta "Export" (.obj)
        // pestania VERTICES (icono mesh): 3 tarjetas. Las listas REUSAN PropListMeshParts (modo 1/2).
        GroupPropertie* propEditItem;   // EDIT MODE: tarjeta "Transform" con X/Y/Z del centro (pivote) de la seleccion
        float editPosX, editPosY, editPosZ; // posicion del centro de la seleccion (convencion Z-up del panel; se
                                            // recalcula cada frame; editarla traslada rigido lo seleccionado)
        GroupPropertie* propUVMaps;     // tarjeta "UV Maps" (lista de UV maps)
        GroupPropertie* propColorLayers;// tarjeta "Color" (lista de capas + modo per-vertex/corner)
        GroupPropertie* propVertexGroups;// tarjeta "Vertex Groups" (huesos del rig: pelvis, spine_01, ...)
        GroupPropertie* propVertexAnim; // tarjeta "Vertex Animation"
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
        PropButton* propBtnApplyMod; // "Apply Modifier": hornea la malla generada en malla real editable
        PropListMeshParts* propListUV;    // lista de UV maps (modo=1)
        PropListMeshParts* propListColor; // lista de capas de color (modo=2)
        PropListMeshParts* propListVertGroups; // lista de grupos de vertices / huesos (modo=4)
        // pestania ARMATURE (icono esqueleto): tarjeta "Animation" (lista de clips + Add / Rename / Delete / Move).
        // REUSA PropListMeshParts en modo 5 (lee arm->animations), igual que la lista de vertex groups.
        GroupPropertie* propArmAnim;       // tarjeta "Animation" (clips del esqueleto)
        PropListMeshParts* propListAnims;  // lista de clips de animacion (modo=5)
        PropButton* propBtnRenameAnim;     // "Rename" del clip activo (nombre unico por armature)
        PropButton* propBtnDupAnim;        // "Duplicate" del clip activo (se oculta sin clips)
        PropButtonRow* propRowAnimOps;     // fila Delete | Move Up | Move Down de clips
        // tarjeta "Bones" (Pose Mode): lista de huesos (modo=6) + parent + transform (pos/rot/scale) del hueso activo
        GroupPropertie* propArmBones;
        PropListMeshParts* propListBones;  // lista de huesos (modo=6)
        PropText* propBoneParent;          // hueso padre (solo lectura)
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
        PropButtonRow* propRowUVOps;    // fila Delete | Move Up | Move Down de UV maps (oculta con 1 sola)
        PropButtonRow* propRowColorOps; // fila Delete | Move Up | Move Down de capas de color (oculta con 1 sola)
        PropButtonRow* propRowGroupOps; // fila Delete | Move Up | Move Down de grupos de vertices
        PropText*   propNameObj;        // campo "Name" del OBJETO activo (tab Objeto): se ve el nombre y se edita al clickear
        int  exportFormat;           // formato de export: 0=OBJ 1=FBX 2=glTF 3=GLB (dropdown propExportFormat)
        bool exportSelectedOnly;     // checkbox "Selected only"
        bool exportApplyModifiers;   // checkbox "Apply Modifiers" (default ON): exporta la malla generada por los mods
        bool exportApplyTransforms;  // checkbox "Apply Transforms" (default ON): hornea el transform en el .obj
        Object* exportLastObj;       // ultimo activo (para el nombre por defecto)
        PropButton* propBtnCamTarget;
        PropButton* propBtnInstTarget;
        PropButton* propBtnNewMaterial;
        PropButton* propBtnTextura;
        PropButton* propBtnNormalTex; // selector de la textura del normal map (visible si Normal Mapping ON)
        PropButton* propBtnReflectMode; // dropdown del MODO de Reflection (Matcap/Sphere Map/Equirect; visible si Reflection ON)
        PropButton* propRotMode; // selector del modo de rotacion (Euler/Quat/Axis)
        PropLabel* propMsgDefault; // aviso "material por defecto no editable" (1 label WRAP multilinea)
        PropSeparator* propSepMat; // separador del card Material (se oculta con el material por defecto)

        void ConstruirGrupos(); // arma los grupos (en el constructor)
        void Rebind();          // re-bindea el material del part activo

        int pestaniaActiva;     // 0 = Objeto (transforms), 1 = Mesh (mesh parts)
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

        void Resize(int newW, int newH) override;
        void Render() override;

        void RefreshTargetProperties();
        void RefreshPropMeshParts();
        void CentrarSeleccion(); // centrar la opcion al navegar

        void button_left() override;
        void button_right() override;
        void button_up() override;
        void button_down() override;
        void Cancel();
        void FindMouseOver(int mx, int my);
        void event_mouse_motion(int mx, int my) override;
        bool event_finger_scroll(int px, int py, int dx, int dy) override; // touch: arrastrar = scroll vertical
#ifndef W3D_SYMBIAN
        void mouse_button_up(SDL_Event &e) override;
        void event_mouse_wheel(SDL_Event &e) override;
        void event_key_down(SDL_Event &e) override;
        void event_key_up(SDL_Event &e) override;
#endif
        // click: plegar/desplegar el grupo cuyo titulo este bajo el mouse
        // (compartido; en PC se cablea a mouse_button_up)
        void ClickEn(int mx, int my);

        // touch: (mx,my) cae sobre el VALUE BOX de un PropFloat? -> ahi el arrastre horizontal EDITA (slider);
        // en el label / otras filas el arrastre SCROLLEA. Lo usa el ruteo tactil en controles.cpp.
        bool PuntoEnCampoNumerico(int mx, int my) override;
        bool TouchSliderArmar(int mx, int my) override;
        void TouchSliderMover(int dx) override;
        void TouchSliderSoltar() override;
        // PropFloat cuyo value box cae bajo (mx,my), o NULL (recorrido de filas comun a lo de arriba)
        PropFloat* PropFloatEnValueBox(int mx, int my);
        // mini-listado (PropListMeshParts) bajo la coordenada py, o NULL (para el scroll tactil de la lista)
        PropListMeshParts* ListaBajoY(int py);

        void key_down_return();
};

// limpia el latch del scroll tactil de listas (lo llama controles.cpp al soltar el dedo). Definida en Properties.cpp.
void PropertiesTouchScrollFin();

#endif