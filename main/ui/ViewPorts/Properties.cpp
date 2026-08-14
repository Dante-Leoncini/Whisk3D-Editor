#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dNombres.h"   // LA regla de nombres unicos (compartida por todo el editor)
#include "variables.h"
#include "io/GuardarW3D.h"   // tarjeta Archivo: guardar el proyecto (.w3d v3: JSON plano + archivos externos)
#include "io/GuardarVersion.h" // tarjeta Archivo: "Guardar version vN" (guardado por versiones)
#ifndef W3D_SYMBIAN
#include <filesystem>   // listar los skins de res/Skins (la tarjeta Ajustes)
#endif
#include "W3dLang.h"
#include "objects/Texto2D.h"   // panel del elemento de texto del Editor 2D
#include "objects/Imagen2D.h"  // panel del elemento de imagen
#include "objects/Rect2D.h"    // panel del elemento rectangulo
#include "objects/Contenedor2D.h" // panel del contenedor (rect invisible)
#include "objects/Slice9.h"       // panel del slice 9 (imagen con bordes fijos)
#include "objects/Boton2D.h"      // panel del boton
#include "objects/Expandir2D.h"   // panel del expandir (resorte de layout)
#include "objects/Video2D.h"      // panel del video
#include "io/Video2DCache.h"      // tamano real del video al elegirlo
#include "io/Fuente2D.h"
#include "io/Textura2D.h"      // tamano natural del archivo al elegir la textura
#include "io/UI2DFormato.h"    // guardar/cargar interfaces (.w3dui)
#include "io/W3dContenedor.h"  // importar un asset = COPIARLO adentro del .w3d (ver el header)
#include "objects/UI.h"
#include "W3dPaletas.h"   // paletas a nivel PROYECTO (gestion + resolucion efectiva)
#include "render/UIOverlay.h"   // UI2D_PuntoAncla (rebase del ancla)   // T(): los textos salen en el idioma del sistema
#include "ViewPorts/Timeline.h"   // keyframe ACTIVO + InvalidarAnimYRedraw (tarjeta "Keyframe")
#include "Properties.h"
#include "Undo.h" // Ctrl+Z: capturar rename
#include "W3dAviso.h" // avisos de rename SIN desbordar (los nombres los tipea el usuario, sin cota)
#include "edit/MeshEdit.h" // Nuevo/Borrar/MoverMeshPart (funciones libres del editor)
#include "objects/EditMesh.h" // CentroSeleccion (campos X/Y/Z de posicion en Edit Mode)
#include "objects/ObjectMode.h" // MoverSeleccionEditLocal (aplicar los campos X/Y/Z a la seleccion)
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "PopUp/ColorPicker.h"
#include "PopUp/FileBrowser.h" // explorador para elegir la carpeta de export
#include "PopUp/ConfirmarPopup.h" // confirmacion de sobrescritura (render / export)
#include "w3dFilesystem.h" // FileExists / GetDefaultOutputDir / JoinPath (rutas de salida)
#include "PopUp/ProgressPopup.h" // barra "Rendering..." durante el render (clave en N95)
#include "ViewPorts/LayoutInput.h" // Notificar (toasts de exito/error)
#include "objects/Camera.h"   // selector de target de la camara
#include "objects/Instance.h" // selector de target de instance/array/mirror
#include "objects/LOD.h"      // tarjeta del objeto LOD (umbrales de distancia)
#include "objects/Culling.h"  // tarjeta del objeto Culling (soloCamaraActiva)
#include "objects/Collection.h" // tarjeta de la Collection (ordenarPorCamara/ordenarUnaVez)
#include "objects/Particulas.h" // tarjeta del objeto Particulas (emisor: textura + cono)
#include "edit/Modifier.h"    // ModifierType (ids del menu Add del stack de modificadores)
#include "objects/Armature.h" // pestania Animation: clips del esqueleto
#include "animation/SkeletalAnimation.h" // CrearAnimacion/BorrarAnimacionActiva/MoverAnimacionActiva
#include "animation/Armature2DAnimation.h" // clips del ARMATURE 2D (selector del timeline + tarjeta Animacion)
#include "edit/BoneEdit.h"               // BoneRenombradoDesdeUI (rename de hueso + vertex group juntos)
#include "ViewPorts/UVEditor.h"          // tarjeta "Armature 2D": UVEditorEnModoHuesos + Bone2D* (huesos 2D del mesh)
#include "edit/WeightPaint.h"          // Assign/Remove/Select/Deselect de Vertex Groups y UV Groups
#include "importers/import_obj.h" // ExportOBJ (boton Wavefront.obj de la tarjeta Export)
#include "importers/export_gltf.h" // ExportGLTF (glTF/GLB: rig + animaciones, sin hornear el skinning)
#include "render/OpcionesRender.h" // g_redraw (scroll de la lista con la rueda)
#include "ViewPorts/ViewPort3D.h"  // Viewport3D::RenderAPNG + Viewport3DActive (render a PNG)
#include <cstdio>
#include <cstdlib> // atof: el valor configurado (string) -> buffer del PropFloat (tarjetas de script)
#include <string>
#include <set> // centro UV de la seleccion (posiciones UV unicas, tarjeta "Transform UV")
#ifdef W3D_SYMBIAN
extern int W3dPantallaAlto; // flip de Y (glesdraw.cpp)
#endif

Properties* PropsActivo = NULL;

// SALIR de la edicion del panel activo tras confirmar/cancelar la edicion numerica por texto: limpia 'editando' para
// que la navegacion (button_up/down) vuelva a moverse por las propiedades en vez de ajustar el valor (bug del "clavado").
void NumEditSalirDelPanel(){ if (PropsActivo) PropsActivo->editando = false; }

void DibujarTitulo(Object* obj, int maxPixels){
    SetColorID(ColorID::blanco);

    //icono de la coleccion
    W3dDrawStrip4(IconMesh, IconsUV[IconoDeObjeto(obj)]->uvs);

    //texto render
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    RenderBitmapText(obj->name, textAlign::left, maxPixels);
    w3dEngine::PopMatrix();
    w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
}

void RebindMaterialMeshPart(); // (definida mas abajo)

// nombre clasico del programa: Material, Material.001, Material.002...
// Delega en LA regla comun (el ctor de Material ya uniquifica igual; esto solo
// evita pedirle un nombre tomado). El bucle viejo tenia tope 999 y al llegar
// devolvia "Material.999" DUPLICADO.
static std::string NombreMaterialLibre(){
    return MaterialNombreLibre("Material", NULL);
}

// el desplegable del selector de materiales (se reconstruye al abrir)
static PopupMenu* MenuMateriales = NULL;

// opcion elegida: 0 = New Material, 1 = Default Material, 2+ = existentes
// Crea un material nuevo (nombre clasico: Material, Material.001, ...) y lo pone en el mesh part 'idx'. Vive aca
// porque aca vive NombreMaterialLibre; lo usa el Add > Reference, que necesita el material hecho para poder abrirle
// el selector de textura de una.
Material* NuevoMaterialEnMeshPart(Mesh* mesh, int idx){
    if (!mesh || idx < 0 || idx >= (int)mesh->materialsGroup.size()) return NULL;
    Material* mat = new Material(NombreMaterialLibre());
    mesh->materialsGroup[idx].material = mat;
    return mat;
}

static void AccionMaterialElegido(int id){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    if (mesh->materialsGroup.empty()) return;
    PropListMeshParts* lista =
        static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;

    UndoCapturarMaterial(mesh, idx); // Ctrl+Z: guarda el Material* previo del mesh part

    if (id == 0) {
        // nombre clasico del programa: Material, Material.001, ...
        mesh->materialsGroup[idx].material = new Material(NombreMaterialLibre());
    } else if (id == 1) {
        // el material por defecto (fuera de la lista global Materials:
        // no aparece entre los "existentes")
        if (!MaterialDefecto) MaterialDefecto = new Material("Default Material", true);
        mesh->materialsGroup[idx].material = MaterialDefecto;
    } else if (id >= 2 && id - 2 < (int)Materials.size()) {
        mesh->materialsGroup[idx].material = Materials[id - 2];
    }
    RebindMaterialMeshPart();
}

// ===== Mesh Parts: New / Assign / Select / Deselect / Delete (botones de la tarjeta) =====
extern bool LayoutToggleEditMode(); // LayoutInput.cpp

// el contenido cambio (material/modo/rename distinto): recalcular la tarjeta y el scroll el proximo frame
static bool PropertiesLayoutDirty = false;

static Mesh* MaterialMesh(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? static_cast<Mesh*>(ObjActivo) : NULL;
}
static int MeshPartActivoIdx(Mesh* m){
    if (!PropsActivo || !m || m->materialsGroup.empty()) return -1;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)m->materialsGroup.size()) idx = 0;
    return idx;
}
// material que muestra el panel (el del mesh part activo) -> para el Ctrl+Z de modificacion de material
static Material* MaterialActivoUI(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m);
    if (!m || idx < 0 || idx >= (int)m->materialsGroup.size()) return NULL;
    return m->materialsGroup[idx].material;
}
static void SelEnListaMeshPart(int idx){
    if (!PropsActivo) return;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    lista->selectIndex = idx; lista->AjustarVentana();
}
// el selector del stack de modificadores SIGUE al modificador activo (tras add/remove/move -> no se pierde la
// seleccion visual: el modificador movido queda resaltado).
static void SelEnListaModificador(){
    if (!PropsActivo || !PropsActivo->propListModifiers) return;
    Mesh* m = MaterialMesh(); if (!m) return;
    PropsActivo->propListModifiers->selectIndex = m->modificadorActivo;
    PropsActivo->propListModifiers->AjustarVentana();
    m->GenerarMallaModificada(); // el stack cambio (add/remove/move) -> regenerar la malla generada
    g_redraw = true;
}
static void AccionNuevoMeshPart(){
    Mesh* m = MaterialMesh(); if (!m) return;
    UndoCapturarMallaGeo(m); // Ctrl+Z: snapshot pre-nuevo-mesh-part (materialsGroup)
    SelEnListaMeshPart(NuevoMeshPart(m)); // crea vacio + lo deja activo
    RebindMaterialMeshPart(); g_redraw = true;
}
static void AccionBorrarMeshPart(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0) return;
    UndoCapturarMallaGeo(m); // Ctrl+Z: snapshot pre-borrar-mesh-part (faces3d.mat + materialsGroup)
    BorrarMeshPart(m, idx); // huerfanas -> anterior; siempre queda >=1
    int n = (int)m->materialsGroup.size();
    SelEnListaMeshPart(idx >= n ? n - 1 : idx);
    RebindMaterialMeshPart(); g_redraw = true;
}
static void AccionAssignMeshPart(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0) return;
    m->AsignarFacesAMeshPart(idx); // las caras seleccionadas (edit) pasan a este mesh part
    g_redraw = true;
}
static void AccionSelectMeshPart(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0) return;
    if (InteractionMode != EditMode) LayoutToggleEditMode(); // entrar a Edit para VER la seleccion
    UndoCapturarSeleccionEdit(m); // Ctrl+Z: seleccionar las caras del mesh part cambia la seleccion edit
    m->SeleccionarMeshPart(idx, true);
    g_redraw = true;
}
static void AccionDeselectMeshPart(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0) return;
    UndoCapturarSeleccionEdit(m); // Ctrl+Z: deseleccionar las caras del mesh part cambia la seleccion edit
    m->SeleccionarMeshPart(idx, false);
    g_redraw = true;
}
// reordenar el mesh part activo (el ORDEN = orden de dibujado: solidos primero, transparentes al final).
static void AccionMeshPartUp(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx <= 0) return; // el primero no sube
    UndoCapturarMallaGeo(m); // Ctrl+Z: reordenar toca faces3d.mat + materialsGroup
    MoverMeshPart(m, idx, -1);
    SelEnListaMeshPart(idx - 1); // el mesh part MOVIDO queda seleccionado
    RebindMaterialMeshPart(); g_redraw = true;
}
static void AccionMeshPartDown(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m);
    if (idx < 0 || idx >= (int)m->materialsGroup.size() - 1) return; // el ultimo no baja
    UndoCapturarMallaGeo(m);
    MoverMeshPart(m, idx, +1);
    SelEnListaMeshPart(idx + 1);
    RebindMaterialMeshPart(); g_redraw = true;
}

// ===== nombres UNICOS (no se pueden duplicar): devuelve 'n', o n.001/.002... evitando 'excl' (el propio
// nombre, para que renombrar al mismo valor no choque). Cada scope junta los punteros a sus nombres.
// TODOS delegan en LA regla comun (base/W3dNombres.h): la version vieja de aca NO pelaba el ".NNN"
// previo y producia "Cubo.001.001", mientras Object::SetName y BoneNombreUnico si lo pelaban. =====
static std::string UniqueNombre(const std::string& n, std::string* excl, const std::vector<std::string*>& nombres){
    return W3dNombreUnicoEnLista(n, "Objeto", nombres, excl);
}
static std::string UniqMaterial(const std::string& n, std::string* excl){
    std::vector<std::string*> v; for (size_t i = 0; i < Materials.size(); i++) v.push_back(&Materials[i]->name);
    return W3dNombreUnicoEnLista(n, "Material", v, excl);
}
// las PARTES de la malla TAMBIEN entran a la regla ("nada con el mismo nombre"):
// antes el rename decia explicitamente "si pueden repetir nombre".
static std::string UniqMeshPart(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->materialsGroup.size(); i++) v.push_back(&m->materialsGroup[i].name); }
    return W3dNombreUnicoEnLista(n, "Mesh", v, excl);
}
static std::string UniqUVMap(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->uvMaps.size(); i++) v.push_back(&m->uvMaps[i]->nombre); }
    return W3dNombreUnicoEnLista(n, "UVMap", v, excl);
}
static std::string UniqColor(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->colorLayers.size(); i++) v.push_back(&m->colorLayers[i]->nombre); }
    return W3dNombreUnicoEnLista(n, "Col", v, excl);
}
// OJO: mira LAS DOS PUNTAS del binding. Si el commit va a ARRASTRAR huesos homonimos (los renombra
// RenameDespuesVGroup), el nombre tambien tiene que quedar libre entre los HUESOS del rig ligado:
// uniquificando solo en la malla, tipear el nombre de otro hueso dejaba DOS huesos iguales. Sin
// arrastre no se miran los huesos: ponerle a un grupo el nombre de un hueso es como se lo liga.
static std::string UniqVGroup(const std::string& n, std::string* excl){
    std::vector<std::string> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->vertexGroups.size(); i++)
            if (m->vertexGroups[i] && &m->vertexGroups[i]->nombre != excl) v.push_back(m->vertexGroups[i]->nombre);
        Armature* rig = m->skinArmature;
        if (!rig)
            for (size_t i = 0; i < m->modificadores.size() && !rig; i++)
                if (m->modificadores[i] && m->modificadores[i]->tipo == ModifierType::Armature)
                    rig = (Armature*)m->modificadores[i]->target;
        if (rig && excl){
            bool arrastra = false;
            for (size_t b = 0; b < rig->bones.size() && !arrastra; b++) if (rig->bones[b].name == *excl) arrastra = true;
            if (arrastra)
                for (size_t b = 0; b < rig->bones.size(); b++)
                    if (rig->bones[b].name != *excl) v.push_back(rig->bones[b].name);
        }
    }
    return W3dNombreUnicoEnValores(n, "Group", v, -1);
}
// los UV groups tienen su PROPIO espacio de nombres (son otra entidad): un UV group puede
// llamarse igual que un vertex group sin chocar (de hecho es lo normal si el rig 2D y el 3D
// comparten nombres de hueso).
// (y con la MISMA regla de las dos puntas que UniqVGroup, pero contra los huesos 2D del mesh)
static std::string UniqUVGroup(const std::string& n, std::string* excl){
    std::vector<std::string> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->uvGroups.size(); i++)
            if (m->uvGroups[i] && &m->uvGroups[i]->nombre != excl) v.push_back(m->uvGroups[i]->nombre);
        if (excl){
            bool arrastra = false;
            for (size_t a = 0; a < m->armatures2d.size() && !arrastra; a++){
                Armature2D* arm = m->armatures2d[a]; if (!arm) continue;
                for (size_t i = 0; i < arm->huesos.size() && !arrastra; i++)
                    if (arm->huesos[i].nombre == *excl) arrastra = true;
            }
            if (arrastra)
                for (size_t a = 0; a < m->armatures2d.size(); a++){
                    Armature2D* arm = m->armatures2d[a]; if (!arm) continue;
                    for (size_t i = 0; i < arm->huesos.size(); i++)
                        if (arm->huesos[i].nombre != *excl) v.push_back(arm->huesos[i].nombre);
                }
        }
    }
    return W3dNombreUnicoEnValores(n, "UV Group", v, -1);
}
// armature activo (o NULL): fuente de la pestania Animation
static Armature* ArmActiva(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::armature) ? (Armature*)ObjActivo : NULL;
}
static std::string UniqAnim(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (Armature* a = ArmActiva())
        for (size_t i = 0; i < a->animations.size(); i++) v.push_back(&a->animations[i]->name);
    return W3dNombreUnicoEnLista(n, "Animation", v, excl);
}
// CLIP del armature 2D activo de la malla animada (antes el rename pasaba SIN uniquificador)
static std::string UniqAnim2D(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ActiveAnimMesh)
        for (size_t i = 0; i < ActiveAnimMesh->Arm2DAnims().size(); i++)
            if (ActiveAnimMesh->Arm2DAnims()[i]) v.push_back(&ActiveAnimMesh->Arm2DAnims()[i]->name);
    return W3dNombreUnicoEnLista(n, "Anim2D", v, excl);
}
// VERTEX ANIMATION de la malla (antes el rename pasaba SIN uniquificador)
static std::string UniqVertexAnim(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ActiveAnimMesh)
        for (size_t i = 0; i < ActiveAnimMesh->animations.size(); i++)
            if (ActiveAnimMesh->animations[i]) v.push_back(&ActiveAnimMesh->animations[i]->name);
    return W3dNombreUnicoEnLista(n, "Anim", v, excl);
}
// ANIMACION DE ESCENA (global de proyecto; antes el rename pasaba SIN uniquificador)
static std::string UniqSceneAnim(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    InitSceneAnimations();
    for (size_t i = 0; i < SceneAnimations.size(); i++)
        if (SceneAnimations[i]) v.push_back(&SceneAnimations[i]->name);
    return W3dNombreUnicoEnLista(n, "Scene", v, excl);
}
// ARMATURE 2D de la malla (antes el rename pasaba NULL: se podian repetir a mano)
static std::string UniqArm2D(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->armatures2d.size(); i++)
            if (m->armatures2d[i]) v.push_back(&m->armatures2d[i]->nombre); }
    return W3dNombreUnicoEnLista(n, "Armature 2D", v, excl);
}
// OBJETOS: el scope NO es global sino POR ESCENA (la raiz UI de la que cuelga el objeto),
// que es el MISMO scope con el que se resuelven las refs de los scripts (SimJuego.cpp).
// Con el scope global, abrir whiskpaddle -14 widgets homonimos entre sus 3 escenas-
// renombraria medio proyecto y los objeto("op1") de lua devolverian nil. Lo resuelve
// Object::NombreLibre (libs/Whisk3DCore/objects/Objects.cpp), la puerta comun.

// ===== Rename: el BOTON se vuelve un INPUT in-place (no un campo abajo). Al ACEPTAR escribe el nombre
// (uniquificado segun el scope); cancelar lo descarta. El input lo rutea controles.cpp a g_textFieldActivo. =====
static std::string* g_renameTarget = NULL; // NULL = no hay rename en curso
static TextField    g_renameField;         // el texto que se edita (se dibuja DENTRO del boton)
static Button*      g_renameBoton = NULL;  // el boton que se volvio input
static std::string (*g_renameUniq)(const std::string&, std::string*) = NULL; // uniquificador del scope (o NULL)
static void (*g_renameDespues)() = NULL;   // hook POST-commit (ej: hueso -> renombrar su vertex group junto)
// IDENTIDAD del destino PARA EL UNDO (ver W3dRenameDest en Undo.h): casi todos los renames
// de esta tarjeta escriben en un dueno del heap (Object/Material/UVMap*/grupo*), pero el de
// MESH PART apunta DENTRO de Mesh::materialsGroup, que es un vector POR VALOR: guardar ahi un
// std::string* dejaba el paso de undo colgando en cuanto se agregaba/borraba una parte.
// OJO: "del heap" NO quiere decir eterno. Las capas de la malla (grupos/UV maps/colores) las
// destruye y recrea Mesh::LiberarCapas (cualquier op de geometria, via MeshGeoUndo), y el
// boton "-" de estas mismas tarjetas las borra. Por eso el Directo tambien se revalida al
// aplicar (W3dNombrePunteroVivo): si el dueno murio, el paso es un no-op.
static W3dRenameDest g_renameDest;

bool RenameActivo(){ return g_renameTarget != NULL; }

static void RenameLimpiar(){
    if (g_renameBoton) g_renameBoton->editField = NULL; // el boton vuelve a ser boton
    g_renameTarget = NULL;
    g_renameDest = W3dRenameDest();
    g_renameBoton = NULL;
    g_renameUniq = NULL;
    g_renameDespues = NULL;
    g_textFieldActivo = NULL;
}
// el nombre que TENIA el destino antes del commit (lo leen los hooks post-commit que
// tienen que arrastrar la contraparte por nombre: vertex group -> hueso, etc.)
static std::string g_renameViejo;
void RenameCommit(){ // ACEPTAR: escribe el texto (uniquificado) en el nombre destino
    void (*despues)() = NULL;
    if (g_renameTarget) {
        g_renameViejo = *g_renameTarget;
        UndoCapturarRename(g_renameDest);   // Ctrl+Z: guarda el nombre PREVIO antes de escribir
        // el uniquificador YA normaliza (trim + fallback): un campo vacio o con solo
        // espacios no puede quedar como nombre (dos vacios chocarian igual que dos "Cubo").
        const std::string pedido = W3dNombreNormalizar(g_renameField.text, "Objeto");
        *g_renameTarget = g_renameUniq ? g_renameUniq(pedido, g_renameTarget) : pedido;
        if (*g_renameTarget != pedido)
            W3dAvisoYaExiste(pedido, *g_renameTarget); // no es un error: es informativo
        despues = g_renameDespues;         // el hook corre DESPUES de limpiar (puede abrir notificaciones)
    }
    RenameLimpiar();
    if (despues) despues();
    RebindMaterialMeshPart(); // refresca el texto mostrado (boton de material / lista de parts)
    g_redraw = true;
}
void RenameCancel(){ RenameLimpiar(); g_redraw = true; } // CANCELAR: no escribe nada

// 'boton' se vuelve input (con TODO seleccionado). 'uniq' = uniquificador del scope (NULL = sin chequeo).
// 'uniq' es OBLIGATORIO (antes tenia default NULL y 5 de los 9 callers no lo pasaban): asi el
// compilador rompe en cada rename que no declare su espacio de nombres y no se puede agregar
// una tarjeta nueva "olvidandose". 'despues' = hook post-commit (arrastrar la contraparte).
// 'dest' = la identidad ESTABLE del mismo nombre para el paso de undo. Es OBLIGATORIO (antes
// tenia default NULL y caia en W3dDestNombre(destino), o sea un 'Directo' con el puntero pelado:
// para un elemento de lista -grupo, capa, clip- eso da un paso de undo MUDO, que no restaura nada
// y no avisa. Con el parametro sin default, una tarjeta nueva no puede "olvidarse" de declararlo.
static void RenameIniciar(Button* boton, std::string* destino, std::string (*uniq)(const std::string&, std::string*),
                          void (*despues)(), const W3dRenameDest* dest){
    if (!boton || !destino || !dest) return;
    g_renameTarget = destino;
    g_renameDest = *dest;
    g_renameBoton = boton;
    g_renameUniq = uniq;
    g_renameDespues = despues; // hook post-commit (rename de vertex group / UV group: la otra punta)
    g_renameField.SetText(*destino);
    g_renameField.SelectAll();   // TODO seleccionado: la 1ra tecla reemplaza
    boton->editField = &g_renameField; // el boton se dibuja como input
    g_textFieldActivo = &g_renameField;
    // TACTIL (Android/Symbian): abrir el teclado QWERTY, igual que al tocar un campo de texto (antes el rename
    // enfocaba el campo pero no salia teclado -> inconsistente con render/export). En PC/web sigue el camino normal.
#if !defined(__EMSCRIPTEN__)
    { extern bool g_uiTapEnCurso; void QwertyAbrir(); if (g_uiTapEnCurso) QwertyAbrir(); }
#endif
}
static void AccionRenameMeshPart(){ // las PARTES tambien son unicas dentro de la malla
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0 || !PropsActivo) return;
    if (!PropsActivo->propRowDelRen || PropsActivo->propRowDelRen->botones.size() < 2) return;
    const W3dRenameDest dest = W3dDestMeshPart(m, idx);  // el nombre vive DENTRO del vector de partes
    RenameIniciar(PropsActivo->propRowDelRen->botones[1], &m->materialsGroup[idx].name, UniqMeshPart,
                  NULL, &dest); // [1] = Rename
}
static void AccionRenameMaterial(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0 || !PropsActivo) return;
    Material* mat = m->materialsGroup[idx].material;
    if (!mat || mat == MaterialDefecto) return; // el material POR DEFECTO no se renombra (es global)
    if (!PropsActivo->propBtnRenameMat) return;
    // el destino va por INDICE en Materials (lista global de punteros): un material borrado
    // deja el Material* colgando y el proximo new puede caer en la misma direccion (Undo.h)
    int im = -1;
    for (size_t k = 0; k < Materials.size(); k++) if (Materials[k] == mat) { im = (int)k; break; }
    if (im < 0) return;
    const W3dRenameDest dest = W3dDestGlobal(W3dRenameDest::MaterialG, im);
    RenameIniciar(PropsActivo->propBtnRenameMat->button, &mat->name, UniqMaterial, NULL, &dest); // GLOBAL unico
}
static void AccionRenameUVMap(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameUV) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->uvMapActivo < 0 || m->uvMapActivo >= (int)m->uvMaps.size()) return;
    // (malla, lista, indice): el UVMap* se libera y se recrea (LiberarCapas / boton "-"), ver Undo.h
    const W3dRenameDest dest = W3dDestCapaMalla(m, W3dRenameDest::UVMap, m->uvMapActivo);
    RenameIniciar(PropsActivo->propBtnRenameUV->button, &m->uvMaps[m->uvMapActivo]->nombre, UniqUVMap,
                  NULL, &dest);
}
static void AccionRenameColor(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameColor) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->colorActivo < 0 || m->colorActivo >= (int)m->colorLayers.size()) return;
    const W3dRenameDest dest = W3dDestCapaMalla(m, W3dRenameDest::ColorLayer, m->colorActivo);
    RenameIniciar(PropsActivo->propBtnRenameColor->button, &m->colorLayers[m->colorActivo]->nombre, UniqColor,
                  NULL, &dest);
}
// POST-COMMIT del vertex group: arrastra el HUESO 3D homonimo del rig ligado (binding POR
// NOMBRE, SkinearMesh). Antes NO existia: renombrar el grupo rompia el rig en silencio (y con
// la regla nueva, tipear un nombre ya usado te daba un .001 que no matchea ningun hueso).
// Se funde con el paso de undo del propio rename -> UN SOLO Ctrl+Z.
static void RenameDespuesVGroup(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->grupoActivo < 0 || m->grupoActivo >= (int)m->vertexGroups.size()) return;
    const std::string nuevo = m->vertexGroups[m->grupoActivo]->nombre;
    if (nuevo == g_renameViejo) return;
    Armature* rig = m->skinArmature;
    if (!rig)
        for (size_t i = 0; i < m->modificadores.size() && !rig; i++)
            if (m->modificadores[i] && m->modificadores[i]->tipo == ModifierType::Armature)
                rig = (Armature*)m->modificadores[i]->target;
    if (!rig) return;
    // (armature, indice): los huesos viven POR VALOR en rig->bones y un alta/baja posterior
    // realoca el vector -> un std::string* guardado en el undo quedaba colgando (ver Undo.h)
    std::vector<W3dRenameDest> v;
    for (size_t b = 0; b < rig->bones.size(); b++)
        if (rig->bones[b].name == g_renameViejo) v.push_back(W3dDestHueso3D(rig, (int)b));
    if (v.empty()) return;
    UndoCapturarRenames(v);
    UndoFundirUltimos(2);                 // grupo + huesos = UN paso
    for (size_t k = 0; k < v.size(); k++)
        if (std::string* p = W3dDestResolver(v[k])) *p = nuevo;
    m->lastSkinFrame = -999999;           // el CSR de skinning hashea los nombres
    W3dAvisoArrastre((int)v.size(), "hueso(s)", g_renameViejo, nuevo);
}
static void AccionRenameGroup(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameGroup) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->grupoActivo < 0 || m->grupoActivo >= (int)m->vertexGroups.size()) return;
    const W3dRenameDest dest = W3dDestCapaMalla(m, W3dRenameDest::VGroup, m->grupoActivo);
    RenameIniciar(PropsActivo->propBtnRenameGroup->button, &m->vertexGroups[m->grupoActivo]->nombre,
                  UniqVGroup, RenameDespuesVGroup, &dest);
}
// POST-COMMIT del UV group: arrastra el HUESO 2D homonimo (binding por nombre del rig 2D)
static void RenameDespuesUVGroup(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->uvGrupoActivo < 0 || m->uvGrupoActivo >= (int)m->uvGroups.size()) return;
    const std::string nuevo = m->uvGroups[m->uvGrupoActivo]->nombre;
    if (nuevo == g_renameViejo) return;
    std::vector<W3dRenameDest> v;         // (malla, armature, indice): idem los huesos 2D
    for (size_t a = 0; a < m->armatures2d.size(); a++){
        Armature2D* arm = m->armatures2d[a];
        if (!arm) continue;
        for (size_t i = 0; i < arm->huesos.size(); i++)
            if (arm->huesos[i].nombre == g_renameViejo) v.push_back(W3dDestHueso2D(m, arm, (int)i));
    }
    if (v.empty()) return;
    UndoCapturarRenames(v);
    UndoFundirUltimos(2);                 // grupo + huesos 2D = UN paso
    for (size_t k = 0; k < v.size(); k++)
        if (std::string* p = W3dDestResolver(v[k])) *p = nuevo;
    m->pose2dDirty = true;
    W3dAvisoArrastre((int)v.size(), "hueso(s) 2D", g_renameViejo, nuevo);
}
static void AccionRenameUVGroup(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameUVGroup) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->uvGrupoActivo < 0 || m->uvGrupoActivo >= (int)m->uvGroups.size()) return;
    const W3dRenameDest dest = W3dDestCapaMalla(m, W3dRenameDest::UVGroup, m->uvGrupoActivo);
    RenameIniciar(PropsActivo->propBtnRenameUVGroup->button, &m->uvGroups[m->uvGrupoActivo]->nombre,
                  UniqUVGroup, RenameDespuesUVGroup, &dest);
}
static void AccionRenameAnim(){
    Armature* a = ArmActiva();
    if (!a || !PropsActivo || !PropsActivo->propBtnRenameAnim) return;
    if (a->animActiva < 0 || a->animActiva >= (int)a->animations.size()) return;
    const W3dRenameDest dest = W3dDestClipArm(a, a->animActiva);   // el clip se borra con el "-"
    RenameIniciar(PropsActivo->propBtnRenameAnim->button, &a->animations[a->animActiva]->name, UniqAnim,
                  NULL, &dest);
}
// NOMBRE del objeto: el campo propNameObj muestra ObjActivo->name (cuando NO se edita) y, al perder el foco,
// escribe lo tipeado (uniquificado) en ObjActivo->name. Se llama por frame desde RefreshTargetProperties.
static std::string* g_nameEditTarget = NULL; // != NULL mientras se edita el nombre (captura el destino al enfocar)
static Object*      g_nameEditObj    = NULL; // el OBJETO que se esta renombrando (si el activo cambia mientras
                                             // se tipea, el commit va igual al que se estaba editando)
static PropText*    g_nameEditCampo  = NULL; // CUAL campo Name esta editando (hay uno por tarjeta contextual)
// sincroniza UN campo Name contra ObjActivo->name: muestra el nombre (cuando no se edita) y al
// perder el foco commitea lo tipeado (uniquificado). Corre ANTES de dibujar los campos
// (RefreshTargetProperties al inicio de Render) -> el nombre ya se ve en el primer frame.
static void SincronizarNombreCampo(PropText* pt){
    if (!pt) return;
    bool foco = (g_textFieldActivo == &pt->field);
    if (foco && !g_nameEditTarget && ObjActivo){ g_nameEditTarget = &ObjActivo->name; g_nameEditObj = ObjActivo; g_nameEditCampo = pt; }
    if (!foco && g_nameEditTarget && g_nameEditCampo == pt){   // termino -> commit uniquificado
        // W3dRenombrarObjeto: uniquifica en el scope de la ESCENA y arrastra los vinculos
        // por nombre (refs de scripts lua, targetName, RielName, escenaInicial) en UN SOLO
        // paso de undo. Antes escribia el nombre a pelo y las refs quedaban rotas EN SILENCIO.
        if (g_nameEditObj && g_nameEditTarget == &g_nameEditObj->name)
            W3dRenombrarObjeto(g_nameEditObj, pt->field.text, true);
        g_nameEditTarget = NULL; g_nameEditObj = NULL; g_nameEditCampo = NULL;
    }
    // sync display cuando NO se edita. Solo si CAMBIO (sino redibuja infinito) + pide un redraw.
    if (!foco && ObjActivo && pt->field.text != ObjActivo->name){ pt->field.SetText(ObjActivo->name); g_redraw = true; }
}
static std::string* g_btnEditTarget = NULL;
static void SincronizarTextoBoton(Properties* p){
    if (!p || !p->propBtnTexto) return;
    Boton2D* b = (ObjActivo && ObjActivo->getType() == ObjectType::boton2d) ? (Boton2D*)ObjActivo : NULL;
    PropText* pt = p->propBtnTexto;
    bool foco = (g_textFieldActivo == &pt->field);
    if (foco && !g_btnEditTarget && b) g_btnEditTarget = &b->texto;
    if (foco && g_btnEditTarget && *g_btnEditTarget != pt->field.text){
        *g_btnEditTarget = pt->field.text;   // vista previa EN VIVO
        g_redraw = true;
    }
    if (!foco && g_btnEditTarget) g_btnEditTarget = NULL;
    if (!foco && b && pt->field.text != b->texto){ pt->field.SetText(b->texto); g_redraw = true; }
}

static void SincronizarNombreObjeto(Properties* p){
    if (!p) return;
    SincronizarNombreCampo(p->propNameObj);
    SincronizarNombreCampo(p->propT2dNombre);   // los elementos 2D no muestran el tab Objeto:
    SincronizarNombreCampo(p->propImgNombre);   // su Name vive arriba de su tarjeta contextual
    SincronizarNombreCampo(p->propRectNombre);
    SincronizarNombreCampo(p->propContNombre);
    SincronizarNombreCampo(p->propS9Nombre);
    SincronizarNombreCampo(p->propBtnNombre);
    SincronizarNombreCampo(p->propExpNombre);
    SincronizarNombreCampo(p->propVidNombre);
    SincronizarNombreCampo(p->propUInombre);
    // (la tarjeta Control ya no tiene campo Name: el nombre se edita en el outliner
    //  y en la pestania Objeto, que es donde vive para TODOS los demas tipos)
}

// TEXTO del elemento 2D: el campo propT2dTexto muestra t->texto y, al perder el foco, escribe lo
// tipeado. Mismo patron que SincronizarNombreObjeto (commit al desenfocar + sync de display).
static std::string* g_t2dEditTarget = NULL;
static void SincronizarTexto2D(Properties* p){
    if (!p || !p->propT2dTexto) return;
    Texto2D* t = (ObjActivo && ObjActivo->getType() == ObjectType::texto2d) ? (Texto2D*)ObjActivo : NULL;
    PropText* pt = p->propT2dTexto;
    bool foco = (g_textFieldActivo == &pt->field);
    if (foco && !g_t2dEditTarget && t) g_t2dEditTarget = &t->texto;
    if (foco && g_t2dEditTarget && *g_t2dEditTarget != pt->field.text){
        *g_t2dEditTarget = pt->field.text;   // VISTA PREVIA EN VIVO: cada tecla se ve en el lienzo
        g_redraw = true;
    }
    if (!foco && g_t2dEditTarget){
        g_t2dEditTarget = NULL;
    }
    if (!foco && t && pt->field.text != t->texto){ pt->field.SetText(t->texto); g_redraw = true; }
}

// DISTANCIAS del LOD: el campo muestra "20, 45, 90" y lo tipeado se aplica EN VIVO
// (el parseo de SetDistanciasTexto es tolerante: numeros a medio tipear no rompen).
// Mismo patron que SincronizarTexto2D: sync de display al no tener foco.
static std::string g_lodDistUlt; // ultimo texto APLICADO (no re-parsear el mismo por frame)
static void SincronizarLodDist(Properties* p){
    if (!p || !p->propLodDist) return;
    LOD* l = (ObjActivo && ObjActivo->getType() == ObjectType::lod) ? (LOD*)ObjActivo : NULL;
    PropText* pt = p->propLodDist;
    bool foco = (g_textFieldActivo == &pt->field);
    if (foco && l && pt->field.text != g_lodDistUlt){
        l->SetDistanciasTexto(pt->field.text);   // vista previa EN VIVO en el viewport
        g_lodDistUlt = pt->field.text;
        g_redraw = true;
    }
    if (!foco){
        g_lodDistUlt.clear();
        if (l && pt->field.text != l->DistanciasTexto()){
            pt->field.SetText(l->DistanciasTexto()); // display fresco (cambio de objeto / undo)
            g_redraw = true;
        }
    }
}

// TEXTURA del objeto Particulas: campo de texto con la ruta del PNG, aplicado EN VIVO
// (el decode fallido se cachea por ruta -> tipear a medias no reintenta por frame).
// Mismo patron que SincronizarLodDist: sync de display al no tener foco.
static std::string g_partTexUlt; // ultimo texto APLICADO (no re-aplicar el mismo por frame)
static void SincronizarPartTextura(Properties* p){
    if (!p || !p->propPartTextura) return;
    Particulas* pt = (ObjActivo && ObjActivo->getType() == ObjectType::particulas)
                   ? (Particulas*)ObjActivo : NULL;
    PropText* f = p->propPartTextura;
    bool foco = (g_textFieldActivo == &f->field);
    if (foco && pt && f->field.text != g_partTexUlt){
        pt->textura = f->field.text;   // vista previa EN VIVO en el viewport
        g_partTexUlt = f->field.text;
        g_redraw = true;
    }
    if (!foco){
        g_partTexUlt.clear();
        if (pt && f->field.text != pt->textura){
            f->field.SetText(pt->textura); // display fresco (cambio de objeto / undo)
            g_redraw = true;
        }
    }
}

// COLOR del objeto Particulas: "r, g, b, a" (parseo tolerante de SetColorTexto)
static std::string g_partColUlt;
static void SincronizarPartColor(Properties* p){
    if (!p || !p->propPartColor) return;
    Particulas* pt = (ObjActivo && ObjActivo->getType() == ObjectType::particulas)
                   ? (Particulas*)ObjActivo : NULL;
    PropText* f = p->propPartColor;
    bool foco = (g_textFieldActivo == &f->field);
    if (foco && pt && f->field.text != g_partColUlt){
        pt->SetColorTexto(f->field.text);
        g_partColUlt = f->field.text;
        g_redraw = true;
    }
    if (!foco){
        g_partColUlt.clear();
        if (pt && f->field.text != pt->ColorTexto()){
            f->field.SetText(pt->ColorTexto());
            g_redraw = true;
        }
    }
}

// nombre corto de una textura (el archivo, sin la ruta)
static std::string NombreDeTextura(Texture* t){
    if (!t) return std::string("No Texture");
    std::string n = t->path;
    size_t pos = n.find_last_of("/\\");
    if (pos != std::string::npos) n = n.substr(pos + 1);
    return n.empty() ? std::string("Texture") : n;
}

// el desplegable del selector de texturas del mesh part
static PopupMenu* MenuTexturas = NULL;
static PopupMenu* MenuTexturasNormal = NULL; // selector de la textura de NORMAL MAP (mat->normalTexture)

// "Load Texture": cada plataforma lo cablea (PC: abre el browser compartido en
// main.cpp). Carga la imagen y la asigna a 'mat' (async).
void (*DialogoCargarTextura)(Material* mat) = NULL;
// "Load Texture" del normal map: usa el MISMO DialogoCargarTextura (browser COMPARTIDO, anda en los 4 OS) pero
// con este flag prendido -> el callback de carga de cada plataforma asigna a mat->normalTexture en vez de
// mat->texture. (Antes habia un DialogoCargarNormalMap aparte SOLO cableado en PC -> en Symbian no abria nada.)
bool gCargarTexturaComoNormal = false;

// 0 = No Texture; 1 = Load Texture (dialogo); 2+ = Textures[5 + id - 2]
// (las primeras 5 son de la UI: font/origen/cursor/linea/lampara)
static void AccionTexturaElegida(int id){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    if (mesh->materialsGroup.empty()) return;
    PropListMeshParts* lista =
        static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;
    Material* mat = mesh->materialsGroup[idx].material;
    if (!mat) return;

    if (id == 0) {
        mat->texture = NULL; // No Texture
    } else if (id == 1) {
        // Load Texture: el browser COMPARTIDO carga la imagen y la asigna a
        // 'mat' (async: el rebind lo hace el callback al elegir el archivo)
        if (DialogoCargarTextura) { gCargarTexturaComoNormal = false; DialogoCargarTextura(mat); return; }
    } else if (5 + id - 2 < (int)Textures.size()) {
        mat->texture = Textures[5 + id - 2];
        mat->textureOn = true;
    }
    RebindMaterialMeshPart();
}

// ESTANDAR de los desplegables de Properties: abre 'menu' JUSTO debajo de 'boton', tocando su borde inferior
// (sin gap; el borde superior del menu se funde con el del boton, como los menus de la barra). Un solo lugar ->
// todos los dropdowns quedan iguales y bien pegados (antes cada accion lo calculaba a mano con un gap de mas).
static void AbrirMenuBajoBoton(PopupMenu* menu, Button* boton){
    if (!menu || !boton) return;
    // el menu se engancha al borde DERECHO del boton (nunca al izquierdo): los items quedan
    // alineados con la columna de valores, que es donde esta el desplegable.
    menu->Resize();   // para conocer su ancho antes de posicionarlo
    menu->Abrir(boton->sx + boton->width - menu->width,
                boton->sy + boton->height - GlobalScale, MenuPantallaW, MenuPantallaH);
    MenuAbierto = menu;
}

static void AccionMenuTexturas(){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    if (!MenuTexturas) {
        MenuTexturas = new PopupMenu();
        MenuTexturas->action = AccionTexturaElegida;
    }
    MenuTexturas->Limpiar(); // las texturas cargadas van cambiando
    MenuTexturas->Agregar(T("No Texture"), 0, IconType::notifError); // la "cruz" de error = sin textura
    MenuTexturas->Agregar(T("Load Texture"), 1, IconType::archive);
    for (size_t i = 5; i < Textures.size(); i++) {
        MenuTexturas->Agregar(NombreDeTextura(Textures[i]),
                              2 + (int)(i - 5), IconType::textura);
    }
    AbrirMenuBajoBoton(MenuTexturas, PropsActivo->propBtnTextura->button);
}

// === NORMAL MAP: selector de textura (mirror del de la textura base, pero -> mat->normalTexture) ===
static void AccionNormalTexElegida(int id){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    if (mesh->materialsGroup.empty()) return;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;
    Material* mat = mesh->materialsGroup[idx].material;
    if (!mat) return;
    if (id == 0) { mat->normalTexture = NULL; }                                   // sin normal map
    else if (id == 1) { if (DialogoCargarTextura) { gCargarTexturaComoNormal = true; DialogoCargarTextura(mat); return; } } // cargar archivo (MISMO browser que la textura base, flag -> normalTexture)
    else if (5 + id - 2 < (int)Textures.size()) { mat->normalTexture = Textures[5 + id - 2]; } // una ya cargada
    RebindMaterialMeshPart();
}

static void AccionMenuTexturasNormal(){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    if (!MenuTexturasNormal) {
        MenuTexturasNormal = new PopupMenu();
        MenuTexturasNormal->action = AccionNormalTexElegida;
    }
    MenuTexturasNormal->Limpiar();
    MenuTexturasNormal->Agregar(T("No Normal Map"), 0, IconType::notifError);
    MenuTexturasNormal->Agregar(T("Load Texture"), 1, IconType::archive);
    for (size_t i = 5; i < Textures.size(); i++) {
        MenuTexturasNormal->Agregar(NombreDeTextura(Textures[i]), 2 + (int)(i - 5), IconType::textura);
    }
    AbrirMenuBajoBoton(MenuTexturasNormal, PropsActivo->propBtnNormalTex->button);
}

// === REFLECTION: el MODO del reflejo (Matcap HW / Sphere Map / Equirect) en un desplegable (reemplaza el viejo
// checkbox "Chrome 360"). Los tags (hardware)/(software) son para el N95 (donde importa el perf): el Matcap es por
// matriz de textura = HW en los 4 OS; el Sphere Map exacto es HW en PC (texgen) pero SW en el N95; el Equirect es SW.
static PopupMenu* MenuReflectMode = NULL;
static const char* NombreReflectMode(int m){
    return (m == 1) ? "Sphere Map (software)"
         : (m == 2) ? "Equirectangular (software)"
         :            "Matcap (hardware)";
}
static void AccionReflectModeElegido(int id){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    if (mesh->materialsGroup.empty()) return;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;
    Material* mat = mesh->materialsGroup[idx].material;
    if (!mat) return;
    if (id >= 0 && id <= 2) mat->reflectMode = id;
    RebindMaterialMeshPart();
}
static void AccionMenuReflectMode(){
    if (!PropsActivo) return;
    if (!MenuReflectMode) { MenuReflectMode = new PopupMenu(); MenuReflectMode->action = AccionReflectModeElegido; }
    MenuReflectMode->Limpiar();
    MenuReflectMode->Agregar(NombreReflectMode(0), 0, IconType::material);
    MenuReflectMode->Agregar(NombreReflectMode(1), 1, IconType::material);
    MenuReflectMode->Agregar(NombreReflectMode(2), 2, IconType::material);
    AbrirMenuBajoBoton(MenuReflectMode, PropsActivo->propBtnReflectMode->button);
}

// ===========================================================================
//  CALCOMANIAS, MEZCLA Y PROFUNDIDAD EN EL PANEL
//
//  Pedido del dueno, textual: "capaz necesitas en el editor crear una propiedad
//  nueva de los materiales para ver como se comporta el elemento tipo un decal o
//  algo asi para las sombras. un menu desplegable con opciones del test de
//  profundidad o cosas asi".
//
//  Eran cuatro campos que solo existian en el .mtl / .w3d (`decal`, `ADITIVO`,
//  `sesgoProfundidad`, `ordenPasada`): para probar una sombra pegada al piso habia
//  que editar un archivo de texto y reabrir el proyecto.
//
//  El TILDE "Decal" no es un campo mas: es la RECETA COMPLETA, la misma que aplica
//  la palabra `decal` del .mtl y la misma que asserta `matflags <mat> decal` del
//  harness -- transparente + NO escribe z + sesgo hacia el ojo + orden de pasada 1.
//  Se muestra tildado cuando el material cumple las cuatro.
// ===========================================================================
// espejos de UI (los Prop* editan un bool/float suelto y el onChange lo baja al material)
static bool  g_matDecal = false;
static float g_matSesgo = 0.0f;
static float g_matOrden = 0.0f;

static void OnMatDecalChange() {
    Mesh* me = MaterialMesh();
    Material* mat = MaterialActivoUI();
    if (!mat || mat == MaterialDefecto) return;
    UndoCapturarMaterial(me, MeshPartActivoIdx(me));
    // LA MISMA receta que la palabra `decal` del .mtl (Materials.cpp): un solo lugar.
    // Se conserva el sesgo que ya tuviera (el usuario pudo afinarlo con Depth Bias).
    W3dMaterialAplicarDecal(mat, g_matDecal, mat->depth_bias < 0.0f ? mat->depth_bias : 0.0f);
    RebindMaterialMeshPart();
}
static void OnMatSesgoChange() {
    Material* mat = MaterialActivoUI();
    if (!mat || mat == MaterialDefecto) return;
    mat->depth_bias = g_matSesgo;
    g_redraw = true;
}
static void OnMatOrdenChange() {
    Material* mat = MaterialActivoUI();
    if (!mat || mat == MaterialDefecto) return;
    int v = (int)(g_matOrden + 0.5f); if (v < 0) v = 0; if (v > 2) v = 2;
    mat->orden_pasada = v;
    RebindMaterialMeshPart();   // el tilde "Decal" depende de esto
}

// --- LINEAS: dibujar las aristas de la malla con el material (checkbox) + el
// grosor en px de glLineWidth (visible solo con el tilde puesto). Mismo patron
// de espejos de UI que Decal/Depth Bias.
static bool  g_matLineas = false;
static float g_matGrosorLinea = 1.0f;
static void OnMatLineasChange() {
    Mesh* me = MaterialMesh();
    Material* mat = MaterialActivoUI();
    if (!mat || mat == MaterialDefecto) return;
    UndoCapturarMaterial(me, MeshPartActivoIdx(me));
    mat->lineas = g_matLineas;
    RebindMaterialMeshPart();   // aparece/desaparece el campo del grosor
}
static void OnMatGrosorLineaChange() {
    Material* mat = MaterialActivoUI();
    if (!mat || mat == MaterialDefecto) return;
    mat->grosorLinea = (g_matGrosorLinea < 1.0f) ? 1.0f : g_matGrosorLinea;
    g_redraw = true;
}

// --- MEZCLA: los modos de gfx::Mezcla, con el nombre del .mtl entre parentesis
static PopupMenu* MenuMezcla = NULL;
static const char* NombreMezcla(int m) {
    switch (m) {
        case w3dEngine::MezclaOff:      return "Opaque (ONE, ZERO)";
        case w3dEngine::MezclaAdd:      return "Additive (ADITIVO)";
        case w3dEngine::MezclaAddAlpha: return "Additive by alpha";
        case w3dEngine::MezclaMultiply: return "Multiply (darkens)";
        case w3dEngine::MezclaScreen:   return "Screen (lightens)";
        case w3dEngine::MezclaPremult:  return "Premultiplied alpha";
        case w3dEngine::MezclaSubtract: return "Subtract (darkens)";
        default:                        return "Alpha (normal)";
    }
}
static void AccionMezclaElegida(int id) {
    Mesh* me = MaterialMesh();
    Material* mat = MaterialActivoUI();
    if (!mat || mat == MaterialDefecto) return;
    if (id < 0 || id >= (int)w3dEngine::MezclaCount_) return;
    UndoCapturarMaterial(me, MeshPartActivoIdx(me));
    mat->mezcla = id;
    RebindMaterialMeshPart();
}
static void AccionMenuMezcla() {
    if (!PropsActivo || !PropsActivo->propBtnMezcla) return;
    if (!MenuMezcla) { MenuMezcla = new PopupMenu(); MenuMezcla->action = AccionMezclaElegida; }
    MenuMezcla->Limpiar();
    for (int i = 0; i < (int)w3dEngine::MezclaCount_; i++)
        MenuMezcla->Agregar(NombreMezcla(i), i, IconType::material);
    AbrirMenuBajoBoton(MenuMezcla, PropsActivo->propBtnMezcla->button);
}

// --- TEST DE PROFUNDIDAD: las 4 combinaciones de (test, escritura) en UN desplegable.
// Por separado son dos bools con nombres poco felices; juntos describen lo que se ve:
//   0 test+escribe = opaco normal | 1 test sin escribir = transparentes y calcomanias
//   2 sin test escribiendo = tapa lo de atras | 3 nada = HUD / overlays
static PopupMenu* MenuProfundidad = NULL;
static const char* NombreProfundidad(bool test, bool write) {
    if (test  &&  write) return "Test + Write (opaque)";
    if (test  && !write) return "Test, no Write (decal)";
    if (!test &&  write) return "No Test, Write";
    return "Off (no test, no write)";
}
static void AccionProfundidadElegida(int id) {
    Mesh* me = MaterialMesh();
    Material* mat = MaterialActivoUI();
    if (!mat || mat == MaterialDefecto) return;
    UndoCapturarMaterial(me, MeshPartActivoIdx(me));
    mat->depth_test  = (id == 0 || id == 1);
    mat->depth_write = (id == 0 || id == 2);
    RebindMaterialMeshPart();
}
static void AccionMenuProfundidad() {
    if (!PropsActivo || !PropsActivo->propBtnProfundidad) return;
    if (!MenuProfundidad) { MenuProfundidad = new PopupMenu(); MenuProfundidad->action = AccionProfundidadElegida; }
    MenuProfundidad->Limpiar();
    MenuProfundidad->Agregar(NombreProfundidad(true,  true),  0, IconType::material);
    MenuProfundidad->Agregar(NombreProfundidad(true,  false), 1, IconType::material);
    MenuProfundidad->Agregar(NombreProfundidad(false, true),  2, IconType::material);
    MenuProfundidad->Agregar(NombreProfundidad(false, false), 3, IconType::material);
    AbrirMenuBajoBoton(MenuProfundidad, PropsActivo->propBtnProfundidad->button);
}

// GL Light de la luz activa: el PropFloat edita un espejo float y al cambiar reasigna el LightID (0..7).
static float g_lightGLIdx = 0.0f;
static void OnLightGLChange(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::light) return;
    int idx = (int)(g_lightGLIdx + 0.5f); if (idx < 0) idx = 0; if (idx > 7) idx = 7;
    static_cast<Light*>(ObjActivo)->SetLightID(GL_LIGHT0 + (GLenum)idx);
}

// click en el selector: abre el desplegable (new / default / existentes)
// ====================================================================
//  TARJETA "AJUSTES" (pestania Render, abajo de todo): lo que vive en el config.ini, editable desde adentro.
//  Antes habia que salir del programa y abrir el .ini con un editor de texto.
//  Casi todo esto SOLO se aplica al arrancar (el contexto GL, la fuente, el skin), asi que al cambiarlo se avisa
//  que hay que reiniciar en vez de fingir que ya paso.
// ====================================================================
extern bool W3dConfigGuardar();
static PopupMenu* MenuIdioma = NULL;

static void AccionIdiomaElegido(int id){
    W3dIdiomaSet((W3dIdioma)id);
    extern bool g_idiomaForzado;
    g_idiomaForzado = true;   // lo eligio el usuario: el idioma del SO ya no manda
    Notificar(T("Restart Whisk3D for this change to take effect"), false);
    g_redraw = true;
}
static void AccionMenuIdioma(){
    if (!PropsActivo || !PropsActivo->propAjIdioma) return;
    if (!MenuIdioma){ MenuIdioma = new PopupMenu(); MenuIdioma->action = AccionIdiomaElegido; }
    MenuIdioma->Limpiar();
    // los idiomas se listan en SU PROPIO nombre y no traducidos: el que abre esto capaz no entiende el idioma actual
    MenuIdioma->Agregar("English",   (int)W3dLangEN);
    MenuIdioma->Agregar("Español",   (int)W3dLangES);
    MenuIdioma->Agregar("Portugues", (int)W3dLangPT);
    AbrirMenuBajoBoton(MenuIdioma, PropsActivo->propAjIdioma->button);
}

static PopupMenu* MenuBackend = NULL;
static void AccionBackendElegido(int id){
    cfg.graphicsAPI = (id == 1) ? "opengles" : "opengl";
    Notificar(T("Restart Whisk3D for this change to take effect"), false);
    g_redraw = true;
}
static void AccionMenuBackend(){
    if (!PropsActivo || !PropsActivo->propAjBackend) return;
    if (!MenuBackend){ MenuBackend = new PopupMenu(); MenuBackend->action = AccionBackendElegido; }
    MenuBackend->Limpiar();
    MenuBackend->Agregar("OpenGL", 0);
    MenuBackend->Agregar("OpenGL ES", 1);
    AbrirMenuBajoBoton(MenuBackend, PropsActivo->propAjBackend->button);
}

// Los skins que hay: carpetas dentro de res/Skins. Vive ACA y no en w3dFileSystem porque eso es del Core, y el
// Core no tiene por que saber que existe el concepto "skin del editor". Si no se puede listar (o no hay nada),
// queda al menos el que esta puesto: el dropdown nunca sale vacio.
static void W3dListarSkins(std::vector<std::string>& out){
    out.clear();
#ifndef W3D_SYMBIAN
    try {
        const std::string dir = w3dFileSystem::GetResDir() + "/Skins";
        for (std::filesystem::directory_iterator it(dir), fin; it != fin; ++it)
            if (it->is_directory()) out.push_back(it->path().filename().string());
    } catch (...) { }
#endif
    if (out.empty()) out.push_back(cfg.SkinName);
}

static PopupMenu* MenuSkin = NULL;
static void AccionSkinElegido(int id){
    std::vector<std::string> skins; W3dListarSkins(skins);
    if (id >= 0 && id < (int)skins.size()) cfg.SkinName = skins[id];
    Notificar(T("Restart Whisk3D for this change to take effect"), false);
    g_redraw = true;
}
static void AccionMenuSkin(){
    if (!PropsActivo || !PropsActivo->propAjSkin) return;
    if (!MenuSkin){ MenuSkin = new PopupMenu(); MenuSkin->action = AccionSkinElegido; }
    MenuSkin->Limpiar();
    std::vector<std::string> skins; W3dListarSkins(skins);
    for (size_t i = 0; i < skins.size(); i++) MenuSkin->Agregar(skins[i], (int)i);
    if (skins.empty()) MenuSkin->Agregar(cfg.SkinName, 0);
    AbrirMenuBajoBoton(MenuSkin, PropsActivo->propAjSkin->button);
}

// el tilde ya toco cfg.enableAntialiasing (PropBool escribe el bool): aca solo se avisa
static void AccionAntialias(){
    Notificar(T("Restart Whisk3D for this change to take effect"), false);
    g_redraw = true;
}

// ESCALA GLOBAL del editor (cfg.scale), cambiada EN VIVO desde Ajustes: re-deriva todas
// las metricas *GS y re-lay-outea el arbol de viewports. x1 = como se ve en el N95.
static float g_ajEscala = 3.0f;
static void AccionEscalaEditor(){
    int v = (int)(g_ajEscala + 0.5f);
    if (v < 1) v = 1;
    if (v > 6) v = 6;
    g_ajEscala = (float)v;
    cfg.scale = v;
    SetGlobalScale(v);
    if (rootViewport) rootViewport->Resize(winW, winH);
    g_redraw = true;
}

static void AccionGuardarConfig(){
    if (W3dConfigGuardar()) Notificar(T("Settings saved"), false);
    else                    Notificar(T("Could not write config.ini"), true);
}

static void AccionMenuMateriales(){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    if (!MenuMateriales) {
        MenuMateriales = new PopupMenu();
        MenuMateriales->action = AccionMaterialElegido;
    }
    MenuMateriales->Limpiar(); // la lista de materiales va cambiando
    MenuMateriales->Agregar(T("New Material"), 0, IconType::material);
    MenuMateriales->Agregar(T("Default Material"), 1, IconType::material);
    for (size_t i = 0; i < Materials.size(); i++) {
        MenuMateriales->Agregar(Materials[i]->name, 2 + (int)i, IconType::material);
    }
    AbrirMenuBajoBoton(MenuMateriales, PropsActivo->propBtnNewMaterial->button);
}

// ====================================================================
// STACK de MODIFICADORES: menu "Add" (los 5 tipos) + acciones Add/Remove/Move. El stack vive en el Mesh
// (editor); aca solo la UI. Por ahora NO se genera ninguna malla: solo se gestiona la lista y su orden.
// ====================================================================
static PopupMenu* MenuAddModifier = NULL;

static void AccionAddModifierElegido(int id){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    UndoModAgregar((Mesh*)ObjActivo, id); // id = ModifierType (Screw/Mirror/Array/SubSurf/Boolean). Ctrl+Z: puerta unica (Undo.h)
    SelEnListaModificador();                     // el nuevo queda seleccionado en el selector
    PropertiesLayoutDirty = true;                // re-layout (aparecen Remove / Move / la 2da tarjeta)
}
static void AccionMenuAddModifier(){
    if (!PropsActivo || !ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    if (!MenuAddModifier){
        MenuAddModifier = new PopupMenu();
        MenuAddModifier->action = AccionAddModifierElegido;
        MenuAddModifier->Agregar("Screw",               ModifierType::Screw);
        MenuAddModifier->Agregar(T("Mirror"),              ModifierType::Mirror);
        MenuAddModifier->Agregar("Array",               ModifierType::Array);
        MenuAddModifier->Agregar(T("Subdivision Surface"), ModifierType::SubdivisionSurface);
        MenuAddModifier->Agregar(T("Boolean"),             ModifierType::Boolean);
        MenuAddModifier->Agregar(T("Culling"),             ModifierType::CullingTri); // PVS por triangulo (estilo PS1)
        MenuAddModifier->Agregar(T("Armature"),            ModifierType::Armature, (int)IconType::armature);
    }
    if (PropsActivo->propRowMod && !PropsActivo->propRowMod->botones.empty()){
        AbrirMenuBajoBoton(MenuAddModifier, PropsActivo->propRowMod->botones[0]); // el boton "Add"
    }
}
static void AccionRemoveModifier(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    UndoModQuitar((Mesh*)ObjActivo); // Ctrl+Z: el modificador NO se libera, lo adopta el paso de undo (Undo.h)
    SelEnListaModificador();
    PropertiesLayoutDirty = true;
}
static void AccionModifierUp(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    UndoModMover((Mesh*)ObjActivo, -1); // sube en el stack (el orden importa)
    SelEnListaModificador();                  // mantiene seleccionado el modificador MOVIDO
    PropertiesLayoutDirty = true;
}
static void AccionModifierDown(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    UndoModMover((Mesh*)ObjActivo, +1); // baja en el stack
    SelEnListaModificador();
    PropertiesLayoutDirty = true;
}

// ====================================================================
// STACK de CONSTRAINTS (pestania 7): menu "Add" (los 3 tipos) + Add/Remove/Move. Calcado del de
// modificadores de arriba, con UNA diferencia de fondo: el stack NO es de la malla. Lo tiene
// CUALQUIER objeto (Objects.h), asi que en vez de castear ObjActivo a Mesh* se trabaja con el
// Object* pelado -- castear una luz o una camara a Mesh* seria leer basura.
// ====================================================================
// los tipos que NO viven en el espacio 3D: los elementos 2D (los acomoda el layout de la interfaz)
// y el objeto Script. Es la MISMA lista con la que ActualizarPestanias arma 'es2D || esScript', y
// esta aparte para que la pestania y sus acciones no puedan discrepar: si discreparan, el boton
// seguiria funcionando sobre un objeto cuya tarjeta ya no se ve.
static bool TipoSinTransform3D(int tipo){
    return tipo == (int)ObjectType::texto2d || tipo == (int)ObjectType::imagen2d ||
           tipo == (int)ObjectType::rect2d  || tipo == (int)ObjectType::cont2d   ||
           tipo == (int)ObjectType::slice9  || tipo == (int)ObjectType::boton2d  ||
           tipo == (int)ObjectType::expandir2d || tipo == (int)ObjectType::video2d ||
           tipo == (int)ObjectType::ui      || tipo == (int)ObjectType::script;
}
// el objeto cuyo stack muestra la pestania, o NULL. El 'ObjActivo != NULL' NO es opcional: sin
// seleccion no hay ninguna pestania de objeto (ver el fallback a la 0 en ActualizarPestanias).
static Object* ObjConstraintsUI(){
    if (!ObjActivo) return NULL;
    return TipoSinTransform3D((int)ObjActivo->getType()) ? NULL : ObjActivo;
}
static W3dConstraint* ConActivoUI(){
    Object* o = ObjConstraintsUI(); if (!o) return NULL;
    if (o->constraintActivo < 0 || o->constraintActivo >= (int)o->constraints.size()) return NULL;
    return o->constraints[o->constraintActivo];
}
// el selector del stack SIGUE al constraint activo tras add/remove/move (el movido queda
// resaltado, igual que en modificadores). No regenera nada: el stack se evalua al dibujar.
static void SelEnListaConstraint(){
    if (!PropsActivo || !PropsActivo->propListConstraints) return;
    Object* o = ObjConstraintsUI(); if (!o) return;
    PropsActivo->propListConstraints->selectIndex = o->constraintActivo;
    PropsActivo->propListConstraints->AjustarVentana();
    g_redraw = true;
}
static PopupMenu* MenuAddConstraint = NULL;
static void AccionAddConstraintElegido(int id){
    Object* o = ObjConstraintsUI(); if (!o) return;
    UndoConAgregar(o, id); // id = W3dConstraintTipo. Ctrl+Z: puerta unica (Undo.h)
    SelEnListaConstraint();
    PropertiesLayoutDirty = true;   // re-layout (aparecen Remove / Move / la 2da tarjeta)
}
static void AccionMenuAddConstraint(){
    if (!PropsActivo || !ObjConstraintsUI()) return;
    if (!MenuAddConstraint){
        MenuAddConstraint = new PopupMenu();
        MenuAddConstraint->action = AccionAddConstraintElegido;
        // los nombres de los tipos NO se traducen: son los MISMOS que van al .w3d y a la lista
        // (W3dNombreTipoConstraint, Core), y el nombre por defecto de la entrada sale de ahi.
        MenuAddConstraint->Agregar(W3dNombreTipoConstraint(W3dConstraintTipo::CopyLocation), W3dConstraintTipo::CopyLocation, (int)IconType::constraint);
        MenuAddConstraint->Agregar(W3dNombreTipoConstraint(W3dConstraintTipo::CopyRotation), W3dConstraintTipo::CopyRotation, (int)IconType::constraint);
        MenuAddConstraint->Agregar(W3dNombreTipoConstraint(W3dConstraintTipo::Billboard),    W3dConstraintTipo::Billboard,    (int)IconType::constraint);
    }
    if (PropsActivo->propRowCon && !PropsActivo->propRowCon->botones.empty())
        AbrirMenuBajoBoton(MenuAddConstraint, PropsActivo->propRowCon->botones[0]); // el boton "Add"
}
static void AccionRemoveConstraint(){
    Object* o = ObjConstraintsUI(); if (!o) return;
    UndoConQuitar(o); // Ctrl+Z: el constraint NO se libera, lo adopta el paso de undo (Undo.h)
    SelEnListaConstraint();
    PropertiesLayoutDirty = true;
}
static void AccionConstraintUp(){
    Object* o = ObjConstraintsUI(); if (!o) return;
    UndoConMover(o, -1); // sube en el stack (el orden importa: el de abajo pisa al de arriba)
    SelEnListaConstraint();
    PropertiesLayoutDirty = true;
}
static void AccionConstraintDown(){
    Object* o = ObjConstraintsUI(); if (!o) return;
    UndoConMover(o, +1);
    SelEnListaConstraint();
    PropertiesLayoutDirty = true;
}
// un parametro del constraint cambio (checkbox / influencia): NO hay nada que regenerar -- se
// evalua al dibujar, POR VISTA. Solo hay que redibujar.
static void AccionConParamChanged(){ g_redraw = true; }

// ---- "Source": los objetos de la escena + la opcion "la vista" (la camara que esta dibujando).
// EXCLUYE al propio objeto y a TODOS sus descendientes: es la 1ra barrera contra ciclos (la
// segunda, la de verdad, es la guardia de reentrada del evaluador). Saltear el subarbol entero
// es lo que saca a los descendientes: al no bajar por el objeto excluido, sus hijos no entran. ----
static std::vector<Object*> gConFuenteCand;   // id - 2 -> objeto (0 = None, 1 = la vista)
static void RecolectarFuentesCon(Object* nodo, Object* excluir){
    if (!nodo) return;
    for (size_t i = 0; i < nodo->Childrens.size(); i++){
        Object* c = nodo->Childrens[i];
        if (c == excluir) continue;   // el propio objeto Y su subarbol (no se recursa adentro)
        if (c->getType() != ObjectType::collection) gConFuenteCand.push_back(c);
        RecolectarFuentesCon(c, excluir);
    }
}
// gConFuenteCand sobrevive ENTRE FRAMES (se llena al abrir el menu y se lee al elegir una
// fila), asi que sus punteros son la unica cosa de la feature que no pasa por la puerta de
// refs de Object: si entre las dos cosas alguien borrara un objeto, elegirlo leeria memoria
// liberada. No hay camino conocido para llegar ahi (el menu es modal), pero el candidato se
// VALIDA igual contra la escena antes de tocarlo -- mismo criterio que ObjetoEnEscena en
// Undo.cpp: un objeto borrado esta DETACHADO del arbol, o sea que no aparece en este barrido.
// Se compara por PUNTERO y no por nombre a proposito: dos escenas pueden tener un objeto con
// el mismo nombre (el scope de nombres unicos es POR ESCENA) y el menu lista las dos.
static bool ConCandidatoVivo(const Object* o){
    if (!o) return false;
    std::vector<Object*> pila; if (SceneCollection) pila.push_back(SceneCollection);
    while (!pila.empty()){
        Object* n = pila.back(); pila.pop_back();
        if (n == o) return true;
        for (size_t i = 0; i < n->Childrens.size(); i++) pila.push_back(n->Childrens[i]);
    }
    return false;
}
static PopupMenu* MenuConFuente = NULL;
static void AccionConFuenteElegida(int id){
    W3dConstraint* c = ConActivoUI(); if (!c) return;
    if (id == 0){ // None: sigue siendo fuente de tipo OBJETO, sin objeto -> el constraint no hace nada
        c->fuenteTipo = W3dConstraintFuente::Objeto; c->fuenteObj = NULL; c->fuenteNombre.clear();
    } else if (id == 1){ // "la vista": tipo EXPLICITO, sin puntero ni nombre (ver W3dConstraint.h)
        c->fuenteTipo = W3dConstraintFuente::Vista; c->fuenteObj = NULL; c->fuenteNombre.clear();
    } else {
        int idx = id - 2;
        if (idx < 0 || idx >= (int)gConFuenteCand.size()) return;
        Object* o = gConFuenteCand[idx];
        if (!ConCandidatoVivo(o)) return;   // ya no esta en la escena: no se toca el constraint
        c->fuenteTipo = W3dConstraintFuente::Objeto;
        c->fuenteObj = o;
        c->fuenteNombre = o->name;   // el vinculo POR NOMBRE: esto es lo que se guarda
    }
    // la fuente CAMBIO -> el aviso de ciclo vuelve a estar disponible. El flag es "ya lo dije
    // para ESTA fuente", no "ya lo dije una vez en la vida del proceso": sin este reset, sacar
    // la fuente y volver a ponerla rearma el ciclo y el evaluador (Objects.cpp, W3dEvalCons)
    // no vuelve a avisar NUNCA MAS. Va aca, en el UNICO lugar donde el usuario elige la fuente.
    c->avisoCiclo = false;
    AccionConParamChanged();
}
static void AccionMenuConFuente(){
    Object* o = ObjConstraintsUI();
    if (!PropsActivo || !PropsActivo->propConFuente || !o || !ConActivoUI()) return;
    if (!MenuConFuente){ MenuConFuente = new PopupMenu(); MenuConFuente->action = AccionConFuenteElegida; }
    MenuConFuente->Limpiar();
    MenuConFuente->titulo = T("Source");
    MenuConFuente->Agregar(T("None"), 0);
    MenuConFuente->Agregar(T("View"), 1, (int)IconType::camera); // la camara que esta dibujando
    gConFuenteCand.clear(); RecolectarFuentesCon(SceneCollection, o);
    for (size_t i = 0; i < gConFuenteCand.size(); i++)
        MenuConFuente->Agregar(gConFuenteCand[i]->name, 2 + (int)i, (int)IconoDeObjeto(gConFuenteCand[i]));
    AbrirMenuBajoBoton(MenuConFuente, PropsActivo->propConFuente->button);
}
// ---- "Mode" del billboard. Las DOS opciones representables desde la UI (W3dConstraint.h):
// tipo arbol = solo yaw; mira a la camara = yaw + pitch. Las dos prenden bbYaw, asi que elegir
// cualquiera normaliza de paso el (!bbYaw && bbPitch) que un archivo hecho a mano podria traer. ----
static const char* ConNombreModoBB(const W3dConstraint* c){
    return (c && c->bbPitch) ? T("Face the camera") : T("Upright (does not tilt)");
}
static PopupMenu* MenuConBBModo = NULL;
static void AccionConBBModoElegido(int id){
    W3dConstraint* c = ConActivoUI(); if (!c) return;
    c->bbYaw = true; c->bbPitch = (id == 1);
    AccionConParamChanged();
}
static void AccionMenuConBBModo(){
    if (!PropsActivo || !PropsActivo->propConBBModo || !ConActivoUI()) return;
    if (!MenuConBBModo){ MenuConBBModo = new PopupMenu(); MenuConBBModo->action = AccionConBBModoElegido; }
    MenuConBBModo->Limpiar();
    MenuConBBModo->titulo = T("Mode");
    MenuConBBModo->Agregar(T("Upright (does not tilt)"), 0);
    MenuConBBModo->Agregar(T("Face the camera"), 1);
    AbrirMenuBajoBoton(MenuConBBModo, PropsActivo->propConBBModo->button);
}


// ====================================================================
// selector de MODO de rotacion (XYZ Euler / Quaternion / Axis Angle)
// ==================== TEXTO 2D (Editor 2D) ====================
static Texto2D* T2dActivo(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::texto2d) ? (Texto2D*)ObjActivo : NULL;
}
static const char* T2dNombreAlign(int v, bool horizontal){
    if (horizontal) return v==0 ? "Izquierda" : (v==1 ? "Centro" : "Derecha");
    return v==0 ? "Arriba" : (v==1 ? "Centro" : "Abajo");
}
static PopupMenu* MenuT2dAlignH = NULL;
static PopupMenu* MenuT2dAlignV = NULL;
static PopupMenu* MenuT2dFuente = NULL;
static PopupMenu* MenuT2dAncla  = NULL;

static const char* T2dNombreAncla(int v){
    switch (v){
        case 1: return "Izquierda";        case 2: return "Derecha";
        case 3: return "Arriba";           case 4: return "Abajo";
        case 5: return "Arriba-Izquierda"; case 6: return "Arriba-Derecha";
        case 7: return "Abajo-Izquierda";  case 8: return "Abajo-Derecha";
        default: return "Centro";
    }
}
static void AccionT2dAnclaElegida(int id){
    Texto2D* t = T2dActivo(); if (!t) return;
    // el ancla cambia SOLO el punto de referencia: X, Y y Z quedan como estan
    // (el elemento salta al nuevo punto, sin tocarle los valores)
    t->ancla = id;
    if (PropsActivo && PropsActivo->propT2dAncla) PropsActivo->propT2dAncla->button->text = T2dNombreAncla(id);
    g_redraw = true;
}
static void AccionMenuT2dAncla(){
    if (!PropsActivo || !T2dActivo()) return;
    if (!MenuT2dAncla){ MenuT2dAncla = new PopupMenu(); MenuT2dAncla->action = AccionT2dAnclaElegida; }
    MenuT2dAncla->Limpiar();
    MenuT2dAncla->titulo = T("Anchor");
    for (int i = 0; i <= 8; i++) MenuT2dAncla->Agregar(T2dNombreAncla(i), i);
    AbrirMenuBajoBoton(MenuT2dAncla, PropsActivo->propT2dAncla->button);
}

static void AccionT2dAlignHElegido(int id){
    Texto2D* t = T2dActivo(); if (!t) return;
    t->alignH = id;
    if (PropsActivo && PropsActivo->propT2dAlignH) PropsActivo->propT2dAlignH->button->text = T2dNombreAlign(id, true);
    g_redraw = true;
}
static void AccionT2dAlignVElegido(int id){
    Texto2D* t = T2dActivo(); if (!t) return;
    t->alignV = id;
    if (PropsActivo && PropsActivo->propT2dAlignV) PropsActivo->propT2dAlignV->button->text = T2dNombreAlign(id, false);
    g_redraw = true;
}
static void AccionMenuT2dAlignH(){
    if (!PropsActivo || !T2dActivo()) return;
    if (!MenuT2dAlignH){ MenuT2dAlignH = new PopupMenu(); MenuT2dAlignH->action = AccionT2dAlignHElegido; }
    MenuT2dAlignH->Limpiar();
    MenuT2dAlignH->Agregar("Izquierda", 0); MenuT2dAlignH->Agregar("Centro", 1); MenuT2dAlignH->Agregar("Derecha", 2);
    AbrirMenuBajoBoton(MenuT2dAlignH, PropsActivo->propT2dAlignH->button);
}
static void AccionMenuT2dAlignV(){
    if (!PropsActivo || !T2dActivo()) return;
    if (!MenuT2dAlignV){ MenuT2dAlignV = new PopupMenu(); MenuT2dAlignV->action = AccionT2dAlignVElegido; }
    MenuT2dAlignV->Limpiar();
    MenuT2dAlignV->Agregar("Arriba", 0); MenuT2dAlignV->Agregar("Centro", 1); MenuT2dAlignV->Agregar("Abajo", 2);
    AbrirMenuBajoBoton(MenuT2dAlignV, PropsActivo->propT2dAlignV->button);
}
// FUENTE: la de Whisk3D o un .ttf elegido con el file browser (se hornea al vuelo, ver Fuente2D)
static void T2dFuenteElegida(const std::string& rutaElegida){
    Texto2D* t = T2dActivo(); if (!t) return;
    // IMPORTAR = COPIAR ADENTRO: el archivo que el usuario acaba de elegir de su
    // disco pasa a ser del proyecto en el acto y se referencia como INTERNO
    // ("texturas/piso.png"). Sin contenedor montado (proyecto v3) no cambia nada.
    const std::string ruta = W3dImportarAsset(rutaElegida);
    t->fuente = ruta;
    if (PropsActivo && PropsActivo->propT2dFuente) PropsActivo->propT2dFuente->button->text = Fuente2DNombre(ruta);
    g_redraw = true;
}
static void AccionT2dFuenteElegida(int id){
    Texto2D* t = T2dActivo(); if (!t) return;
    if (id == 0) { T2dFuenteElegida(""); }                                    // la fuente de Whisk3D
    else AbrirFileBrowser(T("Load font"), T("Open"), ".ttf .otf", T2dFuenteElegida);
}
static void AccionMenuT2dFuente(){
    if (!PropsActivo || !T2dActivo()) return;
    if (!MenuT2dFuente){ MenuT2dFuente = new PopupMenu(); MenuT2dFuente->action = AccionT2dFuenteElegida; }
    MenuT2dFuente->Limpiar();
    MenuT2dFuente->Agregar("Whisk3D", 0);
    MenuT2dFuente->Agregar(T("Load font") + std::string("..."), 1);
    AbrirMenuBajoBoton(MenuT2dFuente, PropsActivo->propT2dFuente->button);
}

// ============================ IMAGEN 2D ============================
static Imagen2D* Img2dActiva(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::imagen2d) ? (Imagen2D*)ObjActivo : NULL;
}
static const char* ImgNombreModo(int m){
    return m == 1 ? "Ajustar" : (m == 2 ? "Cover" : "Estirar");
}
// nombre de archivo pelado (para el boton de la textura)
static std::string NombreDeArchivo(const std::string& ruta){
    size_t b = ruta.find_last_of("/\\");
    return (b == std::string::npos) ? ruta : ruta.substr(b + 1);
}
static PopupMenu* MenuImgModo = NULL;
static void AccionImgModoElegido(int id){
    Imagen2D* im = Img2dActiva(); if (!im) return;
    im->modo = id;
    if (PropsActivo && PropsActivo->propImgModo) PropsActivo->propImgModo->button->text = ImgNombreModo(id);
    g_redraw = true;
}
static void AccionMenuImgModo(){
    if (!PropsActivo || !Img2dActiva()) return;
    if (!MenuImgModo){ MenuImgModo = new PopupMenu(); MenuImgModo->action = AccionImgModoElegido; }
    MenuImgModo->Limpiar();
    MenuImgModo->titulo = T("Mode");
    MenuImgModo->Agregar("Estirar", 0);   // deforma para llenar el rect
    MenuImgModo->Agregar("Ajustar", 1);   // entera, con bandas
    MenuImgModo->Agregar("Cover", 2);     // llena recortando
    AbrirMenuBajoBoton(MenuImgModo, PropsActivo->propImgModo->button);
}
static PopupMenu* MenuImgAncla = NULL;
static void AccionImgAnclaElegida(int id){
    Imagen2D* im = Img2dActiva(); if (!im) return;
    im->ancla = id;   // igual que el texto: el ancla NO toca X/Y/Z
    if (PropsActivo && PropsActivo->propImgAncla) PropsActivo->propImgAncla->button->text = T2dNombreAncla(id);
    g_redraw = true;
}
static void AccionMenuImgAncla(){
    if (!PropsActivo || !Img2dActiva()) return;
    if (!MenuImgAncla){ MenuImgAncla = new PopupMenu(); MenuImgAncla->action = AccionImgAnclaElegida; }
    MenuImgAncla->Limpiar();
    MenuImgAncla->titulo = T("Anchor");
    for (int i = 0; i <= 8; i++) MenuImgAncla->Agregar(T2dNombreAncla(i), i);
    AbrirMenuBajoBoton(MenuImgAncla, PropsActivo->propImgAncla->button);
}
// TEXTURA: elegir el archivo con el file browser; una imagen recien creada toma su tamano natural
static void ImgTexturaElegida(const std::string& rutaElegida){
    Imagen2D* im = Img2dActiva(); if (!im) return;
    // IMPORTAR = COPIAR ADENTRO: el archivo que el usuario acaba de elegir de su
    // disco pasa a ser del proyecto en el acto y se referencia como INTERNO
    // ("texturas/piso.png"). Sin contenedor montado (proyecto v3) no cambia nada.
    const std::string ruta = W3dImportarAsset(rutaElegida);
    im->textura = ruta;
    int w = 0, h = 0;
    if (Textura2DObtener(ruta, &w, &h) && w > 0 && h > 0) { im->ancho = (float)w; im->alto = (float)h; }
    if (PropsActivo && PropsActivo->propImgTextura)
        PropsActivo->propImgTextura->button->text = NombreDeArchivo(ruta);
    g_redraw = true;
}
static void AccionImgTextura(){
    if (!Img2dActiva()) return;
    AbrirFileBrowser(T("Load image"), T("Open"), ".png .jpg .jpeg .bmp .tga .gif", ImgTexturaElegida);
}

// ============================ UI RESPONSIVE ============================
static UI* UIActivaProps(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::ui) ? (UI*)ObjActivo : NULL;
}
static const char* UINombreRes(int p){
    return p == 2160 ? "4k" : p == 1080 ? "1080p" : p == 720 ? "720p" : p == 480 ? "480p" : "240p";
}
static const char* UINombreAspecto(int a){
    return a == 0 ? "16:9" : (a == 1 ? "4:3" : "1:1");
}
static void AccionUIigualRender(){   // onChange del checkbox "como el render"
    UI* u = UIActivaProps(); if (!u) return;
    // recien pasado a RESPONSIVE: arranca del tamano actual del render (continuidad visual)
    if (!u->igualQueRender) UI2D_TamanoVentana(&u->ancho, &u->alto);
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: muestra/oculta las filas responsive
    g_redraw = true;
}
static PopupMenu* MenuUIres = NULL;
static void AccionUIresElegida(int id){
    UI* u = UIActivaProps(); if (!u) return;
    u->resPreset = id;
    u->AplicarPreset();
    if (PropsActivo && PropsActivo->propUIres) PropsActivo->propUIres->button->text = UINombreRes(id);
    g_redraw = true;
}
static void AccionMenuUIres(){
    if (!PropsActivo || !UIActivaProps()) return;
    if (!MenuUIres){ MenuUIres = new PopupMenu(); MenuUIres->action = AccionUIresElegida; }
    MenuUIres->Limpiar();
    MenuUIres->titulo = T("Resolution");
    MenuUIres->Agregar("4k", 2160);
    MenuUIres->Agregar("1080p", 1080);
    MenuUIres->Agregar("720p", 720);
    MenuUIres->Agregar("480p", 480);
    MenuUIres->Agregar("240p", 240);    // para simular el Nokia (con 4:3 y Rotar: 240x320)
    AbrirMenuBajoBoton(MenuUIres, PropsActivo->propUIres->button);
}
static PopupMenu* MenuUIaspecto = NULL;
static void AccionUIaspectoElegido(int id){
    UI* u = UIActivaProps(); if (!u) return;
    u->aspectoPreset = id;
    u->AplicarPreset();
    if (PropsActivo && PropsActivo->propUIaspecto) PropsActivo->propUIaspecto->button->text = UINombreAspecto(id);
    g_redraw = true;
}
static void AccionMenuUIaspecto(){
    if (!PropsActivo || !UIActivaProps()) return;
    if (!MenuUIaspecto){ MenuUIaspecto = new PopupMenu(); MenuUIaspecto->action = AccionUIaspectoElegido; }
    MenuUIaspecto->Limpiar();
    MenuUIaspecto->titulo = T("Aspect");
    MenuUIaspecto->Agregar("16:9", 0);
    MenuUIaspecto->Agregar("4:3", 1);
    MenuUIaspecto->Agregar("1:1", 2);
    AbrirMenuBajoBoton(MenuUIaspecto, PropsActivo->propUIaspecto->button);
}
// EXPORTAR el UI activo como .w3dui: elegis la carpeta y se escribe <nombre>.w3dui
static void UIExportCarpetaElegida(const std::string& carpeta){
    UI* u = UIActivaProps(); if (!u) return;
    // "Usar carpeta actual" devuelve la CARPETA -> carpeta + nombre de la UI + ".w3dui".
    // (antes se concatenaba con "/" a mano: en la raiz salia "//menu.w3dui")
    std::string ruta = W3dRutaDeSalida(carpeta, u->name, ".w3dui");
    if (UI2DGuardar(u, ruta)) Notificar(std::string(T("Saved: ")) + ruta, false);
    else                      Notificar(T("Could not write the file"), true);
}
// el techo del cache de estados (espejo float del int de SimJuego)
extern int gSimCacheMax;   // SimJuego.cpp
static float g_simCacheF = 250.0f;
static void AccionSimCache(){
    gSimCacheMax = (int)(g_simCacheF + 0.5f);
    if (gSimCacheMax < 10) gSimCacheMax = 10;
}
// Compilar juego: exporta + genera + compila EN SEGUNDO PLANO (worker): el editor no
// se congela; el progreso (etapa + %) lo muestra la barra overlay de abajo y el
// resultado llega por notificacion. Si ya hay un build corriendo, CompilarJuego avisa
// y no dispara otro. Usa el UI activo o el primero de la escena (boton tarjeta Juego).
// La CONFIG entera de la tarjeta (plataforma, modo ventana, orientacion, assets,
// fisica/sonido/debug) vive en g_proyCompilar (GuardarW3D.h): viaja con el
// PROYECTO en el bloque raiz "compilar" del .w3d, se carga al abrir (import_w3d)
// y se escribe al guardar con los valores vigentes. Antes eran statics de aca y
// se reseteaban en cada arranque (el modo ventana elegido se perdia).
// nombres VISIBLES de cada valor (los del JSON son otros: ver W3dCompilar*Str)
static const char* NombrePlat(int p){ return (p==3)?"Android":(p==2)?"WebGL":(p==1)?"Linux AppImage":"Linux .deb"; }
static const char* NombreModoVent(int m){ return (m==2)?"Sin bordes":(m==0)?"Ventana":"Pantalla completa"; }
// la ORIENTACION solo afecta a Android (manifest + hint SDL); en desktop/web no aplica.
static const char* NombreOrientacion(int o){ return (o==2)?"Solo horizontal":(o==1)?"Solo vertical":"Todas"; }
// ASSETS: Sueltos = archivos visibles y editables al lado del binario (modding
// facil). Empaquetados = protegidos, DENTRO del binario, ofuscados (pak.cpp).
static const char* NombreAssetsModo(int m){ return (m==1)?"Empaquetados (protegidos)":"Sueltos (editables)"; }
static UI* UIParaCompilar(){
    UI* u = (ObjActivo && ObjActivo->getType() == ObjectType::ui) ? (UI*)ObjActivo : NULL;
    if (!u && SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
            if (SceneCollection->Childrens[i]->getType() == ObjectType::ui)
                { u = (UI*)SceneCollection->Childrens[i]; break; }
    return u;
}
static void AccionCompilarJuego(){
    extern bool CompilarJuego(UI*, int, int, int, bool, bool, bool, bool);
    UI* u = UIParaCompilar();
    if (u) CompilarJuego(u, g_proyCompilar.plataforma, g_proyCompilar.modoVentana,
                         g_proyCompilar.orientacion, g_proyCompilar.usarFisica,
                         g_proyCompilar.usarSonido, g_proyCompilar.modoDebug,
                         g_proyCompilar.assetsModo == 1);
    else Notificar("Compilar: no hay ninguna UI en la escena", true);
}
static PopupMenu* MenuPlat = NULL;
static void AccionPlatElegida(int id){
    g_proyCompilar.plataforma = id;   // queda en el .w3d al guardar el proyecto
    if (PropsActivo && PropsActivo->propJuegoPlat) PropsActivo->propJuegoPlat->button->text = NombrePlat(id);
    g_redraw = true;
}
static void AccionMenuPlat(){
    if (!PropsActivo || !PropsActivo->propJuegoPlat) return;
    if (!MenuPlat){ MenuPlat = new PopupMenu(); MenuPlat->action = AccionPlatElegida; }
    MenuPlat->Limpiar();
    MenuPlat->titulo = "Plataforma";
    MenuPlat->Agregar("Linux .deb", 0);
    MenuPlat->Agregar("Linux AppImage", 1);
    MenuPlat->Agregar("WebGL", 2);
    MenuPlat->Agregar("Android", 3);
    AbrirMenuBajoBoton(MenuPlat, PropsActivo->propJuegoPlat->button);
}
// desplegable "Modo ventana": como arranca la ventana del juego COMPILADO
static PopupMenu* MenuModoVent = NULL;
static void AccionModoVentElegido(int id){
    g_proyCompilar.modoVentana = id;   // queda en el .w3d al guardar el proyecto
    if (PropsActivo && PropsActivo->propJuegoModoVent) PropsActivo->propJuegoModoVent->button->text = NombreModoVent(id);
    g_redraw = true;
}
static void AccionMenuModoVent(){
    if (!PropsActivo || !PropsActivo->propJuegoModoVent) return;
    if (!MenuModoVent){ MenuModoVent = new PopupMenu(); MenuModoVent->action = AccionModoVentElegido; }
    MenuModoVent->Limpiar();
    MenuModoVent->titulo = "Modo ventana";
    MenuModoVent->Agregar("Ventana", 0);
    MenuModoVent->Agregar("Pantalla completa", 1);
    MenuModoVent->Agregar("Sin bordes", 2);
    AbrirMenuBajoBoton(MenuModoVent, PropsActivo->propJuegoModoVent->button);
}
// desplegable "Orientacion": a que orientacion queda clavado el juego COMPILADO.
// En Android va al manifest (screenOrientation) + hint SDL; desktop/web no aplica.
// (MenuOrientJuego: "MenuOrient" a secas ya existe, es el Orient del transform 3D)
static PopupMenu* MenuOrientJuego = NULL;
static void AccionOrientElegida(int id){
    g_proyCompilar.orientacion = id;   // queda en el .w3d al guardar el proyecto
    if (PropsActivo && PropsActivo->propJuegoOrient) PropsActivo->propJuegoOrient->button->text = NombreOrientacion(id);
    g_redraw = true;
}
static void AccionMenuOrient(){
    if (!PropsActivo || !PropsActivo->propJuegoOrient) return;
    if (!MenuOrientJuego){ MenuOrientJuego = new PopupMenu(); MenuOrientJuego->action = AccionOrientElegida; }
    MenuOrientJuego->Limpiar();
    MenuOrientJuego->titulo = "Orientacion";
    MenuOrientJuego->Agregar("Todas", 0);
    MenuOrientJuego->Agregar("Solo vertical", 1);
    MenuOrientJuego->Agregar("Solo horizontal", 2);
    AbrirMenuBajoBoton(MenuOrientJuego, PropsActivo->propJuegoOrient->button);
}
// desplegable "Assets": como viajan los archivos del juego COMPILADO (sueltos y
// editables como siempre, o empaquetados/ofuscados dentro del binario)
static PopupMenu* MenuAssetsJuego = NULL;
static void AccionAssetsElegido(int id){
    g_proyCompilar.assetsModo = id;   // queda en el .w3d al guardar el proyecto
    if (PropsActivo && PropsActivo->propJuegoAssets) PropsActivo->propJuegoAssets->button->text = NombreAssetsModo(id);
    g_redraw = true;
}
static void AccionMenuAssets(){
    if (!PropsActivo || !PropsActivo->propJuegoAssets) return;
    if (!MenuAssetsJuego){ MenuAssetsJuego = new PopupMenu(); MenuAssetsJuego->action = AccionAssetsElegido; }
    MenuAssetsJuego->Limpiar();
    MenuAssetsJuego->titulo = "Assets";
    MenuAssetsJuego->Agregar("Sueltos (editables)", 0);
    MenuAssetsJuego->Agregar("Empaquetados (protegidos)", 1);
    AbrirMenuBajoBoton(MenuAssetsJuego, PropsActivo->propJuegoAssets->button);
}
// ICONO del juego: un PNG con alpha en su MAXIMA definicion; Compilar juego genera
// de ahi los tamanos chicos (hicolor del .deb, mipmaps del APK, icono de ventana).
// Se persiste en el .w3d como ruta EXTERNA relativa (g_proyIcono, ver GuardarW3D.h).
static std::string NombreIconoJuego(){
    if (g_proyIcono.empty()) return "(ninguno)";
    size_t s = g_proyIcono.find_last_of("/\\");
    return (s == std::string::npos) ? g_proyIcono : g_proyIcono.substr(s + 1);
}
static void AccionIconoElegido(const std::string& rutaElegida){
    // IMPORTAR = COPIAR ADENTRO: el archivo que el usuario acaba de elegir de su
    // disco pasa a ser del proyecto en el acto y se referencia como INTERNO
    // ("texturas/piso.png"). Sin contenedor montado (proyecto v3) no cambia nada.
    const std::string ruta = W3dImportarAsset(rutaElegida);
    std::string r = ruta;
    // relativa al .w3d si el archivo cae bajo su carpeta (asi el proyecto viaja entero)
    if (!w3dPath.empty()){
        size_t s = w3dPath.find_last_of("/\\");
        std::string dir = (s == std::string::npos) ? std::string() : w3dPath.substr(0, s + 1);
        if (!dir.empty() && r.size() > dir.size() && r.compare(0, dir.size(), dir) == 0)
            r = r.substr(dir.size());
    }
    g_proyIcono = r;
    if (PropsActivo && PropsActivo->propJuegoIcono)
        PropsActivo->propJuegoIcono->button->text = NombreIconoJuego();
    g_redraw = true;
}
static void AccionMenuIcono(){
    AbrirFileBrowser("Icono del juego", "Elegir", ".png", AccionIconoElegido);
}
static void AccionUIexportar(){
    if (!UIActivaProps()) return;
    AbrirFileBrowser(T("Export UI to..."), T("Use this file"), ".w3dui", UIExportCarpetaElegida, true);
}

static void AccionUIrotar(){   // el ancho se vuelve el alto y viceversa
    UI* u = UIActivaProps(); if (!u) return;
    u->Rotar();
    g_redraw = true;
}

// ============================ POSICION RELATIVA / ABSOLUTA ============================
// por defecto la posicion X/Y se muestra RELATIVA al tamano de la UI (1.0 = todo el ancho,
// 0.5 = la mitad); el checkbox "Pixels" la pasa a pixeles absolutos. Los campos editan un
// PROXY que se sincroniza por frame y el onChange escribe de vuelta en pos.
static bool  g_pos2dAbs = false;
static float g_pos2dX = 0.0f, g_pos2dY = 0.0f;
static Object* ElemActivo2D(){
    return (ObjActivo && UI2D_EsElemento2D(ObjActivo)) ? ObjActivo : NULL;
}
static void AccionPos2DEditada(){
    Object* o = ElemActivo2D(); if (!o) return;
    // la posicion GUARDADA es RELATIVA (el numero que no se toca); el modo px es la
    // vista "final": lo tipeado en px se convierte de vuelta a relativo
    float vw, vh; UI2D_TamanoLienzo(&vw, &vh);
    o->pos.x = g_pos2dAbs ? (vw > 0.0f ? g_pos2dX / vw : 0.0f) : g_pos2dX;
    o->pos.y = g_pos2dAbs ? (vh > 0.0f ? g_pos2dY / vh : 0.0f) : g_pos2dY;
    g_redraw = true;
}
static void AccionPos2DAbsToggle(){ g_redraw = true; }   // el sync por frame rehace el proxy

// ============================ TIPO DEL TEXTO ============================
static const char* T2dNombreTipo(int t){
    return t == 1 ? "Number" : (t == 2 ? "Float" : "String");
}
static PopupMenu* MenuT2dTipo = NULL;
static void AccionT2dTipoElegido(int id){
    Texto2D* t = T2dActivo(); if (!t) return;
    t->tipo = id;
    if (PropsActivo && PropsActivo->propT2dTipo) PropsActivo->propT2dTipo->button->text = T2dNombreTipo(id);
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: Decimales aparece solo en float
    g_redraw = true;
}
static void AccionMenuT2dTipo(){
    if (!PropsActivo || !T2dActivo()) return;
    if (!MenuT2dTipo){ MenuT2dTipo = new PopupMenu(); MenuT2dTipo->action = AccionT2dTipoElegido; }
    MenuT2dTipo->Limpiar();
    MenuT2dTipo->titulo = T("Type");
    MenuT2dTipo->Agregar("String", 0);   // tal cual se escribe
    MenuT2dTipo->Agregar("Number", 1);   // entero
    MenuT2dTipo->Agregar("Float", 2);    // con decimales configurables
    AbrirMenuBajoBoton(MenuT2dTipo, PropsActivo->propT2dTipo->button);
}

// ============================ LINEAS DEL TEXTO ============================
static const char* T2dNombreLineas(int l){
    return l == 1 ? "Por palabras" : (l == 2 ? "En cualquier parte" : "Una linea");
}
static PopupMenu* MenuT2dLineas = NULL;
static void AccionT2dLineasElegido(int id){
    Texto2D* t = T2dActivo(); if (!t) return;
    t->lineas = id;
    if (PropsActivo && PropsActivo->propT2dLineas) PropsActivo->propT2dLineas->button->text = T2dNombreLineas(id);
    g_redraw = true;
}
static void AccionMenuT2dLineas(){
    if (!PropsActivo || !T2dActivo()) return;
    if (!MenuT2dLineas){ MenuT2dLineas = new PopupMenu(); MenuT2dLineas->action = AccionT2dLineasElegido; }
    MenuT2dLineas->Limpiar();
    MenuT2dLineas->titulo = T("Lines");
    MenuT2dLineas->Agregar("Una linea", 0);            // todo junto, sin saltos
    MenuT2dLineas->Agregar("Por palabras", 1);         // salta en los espacios (como css)
    MenuT2dLineas->Agregar("En cualquier parte", 2);   // salta donde haga falta
    AbrirMenuBajoBoton(MenuT2dLineas, PropsActivo->propT2dLineas->button);
}

// ============================ RECTANGULO 2D ============================
static Rect2D* Rect2dActivo(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::rect2d) ? (Rect2D*)ObjActivo : NULL;
}
// ============================ LAYOUT DE LOS HIJOS ============================
static const char* HijosNombreLayout(int l){
    return l == 1 ? "Filas" : (l == 2 ? "Columnas" : "Libremente");
}
static int* HijosLayoutDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->layoutHijos;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->layoutHijos;
    return NULL;
}
static float* HijosGapDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->gap;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->gap;
    return NULL;
}
static bool* HijosClipXDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->recortaX;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->recortaX;
    return NULL;
}
static bool* HijosClipYDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->recortaY;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->recortaY;
    return NULL;
}
static bool* HijosScrollDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->conScroll;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->conScroll;
    return NULL;
}
static float* HijosScrollXDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->scrollX;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->scrollX;
    return NULL;
}
static float* HijosScrollYDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->scrollY;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->scrollY;
    return NULL;
}
static bool* HijosPadGapPxDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->padGapPx;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->padGapPx;
    return NULL;
}
static void AccionHijosRefrescar(){
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: filas visibles cambian
    g_redraw = true;
}
// ============ UNIDAD del tamano (tamModo): fraccion / pixeles / escalado ============
static const char* TamNombreModo(int m){
    return m == TAM2D_FRACCION ? "Fraccion"
         : (m == TAM2D_ESCALADO ? "Escalado" : "Pixeles");
}
// el desplegable de Unidad de la tarjeta del tipo ACTIVO (imagen/rect/cont/slice9/video)
static PropButton* PropUnidadDelActivo(){
    if (!PropsActivo || !ObjActivo) return NULL;
    ObjectType t = ObjActivo->getType();
    if (t == ObjectType::imagen2d) return PropsActivo->propImgUnidad;
    if (t == ObjectType::rect2d)   return PropsActivo->propRectUnidad;
    if (t == ObjectType::cont2d)   return PropsActivo->propContUnidad;
    if (t == ObjectType::slice9)   return PropsActivo->propS9Unidad;
    if (t == ObjectType::video2d)  return PropsActivo->propVidUnidad;
    return NULL;
}
static PopupMenu* MenuTamModo = NULL;
static void AccionTamModoElegido(int id){
    if (!ObjActivo || !UI2D_EsElemento2D(ObjActivo)) return;
    ((Elemento2D*)ObjActivo)->tamModo = id;
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: unidades y rangos cambian
    g_redraw = true;
}
static void AccionMenuTamModo(){
    PropButton* pb = PropUnidadDelActivo();
    if (!pb || !UI2D_EsElemento2D(ObjActivo)) return;
    if (!MenuTamModo){ MenuTamModo = new PopupMenu(); MenuTamModo->action = AccionTamModoElegido; }
    MenuTamModo->Limpiar();
    MenuTamModo->titulo = T("Unit");
    MenuTamModo->Agregar("Fraccion", TAM2D_FRACCION);   // fraccion del rect del padre
    MenuTamModo->Agregar("Pixeles", TAM2D_PX);          // px del lienzo, tal cual
    MenuTamModo->Agregar("Escalado", TAM2D_ESCALADO);   // px x escala de pantalla (tipo dpi)
    AbrirMenuBajoBoton(MenuTamModo, pb->button);
}
// ajusta unidad/rango/pasos de una fila Width/Height segun la unidad (tamModo)
static void AjustarFilaTam(PropFloat* f, int modo){
    if (!f) return;
    bool px = (modo != TAM2D_FRACCION);   // pixeles y escalado editan en px
    f->unit = (modo == TAM2D_PX) ? "px" : (modo == TAM2D_ESCALADO ? "esc" : "");
    f->SetRango(px ? 1.0f : 0.005f, px ? 8192.0f : 8.0f);
    f->stepFino   = px ? 1.0f  : 0.005f;
    f->stepGrueso = px ? 10.0f : 0.05f;
    f->dragStep   = px ? 1.0f  : 0.002f;
}
static void AccionHijosPxToggle(){
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: unidades y rangos cambian
    g_redraw = true;
}
static const char* HijosNombreAjuste(int a){ return a == 1 ? "Minimo" : "Estirar"; }
static const char* HijosNombreAlign(int a){
    return a == 1 ? "Centro" : (a == 2 ? "Fin" : "Inicio");
}
static int* HijosAjusteDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->layoutAjuste;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->layoutAjuste;
    return NULL;
}
static int* HijosAlignDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->layoutAlign;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->layoutAlign;
    return NULL;
}
static PopupMenu* MenuHijosAjuste = NULL;
static void AccionHijosAjusteElegido(int id){
    int* a = HijosAjusteDe(ObjActivo); if (!a) return;
    *a = id;
    if (PropsActivo && PropsActivo->propHijosAjuste)
        PropsActivo->propHijosAjuste->button->text = HijosNombreAjuste(id);
    if (PropsActivo) PropsActivo->target = NULL;
    g_redraw = true;
}
static void AccionMenuHijosAjuste(){
    if (!PropsActivo || !HijosAjusteDe(ObjActivo)) return;
    if (!MenuHijosAjuste){ MenuHijosAjuste = new PopupMenu(); MenuHijosAjuste->action = AccionHijosAjusteElegido; }
    MenuHijosAjuste->Limpiar();
    MenuHijosAjuste->titulo = T("Fit");
    MenuHijosAjuste->Agregar("Estirar", 0);   // se reparten el 100% por peso
    MenuHijosAjuste->Agregar("Minimo", 1);    // cada uno su tamano; Expandir absorbe el resto
    AbrirMenuBajoBoton(MenuHijosAjuste, PropsActivo->propHijosAjuste->button);
}
static PopupMenu* MenuHijosAlign = NULL;
static void AccionHijosAlignElegido(int id){
    int* a = HijosAlignDe(ObjActivo); if (!a) return;
    *a = id;
    if (PropsActivo && PropsActivo->propHijosAlign)
        PropsActivo->propHijosAlign->button->text = HijosNombreAlign(id);
    g_redraw = true;
}
static void AccionMenuHijosAlign(){
    if (!PropsActivo || !HijosAlignDe(ObjActivo)) return;
    if (!MenuHijosAlign){ MenuHijosAlign = new PopupMenu(); MenuHijosAlign->action = AccionHijosAlignElegido; }
    MenuHijosAlign->Limpiar();
    MenuHijosAlign->titulo = T("Align");
    MenuHijosAlign->Agregar("Inicio", 0);
    MenuHijosAlign->Agregar("Centro", 1);
    MenuHijosAlign->Agregar("Fin", 2);
    AbrirMenuBajoBoton(MenuHijosAlign, PropsActivo->propHijosAlign->button);
}
// DISTRIBUCION del eje principal (estilo css). Solo aplica con ajuste MINIMO: con
// ESTIRAR los hijos ocupan el 100% y no hay sobrante que repartir (el desplegable
// ni se muestra). Con un modo distinto de Gap el align y los Expandir no aplican.
static const char* HijosNombreDistrib(int d){
    return d == 1 ? "Space-between" : (d == 2 ? "Space-around"
         : (d == 3 ? "Space-evenly" : "Gap"));
}
static int* HijosDistribDe(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::ui) return &((UI*)o)->distribucion;
    if (UI2D_EsElemento2D(o))           return &((Elemento2D*)o)->distribucion;
    return NULL;
}
static PopupMenu* MenuHijosDistrib = NULL;
static void AccionHijosDistribElegido(int id){
    int* d = HijosDistribDe(ObjActivo); if (!d) return;
    *d = id;
    if (PropsActivo && PropsActivo->propHijosDistrib)
        PropsActivo->propHijosDistrib->button->text = HijosNombreDistrib(id);
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: el align aparece/desaparece
    g_redraw = true;
}
static void AccionMenuHijosDistrib(){
    if (!PropsActivo || !HijosDistribDe(ObjActivo)) return;
    if (!MenuHijosDistrib){ MenuHijosDistrib = new PopupMenu(); MenuHijosDistrib->action = AccionHijosDistribElegido; }
    MenuHijosDistrib->Limpiar();
    MenuHijosDistrib->titulo = T("Distribution");
    MenuHijosDistrib->Agregar("Gap", 0);             // clasico: gap fijo + align
    MenuHijosDistrib->Agregar("Space-between", 1);   // extremos pegados
    MenuHijosDistrib->Agregar("Space-around", 2);    // media unidad en los extremos
    MenuHijosDistrib->Agregar("Space-evenly", 3);    // unidades iguales
    AbrirMenuBajoBoton(MenuHijosDistrib, PropsActivo->propHijosDistrib->button);
}

// fila COMPACTA de la paleta: NOMBRE EDITABLE a la izquierda (se clickea y se tipea,
// como el campo Name), el swatch de color, y un BOTON cuadrado con la X para borrar
// pegado al borde DERECHO. Las zonas las resuelve el click handler de Color.
class PropColorPal : public PropColor {
public:
    int idx;
    TextField field;    // el nombre se edita inline (g_textFieldActivo al clickear)
    std::string* nom;   // apunta al nombre en la paleta (estable: colores con reserve)
    PropColorPal(const std::string& nomIni, int i) : PropColor(nomIni), idx(i) {
        nom = NULL; field.SetText(nomIni);
    }
    int PaletaIdx() const W3D_OVERRIDE { return idx; }
    void RenderPropertiBox(Card* box) W3D_OVERRIDE {
        if (!value) return;
        int cb = box->width;
        float dxS = (float)(width - PropColEtiqueta - cb * 2 - gapGS - bordersGS);
        float dxX = (float)(width - PropColEtiqueta - cb - bordersGS);
        w3dEngine::Translatef(dxS, 0, 0);
        box->Render(false);                 // el swatch (el group ya seteo su color)
        w3dEngine::Translatef(dxX - dxS, 0, 0);
        SetColorID(ColorID::gris);          // el boton X: card gris como los botones...
        box->Render(false);
        SetColorID(ColorID::grisLinea);     // ...con borde propio SIEMPRE visible
        box->RenderBorder(false);
        w3dEngine::Translatef(-dxX, RenglonHeightGS + gapGS, 0);
    }
    void RenderPropertiBoxBorder(Card* box) W3D_OVERRIDE {
        if (!value) return;
        int cb = box->width;
        float dxS = (float)(width - PropColEtiqueta - cb * 2 - gapGS - bordersGS);
        float dxX = (float)(width - PropColEtiqueta - cb - bordersGS);
        w3dEngine::Translatef(dxS, -RenglonHeightGS - gapGS, 0);
        box->RenderBorder(false);
        w3dEngine::Translatef(dxX - dxS, 0, 0);
        box->RenderBorder(false);
        w3dEngine::Translatef(-dxX, RenglonHeightGS + gapGS, 0);
    }
    void RenderPropertiValue(Card* propertiBox) W3D_OVERRIDE {
        if (!value) return;
        int cb = RenglonHeightGS + GlobalScale * 2;
        bool foco = (g_textFieldActivo == &field);
        if (foco && nom) *nom = field.text;   // lo tipeado pisa el nombre EN VIVO
        // el nombre arranca en el borde IZQUIERDO de la fila (no en la col de valores)
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((float)(bordersGS - PropColEtiqueta), 0, 0);
        int wNombre = width - cb * 2 - gapGS * 2 - bordersGS * 2;
        if (foco && field.selectAll) {
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
            RenderBitmapText(field.text, textAlign::left, wNombre);
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::blanco)]);
        } else {
            std::string s = foco ? field.text.substr(0, field.caret) + "|" + field.text.substr(field.caret)
                                 : (nom ? *nom : field.text);
            RenderBitmapText(s, textAlign::left, wNombre);
        }
        w3dEngine::PopMatrix();
        // la X centrada sobre su boton
        float dxX = (float)(width - PropColEtiqueta - cb - bordersGS);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(dxX, 0, 0);
        RenderBitmapText("x", textAlign::center, cb);
        w3dEngine::PopMatrix();
        w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
    }
    void RenderPropertiLabel(Card* propertiBox) W3D_OVERRIDE {
        if (value) w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
    }
};

// ============================ VIDEO 2D ============================
static Video2D* Vid2dActivo(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::video2d) ? (Video2D*)ObjActivo : NULL;
}
static PopupMenu* MenuVidModo = NULL;
static void AccionVidModoElegido(int id){
    Video2D* v = Vid2dActivo(); if (!v) return;
    v->modo = id;
    if (PropsActivo && PropsActivo->propVidModo) PropsActivo->propVidModo->button->text = ImgNombreModo(id);
    g_redraw = true;
}
static void AccionMenuVidModo(){
    if (!PropsActivo || !Vid2dActivo()) return;
    if (!MenuVidModo){ MenuVidModo = new PopupMenu(); MenuVidModo->action = AccionVidModoElegido; }
    MenuVidModo->Limpiar();
    MenuVidModo->titulo = T("Mode");
    MenuVidModo->Agregar("Estirar", 0);
    MenuVidModo->Agregar("Ajustar", 1);
    MenuVidModo->Agregar("Cover", 2);
    AbrirMenuBajoBoton(MenuVidModo, PropsActivo->propVidModo->button);
}
static PopupMenu* MenuVidAncla = NULL;
static void AccionVidAnclaElegida(int id){
    Video2D* v = Vid2dActivo(); if (!v) return;
    v->ancla = id;
    if (PropsActivo && PropsActivo->propVidAncla) PropsActivo->propVidAncla->button->text = T2dNombreAncla(id);
    g_redraw = true;
}
static void AccionMenuVidAncla(){
    if (!PropsActivo || !Vid2dActivo()) return;
    if (!MenuVidAncla){ MenuVidAncla = new PopupMenu(); MenuVidAncla->action = AccionVidAnclaElegida; }
    MenuVidAncla->Limpiar();
    MenuVidAncla->titulo = T("Anchor");
    for (int i = 0; i <= 8; i++) MenuVidAncla->Agregar(T2dNombreAncla(i), i);
    AbrirMenuBajoBoton(MenuVidAncla, PropsActivo->propVidAncla->button);
}
static void VidArchivoElegido(const std::string& rutaElegida){
    Video2D* v = Vid2dActivo(); if (!v) return;
    // IMPORTAR = COPIAR ADENTRO: el archivo que el usuario acaba de elegir de su
    // disco pasa a ser del proyecto en el acto y se referencia como INTERNO
    // ("texturas/piso.png"). Sin contenedor montado (proyecto v3) no cambia nada.
    const std::string ruta = W3dImportarAsset(rutaElegida);
    v->video = ruta;
    const VideoPreview* pv = Video2DPreview(ruta);   // extrae la preview + tamano real
    if (pv && pv->anchoReal > 0 && v->tamModo != TAM2D_FRACCION) {
        v->ancho = (float)pv->anchoReal; v->alto = (float)pv->altoReal;
    }
    if (PropsActivo && PropsActivo->propVidArchivo)
        PropsActivo->propVidArchivo->button->text = NombreDeArchivo(ruta);
    g_redraw = true;
}
static void AccionVidArchivo(){
    if (!Vid2dActivo()) return;
    AbrirFileBrowser("Cargar video", T("Open"), ".mp4 .webm .gif .mov .avi", VidArchivoElegido);
}

// ============================ SCRIPT (estilo Unity) ============================
#include "script/W3dScript.h"
#include "ViewPorts/Notificaciones.h"   // el progreso de Compilar juego
// las propiedades expuestas por CADA script del objeto activo (una tarjeta por script)
static std::vector<std::vector<W3dScriptProp> > gScriptPropsMulti;
static int g_scriptCardSel = -1;     // la tarjeta cuyo menu esta abierto
static int g_scriptPropSel = -1;     // la propiedad elegida en esa tarjeta
static int g_scriptCambiarIdx = -1;  // el file browser cambia ESTE script (-1 = agrega)
static std::vector<std::string> gScriptObjNombres;
static PopupMenu* MenuScriptRef = NULL;

static void ScriptRefrescar(){
    if (PropsActivo) { PropsActivo->target = NULL; PropsActivo->scriptFirma = -1; }
    g_redraw = true;
}
// el file browser eligio un .lua: agregarlo o cambiar el del script elegido
static void ScriptElegido(const std::string& rutaElegida) {
    if (!ObjActivo) return;
    // IMPORTAR = COPIAR ADENTRO: el archivo que el usuario acaba de elegir de su
    // disco pasa a ser del proyecto en el acto y se referencia como INTERNO
    // ("texturas/piso.png"). Sin contenedor montado (proyecto v3) no cambia nada.
    const std::string ruta = W3dImportarAsset(rutaElegida);
    if (!ObjActivo->scriptDatos) ObjActivo->scriptDatos = new W3dScriptDatos();
    if (g_scriptCambiarIdx >= 0 && g_scriptCambiarIdx < (int)ObjActivo->scriptDatos->scripts.size()) {
        ObjActivo->scriptDatos->scripts[g_scriptCambiarIdx].ruta = ruta;
    } else {
        W3dScriptEntrada e; e.ruta = ruta;
        ObjActivo->scriptDatos->scripts.push_back(e);
    }
    g_scriptCambiarIdx = -1;
    { extern void SimScriptsCambiados(Object*); SimScriptsCambiados(ObjActivo); }
    ScriptRefrescar();
}
static void AccionScriptAgregar() {
    if (!ObjActivo) return;
    g_scriptCambiarIdx = -1;
    AbrirFileBrowser("Elegir script", T("Open"), ".lua .luac", ScriptElegido);
}
// el script SELECCIONADO en la lista de la tarjeta Control (-1 = ninguno)
static int ScriptActivoIdx() {
    if (!ObjActivo || !ObjActivo->scriptDatos) return -1;
    int n = (int)ObjActivo->scriptDatos->scripts.size();
    if (n == 0) return -1;
    int a = ObjActivo->scriptDatos->activo;
    if (a < 0) a = 0; if (a >= n) a = n - 1;
    ObjActivo->scriptDatos->activo = a;
    return a;
}
// guarda el valor elegido para una propiedad de UN script
static void ScriptAsignar(int si, const std::string& prop, const std::string& valor) {
    if (!ObjActivo || !ObjActivo->scriptDatos) return;
    if (si < 0 || si >= (int)ObjActivo->scriptDatos->scripts.size()) return;
    W3dScriptEntrada& e = ObjActivo->scriptDatos->scripts[si];
    for (size_t i = 0; i < e.refs.size(); i++)
        if (e.refs[i].first == prop) { e.refs[i].second = valor; return; }
    e.refs.push_back(std::make_pair(prop, valor));
}
static const char* ScriptValorDe(Object* o, int si, const std::string& prop) {
    if (!o || !o->scriptDatos || si < 0 || si >= (int)o->scriptDatos->scripts.size()) return "";
    W3dScriptEntrada& e = o->scriptDatos->scripts[si];
    for (size_t i = 0; i < e.refs.size(); i++)
        if (e.refs[i].first == prop) return e.refs[i].second.c_str();
    return "";
}
// ---- propiedades de VALOR (tipo 2: numero / bool / texto) -------------------
// Cada fila tiene su editor nativo (PropFloat/PropBool/PropText) apuntando a un
// buffer PROPIO de esta lista; el commit escribe el string por instancia en
// W3dScriptEntrada.refs (el MISMO canal donde viajan las refs) via ScriptAsignar.
// Las filas viven exactamente lo que las tarjetas: se reconstruyen juntas.
struct ScriptValRow {
    int card;             // que tarjeta de script (indice en scriptDatos->scripts)
    std::string prop;     // el nombre de la propiedad en el .lua
    PropFloat* pf;        // subtipo 0 (numero); NULL si no
    PropBool*  pb;        // subtipo 1 (bool)
    PropText*  pt;        // subtipo 2 (texto)
    float f; bool b;      // los buffers estables que edita el widget
    std::string ultimo;   // lo ULTIMO escrito/leido (si el widget difiere, hay que guardar)
    ScriptValRow() : card(-1), pf(NULL), pb(NULL), pt(NULL), f(0.0f), b(false) {}
};
static std::vector<ScriptValRow*> gScriptValRows;
static std::string ScriptValFmtFloat(float v) {
    char buf[48]; snprintf(buf, sizeof(buf), "%g", (double)v); return std::string(buf);
}
// guarda el valor de UNA fila si cambio (y avisa a la sim para verlo en vivo)
static void ScriptValGuardar(ScriptValRow* r, const std::string& v) {
    if (r->ultimo == v || !ObjActivo) return;
    ScriptAsignar(r->card, r->prop, v);
    r->ultimo = v;
    // el proyecto queda con cambios (el guardado los lleva) y se ve al instante
    { extern bool SimActiva(); extern void SimReresolver(Object*);
      if (SimActiva()) SimReresolver(ObjActivo); }
    g_redraw = true;
}
// pasa por TODAS las filas de valor y persiste lo que el usuario haya editado.
// Corre una vez por frame (en el rebind) y tambien como onChange de los widgets:
// cubre flechas, arrastre, numpad, checkbox y el tipeo del campo de texto.
static void ScriptValsSincronizar() {
    for (size_t i = 0; i < gScriptValRows.size(); i++) {
        ScriptValRow* r = gScriptValRows[i];
        if (r->pf)      ScriptValGuardar(r, ScriptValFmtFloat(r->f));
        else if (r->pb) ScriptValGuardar(r, r->b ? "true" : "false");
        else if (r->pt) ScriptValGuardar(r, r->pt->field.text);
    }
}
// las filas se destruyen JUNTO con las tarjetas (rebuild): soltar los buffers y,
// si el foco de texto estaba en una de estas filas, soltarlo tambien (mismo
// criterio que la tarjeta Paletas: un puntero colgado = crash al tipear).
static void ScriptValsLimpiar() {
    for (size_t i = 0; i < gScriptValRows.size(); i++) delete gScriptValRows[i];
    gScriptValRows.clear();
}
static void RecolectarNombres(Object* o, std::vector<std::string>* v) {
    if (!o) return;
    if (o != SceneCollection) v->push_back(o->name);
    for (size_t i = 0; i < o->Childrens.size(); i++) RecolectarNombres(o->Childrens[i], v);
}
static void AccionScriptRefElegida(int id) {
    if (g_scriptCardSel < 0 || g_scriptCardSel >= (int)gScriptPropsMulti.size()) return;
    std::vector<W3dScriptProp>& props = gScriptPropsMulti[g_scriptCardSel];
    if (g_scriptPropSel < 0 || g_scriptPropSel >= (int)props.size()) return;
    W3dScriptProp& p = props[g_scriptPropSel];
    std::string valor;
    if (p.tipo == 1) { if (id >= 0 && id < (int)p.opciones.size()) valor = p.opciones[id]; }
    else             { if (id >= 0 && id < (int)gScriptObjNombres.size()) valor = gScriptObjNombres[id]; }
    if (!valor.empty()) ScriptAsignar(g_scriptCardSel, p.nombre, valor);
    // con el juego ANDANDO el cambio se ve al instante (se re-resuelven las refs)
    { extern bool SimActiva(); extern void SimReresolver(Object*);
      if (SimActiva()) SimReresolver(ObjActivo); }
    ScriptRefrescar();
}
// QUITAR un script de un objeto (la ultima fila de su tarjeta). Sale de la UI para que el
// aviso al undo y el erase queden en UN solo lugar (mismo criterio que UndoBorrarClipArm).
//
// QUITARLO CORRE LOS INDICES de la lista de scripts, y los destinos de rename de las refs de
// lua van por (objeto, INDICE de script, indice de ref): sin el aviso, el Ctrl+Z de un rename
// anterior no deshacia la ref Y ADEMAS escribia el nombre viejo encima de la ref de OTRO
// script (vinculo roto en silencio, ni crash ni aviso). Quitar un script todavia no es
// deshacible -> es la familia (2) de Undo.h: hay que avisar. El .i del destino ES el indice de
// script, que es justo el que corre; el .j (la ref) no se toca, las refs solo crecen.
static void QuitarScriptDeObjeto(Object* o, int card) {
    if (!o || !o->scriptDatos) return;
    if (card < 0 || card >= (int)o->scriptDatos->scripts.size()) return;
    UndoListaBorrada(W3dDestRefLua(o, card, 0));
    o->scriptDatos->scripts.erase(o->scriptDatos->scripts.begin() + card);
    { extern void SimScriptsCambiados(Object*); SimScriptsCambiados(o); }
}
// el harness de tests aprieta el MISMO boton (mismo idiom que _BorrarVertexAnimDeFwd)
void _QuitarScriptDeObjetoFwd(Object* o, int card){ QuitarScriptDeObjeto(o, card); }

// ---- botones de la tarjeta "Control" (la lista de scripts, estilo modificadores) ----
// QUITAR el script seleccionado.
static void AccionScriptQuitar() {
    int a = ScriptActivoIdx();
    if (a < 0) return;
    QuitarScriptDeObjeto(ObjActivo, a);
    // la seleccion se queda en el lugar que ocupaba el borrado (o en el ultimo)
    int n = (int)ObjActivo->scriptDatos->scripts.size();
    ObjActivo->scriptDatos->activo = (a >= n) ? n - 1 : a;
    ScriptRefrescar();
}
// REORDENAR: el orden de la lista ES el orden de ejecucion, asi que mover uno cambia el
// comportamiento del juego. Mover CORRE LOS INDICES y los destinos de rename de las refs
// de lua van por (objeto, indice de script, indice de ref) -> hay que avisarle al undo
// (UndoListaMovida), que es lo que exige el criterio general de Undo.h. Mover scripts no
// es deshacible todavia, igual que quitarlos.
static void MoverScriptDe(Object* o, int i, int j) {
    if (!o || !o->scriptDatos) return;
    std::vector<W3dScriptEntrada>& v = o->scriptDatos->scripts;
    if (i < 0 || j < 0 || i >= (int)v.size() || j >= (int)v.size() || i == j) return;
    W3dScriptEntrada t = v[i]; v[i] = v[j]; v[j] = t;
    o->scriptDatos->activo = j;                    // el activo VIAJA con el script que se movio
    UndoListaMovida(W3dDestRefLua(o, -1, 0), i, j);
    { extern void SimScriptsCambiados(Object*); SimScriptsCambiados(o); }
    ScriptRefrescar();
}
// el harness de tests aprieta los MISMOS botones (mismo idiom que _QuitarScriptDeObjetoFwd)
void _MoverScriptDeObjetoFwd(Object* o, int i, int j){ MoverScriptDe(o, i, j); }
static void MoverScript(int i, int j) { MoverScriptDe(ObjActivo, i, j); }
static void AccionScriptUp()   { int a = ScriptActivoIdx(); if (a > 0) MoverScript(a, a - 1); }
static void AccionScriptDown() {
    int a = ScriptActivoIdx();
    if (a >= 0 && ObjActivo && ObjActivo->scriptDatos &&
        a + 1 < (int)ObjActivo->scriptDatos->scripts.size()) MoverScript(a, a + 1);
}
// el click en cualquier fila de una tarjeta de script: se localiza QUE tarjeta fue
// (el handler del panel deja su selectIndex >= 0) y que fila
static void AccionScriptCardFila() {
    if (!PropsActivo) return;
    int card = -1, fila = -1;
    for (int i = 0; i < Properties::kMaxScriptCards; i++) {
        GroupPropertie* g = PropsActivo->propScriptCards[i];
        if (g && g->visible && g->selectIndex >= 0) { card = i; fila = g->selectIndex; break; }
    }
    if (card < 0 || !ObjActivo || !ObjActivo->scriptDatos) return;
    if (card >= (int)ObjActivo->scriptDatos->scripts.size()) return;
    GroupPropertie* g = PropsActivo->propScriptCards[card];
    int ultima = (int)g->properties.size() - 1;
    if (fila == 0) {                 // [0] el archivo: elegir otro .lua
        g_scriptCambiarIdx = card;
        AbrirFileBrowser("Elegir script", T("Open"), ".lua .luac", ScriptElegido);
        return;
    }
    if (fila == ultima) {            // ultima fila: QUITAR el script
        QuitarScriptDeObjeto(ObjActivo, card);
        ScriptRefrescar();
        return;
    }
    // filas del medio: una PROPIEDAD expuesta (desplegable de objetos u opciones)
    int pi = fila - 1;
    g_scriptCardSel = card; g_scriptPropSel = pi;
    if (card >= (int)gScriptPropsMulti.size() || pi < 0 || pi >= (int)gScriptPropsMulti[card].size()) return;
    W3dScriptProp& p = gScriptPropsMulti[card][pi];
    if (!MenuScriptRef) { MenuScriptRef = new PopupMenu(); MenuScriptRef->action = AccionScriptRefElegida; }
    MenuScriptRef->Limpiar();
    MenuScriptRef->titulo = p.nombre;
    if (p.tipo == 1) {
        for (size_t i = 0; i < p.opciones.size(); i++)
            MenuScriptRef->Agregar(p.opciones[i], (int)i);
    } else {
        gScriptObjNombres.clear();
        RecolectarNombres(SceneCollection, &gScriptObjNombres);
        for (size_t i = 0; i < gScriptObjNombres.size(); i++)
            MenuScriptRef->Agregar(gScriptObjNombres[i], (int)i);
    }
    PropButton* pb = (PropButton*)g->properties[fila];
    AbrirMenuBajoBoton(MenuScriptRef, pb->button);
}

// ============================ PALETA DE COLORES ============================
// Las paletas viven a NIVEL PROYECTO (W3dPaletas.h): cualquier objeto (3D o
// 2D) elige una POR NOMBRE con herencia (tarjeta "Paleta" del objeto; "" =
// igual que el padre). La GESTION completa (crear/renombrar/borrar paletas y
// colores, con los invariantes de mismo-largo y correccion de referencias)
// vive en la tarjeta "Paletas" de la pestania 0 (la del proyecto).
static Imagen2D* Img2dActiva();      // (definidos mas abajo; las acciones de paleta los usan)
static Rect2D* Rect2dActivo();
static Slice9* S9Activo();
static Boton2D* Btn2dActivo();
static UI* UIActivaProps();
// nombre a mostrar para un indice pal* del objeto (contra su paleta EFECTIVA)
static const char* PalNombre(Object* o, int idx){
    if (!o || idx < 0) return "Propio";
    std::vector<PaletaColor>* cs = W3dColoresEfectivos(o);
    if (!cs || idx >= (int)cs->size()) return "Propio";
    return (*cs)[idx].nombre.c_str();
}
// que indice esta editando el menu de paleta abierto (apunta al campo int del elemento)
static int* g_palTarget = NULL;
static PropButton* g_palBoton = NULL;
static PopupMenu* MenuPal = NULL;
static void AccionPalElegida(int id){
    if (!g_palTarget) return;
    *g_palTarget = id;   // -1 = color propio
    if (g_palBoton) g_palBoton->button->text = PalNombre(ObjActivo, id);
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: el swatch aparece/desaparece
    g_redraw = true;
}
static void AbrirMenuPal(int* target, PropButton* boton){
    if (!boton) return;
    std::vector<PaletaColor>* cs = W3dColoresEfectivos(ObjActivo);
    g_palTarget = target; g_palBoton = boton;
    if (!MenuPal){ MenuPal = new PopupMenu(); MenuPal->action = AccionPalElegida; }
    MenuPal->Limpiar();
    MenuPal->titulo = T("Palette");
    MenuPal->Agregar("Propio", -1);
    if (cs)
        for (size_t i = 0; i < cs->size(); i++)
            MenuPal->Agregar((*cs)[i].nombre, (int)i);
    AbrirMenuBajoBoton(MenuPal, boton->button);
}
static void AccionPalT2d(){  Texto2D* t = T2dActivo();   if (t && PropsActivo) AbrirMenuPal(&t->palColor, PropsActivo->propT2dPal); }
static void AccionPalImg(){  Imagen2D* i = Img2dActiva();if (i && PropsActivo) AbrirMenuPal(&i->palTinte, PropsActivo->propImgPal); }
static void AccionPalRect(){ Rect2D* r = Rect2dActivo(); if (r && PropsActivo) AbrirMenuPal(&r->palColor, PropsActivo->propRectPal); }
static void AccionPalS9(){   Slice9* s = S9Activo();     if (s && PropsActivo) AbrirMenuPal(&s->palTinte, PropsActivo->propS9Pal); }
static void AccionPalBtnFondo(){ Boton2D* b = Btn2dActivo(); if (b && PropsActivo) AbrirMenuPal(&b->palFondo, PropsActivo->propBtnPalFondo); }
static void AccionPalBtnTexto(){ Boton2D* b = Btn2dActivo(); if (b && PropsActivo) AbrirMenuPal(&b->palTexto, PropsActivo->propBtnPalTexto); }
static void AccionPalBtnBorde(){ Boton2D* b = Btn2dActivo(); if (b && PropsActivo) AbrirMenuPal(&b->palBorde, PropsActivo->propBtnPalBorde); }
static void AccionPalBtnHover(){ Boton2D* b = Btn2dActivo(); if (b && PropsActivo) AbrirMenuPal(&b->palHover, PropsActivo->propBtnPalHover); }
// cual paleta del proyecto muestra/edita la tarjeta "Paletas" (pestania 0)
static int gPalEdit = 0;
// agregar un color: INVARIANTE 1 (va a TODAS las paletas, mismo nombre/valor)
static void AccionPaletaAgregar(){
    if (W3dPaletas().empty()) return;   // primero crear una paleta (desplegable)
    // el nombre unico lo pone W3dPaletaAgregarColor (antes "Color N" con cantidad+1 y SIN
    // chequear: borrar uno del medio y agregar otro repetia el nombre)
    float blanco[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    W3dPaletaAgregarColor("Color", blanco);
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: la tarjeta se reconstruye
    g_redraw = true;
}
// los nombres de los colores se editan EN VIVO (las filas PropColorPal de la
// tarjeta); el de la PALETA en edicion, en su PropText (SincronizarNombrePaleta)
// desplegable de PALETAS de la tarjeta del proyecto: elegir cual se EDITA,
// crear una nueva (copia de la editada) o borrar la editada
static PopupMenu* MenuPaletas = NULL;
static void AccionPaletasElegida(int id){
    int n = (int)W3dPaletas().size();
    if (id == n) {                       // "Nueva paleta": copia de la editada
        // el nombre unico lo pone W3dPaletaNueva (LA regla comun): antes habia OTRO
        // bucle propio aca ("Paleta N", con tope 99)
        int idx = W3dPaletaNueva("Paleta", gPalEdit);
        if (idx >= 0) gPalEdit = idx;
    } else if (id == n + 1) {            // "Borrar paleta": la editada; sus
        W3dPaletaBorrarPaleta(gPalEdit); // seleccionados vuelven a heredar
        if (gPalEdit >= (int)W3dPaletas().size()) gPalEdit = (int)W3dPaletas().size() - 1;
        if (gPalEdit < 0) gPalEdit = 0;
    } else gPalEdit = id;
    if (PropsActivo) PropsActivo->target = NULL;
    g_redraw = true;
}
static void AccionMenuPaletas(){
    if (!PropsActivo || !PropsActivo->propPaletaSel) return;
    if (!MenuPaletas){ MenuPaletas = new PopupMenu(); MenuPaletas->action = AccionPaletasElegida; }
    MenuPaletas->Limpiar();
    MenuPaletas->titulo = T("Palette");
    std::vector<Paleta>& ps = W3dPaletas();
    for (size_t i = 0; i < ps.size(); i++)
        MenuPaletas->Agregar(ps[i].nombre, (int)i);
    MenuPaletas->Agregar(T("New Palette"), (int)ps.size());
    if (!ps.empty()) MenuPaletas->Agregar("Borrar paleta", (int)ps.size() + 1);
    AbrirMenuBajoBoton(MenuPaletas, PropsActivo->propPaletaSel->button);
}
// el desplegable "Paleta" del OBJETO: "Igual que el padre" (default, hereda)
// o una paleta del proyecto. Cambiarla re-pinta al objeto Y sus herederos
// (palette swap en vivo).
static PopupMenu* MenuPaletaObj = NULL;
static void AccionPaletaObjElegida(int id){
    if (!ObjActivo) return;
    std::vector<Paleta>& ps = W3dPaletas();
    if (id < 0 || id >= (int)ps.size()) ObjActivo->paleta.clear();
    else ObjActivo->paleta = ps[id].nombre;
    if (PropsActivo) PropsActivo->target = NULL;
    g_redraw = true;
}
static void AccionMenuPaletaObj(){
    if (!PropsActivo || !PropsActivo->propPaletaObjSel || !ObjActivo) return;
    if (!MenuPaletaObj){ MenuPaletaObj = new PopupMenu(); MenuPaletaObj->action = AccionPaletaObjElegida; }
    MenuPaletaObj->Limpiar();
    MenuPaletaObj->titulo = T("Palette");
    MenuPaletaObj->Agregar("Igual que el padre", -1);
    std::vector<Paleta>& ps = W3dPaletas();
    for (size_t i = 0; i < ps.size(); i++)
        MenuPaletaObj->Agregar(ps[i].nombre, (int)i);
    AbrirMenuBajoBoton(MenuPaletaObj, PropsActivo->propPaletaObjSel->button);
}
// aviso del ColorPicker (pestania Pal): cambio una referencia/seleccion de
// paleta -> re-bind del panel (swatches) + redibujar el lienzo
static void AccionPickerPalCambio(){
    if (PropsActivo) PropsActivo->target = NULL;
    g_redraw = true;
}
// el NOMBRE de la paleta en edicion (PropText de la tarjeta del proyecto): se
// renombra EN VIVO propagando a las selecciones por nombre de todo el
// proyecto (W3dPaletaRenombrar). Un nombre ya tomado no pisa (se aplica
// cuando el tipeo lo vuelve unico); al desenfocar se muestra el real.
static void SincronizarNombrePaleta(Properties* p){
    if (!p || !p->propPaletaNombre) return;
    std::vector<Paleta>& ps = W3dPaletas();
    if (gPalEdit < 0 || gPalEdit >= (int)ps.size()) return;
    PropText* pt = p->propPaletaNombre;
    bool foco = (g_textFieldActivo == &pt->field);
    if (foco && !pt->field.text.empty() && pt->field.text != ps[gPalEdit].nombre){
        W3dPaletaRenombrar(gPalEdit, pt->field.text);
        if (p->propPaletaSel) p->propPaletaSel->button->text = ps[gPalEdit].nombre;
        g_redraw = true;
    }
    if (!foco && pt->field.text != ps[gPalEdit].nombre){
        pt->field.SetText(ps[gPalEdit].nombre);
        g_redraw = true;
    }
}

// ============================ BOTON 2D ============================
static Boton2D* Btn2dActivo(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::boton2d) ? (Boton2D*)ObjActivo : NULL;
}
static PopupMenu* MenuBtnAncla = NULL;
static void AccionBtnAnclaElegida(int id){
    Boton2D* b = Btn2dActivo(); if (!b) return;
    b->ancla = id;
    if (PropsActivo && PropsActivo->propBtnAncla) PropsActivo->propBtnAncla->button->text = T2dNombreAncla(id);
    g_redraw = true;
}
static void AccionMenuBtnAncla(){
    if (!PropsActivo || !Btn2dActivo()) return;
    if (!MenuBtnAncla){ MenuBtnAncla = new PopupMenu(); MenuBtnAncla->action = AccionBtnAnclaElegida; }
    MenuBtnAncla->Limpiar();
    MenuBtnAncla->titulo = T("Anchor");
    for (int i = 0; i <= 8; i++) MenuBtnAncla->Agregar(T2dNombreAncla(i), i);
    AbrirMenuBajoBoton(MenuBtnAncla, PropsActivo->propBtnAncla->button);
}
static void BtnTexElegida(const std::string& rutaElegida){
    Boton2D* b = Btn2dActivo(); if (!b) return;
    // IMPORTAR = COPIAR ADENTRO: el archivo que el usuario acaba de elegir de su
    // disco pasa a ser del proyecto en el acto y se referencia como INTERNO
    // ("texturas/piso.png"). Sin contenedor montado (proyecto v3) no cambia nada.
    const std::string ruta = W3dImportarAsset(rutaElegida);
    b->texturaFondo = ruta;
    if (PropsActivo && PropsActivo->propBtnTex)
        PropsActivo->propBtnTex->button->text = ruta.empty() ? std::string(T("Choose..."))
                                                             : NombreDeArchivo(ruta);
    if (PropsActivo) PropsActivo->target = NULL;
    g_redraw = true;
}
static void AccionBtnTex(){
    if (!Btn2dActivo()) return;
    AbrirFileBrowser(T("Load image"), T("Open"), ".png .jpg .jpeg .bmp .tga .gif", BtnTexElegida);
}
static void BtnIconoElegido(const std::string& rutaElegida){
    Boton2D* b = Btn2dActivo(); if (!b) return;
    // IMPORTAR = COPIAR ADENTRO: el archivo que el usuario acaba de elegir de su
    // disco pasa a ser del proyecto en el acto y se referencia como INTERNO
    // ("texturas/piso.png"). Sin contenedor montado (proyecto v3) no cambia nada.
    const std::string ruta = W3dImportarAsset(rutaElegida);
    b->icono = ruta;
    if (PropsActivo && PropsActivo->propBtnIcono)
        PropsActivo->propBtnIcono->button->text = ruta.empty() ? std::string(T("Choose..."))
                                                               : NombreDeArchivo(ruta);
    g_redraw = true;
}
static void AccionBtnIcono(){
    if (!Btn2dActivo()) return;
    AbrirFileBrowser(T("Load image"), T("Open"), ".png .jpg .jpeg .bmp .tga .gif", BtnIconoElegido);
}

static PopupMenu* MenuHijosLayout = NULL;
static void AccionHijosLayoutElegido(int id){
    int* l = HijosLayoutDe(ObjActivo); if (!l) return;
    *l = id;
    if (PropsActivo && PropsActivo->propHijosLayout)
        PropsActivo->propHijosLayout->button->text = HijosNombreLayout(id);
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: Gap aparece/desaparece
    g_redraw = true;
}
static void AccionMenuHijosLayout(){
    if (!PropsActivo || !HijosLayoutDe(ObjActivo)) return;
    if (!MenuHijosLayout){ MenuHijosLayout = new PopupMenu(); MenuHijosLayout->action = AccionHijosLayoutElegido; }
    MenuHijosLayout->Limpiar();
    MenuHijosLayout->titulo = T("Layout");
    MenuHijosLayout->Agregar("Libremente", 0);   // cada hijo con su ancla y su posicion
    MenuHijosLayout->Agregar("Filas", 1);        // se reparten el alto (100% del area)
    MenuHijosLayout->Agregar("Columnas", 2);     // se reparten el ancho
    AbrirMenuBajoBoton(MenuHijosLayout, PropsActivo->propHijosLayout->button);
}

static Contenedor2D* Cont2dActivo(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::cont2d) ? (Contenedor2D*)ObjActivo : NULL;
}
static PopupMenu* MenuContAncla = NULL;
static void AccionContAnclaElegida(int id){
    Contenedor2D* c = Cont2dActivo(); if (!c) return;
    c->ancla = id;
    if (PropsActivo && PropsActivo->propContAncla) PropsActivo->propContAncla->button->text = T2dNombreAncla(id);
    g_redraw = true;
}
static void AccionMenuContAncla(){
    if (!PropsActivo || !Cont2dActivo()) return;
    if (!MenuContAncla){ MenuContAncla = new PopupMenu(); MenuContAncla->action = AccionContAnclaElegida; }
    MenuContAncla->Limpiar();
    MenuContAncla->titulo = T("Anchor");
    for (int i = 0; i <= 8; i++) MenuContAncla->Agregar(T2dNombreAncla(i), i);
    AbrirMenuBajoBoton(MenuContAncla, PropsActivo->propContAncla->button);
}

// ============================ SLICE 9 ============================
static Slice9* S9Activo(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::slice9) ? (Slice9*)ObjActivo : NULL;
}
static PopupMenu* MenuS9Ancla = NULL;
static void AccionS9AnclaElegida(int id){
    Slice9* s9 = S9Activo(); if (!s9) return;
    s9->ancla = id;
    if (PropsActivo && PropsActivo->propS9Ancla) PropsActivo->propS9Ancla->button->text = T2dNombreAncla(id);
    g_redraw = true;
}
static void AccionMenuS9Ancla(){
    if (!PropsActivo || !S9Activo()) return;
    if (!MenuS9Ancla){ MenuS9Ancla = new PopupMenu(); MenuS9Ancla->action = AccionS9AnclaElegida; }
    MenuS9Ancla->Limpiar();
    MenuS9Ancla->titulo = T("Anchor");
    for (int i = 0; i <= 8; i++) MenuS9Ancla->Agregar(T2dNombreAncla(i), i);
    AbrirMenuBajoBoton(MenuS9Ancla, PropsActivo->propS9Ancla->button);
}
static void S9TexturaElegida(const std::string& rutaElegida){
    Slice9* s9 = S9Activo(); if (!s9) return;
    // IMPORTAR = COPIAR ADENTRO: el archivo que el usuario acaba de elegir de su
    // disco pasa a ser del proyecto en el acto y se referencia como INTERNO
    // ("texturas/piso.png"). Sin contenedor montado (proyecto v3) no cambia nada.
    const std::string ruta = W3dImportarAsset(rutaElegida);
    s9->textura = ruta;
    int w = 0, h = 0;
    if (Textura2DObtener(ruta, &w, &h) && w > 0 && h > 0 && s9->tamModo != TAM2D_FRACCION) {
        s9->ancho = (float)w; s9->alto = (float)h;
    }
    if (PropsActivo && PropsActivo->propS9Textura)
        PropsActivo->propS9Textura->button->text = NombreDeArchivo(ruta);
    g_redraw = true;
}
static void AccionS9Textura(){
    if (!S9Activo()) return;
    AbrirFileBrowser(T("Load image"), T("Open"), ".png .jpg .jpeg .bmp .tga .gif", S9TexturaElegida);
}

static PopupMenu* MenuRectAncla = NULL;
static void AccionRectAnclaElegida(int id){
    Rect2D* r = Rect2dActivo(); if (!r) return;
    r->ancla = id;   // igual que el resto: el ancla NO toca X/Y/Z
    if (PropsActivo && PropsActivo->propRectAncla) PropsActivo->propRectAncla->button->text = T2dNombreAncla(id);
    g_redraw = true;
}
static void AccionMenuRectAncla(){
    if (!PropsActivo || !Rect2dActivo()) return;
    if (!MenuRectAncla){ MenuRectAncla = new PopupMenu(); MenuRectAncla->action = AccionRectAnclaElegida; }
    MenuRectAncla->Limpiar();
    MenuRectAncla->titulo = T("Anchor");
    for (int i = 0; i <= 8; i++) MenuRectAncla->Agregar(T2dNombreAncla(i), i);
    AbrirMenuBajoBoton(MenuRectAncla, PropsActivo->propRectAncla->button);
}

// ====================================================================
static PopupMenu* MenuRotMode = NULL;

static void AccionRotModeElegido(int id){
    if (!ObjActivo) return;
    ObjActivo->rotMode = id;            // 0=XYZ Euler, 1=Quaternion, 2=Axis Angle
    ObjActivo->ActualizarDisplayRot();  // pasa el display al nuevo modo
    if (PropsActivo) PropsActivo->target = NULL; // fuerza el re-bind (RefreshTarget)
    PropertiesLayoutDirty = true;       // aparece/desaparece el campo W
}

// click en el selector: abre el desplegable con los 3 modos
static void AccionMenuRotMode(){
    if (!PropsActivo || !ObjActivo) return;
    if (!MenuRotMode){
        MenuRotMode = new PopupMenu();
        MenuRotMode->action = AccionRotModeElegido;
    }
    MenuRotMode->Limpiar();
    MenuRotMode->Agregar(T("XYZ Euler"), RotEulerXYZ);
    MenuRotMode->Agregar(T("Quaternion (WXYZ)"), RotQuaternion);
    MenuRotMode->Agregar(T("Axis Angle"), RotAxisAngle);
    AbrirMenuBajoBoton(MenuRotMode, PropsActivo->propRotMode->button);
}

// ====================================================================
// selector de TARGET (objeto linkeado) para camara e instance/array/mirror
// (ambos tipos heredan de Target). Un desplegable con los objetos de la escena.
// ====================================================================
static PopupMenu* MenuTarget = NULL;
static std::vector<Object*> gTargetCandidatos; // id - 1 -> objeto

// devuelve la parte Target* de ObjActivo si es camara o instance (sino NULL)
static Target* ObjComoTarget(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::camera)   return static_cast<Camera*>(o);
    if (o->getType() == ObjectType::instance) return static_cast<Instance*>(o);
    return NULL;
}

// junta los objetos de la escena que pueden ser target (no el activo, no las
// colecciones, no a si mismo para evitar recursion)
static void RecolectarTargets(Object* nodo){
    if (!nodo) return;
    for (size_t i = 0; i < nodo->Childrens.size(); i++){
        Object* c = nodo->Childrens[i];
        if (c != ObjActivo && c->getType() != ObjectType::collection)
            gTargetCandidatos.push_back(c);
        RecolectarTargets(c);
    }
}

static void AccionTargetElegido(int id){
    Target* tgt = ObjComoTarget(ObjActivo);
    if (!tgt) return;
    if (id == 0){ tgt->target = NULL; tgt->targetName = ""; return; } // None
    int idx = id - 1;
    if (idx >= 0 && idx < (int)gTargetCandidatos.size()){
        Object* o = gTargetCandidatos[idx];
        tgt->target = o;
        tgt->targetName = o->name;
    }
}

static void AccionMenuTarget(){
    if (!PropsActivo) return;
    if (!ObjComoTarget(ObjActivo)) return;
    if (!MenuTarget){ MenuTarget = new PopupMenu(); MenuTarget->action = AccionTargetElegido; }
    MenuTarget->Limpiar();
    MenuTarget->Agregar(T("None"), 0);
    gTargetCandidatos.clear();
    RecolectarTargets(SceneCollection);
    for (size_t i = 0; i < gTargetCandidatos.size(); i++)
        MenuTarget->Agregar(gTargetCandidatos[i]->name, 1 + (int)i,
                            (int)IconoDeObjeto(gTargetCandidatos[i]));
    bool esCam = ObjActivo->getType() == ObjectType::camera;
    Button* b = (esCam ? PropsActivo->propBtnCamTarget
                       : PropsActivo->propBtnInstTarget)->button;
    MenuTarget->Abrir(b->sx, b->sy + b->height - GlobalScale,
                      MenuPantallaW, MenuPantallaH);
    MenuAbierto = MenuTarget;
}

// ===== props del modificador MIRROR (tarjeta de abajo): helper + acciones (param change / target / apply) =====
static Modifier* ModActivoUI(){
    if (!ObjActivo || ObjActivo->getType()!=ObjectType::mesh) return NULL;
    Mesh* m=(Mesh*)ObjActivo;
    if (m->modificadorActivo<0 || m->modificadorActivo>=(int)m->modificadores.size()) return NULL;
    return m->modificadores[m->modificadorActivo];
}
// un param del modificador cambio (checkbox/float/target) -> REGENERAR la malla generada + redibujar
static void AccionModParamChanged(){
    if (ObjActivo && ObjActivo->getType()==ObjectType::mesh) ((Mesh*)ObjActivo)->GenerarMallaModificada();
    g_redraw = true;
}
// EDIT MODE: al editar un campo X/Y/Z del panel de Vertices, traslada RIGIDO la seleccion para que su centro caiga
// en el valor tipeado. Convencion Z-up del panel: campo X->local x, campo Y->local z, campo Z->local y (igual que
// el transform de objeto). Permite dejar un vert EXACTO (ej. X e Y en 0 -> sobre el eje del Screw).
static void AccionEditPos(){
    if (InteractionMode != EditMode || !g_editMesh || !PropsActivo) return;
    Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit();
    if (!m->edit) return;
    float cx, cy, cz; if (!m->edit->CentroSeleccion(cx, cy, cz)) return; // centro LOCAL actual
    Vector3 delta(PropsActivo->editPosX - cx, PropsActivo->editPosZ - cy, PropsActivo->editPosY - cz);
    MoverSeleccionEditLocal(m, delta); // no-op si delta=0
    g_redraw = true;
}
// centro UV de la SELECCION EFECTIVA (tarjeta "Transform UV" de la pestania Transformar):
// promedio de las posiciones UV UNICAS de los verts seleccionados (misma regla que el pivote
// mediana del editor UV: los splits en el mismo lugar cuentan 1 sola vez). El conjunto sale de
// UVVertsSelEfectivos (UVEditor.cpp): si hay un editor UV con seleccion PROPIA (sync OFF) manda
// esa -- lo que se ve marcado en el UV es lo que la tarjeta muestra y mueve; si no, la del 3D.
// false si la malla no tiene uv o no hay nada seleccionado.
bool UVVertsSelEfectivos(Mesh* m, std::vector<char>& sv); // (decl. en UVEditor.h)
static bool CentroUVSeleccionEdit(Mesh* m, float& cu, float& cv){
    if (!m || !m->uv || m->vertexSize <= 0) return false;
    std::vector<char> sv;
    if (!UVVertsSelEfectivos(m, sv)) return false;
    std::set< std::pair<float,float> > unicos;
    double su = 0, svv = 0;
    for (int i = 0; i < m->vertexSize; i++) if (sv[i]) {
        std::pair<float,float> p(m->uv[i*2], m->uv[i*2+1]);
        if (unicos.insert(p).second) { su += p.first; svv += p.second; }
    }
    if (unicos.empty()) return false;
    cu = (float)(su / (double)unicos.size());
    cv = (float)(svv / (double)unicos.size());
    return true;
}
// pestania TRANSFORMAR: al editar X/Y de la tarjeta "Transform UV", traslada RIGIDO los UVs de la
// seleccion de edit mode para que su centro caiga en el valor tipeado. Reusa el NUCLEO compartido
// UVMoverSeleccionEdit (UVEditor.cpp): mismo tratamiento que el G de UVs, con su undo LIVIANO
// (UndoUV*, un paso por edicion de campo -> Ctrl+Z lo deshace).
bool UVMoverSeleccionEdit(Mesh* m, float dU, float dV); // nucleo compartido (decl. en UVEditor.h)
static void AccionEditUV(){
    if (InteractionMode != EditMode || !g_editMesh || !PropsActivo) return;
    Mesh* m = (Mesh*)g_editMesh;
    float cu, cv; if (!CentroUVSeleccionEdit(m, cu, cv)) return;
    UVMoverSeleccionEdit(m, PropsActivo->uvPosU - cu, PropsActivo->uvPosV - cv); // no-op si delta=0
    g_redraw = true;
}
// menu "Mirror Object": elegir CUALQUIER objeto de la escena como target del mirror (reusa RecolectarTargets)
static PopupMenu* MenuModTarget = NULL;
static void AccionModTargetElegido(int id){
    Modifier* mod = ModActivoUI(); if (!mod) return;
    int idx = id - 1; // 0 = None
    mod->target = (idx>=0 && idx<(int)gTargetCandidatos.size()) ? gTargetCandidatos[idx] : NULL;
    AccionModParamChanged();
}
static void AccionMenuModTarget(){
    if (!PropsActivo || !PropsActivo->propMirTarget) return;
    if (!MenuModTarget){ MenuModTarget=new PopupMenu(); MenuModTarget->action=AccionModTargetElegido; }
    MenuModTarget->Limpiar();
    MenuModTarget->Agregar(T("None"), 0);
    gTargetCandidatos.clear(); RecolectarTargets(SceneCollection);
    for (size_t i=0;i<gTargetCandidatos.size();i++)
        MenuModTarget->Agregar(gTargetCandidatos[i]->name, 1+(int)i, (int)IconoDeObjeto(gTargetCandidatos[i]));
    AbrirMenuBajoBoton(MenuModTarget, PropsActivo->propMirTarget->button);
}
// ===== target del modificador ARMATURE: SOLO esqueletos. Al elegirlo, la malla se skinnea a ese rig =====
static std::vector<Object*> gArmTargets;
static void RecolectarArmatures(Object* nodo){
    if (!nodo) return;
    for (size_t i = 0; i < nodo->Childrens.size(); i++){ Object* c = nodo->Childrens[i];
        if (c->getType() == ObjectType::armature) gArmTargets.push_back(c);
        RecolectarArmatures(c); }
}
// sincroniza mesh->skinArmature con el target del modificador Armature del stack (o NULL si no hay)
static void ActualizarSkinArmature(Mesh* m){
    if (!m) return;
    Object* arm = NULL;
    bool enEdit = ((Object*)m == g_editMesh); // en Edit Mode se respeta "Display in Edit Mode"
    for (size_t i = 0; i < m->modificadores.size(); i++){
        Modifier* md = m->modificadores[i];
        if (md->tipo != ModifierType::Armature) continue;
        // el modificador MANDA sobre el skinning: target=none, "Display in viewport" OFF, o (en Edit) "Display in
        // Edit Mode" OFF -> NO se deforma (la malla se ve en bind, igual que con target=none).
        if (!md->target || !md->mostrarViewport || (enEdit && !md->mostrarEdit)) arm = NULL;
        else arm = md->target;
        // CACHE de vertex-animation: sincronizar on/off + skip desde el modificador. Si el skip cambia, la firma del
        // cache cambia (SkinCacheFirma) y se re-dimensiona solo en el proximo SkinearMesh. Apagar libera la memoria.
        m->skinCacheOn = md->cacheAnim;
        int nuevoSkip = (int)(md->cacheSkip + 0.5f); if (nuevoSkip < 0) nuevoSkip = 0;
        m->skinCacheSkip = nuevoSkip;
        break;
    }
    if ((Object*)m->skinArmature != arm){
        m->skinArmature = (Armature*)arm; m->lastSkinFrame = -999999; g_redraw = true;
        // RIG AUTORADO (Fase 3): al ligar la malla a un armature creado en el editor, preparar el skin
        // (rest + matrices desde head/tail). Sin esto SkinearMesh descartaba los huesos (hasSkin=false)
        // y un rig autorado no deformaba. En rigs importados es no-op.
        if (arm) PrepararSkinAutorado((Armature*)arm);
    }
}
// wrapper publico: lo llama el update por-frame (ActualizarEditMeshActivo) para que "Display in viewport/Edit"
// y el cambio de modo (entrar/salir de Edit) actualicen el skinning aunque el panel de Propiedades no este abierto.
void SincronizarSkinConModificador(Mesh* m){ ActualizarSkinArmature(m); }
static PopupMenu* MenuArmTarget = NULL;
static void AccionArmTargetElegido(int id){
    Modifier* mod = ModActivoUI(); if (!mod) return;
    int idx = id - 1;
    mod->target = (idx >= 0 && idx < (int)gArmTargets.size()) ? gArmTargets[idx] : NULL;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ActualizarSkinArmature((Mesh*)ObjActivo);
    g_redraw = true;
}
static void AccionMenuArmTarget(){
    if (!PropsActivo || !PropsActivo->propArmTarget) return;
    if (!MenuArmTarget){ MenuArmTarget = new PopupMenu(); MenuArmTarget->action = AccionArmTargetElegido; }
    MenuArmTarget->Limpiar();
    MenuArmTarget->Agregar(T("None"), 0);
    gArmTargets.clear(); RecolectarArmatures(SceneCollection);
    for (size_t i = 0; i < gArmTargets.size(); i++)
        MenuArmTarget->Agregar(gArmTargets[i]->name, 1 + (int)i, (int)IconType::armature);
    AbrirMenuBajoBoton(MenuArmTarget, PropsActivo->propArmTarget->button);
}
// menu "Axis" del Screw: dropdown X/Y/Z (como el modo de rotacion; nada de pestaña rara)
static PopupMenu* MenuScrewAxis = NULL;
static void AccionScrewAxisElegido(int id){
    Modifier* mod = ModActivoUI(); if (!mod) return;
    mod->screwAxis = id; // 0=X, 1=Y, 2=Z
    AccionModParamChanged();
}
static void AccionMenuScrewAxis(){
    if (!PropsActivo || !PropsActivo->propScrewAxis) return;
    if (!MenuScrewAxis){ MenuScrewAxis = new PopupMenu(); MenuScrewAxis->action = AccionScrewAxisElegido; }
    MenuScrewAxis->Limpiar();
    MenuScrewAxis->Agregar("X", 0); MenuScrewAxis->Agregar("Y", 1); MenuScrewAxis->Agregar("Z", 2);
    AbrirMenuBajoBoton(MenuScrewAxis, PropsActivo->propScrewAxis->button);
}
// --- modificador CULLING (PVS por triangulo): metodo (desplegable) + boton Recalcular ---
// "Triangulos (PVS)" es el implementado; "BSP" se LISTA pero deshabilitado (gris) hasta
// que exista. Recalcular re-lee el sidecar <modelo>.pvs.json del .obj de origen.
static PopupMenu* MenuPvsMetodo = NULL;
static void AccionPvsMetodoElegido(int id){
    Modifier* mod = ModActivoUI(); if (!mod) return;
    if (id == 1) return;              // BSP: pendiente (el item esta gris; esto es el cinturon)
    mod->metodoPVS = id;
    AccionModParamChanged();          // re-sincroniza el override (GenerarMallaModificada -> W3dPVSSincronizar)
}
static void AccionMenuPvsMetodo(){
    if (!PropsActivo || !PropsActivo->propPvsMetodo) return;
    if (!MenuPvsMetodo){ MenuPvsMetodo = new PopupMenu(); MenuPvsMetodo->action = AccionPvsMetodoElegido; }
    MenuPvsMetodo->Limpiar();
    MenuPvsMetodo->Agregar(T("Triangulos (PVS)"), 0);
    { static bool gBspOff = false;    // *gris == false -> item DESHABILITADO (gris, no clickeable)
      MenuItem* it = MenuPvsMetodo->Agregar("BSP (pendiente)", 1);
      if (it) it->gris = &gBspOff; }
    AbrirMenuBajoBoton(MenuPvsMetodo, PropsActivo->propPvsMetodo->button);
}
static void AccionPvsRecalcular(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    extern int W3dPVSRecalcular(Mesh*);   // main/edit/MeshEdit.cpp
    int n = W3dPVSRecalcular((Mesh*)ObjActivo);
    g_redraw = true;
    if (n > 0) Notificar(T("PVS recalculado"), false);
    else       Notificar(T("PVS: falta <modelo>.pvs.json junto al .obj de origen"), true);
}

// "Apply Modifier": hornea la malla generada en la editable + saca el modificador del stack
static void AccionAplicarModificador(){
    if (!ObjActivo || ObjActivo->getType()!=ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->AplicarModificadorActivo();
    PropertiesLayoutDirty = true; g_redraw = true;
    Notificar(T("Modifier applied"), false);
}

// "Optimize Vertex Groups" (1 hueso por vertice): DESTRUCTIVO -> confirmar antes. ConfirmarPopup::onSi no lleva
// argumentos -> se guarda la malla objetivo en un estatico (mismo patron que el export).
static Mesh* g_pendingOptVGMesh = NULL;
static void HacerOptimizarVG(){
    if (!g_pendingOptVGMesh) return;
    extern void OptimizarVertexGroups1Hueso(Mesh*); // main/edit/MeshEdit.cpp
    OptimizarVertexGroups1Hueso(g_pendingOptVGMesh);
    g_pendingOptVGMesh = NULL;
    g_redraw = true;
    Notificar(T("Vertex groups optimized (1 bone/vertex)"), false);
}
static void AccionOptimizarVertexGroups(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    g_pendingOptVGMesh = (Mesh*)ObjActivo;
    if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
    confirmarPopup->Abrir("Esto modificara el vertex group y lo simplificara a 1 hueso por vertice (skinning mas rapido). Puede haber perdida de datos.", HacerOptimizarVG);
}

#ifdef __EMSCRIPTEN__
extern "C" void WebDescargarArchivo(const char* path, const char* name); // main.cpp (EM_JS): baja un archivo del FS al disco
#endif
extern bool g_uiTapEnCurso; // (controles.cpp; en Symbian lo define variables.cpp) tap tactil diferido en curso
// teclado tactil de Whisk3D (NumPad numerico + QWERTY). TAMBIEN en Symbian: NumPad.cpp esta en el .mmp y la
// edicion por tap abre QwertyAbrir() (ver rama Text de ClickEn). Antes iba guardado -> QwertyAbrir sin declarar.
#include "ViewPorts/PopUp/NumPad.h"

// solo el nombre de archivo de una ruta (sin carpetas)
static std::string SoloNombre(const std::string& p){
    size_t s = p.find_last_of("/\\");
    return (s == std::string::npos) ? p : p.substr(s + 1);
}
// une el campo Path (carpeta) + el campo File name en una ruta completa. Path vacio -> carpeta
// de salida por defecto (Android: Descargas); Path RELATIVO -> contra la carpeta del proyecto.
// NO duplica logica: delega en W3dRutaEnCarpeta (FileBrowser), la unica que pega carpeta+nombre
// en todo el editor (antes esta funcion era una segunda implementacion y export/render se
// quedaban afuera de los arreglos que se le hacian a la otra).
std::string W3dRutaSalidaCampos(const std::string& dir, const std::string& nombre, const char* nombreDefecto){
    std::string nm = SoloNombre(nombre);
    // nombre vacio (o solo espacios) -> el default del flujo ("model.obj" / "render.png")
    if (nm.find_first_not_of(" \t") == std::string::npos) nm = nombreDefecto ? nombreDefecto : "";
    return W3dRutaEnCarpeta(dir, nm, "");   // sin ext: el nombre ya la trae
}

// ---------- EXPORT (dropdown de formato: OBJ / FBX / glTF / GLB + Path + File name) ----------
// Formato: 0=OBJ 1=FBX 2=glTF 3=GLB. OBJ solo malla (avisa que pierde rig/animaciones); glTF/GLB llevan
// el rig y sus clips SIN hornear el skinning (import_gltf es su inverso). FBX (binario) todavia no exporta.
static const char* ExtDeFormato(int f){ return (f==0)?".obj":(f==1)?".fbx":(f==2)?".gltf":".glb"; }
static const char* NombreFormato(int f){ return (f==0)?"Wavefront .obj":(f==1)?"FBX":(f==2)?"glTF":"GLB"; }

// cambia la extension del File name al formato elegido (respeta el nombre base)
static void ActualizarExtensionExport(){
    if (!PropsActivo || !PropsActivo->propExportName) return;
    std::string nom = PropsActivo->propExportName->field.text;
    size_t dot = nom.find_last_of('.');
    std::string base = (dot == std::string::npos) ? nom : nom.substr(0, dot);
    if (base.empty()) base = "model";
    PropsActivo->propExportName->field.SetText(base + ExtDeFormato(PropsActivo->exportFormat));
}

// true si el modelo a exportar tiene rig (skinArmature) o vertex groups -> el OBJ los perderia (aviso).
static bool ExportMeshPierdeRig(Object* o, bool selOnly){
    if (!o) return false;
    for (size_t i = 0; i < o->Childrens.size(); i++) { Object* c = o->Childrens[i];
        if (c->getType() == ObjectType::mesh && (!selOnly || c->select)) { Mesh* m = (Mesh*)c;
            if (m->skinArmature || !m->vertexGroups.empty()) return true; }
        if (ExportMeshPierdeRig(c, selOnly)) return true; }
    return false;
}

// ConfirmarPopup::onSi no lleva argumentos -> se guarda la ruta pendiente en un estatico.
static std::string g_pendingExportPath;
static void HacerExportActual(){
    if (!PropsActivo) return;
    std::string path = g_pendingExportPath;
    int f = PropsActivo->exportFormat;
    if (f == 1) { Notificar(T("FBX export: not available yet"), true); return; } // el exportador FBX binario todavia no esta
    bool ok = false;
    if (f == 0) {
        ok = ExportOBJ(path, PropsActivo->exportSelectedOnly, PropsActivo->exportApplyModifiers, PropsActivo->exportApplyTransforms);
        if (ok) Notificar(T("OBJ saved successfully!"), false); else Notificar(T("Error: could not save the OBJ"), true);
#ifdef __EMSCRIPTEN__
        if (ok) { std::string mtl = ExtractBaseName(path) + ".mtl"; // web: bajar .obj + .mtl (FS virtual de emscripten)
            WebDescargarArchivo(path.c_str(), SoloNombre(path).c_str()); WebDescargarArchivo(mtl.c_str(), SoloNombre(mtl).c_str()); }
#endif
    } else {
        ok = ExportGLTF(path, PropsActivo->exportSelectedOnly, f == 3); // f==2 glTF (texto), f==3 GLB (binario)
        // ExportGLTF ya tira su propia notificacion + autodescarga en web
    }
    (void)ok;
}
// boton "Export": arma Path + File name segun formato. OBJ avisa la perdida del rig; sino pide sobrescritura.
static void AccionExport(){
    if (!PropsActivo || !PropsActivo->propExportName) return;
    int f = PropsActivo->exportFormat;
    std::string dir    = PropsActivo->propExportPath ? PropsActivo->propExportPath->field.text : std::string();
    std::string nombre = PropsActivo->propExportName->field.text;
    std::string porDefecto = std::string("model") + ExtDeFormato(f);
    std::string full   = W3dRutaSalidaCampos(dir, nombre, porDefecto.c_str());
#ifdef W3D_SYMBIAN
    // N95: carpeta FIJA E:/whisk3d/models/ (creada en AppInit). Toma solo el nombre.
    full = std::string("E:/whisk3d/models/") + SoloNombre(nombre.empty() ? porDefecto : nombre);
#endif
    g_pendingExportPath = full;
    // OBJ con rig: un SOLO cartel que avisa la perdida (y que se sobrescribe si existe) -> Continuar = exportar.
    if (f == 0 && ExportMeshPierdeRig(SceneCollection, PropsActivo->exportSelectedOnly)) {
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir("OBJ does not store the skeleton or animations: only the mesh will be exported (vertex groups, armature and clips will be lost). It will overwrite the file if it already exists. Continue?", HacerExportActual);
        return;
    }
#ifndef W3D_SYMBIAN
    if (w3dFileSystem::FileExists(full)) {
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir("The file \"" + SoloNombre(full) + "\" already exists. Do you want to replace it?", HacerExportActual);
        return;
    }
#endif
    HacerExportActual();
}
// dropdown de formato: elige OBJ/FBX/glTF/GLB y ajusta la extension del File name.
static PopupMenu* MenuExportFormat = NULL;
static void AccionExportFormatElegido(int id){
    if (!PropsActivo) return;
    if (id >= 0 && id <= 3) { PropsActivo->exportFormat = id; ActualizarExtensionExport(); }
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionMenuExportFormat(){
    if (!PropsActivo || !PropsActivo->propExportFormat) return;
    if (!MenuExportFormat) { MenuExportFormat = new PopupMenu(); MenuExportFormat->action = AccionExportFormatElegido; }
    MenuExportFormat->Limpiar();
    MenuExportFormat->Agregar("Wavefront .obj", 0, IconType::mesh);
    MenuExportFormat->Agregar("FBX", 1, IconType::armature);
    MenuExportFormat->Agregar("glTF", 2, IconType::mesh);
    MenuExportFormat->Agregar("GLB", 3, IconType::mesh);
    AbrirMenuBajoBoton(MenuExportFormat, PropsActivo->propExportFormat->button);
}
// el explorador (modo guardar) devolvio una CARPETA: se pone en el campo Path (el nombre no se toca).
static void ExportFolderElegido(const std::string& elegido){
    if (!PropsActivo || !PropsActivo->propExportPath) return;
    // si el usuario eligio un archivo de modelo existente -> separar carpeta y nombre.
    // Se pregunta al disco (IsDir) en vez de mirar si hay un punto: una carpeta con
    // punto en el nombre ("v1.2") se tomaba como archivo.
    bool esArchivo = !elegido.empty() && !w3dFileSystem::IsDir(elegido);
    if (esArchivo) {
        PropsActivo->propExportPath->field.SetText(w3dFileSystem::ParentPath(elegido));
        if (PropsActivo->propExportName) PropsActivo->propExportName->field.SetText(SoloNombre(elegido));
    } else {
        PropsActivo->propExportPath->field.SetText(elegido);
    }
}
// boton de la carpeta: abre el explorador para elegir la carpeta de salida
static void AccionBrowseExport(){
    AbrirFileBrowser("Export to...", T("Use this file"), ".obj .gltf .glb .fbx", ExportFolderElegido, true);
}

// ---------- RENDER (Path + File name + confirmacion) ----------
// base del render = Path/FileName-sin-extension (seteada por AccionRenderImage); RenderFileNamePNG
// le agrega "[_tag]_0001.png". El _0001 es CurrentFrame (para secuencias mas adelante).
static std::string g_pendingRenderBase;
static std::string RenderFileNamePNG(const char* tag){
    std::string base = g_pendingRenderBase;
    if (base.empty()) base = "render";
    if (tag && tag[0]) { base += "_"; base += tag; }
    int frame = 0;
#ifndef W3D_SYMBIAN
    extern int CurrentFrame; // frame de animacion actual (Animation.cpp; en N95 no se linkea todavia)
    frame = CurrentFrame;
#endif
    char suf[24];
    snprintf(suf, sizeof(suf), "_%04d.png", frame);
    base += suf;
    return base;
}
// arma g_pendingRenderBase (Path/FileName-sin-ext) desde los dos campos.
static void CalcularRenderBase(){
    std::string dir = (PropsActivo && PropsActivo->propRenderPath) ? PropsActivo->propRenderPath->field.text : std::string();
    std::string nm  = (PropsActivo && PropsActivo->propRenderOutput) ? PropsActivo->propRenderOutput->field.text : std::string("render.png");
    std::string full = W3dRutaSalidaCampos(dir, nm, "render.png");
    // sacar la extension -> queda dir/nombre
    size_t dot = full.find_last_of('.'), sl = full.find_last_of("/\\");
    std::string base = (dot != std::string::npos && (sl == std::string::npos || dot > sl)) ? full.substr(0, dot) : full;
#ifdef W3D_SYMBIAN
    // N95: carpeta FIJA E:/whisk3d/render/ (prolijo + sabes donde queda). Toma solo el nombre.
    std::string nombre = SoloNombre(base); if (nombre.empty()) nombre = "render";
    base = std::string("E:/whisk3d/render/") + nombre;
#endif
    g_pendingRenderBase = base;
}
// carpeta elegida para el render -> al campo Path
static void RenderFolderElegido(const std::string& elegido){
    if (!PropsActivo || !PropsActivo->propRenderPath) return;
    // igual que el export: se pregunta al disco, no por la extension
    bool esPng = !elegido.empty() && !w3dFileSystem::IsDir(elegido);
    if (esPng) {
        PropsActivo->propRenderPath->field.SetText(w3dFileSystem::ParentPath(elegido));
        if (PropsActivo->propRenderOutput) PropsActivo->propRenderOutput->field.SetText(SoloNombre(elegido));
    } else {
        PropsActivo->propRenderPath->field.SetText(elegido);
    }
}
static void AccionBrowseRender(){
    AbrirFileBrowser("Render to...", T("Use this file"), ".png", RenderFolderElegido, true);
}

// boton "Render Image": guarda el pase beauty (siempre) + los pases tildados (zbuffer/normal)
// como PNG a la resolucion pedida. El render por tiles permite tamanos mayores que la ventana.
// regenera la malla modificada de TODAS las mallas de la escena (para aplicar el cambio de nivel viewport<->render)
static void RegenerarModsEscena(Object* nodo){
    if (!nodo) return;
    for (size_t i=0; i<nodo->Childrens.size(); i++){ Object* o = nodo->Childrens[i]; if (!o) continue;
        if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (!m->modificadores.empty()) m->GenerarMallaModificada(); }
        RegenerarModsEscena(o); }
}

// al cambiar Width/Height del render -> actualiza el aspecto global (la geometria de las camaras lo sigue,
// responsive: 1:1 cuadrada, 4:3 en 4:3, etc.). onChange de propRenderW/propRenderH.
static void ActualizarAspectoRender(){
    if (!PropsActivo) return;
    float w = PropsActivo->renderW, h = PropsActivo->renderH;
    g_renderAspect = (h > 0.5f) ? (w / h) : 1.0f;
}

// pases activos a guardar: beauty (siempre) + los tildados (zbuffer/normal/alpha)
static int PasesActivos(){
    return 1 + (PropsActivo->renderZbuffer?1:0) + (PropsActivo->renderNormal?1:0) + (PropsActivo->renderAlpha?1:0);
}
// rendea los pases de UN frame, avanzando la barra de progreso desde 'progBase' hacia 'progTotal' (contados en
// TILES). Devuelve cuantos tiles consumio (para que el caller acumule el base entre frames). Suma a 'fallos'.
// El 'total' NO se resetea aca -> Render Image usa 1 frame; Render Animation usa frames x pases x tiles.
static int RenderPasesFrame(Viewport3D* vp, int w, int h, int progBase, int progTotal, int& fallos){
    bool doZ = PropsActivo->renderZbuffer, doN = PropsActivo->renderNormal, doA = PropsActivo->renderAlpha;
    int tpp = vp->TilesNecesarios(w, h);
    // Subdivision (y cualquier modificador con nivel de render): regenerar con el nivel de RENDER antes de renderizar
    extern bool g_modRenderMode;
    g_modRenderMode = true; RegenerarModsEscena(SceneCollection);
    int base = progBase;
    if (!vp->RenderAPNG(w, h, RenderType::Rendered, RenderFileNamePNG("").c_str(), base, progTotal)) fallos++;
    base += tpp;
    if (doZ){ if (!vp->RenderAPNG(w, h, RenderType::ZBuffer,    RenderFileNamePNG("zbuffer").c_str(), base, progTotal)) fallos++; base += tpp; }
    if (doN){ if (!vp->RenderAPNG(w, h, RenderType::NormalView, RenderFileNamePNG("normal").c_str(),  base, progTotal)) fallos++; base += tpp; }
    if (doA){ if (!vp->RenderAPNG(w, h, RenderType::Alpha,      RenderFileNamePNG("alpha").c_str(),   base, progTotal)) fallos++; base += tpp; }
    g_modRenderMode = false; RegenerarModsEscena(SceneCollection); // volver al nivel de VIEWPORT
    return base - progBase; // tiles consumidos por este frame (nPases * tpp)
}

// hace el render REAL de UNA imagen (llamado directo, o desde el "Si" de la confirmacion de sobrescritura).
static void HacerRenderImage(){
    if (!PropsActivo) return;
    Viewport3D* vp = Viewport3DActive;
    if (!vp) { Notificar(T("No active 3D viewport"), true); return; }
    int w = (int)(PropsActivo->renderW + 0.5f); if (w < 1) w = 1;
    int h = (int)(PropsActivo->renderH + 0.5f); if (h < 1) h = 1;
    int total = PasesActivos() * vp->TilesNecesarios(w, h); // 1 frame: PASES x TILES
    ProgresoIniciar("Rendering...");
    int fallos = 0;
    RenderPasesFrame(vp, w, h, 0, total, fallos);
    ProgresoFin();
    if (fallos == 0) Notificar(T("Render saved!"), false);
    else             Notificar(T("Error: could not save the render"), true);
}
// boton "Render Image": arma Path + File name; si el PNG (pase beauty) ya existe, pide confirmacion.
// Start / End / FPS de la animacion (tarjeta Animation): espejos float de los int globales StartFrame/EndFrame/AnimFPS
static float g_animFpsF = 30.0f, g_animStartF = 1.0f, g_animEndF = 250.0f;
static PropFloat* gPropAnimFps = NULL;   // campo "FPS"
static PropFloat* gPropAnimStart = NULL; // campo "Start"
static PropFloat* gPropAnimEnd = NULL;   // campo "End"
static void AccionAnimFps(){ int f = (int)(g_animFpsF + 0.5f); if (f < 1) f = 1; if (f > 120) f = 120; AnimSetFps(f); g_animFpsF = (float)f; }
// "Velocidad" de la VERTEX ANIM activa (kind 3): cada anim tiene la suya (no
// todos los clips van a la misma; el juego la usa para el blend de frames)
static float g_vertVelF = 1.0f;
static PropFloat* gPropVertVel = NULL;
static VertexAnimation* VertexAnimActivaSel(){
    if (ActiveAnimKind != 3 || !ActiveAnimMesh) return NULL;
    VertexAnimationActive* va = FindTargetAnim(ActiveAnimMesh);
    int i = va ? va->currentAnim : -1;
    return (i >= 0 && i < (int)ActiveAnimMesh->animations.size()) ? ActiveAnimMesh->animations[i] : NULL;
}
static void AccionVertVel(){
    VertexAnimation* an = VertexAnimActivaSel();
    if (an) { if (g_vertVelF < 0.01f) g_vertVelF = 0.01f; an->speed = g_vertVelF; }
}
static void AccionAnimStart(){ int v = (int)(g_animStartF + 0.5f); if (v < 0) v = 0; AnimSetStart(v); }
static void AccionAnimEnd(){ int v = (int)(g_animEndF + 0.5f); if (v < 1) v = 1; AnimSetEnd(v); }
// los campos SIEMPRE reflejan los globales reales (que el import / el timeline cambian). Sin esto el display
// mostraba 30 pero se reproducia a 24. Lo llama RefreshTargetProperties cada frame (salvo el campo en edicion).
void SincronizarAnimFps(){
    // los campos Inicio/Fin/FPS de la tarjeta Animacion del OBJETO reflejan la
    // animacion propia activa (kind 3): sus valores viven en StartFrame/EndFrame/AnimFPS
    extern PropFloat *gPropObjAnimStart, *gPropObjAnimEnd, *gPropObjAnimFps;
    extern float g_objAnimStartF, g_objAnimEndF, g_objAnimFpsF;
    if (gPropObjAnimStart && g_propFloatEditando != gPropObjAnimStart) g_objAnimStartF = (float)StartFrame;
    if (gPropObjAnimEnd   && g_propFloatEditando != gPropObjAnimEnd)   g_objAnimEndF   = (float)EndFrame;
    if (gPropObjAnimFps   && g_propFloatEditando != gPropObjAnimFps)   g_objAnimFpsF   = (float)AnimFPS;
    if (gPropAnimFps   && g_propFloatEditando != gPropAnimFps)   g_animFpsF   = (float)AnimFPS;
    if (gPropAnimStart && g_propFloatEditando != gPropAnimStart) g_animStartF = (float)StartFrame;
    if (gPropAnimEnd   && g_propFloatEditando != gPropAnimEnd)   g_animEndF   = (float)EndFrame;
    if (gPropVertVel && g_propFloatEditando != gPropVertVel) {
        VertexAnimation* an = VertexAnimActivaSel();
        gPropVertVel->value = an ? &g_vertVelF : NULL;   // sin vertex anim activa la fila no ocupa lugar
        if (an) g_vertVelF = an->speed;
    }
}
static void AccionRenderImage(){
    if (!PropsActivo) return;
    if (!Viewport3DActive) { Notificar(T("No active 3D viewport"), true); return; }
    CalcularRenderBase(); // setea g_pendingRenderBase desde los campos Path + File name
#ifndef W3D_SYMBIAN
    std::string beauty = RenderFileNamePNG(""); // el pase principal
    if (w3dFileSystem::FileExists(beauty)) {
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir("The file \"" + SoloNombre(beauty) + "\" already exists. Do you want to replace it?", HacerRenderImage);
        return;
    }
#endif
    HacerRenderImage();
}

// "Render Animation": rendea la SECUENCIA de PNGs de StartFrame..EndFrame (loop del timeline). Cada frame evalua la
// animacion (esqueleto + transform de objetos) y guarda base_0001.png, base_0002.png, ... (RenderFileNamePNG usa
// CurrentFrame). Se restaura el frame al terminar.
static void HacerRenderAnimation(){
    if (!PropsActivo || !Viewport3DActive) return;
    Viewport3D* vp = Viewport3DActive;
    extern int CurrentFrame, StartFrame, EndFrame;
    extern void AplicarAnimacionObjetos();
    int w = (int)(PropsActivo->renderW + 0.5f); if (w < 1) w = 1;
    int h = (int)(PropsActivo->renderH + 0.5f); if (h < 1) h = 1;
    int f0 = StartFrame, f1 = EndFrame; if (f1 < f0){ int t=f0; f0=f1; f1=t; }
    int nFrames = f1 - f0 + 1;
    // barra de progreso UNICA para TODA la secuencia: total = FRAMES x PASES x TILES. Cada imagen (frame+pase)
    // es una fraccion del total -> 50 frames x 2 pases = 100 imagenes, cada una ~1%. (Antes iba 0..100 por frame.)
    int total = nFrames * PasesActivos() * vp->TilesNecesarios(w, h);
    int guardado = CurrentFrame;
    CalcularRenderBase();
    ProgresoIniciar("Rendering animation...");
    int fallos = 0, base = 0;
    for (int f = f0; f <= f1; f++){
        CurrentFrame = f;
        AplicarAnimacionObjetos(); // transform de objetos al frame f
        base += RenderPasesFrame(vp, w, h, base, total, fallos); // rendea el frame; avanza la barra GLOBAL
    }
    ProgresoFin();
    CurrentFrame = guardado;
    AplicarAnimacionObjetos(); // volver la escena al frame que estaba
    if (fallos == 0) Notificar(T("Animation rendered!"), false);
    else             Notificar(T("Error rendering animation"), true);
    g_redraw = true;
}
static void AccionRenderAnimation(){
    if (!PropsActivo) return;
    if (!Viewport3DActive) { Notificar(T("No active 3D viewport"), true); return; }
    HacerRenderAnimation();
}

// === pestaña VERTICES: helpers + acciones (UV Maps + capas de color) ===
static Mesh* VerticesMesh() {
    return (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
}
// (la SELECCION de la capa activa la hace la lista PropListMeshParts; aca solo Add + el toggle)
// PropertiesLayoutDirty = recalcula alturas + la SCROLLBAR (sino no se podia scrollear al item nuevo)
static void AccionVertAddUVMap()  { Mesh* m = VerticesMesh(); if (m) { DuplicarUVMapActivo(m); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertAddColor()  { Mesh* m = VerticesMesh(); if (m) { DuplicarColorLayerActivo(m); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertDelUVMap()  { Mesh* m = VerticesMesh(); if (m) { BorrarUVMapActivo(m);   m->AplicarCapasAlRender(); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertUVMapUp()   { Mesh* m = VerticesMesh(); if (m) { MoverUVMapActivo(m,-1);  m->AplicarCapasAlRender(); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertUVMapDown() { Mesh* m = VerticesMesh(); if (m) { MoverUVMapActivo(m,+1);  m->AplicarCapasAlRender(); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertDelColor()  { Mesh* m = VerticesMesh(); if (m) { BorrarColorLayerActivo(m);  m->AplicarCapasAlRender(); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertColorUp()   { Mesh* m = VerticesMesh(); if (m) { MoverColorLayerActivo(m,-1); m->AplicarCapasAlRender(); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertColorDown() { Mesh* m = VerticesMesh(); if (m) { MoverColorLayerActivo(m,+1); m->AplicarCapasAlRender(); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertAddGroup()  { Mesh* m = VerticesMesh(); if (m) { CrearVertexGroup(m);         PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertDelGroup()  { Mesh* m = VerticesMesh(); if (m) { BorrarVertexGroupActivo(m);  PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertGroupUp()   { Mesh* m = VerticesMesh(); if (m) { MoverVertexGroupActivo(m,-1); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertGroupDown() { Mesh* m = VerticesMesh(); if (m) { MoverVertexGroupActivo(m,+1); PropertiesLayoutDirty = true; g_redraw = true; } }
// ===== ASSIGN / REMOVE / SELECT / DESELECT de los DOS grupos de pesos =====
// Es el MISMO patron (y los mismos labels) que la fila de Mesh Parts, aplicado a las dos
// entidades de pesos. Antes, la unica forma de meter algo en un grupo era PINTARLO con el
// pincel: no habia camino "seleccionar y asignar", que es como se arma un rig a mano.
//
// PESO ASIGNADO = 1.0 (a proposito, por ahora): es el peso util el 99% de las veces y evita
// meter un slider "Weight" mas en la tarjeta. Remove BORRA la entrada sparse (PesoAsignar con 0),
// no la deja en cero. Los dos van en UN paso de undo (UndoPesosIniciar/Confirmar guarda las dos
// listas de grupos, ver Undo.h).
//
// LA DIFERENCIA ENTRE LAS DOS ENTIDADES (ver el bloque VertexGroup/UVGroup en Mesh.h) se respeta:
//   Vertex Groups -> CONTROL-POINTS, operan la seleccion de EDIT MODE del 3D.
//   UV Groups     -> RENDER-VERTS (corners), operan la seleccion EFECTIVA del UV
//                    (UVVertsSelEfectivos: la propia del editor UV si la hay, si no la del 3D).
// Nada se bakea de una a la otra.
static void AccionVertGroupAssign()   { Mesh* m = VerticesMesh(); VertexGroupAsignarSel(m, true); }
static void AccionVertGroupRemove()   { Mesh* m = VerticesMesh(); VertexGroupAsignarSel(m, false); }
static void AccionVertGroupSelect()   { Mesh* m = VerticesMesh(); VertexGroupSeleccionar(m, true); }
static void AccionVertGroupDeselect() { Mesh* m = VerticesMesh(); VertexGroupSeleccionar(m, false); }
static void AccionUVGroupAssign()     { Mesh* m = VerticesMesh(); UVGroupAsignarSel(m, true); }
static void AccionUVGroupRemove()     { Mesh* m = VerticesMesh(); UVGroupAsignarSel(m, false); }
static void AccionUVGroupSelect()     { Mesh* m = VerticesMesh(); UVGroupSeleccionar(m, true); }
static void AccionUVGroupDeselect()   { Mesh* m = VerticesMesh(); UVGroupSeleccionar(m, false); }
// UV GROUPS (pesos por corner: editor UV + armature 2D). Mismas 4 acciones, otra entidad.
static void AccionUVAddGroup()    { Mesh* m = VerticesMesh(); if (m) { CrearUVGroup(m, "UV Group"); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionUVDelGroup()    { Mesh* m = VerticesMesh(); if (m) { BorrarUVGroupActivo(m);   PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionUVGroupUp()     { Mesh* m = VerticesMesh(); if (m) { MoverUVGroupActivo(m,-1); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionUVGroupDown()   { Mesh* m = VerticesMesh(); if (m) { MoverUVGroupActivo(m,+1); PropertiesLayoutDirty = true; g_redraw = true; } }
// ARMATURE: crear / borrar / mover el clip de animacion activo (mismo patron que los vertex groups)
static void InvalidarSkinEscena(); // def mas abajo (re-deforma las mallas skinneadas a la pose actual)
static void AccionAnimAdd()  { Armature* a = ArmActiva(); if (a) { CrearAnimacion(a); InvalidarSkinEscena(); PropertiesLayoutDirty = true; g_redraw = true; } } // New Animation: clip vacio en pose reset
static void AccionAnimDup()  { Armature* a = ArmActiva(); if (a && a->animActiva >= 0) { DuplicarAnimacionActiva(a); PropertiesLayoutDirty = true; g_redraw = true; } } // Duplicate: copia el clip activo
// borrar/mover NO llaman a BorrarAnimacionActiva/MoverAnimacionActiva del Core: los destinos de
// rename ya capturados van por (armature, INDICE) y las dos operaciones corren esos indices ->
// van por el undo, que remapea (y el mover ademas deja el Ctrl+Z). Ver Undo.h.
static void AccionAnimDel()  { Armature* a = ArmActiva(); if (a) { UndoBorrarClipArm(a);       PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionAnimUp()   { Armature* a = ArmActiva(); if (a) { UndoMoverClipArm(a, a->animActiva, a->animActiva-1); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionAnimDown() { Armature* a = ArmActiva(); if (a) { UndoMoverClipArm(a, a->animActiva, a->animActiva+1); PropertiesLayoutDirty = true; g_redraw = true; } }

// ===== tarjeta ANIMATION: selector de la animacion ACTIVA (Scene(s) / clips del armature seleccionado) + New/Delete
// + Rename + Render Animation. La seleccion es APP-WIDE (ActiveAnimKind/ActiveAnimArm/SceneAnimActiva en el core): la
// comparten esta card y el Timeline, y NO depende del objeto seleccionado (clickear un armature no la cambia). =====
// El menu del selector es JERARQUICO (sino un esqueleto con 200 clips seria inmanejable): un submenu "Scenes" con
// todas las animaciones de escena, y un submenu por ARMADURA (con el nombre del esqueleto) con SUS clips. Asi las
// animaciones de todas las armaduras estan disponibles sin tener que seleccionar el objeto.
// ids: [0..) = escena; [BASE + armIdx*STRIDE + clipIdx] = clip. armIdx indexa g_animMenuArms (llenado al construir).
static const int ANIM_CLIP_BASE = 100000;
static const int ANIM_CLIP_STRIDE = 1000; // hasta 1000 clips por armadura
static const int ANIM_VERT_BASE = 200000;   // vertex anims: [BASE + meshIdx*STRIDE + animIdx]
static const int ANIM_ARM2D_BASE = 300000;  // clips del ARMATURE 2D: [BASE + meshIdx*STRIDE + clipIdx]
static std::vector<Mesh*> g_animMenuMeshes; // mallas (con vertex anims) en el orden del menu
static std::vector<Mesh*> g_animMenu2D;     // mallas (con clips de armature 2D) en el orden del menu
static std::vector<int>   g_animMenu2DArm;  // paralelo: QUE armature 2D de esa malla es cada entrada
static std::vector<PopupMenu*> g_animSubmenus; // pool reutilizable (0 = Scenes, 1.. = por armadura); persiste entre aperturas
static std::vector<Armature*>  g_animMenuArms; // armaduras (con clips) en el orden del menu, para decodificar el id
static std::string NombreAnimActiva(){
    if (ActiveAnimKind == 2) return "Juego";
    if (ActiveAnimKind == 4 && ActiveAnimMesh) {   // clip del ARMATURE 2D de la malla
        Armature2DAnimation* c2 = Arm2DClipActivo(ActiveAnimMesh);
        return c2 ? c2->name : ActiveAnimMesh->name;
    }
    if (ActiveAnimKind == 3 && ActiveAnimMesh) {
        VertexAnimationActive* va = FindTargetAnim(ActiveAnimMesh);
        int i = va ? va->currentAnim : -1;
        if (i >= 0 && i < (int)ActiveAnimMesh->animations.size() && ActiveAnimMesh->animations[i])
            return ActiveAnimMesh->animations[i]->name;
        return ActiveAnimMesh->name;
    }
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size() &&
        ActiveAnimArm->animations[ActiveAnimArm->animActiva])
        return ActiveAnimArm->animations[ActiveAnimArm->animActiva]->name;
    return NombreEscenaActiva();
}
static void RecolectarMeshesAnim(Object* nodo, std::vector<Mesh*>& out){
    if (!nodo) return;
    for (size_t i=0;i<nodo->Childrens.size();i++){ Object* o=nodo->Childrens[i]; if(!o) continue;
        if (o->getType()==ObjectType::mesh && !((Mesh*)o)->animations.empty()) out.push_back((Mesh*)o);
        RecolectarMeshesAnim(o, out); }
}
// ARMATURES 2D con CLIPS (kind 4): cada (malla, armature) es un submenu propio del selector. Se
// listan TODOS los armatures de la malla, no solo el activo: elegir un clip de otro rig 2D tambien
// lo deja ACTIVO (AnimSelArm2DEn), que es lo que hace que el dope muestre SUS curvas.
static void RecolectarMeshes2D(Object* nodo, std::vector<Mesh*>& out, std::vector<int>& outArm){
    if (!nodo) return;
    for (size_t i=0;i<nodo->Childrens.size();i++){ Object* o=nodo->Childrens[i]; if(!o) continue;
        if (o->getType()==ObjectType::mesh){
            Mesh* mm = (Mesh*)o;
            for (size_t a=0;a<mm->armatures2d.size();a++)
                if (mm->armatures2d[a] && !mm->armatures2d[a]->anims.empty()){ out.push_back(mm); outArm.push_back((int)a); }
        }
        RecolectarMeshes2D(o, out, outArm); }
}
static void RecolectarArmaduras(Object* nodo, std::vector<Armature*>& out){
    if (!nodo) return;
    for (size_t i=0;i<nodo->Childrens.size();i++){ Object* o=nodo->Childrens[i]; if(!o) continue;
        if (o->getType()==ObjectType::armature) out.push_back((Armature*)o);
        RecolectarArmaduras(o, out); }
}
static PopupMenu* AnimSubmenuPool(size_t i){ while (g_animSubmenus.size() <= i) g_animSubmenus.push_back(new PopupMenu()); return g_animSubmenus[i]; }
// construye el menu jerarquico en 'menu' (lo comparten la tarjeta y el timeline). El id de los items bubbles hasta
// menu->action (ver PopupMenu::Click), asi que los submenus heredan la misma action.
// id reservado del selector: la "animacion" JUEGO (tiempo infinito, corre los
// scripts). POSITIVO y debajo de ANIM_CLIP_BASE: el dispatch del menu ignora los
// ids negativos (por eso el "Juego" original no entraba) y este valor no puede
// colisionar con una escena real (se chequea PRIMERO en AnimSelPorId).
#define ANIM_ID_JUEGO 99999
// ids del submenu "Nueva animacion" (positivos y por DEBAJO de ANIM_ID_JUEGO -> se chequean primero
// en AnimSelPorId, antes de que 99990.. se confunda con el indice de una escena).
#define ANIM_ID_NEW_ESCENA 99990
#define ANIM_ID_NEW_OBJETO 99991
#define ANIM_ID_NEW_ARM    99992
#define ANIM_ID_NEW_ARM2D  99993

void ConstruirMenuAnim(PopupMenu* menu){
    menu->Limpiar();
    InitSceneAnimations();
    menu->Agregar("Juego", ANIM_ID_JUEGO, IconType::gamepad);
    PopupMenu* subEsc = AnimSubmenuPool(0); subEsc->Limpiar(); subEsc->action = menu->action; // submenu "Scenes"
    for (size_t i=0;i<SceneAnimations.size();i++) subEsc->Agregar(SceneAnimations[i]->name, (int)i, IconType::camera);
    menu->Agregar(T("Scenes"), 0, IconType::camera, subEsc);
    g_animMenuArms.clear();                                                   // un submenu por armadura CON clips
    std::vector<Armature*> todas; RecolectarArmaduras(SceneCollection, todas);
    for (size_t t=0;t<todas.size();t++){
        Armature* arm = todas[t]; if (arm->animations.empty()) continue;      // sin clips no aporta al selector
        int a = (int)g_animMenuArms.size(); g_animMenuArms.push_back(arm);
        PopupMenu* sub = AnimSubmenuPool(a+1); sub->Limpiar(); sub->action = menu->action;
        for (size_t c=0;c<arm->animations.size();c++)
            sub->Agregar(arm->animations[c] ? arm->animations[c]->name : std::string("Animation"),
                         ANIM_CLIP_BASE + a*ANIM_CLIP_STRIDE + (int)c, IconType::armature);
        menu->Agregar(arm->name, 0, IconType::armature, sub);
    }
    // un submenu por MALLA con vertex animations (idle/correr/caer de un personaje...):
    // elegir una la pone en el timeline (kind 3, el playhead aplica sus frames)
    g_animMenuMeshes.clear();
    std::vector<Mesh*> mallas; RecolectarMeshesAnim(SceneCollection, mallas);
    for (size_t t=0;t<mallas.size();t++){
        Mesh* mm = mallas[t];
        int a = (int)g_animMenuMeshes.size(); g_animMenuMeshes.push_back(mm);
        PopupMenu* sub = AnimSubmenuPool(1 + g_animMenuArms.size() + a);
        sub->Limpiar(); sub->action = menu->action;
        for (size_t c=0;c<mm->animations.size();c++)
            sub->Agregar(mm->animations[c] ? mm->animations[c]->name : std::string("Anim"),
                         ANIM_VERT_BASE + a*ANIM_CLIP_STRIDE + (int)c, IconType::mesh);
        menu->Agregar(mm->name, 0, IconType::mesh, sub);
    }
    // un submenu por MALLA con CLIPS DEL ARMATURE 2D (kind 4): las curvas por hueso del rig 2D.
    // Es una lista APARTE de la de vertex anims: una misma malla puede tener las dos cosas (UV morph
    // horneado + rig 2D con curvas) y son animaciones distintas.
    g_animMenu2D.clear(); g_animMenu2DArm.clear();
    std::vector<Mesh*> m2d; std::vector<int> m2dArm; RecolectarMeshes2D(SceneCollection, m2d, m2dArm);
    for (size_t t=0;t<m2d.size();t++){
        Mesh* mm = m2d[t];
        const Armature2D* arm = mm->armatures2d[m2dArm[t]];
        int a = (int)g_animMenu2D.size(); g_animMenu2D.push_back(mm); g_animMenu2DArm.push_back(m2dArm[t]);
        PopupMenu* sub = AnimSubmenuPool(1 + g_animMenuArms.size() + g_animMenuMeshes.size() + a);
        sub->Limpiar(); sub->action = menu->action;
        for (size_t c=0;c<arm->anims.size();c++)
            sub->Agregar(arm->anims[c] ? arm->anims[c]->name : std::string("Anim2D"),
                         ANIM_ARM2D_BASE + a*ANIM_CLIP_STRIDE + (int)c, IconType::armature);
        // el nombre dice la malla Y el rig 2D: una malla puede tener varios (curvas independientes)
        menu->Agregar(mm->name + " (" + arm->nombre + ")", 0, IconType::armature, sub);
    }
    // ---- "Nueva animacion": CREAR desde el selector (antes solo se podia desde el panel, y una
    // malla sin animaciones ni siquiera aparecia aca -> no habia camino desde el timeline).
    // Cada item es EXPLICITO (no adivina por contexto como hacia el boton New de la tarjeta).
    {
        PopupMenu* subN = AnimSubmenuPool(1 + g_animMenuArms.size() + g_animMenuMeshes.size() + g_animMenu2D.size());
        subN->Limpiar(); subN->action = menu->action;
        subN->Agregar(T("Scene Animation"), ANIM_ID_NEW_ESCENA, IconType::camera);
        Mesh* mAct = (ObjActivo && ObjActivo->getType()==ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (mAct) subN->Agregar(T("Object Animation"), ANIM_ID_NEW_OBJETO, IconType::mesh);
        if (ObjActivo && ObjActivo->getType()==ObjectType::armature)
            subN->Agregar(T("Armature Animation"), ANIM_ID_NEW_ARM, IconType::armature);
        else if (ActiveAnimKind == 1 && ActiveAnimArm)
            subN->Agregar(T("Armature Animation"), ANIM_ID_NEW_ARM, IconType::armature);
        if (mAct && !mAct->Arm2DHuesos().empty())
            subN->Agregar(T("2D Armature Animation"), ANIM_ID_NEW_ARM2D, IconType::armature);
        menu->Agregar(T("New Animation"), 0, IconType::mas, subN);
    }
}

// selecciona una VERTEX ANIM en el timeline (kind 3): el rango del timeline pasa a
// ser sus frames y el playhead manda sobre la malla (UpdateAnimations la saltea)
void AnimSelVertex(Mesh* m, int idx){
    if (!m || idx < 0 || idx >= (int)m->animations.size() || !m->animations[idx]) return;
    // Las filas de VERTICES/UV/NORMALES del dope se llaman "objvtx:<malla>" y NO dicen de que
    // vertex anim son (se resuelven por la ACTIVA al usarlas): pasar a OTRA anim con keyframes
    // elegidos aplicaria esa seleccion -mover / borrar / interpolar, y la tarjeta Keyframe- a
    // las poses de la otra. Se sueltan (ver DopeSoltarVertexAnim en Timeline.h).
    {
        VertexAnimationActive* vaAnt = FindTargetAnim(m);
        const bool misma = (ActiveAnimKind == 3 && ActiveAnimMesh == m && vaAnt && vaAnt->currentAnim == idx);
        if (!misma) DopeSoltarVertexAnim();
    }
    m->animations[idx]->target = m;   // las anims del .w3d traen target NULL (ApplyVertexFrame lo exige)
    ActiveAnimKind = 3; ActiveAnimMesh = m;
    AnimEsJuego = false;
    VertexAnimationActive* va = FindTargetAnim(m);
    if (!va) { NewActiveVertexAnimation(m, m->animations[idx]); va = FindTargetAnim(m); }
    if (va) { va->currentAnim = va->nextAnim = idx; va->currentFrame = va->nextFrame = 0; va->blendStep = 0.0f; va->playFrame = (float)m->animations[idx]->startFrame; }
    // cargar el rango/fps PROPIOS de esta animacion (cada una los suyos)
    VertexAnimation* an = m->animations[idx];
    StartFrame = an->startFrame; EndFrame = an->endFrame; AnimFPS = an->fps;
    if (EndFrame < StartFrame) EndFrame = StartFrame;
    if (CurrentFrame < StartFrame || CurrentFrame > EndFrame) CurrentFrame = StartFrame;
    g_redraw = true;
}
// selecciona un CLIP DEL ARMATURE 2D en el timeline (kind 4): el rango del timeline pasa a ser el
// del clip y el playhead evalua las curvas por hueso (Armature2DEvaluar) -> el UV se deforma por
// SKINNING, no por la capa uv horneada. OJO: kind 4 NO congela UpdateAnimations de la escena (eso
// lo hace el kind 3 a proposito); un rig 2D animado convive con el resto del mundo.
// version que ademas ELIGE el armature 2D (arm) de la malla: el timeline muestra UN clip por vez,
// asi que seleccionar el clip de otro rig lo hace ACTIVO. AnimSelArm2D(m,idx) = el activo de ahora.
void AnimSelArm2D(Mesh* m, int idx);   // (definida justo abajo)
void AnimSelArm2DEn(Mesh* m, int arm, int idx){
    if (!m || arm < 0 || arm >= (int)m->armatures2d.size()) return;
    // aca el indice se escribe DIRECTO (y no por Arm2DSetActivo) porque el AnimSelArm2D de abajo
    // carga el rango del clip 'idx' que se acaba de elegir, que es mas especifico: recargar antes
    // seria cargar el rango del clip que ESTABA activo en ese rig y pisarlo un renglon despues.
    // Todo OTRO camino que cambia el rig activo tiene que ir por Arm2DSetActivo.
    m->armature2dActivo = arm;
    AnimSelArm2D(m, idx);
}
void AnimSelArm2D(Mesh* m, int idx){
    if (!m || idx < 0 || idx >= (int)m->Arm2DAnims().size() || !m->Arm2DAnims()[idx]) return;
    ActiveAnimKind = 4; ActiveAnimMesh = m; m->Arm2DAnimActiva() = idx;
    AnimEsJuego = false;
    Armature2DAnimation* c = m->Arm2DAnims()[idx];
    StartFrame = c->startFrame; EndFrame = c->endFrame; if (c->fps > 0) AnimFPS = c->fps;
    if (EndFrame < StartFrame) EndFrame = StartFrame;
    if (CurrentFrame < StartFrame || CurrentFrame > EndFrame) CurrentFrame = StartFrame;
    m->last2dFrame = -999999; m->last2dAnim = -999;   // forzar re-evaluacion del clip nuevo
    g_redraw = true;
}
// aplica la seleccion segun el id del menu (escena o clip de una armadura). La comparten la card y el timeline.
// al cambiar de animacion activa: invalidar pose+skin de TODA la escena para que la malla se deforme YA a la pose del
// frame actual (sin esperar al play). Cada armature re-evalua su pose; cada malla skinneada re-skinnea en el proximo render.
static void InvalidarSkinEscena(){
    extern Object* SceneCollection;
    struct L { static void rec(Object* o){ if (!o) return;
        if (o->getType()==ObjectType::armature) ((Armature*)o)->lastPoseFrame = -999999;
        else if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (m->skinArmature) m->lastSkinFrame = -999999; }
        for (size_t i=0;i<o->Childrens.size();i++) rec(o->Childrens[i]); } };
    L::rec(SceneCollection);
    g_redraw = true;
}
void AnimSelPorId(int id){
    if (id == ANIM_ID_JUEGO){
        // el JUEGO: tiempo INFINITO (sin Fin, sin loop); el PLAY corre la simulacion
        // de scripts y el cache rojo del viaje en el tiempo vive aca. Las animaciones
        // normales se siguen editando eligiendo una escena o un clip.
        ActiveAnimKind = 2; AnimEsJuego = true;
        StartFrame = 1;
        if (CurrentFrame < StartFrame) CurrentFrame = StartFrame;
        InvalidarSkinEscena();
        return;
    }
    // ---- "Nueva animacion": CREAR (los ids van ANTES de las bases; ver ANIM_ID_NEW_*) ----
    if (id == ANIM_ID_NEW_ESCENA){ AnimEsJuego = false; NuevaEscena(); ActiveAnimKind = 0; ActiveAnimMesh = NULL;
                                   AnimCargarRangoActivo(); InvalidarSkinEscena(); return; }
    if (id == ANIM_ID_NEW_OBJETO){ AnimEsJuego = false;
        extern void _NuevaVertexAnimationFwd(Mesh*);   // definida mas abajo (orden de declaracion)
        if (ObjActivo && ObjActivo->getType()==ObjectType::mesh) _NuevaVertexAnimationFwd((Mesh*)ObjActivo);
        else Notificar(T("Object animation: meshes only for now"), true);
        return; }
    if (id == ANIM_ID_NEW_ARM){ AnimEsJuego = false;
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : ActiveAnimArm;
        if (a){ CrearAnimacion(a); ActiveAnimKind = 1; ActiveAnimArm = a; AnimCargarRangoActivo(); InvalidarSkinEscena(); }
        return; }
    if (id == ANIM_ID_NEW_ARM2D){ AnimEsJuego = false;
        Mesh* m = (ObjActivo && ObjActivo->getType()==ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (m && !m->Arm2DHuesos().empty()){ Arm2DCrearAnimacion(m); AnimSelArm2D(m, m->Arm2DAnimActiva()); }
        else Notificar(T("2D armature animation: the mesh has no 2D armature"), true);
        return; }
    AnimEsJuego = false;
    if (id >= ANIM_ARM2D_BASE){
        int k = id - ANIM_ARM2D_BASE, mIdx = k / ANIM_CLIP_STRIDE, aIdx = k % ANIM_CLIP_STRIDE;
        if (mIdx >= 0 && mIdx < (int)g_animMenu2D.size()) AnimSelArm2DEn(g_animMenu2D[mIdx], g_animMenu2DArm[mIdx], aIdx);
        return;
    }
    if (id >= ANIM_VERT_BASE){
        int k = id - ANIM_VERT_BASE, mIdx = k / ANIM_CLIP_STRIDE, aIdx = k % ANIM_CLIP_STRIDE;
        if (mIdx >= 0 && mIdx < (int)g_animMenuMeshes.size())
            AnimSelVertex(g_animMenuMeshes[mIdx], aIdx);
        return;
    }
    if (id >= ANIM_CLIP_BASE){
        int k = id - ANIM_CLIP_BASE, armIdx = k / ANIM_CLIP_STRIDE, clipIdx = k % ANIM_CLIP_STRIDE;
        if (armIdx >= 0 && armIdx < (int)g_animMenuArms.size()){
            Armature* a = g_animMenuArms[armIdx];
            if (a && clipIdx >= 0 && clipIdx < (int)a->animations.size()){ ActiveAnimKind = 1; ActiveAnimArm = a; a->animActiva = clipIdx; }
        }
    } else { ActiveAnimKind = 0; SetEscenaActiva(id); }
    AnimCargarRangoActivo(); // Start/End/FPS propios de la animacion elegida
    InvalidarSkinEscena();   // deformar la malla YA a la pose del frame actual (sin esperar al play)
}
static void AccionAnimSelElegida(int id){ AnimSelPorId(id); PropertiesLayoutDirty = true; g_redraw = true; }
// hook de la LISTA de animaciones (PropList modo 5, tab Armature): al elegir un clip ahi, sincroniza la seleccion
// APP-WIDE (igual que el selector del timeline) + carga Start/End/FPS. Antes la lista solo cambiaba animActiva y el
// timeline no se enteraba (bug: "el selector de animation no cambia la animacion del timeline").
static void SincronizarAnimClipDesdeLista(Armature* a, int clipIdx){
    if (!a || clipIdx < 0 || clipIdx >= (int)a->animations.size()) return;
    ActiveAnimKind = 1; ActiveAnimArm = a; a->animActiva = clipIdx;
    AnimCargarRangoActivo(); InvalidarSkinEscena();
    PropertiesLayoutDirty = true; g_redraw = true;
}
static PopupMenu* MenuAnimSel = NULL;
static void AccionMenuAnimSel(){
    if (!PropsActivo || !PropsActivo->propBtnAnimSel) return;
    if (!MenuAnimSel){ MenuAnimSel = new PopupMenu(); MenuAnimSel->action = AccionAnimSelElegida; }
    ConstruirMenuAnim(MenuAnimSel);
    AbrirMenuBajoBoton(MenuAnimSel, PropsActivo->propBtnAnimSel->button);
}
// crea una VERTEX ANIMATION nueva en la malla: su frame 1 es la malla COMO ESTA
// (despues se agregan frames con Insertar keyframe / Auto Key en Edit Mode)
static void NuevaVertexAnimation(Mesh* m);
void _NuevaVertexAnimationFwd(Mesh* m){ NuevaVertexAnimation(m); }
static void NuevaVertexAnimation(Mesh* m){
    if (!m || !m->vertex || m->vertexSize <= 0) return;
    // nombre UNICO (antes "Anim.%03d" con size()+1 SIN chequear: borrar una del medio repetia)
    const std::string nom = m->NombreLibreVertexAnim("Anim", -1);
    // La animacion nace VACIA: es un CONTENEDOR (puede tener frames de vertices Y/O curvas
    // de transform). NO se crea un frame de vertices automatico -desperdicia memoria (una
    // copia entera de la malla) si el usuario nunca hace vertex anim-. Los frames se agregan
    // recien al hacer "i"/Auto Key; si la anim nunca los tuvo, no los tiene y listo (no se
    // guardan). Igual con las curvas: aparecen al keyframear un transform.
    VertexAnimation* anim = new VertexAnimation(m, nom);
    // capturar tambien las NORMALES en cada keyframe (si la malla las tiene): sin esto la
    // iluminacion de la pose deformada quedaba con las normales de reposo (se veia mal).
    anim->UseNormals = (m->normals != NULL);
    // UV: si la malla tiene coordenadas de textura, la anim PUEDE animarlas (UV morph),
    // pero en una curva SEPARADA de la de vertices: los keyframes de UV se insertan desde
    // el UV editor (VertexAnimInsertarKeyframeUV) y solo esos frames traen la capa uv.
    anim->UseUV = (m->uv != NULL);
    NewActiveVertexAnimation(m, anim);   // agrega a m->animations + controlador
    AnimSelVertex(m, (int)m->animations.size() - 1);
    Notificar(std::string("Animacion nueva: ") + nom, false);
}
// La ANIMACION DEL OBJETO activa (kind 3), CREANDOLA si no hay ninguna: mismo patron que
// InsertarKeyframeEsqueleto (que crea el clip si el armature no tiene). Insertar un keyframe
// no puede ser un ERROR por "no elegiste nada": si el objeto activo es una malla valida se le
// crea la animacion y se sigue. Solo se avisa cuando NO hay objeto o no es una malla.
// Devuelve la malla lista para keyframear, o NULL (ya notifico).
static Mesh* AnimObjetoActivaOCrear(const char* queFalla){
    if (ActiveAnimKind == 3 && ActiveAnimMesh) return ActiveAnimMesh;
    // sin animacion del objeto elegida: crearla sobre la malla activa (o sobre la que se edita)
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m && g_editMesh && g_editMesh->getType() == ObjectType::mesh) m = (Mesh*)g_editMesh;
    if (!m || !m->vertex || m->vertexSize <= 0){
        Notificar(std::string(queFalla) + ": " + T("select a mesh object first"), true);
        return NULL;
    }
    NuevaVertexAnimation(m);                       // nace vacia + queda ACTIVA en el timeline
    return (ActiveAnimKind == 3 && ActiveAnimMesh) ? ActiveAnimMesh : NULL;
}
// nucleo COMPARTIDO de los dos insert (capa VERTICES / capa UV): asegura la animacion DEL
// OBJETO activa (kind 3 + malla; la crea si no hay) y captura el keyframe en el playhead.
// capaUV=false guarda posiciones (+normales) - la curva "Vertices"; capaUV=true guarda SOLO
// las UV - la curva "UV". Son curvas SEPARADAS: insertar una NO pisa la otra en el mismo
// cuadro (VertexAnimSetKey conserva las capas que no vienen).
// OJO: cuando lo que se anima son VERTICES no hay canales Loc/Rot/Scl que
// elegir (una pose de vertices es UNA sola cosa) -> la tecla I inserta DIRECTO, sin menu
// desplegable. El menu de canales es solo para objetos y para poses de huesos (3D y 2D).
// capa: 0 = posiciones (+normales si la anim las usa), 1 = UV, 2 = SOLO normales.
// "Vertex animation", "normal animation" y "animar los UV" son LA MISMA animacion con capas
// distintas (VertexFrame tiene los tres punteros y el que no viene no se toca): por eso el menu
// deja elegir la capa en vez de inventar tres TIPOS de animacion.
static void VertexAnimInsertarKeyframeCapa(int capa){
    const bool capaUV = (capa == 1);
    Mesh* m = AnimObjetoActivaOCrear(T("Insert Keyframe"));
    if (!m) return;
    VertexAnimationActive* va = FindTargetAnim(m);
    int ai = va ? va->currentAnim : -1;
    if (ai < 0 || ai >= (int)m->animations.size() || !m->animations[ai]) return;
    VertexAnimation* an = m->animations[ai];
    if (!m->vertex || m->vertexSize <= 0) return;
    if (capaUV && !m->uv) { Notificar("Insertar keyframe UV: la malla no tiene UV", true); return; }
    an->target = m;
    // Ctrl+Z: snapshot ANTES de tocar los frames (KeyframesUndo captura el blob de la anim
    // activa). Cierra al final -> un paso de undo por keyframe insertado/reemplazado.
    extern void UndoKeyframesIniciar(); extern void UndoKeyframesConfirmar();
    UndoKeyframesIniciar();
    // captura la capa COMO ESTA como KEYFRAME en el cuadro del playhead. VertexAnimSetKey
    // inserta ORDENADO por frame o actualiza si ya hay uno ahi: se puede keyframear en 3,
    // luego 10, luego 5 (queda 3,5,10) sin problemas.
    int interp = KfLinear; // nuevo keyframe: recta hacia el proximo (como la anim vieja)
    // si ya hay un keyframe en ese cuadro, conservar SU interpolacion al reemplazar
    for (size_t k = 0; k < an->frames.size(); ++k)
        if (an->frames[k]->frame == CurrentFrame) { interp = an->frames[k]->interp; break; }
    if (capa == 1)      VertexAnimSetKey(*an, CurrentFrame, NULL, NULL, m->uv, interp);
    else if (capa == 2){ if (!m->normals){ Notificar(T("Insert Keyframe: the mesh has no normals"), true); UndoKeyframesConfirmar(); return; }
                        an->UseNormals = true;   // pedirla explicitamente la vuelve a habilitar
                        VertexAnimSetKey(*an, CurrentFrame, NULL, m->normals, NULL, interp); }
    else                VertexAnimSetKey(*an, CurrentFrame, m->vertex, an->UseNormals ? m->normals : NULL, NULL, interp);
    // NO pisar el End con el numero de frames: el End es el rango que configuro el usuario.
    // Solo se EXTIENDE si el keyframe cae mas alla del final.
    if (CurrentFrame > EndFrame) { EndFrame = CurrentFrame; an->endFrame = EndFrame; }
    UndoKeyframesConfirmar();
    m->skinGeomVersion++;
    // SIN notificacion: con Auto Key, mover un vertice inserta un keyframe por cada
    // confirmacion -> llenaba la pantalla de avisos "Keyframe de vertices" (molesto).
    g_redraw = true;
}
// captura mesh->vertex (posiciones + normales) en la curva VERTICES: el camino del 3D
// (menu Object/Mesh en Edit Mode, auto-key del transform de vertices, la 'i')
void VertexAnimInsertarKeyframe(){ VertexAnimInsertarKeyframeCapa(0); }
// captura mesh->uv en la curva UV: el camino del UV EDITOR (menu Animation, auto-key
// del mapeo y de la pose 2D). "Son cosas distintas": cada editor keyframea SU curva.
void VertexAnimInsertarKeyframeUV(){ VertexAnimInsertarKeyframeCapa(1); }
// captura SOLO mesh->normals en la curva NORMALES ("normal animation"): la tercera capa, elegible
// desde el submenu "Insert Keyframe Layer" del menu Animation.
void VertexAnimInsertarKeyframeNormales(){ VertexAnimInsertarKeyframeCapa(2); }
// ---- BORRAR LA CAPA DE NORMALES de la vertex anim activa (menu Animation) ----
// Un modelo SIN ILUMINACION (estilo plataformas de PS1) no necesita normales animadas: son 3 bytes por vertice por
// keyframe de memoria y una interpolacion entera por cuadro reproducido. Al borrarlas, EvalVertexAnim ya no
// encuentra ningun frame con capa de normales y se saltea esa pasada por completo.
// FALLBACK del render: la malla se queda con SU array de normales (el estatico de siempre, mesh->normals). Como
// lo ultimo que quedo escrito ahi es la interpolacion del cuadro en el que estabas, se RECALCULAN desde la
// geometria actual para que sean coherentes y no una pose congelada a medias.
// OJO (asimetria a proposito): insertar keyframes de vertices captura normales solo si an->UseNormals, que esto
// apaga -> despues de borrar la capa, los keyframes nuevos ya no la vuelven a crear (que es justamente lo que se
// pidio: ahorrar procesamiento). Para volver atras, Ctrl+Z.
void VertexAnimBorrarCapaNormalesActiva(){
    // aca NO se auto-crea nada: borrar una capa de una animacion que no existe no tiene sentido.
    if (ActiveAnimKind != 3 || !ActiveAnimMesh){ Notificar(T("Delete normals layer: select an object animation in the timeline"), true); return; }
    Mesh* m = ActiveAnimMesh;
    VertexAnimationActive* va = FindTargetAnim(m);
    int ai = va ? va->currentAnim : -1;
    if (ai < 0 || ai >= (int)m->animations.size() || !m->animations[ai]) return;
    VertexAnimation* an = m->animations[ai];
    extern void UndoKeyframesIniciar(); extern void UndoKeyframesConfirmar();
    UndoKeyframesIniciar();                       // Ctrl+Z: un paso de undo por operacion
    bool algo = VertexAnimBorrarCapaNormales(*an);
    UndoKeyframesConfirmar();
    if (!algo){ Notificar(T("This animation has no normals layer"), true); return; }
    if (m->normals) m->RecalcularNormales();      // normales ESTATICAS coherentes con la pose actual
    m->skinGeomVersion++;                         // re-subir el VBO
    Notificar(T("Normals layer deleted"), false);
    g_redraw = true;
}
// ---- tarjeta "Animacion" del OBJETO (pestania Objeto): las animaciones DEL objeto.
// Una animacion de objeto es un CONTENEDOR: hoy envuelve una vertex animation
// (mallas); a futuro suma canales de transform (mover/rotar/escalar), visible
// (curva 0/1) y mas parametros. Los clips de ARMATURE no van aca (son del hueso).
static void SincronizarAnimVertexDesdeLista(Mesh* m, int idx){
    AnimSelVertex(m, idx);
    PropertiesLayoutDirty = true; g_redraw = true;
}
static int ActivaAnimVertexDeMesh(Mesh* m){
    if (ActiveAnimKind != 3 || ActiveAnimMesh != m) return -1;
    VertexAnimationActive* va = FindTargetAnim(m);
    return va ? va->currentAnim : -1;
}
static void AccionObjAnimNew(){
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){
        extern void _NuevaVertexAnimationFwd(Mesh*);   // definida abajo (orden de declaracion)
        _NuevaVertexAnimationFwd((Mesh*)ObjActivo);
        PropertiesLayoutDirty = true; g_redraw = true;
        return;
    }
    // (el "(transform/visible: proximamente)" era mentira: las curvas de transform PROPIAS del
    // objeto ya existen -VertexAnimation::curvas, las usa CurvasActivas- desde hace rato)
    Notificar(T("Object animation: meshes only for now"), true);
}
// borra la vertex anim ACTIVA de 'm' (la usan el boton de la tarjeta del objeto
// y el Delete de la tarjeta Animation cuando lo activo es una vertex anim)
static void BorrarVertexAnimDe(Mesh* m);
// el harness de tests aprieta el MISMO boton (mismo idiom que _NuevaVertexAnimationFwd)
void _BorrarVertexAnimDeFwd(Mesh* m){ BorrarVertexAnimDe(m); }
static void BorrarVertexAnimDe(Mesh* m){
    if (!m) return;
    VertexAnimationActive* va = FindTargetAnim(m);
    int i = va ? va->currentAnim : (m->animations.empty() ? -1 : 0);
    if (i < 0 || i >= (int)m->animations.size()) return;
    std::string nom = m->animations[i] ? m->animations[i]->name : "anim";
    // ANTES de borrar: si la malla esta POSADA por la anim, vertex[] tiene la POSE y el reposo
    // vive en el keyframe base. Al borrar la anim se va la definicion de reposo, asi que esa pose
    // pasaria a ser LA GEOMETRIA del modelo (irreversible: el "-" no empuja paso de undo) y las
    // claves por posicion de sharp/seam quedarian huerfanas -> la primera edicion las poda todas
    // en silencio, y guardar antes de eso escribe la malla deformada y sin marcas.
    // Devolver el reposo ANTES del erase deja la malla como el usuario la modelo. Ver el bloque
    // de PodarMarcasSinArista en Mesh.h (invariante de POSE).
    if (m->PosadaPorVertexAnim()) {
        const GLfloat* rep = m->PosicionesReposo();
        if (rep && rep != m->vertex && m->vertex && m->vertexSize > 0) {
            memcpy(m->vertex, rep, (size_t)m->vertexSize * 3 * sizeof(GLfloat));
            m->posadaPorAnim = false;
            m->skinGeomVersion++;
        }
    }
    // borrar CORRE los indices de la lista y el "-" de las vertex anims no empuja ningun paso
    // de undo: sin este aviso, el Ctrl+Z de un rename anterior escribia el nombre viejo encima
    // de la anim de al lado (los destinos van por (malla, lista, INDICE)). Ver Undo.h.
    UndoListaBorrada(W3dDestCapaMalla(m, W3dRenameDest::VertAnim, i));
    delete m->animations[i];                       // libera los frames
    m->animations.erase(m->animations.begin() + i);
    // ...y las referencias INTERNAS de la lista a si misma: 'proximaAnimacion' es un INDICE a
    // Mesh::animations (la cadena "cuando termino esta, segui con aquella"), se GUARDA en el
    // .w3d ("proxima") y lo usa el runtime (VertexAnimation.cpp). Este erase CORRE los indices:
    // sin remapear, toda cadena que apuntaba mas arriba que 'i' pasaba a encadenar con la
    // animacion DE AL LADO, en silencio, y eso se guardaba. Es el mismo aviso que ya se le da al
    // undo y al dope, pero hacia adentro de la propia lista. El que apuntaba a la BORRADA queda
    // en -1 ("sin proxima"), el no-op seguro de siempre: inventarle otro destino seria encadenar
    // con una animacion que el usuario nunca eligio.
    for (size_t k = 0; k < m->animations.size(); k++) {
        VertexAnimation* a = m->animations[k]; if (!a) continue;
        if (a->proximaAnimacion == i)     a->proximaAnimacion = -1;
        else if (a->proximaAnimacion > i) a->proximaAnimacion--;
    }
    if (va){ if (va->currentAnim >= (int)m->animations.size()) va->currentAnim = (int)m->animations.size() - 1;
             va->nextAnim = va->currentAnim; va->currentFrame = va->nextFrame = 0; va->blendStep = 0; va->playFrame = 0.0f; }
    if (ActiveAnimKind == 3 && ActiveAnimMesh == m && m->animations.empty()){
        ActiveAnimKind = 0; ActiveAnimMesh = NULL;   // sin anims: volver a la escena
    }
    // la ACTIVA pasa a ser otra (o ninguna) y las claves del dope "objvtx:/objuv:/objnrm:" no
    // dicen de cual son -> se sueltan, o la 'X' del dope borraria poses de la anim de al lado
    DopeSoltarVertexAnim();
    Notificar("Animacion borrada: " + nom, false);
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionObjAnimDel(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    BorrarVertexAnimDe((Mesh*)ObjActivo);
}
static void AccionAnimNewCard(){
    // clip del ARMATURE 2D activo: New crea OTRO clip del mismo rig 2D (no una vertex anim)
    if (ActiveAnimKind == 4 && ActiveAnimMesh && !ActiveAnimMesh->Arm2DHuesos().empty()){
        Mesh* m = ActiveAnimMesh;
        Arm2DCrearAnimacion(m); AnimSelArm2D(m, m->Arm2DAnimActiva());
        PropertiesLayoutDirty = true; g_redraw = true;
        return;
    }
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh && ActiveAnimKind != 1){
        // con una MALLA seleccionada: New crea una VERTEX animation de esa malla
        NuevaVertexAnimation((Mesh*)ObjActivo);
        PropertiesLayoutDirty = true; g_redraw = true;
        return;
    }
    if (ActiveAnimKind == 1 && ActiveAnimArm){ CrearAnimacion(ActiveAnimArm); InvalidarSkinEscena(); } // nuevo clip (arranca en pose reset)
    else { NuevaEscena(); ActiveAnimKind = 0; }                             // nueva animacion de ESCENA
    AnimCargarRangoActivo(); // la animacion nueva arranca con su rango/fps por defecto
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionAnimDupCard(){ // Duplicate: copia el clip activo del armature (solo si hay clip)
    if (ActiveAnimKind == 4 && ActiveAnimMesh && ActiveAnimMesh->Arm2DAnimActiva() >= 0){
        Mesh* m = ActiveAnimMesh;
        Arm2DDuplicarAnimacionActiva(m); AnimSelArm2D(m, m->Arm2DAnimActiva());
        PropertiesLayoutDirty = true; g_redraw = true;
        return;
    }
    if (ActiveAnimKind == 1 && ActiveAnimArm && ActiveAnimArm->animActiva >= 0){
        DuplicarAnimacionActiva(ActiveAnimArm); AnimCargarRangoActivo();
    }
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionAnimDelCard(){
    // vertex anim activa (kind 3): Delete borra ESA anim, como ya hacen New y
    // Rename con su rama kind 3. Antes caia a BorrarEscenaActiva y volaba una
    // animacion de ESCENA que no tenia nada que ver.
    if (ActiveAnimKind == 4 && ActiveAnimMesh){
        // clip del armature 2D: borra ESE clip (sin esto caia a BorrarEscenaActiva, como pasaba
        // antes con las vertex anims: volaba una animacion de escena que no tenia nada que ver)
        Mesh* m = ActiveAnimMesh;
        UndoBorrarClip2D(m);   // = Arm2DBorrarAnimacionActiva + el remapeo de los indices (Undo.h)
        if (m->Arm2DAnims().empty()){ ActiveAnimKind = 0; ActiveAnimMesh = NULL; }
    }
    else if (ActiveAnimKind == 3 && ActiveAnimMesh){
        BorrarVertexAnimDe(ActiveAnimMesh);
    }
    else if (ActiveAnimKind == 1 && ActiveAnimArm){
        UndoBorrarClipArm(ActiveAnimArm);   // = BorrarAnimacionActiva + el remapeo de los indices (Undo.h)
        if (ActiveAnimArm->animations.empty()){ ActiveAnimKind = 0; ActiveAnimArm = NULL; } // sin clips -> volver a escena
    } else UndoBorrarEscenaActiva();   // = BorrarEscenaActiva + el remapeo de los indices (Undo.h)
    AnimCargarRangoActivo(); // cargar el rango/fps de la animacion que quedo activa
    PropertiesLayoutDirty = true; g_redraw = true;
}
// el harness de tests aprieta el MISMO boton "-" (mismo idiom que _BorrarVertexAnimDeFwd)
void _AnimDelCardFwd(){ AccionAnimDelCard(); }
static void AccionAnimRenameCard(){                                          // renombra la animacion activa in-place
    if (!PropsActivo || !PropsActivo->propBtnAnimRename) return;
    // TODOS los clips viven en un vector<T*> y se BORRAN con el "-" de esta misma tarjeta
    // (AccionAnimDelCard, aca arriba): el destino del undo va por (dueno, indice), nunca por
    // puntero, o el Ctrl+Z siguiente escribia en el clip liberado. Ver W3dRenameDest en Undo.h.
    if (ActiveAnimKind == 4 && ActiveAnimMesh){
        Armature2D* arm = ActiveAnimMesh->Arm2DActivoP();
        Armature2DAnimation* c2 = Arm2DClipActivo(ActiveAnimMesh);
        if (c2 && arm){
            const W3dRenameDest dest = W3dDestClip2D(ActiveAnimMesh, arm, arm->animActiva);
            RenameIniciar(PropsActivo->propBtnAnimRename->button, &c2->name, UniqAnim2D, NULL, &dest); return;
        }
    }
    if (ActiveAnimKind == 3 && ActiveAnimMesh) {
        VertexAnimationActive* va = FindTargetAnim(ActiveAnimMesh);
        int i = va ? va->currentAnim : -1;
        if (i >= 0 && i < (int)ActiveAnimMesh->animations.size() && ActiveAnimMesh->animations[i]) {
            const W3dRenameDest dest = W3dDestCapaMalla(ActiveAnimMesh, W3dRenameDest::VertAnim, i);
            RenameIniciar(PropsActivo->propBtnAnimRename->button, &ActiveAnimMesh->animations[i]->name,
                          UniqVertexAnim, NULL, &dest);
            return;
        }
    }
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size()) {
        const W3dRenameDest dest = W3dDestClipArm(ActiveAnimArm, ActiveAnimArm->animActiva);
        RenameIniciar(PropsActivo->propBtnAnimRename->button, &ActiveAnimArm->animations[ActiveAnimArm->animActiva]->name,
                      UniqAnim, NULL, &dest);
    }
    else { InitSceneAnimations();
           const W3dRenameDest dest = W3dDestGlobal(W3dRenameDest::SceneAnimG, SceneAnimActiva);
           RenameIniciar(PropsActivo->propBtnAnimRename->button, &SceneAnimations[SceneAnimActiva]->name,
                         UniqSceneAnim, NULL, &dest); }
}
// ARMATURE / Pose Mode: transform del HUESO activo. Los campos PropFloat escriben en estos mirrors; el callback los
// vuelca a restT/restR/restS del hueso (la pose de rest, que es la que se ve cuando el clip activo no anima ese hueso).
static float g_bonePosX=0, g_bonePosY=0, g_bonePosZ=0;
static float g_boneRotX=0, g_boneRotY=0, g_boneRotZ=0;
static float g_boneSclX=1, g_boneSclY=1, g_boneSclZ=1;
static W3dBone* BoneActivoUI(){
    Armature* a = ArmActiva();
    if (!a || a->boneActivo < 0 || a->boneActivo >= (int)a->bones.size()) return NULL;
    return &a->bones[a->boneActivo];
}
// ===== NOMBRE del hueso activo (tarjeta Bones): fila "Name" con el cuadro editable a la derecha
// (mismo patron inline que el Name del objeto). Al PERDER el foco commitea: BoneRenombrar uniquifica,
// renombra el VERTEX GROUP asociado en las mallas del rig (binding POR NOMBRE) y empuja su undo. =====
static Armature* g_boneNombreArm = NULL;   // capturados al ENFOCAR (si el hueso activo cambia mientras
static int       g_boneNombreIdx = -1;     // se tipea, el commit va al hueso que se estaba editando)
static void SincronizarNombreBone(Properties* p){
    if (!p || !p->propBoneNombre) return;
    Armature* a = ArmActiva();
    W3dBone* b = BoneActivoUI();
    PropText* pt = p->propBoneNombre;
    bool foco = (g_textFieldActivo == &pt->field);
    if (foco && g_boneNombreIdx < 0 && a && b){ g_boneNombreArm = a; g_boneNombreIdx = a->boneActivo; }
    if (!foco && g_boneNombreIdx >= 0){        // termino de editar -> commit (hueso + vertex group)
        Armature* ea = g_boneNombreArm; int ei = g_boneNombreIdx;
        g_boneNombreArm = NULL; g_boneNombreIdx = -1;
        if (ea && ei >= 0 && ei < (int)ea->bones.size() && !pt->field.text.empty())
            BoneRenombrar(ea, ei, pt->field.text);   // uniquifica + vgroups + undo + notificacion
    }
    // sync display cuando NO se edita (solo si cambio, para no redibujar infinito)
    const std::string mostrar = b ? b->name : std::string("");
    if (!foco && pt->field.text != mostrar){ pt->field.SetText(mostrar); g_redraw = true; }
    pt->oculto = (b == NULL);
}
// ===== PADRE del hueso activo (tarjeta Bones): desplegable "None" + los demas huesos del armature,
// excluyendo al propio y a sus DESCENDIENTES (no se pueden armar ciclos). Elegir re-parenta con undo. =====
static PopupMenu* MenuBoneParent = NULL;
static void AccionBoneParentElegido(int id){
    Armature* a = ArmActiva();
    if (!a || a->boneActivo < 0 || a->boneActivo >= (int)a->bones.size()) return;
    BoneReparentar(a, a->boneActivo, id - 1); // id 0 = None (-1); 1+i = hueso i. Valida ciclos adentro.
}
static void AccionMenuBoneParent(){
    Armature* a = ArmActiva();
    W3dBone* b = BoneActivoUI();
    if (!a || !b || !PropsActivo || !PropsActivo->propBoneParent) return;
    if (!MenuBoneParent){ MenuBoneParent = new PopupMenu(); MenuBoneParent->action = AccionBoneParentElegido; }
    MenuBoneParent->Limpiar();
    MenuBoneParent->titulo = T("Parent");
    MenuBoneParent->Agregar(T("None"), 0);
    for (size_t i = 0; i < a->bones.size(); i++){
        if (BoneEsDescendiente(a, a->boneActivo, (int)i)) continue; // el propio hueso y sus descendientes
        MenuBoneParent->Agregar(a->bones[i].name, (int)i + 1);
    }
    AbrirMenuBajoBoton(MenuBoneParent, PropsActivo->propBoneParent->button);
}
// ===== checkbox "Connected" del hueso activo (tarjeta Bones): es el MISMO flag 'conectado' que
// operan el Ctrl+P > Connected y el Alt+P > Disconnect Bone -> un solo camino (BoneSetConectado,
// con undo). TILDADO = head soldado al tail del padre (mover uno mueve al otro); tildarlo estando
// separado MUEVE el hueso para pegarlo. DESTILDADO = emparentado pero suelto (linea punteada).
// El mirror lo refresca ActualizarPestanias por frame; sin padre la fila se oculta. =====
static bool g_boneConectado = false;
static void AccionBoneConectado(){
    Armature* a = ArmActiva();
    if (!a || a->boneActivo < 0 || a->boneActivo >= (int)a->bones.size()) return;
    if (!BoneSetConectado(a, a->boneActivo, g_boneConectado))
        g_boneConectado = BoneEsConectado(a, a->boneActivo); // no se pudo (sin padre): revertir el tilde
}
// marca la pose como editada (para re-FK sin refrescar desde la curva) e invalida el skin de las mallas que usan
// este armature (sino, al posar en el MISMO frame, SkinearMesh cortaria por lastSkinFrame y no se veria el cambio).
static void InvalidarPoseYSkin(Armature* a){
    if (!a) return;
    a->poseDirty = true;
    extern Object* SceneCollection; // ojo: es global del core
    struct L { static void rec(Object* o, Armature* arm){ if (!o) return;
        if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (m->skinArmature==arm) m->lastSkinFrame=-999999; }
        for (size_t i=0;i<o->Childrens.size();i++) rec(o->Childrens[i], arm); } };
    L::rec(SceneCollection, a);
}
static void AccionBoneTransform(){
    W3dBone* b = BoneActivoUI(); if (!b) return;
    // se edita la POSE (no el rest): se ve al toque pero NO se guarda en la animacion hasta "Insert Keyframe".
    b->poseT = Vector3(g_bonePosX, g_bonePosY, g_bonePosZ);
    b->poseR = Vector3(g_boneRotX, g_boneRotY, g_boneRotZ);
    b->poseS = Vector3(g_boneSclX, g_boneSclY, g_boneSclZ);
    InvalidarPoseYSkin(ArmActiva());
    g_redraw = true;
}
// mirrors <- hueso activo (Rebind / cambio de seleccion): los campos muestran la POSE actual del hueso elegido.
static void SincronizarCamposBone(){
    W3dBone* b = BoneActivoUI();
    if (!b) { g_bonePosX=g_bonePosY=g_bonePosZ=0; g_boneRotX=g_boneRotY=g_boneRotZ=0; g_boneSclX=g_boneSclY=g_boneSclZ=1; return; }
    g_bonePosX=b->poseT.x; g_bonePosY=b->poseT.y; g_bonePosZ=b->poseT.z;
    g_boneRotX=b->poseR.x; g_boneRotY=b->poseR.y; g_boneRotZ=b->poseR.z;
    g_boneSclX=b->poseS.x; g_boneSclY=b->poseS.y; g_boneSclZ=b->poseS.z;
}
// ============================================================================
//  Tarjeta "ARMATURE 2D" (rigs 2D del MESH, Mesh::armatures2d). Visible mientras algun editor UV
//  esta en Edit Bones / Pose / MODO OBJETO (UVEditorConRig2D) con la malla activa en Edit Mode.
//  Las filas de HUESO (lista de huesos, Name, Parent, Connected, Pos/Rot/Scale) SOLO aparecen en
//  Edit Bones/Pose (Bones2DMeshUI); en modo objeto queda la lista de ARMATURES con Add/Rename/
//  Delete, que es lo que ese modo necesita (elegir/administrar rigs).
//  - Lista de huesos (PropListMeshParts modo 8) con el activo.
//  - Name inline: renombra el hueso Y su vertex group homonimo (Bone2DRenombrar, como el 3D).
//  - Parent desplegable (None + huesos sin ciclos) -> Bone2DReparentar (keep offset).
//  - Pos X/Y de LO SELECCIONADO del hueso activo: hueso entero = su CENTRO (editar traslada
//    head+tail rigido, como la tarjeta Transform Mesh); solo head o solo tail = esa punta (la
//    punta NO tiene rotacion). En POSE: Pos = traslacion 2D + Rotation/Scale X/Y de la pose.
// ============================================================================
static float g_b2dPosX = 0, g_b2dPosY = 0, g_b2dRot = 0, g_b2dSclX = 1, g_b2dSclY = 1;
static bool  g_b2dConectado = false;   // mirror del checkbox "Connected" del hueso 2D activo
static PropFloat *gB2dPosX = NULL, *gB2dPosY = NULL, *gB2dRot = NULL, *gB2dSclX = NULL, *gB2dSclY = NULL;
// el mesh cuya tarjeta Armature 2D esta activa (NULL = la tarjeta no aplica)
static Mesh* Bones2DMeshUI() {
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return NULL;
    Mesh* m = (Mesh*)ObjActivo;
    // basta con que la malla TENGA algun armature 2D: uno recien creado (o vaciado) sigue
    // mereciendo la tarjeta, que es donde se lo renombra/borra y se agregan otros.
    if ((Object*)m != g_editMesh || !m->TieneArm2D()) return NULL;
    return UVEditorEnModoHuesos() ? m : NULL;
}
static W3dBone2D* Bone2DActivoUI(Mesh* m) {
    if (!m || m->Arm2DBoneActivo() < 0 || m->Arm2DBoneActivo() >= (int)m->Arm2DHuesos().size()) return NULL;
    return &m->Arm2DHuesos()[m->Arm2DBoneActivo()];
}
// el editor UV en modo huesos esta POSANDO? (decide que muestran los campos de la tarjeta)
static bool Bones2DEnPoseUI() {
    UVEditor* uv = UVEditorEnModoHuesos();
    return uv && uv->uvModo == UVModoPose;
}
// punto que MUESTRA la fila Pos en modo huesos (rest): entero -> centro; solo head/tail -> esa punta
static void Bone2DPuntoUI(const W3dBone2D* b, float& u, float& v) {
    if (b->selHead && !b->selTail)      { u = b->headU; v = b->headV; }
    else if (b->selTail && !b->selHead) { u = b->tailU; v = b->tailV; }
    else { u = (b->headU + b->tailU) * 0.5f; v = (b->headV + b->tailV) * 0.5f; }
}
static void AccionBone2DPos() { // campo Pos X/Y editado
    Mesh* m = Bones2DMeshUI(); if (!m) return;
    W3dBone2D* b = Bone2DActivoUI(m); if (!b) return;
    if (Bones2DEnPoseUI()) {
        // POSE: los campos SON la traslacion 2D del hueso activo
        if (b->poseTU == g_b2dPosX && b->poseTV == g_b2dPosY) return;
        UndoBones2DCapturar(m);
        b->poseTU = g_b2dPosX; b->poseTV = g_b2dPosY;
        m->Armature2DRestCapturar(); m->Armature2DAplicar(); // los UV pesados siguen al hueso
        m->skinGeomVersion++;
        g_redraw = true;
        return;
    }
    // HUESOS (rest): delta contra el punto mostrado, aplicado a la SELECCION (con soldadura)
    float cu, cv; Bone2DPuntoUI(b, cu, cv);
    Bone2DMoverSeleccion(m, g_b2dPosX - cu, g_b2dPosY - cv); // undo adentro (no-op si delta 0)
}
static void AccionBone2DPose() { // Rotation / Scale X/Y (solo en POSE, hueso entero)
    Mesh* m = Bones2DMeshUI(); if (!m || !Bones2DEnPoseUI()) return;
    W3dBone2D* b = Bone2DActivoUI(m); if (!b) return;
    if (b->poseRot == g_b2dRot && b->poseSX == g_b2dSclX && b->poseSY == g_b2dSclY) return;
    UndoBones2DCapturar(m);
    b->poseRot = g_b2dRot; b->poseSX = g_b2dSclX; b->poseSY = g_b2dSclY;
    m->Armature2DRestCapturar(); m->Armature2DAplicar();
    m->skinGeomVersion++;
    g_redraw = true;
}
// Name inline del hueso 2D activo: MISMO patron que SincronizarNombreBone (captura al enfocar,
// commit al perder el foco -> Bone2DRenombrar uniquifica + renombra el vertex group + undo).
static Mesh* g_b2dNombreMesh = NULL;
static int   g_b2dNombreIdx  = -1;
static void SincronizarNombreBone2D(Properties* p) {
    if (!p || !p->propBone2DNombre) return;
    Mesh* m = Bones2DMeshUI();
    W3dBone2D* b = Bone2DActivoUI(m);
    PropText* pt = p->propBone2DNombre;
    bool foco = (g_textFieldActivo == &pt->field);
    if (foco && g_b2dNombreIdx < 0 && m && b) { g_b2dNombreMesh = m; g_b2dNombreIdx = m->Arm2DBoneActivo(); }
    if (!foco && g_b2dNombreIdx >= 0) {        // termino de editar -> commit (hueso + vertex group)
        Mesh* em = g_b2dNombreMesh; int ei = g_b2dNombreIdx;
        g_b2dNombreMesh = NULL; g_b2dNombreIdx = -1;
        if (em && ei >= 0 && ei < (int)em->Arm2DHuesos().size() && !pt->field.text.empty())
            Bone2DRenombrar(em, ei, pt->field.text);
    }
    const std::string mostrar = b ? b->nombre : std::string("");
    if (!foco && pt->field.text != mostrar) { pt->field.SetText(mostrar); g_redraw = true; }
    pt->oculto = (b == NULL);
}
// desplegable "Parent" del hueso 2D activo: None + los demas huesos (sin el propio ni sus
// descendientes -> sin ciclos). Elegir re-parenta con undo (keep offset).
static PopupMenu* MenuBone2DParent = NULL;
static void AccionBone2DParentElegido(int id) {
    Mesh* m = Bones2DMeshUI(); if (!m) return;
    if (m->Arm2DBoneActivo() < 0 || m->Arm2DBoneActivo() >= (int)m->Arm2DHuesos().size()) return;
    Bone2DReparentar(m, m->Arm2DBoneActivo(), id - 1); // id 0 = None (-1); 1+i = hueso i
}
static void AccionMenuBone2DParent() {
    Mesh* m = Bones2DMeshUI();
    W3dBone2D* b = Bone2DActivoUI(m);
    if (!m || !b || !PropsActivo || !PropsActivo->propBone2DParent) return;
    if (!MenuBone2DParent) { MenuBone2DParent = new PopupMenu(); MenuBone2DParent->action = AccionBone2DParentElegido; }
    MenuBone2DParent->Limpiar();
    MenuBone2DParent->titulo = T("Parent");
    MenuBone2DParent->Agregar(T("None"), 0);
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++) {
        if (Bone2DEsDescendiente(m, m->Arm2DBoneActivo(), (int)i)) continue; // el propio y sus descendientes
        MenuBone2DParent->Agregar(m->Arm2DHuesos()[i].nombre, (int)i + 1);
    }
    AbrirMenuBajoBoton(MenuBone2DParent, PropsActivo->propBone2DParent->button);
}
// checkbox "Connected" del hueso 2D activo: MISMO flag y MISMO camino que el Ctrl+P > Connected
// y el Alt+P > Disconnect Bone (Bone2DSetConectado, con undo). Tildarlo con el head separado
// MUEVE el hueso para pegarlo al tail del padre (y activa la soldadura al mover); destildarlo la
// apaga sin mover nada.
static void AccionBone2DConectado() {
    Mesh* m = Bones2DMeshUI(); if (!m) return;
    if (m->Arm2DBoneActivo() < 0 || m->Arm2DBoneActivo() >= (int)m->Arm2DHuesos().size()) return;
    if (!Bone2DSetConectado(m, m->Arm2DBoneActivo(), g_b2dConectado))
        g_b2dConectado = Bone2DEsConectado(m, m->Arm2DBoneActivo()); // no aplico (sin padre): revertir
}

// ---- lista de ARMATURES 2D de la malla (Add / Rename / Delete). Opera Mesh::armatures2d: cada
// armature es un rig 2D independiente (sus huesos, sus clips, su hueso/clip activo). ----
// OJO: aca NO se usa Bones2DMeshUI (pide hueso activo); alcanza con la malla en Edit Mode y un
// editor UV en modo huesos, que es cuando la tarjeta se ve.
static Mesh* Arm2DListaMeshUI() {
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return NULL;
    Mesh* m = (Mesh*)ObjActivo;
    if ((Object*)m != g_editMesh) return NULL;
    return UVEditorConRig2D() ? m : NULL;   // Edit Bones, Pose Y MODO OBJETO (el default)
}
// mesh de la TARJETA entera (pestania 6): la lista de rigs + (solo en huesos/pose) las filas de hueso
static Mesh* Arm2DTarjetaMeshUI() {
    Mesh* m = Arm2DListaMeshUI();
    return (m && m->TieneArm2D()) ? m : NULL; // sin ningun rig 2D no hay tarjeta (se crea con Add > Armature 2D)
}
static void AccionArm2DAdd() {
    Mesh* m = Arm2DListaMeshUI(); if (!m) return;
    UVEditor* uv = UVEditorConRig2D();
    if (uv) uv->Armature2DNuevo(m);          // armature nuevo con 1 hueso, activo y en Edit Bones
    else    m->Arm2DAgregar("Armature 2D");
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionArm2DDel() {
    Mesh* m = Arm2DListaMeshUI(); if (!m) return;
    if (m->armature2dActivo < 0) return;
    // UndoArm2DBorrar hace el borrado Y el paso de undo (se queda con el Armature2D vivo, asi
    // Ctrl+Z devuelve el MISMO puntero y los Bones2DUndo previos de ese rig siguen sirviendo);
    // ademas re-aplica el skinning. Ver Undo.h.
    UndoArm2DBorrar(m, m->armature2dActivo);
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionRenameArm2D() {
    Mesh* m = Arm2DListaMeshUI();
    if (!m || !PropsActivo || !PropsActivo->propBtnRenameArm2D) return;
    if (m->armature2dActivo < 0 || m->armature2dActivo >= (int)m->armatures2d.size()) return;
    const W3dRenameDest dest = W3dDestCapaMalla(m, W3dRenameDest::Arm2D, m->armature2dActivo);
    RenameIniciar(PropsActivo->propBtnRenameArm2D->button,
                  &m->armatures2d[m->armature2dActivo]->nombre, UniqArm2D, NULL, &dest);
}

static void AccionVertColorMode() {   // toggle Per-Vertex / Per-Corner de la capa de color activa
    Mesh* m = VerticesMesh(); if (!m) return;
    if (m->colorActivo >= 0 && m->colorActivo < (int)m->colorLayers.size()) {
        ColorLayer* cl = m->colorLayers[m->colorActivo];
        cl->porVertice = !cl->porVertice;
        m->AplicarCapasAlRender(); g_redraw = true;
    }
}


// ============================================================================
//  Tarjeta "Keyframe": edita con EXACTITUD el keyframe elegido en el editor de curvas (el ultimo clickeado).
//  Los campos son espejos float; cada onChange escribe en la curva VIVA y re-evalua la animacion.
//  Se resuelve el keyframe en cada acceso (DopeKeyframeActivo): el vector se reordena al moverlo.
// ============================================================================
static float g_kfFrame = 0.0f, g_kfValor = 0.0f;
static float g_kfInDF = 0.0f, g_kfInDV = 0.0f, g_kfOutDF = 0.0f, g_kfOutDV = 0.0f;
static PropFloat *gKfFrame=NULL, *gKfValor=NULL, *gKfInDF=NULL, *gKfInDV=NULL, *gKfOutDF=NULL, *gKfOutDV=NULL;
static PropButton *gKfInterp=NULL, *gKfHandle=NULL;
static PopupMenu* g_menuKfInterp = NULL;
static PopupMenu* g_menuKfHandle = NULL;

static const char* KfNombreInterp(int i){
    return (i==KfConstant)?"Constant" : (i==KfLinear)?"Linear" : "Bezier";
}
static const char* KfNombreHandle(int h){
    switch (h){ case HFree: return "Free"; case HAligned: return "Aligned"; case HVector: return "Vector";
                case HAuto: return "Automatic"; default: return "Auto Clamped"; }
}
static void KfAplicado(){ AplicarAnimacionObjetos(); InvalidarAnimYRedraw(); }

static void AccionKfFrame(){
    int i; AnimProperty* ap = DopeKeyframeActivo(&i); if (!ap) return;
    int nf = (int)floorf(g_kfFrame + 0.5f);
    if (nf == ap->keyframes[i].frame) return;
    UndoKeyframesIniciar();
    // el frame de destino puede estar ocupado: gana el que se mueve (misma regla que arrastrarlo en el timeline)
    for (size_t k=ap->keyframes.size(); k-- > 0; )
        if ((int)k != i && ap->keyframes[k].frame == nf) ap->keyframes.erase(ap->keyframes.begin()+k);
    int j; ap = DopeKeyframeActivo(&j); if (!ap) return;   // el erase pudo correr el indice
    ap->keyframes[j].frame = nf;
    ap->SortKeyFrames();
    DopeKeyframeActivoReFrame(nf);
    UndoKeyframesConfirmar(); KfAplicado();
}
static void AccionKfValor(){
    int i; AnimProperty* ap = DopeKeyframeActivo(&i); if (!ap) return;
    if (ap->keyframes[i].value == g_kfValor) return;
    UndoKeyframesIniciar(); ap->keyframes[i].value = g_kfValor; UndoKeyframesConfirmar(); KfAplicado();
}
static void AccionKfHandles(){
    int i; AnimProperty* ap = DopeKeyframeActivo(&i); if (!ap) return;
    keyFrame& k = ap->keyframes[i];
    // tocar un handle a mano solo tiene sentido si el tipo es de los que se guardan
    if (k.handleType != HFree && k.handleType != HAligned) return;
    UndoKeyframesIniciar();
    k.inDF = g_kfInDF; k.inDV = g_kfInDV; k.outDF = g_kfOutDF; k.outDV = g_kfOutDV;
    UndoKeyframesConfirmar(); KfAplicado();
}
static void AccionMenuKfInterp(int id){
    if (id >= 0){ int i; AnimProperty* ap = DopeKeyframeActivo(&i);
        if (ap){ UndoKeyframesIniciar(); ap->keyframes[i].Interpolation = id;
                 if (id == KfBezier && ap->keyframes[i].handleType != HFree && ap->keyframes[i].handleType != HAligned)
                     ap->keyframes[i].handleType = HAuto;
                 UndoKeyframesConfirmar(); KfAplicado(); } }
    if (g_menuKfInterp && MenuAbierto == g_menuKfInterp) g_menuKfInterp->Cerrar();
}
static void AccionMenuKfHandle(int id){
    if (id >= 0){ int i; AnimProperty* ap = DopeKeyframeActivo(&i);
        if (ap){ UndoKeyframesIniciar();
                 keyFrame& k = ap->keyframes[i];
                 if (id == HFree || id == HAligned){   // congelar lo que se veia (los calculados no guardan nada)
                     float aDF,aDV,bDF,bDV;
                     ap->HandleEfectivo((size_t)i, false, aDF, aDV);
                     ap->HandleEfectivo((size_t)i, true,  bDF, bDV);
                     k.inDF=aDF; k.inDV=aDV; k.outDF=bDF; k.outDV=bDV;
                 }
                 k.handleType = id;
                 UndoKeyframesConfirmar(); KfAplicado(); } }
    if (g_menuKfHandle && MenuAbierto == g_menuKfHandle) g_menuKfHandle->Cerrar();
}
static void AccionKfBtnInterp(){
    if (!gKfInterp) return;
    if (!g_menuKfInterp){ g_menuKfInterp = new PopupMenu(); g_menuKfInterp->action = AccionMenuKfInterp;
                          g_menuKfInterp->titulo = T("Interpolation"); }
    g_menuKfInterp->Limpiar();
    g_menuKfInterp->Agregar(T("Constant"), KfConstant);
    g_menuKfInterp->Agregar(T("Linear"), KfLinear);
    g_menuKfInterp->Agregar("Bezier", KfBezier);
    if (MenuAbierto && MenuAbierto != g_menuKfInterp) MenuAbierto->Cerrar();
    g_menuKfInterp->Abrir(gKfInterp->button->sx, gKfInterp->button->sy + gKfInterp->button->height,
                          MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_menuKfInterp;
}
static void AccionKfBtnHandle(){
    if (!gKfHandle) return;
    if (!g_menuKfHandle){ g_menuKfHandle = new PopupMenu(); g_menuKfHandle->action = AccionMenuKfHandle;
                          g_menuKfHandle->titulo = T("Handle Type"); }
    g_menuKfHandle->Limpiar();
    g_menuKfHandle->Agregar(T("Free"), HFree);
    g_menuKfHandle->Agregar(T("Aligned"), HAligned);
    g_menuKfHandle->Agregar(T("Vector"), HVector);
    g_menuKfHandle->Agregar(T("Automatic"), HAuto);
    g_menuKfHandle->Agregar(T("Auto Clamped"), HAutoClamped);
    if (MenuAbierto && MenuAbierto != g_menuKfHandle) MenuAbierto->Cerrar();
    g_menuKfHandle->Abrir(gKfHandle->button->sx, gKfHandle->button->sy + gKfHandle->button->height,
                          MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_menuKfHandle;
}

// ===== tarjeta ARCHIVO: abrir/guardar el proyecto .w3d (v3: JSON plano + archivos externos) =====
static std::string ProyRutaCompleta() {
    if (!PropsActivo) return "";
    std::string c = PropsActivo->propProyCarpeta ? PropsActivo->propProyCarpeta->field.text : "";
    std::string n = PropsActivo->propProyNombre ? PropsActivo->propProyNombre->field.text : "";
    // sin NOMBRE no hay archivo que guardar: el caller cae en "Guardar como"
    // (antes salia "/carpeta/.w3d", un archivo oculto sin nombre)
    if (n.find_first_not_of(" \t") == std::string::npos) return "";
    return W3dRutaEnCarpeta(c, n, ".w3d"); // carpeta + nombre + ext, sin barras duplicadas
}

// nombre tipeado en la tarjeta Archivo ("Nombre"). Lo usa GuardarW3D cuando el
// explorador devuelve una CARPETA: la ruta final es carpeta + ESTE nombre + ".w3d".
// "" si el panel todavia no existe (arranque sin pestania Render abierta).
std::string ProyectoNombreCampo() {
    if (!PropsActivo || !PropsActivo->propProyNombre) return std::string();
    return PropsActivo->propProyNombre->field.text;
}

// carpeta elegida en el explorador para el PROYECTO -> al campo "Carpeta" de la
// tarjeta Archivo (si tocaste un .w3d, se parte en carpeta + nombre).
static void ProyCarpetaElegida(const std::string& elegido) {
    if (!PropsActivo || !PropsActivo->propProyCarpeta) return;
    if (!elegido.empty() && !w3dFileSystem::IsDir(elegido)) { // es un ARCHIVO: partirlo
        PropsActivo->propProyCarpeta->field.SetText(w3dFileSystem::ParentPath(elegido));
        if (PropsActivo->propProyNombre) {
            size_t s = elegido.find_last_of("/\\");
            PropsActivo->propProyNombre->field.SetText(s == std::string::npos ? elegido : elegido.substr(s + 1));
        }
    } else {
        PropsActivo->propProyCarpeta->field.SetText(elegido);
    }
    g_redraw = true;
}
// el campo "Carpeta" ES el boton de examinar: abre el explorador en modo carpeta
static void AccionBrowseProyCarpeta() {
    AbrirFileBrowser("Carpeta del proyecto", T("Use this file"), ".w3d", ProyCarpetaElegida, true);
}

// llena los campos de la tarjeta desde w3dPath (al abrir/guardar/cambiar)
void ProyectoSincronizarCampos() {
    if (!PropsActivo) return;
    std::string dir, nom;
    if (!w3dPath.empty()) {
        size_t s = w3dPath.find_last_of("/\\");
        dir = (s == std::string::npos) ? "" : w3dPath.substr(0, s);
        nom = (s == std::string::npos) ? w3dPath : w3dPath.substr(s + 1);
    }
    if (PropsActivo->propProyCarpeta) PropsActivo->propProyCarpeta->field.SetText(dir);
    if (PropsActivo->propProyNombre)  PropsActivo->propProyNombre->field.SetText(nom);
    // "Guardar version vN": el N real segun los <proyecto>_vNN.w3d que hay al lado
    // (se recalcula al abrir un proyecto y tras cada guardado de version)
    if (PropsActivo->propProyVersion) PropsActivo->propProyVersion->button->text = GuardarVersionLabel();
    // el icono del juego viaja con el proyecto: refrescar el boton de la tarjeta Juego
    if (PropsActivo->propJuegoIcono)  PropsActivo->propJuegoIcono->button->text = NombreIconoJuego();
    // la CONFIG de "Compilar juego" tambien viaja con el proyecto (bloque
    // "compilar" del .w3d, cargado en g_proyCompilar al abrir): refrescar los
    // desplegables de la tarjeta Juego. Los checkboxes (fisica/sonido/debug)
    // apuntan directo a g_proyCompilar y se refrescan solos.
    if (PropsActivo->propJuegoPlat)     PropsActivo->propJuegoPlat->button->text     = NombrePlat(g_proyCompilar.plataforma);
    if (PropsActivo->propJuegoModoVent) PropsActivo->propJuegoModoVent->button->text = NombreModoVent(g_proyCompilar.modoVentana);
    if (PropsActivo->propJuegoOrient)   PropsActivo->propJuegoOrient->button->text   = NombreOrientacion(g_proyCompilar.orientacion);
    if (PropsActivo->propJuegoAssets)   PropsActivo->propJuegoAssets->button->text   = NombreAssetsModo(g_proyCompilar.assetsModo);
    g_redraw = true;
}

static void AccionProyAbrirElegido(const std::string& ruta) {
    extern void AbrirProyectoDesde(const std::string&);
    AbrirProyectoDesde(ruta);
}
static void AccionProyAbrir() {
    AbrirFileBrowser("Abrir proyecto", "Abrir", ".w3d", AccionProyAbrirElegido);
}
static void AccionProyGuardar() {
    std::string r = ProyRutaCompleta();
    if (r.empty() || r == ".w3d") { GuardarProyectoComo(); return; }
    if (GuardarW3D(r)) { w3dPath = r; ProyectoSincronizarCampos(); }
}
static void AccionProyComo() {
    GuardarProyectoComo();
}
// "Guardar version vN": guardar normal + UNA COPIA del .w3d recien guardado, al
// lado, como <proyecto>_vNN.w3d (ver GuardarVersion.h): el .w3d es un contenedor,
// asi que copiarlo es copiar la version entera. Despues se refresca el label.
static void AccionProyVersion() {
    GuardarVersionEjecutar();
    ProyectoSincronizarCampos();   // el boton pasa a "Guardar version v(N+1)"
}

// ===== EXTRAER LOS ASSETS: sacar lo de adentro del .w3d a una carpeta del disco
//  para editarlo con otro programa (Krita, Audacity, un editor de texto).
//
//  Es la contraparte de "importar": importar mete lo de afuera adentro; extraer
//  saca una copia hacia afuera. NO cambia nada del proyecto (el .w3d queda igual
//  y las referencias siguen siendo internas): lo que sale es una COPIA, y para
//  que un cambio vuelva hay que volver a importarlo. Se dice asi en el toast,
//  porque lo contrario ("edito el png extraido y el juego cambia solo") seria la
//  expectativa razonable y es justo lo que NO pasa.
//
//  Si lo que queres es que un archivo viva AFUERA de forma permanente (una
//  textura que editas todo el tiempo con otro programa), eso es otra cosa: se
//  referencia como externa y el .w3d guarda la ruta (ver W3dRefExternaMarcar en
//  io/W3dContenedor.h). Ahi si, editar el archivo cambia el proyecto.
static void ProyExtraerCarpetaElegida(const std::string& elegido) {
    if (elegido.empty()) return;
    std::string dir = w3dFileSystem::IsDir(elegido) ? elegido : w3dFileSystem::ParentPath(elegido);
    if (dir.empty()) return;
    std::vector<std::string> ents;
    W3dContenedorListarCarpeta("", ents);      // "" = todas las entradas
    int sacados = 0, fallados = 0;
    for (size_t i = 0; i < ents.size(); i++) {
        if (W3dEsEntradaDeServicio(ents[i])) continue;   // mimetype/LEEME/EXTERNOS: no son assets
        // W3dContenedorExtraer crea las carpetas del destino ("texturas/") solo
        if (W3dContenedorExtraer(ents[i], dir + "/" + ents[i])) sacados++; else fallados++;
    }
    char b[256];
    if (fallados)
        snprintf(b, sizeof(b), "Extraidos %d archivo(s); %d fallaron (ver el log)", sacados, fallados);
    else
        snprintf(b, sizeof(b), "Extraidos %d archivo(s). Son COPIAS: para que un cambio "
                               "vuelva al proyecto hay que importarlo de nuevo", sacados);
    Notificar(b, fallados != 0);
}
static void AccionProyExtraer() {
    if (!W3dContenedorHayMontado()) {
        Notificar("Extraer: primero guarda el proyecto (los assets viven adentro del .w3d)", true);
        return;
    }
    AbrirFileBrowser("Extraer los assets a...", T("Use this file"), "", ProyExtraerCarpetaElegida, true);
}


// ROMBO de keyframe del panel: opera sobre el OBJETO ACTIVO en el FRAME ACTUAL.
static int PropKeyEstadoHook(int prop, int comp){
    extern int AnimCanalEstado(Object*, int, int, int);
    return ObjActivo ? AnimCanalEstado(ObjActivo, prop, comp, CurrentFrame) : 0;
}
static void PropKeyToggleHook(int prop, int comp){
    extern void AnimCanalToggle(Object*, int, int, int);
    if (ObjActivo){ AnimCanalToggle(ObjActivo, prop, comp, CurrentFrame);
                    PropertiesLayoutDirty = true; g_redraw = true; }
}

// Start/End/FPS de la animacion del objeto seleccionada (espejos float; AnimSet*
// rutean a la animacion PROPIA cuando kind 3)
float g_objAnimStartF=1, g_objAnimEndF=250, g_objAnimFpsF=30;
PropFloat *gPropObjAnimStart=0, *gPropObjAnimEnd=0, *gPropObjAnimFps=0;
static void AccionObjAnimStart(){ int v=(int)(g_objAnimStartF+0.5f); if(v<0)v=0; AnimSetStart(v); if(CurrentFrame<StartFrame)CurrentFrame=StartFrame; g_redraw=true; }
static void AccionObjAnimEnd(){ int v=(int)(g_objAnimEndF+0.5f); if(v<1)v=1; AnimSetEnd(v); if(CurrentFrame>EndFrame)CurrentFrame=EndFrame; g_redraw=true; }
static void AccionObjAnimFps(){ int v=(int)(g_objAnimFpsF+0.5f); if(v<1)v=1; if(v>120)v=120; AnimSetFps(v); g_objAnimFpsF=(float)v; g_redraw=true; }

void Properties::ConstruirGrupos(){
    propTransform = new GroupPropertie(T("Transform"));
    propTransform->reservaKeyBtn = true;   // columna del boton de keyframe a la derecha

    propTransform->properties.push_back(new PropFloat(T("Location X")));
    propTransform->properties.push_back(new PropFloat("Y"));
    propTransform->properties.push_back(new PropFloat("Z"));

    propTransform->properties.push_back(new PropGap(""));

    // selector de modo de rotacion (dropdown) + campos W/X/Y/Z. Que campos se
    // muestran (W solo en Quaternion/Axis) y a que apuntan lo hace RefreshTarget.
    propRotMode = new PropButton(T("Mode"));                            // [4]
    propRotMode->conLabel = true;   // label a la izquierda, el desplegable a la derecha
    propRotMode->button->desplegable = true;
    propRotMode->action = AccionMenuRotMode;
    propTransform->properties.push_back(propRotMode);
    propTransform->properties.push_back(new PropFloat(T("Rotation W"))); // [5] (condicional)
    propTransform->properties.push_back(new PropFloat(T("Rotation X"))); // [6]
    propTransform->properties.push_back(new PropFloat("Y"));          // [7]
    propTransform->properties.push_back(new PropFloat("Z"));          // [8]
    // editar W/X/Y/Z (flechas o arrastre) reconstruye el quaternion real
    for (int r = 5; r <= 8; r++)
        static_cast<PropFloat*>(propTransform->properties[r])->onChange = SincronizarRotacionActiva;

    propTransform->properties.push_back(new PropGap(""));

    propTransform->properties.push_back(new PropFloat(T("Scale X")));
    propTransform->properties.push_back(new PropFloat("Y"));
    propTransform->properties.push_back(new PropFloat("Z"));

    // NOMBRE del objeto (campo de texto tipo render, NO un boton): se ve el nombre y al clickear se edita (teclado en
    // tactil). El commit -> ObjActivo->name (uniquificado) lo hace SincronizarNombreObjeto por frame. Antes era un boton
    // "Rename" que se volvia input pero no sacaba teclado en Android (inconsistente). El nombre del objeto no se ve en
    // otro lado, asi que aca SI conviene el campo (en material/uv/color el nombre ya se ve -> se dejaron como boton).
    propNameObj = new PropText(T("Name"), "");
    propTransform->properties.push_back(propNameObj);

    // canal de animacion (rombo de keyframe a la izquierda) en pos/rot/escala.
    // Se ubican por INDICE del push de arriba: LocX[0] Y[1] Z[2], RotX[6] Y[7] Z[8], SclX[10] Y[11] Z[12]
    { PropFloat* pf;
      pf=(PropFloat*)propTransform->properties[0]; pf->animProp=AnimPosition; pf->animComp=AnimX;
      pf=(PropFloat*)propTransform->properties[1]; pf->animProp=AnimPosition; pf->animComp=AnimZ; // el campo "Y" es pos.z (display Z-up)
      pf=(PropFloat*)propTransform->properties[2]; pf->animProp=AnimPosition; pf->animComp=AnimY; // el campo "Z" es pos.y
      pf=(PropFloat*)propTransform->properties[6]; pf->animProp=AnimRotation; pf->animComp=AnimX;
      pf=(PropFloat*)propTransform->properties[7]; pf->animProp=AnimRotation; pf->animComp=AnimY;
      pf=(PropFloat*)propTransform->properties[8]; pf->animProp=AnimRotation; pf->animComp=AnimZ;
      pf=(PropFloat*)propTransform->properties[10]; pf->animProp=AnimScale; pf->animComp=AnimX;
      pf=(PropFloat*)propTransform->properties[11]; pf->animProp=AnimScale; pf->animComp=AnimY;
      pf=(PropFloat*)propTransform->properties[12]; pf->animProp=AnimScale; pf->animComp=AnimZ; }
    // checkboxes VISIBLE + RENDERIZAR (propiedades del objeto que nunca se mostraban;
    // animables: la curva 0/1 dice si en ese frame se ve / se renderiza)
    propObjVisible = new PropBool("Visible");
    propObjVisible->animProp = AnimVisible; propObjVisible->animComp = AnimX;
    propTransform->properties.push_back(propObjVisible);
    propObjRender = new PropBool("Renderizar");
    propObjRender->animProp = AnimRender; propObjRender->animComp = AnimX;
    propTransform->properties.push_back(propObjRender);
    // LINEAS PARENTALES por objeto. El flag existia desde siempre (Objects.h) y lo leia
    // el render, pero NUNCA se expuso: no habia forma de apagarlas salvo el toggle global
    // del menu Overlays. Va en propTransform, que es la tarjeta GENERICA bindeada a
    // ObjActivo sin mirar getType(), asi que queda universal para TODOS los tipos: es lo
    // que permite apagarle las lineas al EscenarioCulling (52 hijos = 52 lineas que
    // molestan a la vista y se calculan por frame) sin perder las de los demas.
    propObjRelLines = new PropBool(T("Relationship Lines"));
    propTransform->properties.push_back(propObjRelLines);
    GroupProperties.push_back(propTransform);

    // ===== pestania OBJETO: tarjeta "Animacion" — las animaciones DEL objeto
    // (contenedores; hoy: vertex anims de la malla). Lista + New + Delete. =====
    propObjAnim = new GroupPropertie("Animacion");
    propListObjAnims = new PropListMeshParts("Animacion");
    propListObjAnims->modo = 7;
    propObjAnim->properties.push_back(propListObjAnims);
    // rango + fps de la animacion seleccionada
    { PropFloat* pS = new PropFloat("Inicio"); pS->entero=true; pS->stepFino=1; pS->stepGrueso=10; pS->dragStep=1;
      pS->value=&g_objAnimStartF; pS->onChange=AccionObjAnimStart; gPropObjAnimStart=pS;
      propObjAnim->properties.push_back(pS); }
    { PropFloat* pE = new PropFloat("Fin"); pE->entero=true; pE->stepFino=1; pE->stepGrueso=10; pE->dragStep=1;
      pE->value=&g_objAnimEndF; pE->onChange=AccionObjAnimEnd; gPropObjAnimEnd=pE;
      propObjAnim->properties.push_back(pE); }
    { PropFloat* pF = new PropFloat("FPS"); pF->entero=true; pF->SetRango(1,120); pF->stepFino=1; pF->stepGrueso=5; pF->dragStep=1;
      pF->value=&g_objAnimFpsF; pF->onChange=AccionObjAnimFps; gPropObjAnimFps=pF;
      propObjAnim->properties.push_back(pF); }
    { PropButton* pb = new PropButton("Nueva animacion", IconType::keyframe);
      pb->action = AccionObjAnimNew;
      propObjAnim->properties.push_back(pb); }
    { PropButton* pb = new PropButton(T("Delete"), -1);
      pb->action = AccionObjAnimDel;
      propObjAnim->properties.push_back(pb); }
    GroupProperties.push_back(propObjAnim);

    // ===== Tarjeta "Texto" (elemento Texto2D del Editor 2D) =====
    // ARRIBA de todo: Nombre y Posicion (los elementos 2D no muestran el tab Objeto; esto
    // era lo unico que se usaba de ahi)
    propTexto2D = new GroupPropertie(T("Text"));
    propTexto2D->icono = (int)IconType::lista;
    propT2dNombre = new PropText(T("Name"), "");
    propTexto2D->properties.push_back(propT2dNombre);
    propT2dPosX = new PropFloat(T("Location X"));
    propTexto2D->properties.push_back(propT2dPosX);
    propT2dPosY = new PropFloat("Y");
    propTexto2D->properties.push_back(propT2dPosY);
    propT2dPosZ = new PropFloat("Z");
    propTexto2D->properties.push_back(propT2dPosZ);
    propT2dPosAbs = new PropBool(T("Pixels"));   // off = relativa al tamano de la UI
    propT2dPosAbs->onChange = AccionPos2DAbsToggle;
    propTexto2D->properties.push_back(propT2dPosAbs);
    propT2dPeso = new PropFloat(T("Weight"));    // reparto en filas/columnas del padre
    propT2dPeso->SetRango(0.01f, 100.0f);
    propTexto2D->properties.push_back(propT2dPeso);
    propT2dTexto = new PropText(T("Text"), "");
    propTexto2D->properties.push_back(propT2dTexto);
    // TIPO del contenido (string / number / float) + decimales del float
    propT2dTipo = new PropButton(T("Type"));
    propT2dTipo->conLabel = true;
    propT2dTipo->button->desplegable = true;
    propT2dTipo->action = AccionMenuT2dTipo;
    propTexto2D->properties.push_back(propT2dTipo);
    propT2dDec = new PropFloat(T("Decimals"));
    propT2dDec->SetRango(0.0f, 8.0f); propT2dDec->entero = true;
    propTexto2D->properties.push_back(propT2dDec);
    propT2dTam = new PropFloat(T("Size"), "px");
    propT2dTam->SetRango(1.0f, 2000.0f);
    propTexto2D->properties.push_back(propT2dTam);
    propT2dRot = new PropFloat(T("Rotation"), "o");   // debajo de Tamano
    propTexto2D->properties.push_back(propT2dRot);
    // desplegables: label a la IZQUIERDA, el boton del menu a la DERECHA (conLabel)
    // la FUENTE va entre Rotacion y Align X (molestaba entre ancla y alineacion)
    propT2dFuente = new PropButton(T("Font"));
    propT2dFuente->conLabel = true;
    propT2dFuente->button->desplegable = true;
    propT2dFuente->action = AccionMenuT2dFuente;
    propTexto2D->properties.push_back(propT2dFuente);
    // LINEAS (una / por palabras / donde sea) + ajustar el tamano al area disponible
    propT2dLineas = new PropButton(T("Lines"));
    propT2dLineas->conLabel = true;
    propT2dLineas->button->desplegable = true;
    propT2dLineas->action = AccionMenuT2dLineas;
    propTexto2D->properties.push_back(propT2dLineas);
    propT2dAutoTam = new PropBool(T("Auto Fit"));
    propTexto2D->properties.push_back(propT2dAutoTam);
    propT2dAlignH = new PropButton(T("Align X"));
    propT2dAlignH->conLabel = true;
    propT2dAlignH->button->desplegable = true;
    propT2dAlignH->action = AccionMenuT2dAlignH;
    propTexto2D->properties.push_back(propT2dAlignH);
    propT2dAlignV = new PropButton(T("Align Y"));
    propT2dAlignV->conLabel = true;
    propT2dAlignV->button->desplegable = true;
    propT2dAlignV->action = AccionMenuT2dAlignV;
    propTexto2D->properties.push_back(propT2dAlignV);
    // ANCLA: desde donde se mide la posicion (el centro del padre por defecto, o bordes/esquinas).
    // Es lo que hace que la interfaz se ADAPTE al tamano de la ventana.
    propT2dAncla = new PropButton(T("Anchor"));
    propT2dAncla->conLabel = true;
    propT2dAncla->button->desplegable = true;
    propT2dAncla->action = AccionMenuT2dAncla;
    propTexto2D->properties.push_back(propT2dAncla);
    propT2dOpac = new PropFloat(T("Opacity"));
    propT2dOpac->SetRango(0.0f, 1.0f);
    propT2dOpac->stepFino = 0.01f; propT2dOpac->stepGrueso = 0.1f; propT2dOpac->dragStep = 0.005f;
    propTexto2D->properties.push_back(propT2dOpac);
    propT2dPal = new PropButton(T("Palette"));   // color de la paleta del UI (o propio)
    propT2dPal->conLabel = true;
    propT2dPal->button->desplegable = true;
    propT2dPal->action = AccionPalT2d;
    propTexto2D->properties.push_back(propT2dPal);
    propT2dColor = new PropColor(T("Color"));   // el color al FINAL de la lista
    propTexto2D->properties.push_back(propT2dColor);
    GroupProperties.push_back(propTexto2D);

    // ===== Tarjeta "Imagen" (elemento Imagen2D del Editor 2D) =====
    propImagen2D = new GroupPropertie(T("Image"));
    propImagen2D->icono = (int)IconType::foto;
    propImgNombre = new PropText(T("Name"), "");
    propImagen2D->properties.push_back(propImgNombre);
    propImgPosX = new PropFloat(T("Location X"));
    propImagen2D->properties.push_back(propImgPosX);
    propImgPosY = new PropFloat("Y");
    propImagen2D->properties.push_back(propImgPosY);
    propImgPosZ = new PropFloat("Z");
    propImagen2D->properties.push_back(propImgPosZ);
    propImgPosAbs = new PropBool(T("Pixels"));
    propImgPosAbs->onChange = AccionPos2DAbsToggle;
    propImagen2D->properties.push_back(propImgPosAbs);
    propImgPeso = new PropFloat(T("Weight"));
    propImgPeso->SetRango(0.01f, 100.0f);
    propImagen2D->properties.push_back(propImgPeso);
    propImgTextura = new PropButton(T("Texture"));   // abre el file browser (con vista previa)
    propImgTextura->conLabel = true;
    propImgTextura->action = AccionImgTextura;
    propImagen2D->properties.push_back(propImgTextura);
    propImgAncho = new PropFloat(T("Width"), "px");
    propImgAncho->SetRango(1.0f, 8192.0f);
    propImagen2D->properties.push_back(propImgAncho);
    propImgAlto = new PropFloat(T("Height"), "px");
    propImgAlto->SetRango(1.0f, 8192.0f);
    propImagen2D->properties.push_back(propImgAlto);
    propImgUnidad = new PropButton(T("Unit"));   // unidad del tamano (fraccion/px/escalado)
    propImgUnidad->conLabel = true;
    propImgUnidad->button->desplegable = true;
    propImgUnidad->action = AccionMenuTamModo;
    propImagen2D->properties.push_back(propImgUnidad);
    propImgRot = new PropFloat(T("Rotation"), "o");
    propImagen2D->properties.push_back(propImgRot);
    // como se acomoda la textura en el rect: estirar / ajustar (bandas) / cover (recorta)
    propImgModo = new PropButton(T("Mode"));
    propImgModo->conLabel = true;
    propImgModo->button->desplegable = true;
    propImgModo->action = AccionMenuImgModo;
    propImagen2D->properties.push_back(propImgModo);
    propImgAncla = new PropButton(T("Anchor"));
    propImgAncla->conLabel = true;
    propImgAncla->button->desplegable = true;
    propImgAncla->action = AccionMenuImgAncla;
    propImagen2D->properties.push_back(propImgAncla);
    propImgOpac = new PropFloat(T("Opacity"));
    propImgOpac->SetRango(0.0f, 1.0f);
    propImgOpac->stepFino = 0.01f; propImgOpac->stepGrueso = 0.1f; propImgOpac->dragStep = 0.005f;
    propImagen2D->properties.push_back(propImgOpac);
    propImgAlpha = new PropBool("Alpha");   // usar el canal alpha de la textura
    propImagen2D->properties.push_back(propImgAlpha);
    propImgFiltro = new PropBool(T("Filtering"));   // off = NEAREST (pixel-perfect)
    propImagen2D->properties.push_back(propImgFiltro);
    propImgPal = new PropButton(T("Palette"));
    propImgPal->conLabel = true;
    propImgPal->button->desplegable = true;
    propImgPal->action = AccionPalImg;
    propImagen2D->properties.push_back(propImgPal);
    propImgColor = new PropColor(T("Color"));   // tinte (blanco = tal cual)
    propImagen2D->properties.push_back(propImgColor);
    GroupProperties.push_back(propImagen2D);

    // ===== Tarjeta "Rectangulo" (color solido o transparente: acomoda hijos) =====
    propRect2D = new GroupPropertie(T("Rectangle"));
    propRect2D->icono = (int)IconType::plane;
    propRectNombre = new PropText(T("Name"), "");
    propRect2D->properties.push_back(propRectNombre);
    propRectPosX = new PropFloat(T("Location X"));
    propRect2D->properties.push_back(propRectPosX);
    propRectPosY = new PropFloat("Y");
    propRect2D->properties.push_back(propRectPosY);
    propRectPosZ = new PropFloat("Z");
    propRect2D->properties.push_back(propRectPosZ);
    propRectPosAbs = new PropBool(T("Pixels"));
    propRectPosAbs->onChange = AccionPos2DAbsToggle;
    propRect2D->properties.push_back(propRectPosAbs);
    propRectPeso = new PropFloat(T("Weight"));
    propRectPeso->SetRango(0.01f, 100.0f);
    propRect2D->properties.push_back(propRectPeso);
    propRectAncho = new PropFloat(T("Width"), "px");
    propRectAncho->SetRango(1.0f, 8192.0f);
    propRect2D->properties.push_back(propRectAncho);
    propRectAlto = new PropFloat(T("Height"), "px");
    propRectAlto->SetRango(1.0f, 8192.0f);
    propRect2D->properties.push_back(propRectAlto);
    propRectUnidad = new PropButton(T("Unit"));
    propRectUnidad->conLabel = true;
    propRectUnidad->button->desplegable = true;
    propRectUnidad->action = AccionMenuTamModo;
    propRect2D->properties.push_back(propRectUnidad);
    propRectRot = new PropFloat(T("Rotation"), "o");
    propRect2D->properties.push_back(propRectRot);
    propRectAncla = new PropButton(T("Anchor"));
    propRectAncla->conLabel = true;
    propRectAncla->button->desplegable = true;
    propRectAncla->action = AccionMenuRectAncla;
    propRect2D->properties.push_back(propRectAncla);
    propRectOpac = new PropFloat(T("Opacity"));
    propRectOpac->SetRango(0.0f, 1.0f);
    propRectOpac->stepFino = 0.01f; propRectOpac->stepGrueso = 0.1f; propRectOpac->dragStep = 0.005f;
    propRect2D->properties.push_back(propRectOpac);
    propRectPal = new PropButton(T("Palette"));
    propRectPal->conLabel = true;
    propRectPal->button->desplegable = true;
    propRectPal->action = AccionPalRect;
    propRect2D->properties.push_back(propRectPal);
    propRectColor = new PropColor(T("Color"));   // alpha 0 = 100% transparente (solo acomoda)
    propRect2D->properties.push_back(propRectColor);
    GroupProperties.push_back(propRect2D);

    // ===== Tarjeta "Contenedor" (rectangulo invisible: solo ordena a sus hijos) =====
    propCont2D = new GroupPropertie(T("Container"));
    propCont2D->icono = (int)IconType::carpeta;
    propContNombre = new PropText(T("Name"), "");
    propCont2D->properties.push_back(propContNombre);
    propContPosX = new PropFloat(T("Location X"));
    propCont2D->properties.push_back(propContPosX);
    propContPosY = new PropFloat("Y");
    propCont2D->properties.push_back(propContPosY);
    propContPosZ = new PropFloat("Z");
    propCont2D->properties.push_back(propContPosZ);
    propContPosAbs = new PropBool(T("Pixels"));
    propContPosAbs->onChange = AccionPos2DAbsToggle;
    propCont2D->properties.push_back(propContPosAbs);
    propContPeso = new PropFloat(T("Weight"));
    propContPeso->SetRango(0.01f, 100.0f);
    propCont2D->properties.push_back(propContPeso);
    propContAncho = new PropFloat(T("Width"), "px");
    propContAncho->SetRango(1.0f, 8192.0f);
    propCont2D->properties.push_back(propContAncho);
    propContAlto = new PropFloat(T("Height"), "px");
    propContAlto->SetRango(1.0f, 8192.0f);
    propCont2D->properties.push_back(propContAlto);
    propContUnidad = new PropButton(T("Unit"));
    propContUnidad->conLabel = true;
    propContUnidad->button->desplegable = true;
    propContUnidad->action = AccionMenuTamModo;
    propCont2D->properties.push_back(propContUnidad);
    propContRot = new PropFloat(T("Rotation"), "o");
    propCont2D->properties.push_back(propContRot);
    propContAncla = new PropButton(T("Anchor"));
    propContAncla->conLabel = true;
    propContAncla->button->desplegable = true;
    propContAncla->action = AccionMenuContAncla;
    propCont2D->properties.push_back(propContAncla);
    propContOpac = new PropFloat(T("Opacity"));
    propContOpac->SetRango(0.0f, 1.0f);
    propContOpac->stepFino = 0.01f; propContOpac->stepGrueso = 0.1f; propContOpac->dragStep = 0.005f;
    propCont2D->properties.push_back(propContOpac);
    GroupProperties.push_back(propCont2D);

    // ===== Tarjeta "Slice 9" (imagen con bordes fijos) =====
    propS9card = new GroupPropertie("Slice 9");
    propS9card->icono = (int)IconType::cuadricula;
    propS9Nombre = new PropText(T("Name"), "");
    propS9card->properties.push_back(propS9Nombre);
    propS9PosX = new PropFloat(T("Location X"));
    propS9card->properties.push_back(propS9PosX);
    propS9PosY = new PropFloat("Y");
    propS9card->properties.push_back(propS9PosY);
    propS9PosZ = new PropFloat("Z");
    propS9card->properties.push_back(propS9PosZ);
    propS9PosAbs = new PropBool(T("Pixels"));
    propS9PosAbs->onChange = AccionPos2DAbsToggle;
    propS9card->properties.push_back(propS9PosAbs);
    propS9Peso = new PropFloat(T("Weight"));
    propS9Peso->SetRango(0.01f, 100.0f);
    propS9card->properties.push_back(propS9Peso);
    propS9Textura = new PropButton(T("Texture"));
    propS9Textura->conLabel = true;
    propS9Textura->action = AccionS9Textura;
    propS9card->properties.push_back(propS9Textura);
    propS9Ancho = new PropFloat(T("Width"), "px");
    propS9Ancho->SetRango(1.0f, 8192.0f);
    propS9card->properties.push_back(propS9Ancho);
    propS9Alto = new PropFloat(T("Height"), "px");
    propS9Alto->SetRango(1.0f, 8192.0f);
    propS9card->properties.push_back(propS9Alto);
    propS9Unidad = new PropButton(T("Unit"));
    propS9Unidad->conLabel = true;
    propS9Unidad->button->desplegable = true;
    propS9Unidad->action = AccionMenuTamModo;
    propS9card->properties.push_back(propS9Unidad);
    // el borde: cuanto mide en el ARCHIVO (por eje: esquinas rectangulares si difieren;
    // minimo 1, maximo la mitad de la imagen menos 1) y a que escala se dibuja
    propS9BordeX = new PropFloat(T("Border X"), "px");
    propS9BordeX->SetRango(1.0f, 512.0f); propS9BordeX->entero = true;
    propS9card->properties.push_back(propS9BordeX);
    propS9BordeY = new PropFloat(T("Border Y"), "px");
    propS9BordeY->SetRango(1.0f, 512.0f); propS9BordeY->entero = true;
    propS9card->properties.push_back(propS9BordeY);
    propS9EscBorde = new PropFloat(T("Border Scale"));
    propS9EscBorde->SetRango(0.05f, 16.0f);
    propS9EscBorde->stepFino = 0.05f; propS9EscBorde->stepGrueso = 0.5f; propS9EscBorde->dragStep = 0.01f;
    propS9card->properties.push_back(propS9EscBorde);
    propS9Rot = new PropFloat(T("Rotation"), "o");
    propS9card->properties.push_back(propS9Rot);
    propS9Ancla = new PropButton(T("Anchor"));
    propS9Ancla->conLabel = true;
    propS9Ancla->button->desplegable = true;
    propS9Ancla->action = AccionMenuS9Ancla;
    propS9card->properties.push_back(propS9Ancla);
    propS9Opac = new PropFloat(T("Opacity"));
    propS9Opac->SetRango(0.0f, 1.0f);
    propS9Opac->stepFino = 0.01f; propS9Opac->stepGrueso = 0.1f; propS9Opac->dragStep = 0.005f;
    propS9card->properties.push_back(propS9Opac);
    propS9Filtro = new PropBool(T("Filtering"));
    propS9card->properties.push_back(propS9Filtro);
    propS9Pal = new PropButton(T("Palette"));
    propS9Pal->conLabel = true;
    propS9Pal->button->desplegable = true;
    propS9Pal->action = AccionPalS9;
    propS9card->properties.push_back(propS9Pal);
    propS9Color = new PropColor(T("Color"));   // tinte (el arte del editor es blanco)
    propS9card->properties.push_back(propS9Color);
    GroupProperties.push_back(propS9card);

    // ===== Tarjeta "Boton" (card con texto y/o icono, estilo Whisk3D) =====
    propBtn2D = new GroupPropertie(T("Button"));
    propBtn2D->icono = (int)IconType::object;
    propBtnNombre = new PropText(T("Name"), "");
    propBtn2D->properties.push_back(propBtnNombre);
    propBtnPosX = new PropFloat(T("Location X"));
    propBtn2D->properties.push_back(propBtnPosX);
    propBtnPosY = new PropFloat("Y");
    propBtn2D->properties.push_back(propBtnPosY);
    propBtnPosZ = new PropFloat("Z");
    propBtn2D->properties.push_back(propBtnPosZ);
    propBtnPosAbs = new PropBool(T("Pixels"));
    propBtnPosAbs->onChange = AccionPos2DAbsToggle;
    propBtn2D->properties.push_back(propBtnPosAbs);
    propBtnPeso = new PropFloat(T("Weight"));
    propBtnPeso->SetRango(0.01f, 100.0f);
    propBtn2D->properties.push_back(propBtnPeso);
    propBtnTexto = new PropText(T("Text"), "");
    propBtn2D->properties.push_back(propBtnTexto);
    propBtnIcono = new PropButton(T("Icon"));   // un png (10x10 estilo Whisk3D, o el que sea)
    propBtnIcono->conLabel = true;
    propBtnIcono->action = AccionBtnIcono;
    propBtn2D->properties.push_back(propBtnIcono);
    propBtnTam = new PropFloat(T("Size"), "px");
    propBtnTam->SetRango(1.0f, 512.0f);
    propBtn2D->properties.push_back(propBtnTam);
    propBtnPad = new PropFloat(T("Padding"), "px");
    propBtnPad->SetRango(0.0f, 256.0f);
    propBtn2D->properties.push_back(propBtnPad);
    propBtnAncla = new PropButton(T("Anchor"));
    propBtnAncla->conLabel = true;
    propBtnAncla->button->desplegable = true;
    propBtnAncla->action = AccionMenuBtnAncla;
    propBtn2D->properties.push_back(propBtnAncla);
    propBtnRot = new PropFloat(T("Rotation"), "o");
    propBtn2D->properties.push_back(propBtnRot);
    propBtnOpac = new PropFloat(T("Opacity"));
    propBtnOpac->SetRango(0.0f, 1.0f);
    propBtnOpac->stepFino = 0.01f; propBtnOpac->stepGrueso = 0.1f; propBtnOpac->dragStep = 0.005f;
    propBtn2D->properties.push_back(propBtnOpac);
    // cada color puede ser PROPIO o apuntar a la PALETA del UI (un puntero, no una copia).
    // El swatch de abajo lleva el MISMO label que su desplegable (sin nombre no se sabia
    // cual era cual).
    propBtnPalFondo = new PropButton(T("Background"));
    propBtnPalFondo->conLabel = true;
    propBtnPalFondo->button->desplegable = true;
    propBtnPalFondo->action = AccionPalBtnFondo;
    propBtn2D->properties.push_back(propBtnPalFondo);
    propBtnColFondo = new PropColor(T("Background"));
    propBtn2D->properties.push_back(propBtnColFondo);
    propBtnPalTexto = new PropButton(T("Text Color"));
    propBtnPalTexto->conLabel = true;
    propBtnPalTexto->button->desplegable = true;
    propBtnPalTexto->action = AccionPalBtnTexto;
    propBtn2D->properties.push_back(propBtnPalTexto);
    propBtnColTexto = new PropColor(T("Text Color"));
    propBtn2D->properties.push_back(propBtnColTexto);
    propBtnPalBorde = new PropButton(T("Border Color"));
    propBtnPalBorde->conLabel = true;
    propBtnPalBorde->button->desplegable = true;
    propBtnPalBorde->action = AccionPalBtnBorde;
    propBtn2D->properties.push_back(propBtnPalBorde);
    propBtnColBorde = new PropColor(T("Border Color"));
    propBtn2D->properties.push_back(propBtnColBorde);
    // el borde de HOVER (mouse-over): tambien propio o de la paleta
    propBtnPalHover = new PropButton("Hover");
    propBtnPalHover->conLabel = true;
    propBtnPalHover->button->desplegable = true;
    propBtnPalHover->action = AccionPalBtnHover;
    propBtn2D->properties.push_back(propBtnPalHover);
    propBtnColHover = new PropColor("Hover");
    propBtn2D->properties.push_back(propBtnColHover);
    // FONDO con textura (9 pedazos, como el slice9): opcional
    propBtnTex = new PropButton(T("Texture"));
    propBtnTex->conLabel = true;
    propBtnTex->action = AccionBtnTex;
    propBtn2D->properties.push_back(propBtnTex);
    propBtnTexBX = new PropFloat(T("Border X"), "px");
    propBtnTexBX->SetRango(1.0f, 512.0f); propBtnTexBX->entero = true;
    propBtn2D->properties.push_back(propBtnTexBX);
    propBtnTexBY = new PropFloat(T("Border Y"), "px");
    propBtnTexBY->SetRango(1.0f, 512.0f); propBtnTexBY->entero = true;
    propBtn2D->properties.push_back(propBtnTexBY);
    propBtnTexEsc = new PropFloat(T("Border Scale"));
    propBtnTexEsc->SetRango(0.05f, 16.0f);
    propBtnTexEsc->stepFino = 0.05f; propBtnTexEsc->stepGrueso = 0.5f; propBtnTexEsc->dragStep = 0.01f;
    propBtn2D->properties.push_back(propBtnTexEsc);
    GroupProperties.push_back(propBtn2D);

    // ===== Tarjeta "Video" (fondos animados y festejos; sin sonido) =====
    propVid2D = new GroupPropertie("Video");
    propVid2D->icono = (int)IconType::camera;
    propVidNombre = new PropText(T("Name"), "");
    propVid2D->properties.push_back(propVidNombre);
    propVidPosX = new PropFloat(T("Location X"));
    propVid2D->properties.push_back(propVidPosX);
    propVidPosY = new PropFloat("Y");
    propVid2D->properties.push_back(propVidPosY);
    propVidPosZ = new PropFloat("Z");
    propVid2D->properties.push_back(propVidPosZ);
    propVidPosAbs = new PropBool(T("Pixels"));
    propVidPosAbs->onChange = AccionPos2DAbsToggle;
    propVid2D->properties.push_back(propVidPosAbs);
    propVidPeso = new PropFloat(T("Weight"));
    propVidPeso->SetRango(0.01f, 100.0f);
    propVid2D->properties.push_back(propVidPeso);
    propVidArchivo = new PropButton("Video");
    propVidArchivo->conLabel = true;
    propVidArchivo->action = AccionVidArchivo;
    propVid2D->properties.push_back(propVidArchivo);
    propVidAncho = new PropFloat(T("Width"), "px");
    propVidAncho->SetRango(1.0f, 8192.0f);
    propVid2D->properties.push_back(propVidAncho);
    propVidAlto = new PropFloat(T("Height"), "px");
    propVidAlto->SetRango(1.0f, 8192.0f);
    propVid2D->properties.push_back(propVidAlto);
    propVidUnidad = new PropButton(T("Unit"));
    propVidUnidad->conLabel = true;
    propVidUnidad->button->desplegable = true;
    propVidUnidad->action = AccionMenuTamModo;
    propVid2D->properties.push_back(propVidUnidad);
    propVidModo = new PropButton(T("Mode"));
    propVidModo->conLabel = true;
    propVidModo->button->desplegable = true;
    propVidModo->action = AccionMenuVidModo;
    propVid2D->properties.push_back(propVidModo);
    propVidLoop = new PropBool("Loop");
    propVid2D->properties.push_back(propVidLoop);
    propVidAlpha = new PropBool("Alpha");        // usar la transparencia del video
    propVid2D->properties.push_back(propVidAlpha);
    propVidPlay = new PropBool("Ver animacion"); // reproducir la preview en el editor
    propVid2D->properties.push_back(propVidPlay);
    propVidFiltro = new PropBool(T("Filtering"));
    propVid2D->properties.push_back(propVidFiltro);
    propVidRot = new PropFloat(T("Rotation"), "o");
    propVid2D->properties.push_back(propVidRot);
    propVidAncla = new PropButton(T("Anchor"));
    propVidAncla->conLabel = true;
    propVidAncla->button->desplegable = true;
    propVidAncla->action = AccionMenuVidAncla;
    propVid2D->properties.push_back(propVidAncla);
    propVidOpac = new PropFloat(T("Opacity"));
    propVidOpac->SetRango(0.0f, 1.0f);
    propVidOpac->stepFino = 0.01f; propVidOpac->stepGrueso = 0.1f; propVidOpac->dragStep = 0.005f;
    propVid2D->properties.push_back(propVidOpac);
    GroupProperties.push_back(propVid2D);

    // ===== Tarjeta "Expandir" (resorte de layout: absorbe el espacio libre) =====
    propExp2D = new GroupPropertie(T("Expand"));
    propExp2D->icono = (int)IconType::arrowRight;
    propExpNombre = new PropText(T("Name"), "");
    propExp2D->properties.push_back(propExpNombre);
    propExpPeso = new PropFloat(T("Weight"));   // reparte el espacio libre entre expandirs
    propExpPeso->SetRango(0.01f, 100.0f);
    propExp2D->properties.push_back(propExpPeso);
    GroupProperties.push_back(propExp2D);

    // ===== Tarjeta "UI" (la raiz de la interfaz) =====
    propUIcard = new GroupPropertie("UI");
    propUIcard->icono = (int)IconType::textura;
    propUInombre = new PropText(T("Name"), "");
    propUIcard->properties.push_back(propUInombre);
    propUIver3D = new PropBool(T("View in 3D"));
    propUIcard->properties.push_back(propUIver3D);
    // la ESCALA GLOBAL del contenido: x1 = N95, x2/x3/x4 = pantallas mas grandes
    propUIescala = new PropFloat("Escala", "x");
    propUIescala->SetRango(1.0f, 8.0f); propUIescala->entero = true;
    propUIcard->properties.push_back(propUIescala);
    // el lienzo: "como el render" (default, en vivo) o RESPONSIVE con tamano propio
    propUIigualRender = new PropBool(T("Match render"));
    propUIigualRender->onChange = AccionUIigualRender;
    propUIcard->properties.push_back(propUIigualRender);
    propUIancho = new PropFloat(T("Width"), "px");    // solo en responsive (value NULL los oculta)
    propUIancho->SetRango(16.0f, 8192.0f); propUIancho->entero = true;
    propUIcard->properties.push_back(propUIancho);
    propUIalto = new PropFloat(T("Height"), "px");
    propUIalto->SetRango(16.0f, 8192.0f); propUIalto->entero = true;
    propUIcard->properties.push_back(propUIalto);
    propUIres = new PropButton(T("Resolution"));      // presets 4k .. 240p (el Nokia)
    propUIres->conLabel = true;
    propUIres->button->desplegable = true;
    propUIres->action = AccionMenuUIres;
    propUIcard->properties.push_back(propUIres);
    propUIaspecto = new PropButton(T("Aspect"));      // 16:9 / 4:3 / 1:1
    propUIaspecto->conLabel = true;
    propUIaspecto->button->desplegable = true;
    propUIaspecto->action = AccionMenuUIaspecto;
    propUIcard->properties.push_back(propUIaspecto);
    propUIrotar = new PropButton(T("Rotate"));        // horizontal <-> vertical
    propUIrotar->action = AccionUIrotar;
    propUIcard->properties.push_back(propUIrotar);
    propUIopac = new PropFloat(T("Opacity"));         // atenua la interfaz entera
    propUIopac->SetRango(0.0f, 1.0f);
    propUIopac->stepFino = 0.01f; propUIopac->stepGrueso = 0.1f; propUIopac->dragStep = 0.005f;
    propUIcard->properties.push_back(propUIopac);
    propUIcolor = new PropColor(T("Color"));          // fondo (transparente por defecto)
    propUIcard->properties.push_back(propUIcolor);
    // GUARDAR la interfaz: un .w3dui (JSON) que despues carga el editor o compila el juego
    propUIexport = new PropButton(T("Export UI"), IconType::archive);
    propUIexport->action = AccionUIexportar;
    propUIcard->properties.push_back(propUIexport);
    GroupProperties.push_back(propUIcard);

    // ===== La tarjeta "Control": la LISTA DE SCRIPTS del objeto, con el MISMO patron
    // que el stack de modificadores (lista + Add/Remove + Move Up/Move Down) y, abajo,
    // la tarjeta del script SELECCIONADO. El orden de la lista ES el de ejecucion.
    // Antes esta tarjeta era "nombre + visible + agregar" y mostraba las 8 tarjetas de
    // script a la vez: el nombre ya lo edita el outliner (y la pestania Objeto) y el
    // checkbox Visible era el del objeto -- los dos quedaron viejos y confundian.
    propControl = new GroupPropertie("Control");
    propControl->anchoValores = 0.30f;                        // igual que la tarjeta Modifiers
    propListScripts = new PropListMeshParts("Scripts");
    propListScripts->modo = 12;                               // 12 = scripts lua del objeto
    propControl->properties.push_back(propListScripts);       // [0] la lista (mismo componente)
    // fila: Add (abre el file browser) | Remove (oculta si no hay scripts)
    propRowScript = new PropButtonRow();
    propRowScript->Agregar(T("Add"), AccionScriptAgregar, (int)IconType::archive);
    propRowScript->Agregar(T("Remove"), AccionScriptQuitar);
    propControl->properties.push_back(propRowScript);
    // fila: Move Up | Move Down (oculta entera con < 2 scripts: el orden solo importa con 2+)
    propRowScriptMove = new PropButtonRow();
    propRowScriptMove->Agregar(T("Move Up"),   AccionScriptUp);
    propRowScriptMove->Agregar(T("Move Down"), AccionScriptDown);
    propControl->properties.push_back(propRowScriptMove);
    for (int i = 0; i < kMaxScriptCards; i++)
        propScriptCards[i] = new GroupPropertie("Script");

    // ===== Tarjeta "Paletas" (del PROYECTO, pestania 0): la gestion completa
    // de las paletas (elegir cual editar / crear / borrar / renombrar) y sus
    // colores con NOMBRE (los elementos los referencian por indice contra su
    // paleta efectiva). Las filas se reconstruyen en el rebind al cambiar la
    // cantidad o la paleta en edicion (firma).
    propPaleta = new GroupPropertie(T("Palette"));

    // ===== Tarjeta "Paleta" del OBJETO (cualquier objeto, 3D o 2D): la
    // seleccion HEREDABLE. "Igual que el padre" (default) o una paleta del
    // proyecto; cambiarla re-pinta al objeto y a todos sus herederos.
    propPaletaObj = new GroupPropertie(T("Palette"));
    propPaletaObjSel = new PropButton(T("Palette"));
    propPaletaObjSel->conLabel = true;
    propPaletaObjSel->button->desplegable = true;
    propPaletaObjSel->action = AccionMenuPaletaObj;
    propPaletaObj->properties.push_back(propPaletaObjSel);

    // ===== Tarjeta "Children": afecta a los HIJOS del seleccionado. El padding encoge el
    // area donde se enganchan las anclas de bordes/esquinas (la linea transparente se ve en
    // el Editor 2D cuando el UI esta seleccionado). =====
    propHijos = new GroupPropertie(T("Children"));
    // el padding se maneja con UN valor (uniforme, default) o POR LADO (checkbox)
    propHijosPadUni = new PropBool(T("Uniform"));
    propHijosPadUni->onChange = AccionHijosRefrescar;
    propHijos->properties.push_back(propHijosPadUni);
    propHijosPadTodos = new PropFloat("Padding", "px");
    propHijosPadTodos->SetRango(0.0f, 2048.0f);
    propHijos->properties.push_back(propHijosPadTodos);
    propHijosPadIzq = new PropFloat("Pad izq", "px");
    propHijosPadIzq->SetRango(0.0f, 2048.0f);
    propHijos->properties.push_back(propHijosPadIzq);
    propHijosPadDer = new PropFloat("Pad der", "px");
    propHijosPadDer->SetRango(0.0f, 2048.0f);
    propHijos->properties.push_back(propHijosPadDer);
    propHijosPadArr = new PropFloat("Pad arriba", "px");
    propHijosPadArr->SetRango(0.0f, 2048.0f);
    propHijos->properties.push_back(propHijosPadArr);
    propHijosPadAba = new PropFloat("Pad abajo", "px");
    propHijosPadAba->SetRango(0.0f, 2048.0f);
    propHijos->properties.push_back(propHijosPadAba);
    // como se ACOMODAN los hijos: libremente (default) o en filas/columnas (se reparten
    // el 100% del area interior y su posicion deja de editarse)
    propHijosLayout = new PropButton(T("Layout"));
    propHijosLayout->conLabel = true;
    propHijosLayout->button->desplegable = true;
    propHijosLayout->action = AccionMenuHijosLayout;
    propHijos->properties.push_back(propHijosLayout);
    // como se REPARTEN: estirar (100% por peso) o minimo (tamano natural + Expandir)
    propHijosAjuste = new PropButton(T("Fit"));
    propHijosAjuste->conLabel = true;
    propHijosAjuste->button->desplegable = true;
    propHijosAjuste->action = AccionMenuHijosAjuste;
    propHijos->properties.push_back(propHijosAjuste);
    propHijosAlign = new PropButton(T("Align"));
    propHijosAlign->conLabel = true;
    propHijosAlign->button->desplegable = true;
    propHijosAlign->action = AccionMenuHijosAlign;
    propHijos->properties.push_back(propHijosAlign);
    propHijosDistrib = new PropButton(T("Distribution"));
    propHijosDistrib->conLabel = true;
    propHijosDistrib->button->desplegable = true;
    propHijosDistrib->action = AccionMenuHijosDistrib;
    propHijos->properties.push_back(propHijosDistrib);
    propHijosGap = new PropFloat(T("Gap"), "px");
    propHijosGap->SetRango(0.0f, 1024.0f);
    propHijos->properties.push_back(propHijosGap);
    // unidad del padding y el gap: pixeles (default) o proporcional al lado menor
    propHijosPx = new PropBool(T("Pixels"));
    propHijosPx->onChange = AccionHijosPxToggle;
    propHijos->properties.push_back(propHijosPx);
    // OVERFLOW (como css): recortar lo que se sale del area, por eje; y scroll opcional
    propHijosClipX = new PropBool(T("Overflow X"));
    propHijosClipX->onChange = AccionHijosRefrescar;
    propHijos->properties.push_back(propHijosClipX);
    propHijosClipY = new PropBool(T("Overflow Y"));
    propHijosClipY->onChange = AccionHijosRefrescar;
    propHijos->properties.push_back(propHijosClipY);
    propHijosScroll = new PropBool(T("Scroll"));
    propHijosScroll->onChange = AccionHijosRefrescar;
    propHijos->properties.push_back(propHijosScroll);
    propHijosScrollX = new PropFloat(T("Scroll X"), "px");
    propHijos->properties.push_back(propHijosScrollX);
    propHijosScrollY = new PropFloat(T("Scroll Y"), "px");
    propHijos->properties.push_back(propHijosScrollY);
    // ===== Tarjeta "Margen" (del elemento 2D): su aire ALREDEDOR cuando esta en una
    // fila/columna del padre, y el checkbox "Expandir" (absorbe el espacio sobrante,
    // como el elemento Expandir: aprovecha el hueco muerto). Uniforme = un solo valor.
    propMargen = new GroupPropertie(T("Margin"));
    propMargExp = new PropBool(T("Expand"));
    propMargen->properties.push_back(propMargExp);
    propMargUni = new PropBool(T("Uniform"));
    propMargUni->onChange = AccionHijosRefrescar;
    propMargen->properties.push_back(propMargUni);
    propMargTodos = new PropFloat(T("Margin"), "px");
    propMargTodos->SetRango(0.0f, 2048.0f);
    propMargen->properties.push_back(propMargTodos);
    propMargIzq = new PropFloat("Marg izq", "px");
    propMargIzq->SetRango(0.0f, 2048.0f);
    propMargen->properties.push_back(propMargIzq);
    propMargDer = new PropFloat("Marg der", "px");
    propMargDer->SetRango(0.0f, 2048.0f);
    propMargen->properties.push_back(propMargDer);
    propMargArr = new PropFloat("Marg arriba", "px");
    propMargArr->SetRango(0.0f, 2048.0f);
    propMargen->properties.push_back(propMargArr);
    propMargAba = new PropFloat("Marg abajo", "px");
    propMargAba->SetRango(0.0f, 2048.0f);
    propMargen->properties.push_back(propMargAba);

    GroupProperties.push_back(propMargen);   // Margen arriba de Hijos (es del elemento)
    GroupProperties.push_back(propHijos);
    GroupProperties.push_back(propPaletaObj); // la seleccion de paleta del objeto
    GroupProperties.push_back(propPaleta);   // la gestion de paletas del proyecto (pestania 0)
    GroupProperties.push_back(propControl);  // el Control y sus scripts, al final
    for (int i = 0; i < kMaxScriptCards; i++)
        GroupProperties.push_back(propScriptCards[i]);

    // ===== Tarjeta "Mesh Parts": selector (lista) + gestion de la PARTE (sin material) =====
    propMeshParts = new GroupPropertie(T("Mesh Parts"));
    propMeshParts->anchoValores = 0.30f;
    propMeshParts->properties.push_back(new PropListMeshParts("Mesh Parts")); // [0] selector (lo lee Rebind)
    PropButton* pbNewPart = new PropButton(T("New Mesh Part"), IconType::mesh);
    pbNewPart->action = AccionNuevoMeshPart;      propMeshParts->properties.push_back(pbNewPart);
    // fila: Assign | Select | Deselect (sin icono, 33% c/u, auto-ancho con gap)
    propRowPartOps = new PropButtonRow();
    propRowPartOps->Agregar(T("Assign"),   AccionAssignMeshPart);
    propRowPartOps->Agregar(T("Select"),   AccionSelectMeshPart);
    propRowPartOps->Agregar(T("Deselect"), AccionDeselectMeshPart);
    propMeshParts->properties.push_back(propRowPartOps);
    // fila: Delete | Rename (sin icono, 50% c/u). Delete se oculta si hay 1 sola parte (no borrable).
    propRowDelRen = new PropButtonRow();
    propRowDelRen->Agregar(T("Delete"), AccionBorrarMeshPart);
    propRowDelRen->Agregar(T("Rename"), AccionRenameMeshPart); // el boton Rename se vuelve input al apretarlo
    propMeshParts->properties.push_back(propRowDelRen);
    // fila: Move Up | Move Down (oculta si hay 1 sola parte). El ORDEN del mesh part = ORDEN DE DIBUJADO
    // (dibujar los solidos primero y los transparentes al final).
    propRowPartMove = new PropButtonRow();
    propRowPartMove->Agregar(T("Move Up"),   AccionMeshPartUp);
    propRowPartMove->Agregar(T("Move Down"), AccionMeshPartDown);
    propMeshParts->properties.push_back(propRowPartMove);
    GroupProperties.push_back(propMeshParts);

    // ===== Tarjeta APARTE "Material". Orden: New Material + Rename Material (las opciones
    // del material), LINEA separadora, y abajo la textura + sus opciones. Los props se guardan en arrays
    // (propMatChk/propMatCol/propMatShin) -> Rebind los setea por nombre, NO por indice (reordenar = libre).
    propMaterial = new GroupPropertie("Material");
    propMaterial->anchoValores = 0.30f;
    propBtnNewMaterial = new PropButton(T("New Material"), IconType::material);
    propBtnNewMaterial->button->desplegable = true;
    propBtnNewMaterial->action = AccionMenuMateriales;
    propMaterial->properties.push_back(propBtnNewMaterial);
    propBtnRenameMat = new PropButton(T("Rename Material"), -1); // ANTES de la textura, SIN icono
    propBtnRenameMat->action = AccionRenameMaterial; // se vuelve input al apretarlo; oculto si es el default
    propMaterial->properties.push_back(propBtnRenameMat);
    // aviso cuando el mesh part usa el material POR DEFECTO (oculto si tiene uno propio). 1 label WRAP: se
    // adapta al ancho (salto de linea en los espacios) -> se lee entero aunque se achique el panel.
    propMsgDefault = new PropLabel("The default material can not be edited. Create a new material.", true /*wrap*/);
    propMaterial->properties.push_back(propMsgDefault);
    // LINEA: separa las opciones del material (arriba) de la textura + sus opciones (abajo). Se OCULTA con el
    // material por defecto (sino queda una linea huerfana molesta debajo del aviso).
    propSepMat = new PropSeparator();
    propMaterial->properties.push_back(propSepMat);
    propBtnTextura = new PropButton(T("No Texture"), IconType::textura);
    propBtnTextura->button->desplegable = true;
    propBtnTextura->action = AccionMenuTexturas;
    propMaterial->properties.push_back(propBtnTextura);
    // Se CONSTRUYE todo primero (el bind es por member, no importa el orden de construccion); el PUSH define el
    // orden VISUAL, que se reorganizo: Lighting arriba de todo, Vertex Color sobre Base Color, y los
    // "pro" (Culling / Depth Test) abajo de todo.
    const char* nombresCol[3] = { "Base Color","Specular","Emission" };
    for (int i = 0; i < 3; i++) propMatCol[i] = new PropColor(nombresCol[i]);
    propMatShin = new PropFloat(T("Shininess"));
    propMatShin->SetRango(0.0f, 255.0f);
    propMatShin->stepFino = 1.0f; propMatShin->stepGrueso = 10.0f; propMatShin->dragStep = 1.0f;
    propMatShin->entero = true;   // es un entero (tiene que ser entero), no float
    propMatShin->acelera = true;  // izq/der arranca en 1 y acelera (empieza lento y despues acelera)
    // [8]="Reflection"; [9] SIN uso (lo reemplaza el dropdown de modo -> oculto en Rebind); [10]="Normal Mapping".
    const char* nombresChk[11] = { "Filtering","Transparent","Vertex Color","Lighting","Repeat","Culling","Depth Test","Smooth Shading","Reflection","(reflect mode)","Normal Mapping" };
    for (int i = 0; i < 11; i++) {
        propMatChk[i] = new PropBool(nombresChk[i]);
        // onChange = re-Rebind: togglear CUALQUIER checkbox re-arma la tarjeta -> aparecen/desaparecen al instante los
        // que dependen de otro (Base Color si Vertex Color; Shininess/Emission/Specular si Lighting; etc).
        propMatChk[i]->onChange = RebindMaterialMeshPart;
    }
    propBtnNormalTex = new PropButton(T("No Normal Map"), IconType::textura);
    propBtnNormalTex->button->desplegable = true;
    propBtnNormalTex->action = AccionMenuTexturasNormal;
    propBtnReflectMode = new PropButton("Matcap (hardware)", IconType::material);
    propBtnReflectMode->button->desplegable = true;
    propBtnReflectMode->action = AccionMenuReflectMode;
    // --- calcomanias / mezcla / profundidad (ver el bloque de arriba) ---
    propMatDecal = new PropBool("Decal");
    propMatDecal->onChange = OnMatDecalChange;
    propBtnMezcla = new PropButton("Alpha (normal)", IconType::material);
    propBtnMezcla->button->desplegable = true;
    propBtnMezcla->action = AccionMenuMezcla;
    propBtnProfundidad = new PropButton("Test + Write (opaque)", IconType::material);
    propBtnProfundidad->button->desplegable = true;
    propBtnProfundidad->action = AccionMenuProfundidad;
    propMatSesgo = new PropFloat("Depth Bias");
    propMatSesgo->SetRango(-64.0f, 64.0f);
    propMatSesgo->stepFino = 1.0f; propMatSesgo->stepGrueso = 4.0f; propMatSesgo->dragStep = 0.5f;
    propMatSesgo->onChange = OnMatSesgoChange;
    propMatOrden = new PropFloat("Pass Order");
    propMatOrden->SetRango(0.0f, 2.0f);
    propMatOrden->entero = true;
    propMatOrden->stepFino = 1.0f; propMatOrden->stepGrueso = 1.0f; propMatOrden->dragStep = 1.0f;
    propMatOrden->onChange = OnMatOrdenChange;
    // --- LINEAS: aristas de la malla con este material + grosor en px ---
    propMatLineas = new PropBool("Lines");
    propMatLineas->onChange = OnMatLineasChange;
    propMatGrosorLinea = new PropFloat("Line Width");
    propMatGrosorLinea->SetRango(1.0f, 64.0f);
    propMatGrosorLinea->stepFino = 1.0f; propMatGrosorLinea->stepGrueso = 2.0f; propMatGrosorLinea->dragStep = 0.5f;
    propMatGrosorLinea->onChange = OnMatGrosorLineaChange;
    // --- ORDEN VISUAL ---
    propMaterial->properties.push_back(propMatChk[3]);  // Lighting  (ARRIBA DE TODO)
    propMaterial->properties.push_back(propMatChk[2]);  // Vertex Color (sobre Base Color)
    propMaterial->properties.push_back(propMatCol[0]);  // Base Color (se oculta si Vertex Color ON)
    propMaterial->properties.push_back(propMatCol[1]);  // Specular  (se oculta si Lighting OFF)
    propMaterial->properties.push_back(propMatCol[2]);  // Emission  (se oculta si Lighting OFF)
    propMaterial->properties.push_back(propMatShin);    // Shininess (se oculta si Lighting OFF)
    propMaterial->properties.push_back(propMatChk[0]);  // Filtering
    propMaterial->properties.push_back(propMatChk[1]);  // Transparent
    propMaterial->properties.push_back(propMatChk[4]);  // Repeat
    propMaterial->properties.push_back(propMatChk[10]); // Normal Mapping
    propMaterial->properties.push_back(propBtnNormalTex);
    propMaterial->properties.push_back(propMatChk[8]);  // Reflection
    propMaterial->properties.push_back(propMatChk[9]);  // (oculto: reemplazado por el dropdown)
    propMaterial->properties.push_back(propBtnReflectMode);
    propMaterial->properties.push_back(propBtnMezcla);  // modo de mezcla (visible si Transparent ON)
    propMaterial->properties.push_back(propMatLineas);      // Lines (aristas por material)
    propMaterial->properties.push_back(propMatGrosorLinea); // Line Width (visible con Lines ON)
    propMaterial->properties.push_back(propMatChk[5]);  // Culling    (ABAJO DE TODO: pro)
    // los "pro" de calcomania: el tilde que aplica la receta y sus tres piezas sueltas.
    // Van al final, debajo de Culling, para no cambiar de lugar nada de lo que ya estaba.
    propMaterial->properties.push_back(propMatDecal);       // Decal (preset)
    propMaterial->properties.push_back(propBtnProfundidad); // Depth Test/Write (desplegable)
    propMaterial->properties.push_back(propMatSesgo);       // Depth Bias
    propMaterial->properties.push_back(propMatOrden);       // Pass Order
    // el viejo checkbox "Depth Test" queda OCULTO: lo reemplaza el desplegable de arriba,
    // que ademas expone la escritura de z (la mitad que faltaba).
    propMaterial->properties.push_back(propMatChk[6]);
    GroupProperties.push_back(propMaterial);

    // pestania de LUZ: TODAS las propiedades editables de la luz de OpenGL. Se ve solo si el
    // objeto activo es una luz. OpenGL = UN tipo de luz configurable: Direccional / Puntual / Spot (ver Light.h).
    propLight = new GroupPropertie(T("Light"));
    propLight->reservaKeyBtn = true;   // columna del rombo (el color diffuse es animable)
    propLightDir = new PropBool(T("Directional"));                 // w=0 (sol) vs puntual/spot
    propLightDir->animProp = AnimLightMisc; propLightDir->animComp = AnimY;   // animable (escalon)
    propLight->properties.push_back(propLightDir);
    propLightGL = new PropFloat(T("GL Light"));                    // numero de GL light (0..7), entero editable
    propLightGL->SetRango(0.0f, 7.0f); propLightGL->entero = true;
    propLightGL->stepFino = 1.0f; propLightGL->stepGrueso = 1.0f; propLightGL->dragStep = 1.0f;
    propLightGL->onChange = OnLightGLChange;
    propLightGL->animProp = AnimLightMisc; propLightGL->animComp = AnimX;     // animable (escalon)
    propLight->properties.push_back(propLightGL);
    propLightDiffuse = new PropColor(T("Diffuse"));  propLight->properties.push_back(propLightDiffuse);
    propLightDiffuse->animProp = AnimColor; propLightDiffuse->animComp = AnimX; // rombo -> anima el color (RGB)
    propLightAmbient = new PropColor(T("Ambient"));  propLight->properties.push_back(propLightAmbient);
    propLightAmbient->animProp = AnimAmbient; propLightAmbient->animComp = AnimX;
    propLightSpecular = new PropColor(T("Specular")); propLight->properties.push_back(propLightSpecular);
    propLightSpecular->animProp = AnimSpecular; propLightSpecular->animComp = AnimX;
    // atenuacion 1/(C + L*d + Q*d^2) (afecta a la puntual/spot)
    propLightAttC = new PropFloat(T("Att Constant")); propLightAttC->SetRango(0.0f, 5.0f);
    propLightAttC->stepFino = 0.02f; propLightAttC->stepGrueso = 0.1f; propLightAttC->dragStep = 0.01f;
    propLightAttC->animProp = AnimAtten; propLightAttC->animComp = AnimX;
    propLight->properties.push_back(propLightAttC);
    propLightAttL = new PropFloat(T("Att Linear")); propLightAttL->SetRango(0.0f, 2.0f);
    propLightAttL->stepFino = 0.01f; propLightAttL->stepGrueso = 0.05f; propLightAttL->dragStep = 0.005f;
    propLightAttL->animProp = AnimAtten; propLightAttL->animComp = AnimY;
    propLight->properties.push_back(propLightAttL);
    propLightAttQ = new PropFloat(T("Att Quadratic")); propLightAttQ->SetRango(0.0f, 1.0f);
    propLightAttQ->stepFino = 0.005f; propLightAttQ->stepGrueso = 0.02f; propLightAttQ->dragStep = 0.002f;
    propLightAttQ->animProp = AnimAtten; propLightAttQ->animComp = AnimZ;
    propLight->properties.push_back(propLightAttQ);
    // spotlight: cono (grados) + concentracion del haz
    propLightSpotCut = new PropFloat(T("Spot Cutoff")); propLightSpotCut->SetRango(1.0f, 180.0f); propLightSpotCut->entero = true;
    propLightSpotCut->stepFino = 1.0f; propLightSpotCut->stepGrueso = 5.0f; propLightSpotCut->dragStep = 1.0f;
    propLightSpotCut->animProp = AnimSpot; propLightSpotCut->animComp = AnimX;
    propLight->properties.push_back(propLightSpotCut);
    propLightSpotExp = new PropFloat(T("Spot Exponent")); propLightSpotExp->SetRango(0.0f, 128.0f); propLightSpotExp->entero = true;
    propLightSpotExp->stepFino = 1.0f; propLightSpotExp->stepGrueso = 8.0f; propLightSpotExp->dragStep = 1.0f;
    propLightSpotExp->animProp = AnimSpot; propLightSpotExp->animComp = AnimY;
    propLight->properties.push_back(propLightSpotExp);
    GroupProperties.push_back(propLight);

    // pestania de CAMARA: lente (ortografica/perspectiva + fov) + target (look-at)
    propCamera = new GroupPropertie(T("Camera"));
    propCamera->reservaKeyBtn = true;   // columna del rombo de keyframe (el fov es animable)
    propCamOrtho = new PropBool(T("Orthographic"));
    propCamera->properties.push_back(propCamOrtho);
    propCamFov = new PropFloat(T("FOV"));
    propCamFov->SetRango(1.0f, 179.0f); propCamFov->entero = false;
    propCamFov->stepFino = 0.5f; propCamFov->stepGrueso = 5.0f; propCamFov->dragStep = 0.5f;
    propCamFov->animProp = AnimFov; propCamFov->animComp = AnimX;   // rombo de keyframe -> anima el FOV
    propCamera->properties.push_back(propCamFov);
    // distancia MINIMA / MAXIMA de dibujado (near/far), animables (canal AnimClip)
    propCamNear = new PropFloat(T("Near"));
    propCamNear->SetRango(0.001f, 100.0f); propCamNear->stepFino = 0.01f; propCamNear->stepGrueso = 0.1f; propCamNear->dragStep = 0.005f;
    propCamNear->animProp = AnimClip; propCamNear->animComp = AnimX;
    propCamera->properties.push_back(propCamNear);
    propCamFar = new PropFloat(T("Far"));
    propCamFar->SetRango(1.0f, 100000.0f); propCamFar->stepFino = 5.0f; propCamFar->stepGrueso = 50.0f; propCamFar->dragStep = 2.0f;
    propCamFar->animProp = AnimClip; propCamFar->animComp = AnimY;
    propCamera->properties.push_back(propCamFar);
    propBtnCamTarget = new PropButton(T("Target"), IconType::object);
    propBtnCamTarget->button->desplegable = true;
    propBtnCamTarget->action = AccionMenuTarget;
    propCamera->properties.push_back(propBtnCamTarget);
    GroupProperties.push_back(propCamera);

    // pestania de los objetos especiales (Duplicate Linked / Array / Mirror):
    // el objeto al que apuntan (target)
    propInstance = new GroupPropertie(T("Linked"));
    propBtnInstTarget = new PropButton(T("Target"), IconType::object);
    propBtnInstTarget->button->desplegable = true;
    propBtnInstTarget->action = AccionMenuTarget;
    propInstance->properties.push_back(propBtnInstTarget);
    GroupProperties.push_back(propInstance);

    // pestania del objeto LOD: los umbrales de distancia, como texto "20, 45, 90"
    // (umbral i = hasta donde llega el hijo i; mas alla del ultimo, el ultimo hijo).
    // El commit lo hace SincronizarLodDist por frame (patron del texto 2D).
    propLOD = new GroupPropertie("LOD");
    propLOD->anchoValores = 0.62f;   // columna de valor ancha (la lista de numeros)
    propLodDist = new PropText(T("Distances"), "");
    propLOD->properties.push_back(propLodDist);
    // MISMO flag que el Culling: sin el, LOD y Culling previsualizan con camaras
    // distintas (el LOD leia la vista del ultimo viewport dibujado).
    propLodSoloCam = new PropBool(T("Only active camera"));
    propLOD->properties.push_back(propLodSoloCam);
    GroupProperties.push_back(propLOD);

    // pestania del objeto Culling: el checkbox bindea DIRECTO al campo del activo
    propCulling = new GroupPropertie("Culling");
    // INTERRUPTOR del recorte: primero de la lista porque es el que se toca en vivo
    // (demo A/B: apagar y ver TODO el escenario, prender y ver desaparecer lo que
    // queda fuera del marco 4:3 de la camara del juego).
    propCullActivo = new PropBool(T("Active"));
    propCulling->properties.push_back(propCullActivo);
    propCullSoloCam = new PropBool(T("Only active camera"));
    propCulling->properties.push_back(propCullSoloCam);
    // culling por DISTANCIA (0 = sin limite): el corte que el frustum no puede
    // hacer en un nivel "pasillo" (ver Culling.h)
    propCullDistMax = new PropFloat(T("Max distance"), "m");
    propCullDistMax->SetRango(0.0f, 100000.0f);
    propCulling->properties.push_back(propCullDistMax);
    GroupProperties.push_back(propCulling);

    // pestania del objeto Particulas: la config del emisor. Los numeros/checks
    // bindean directo a los campos del activo; textura y color son de TEXTO
    // (commit en vivo, ver SincronizarPartTextura/SincronizarPartColor)
    propParticulas = new GroupPropertie("Particles");
    propParticulas->anchoValores = 0.62f;   // columna ancha (la ruta de la textura)
    propPartTextura = new PropText(T("Texture"), "");
    propParticulas->properties.push_back(propPartTextura);
    propPartCantidad = new PropFloat(T("Rate"), "p/s");     // 0 = solo rafagas emitir()
    propPartCantidad->SetRango(0.0f, 4096.0f);
    propParticulas->properties.push_back(propPartCantidad);
    propPartVida = new PropFloat(T("Lifetime"), "s");
    propPartVida->SetRango(0.01f, 600.0f);
    propParticulas->properties.push_back(propPartVida);
    propPartTam = new PropFloat(T("Size"), "m");            // lado del billboard, en mundo
    propPartTam->SetRango(0.0f, 1000.0f);
    propParticulas->properties.push_back(propPartTam);
    propPartVel = new PropFloat(T("Velocity"), "m/s");
    propPartVel->SetRango(0.0f, 10000.0f);
    propParticulas->properties.push_back(propPartVel);
    propPartDispersion = new PropFloat(T("Spread"), "deg"); // apertura TOTAL del cono
    propPartDispersion->SetRango(0.0f, 360.0f);
    propParticulas->properties.push_back(propPartDispersion);
    propPartGravedad = new PropFloat(T("Gravity"), "");     // + cae / - sube
    propPartGravedad->SetRango(-1000.0f, 1000.0f);
    propParticulas->properties.push_back(propPartGravedad);
    propPartVariacion = new PropFloat(T("Variation"), "");  // 0..1: jitter por particula (vel/vida/tam)
    propPartVariacion->SetRango(0.0f, 1.0f);
    propParticulas->properties.push_back(propPartVariacion);
    propPartTurbulencia = new PropFloat(T("Turbulence"), ""); // deriva suave por particula (unid/s^2)
    propPartTurbulencia->SetRango(0.0f, 1000.0f);
    propParticulas->properties.push_back(propPartTurbulencia);
    propPartRotacion = new PropBool(T("Rotation"));           // angulo azaroso fijo al nacer
    propParticulas->properties.push_back(propPartRotacion);
    propPartVelRot = new PropFloat(T("Spin"), "deg/s");       // giro continuo, signo azaroso
    propPartVelRot->SetRango(-3600.0f, 3600.0f);
    propParticulas->properties.push_back(propPartVelRot);
    propPartColor = new PropText(T("Color"), "");           // "r, g, b, a"
    propParticulas->properties.push_back(propPartColor);
    propPartAditivo = new PropBool(T("Additive"));
    propParticulas->properties.push_back(propPartAditivo);
    // SUSTRACTIVA (dst - src): oscurece. Es la mezcla del humo y el polvo del
    // originales de PS1, que con alpha o aditiva no se pueden hacer. Gana sobre
    // "Additive" si las dos estan marcadas (ver Particulas.h).
    propPartSustractivo = new PropBool(T("Subtractive"));
    propParticulas->properties.push_back(propPartSustractivo);
    propPartDesvanecer = new PropBool(T("Fade out"));
    propParticulas->properties.push_back(propPartDesvanecer);
    propPartActivo = new PropBool(T("Active"));
    propParticulas->properties.push_back(propPartActivo);
    GroupProperties.push_back(propParticulas);

    // pestania de la Collection: orden de dibujo de los hijos para TRANSPARENTES
    // (lejos -> cerca respecto de la camara; ver Collection.h)
    propCollection = new GroupPropertie("Collection");
    propCollOrdenCam    = new PropBool(T("Sort by camera"));
    propCollOrdenUnaVez = new PropBool(T("Sort once (active camera)"));
    propCollection->properties.push_back(propCollOrdenCam);
    propCollection->properties.push_back(propCollOrdenUnaVez);
    GroupProperties.push_back(propCollection);

    // ===== pestania RENDER: tarjeta "Render" (arriba) + tarjeta "Export" (abajo) =====
    // ===== tarjeta ARCHIVO (pestania Render, PRIMERA): el proyecto .w3d =====
    propArchivo = new GroupPropertie("Archivo");
    propArchivo->anchoValores = 0.62f;   // columna ancha (paths)
    propProyAbrir = new PropButton("Abrir proyecto", IconType::carpeta);
    propProyAbrir->action = AccionProyAbrir;
    propArchivo->properties.push_back(propProyAbrir);
    { std::string dir, nom;   // al abrir por doble click w3dPath ya esta seteado
      size_t sb = w3dPath.find_last_of("/\\");
      if (sb != std::string::npos) { dir = w3dPath.substr(0, sb); nom = w3dPath.substr(sb + 1); }
      else nom = w3dPath;
      propProyCarpeta = new PropText("Carpeta", dir);
      propProyCarpeta->onClick = AccionBrowseProyCarpeta; // el campo Carpeta ES el "Browse folder"
      propArchivo->properties.push_back(propProyCarpeta);
      propProyNombre = new PropText("Nombre", nom); }
    propArchivo->properties.push_back(propProyNombre);
    // (el checkbox "Empaquetar assets" viejo se elimino: los assets del PROYECTO
    // son siempre archivos externos; empaquetar es del juego COMPILADO, tarjeta Juego)
    propProyGuardar = new PropButton("Guardar", IconType::guardar);
    propProyGuardar->action = AccionProyGuardar;
    propArchivo->properties.push_back(propProyGuardar);
    // GUARDADO POR VERSIONES: guarda normal + deja <proyecto>_vNN.w3d al lado
    // (el label muestra el N real; se refresca en ProyectoSincronizarCampos)
    propProyVersion = new PropButton(GuardarVersionLabel(), IconType::guardar);
    propProyVersion->action = AccionProyVersion;
    propArchivo->properties.push_back(propProyVersion);
    propProyComo = new PropButton("Guardar como", IconType::guardar);
    propProyComo->action = AccionProyComo;
    propArchivo->properties.push_back(propProyComo);
    // EXTRAER: los assets de adentro del .w3d a una carpeta, para editarlos afuera
    propProyExtraer = new PropButton("Extraer assets", IconType::carpeta);
    propProyExtraer->action = AccionProyExtraer;
    propArchivo->properties.push_back(propProyExtraer);
    GroupProperties.push_back(propArchivo);


    propRender = new GroupPropertie("Render");
    propRender->anchoValores = 0.62f; // columna de valor ANCHA (paths)
    // salida partida en dos campos: Path (carpeta) + File name (nombre.png).
    // Default del path: carpeta de salida por defecto (Android = Descargas).
    propRenderPath = new PropText(T("Path"), w3dFileSystem::GetDefaultOutputDir());
    propRenderPath->onClick = AccionBrowseRender; // el campo Path ES el "Browse folder": al clickear abre el explorador
    propRender->properties.push_back(propRenderPath);
    propRenderOutput = new PropText("Nombre", "render.png");
    propRender->properties.push_back(propRenderOutput);
    // resolucion editable (default 640x480). Puede ser MAYOR que la ventana: se rinde por tiles.
    renderW = 640.0f; renderH = 480.0f;
    g_renderAspect = renderW / renderH; // arranca con el aspecto por defecto (4:3)
    propRenderW = new PropFloat(T("Width"));
    propRenderW->SetRango(1.0f, 8192.0f); propRenderW->entero = true;
    propRenderW->stepFino = 1.0f; propRenderW->stepGrueso = 16.0f; propRenderW->dragStep = 1.0f;
    propRenderW->value = &renderW; propRenderW->onChange = ActualizarAspectoRender; // geometria de camaras responsive
    propRender->properties.push_back(propRenderW);
    propRenderH = new PropFloat(T("Height"));
    propRenderH->SetRango(1.0f, 8192.0f); propRenderH->entero = true;
    propRenderH->stepFino = 1.0f; propRenderH->stepGrueso = 16.0f; propRenderH->dragStep = 1.0f;
    propRenderH->value = &renderH; propRenderH->onChange = ActualizarAspectoRender;
    propRender->properties.push_back(propRenderH);
    // (el FPS de reproduccion se movio a la tarjeta Animation, junto a Start/End)
    // pases EXTRA a exportar (el beauty/render siempre se guarda). Nombre: base_zbuffer_0001.png, etc.
    renderZbuffer = false; renderNormal = false; renderAlpha = false;
    propRenderZbuffer = new PropBool("ZBuffer"); propRenderZbuffer->value = &renderZbuffer;
    propRender->properties.push_back(propRenderZbuffer);
    propRenderNormal = new PropBool("Normal"); propRenderNormal->value = &renderNormal;
    propRender->properties.push_back(propRenderNormal);
    propRenderAlpha = new PropBool("Alpha"); propRenderAlpha->value = &renderAlpha;
    propRender->properties.push_back(propRenderAlpha);
    // color de FONDO del render (global g_renderBg, solo para el pase Rendered). Se edita con el color picker.
    propRenderBg = new PropColor(T("Background"));
    propRenderBg->value = g_renderBg; // el array global decae a puntero (igual que los colores de material/luz)
    propRender->properties.push_back(propRenderBg);
    // boton con action real (antes era no-op)
    PropButton* pbRenderImg = new PropButton(T("Render Image"), IconType::foto); // foto: renderiza una imagen
    pbRenderImg->action = AccionRenderImage;
    propRender->properties.push_back(pbRenderImg);
    GroupProperties.push_back(propRender);

    // tarjeta "Animation" (pestania Render): selector de la animacion ACTIVA (Scene(s) / clips del armature) + Start/End/
    // FPS + New|Delete + Rename + Render Animation (rendea la SECUENCIA StartFrame..EndFrame). Delete se oculta sin nada
    // que borrar y Render se grisa sin animaciones.
    propAnimation = new GroupPropertie(T("Animation"));
    propAnimation->anchoValores = 0.55f; // Start/End/FPS son campos numericos: mas lugar al valor
    propBtnAnimSel = new PropButton(T("Scene"), IconType::camera); // dropdown: animacion activa (Scene por defecto)
    propBtnAnimSel->button->desplegable = true;
    propBtnAnimSel->button->caretMenu = true; // aca SI conviene la flechita (no es obvio que es un selector)
    propBtnAnimSel->action = AccionMenuAnimSel;
    propAnimation->properties.push_back(propBtnAnimSel);
    // Start / End / FPS de la animacion (espejos float de los int StartFrame/EndFrame/AnimFPS)
    { PropFloat* pS = new PropFloat(T("Start"));
      pS->SetRango(0.0f, 100000.0f); pS->entero = true; pS->stepFino = 1.0f; pS->stepGrueso = 10.0f; pS->dragStep = 1.0f;
      g_animStartF = (float)StartFrame; pS->value = &g_animStartF; pS->onChange = AccionAnimStart; gPropAnimStart = pS;
      propAnimation->properties.push_back(pS); }
    { PropFloat* pE = new PropFloat(T("End"));
      pE->SetRango(1.0f, 100000.0f); pE->entero = true; pE->stepFino = 1.0f; pE->stepGrueso = 10.0f; pE->dragStep = 1.0f;
      g_animEndF = (float)EndFrame; pE->value = &g_animEndF; pE->onChange = AccionAnimEnd; gPropAnimEnd = pE;
      propAnimation->properties.push_back(pE); }

    { PropFloat* pF = new PropFloat("FPS");
      pF->SetRango(1.0f, 120.0f); pF->entero = true; pF->stepFino = 1.0f; pF->stepGrueso = 5.0f; pF->dragStep = 1.0f;
      g_animFpsF = (float)AnimFPS; pF->value = &g_animFpsF; pF->onChange = AccionAnimFps; gPropAnimFps = pF;
      propAnimation->properties.push_back(pF); }
    { PropFloat* pV = new PropFloat("Velocidad");
      pV->SetRango(0.01f, 20.0f); pV->stepFino = 0.05f; pV->stepGrueso = 0.25f; pV->dragStep = 0.05f;
      pV->value = NULL;   // solo visible con una VERTEX ANIM activa (kind 3); lo maneja SincronizarAnimFps
      pV->onChange = AccionVertVel; gPropVertVel = pV;
      propAnimation->properties.push_back(pV); }
    // New | Delete en UNA fila
    propRowAnimNewDel = new PropButtonRow();
    propRowAnimNewDel->Agregar(T("New"), AccionAnimNewCard, IconType::armature);
    propRowAnimNewDel->Agregar(T("Duplicate"), AccionAnimDupCard); // duplica el clip activo (se oculta sin clips)
    propRowAnimNewDel->Agregar(T("Delete"), AccionAnimDelCard, IconType::borrar);
    propAnimation->properties.push_back(propRowAnimNewDel);
    // Rename de la animacion activa (escena o clip): el boton se vuelve input in-place
    propBtnAnimRename = new PropButton(T("Rename"), -1);
    propBtnAnimRename->action = AccionAnimRenameCard;
    propAnimation->properties.push_back(propBtnAnimRename);
    propBtnAnimRender = new PropButton(T("Render Animation"), IconType::camera);
    propBtnAnimRender->action = AccionRenderAnimation;
    propAnimation->properties.push_back(propBtnAnimRender);
    GroupProperties.push_back(propAnimation);

    // ===== Tarjeta "Juego" (debajo de Animacion): compilar + el cache del viaje en
    // el tiempo. El juego NO se mezcla con la UI ni con las animaciones normales.
    propJuego = new GroupPropertie("Juego");
    propJuegoPlat = new PropButton("Plataforma");
    propJuegoPlat->conLabel = true;
    propJuegoPlat->button->desplegable = true;
    propJuegoPlat->button->text = NombrePlat(g_proyCompilar.plataforma);
    propJuegoPlat->action = AccionMenuPlat;
    propJuego->properties.push_back(propJuegoPlat);
    propJuegoModoVent = new PropButton("Modo ventana");
    propJuegoModoVent->conLabel = true;
    propJuegoModoVent->button->desplegable = true;
    propJuegoModoVent->button->text = NombreModoVent(g_proyCompilar.modoVentana);
    propJuegoModoVent->action = AccionMenuModoVent;
    propJuego->properties.push_back(propJuegoModoVent);
    propJuegoOrient = new PropButton("Orientacion");
    propJuegoOrient->conLabel = true;
    propJuegoOrient->button->desplegable = true;
    propJuegoOrient->button->text = NombreOrientacion(g_proyCompilar.orientacion);
    propJuegoOrient->action = AccionMenuOrient;
    propJuego->properties.push_back(propJuegoOrient);
    // icono del juego: elegir un PNG (con alpha, maxima definicion); las versiones
    // chicas las genera Compilar juego. Se guarda en el .w3d como ruta externa.
    propJuegoIcono = new PropButton("Icono");
    propJuegoIcono->conLabel = true;
    propJuegoIcono->button->text = NombreIconoJuego();
    propJuegoIcono->action = AccionMenuIcono;
    propJuego->properties.push_back(propJuegoIcono);
    // assets del juego compilado: sueltos (editables, como siempre) o empaquetados
    // (protegidos: dentro del binario, ofuscados, sin archivos que copiar/instalar)
    propJuegoAssets = new PropButton("Assets");
    propJuegoAssets->conLabel = true;
    propJuegoAssets->button->desplegable = true;
    propJuegoAssets->button->text = NombreAssetsModo(g_proyCompilar.assetsModo);
    propJuegoAssets->action = AccionMenuAssets;
    propJuego->properties.push_back(propJuegoAssets);
    // subsistemas opcionales: destildado = el juego compilado sale SIN ese modulo
    propJuegoFisica = new PropBool("Usar motor de fisica");
    propJuegoFisica->value = &g_proyCompilar.usarFisica;
    propJuego->properties.push_back(propJuegoFisica);
    propJuegoSonido = new PropBool("Usar sonido");
    propJuegoSonido->value = &g_proyCompilar.usarSonido;
    propJuego->properties.push_back(propJuegoSonido);
    // modo debug: tildado = W3D_DEV_LOG=1 (log + ring + depurar()); destildado
    // (default) = produccion, W3D_DEV_LOG=0 (sin mensajes de debug ni ring)
    propJuegoDebug = new PropBool("Modo debug");
    propJuegoDebug->value = &g_proyCompilar.modoDebug;
    propJuego->properties.push_back(propJuegoDebug);
    propJuegoCompilar = new PropButton("Compilar juego", IconType::gamepad);
    propJuegoCompilar->action = AccionCompilarJuego;
    propJuego->properties.push_back(propJuegoCompilar);
    { PropFloat* pC = new PropFloat("Cache", "frames");
      pC->SetRango(10.0f, 100000.0f); pC->entero = true;
      pC->stepFino = 10.0f; pC->stepGrueso = 50.0f; pC->dragStep = 1.0f;
      g_simCacheF = (float)gSimCacheMax; pC->value = &g_simCacheF; pC->onChange = AccionSimCache;
      propJuego->properties.push_back(pC); }
    propAnimConservar = new PropBool("No reemplazar estados");
    propAnimConservar->value = &AnimConservarEstados;
    propJuego->properties.push_back(propAnimConservar);
    GroupProperties.push_back(propJuego);

    // ===== Tarjeta "Keyframe": el keyframe elegido en el editor de curvas, con numeros exactos =====
    // Aparece SOLO cuando hay uno elegido. X = frame (entero), Y = valor. Los handles son puntos (offset desde el
    // keyframe) y solo se pueden tipear si el tipo los guarda (Free/Aligned); con los calculados quedan grises.
    propKeyframe = new GroupPropertie("Keyframe");
    propKeyframe->icono = (int)IconType::keyframe;   // el rombo, igual que el del timeline
    propKeyframe->anchoValores = 0.55f;
    { PropFloat* p1 = new PropFloat("Frame X");
      p1->entero = true; p1->stepFino = 1.0f; p1->stepGrueso = 10.0f; p1->dragStep = 1.0f;
      p1->value = &g_kfFrame; p1->onChange = AccionKfFrame; gKfFrame = p1;
      propKeyframe->properties.push_back(p1); }
    { PropFloat* p1 = new PropFloat(T("Value Y"));
      p1->value = &g_kfValor; p1->onChange = AccionKfValor; gKfValor = p1;
      propKeyframe->properties.push_back(p1); }
    propKeyframe->properties.push_back(new PropGap(""));
    gKfInterp = new PropButton(T("Interpolation"));
    gKfInterp->button->desplegable = true; gKfInterp->button->caretMenu = true;
    gKfInterp->action = AccionKfBtnInterp;
    propKeyframe->properties.push_back(gKfInterp);
    gKfHandle = new PropButton(T("Handle Type"));
    gKfHandle->button->desplegable = true; gKfHandle->button->caretMenu = true;
    gKfHandle->action = AccionKfBtnHandle;
    propKeyframe->properties.push_back(gKfHandle);
    propKeyframe->properties.push_back(new PropGap(""));
    { PropFloat* p1 = new PropFloat(T("Left Handle X"));
      p1->value = &g_kfInDF; p1->onChange = AccionKfHandles; gKfInDF = p1;
      propKeyframe->properties.push_back(p1); }
    { PropFloat* p1 = new PropFloat("Y");
      p1->value = &g_kfInDV; p1->onChange = AccionKfHandles; gKfInDV = p1;
      propKeyframe->properties.push_back(p1); }
    { PropFloat* p1 = new PropFloat(T("Right Handle X"));
      p1->value = &g_kfOutDF; p1->onChange = AccionKfHandles; gKfOutDF = p1;
      propKeyframe->properties.push_back(p1); }
    { PropFloat* p1 = new PropFloat("Y");
      p1->value = &g_kfOutDV; p1->onChange = AccionKfHandles; gKfOutDV = p1;
      propKeyframe->properties.push_back(p1); }
    GroupProperties.push_back(propKeyframe);

    propExport = new GroupPropertie(T("Export"));
    propExport->anchoValores = 0.62f;
    // dropdown de FORMATO (arriba de todo): OBJ / FBX / glTF / GLB. La etiqueta muestra el formato activo.
    propExportFormat = new PropButton(NombreFormato(exportFormat), IconType::mesh);
    propExportFormat->button->desplegable = true;
    propExportFormat->button->caretMenu = true; // flechita: es un selector de formato
    propExportFormat->action = AccionMenuExportFormat;
    propExport->properties.push_back(propExportFormat);
    PropBool* pbSel = new PropBool(T("Selected only"));
    pbSel->value = &exportSelectedOnly;
    propExport->properties.push_back(pbSel);
    PropBool* pbMods = new PropBool(T("Apply Modifiers")); // OBJ: ON = exporta la malla generada por los modificadores
    pbMods->value = &exportApplyModifiers;
    propExport->properties.push_back(pbMods);
    PropBool* pbXf = new PropBool(T("Apply Transforms")); // OBJ: ON = hornea el transform del objeto en el .obj (mundo)
    pbXf->value = &exportApplyTransforms;
    propExport->properties.push_back(pbXf);
    // salida partida en dos: Path (carpeta) + File name (nombre + extension del formato). Default: Descargas en Android.
    propExportPath = new PropText(T("Path"), w3dFileSystem::GetDefaultOutputDir());
    propExportPath->onClick = AccionBrowseExport; // el campo Path ES el "Browse folder": al clickear abre el explorador
    propExport->properties.push_back(propExportPath);
    propExportName = new PropText(T("File name"), std::string("model") + ExtDeFormato(exportFormat));
    propExport->properties.push_back(propExportName);
    PropButton* pbExp = new PropButton(T("Export"), IconType::guardar);
    pbExp->action = AccionExport;
    propExport->properties.push_back(pbExp);
    GroupProperties.push_back(propExport);

    // ===== tarjeta "Ajustes" (ABAJO DE TODO en la pestania Render): el config.ini editable desde adentro =====
    propAjustes = new GroupPropertie(T("Settings"));
    propAjustes->anchoValores = 0.62f;

    propAjIdioma = new PropButton(T("Language"));   // label a la izquierda, el valor a la derecha
    propAjIdioma->conLabel = true;
    propAjIdioma->button->text = W3dIdiomaNombre(g_idioma);
    propAjIdioma->button->desplegable = true;
    propAjIdioma->action = AccionMenuIdioma;
    propAjustes->properties.push_back(propAjIdioma);

    propAjAntialias = new PropBool(T("Antialiasing"));
    propAjAntialias->value = &cfg.enableAntialiasing;
    propAjAntialias->onChange = AccionAntialias;
    propAjustes->properties.push_back(propAjAntialias);

    propAjBackend = new PropButton(T("Graphics"));
    propAjBackend->conLabel = true;
    propAjBackend->button->text = cfg.graphicsAPI;
    propAjBackend->button->desplegable = true;
    propAjBackend->action = AccionMenuBackend;
    propAjustes->properties.push_back(propAjBackend);

    propAjSkin = new PropButton("Skin");
    propAjSkin->conLabel = true;
    propAjSkin->button->text = cfg.SkinName;
    propAjSkin->button->desplegable = true;
    propAjSkin->action = AccionMenuSkin;
    propAjustes->properties.push_back(propAjSkin);

    // la ESCALA del editor, en vivo (x1 = N95, x3 = default PC)
    { PropFloat* pe = new PropFloat("Escala", "x");
      g_ajEscala = (float)(cfg.scale > 0 ? cfg.scale : 3);
      pe->SetRango(1.0f, 6.0f); pe->entero = true;
      pe->value = &g_ajEscala;
      pe->onChange = AccionEscalaEditor;
      propAjustes->properties.push_back(pe); }

    { PropButton* pbSave = new PropButton(T("Save Changes"), IconType::guardar);
      pbSave->action = AccionGuardarConfig;
      propAjustes->properties.push_back(pbSave); }

    GroupProperties.push_back(propAjustes);   // ULTIMA -> queda abajo de todo

    // pestana TRANSFORMAR (icono del modo de seleccion, tab 5): tarjeta "Transform Mesh" (posicion
    // X/Y/Z del centro de la seleccion, editable -> traslada rigido; antes vivia en la pestania
    // Vertices) + tarjeta "Transform UV" (centro X/Y de los UVs de ESA MISMA seleccion; editar
    // los campos mueve los UVs con el undo liviano de UV). Solo en Edit Mode con algo seleccionado.
    propEditItem = new GroupPropertie(T("Transform Mesh"));
    { PropFloat* px = new PropFloat("X"); px->value = &editPosX; px->onChange = AccionEditPos; propEditItem->properties.push_back(px);
      PropFloat* py = new PropFloat("Y"); py->value = &editPosY; py->onChange = AccionEditPos; propEditItem->properties.push_back(py);
      PropFloat* pz = new PropFloat("Z"); pz->value = &editPosZ; pz->onChange = AccionEditPos; propEditItem->properties.push_back(pz); }
    GroupProperties.push_back(propEditItem);
    propUVTransform = new GroupPropertie(T("Transform UV"));
    { PropFloat* pu = new PropFloat("X"); pu->value = &uvPosU; pu->onChange = AccionEditUV; propUVTransform->properties.push_back(pu);
      PropFloat* pv = new PropFloat("Y"); pv->value = &uvPosV; pv->onChange = AccionEditUV; propUVTransform->properties.push_back(pv); }
    GroupProperties.push_back(propUVTransform);

    // pestaña VERTICES (icono mesh): TARJETAS. Las listas REUSAN PropListMeshParts (con scroll, resize, etc., el
    // MISMO componente que el selector de mesh part) en modo 4 (vertex groups) / 1 (uvmaps) / 2 (colors).
    // "Vertex Groups" va PRIMERA: es la mas importante (huesos del rig, pesos para deformar la malla).
    propVertexGroups = new GroupPropertie(T("Vertex Groups"));
    propListVertGroups = new PropListMeshParts("Vertex Groups"); propListVertGroups->modo = 4;
    propVertexGroups->properties.push_back(propListVertGroups);
    PropButton* pbAddGrp = new PropButton(T("Add Vertex Group"), IconType::mesh);
    pbAddGrp->action = AccionVertAddGroup;
    propVertexGroups->properties.push_back(pbAddGrp);
    propBtnRenameGroup = new PropButton(T("Rename"), -1); // renombra el grupo activo (nombre unico por malla)
    propBtnRenameGroup->action = AccionRenameGroup;
    propVertexGroups->properties.push_back(propBtnRenameGroup);
    // fila Delete | Move Up | Move Down (Delete si hay >=1; Move si hay >=2)
    // fila Assign | Remove: los VERTICES seleccionados en Edit Mode entran (peso 1) o salen del
    // grupo activo. Es LA operacion que faltaba para armar un vertex group sin pincel.
    propRowGroupAsig = new PropButtonRow();
    propRowGroupAsig->Agregar(T("Assign"), AccionVertGroupAssign);
    propRowGroupAsig->Agregar(T("Remove"), AccionVertGroupRemove);
    propVertexGroups->properties.push_back(propRowGroupAsig);
    // fila Select | Deselect: el camino inverso (ver que agarra el grupo)
    propRowGroupSel = new PropButtonRow();
    propRowGroupSel->Agregar(T("Select"),   AccionVertGroupSelect);
    propRowGroupSel->Agregar(T("Deselect"), AccionVertGroupDeselect);
    propVertexGroups->properties.push_back(propRowGroupSel);
    propRowGroupOps = new PropButtonRow();
    propRowGroupOps->Agregar(T("Delete"),    AccionVertDelGroup);
    propRowGroupOps->Agregar(T("Move Up"),   AccionVertGroupUp);
    propRowGroupOps->Agregar(T("Move Down"), AccionVertGroupDown);
    propVertexGroups->properties.push_back(propRowGroupOps);
    GroupProperties.push_back(propVertexGroups);

    // "UV Groups": la OTRA entidad de pesos (por CORNER; la pinta el editor UV y la bindea por
    // nombre el armature 2D). Va en la MISMA pestania Vertices, JUSTO DEBAJO de Vertex Groups:
    // es donde el usuario ya viene a administrar los datos de la malla (UV maps / color / grupos),
    // esta siempre disponible (la pestania del armature 2D solo existe editando huesos) y tenerlas
    // pegadas hace obvio que son dos cosas distintas. Mismo patron de tarjeta que Vertex Groups.
    propUVGroups = new GroupPropertie(T("UV Groups"));
    propListUVGroups = new PropListMeshParts("UV Groups"); propListUVGroups->modo = 9;
    propUVGroups->properties.push_back(propListUVGroups);
    PropButton* pbAddUVGrp = new PropButton(T("Add UV Group"), IconType::mesh);
    pbAddUVGrp->action = AccionUVAddGroup;
    propUVGroups->properties.push_back(pbAddUVGrp);
    propBtnRenameUVGroup = new PropButton(T("Rename"), -1);
    propBtnRenameUVGroup->action = AccionRenameUVGroup;
    propUVGroups->properties.push_back(propBtnRenameUVGroup);
    // MISMAS dos filas que Vertex Groups (mismo orden y mismos labels), pero POR CORNER y sobre
    // la seleccion del editor UV: se aprende una vez y sirve para las dos tarjetas.
    propRowUVGroupAsig = new PropButtonRow();
    propRowUVGroupAsig->Agregar(T("Assign"), AccionUVGroupAssign);
    propRowUVGroupAsig->Agregar(T("Remove"), AccionUVGroupRemove);
    propUVGroups->properties.push_back(propRowUVGroupAsig);
    propRowUVGroupSel = new PropButtonRow();
    propRowUVGroupSel->Agregar(T("Select"),   AccionUVGroupSelect);
    propRowUVGroupSel->Agregar(T("Deselect"), AccionUVGroupDeselect);
    propUVGroups->properties.push_back(propRowUVGroupSel);
    propRowUVGroupOps = new PropButtonRow();
    propRowUVGroupOps->Agregar(T("Delete"),    AccionUVDelGroup);
    propRowUVGroupOps->Agregar(T("Move Up"),   AccionUVGroupUp);
    propRowUVGroupOps->Agregar(T("Move Down"), AccionUVGroupDown);
    propUVGroups->properties.push_back(propRowUVGroupOps);
    GroupProperties.push_back(propUVGroups);

    // ===== pestania ARMATURE: tarjeta "Animation" (lista de clips del esqueleto + Add / Rename / Delete / Move) =====
    // MISMO componente que la lista de vertex groups (PropListMeshParts), pero en modo 5 (lee arm->animations).
    propArmAnim = new GroupPropertie(T("Animation"));
    propListAnims = new PropListMeshParts("Animation"); propListAnims->modo = 5;
    propArmAnim->properties.push_back(propListAnims);
    PropButton* pbAddAnim = new PropButton(T("New Animation"), IconType::armature); // crea un clip vacio en pose reset
    pbAddAnim->action = AccionAnimAdd;
    propArmAnim->properties.push_back(pbAddAnim);
    propBtnDupAnim = new PropButton(T("Duplicate"), IconType::armature); // duplica el clip activo (se oculta sin clips)
    propBtnDupAnim->action = AccionAnimDup;
    propArmAnim->properties.push_back(propBtnDupAnim);
    propBtnRenameAnim = new PropButton(T("Rename"), -1); // renombra el clip activo (nombre unico por armature)
    propBtnRenameAnim->action = AccionRenameAnim;
    propArmAnim->properties.push_back(propBtnRenameAnim);
    // fila Delete | Move Up | Move Down (Delete si hay >=1; Move si hay >=2)
    propRowAnimOps = new PropButtonRow();
    propRowAnimOps->Agregar(T("Delete"),    AccionAnimDel);
    propRowAnimOps->Agregar(T("Move Up"),   AccionAnimUp);
    propRowAnimOps->Agregar(T("Move Down"), AccionAnimDown);
    propArmAnim->properties.push_back(propRowAnimOps);
    GroupProperties.push_back(propArmAnim);

    // ===== pestania ARMATURE: tarjeta "Bones": lista de huesos + Name editable + Parent desplegable
    // + pos/rot/scale (pose) del hueso activo =====
    propArmBones = new GroupPropertie(T("Bones"));
    propListBones = new PropListMeshParts("Bones"); propListBones->modo = 6; // lee arm->bones (mismo componente de lista)
    propArmBones->properties.push_back(propListBones);
    // fila "Name": cuadro editable inline (patron del Name del objeto). Al confirmar renombra el hueso
    // Y su vertex group asociado (SincronizarNombreBone -> BoneRenombrar; binding por nombre).
    propBoneNombre = new PropText(T("Name"), "");
    propArmBones->properties.push_back(propBoneNombre);
    // fila "Parent": desplegable con "None" + los huesos validos (sin el propio ni sus descendientes)
    propBoneParent = new PropButton(T("Parent"));
    propBoneParent->conLabel = true;
    propBoneParent->button->desplegable = true;
    propBoneParent->action = AccionMenuBoneParent;
    propArmBones->properties.push_back(propBoneParent);
    // fila "Connected": soldadura head-del-hijo <-> tail-del-padre. Nace TILDADA para los huesos
    // creados por extrude (el flag arranca en true). Se oculta si el hueso no tiene padre.
    propBoneConectado = new PropBool(T("Connected"));
    propBoneConectado->value = &g_boneConectado;
    propBoneConectado->onChange = AccionBoneConectado;
    propArmBones->properties.push_back(propBoneConectado);
    { PropFloat* px=new PropFloat("Pos X"); px->value=&g_bonePosX; px->onChange=AccionBoneTransform; propArmBones->properties.push_back(px);
      PropFloat* py=new PropFloat("Pos Y"); py->value=&g_bonePosY; py->onChange=AccionBoneTransform; propArmBones->properties.push_back(py);
      PropFloat* pz=new PropFloat("Pos Z"); pz->value=&g_bonePosZ; pz->onChange=AccionBoneTransform; propArmBones->properties.push_back(pz); }
    { PropFloat* rx=new PropFloat("Rot X"); rx->value=&g_boneRotX; rx->onChange=AccionBoneTransform; propArmBones->properties.push_back(rx);
      PropFloat* ry=new PropFloat("Rot Y"); ry->value=&g_boneRotY; ry->onChange=AccionBoneTransform; propArmBones->properties.push_back(ry);
      PropFloat* rz=new PropFloat("Rot Z"); rz->value=&g_boneRotZ; rz->onChange=AccionBoneTransform; propArmBones->properties.push_back(rz); }
    { PropFloat* sx=new PropFloat(T("Scale X")); sx->value=&g_boneSclX; sx->onChange=AccionBoneTransform; propArmBones->properties.push_back(sx);
      PropFloat* sy=new PropFloat(T("Scale Y")); sy->value=&g_boneSclY; sy->onChange=AccionBoneTransform; propArmBones->properties.push_back(sy);
      PropFloat* sz=new PropFloat(T("Scale Z")); sz->value=&g_boneSclZ; sz->onChange=AccionBoneTransform; propArmBones->properties.push_back(sz); }
    GroupProperties.push_back(propArmBones);

    // ===== tarjeta "ARMATURE 2D" (huesos 2D del mesh): lista + Name inline + Parent desplegable +
    // Pos de lo seleccionado (y Rotation/Scale de la POSE 2D). Solo visible mientras un editor UV
    // esta en Edit Bones/Pose (ver Bones2DMeshUI / ActualizarPestanias). =====
    propBones2D = new GroupPropertie(T("Armature") + std::string(" 2D"));
    // lista de ARMATURES 2D (arriba de todo: dice CUAL se esta editando) + Add / Rename / Delete
    propListArm2Ds = new PropListMeshParts(T("Armature") + std::string(" 2D")); propListArm2Ds->modo = 10;
    propBones2D->properties.push_back(propListArm2Ds);
    { PropButtonRow* fila = new PropButtonRow();
      fila->Agregar(T("Add"),    AccionArm2DAdd);
      fila->Agregar(T("Delete"), AccionArm2DDel);
      propBones2D->properties.push_back(fila); }
    propBtnRenameArm2D = new PropButton(T("Rename"), -1);
    propBtnRenameArm2D->action = AccionRenameArm2D;
    propBones2D->properties.push_back(propBtnRenameArm2D);
    propListBones2D = new PropListMeshParts("Bones"); propListBones2D->modo = 8; // huesos del armature ACTIVO
    propBones2D->properties.push_back(propListBones2D);
    // fila "Name": renombra el hueso Y su vertex group homonimo (SincronizarNombreBone2D)
    propBone2DNombre = new PropText(T("Name"), "");
    propBones2D->properties.push_back(propBone2DNombre);
    // fila "Parent": desplegable None + huesos validos (sin ciclos)
    propBone2DParent = new PropButton(T("Parent"));
    propBone2DParent->conLabel = true;
    propBone2DParent->button->desplegable = true;
    propBone2DParent->action = AccionMenuBone2DParent;
    propBones2D->properties.push_back(propBone2DParent);
    // fila "Connected" (mismo flag y semantica que la tarjeta Bones del 3D): tildada por defecto
    // en los huesos nacidos por extrude; oculta si el hueso no tiene padre.
    propBone2DConectado = new PropBool(T("Connected"));
    propBone2DConectado->value = &g_b2dConectado;
    propBone2DConectado->onChange = AccionBone2DConectado;
    propBones2D->properties.push_back(propBone2DConectado);
    { gB2dPosX = new PropFloat("Pos X"); gB2dPosX->value = &g_b2dPosX; gB2dPosX->onChange = AccionBone2DPos; propBones2D->properties.push_back(gB2dPosX);
      gB2dPosY = new PropFloat("Pos Y"); gB2dPosY->value = &g_b2dPosY; gB2dPosY->onChange = AccionBone2DPos; propBones2D->properties.push_back(gB2dPosY);
      gB2dRot  = new PropFloat(T("Rotation")); gB2dRot->value = NULL; gB2dRot->onChange = AccionBone2DPose; propBones2D->properties.push_back(gB2dRot);
      gB2dSclX = new PropFloat(T("Scale X")); gB2dSclX->value = NULL; gB2dSclX->onChange = AccionBone2DPose; propBones2D->properties.push_back(gB2dSclX);
      gB2dSclY = new PropFloat(T("Scale Y")); gB2dSclY->value = NULL; gB2dSclY->onChange = AccionBone2DPose; propBones2D->properties.push_back(gB2dSclY); }
    propBone2DPosX = gB2dPosX; propBone2DRot = gB2dRot; // (miembros: los mira el test uvarm2d)
    GroupProperties.push_back(propBones2D);

    propUVMaps = new GroupPropertie(T("UV Maps"));
    propListUV = new PropListMeshParts("UV Maps"); propListUV->modo = 1;
    propUVMaps->properties.push_back(propListUV);
    PropButton* pbAddUV = new PropButton(T("Add UV Map"), IconType::mesh);
    pbAddUV->action = AccionVertAddUVMap;
    propUVMaps->properties.push_back(pbAddUV);
    propBtnRenameUV = new PropButton(T("Rename"), -1); // renombra la UV map activa (nombre unico por malla)
    propBtnRenameUV->action = AccionRenameUVMap;
    propUVMaps->properties.push_back(propBtnRenameUV);
    // fila Delete | Move Up | Move Down (toda la fila oculta con 1 sola UV map)
    propRowUVOps = new PropButtonRow();
    propRowUVOps->Agregar(T("Delete"),    AccionVertDelUVMap);
    propRowUVOps->Agregar(T("Move Up"),   AccionVertUVMapUp);
    propRowUVOps->Agregar(T("Move Down"), AccionVertUVMapDown);
    propUVMaps->properties.push_back(propRowUVOps);
    GroupProperties.push_back(propUVMaps);

    propColorLayers = new GroupPropertie(T("Color"));
    propListColor = new PropListMeshParts("Color"); propListColor->modo = 2;
    propColorLayers->properties.push_back(propListColor);
    PropButton* pbAddCol = new PropButton(T("Add Color Layer"), IconType::mesh);
    pbAddCol->action = AccionVertAddColor;
    propColorLayers->properties.push_back(pbAddCol);
    propBtnColorMode = new PropButton(T("Color Mode"), IconType::mesh);
    propBtnColorMode->action = AccionVertColorMode;
    propColorLayers->properties.push_back(propBtnColorMode);
    propBtnRenameColor = new PropButton(T("Rename"), -1); // renombra la capa de color activa (nombre unico)
    propBtnRenameColor->action = AccionRenameColor;
    propColorLayers->properties.push_back(propBtnRenameColor);
    // fila Delete | Move Up | Move Down (toda la fila oculta con 1 sola capa de color)
    propRowColorOps = new PropButtonRow();
    propRowColorOps->Agregar(T("Delete"),    AccionVertDelColor);
    propRowColorOps->Agregar(T("Move Up"),   AccionVertColorUp);
    propRowColorOps->Agregar(T("Move Down"), AccionVertColorDown);
    propColorLayers->properties.push_back(propRowColorOps);
    GroupProperties.push_back(propColorLayers);

    // (la vieja tarjeta "Vertex Animation (coming soon)" se fue: las animaciones
    // del objeto -incluida la vertex anim- viven en la tarjeta "Animacion" de la
    // pestania Objeto, que es lo que corresponde)

    // ===== pestania "Modifiers" (mesh): selector del stack + Add/Remove + Move Up/Down =====
    propModifiers = new GroupPropertie(T("Modifiers"));
    propModifiers->anchoValores = 0.30f;
    propListModifiers = new PropListMeshParts("Modifiers");
    propListModifiers->modo = 3;                              // 3 = stack de modificadores (mesh->modificadores)
    propModifiers->properties.push_back(propListModifiers);   // [0] selector (el mismo componente de UV/parts)
    // fila: Add (desplegable: abre el menu de tipos) | Remove (oculto si no hay modificadores)
    propRowMod = new PropButtonRow();
    Button* bAddMod = propRowMod->Agregar(T("Add"), AccionMenuAddModifier);
    bAddMod->desplegable = true;
    propRowMod->Agregar(T("Remove"), AccionRemoveModifier);
    propModifiers->properties.push_back(propRowMod);
    // fila: Move Up | Move Down (toda la fila oculta si hay < 2 -> el orden solo importa con 2+)
    propRowModMove = new PropButtonRow();
    propRowModMove->Agregar(T("Move Up"),   AccionModifierUp);
    propRowModMove->Agregar(T("Move Down"), AccionModifierDown);
    propModifiers->properties.push_back(propRowModMove);
    GroupProperties.push_back(propModifiers);

    // ===== 2da tarjeta: props del modificador SELECCIONADO. Por ahora las del MIRROR (bindeadas al modificador
    // activo en ActualizarPestanias); otros tipos muestran "(no properties yet)". Solo visible con un modificador. =====
    propModifierProps = new GroupPropertie(T("Modifier"));
    propModifierProps->anchoValores = 0.55f;
    // Visibilidad (TODOS los modificadores): en el viewport (OFF = nunca se calcula) y en Edit Mode (OFF = edicion
    // rapida en N95, se recalcula al salir). onChange regenera la malla.
    propModVerViewport = new PropBool(T("Display in Viewport")); propModVerViewport->onChange = AccionModParamChanged;
    propModifierProps->properties.push_back(propModVerViewport);
    propModVerEdit = new PropBool(T("Display in Edit Mode")); propModVerEdit->onChange = AccionModParamChanged;
    propModifierProps->properties.push_back(propModVerEdit);
    propModVacio = new PropLabel("(no properties yet)");
    propModifierProps->properties.push_back(propModVacio);
    // Mirror: ejes X/Y/Z
    propMirX = new PropBool(T("Mirror X")); propMirX->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirX);
    propMirY = new PropBool(T("Mirror Y")); propMirY->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirY);
    propMirZ = new PropBool(T("Mirror Z")); propMirZ->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirZ);
    // Mirror Object (target: cualquier objeto; su posicion+rotacion define el plano)
    propMirTarget = new PropButton(T("Mirror Object"), IconType::object);
    propMirTarget->button->desplegable = true; propMirTarget->action = AccionMenuModTarget;
    propModifierProps->properties.push_back(propMirTarget);
    // Armature: target (dropdown solo esqueletos). La malla se deforma (skinning) al rig elegido.
    propArmTarget = new PropButton(T("Target"), IconType::armature);
    propArmTarget->button->desplegable = true; propArmTarget->action = AccionMenuArmTarget;
    propModifierProps->properties.push_back(propArmTarget);
    // Merge (soldar los verts del plano) + distancia
    propMirMerge = new PropBool(T("Merge")); propMirMerge->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirMerge);
    propMirDist = new PropFloat(T("Merge Distance"), "m"); propMirDist->onChange = AccionModParamChanged;
    propMirDist->SetRango(0.0f, 1.0f); propMirDist->stepFino = 0.0001f; propMirDist->dragStep = 0.0005f;
    propModifierProps->properties.push_back(propMirDist);
    // Clipping (edit-time): clampea los verts al plano al moverlos y, una vez pegados, los deja pegados (arranca ON)
    propMirClip = new PropBool(T("Clipping")); propMirClip->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirClip);
    // Subdivision Surface: modo Simple (OFF = Catmull-Clark, suaviza) + niveles viewport/render (enteros 0..6)
    propSubSimple = new PropBool(T("Simple")); propSubSimple->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propSubSimple);
    propSubLevel = new PropFloat(T("Levels Viewport")); propSubLevel->onChange = AccionModParamChanged;
    propSubLevel->SetRango(0.0f, 6.0f); propSubLevel->entero = true; propModifierProps->properties.push_back(propSubLevel);
    propSubRender = new PropFloat("Render"); propSubRender->onChange = AccionModParamChanged;
    propSubRender->SetRango(0.0f, 6.0f); propSubRender->entero = true; propModifierProps->properties.push_back(propSubRender);
    // Screw: angle (grados), screw (subida por el eje), steps viewport/render, eje (dropdown), stretch U/V (UV)
    propScrewAngle = new PropFloat(T("Angle"), "\xc2\xb0"); propScrewAngle->onChange = AccionModParamChanged;
    propScrewAngle->SetRango(-3600.0f, 3600.0f); propModifierProps->properties.push_back(propScrewAngle);
    propScrewHeight = new PropFloat("Screw", "m"); propScrewHeight->onChange = AccionModParamChanged;
    propScrewHeight->SetRango(-1000.0f, 1000.0f); propModifierProps->properties.push_back(propScrewHeight);
    propScrewAxis = new PropButton(T("Axis")); propScrewAxis->button->desplegable = true; propScrewAxis->action = AccionMenuScrewAxis;
    propModifierProps->properties.push_back(propScrewAxis);
    propScrewSteps = new PropFloat(T("Steps Viewport")); propScrewSteps->onChange = AccionModParamChanged;
    propScrewSteps->SetRango(2.0f, 512.0f); propScrewSteps->entero = true; propModifierProps->properties.push_back(propScrewSteps);
    propScrewRender = new PropFloat("Render"); propScrewRender->onChange = AccionModParamChanged;
    propScrewRender->SetRango(2.0f, 512.0f); propScrewRender->entero = true; propModifierProps->properties.push_back(propScrewRender);
    propScrewStretchU = new PropBool(T("Stretch U")); propScrewStretchU->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewStretchU);
    propScrewStretchV = new PropBool(T("Stretch V")); propScrewStretchV->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewStretchV);
    propScrewSmooth = new PropBool(T("Smooth")); propScrewSmooth->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewSmooth);
    propScrewMerge = new PropBool(T("Merge")); propScrewMerge->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewMerge);
    propScrewFlip = new PropBool(T("Flip Normals")); propScrewFlip->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewFlip);
    // "Optimize Vertex Groups" (SOLO modificador Armature): colapsa a 1 hueso por vertice -> skinning mucho mas
    // rapido en el N95 (destructivo -> pide confirmacion). Se oculta salvo en el Armature (ActualizarPropiedades).
    propBtnOptVG = new PropButton(T("Optimize Vertex Groups"));
    propBtnOptVG->action = AccionOptimizarVertexGroups;
    propModifierProps->properties.push_back(propBtnOptVG);
    // "Cache Animation" (SOLO Armature): bakea el skinning por frame -> reproduccion sin recomputar (4fps -> techo de
    // render). "Frame Skip": 0 = todos los frames; N = guarda cada N+1 e interpola (menos memoria en el N95).
    propArmCache = new PropBool(T("Cache Animation")); propArmCache->onChange = AccionModParamChanged;
    propModifierProps->properties.push_back(propArmCache);
    propArmCacheSkip = new PropFloat(T("Frame Skip")); propArmCacheSkip->SetRango(0.0f, 10.0f); propArmCacheSkip->entero = true;
    propArmCacheSkip->onChange = AccionModParamChanged;
    propModifierProps->properties.push_back(propArmCacheSkip);
    // Culling (PVS por triangulo): metodo (desplegable) + Recalcular + info del sidecar
    propPvsMetodo = new PropButton(T("Method")); propPvsMetodo->button->desplegable = true;
    propPvsMetodo->action = AccionMenuPvsMetodo;
    propModifierProps->properties.push_back(propPvsMetodo);
    propPvsRecalc = new PropButton(T("Recalcular"));
    propPvsRecalc->action = AccionPvsRecalcular;
    propModifierProps->properties.push_back(propPvsRecalc);
    propPvsInfo = new PropLabel("");   // "N sectores, sector activo S" / "sin .pvs.json"
    propModifierProps->properties.push_back(propPvsInfo);
    // Apply Modifier (cualquier modificador): hornea la malla generada en la editable
    propBtnApplyMod = new PropButton(T("Apply Modifier"));
    propBtnApplyMod->action = AccionAplicarModificador;
    propModifierProps->properties.push_back(propBtnApplyMod);
    GroupProperties.push_back(propModifierProps);

    // ===== pestania "Constraints" (tab 7): selector del stack + Add/Remove + Move Up/Down =====
    // Calcada de Modifiers a proposito (el dueno ya sabe usar esa): misma disposicion, mismos
    // textos, mismas reglas de visibilidad. La lista va por 'obj' y no por 'mesh' (ver PropList.h).
    propConstraints = new GroupPropertie(T("Constraints"));
    propConstraints->anchoValores = 0.30f;
    propListConstraints = new PropListMeshParts("Constraints");
    propListConstraints->modo = 11;                            // 11 = stack de constraints (Object::constraints)
    propConstraints->properties.push_back(propListConstraints); // [0] selector
    propRowCon = new PropButtonRow();
    Button* bAddCon = propRowCon->Agregar(T("Add"), AccionMenuAddConstraint);
    bAddCon->desplegable = true;
    propRowCon->Agregar(T("Remove"), AccionRemoveConstraint);
    propConstraints->properties.push_back(propRowCon);
    propRowConMove = new PropButtonRow();
    propRowConMove->Agregar(T("Move Up"),   AccionConstraintUp);
    propRowConMove->Agregar(T("Move Down"), AccionConstraintDown);
    propConstraints->properties.push_back(propRowConMove);
    // (aca vivia un parrafo que explicaba que la pestania Objeto muestra la transform BASE. LO
    //  SACO EL DUENO: "quita todo el texto de la pestania de objeto muestra bla bla bla. ocupa
    //  espacio valiosisimo". Es cierto: era un parrafo de 2 renglones SIEMPRE visible, en un panel
    //  angosto que ademas tiene que entrar en un N95. Lo que explicaba sigue documentado donde
    //  importa -en W3dConstraint.h y en la regla EFECTIVA/BASE de Objects.h-, y ahora ademas se
    //  puede APAGAR el constraint en Edit Mode -ver "Show in Edit Mode"-, que era el momento en el
    //  que la diferencia molestaba de verdad.)
    GroupProperties.push_back(propConstraints);

    // ===== 2da tarjeta: props del constraint SELECCIONADO. Que fila se ve depende del TIPO, y
    // OCULTAR ES value = NULL (PropBool/PropFloat) u oculto = true (PropButton/PropLabel): es lo
    // unico que mantiene coherentes el dibujo, el hit-test del mouse y la nav por teclado. =====
    propConstraintProps = new GroupPropertie(T("Constraint"));
    propConstraintProps->anchoValores = 0.55f;
    // Enabled: el "ojito" de la entrada (los 3 tipos)
    propConActivo = new PropBool(T("Enabled")); propConActivo->onChange = AccionConParamChanged;
    propConstraintProps->properties.push_back(propConActivo);
    // Show in Edit Mode: apagado (el default), mientras se editan los vertices de ESTE objeto el
    // constraint no se aplica y la malla se edita en su transform real. Lo pidio el dueno porque
    // editar un arbol que persigue a la camara "se hace muy dificil". Va en los 3 tipos.
    propConVerEdit = new PropBool(T("Show in Edit Mode")); propConVerEdit->onChange = AccionConParamChanged;
    propConstraintProps->properties.push_back(propConVerEdit);
    // Source: desplegable de objetos + "la vista" (Copy Location / Copy Rotation)
    propConFuente = new PropButton(T("Source"), IconType::object);
    propConFuente->button->desplegable = true; propConFuente->action = AccionMenuConFuente;
    propConstraintProps->properties.push_back(propConFuente);
    // AVISO 2: sin fuente el constraint existe pero no hace NADA. Se ve justo debajo del boton
    // que quedo en "None", que es el caso al que llega solo cuando borran el objeto fuente.
    propConAvisoFuente = new PropLabel(T("No source: this constraint does nothing."), true /*wrap*/);
    propConstraintProps->properties.push_back(propConAvisoFuente);
    // Influence: 0..100 CON el "%" (se guarda asi, no en 0..1: ver W3dConstraint.h)
    propConInfluencia = new PropFloat(T("Influence"), "%"); propConInfluencia->onChange = AccionConParamChanged;
    propConInfluencia->SetRango(0.0f, 100.0f);
    propConstraintProps->properties.push_back(propConInfluencia);
    // ================================================================================
    //  LOS EJES QUE SE COPIAN (Copy Location / Copy Rotation)
    //
    //  OJO CON EL NOMBRE DE LOS EJES: el panel muestra la POSICION en Z-ARRIBA (el campo
    //  "Y" de la tarjeta Transform apunta a pos.z y el "Z" a pos.y; esta escrito en
    //  ConstruirGrupos, en el bloque de los canales de animacion). La ROTACION y la
    //  ESCALA, en cambio, se muestran DIRECTAS (rotEuler.y va al campo "Y").
    //  O sea que estos tres checkboxes NO se pueden bindear igual para los dos tipos:
    //    - Copy LOCATION: "Copy Y" tiene que enmascarar ejeZ y "Copy Z" ejeY, para que
    //      diga lo mismo que el campo de posicion que el usuario esta mirando arriba.
    //    - Copy ROTATION: van derecho, porque la rotacion se muestra derecho.
    //  El dato GUARDADO (W3dConstraint::ejeX/Y/Z) es siempre el eje INTERNO del motor,
    //  igual que pos: el .w3d nunca ve el espacio de display. El cruce vive SOLO aca,
    //  en el bindeo (ver ActualizarPestanias).
    //  Lo reporto el dueno: "apreto copiar Z y copia el Y. y el Y es el Z".
    // ================================================================================
    propConEjeX = new PropBool(T("Copy X")); propConEjeX->onChange = AccionConParamChanged;
    propConstraintProps->properties.push_back(propConEjeX);
    propConEjeY = new PropBool(T("Copy Y")); propConEjeY->onChange = AccionConParamChanged;
    propConstraintProps->properties.push_back(propConEjeY);
    propConEjeZ = new PropBool(T("Copy Z")); propConEjeZ->onChange = AccionConParamChanged;
    propConstraintProps->properties.push_back(propConEjeZ);
    // AVISO 3: destildar los tres ejes APAGA el constraint (W3dConEfectivo, Objects.cpp: "sin ejes
    // no copia nada"). Sin esta linea el constraint sigue en la lista, con su ojito prendido y su
    // influencia al 100, y no hace absolutamente nada -- sin decir por que. Va JUSTO DEBAJO de los
    // tres checkboxes, que es donde se mira cuando se los destilda.
    propConAvisoEjes = new PropLabel(T("No axes: this constraint does nothing."), true /*wrap*/);
    propConstraintProps->properties.push_back(propConAvisoEjes);
    // Mode del billboard (2 opciones)
    propConBBModo = new PropButton(T("Mode"));
    propConBBModo->button->desplegable = true; propConBBModo->action = AccionMenuConBBModo;
    propConstraintProps->properties.push_back(propConBBModo);
    // AVISO 4: el giro de 180 del billboard con influencia intermedia. Es conducta CONOCIDA y
    // aceptada (mezclar contra una orientacion absoluta que gira), no un bug -- suavizarlo pediria
    // acumular vueltas, o sea estado POR VISTA, que es justo lo que este diseno elimina.
    propConAvisoBB = new PropLabel(T("With influence between 0 and 100 the billboard can flip 180 degrees "
                                     "when it crosses the camera. Use 100% to avoid it."), true /*wrap*/);
    propConstraintProps->properties.push_back(propConAvisoBB);
    GroupProperties.push_back(propConstraintProps);
}


// rebindea las propiedades del material al MESH PART seleccionado en la
// lista (antes siempre era el [0], y "Texture" apuntaba a transparent)

// arrastre del borde inferior de la lista de mesh parts (cambia filasMax)
static bool gListaResize = false;
static int gListaResizeY0 = 0;
static int gListaFilas0 = 3;
// DRAG-SCROLL tactil de un mini-listado (UV/color/grupos/modificadores/parts): al arrastrar el dedo sobre la lista
// se scrollea ELLA (scrollFila), no el panel entero. Antes solo se podia con la rueda -> inusable en tactil.
static PropListMeshParts* gListaScrollLista = NULL;
static int gListaScrollY0 = 0;   // my del press
static int gListaScroll0 = 0;    // scrollFila al empezar el arrastre

// arrastre de un PropFloat con el mouse: click + mover horizontal acumula el
// delta 'dx' en el valor (como en Blender). NULL = no se esta arrastrando.
static PropFloat* gFloatDrag = NULL;
static bool  gFloatDragMoved = false; // se paso el umbral de arrastre? (si NO al soltar -> fue un click -> editar texto)
static float gFloatDragAccum = 0.0f;  // delta acumulado desde el mouse-down (zona muerta antes de arrastrar)

// el rebind global opera sobre el panel con el que se interactuo
void RebindMaterialMeshPart(){
    if (PropsActivo) PropsActivo->Rebind();
}

void Properties::Rebind(){
    if (!propMeshParts || !propMaterial || !ObjActivo) return;
    if (ObjActivo->getType() != ObjectType::mesh) return;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(propMeshParts->properties[0]);
    Mesh* mesh = lista->mesh;
    if (!mesh || mesh->materialsGroup.empty()) return;
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;
    Material* material = mesh->materialsGroup[idx].material;

    // Tarjeta "Material" (propMaterial): el bind es POR MEMBER (propMatChk/propMatCol/...),
    // reordenar la tarjeta no rompe nada. El material POR DEFECTO no se edita: se ocultan
    // sus filas (value=NULL) y se muestra el aviso.
    bool esDefault = (!material || material == MaterialDefecto);
    if (propMsgDefault) propMsgDefault->oculto = !esDefault; // aviso: SOLO con el material por defecto
    if (propSepMat)     propSepMat->oculto     = esDefault;  // separador: OCULTO con el default (sino linea huerfana)
    if (propBtnRenameMat) propBtnRenameMat->oculto = esDefault; // el material por defecto NO se renombra
    if (propBtnTextura) {
        propBtnTextura->oculto = esDefault;
        propBtnTextura->button->text =
            NombreDeTextura(material ? material->texture : NULL);
    }
    // "Delete" (botones[0] de la fila Delete|Rename) solo si hay >1 parte. Aca (Rebind se llama tras
    // Add/Delete via RebindMaterialMeshPart) se actualiza apenas cambia la cantidad de partes.
    if (propRowDelRen && !propRowDelRen->botones.empty())
        propRowDelRen->botones[0]->visible = (mesh->materialsGroup.size() > 1);
    // "Move Up/Down": toda la fila solo si hay >1 parte (reordenar = orden de dibujado)
    if (propRowPartMove && propRowPartMove->botones.size() >= 2) {
        bool m2 = (mesh->materialsGroup.size() > 1);
        propRowPartMove->botones[0]->visible = m2;
        propRowPartMove->botones[1]->visible = m2;
    }
    // props del material por MEMBER (index-independiente: reordenar la tarjeta no rompe nada)
    propMatChk[0]->value = (!esDefault && material->texture) ? &material->filtrado : NULL; // Filtering
    propMatChk[1]->value = esDefault ? NULL : &material->transparent;
    propMatChk[2]->value = esDefault ? NULL : &material->vertexColor;
    propMatChk[3]->value = esDefault ? NULL : &material->lighting;
    propMatChk[4]->value = esDefault ? NULL : &material->repeat;
    propMatChk[5]->value = esDefault ? NULL : &material->culling;
    propMatChk[6]->value = NULL; // viejo "Depth Test": lo reemplaza el desplegable propBtnProfundidad
    propMatChk[7]->value = NULL; // Smooth Shading se quito del material (el shading lo dan las normales de la malla)
    // "Reflection" (chrome) se OCULTA cuando hay Normal Mapping (excluyentes: mismo combiner).
    propMatChk[8]->value = (esDefault || material->normalMap) ? NULL : &material->chrome; // Reflection on/off
    propMatChk[9]->value = NULL; // viejo "Chrome 360": SIEMPRE oculto -> lo reemplaza el dropdown propBtnReflectMode
    propMatChk[10]->value = esDefault ? NULL : &material->normalMap; // NORMAL MAPPING (DOT3)
    // dropdown del MODO de Reflection: visible SOLO si Reflection esta tildado (y sin normal map); muestra el modo.
    if (propBtnReflectMode) {
        propBtnReflectMode->oculto = (esDefault || material->normalMap || !material->chrome);
        propBtnReflectMode->button->text = material ? NombreReflectMode(material->reflectMode)
                                                    : std::string("Matcap (hardware)");
    }
    // --- calcomania / mezcla / profundidad ---
    // El tilde "Decal" es DERIVADO: refleja si el material cumple la receta entera, y al
    // togglearlo la aplica o la deshace (OnMatDecalChange). Por eso su value es un espejo.
    if (propMatDecal) {
        g_matDecal = W3dMaterialEsDecal(material);
        propMatDecal->value = esDefault ? NULL : &g_matDecal;
    }
    if (propMatSesgo) {
        g_matSesgo = esDefault ? 0.0f : material->depth_bias;
        propMatSesgo->value = esDefault ? NULL : &g_matSesgo;
    }
    if (propMatOrden) {
        g_matOrden = esDefault ? 0.0f : (float)material->orden_pasada;
        propMatOrden->value = esDefault ? NULL : &g_matOrden;
    }
    // --- LINEAS: el tilde + su grosor (el grosor solo se ve con el tilde puesto) ---
    if (propMatLineas) {
        g_matLineas = esDefault ? false : material->lineas;
        propMatLineas->value = esDefault ? NULL : &g_matLineas;
    }
    if (propMatGrosorLinea) {
        g_matGrosorLinea = esDefault ? 1.0f : material->grosorLinea;
        propMatGrosorLinea->value = (esDefault || !material->lineas) ? NULL : &g_matGrosorLinea;
    }
    // el desplegable de MEZCLA solo tiene sentido con Transparent ON (sin blend, el modo no
    // se aplica); el de PROFUNDIDAD siempre, que es la mitad que el checkbox no mostraba.
    if (propBtnMezcla) {
        propBtnMezcla->oculto = (esDefault || !material->transparent);
        propBtnMezcla->button->text = material ? NombreMezcla(material->mezcla)
                                               : std::string("Alpha (normal)");
    }
    if (propBtnProfundidad) {
        propBtnProfundidad->oculto = esDefault;
        propBtnProfundidad->button->text = material
            ? NombreProfundidad(material->depth_test, material->depth_write)
            : std::string("Test + Write (opaque)");
    }
    // selector de la textura del normal map: visible SOLO si Normal Mapping esta tildado; muestra su nombre
    if (propBtnNormalTex) {
        propBtnNormalTex->oculto = (esDefault || !material->normalMap);
        // GUARD material!=NULL (CRASH N95): el cubo de escena fresca (W3dNewSceneInit) tiene material==NULL
        // -> esDefault, pero ESTA linea derefenciaba material-> sin chequear (todo el resto del rebind SI guardea
        //    material NULL, ej. linea de propBtnTextura). En PC no se veia porque autocargaba una escena con material real.
        propBtnNormalTex->button->text = (material && material->normalTexture)
            ? NombreDeTextura(material->normalTexture) : std::string("No Normal Map");
    }
    // Base Color: se OCULTA si Vertex Color esta ON (ahi manda el color del vertice, la base "no se ve" -> al pepe).
    propMatCol[0]->value = (esDefault || material->vertexColor) ? NULL : material->diffuse;
    // Specular / Emission / Shininess: solo tienen sentido con LIGHTING ON -> se ocultan si esta OFF.
    propMatCol[1]->value = (esDefault || !material->lighting) ? NULL : material->specular;
    propMatCol[2]->value = (esDefault || !material->lighting) ? NULL : material->emission;
    propMatShin->value   = (esDefault || !material->lighting) ? NULL : &material->shininess;

    // el selector muestra el material actual del mesh part
    if (propBtnNewMaterial) {
        propBtnNewMaterial->button->text =
            material ? material->name : std::string("Default Material");
    }
    PropertiesLayoutDirty = true; // el alto de la tarjeta pudo cambiar
}

void Properties::RefreshPropMeshParts(){
    if (ObjActivo->getType() != ObjectType::mesh){
        propMeshParts->visible = false;
        if (propMaterial) propMaterial->visible = false;
        static_cast<PropListMeshParts*>(propMeshParts->properties[0])->mesh = NULL;
        return;
    }

    propMeshParts->visible = true;
    if (propMaterial) propMaterial->visible = true;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    static_cast<PropListMeshParts*>(propMeshParts->properties[0])->mesh = mesh;
    static_cast<PropListMeshParts*>(propMeshParts->properties[0])->selectIndex = 0;

    if (mesh->materialsGroup.empty()) return;

    // "Delete" (botones[0] de la fila Delete|Rename) solo si hay MAS de 1 parte (no se borra la unica)
    if (propRowDelRen && !propRowDelRen->botones.empty())
        propRowDelRen->botones[0]->visible = (mesh->materialsGroup.size() > 1);
    // "Move Up/Down": la fila entera solo si hay >1 parte (el orden = orden de dibujado)
    if (propRowPartMove && propRowPartMove->botones.size() >= 2) {
        bool m2 = (mesh->materialsGroup.size() > 1);
        propRowPartMove->botones[0]->visible = m2;
        propRowPartMove->botones[1]->visible = m2;
    }

    Rebind();
}

// el PropColor que tiene el picker abierto (borde verde en su fila)
static PropColor* gColorAbierto = NULL;
// posicion en pantalla de la fila seleccionada por TECLADO (para abrir el ColorPicker con OK/Enter)
static int gColorSelSx = 0, gColorSelSy = 0;

void Properties::RefreshTargetProperties(){
    SincronizarAnimFps(); // el campo "FPS" refleja el AnimFPS real (el import lo pone al fps del archivo)
    // al cerrarse el picker, la fila del color pierde el borde verde
    if (gColorAbierto && !PopUpActive) {
        gColorAbierto->editando = false;
        gColorAbierto = NULL;
        // FIX: el picker se cerro -> DESBLOQUEAR la nav de propiedades. EnterPropertieSelect dejaba
        // editando=true + ViewPortClickDown=true (el picker es modal) y nadie los reseteaba al cerrar ->
        // las propiedades quedaban "trabadas" (el color seguia activo).
        if (PropsActivo) PropsActivo->editando = false;
        ViewPortClickDown = false;
    }
    // (El band-aid per-frame de "Chrome 360" se SACO: ahora cada checkbox de material lleva onChange=RebindMaterial
    //  -> togglear Chrome/Normal Mapping re-arma la tarjeta al instante por CUALQUIER camino (click PC o teclado
    //  Symbian). Ver ConstruirGrupos. Las visibilidades dependientes ya no necesitan un parche por-frame por-prop.)
    if (PropertiesLayoutDirty) {
        PropertiesLayoutDirty = false;
        Resize(width, height); // tambien recalcula la scrollbar
    }
    SincronizarNombreObjeto(this); // el campo "Name": muestra el nombre del objeto y commitea lo editado al perder foco
    // proxy de POSICION 2D: muestra pos.x/y relativa al tamano de la UI (o en px con el checkbox)
    {
        Object* e2d = (ObjActivo && UI2D_EsElemento2D(ObjActivo)) ? ObjActivo : NULL;
        if (e2d){
            float vw, vh; UI2D_TamanoLienzo(&vw, &vh);
            g_pos2dX = g_pos2dAbs ? e2d->pos.x * vw : e2d->pos.x;
            g_pos2dY = g_pos2dAbs ? e2d->pos.y * vh : e2d->pos.y;
            PropFloat* xs[6] = { propT2dPosX, propT2dPosY, propImgPosX, propImgPosY,
                                 propRectPosX, propRectPosY };
            for (int i = 0; i < 6; i++) if (xs[i]){
                xs[i]->unit = g_pos2dAbs ? "px" : "";
                xs[i]->stepFino   = g_pos2dAbs ? 1.0f  : 0.01f;
                xs[i]->stepGrueso = g_pos2dAbs ? 10.0f : 0.1f;
                xs[i]->dragStep   = g_pos2dAbs ? 1.0f  : 0.002f;
            }
        }
    }
    SincronizarTexto2D(this);      // idem para el campo "Text" del elemento de texto 2D
    SincronizarLodDist(this);      // y el campo "Distances" del objeto LOD
    SincronizarPartTextura(this);  // y los dos campos de texto del objeto Particulas
    SincronizarPartColor(this);
    SincronizarTextoBoton(this);   // y el del boton 2D
    SincronizarNombreBone(this);   // fila "Name" de la tarjeta Bones: renombra hueso + vertex group al commitear
    SincronizarNombreBone2D(this); // idem para la tarjeta Armature 2D (huesos 2D del mesh)
    // UNIFORME de padding/margen: mientras el checkbox esta prendido los 4 lados siguen
    // al primero (el valor unico del panel bindea a Izq y aca se replica en vivo)
    if (ObjActivo){
        if (UI2D_EsElemento2D(ObjActivo)){
            Elemento2D* eU = (Elemento2D*)ObjActivo;
            if (eU->padUni)  { eU->padDer = eU->padArr = eU->padAba = eU->padIzq; }
            if (eU->margUni) { eU->margDer = eU->margArr = eU->margAba = eU->margIzq; }
        } else if (ObjActivo->getType() == ObjectType::ui){
            UI* uU = (UI*)ObjActivo;
            if (uU->padUni) { uU->padDer = uU->padArr = uU->padAba = uU->padIzq; }
        }
    }
    if (!ObjActivo) {
        if (target) {
            target = NULL;
            // soltar el mesh de la lista de partes: quedaba un puntero
            // al mesh BORRADO y el proximo click/resize crasheaba
            if (propMeshParts && !propMeshParts->properties.empty()) {
                static_cast<PropListMeshParts*>(
                    propMeshParts->properties[0])->mesh = NULL;
                propMeshParts->visible = false;
            }
            Resize(width, height);
        }
        return;
    }
    if (ObjActivo == target) return;
    target = ObjActivo;

    //posicion
    static_cast<PropFloat*>(propTransform->properties[0])->value = &ObjActivo->pos.x;
    static_cast<PropFloat*>(propTransform->properties[1])->value = &ObjActivo->pos.z;
    static_cast<PropFloat*>(propTransform->properties[2])->value = &ObjActivo->pos.y;

    //rotacion: el MODO decide que campos se muestran y a que apuntan
    ObjActivo->ActualizarDisplayRot(); // display fresco desde el quaternion
    int rm = ObjActivo->rotMode;
    propRotMode->button->text = (rm == RotQuaternion) ? "Quaternion (WXYZ)"
                              : (rm == RotAxisAngle)  ? "Axis Angle" : "XYZ Euler";
    PropFloat* pw = static_cast<PropFloat*>(propTransform->properties[5]); // W
    PropFloat* px = static_cast<PropFloat*>(propTransform->properties[6]); // X
    PropFloat* py = static_cast<PropFloat*>(propTransform->properties[7]); // Y
    PropFloat* pz = static_cast<PropFloat*>(propTransform->properties[8]); // Z
    if (rm == RotQuaternion){
        // el panel edita la COPIA de display (rotQuat); SincronizarRotacionActiva la baja al quaternion real
        pw->name = "Rotation W"; pw->value = &ObjActivo->rotQuat.w; pw->unit = "";
        px->name = "X"; px->value = &ObjActivo->rotQuat.x; px->unit = "";
        py->value = &ObjActivo->rotQuat.y; py->unit = "";
        pz->value = &ObjActivo->rotQuat.z; pz->unit = "";
    } else if (rm == RotAxisAngle){
        pw->name = "Rotation W"; pw->value = &ObjActivo->rotAngle; pw->unit = "°";
        px->name = "X"; px->value = &ObjActivo->rotAxis.x; px->unit = "";
        py->value = &ObjActivo->rotAxis.y; py->unit = "";
        pz->value = &ObjActivo->rotAxis.z; pz->unit = "";
    } else { // XYZ Euler
        pw->value = NULL; // oculto (Resize devuelve 0; el teclado lo saltea)
        px->name = "Rotation X"; px->value = &ObjActivo->rotEuler.x; px->unit = "°";
        py->value = &ObjActivo->rotEuler.y; py->unit = "°";
        pz->value = &ObjActivo->rotEuler.z; pz->unit = "°";
    }

    //escala (indices corridos +2 por el Mode y el W)
    static_cast<PropFloat*>(propTransform->properties[10])->value = &ObjActivo->scale.x;
    static_cast<PropFloat*>(propTransform->properties[11])->value = &ObjActivo->scale.y;
    static_cast<PropFloat*>(propTransform->properties[12])->value = &ObjActivo->scale.z;
    if (propObjVisible) propObjVisible->value = &ObjActivo->visible;       // checkboxes visible/render
    if (propObjRender)  propObjRender->value  = &ObjActivo->renderizable;
    if (propObjRelLines) propObjRelLines->value = &ObjActivo->showRelantionshipsLines;

    //Mesh Parts
    RefreshPropMeshParts();

    // TEXTO 2D: bindea la tarjeta (NULL/labels segun el activo sea o no un Texto2D)
    if (propTexto2D && propT2dTam){
        Texto2D* t = (ObjActivo && ObjActivo->getType() == ObjectType::texto2d) ? (Texto2D*)ObjActivo : NULL;
        propT2dTam->value   = t ? &t->tam   : NULL;
        propT2dColor->value = (t && t->palColor < 0) ? t->color : NULL;
        propT2dColor->palRef = t ? &t->palColor : NULL; propT2dColor->palObj = t;
        if (propT2dPal && t) propT2dPal->button->text = PalNombre(t, t->palColor);
        if (propT2dRot)  propT2dRot->value  = t ? &t->rot2d : NULL;
        if (propT2dPosX){ propT2dPosX->value = t ? &g_pos2dX : NULL; propT2dPosX->onChange = AccionPos2DEditada; }
        if (propT2dPosY){ propT2dPosY->value = t ? &g_pos2dY : NULL; propT2dPosY->onChange = AccionPos2DEditada; }
        if (propT2dPosZ) propT2dPosZ->value = t ? &t->pos.z : NULL;
        if (propT2dPosAbs) propT2dPosAbs->value = t ? &g_pos2dAbs : NULL;
        if (propT2dOpac) propT2dOpac->value = t ? &t->opacidad : NULL;
        if (propT2dDec)  propT2dDec->value  = (t && t->tipo == 2) ? &t->decimales : NULL;
        if (propT2dAutoTam) propT2dAutoTam->value = t ? &t->autoTam : NULL;
        if (propT2dLineas && t) propT2dLineas->button->text = T2dNombreLineas(t->lineas);
        if (t){
            propT2dTexto->field.SetText(t->texto);
            propT2dAlignH->button->text = T2dNombreAlign(t->alignH, true);
            propT2dAlignV->button->text = T2dNombreAlign(t->alignV, false);
            propT2dFuente->button->text = Fuente2DNombre(t->fuente);
            propT2dAncla->button->text  = T2dNombreAncla(t->ancla);
            if (propT2dTipo) propT2dTipo->button->text = T2dNombreTipo(t->tipo);
        }
    }
    // IMAGEN 2D: bindea la tarjeta (NULL/labels segun el activo sea o no una Imagen2D)
    if (propImagen2D && propImgAncho){
        Imagen2D* im = (ObjActivo && ObjActivo->getType() == ObjectType::imagen2d) ? (Imagen2D*)ObjActivo : NULL;
        propImgAncho->value = im ? &im->ancho : NULL;
        propImgAlto->value  = im ? &im->alto  : NULL;
        propImgRot->value   = im ? &im->rot2d : NULL;
        if (propImgPosX){ propImgPosX->value = im ? &g_pos2dX : NULL; propImgPosX->onChange = AccionPos2DEditada; }
        if (propImgPosY){ propImgPosY->value = im ? &g_pos2dY : NULL; propImgPosY->onChange = AccionPos2DEditada; }
        if (propImgPosZ) propImgPosZ->value = im ? &im->pos.z : NULL;
        if (propImgPosAbs) propImgPosAbs->value = im ? &g_pos2dAbs : NULL;
        if (propImgOpac) propImgOpac->value = im ? &im->opacidad : NULL;
        if (propImgUnidad && im) propImgUnidad->button->text = TamNombreModo(im->tamModo);
        if (propImgColor){ propImgColor->value = (im && im->palTinte < 0) ? im->color : NULL;
                           propImgColor->palRef = im ? &im->palTinte : NULL; propImgColor->palObj = im; }
        if (propImgPal && im) propImgPal->button->text = PalNombre(im, im->palTinte);
        if (propImgAlpha) propImgAlpha->value = im ? &im->usarAlpha : NULL;
        if (propImgFiltro) propImgFiltro->value = im ? &im->filtrado : NULL;
        AjustarFilaTam(propImgAncho, im ? im->tamModo : TAM2D_PX);
        AjustarFilaTam(propImgAlto,  im ? im->tamModo : TAM2D_PX);
        if (im){
            propImgTextura->button->text = im->textura.empty() ? std::string(T("Choose..."))
                                                               : NombreDeArchivo(im->textura);
            propImgModo->button->text  = ImgNombreModo(im->modo);
            propImgAncla->button->text = T2dNombreAncla(im->ancla);
        }
    }
    // RECTANGULO 2D
    if (propRect2D && propRectAncho){
        Rect2D* r = (ObjActivo && ObjActivo->getType() == ObjectType::rect2d) ? (Rect2D*)ObjActivo : NULL;
        propRectAncho->value = r ? &r->ancho : NULL;
        propRectAlto->value  = r ? &r->alto  : NULL;
        propRectRot->value   = r ? &r->rot2d : NULL;
        if (propRectPosX){ propRectPosX->value = r ? &g_pos2dX : NULL; propRectPosX->onChange = AccionPos2DEditada; }
        if (propRectPosY){ propRectPosY->value = r ? &g_pos2dY : NULL; propRectPosY->onChange = AccionPos2DEditada; }
        if (propRectPosZ) propRectPosZ->value = r ? &r->pos.z : NULL;
        if (propRectPosAbs) propRectPosAbs->value = r ? &g_pos2dAbs : NULL;
        if (propRectOpac)  propRectOpac->value  = r ? &r->opacidad : NULL;
        if (propRectColor){ propRectColor->value = (r && r->palColor < 0) ? r->color : NULL;
                            propRectColor->palRef = r ? &r->palColor : NULL; propRectColor->palObj = r; }
        if (propRectPal && r) propRectPal->button->text = PalNombre(r, r->palColor);
        if (propRectUnidad && r) propRectUnidad->button->text = TamNombreModo(r->tamModo);
        AjustarFilaTam(propRectAncho, r ? r->tamModo : TAM2D_PX);
        AjustarFilaTam(propRectAlto,  r ? r->tamModo : TAM2D_PX);
        if (r && propRectAncla) propRectAncla->button->text = T2dNombreAncla(r->ancla);
    }
    // CONTENEDOR 2D
    if (propCont2D && propContAncho){
        Contenedor2D* c = (ObjActivo && ObjActivo->getType() == ObjectType::cont2d) ? (Contenedor2D*)ObjActivo : NULL;
        propContAncho->value = c ? &c->ancho : NULL;
        propContAlto->value  = c ? &c->alto  : NULL;
        propContRot->value   = c ? &c->rot2d : NULL;
        if (propContPosX){ propContPosX->value = c ? &g_pos2dX : NULL; propContPosX->onChange = AccionPos2DEditada; }
        if (propContPosY){ propContPosY->value = c ? &g_pos2dY : NULL; propContPosY->onChange = AccionPos2DEditada; }
        if (propContPosZ) propContPosZ->value = c ? &c->pos.z : NULL;
        if (propContPosAbs) propContPosAbs->value = c ? &g_pos2dAbs : NULL;
        if (propContOpac) propContOpac->value = c ? &c->opacidad : NULL;
        if (propContUnidad && c) propContUnidad->button->text = TamNombreModo(c->tamModo);
        AjustarFilaTam(propContAncho, c ? c->tamModo : TAM2D_PX);
        AjustarFilaTam(propContAlto,  c ? c->tamModo : TAM2D_PX);
        if (c && propContAncla) propContAncla->button->text = T2dNombreAncla(c->ancla);
    }
    // SLICE 9
    if (propS9card && propS9Ancho){
        Slice9* s9 = (ObjActivo && ObjActivo->getType() == ObjectType::slice9) ? (Slice9*)ObjActivo : NULL;
        propS9Ancho->value = s9 ? &s9->ancho : NULL;
        propS9Alto->value  = s9 ? &s9->alto  : NULL;
        propS9Rot->value   = s9 ? &s9->rot2d : NULL;
        if (propS9PosX){ propS9PosX->value = s9 ? &g_pos2dX : NULL; propS9PosX->onChange = AccionPos2DEditada; }
        if (propS9PosY){ propS9PosY->value = s9 ? &g_pos2dY : NULL; propS9PosY->onChange = AccionPos2DEditada; }
        if (propS9PosZ) propS9PosZ->value = s9 ? &s9->pos.z : NULL;
        if (propS9PosAbs) propS9PosAbs->value = s9 ? &g_pos2dAbs : NULL;
        if (propS9Unidad && s9) propS9Unidad->button->text = TamNombreModo(s9->tamModo);
        if (propS9BordeX) propS9BordeX->value = s9 ? &s9->bordeX : NULL;
        if (propS9BordeY) propS9BordeY->value = s9 ? &s9->bordeY : NULL;
        if (propS9EscBorde) propS9EscBorde->value = s9 ? &s9->escalaBorde : NULL;
        if (propS9Opac) propS9Opac->value = s9 ? &s9->opacidad : NULL;
        if (propS9Color){ propS9Color->value = (s9 && s9->palTinte < 0) ? s9->color : NULL;
                          propS9Color->palRef = s9 ? &s9->palTinte : NULL; propS9Color->palObj = s9; }
        if (propS9Pal && s9) propS9Pal->button->text = PalNombre(s9, s9->palTinte);
        if (propS9Filtro) propS9Filtro->value = s9 ? &s9->filtrado : NULL;
        AjustarFilaTam(propS9Ancho, s9 ? s9->tamModo : TAM2D_PX);
        AjustarFilaTam(propS9Alto,  s9 ? s9->tamModo : TAM2D_PX);
        if (s9){
            propS9Textura->button->text = s9->textura.empty() ? std::string(T("Choose..."))
                                                              : NombreDeArchivo(s9->textura);
            if (propS9Ancla) propS9Ancla->button->text = T2dNombreAncla(s9->ancla);
        }
    }
    // BOTON 2D
    if (propBtn2D && propBtnTam){
        Boton2D* b = (ObjActivo && ObjActivo->getType() == ObjectType::boton2d) ? (Boton2D*)ObjActivo : NULL;
        propBtnTam->value = b ? &b->tam : NULL;
        if (propBtnPad)  propBtnPad->value  = b ? &b->pad : NULL;
        if (propBtnPosX){ propBtnPosX->value = b ? &g_pos2dX : NULL; propBtnPosX->onChange = AccionPos2DEditada; }
        if (propBtnPosY){ propBtnPosY->value = b ? &g_pos2dY : NULL; propBtnPosY->onChange = AccionPos2DEditada; }
        if (propBtnPosZ) propBtnPosZ->value = b ? &b->pos.z : NULL;
        if (propBtnPosAbs) propBtnPosAbs->value = b ? &g_pos2dAbs : NULL;
        if (propBtnRot) propBtnRot->value = b ? &b->rot2d : NULL;
        if (propBtnOpac) propBtnOpac->value = b ? &b->opacidad : NULL;
        if (propBtnColFondo){ propBtnColFondo->value = (b && b->palFondo < 0) ? b->colorFondo : NULL;
                              propBtnColFondo->palRef = b ? &b->palFondo : NULL; propBtnColFondo->palObj = b; }
        if (propBtnColTexto){ propBtnColTexto->value = (b && b->palTexto < 0) ? b->colorTexto : NULL;
                              propBtnColTexto->palRef = b ? &b->palTexto : NULL; propBtnColTexto->palObj = b; }
        if (propBtnColBorde){ propBtnColBorde->value = (b && b->palBorde < 0) ? b->colorBorde : NULL;
                              propBtnColBorde->palRef = b ? &b->palBorde : NULL; propBtnColBorde->palObj = b; }
        if (propBtnColHover){ propBtnColHover->value = (b && b->palHover < 0) ? b->colorHover : NULL;
                              propBtnColHover->palRef = b ? &b->palHover : NULL; propBtnColHover->palObj = b; }
        if (b){
            if (propBtnPalFondo) propBtnPalFondo->button->text = PalNombre(b, b->palFondo);
            if (propBtnPalTexto) propBtnPalTexto->button->text = PalNombre(b, b->palTexto);
            if (propBtnPalBorde) propBtnPalBorde->button->text = PalNombre(b, b->palBorde);
            if (propBtnPalHover) propBtnPalHover->button->text = PalNombre(b, b->palHover);
        }
        if (propBtnTexBX) propBtnTexBX->value = (b && !b->texturaFondo.empty()) ? &b->bordeTexX : NULL;
        if (propBtnTexBY) propBtnTexBY->value = (b && !b->texturaFondo.empty()) ? &b->bordeTexY : NULL;
        if (propBtnTexEsc) propBtnTexEsc->value = (b && !b->texturaFondo.empty()) ? &b->escalaBordeTex : NULL;
        if (b){
            if (propBtnIcono) propBtnIcono->button->text = b->icono.empty() ? std::string(T("Choose..."))
                                                                            : NombreDeArchivo(b->icono);
            if (propBtnTex) propBtnTex->button->text = b->texturaFondo.empty() ? std::string(T("Choose..."))
                                                                               : NombreDeArchivo(b->texturaFondo);
            if (propBtnAncla) propBtnAncla->button->text = T2dNombreAncla(b->ancla);
        }
    }
    // VIDEO 2D
    if (propVid2D && propVidAncho){
        Video2D* v = (ObjActivo && ObjActivo->getType() == ObjectType::video2d) ? (Video2D*)ObjActivo : NULL;
        propVidAncho->value = v ? &v->ancho : NULL;
        propVidAlto->value  = v ? &v->alto  : NULL;
        if (propVidPosX){ propVidPosX->value = v ? &g_pos2dX : NULL; propVidPosX->onChange = AccionPos2DEditada; }
        if (propVidPosY){ propVidPosY->value = v ? &g_pos2dY : NULL; propVidPosY->onChange = AccionPos2DEditada; }
        if (propVidPosZ) propVidPosZ->value = v ? &v->pos.z : NULL;
        if (propVidPosAbs) propVidPosAbs->value = v ? &g_pos2dAbs : NULL;
        if (propVidUnidad && v) propVidUnidad->button->text = TamNombreModo(v->tamModo);
        if (propVidLoop) propVidLoop->value = v ? &v->loop : NULL;
        if (propVidAlpha) propVidAlpha->value = v ? &v->usarAlpha : NULL;
        if (propVidPlay) propVidPlay->value = v ? &v->reproducir : NULL;
        if (propVidFiltro) propVidFiltro->value = v ? &v->filtrado : NULL;
        if (propVidRot) propVidRot->value = v ? &v->rot2d : NULL;
        if (propVidOpac) propVidOpac->value = v ? &v->opacidad : NULL;
        AjustarFilaTam(propVidAncho, v ? v->tamModo : TAM2D_PX);
        AjustarFilaTam(propVidAlto,  v ? v->tamModo : TAM2D_PX);
        if (v){
            if (propVidArchivo) propVidArchivo->button->text = v->video.empty() ? std::string(T("Choose..."))
                                                                                : NombreDeArchivo(v->video);
            if (propVidModo) propVidModo->button->text = ImgNombreModo(v->modo);
            if (propVidAncla) propVidAncla->button->text = T2dNombreAncla(v->ancla);
        }
    }
    // EXPANDIR
    if (propExp2D && propExpPeso){
        Expandir2D* ex = (ObjActivo && ObjActivo->getType() == ObjectType::expandir2d) ? (Expandir2D*)ObjActivo : NULL;
        propExpPeso->value = ex ? &ex->peso : NULL;
    }
    // SCRIPT: reconstruir LAS TARJETAS (una por script) si cambio algo
    if (propControl){
        W3dScriptDatos* d = (ObjActivo && ObjActivo->scriptDatos) ? ObjActivo->scriptDatos : NULL;
        int firma = (int)(((size_t)ObjActivo) & 0xffff) * 31;
        if (d) for (size_t i = 0; i < d->scripts.size(); i++)
            firma += (int)d->scripts[i].ruta.size() + (int)d->scripts[i].refs.size() * 1000 + (int)i * 7;
        if (firma != scriptFirma){
            gScriptPropsMulti.clear();
            ScriptValsLimpiar();   // las filas de valor viven lo que las tarjetas
            for (int c = 0; c < kMaxScriptCards; c++){
                GroupPropertie* g = propScriptCards[c];
                for (size_t i = 0; i < g->properties.size(); i++){
                    // el foco de texto/numpad puede estar en una fila de VALOR que se
                    // destruye: soltarlo (mismo criterio que la tarjeta Paletas)
                    PropertieBase* pr = g->properties[i];
                    if (pr->GetType() == PropertyType::Float){
                        PropFloat* pf = (PropFloat*)pr;
                        if (g_textFieldActivo == &pf->field) g_textFieldActivo = NULL;
                        if (g_propFloatEditando == pf) g_propFloatEditando = NULL;
                    }
                    if (pr->GetType() == PropertyType::Text &&
                        g_textFieldActivo == &((PropText*)pr)->field)
                        g_textFieldActivo = NULL;
                    delete pr;
                }
                g->properties.clear();
                if (!ObjActivo || !d || c >= (int)d->scripts.size()) continue;
                // [0] el ARCHIVO del script (click: elegir otro)
                g->name = NombreDeArchivo(d->scripts[c].ruta);
                PropButton* pa = new PropButton("Archivo");
                pa->conLabel = true;
                pa->button->text = NombreDeArchivo(d->scripts[c].ruta);
                pa->action = AccionScriptCardFila;
                g->properties.push_back(pa);
                // [1..n] sus propiedades expuestas (estilo Unity)
                std::vector<W3dScriptProp> props;
                W3dScriptLeerPropiedades(d->scripts[c].ruta, &props);
                gScriptPropsMulti.push_back(props);
                for (size_t pi = 0; pi < props.size(); pi++){
                    if (props[pi].tipo == 2){
                        // propiedad de VALOR: editor nativo segun el subtipo. El valor
                        // mostrado es el configurado de ESTA instancia (o el default del
                        // .lua); editar lo escribe como string en refs (ScriptValGuardar).
                        const char* v = ScriptValorDe(ObjActivo, c, props[pi].nombre);
                        std::string val = *v ? std::string(v) : props[pi].defecto;
                        ScriptValRow* r = new ScriptValRow();
                        r->card = c; r->prop = props[pi].nombre; r->ultimo = val;
                        if (props[pi].subtipo == 0){
                            PropFloat* pf = new PropFloat(props[pi].nombre);
                            r->f = (float)atof(val.c_str());
                            pf->value = &r->f;
                            // default declarado SIN punto -> se edita como entero (id, frame)
                            pf->entero = (props[pi].defecto.find('.') == std::string::npos &&
                                          props[pi].defecto.find('e') == std::string::npos &&
                                          props[pi].defecto.find('E') == std::string::npos);
                            pf->onChange = ScriptValsSincronizar;
                            r->pf = pf;
                            g->properties.push_back(pf);
                        } else if (props[pi].subtipo == 1){
                            PropBool* pb = new PropBool(props[pi].nombre);
                            r->b = (val == "true" || val == "1");
                            pb->value = &r->b;
                            pb->onChange = ScriptValsSincronizar;
                            r->pb = pb;
                            g->properties.push_back(pb);
                        } else {
                            PropText* pt = new PropText(props[pi].nombre, val);
                            r->pt = pt;
                            g->properties.push_back(pt);
                        }
                        gScriptValRows.push_back(r);
                        continue;
                    }
                    PropButton* pb = new PropButton(props[pi].nombre);
                    pb->conLabel = true;
                    pb->button->desplegable = true;
                    const char* v = ScriptValorDe(ObjActivo, c, props[pi].nombre);
                    pb->button->text = *v ? std::string(v)
                        : (props[pi].tipo == 1 && !props[pi].opciones.empty()
                           ? props[pi].opciones[0] : std::string("-"));
                    pb->action = AccionScriptCardFila;
                    g->properties.push_back(pb);
                }
                // ultima fila: QUITAR este script
                PropButton* pq = new PropButton("Quitar script", -1);
                pq->action = AccionScriptCardFila;
                g->properties.push_back(pq);
            }
            scriptFirma = firma;
            PropertiesLayoutDirty = true;   // cambio la cantidad de tarjetas visibles
        }
        // commit de las filas de VALOR editadas (texto tipeado, checkbox, arrastre):
        // corre por frame, ademas del onChange, para no perder el tipeo del PropText
        ScriptValsSincronizar();
        // la tarjeta Control ya no bindea nombre ni visibilidad: es la LISTA de scripts
        // (ver ActualizarPestanias, que la sigue al objeto activo)
    }
    // PALETAS del PROYECTO: reconstruir las filas si cambio la cantidad de
    // colores/paletas o la paleta en edicion (gPalEdit). La tarjeta vive en
    // la pestania 0 (proyecto): no depende del objeto activo.
    if (propPaleta){
        std::vector<Paleta>& ps = W3dPaletas();
        if (gPalEdit >= (int)ps.size()) gPalEdit = ps.empty() ? 0 : (int)ps.size() - 1;
        if (gPalEdit < 0) gPalEdit = 0;
        int n = ps.empty() ? 0 : (int)ps[gPalEdit].colores.size();
        int firma = 1 + n + gPalEdit * 1000 + (int)ps.size() * 100000;
        if (firma != paletaFilas){
            for (size_t i = 0; i < propPaleta->properties.size(); i++){
                PropertieBase* p = propPaleta->properties[i];
                // si el foco de texto estaba en el nombre de una fila que se destruye,
                // soltarlo (si no, g_textFieldActivo queda apuntando a memoria borrada)
                if (p->GetType() == PropertyType::Color){
                    PropColorPal* pp = (PropColorPal*)p;
                    if (pp->PaletaIdx() >= 0 && g_textFieldActivo == &pp->field)
                        g_textFieldActivo = NULL;
                }
                if (p->GetType() == PropertyType::Text &&
                    g_textFieldActivo == &((PropText*)p)->field)
                    g_textFieldActivo = NULL;
                delete p;
            }
            propPaleta->properties.clear();
            propPaletaSel = NULL;
            propPaletaNombre = NULL;
            // [0] el desplegable: cual paleta EDITA la tarjeta / nueva / borrar
            propPaletaSel = new PropButton(T("Palette"));
            propPaletaSel->conLabel = true;
            propPaletaSel->button->desplegable = true;
            propPaletaSel->button->text = ps.empty() ? "-" : ps[gPalEdit].nombre;
            propPaletaSel->action = AccionMenuPaletas;
            propPaleta->properties.push_back(propPaletaSel);
            if (!ps.empty()){
                // [1] el NOMBRE de la paleta en edicion (renombra en vivo,
                // propagando a las selecciones: SincronizarNombrePaleta)
                propPaletaNombre = new PropText(T("Name"), ps[gPalEdit].nombre);
                propPaleta->properties.push_back(propPaletaNombre);
                // una fila COMPACTA por color: nombre editable + swatch + boton X.
                // OJO invariantes: el nombre y la X actuan sobre TODAS las paletas
                // (via W3dPaletaBorrarColor); el swatch edita SOLO la editada.
                std::vector<PaletaColor>& cs = ps[gPalEdit].colores;
                for (int i = 0; i < n; i++){
                    PropColorPal* col = new PropColorPal(cs[i].nombre, i);
                    col->value = cs[i].rgba;    // punteros ESTABLES (colores con reserve)
                    col->nom = &cs[i].nombre;
                    propPaleta->properties.push_back(col);
                }
                PropButton* mas = new PropButton(T("Add Color"), IconType::material);
                mas->action = AccionPaletaAgregar;
                propPaleta->properties.push_back(mas);
            }
            paletaFilas = firma;
            PropertiesLayoutDirty = true;   // cambio la altura de la tarjeta
        }
        // el nombre editable de la paleta (commit en vivo + display al desenfocar)
        SincronizarNombrePaleta(this);
        // INVARIANTE: los NOMBRES de los colores van por INDICE y son los
        // mismos en TODAS las paletas (renombrar uno en la tarjeta lo
        // renombra en todas; la fuente es la paleta en edicion)
        // ...y ademas son UNICOS entre si (el rename in-place de la fila PropColorPal no
        // chequeaba nada: dos colores homonimos eran triviales)
        if (!ps.empty())
            for (size_t i = 0; i < ps[gPalEdit].colores.size(); i++){
                const std::string libre = W3dPaletaColorNombreLibre(ps[gPalEdit].colores[i].nombre, (int)i);
                if (libre != ps[gPalEdit].colores[i].nombre) ps[gPalEdit].colores[i].nombre = libre;
                const std::string& src = ps[gPalEdit].colores[i].nombre;
                for (size_t j = 0; j < ps.size(); j++)
                    if ((int)j != gPalEdit && i < ps[j].colores.size() &&
                        ps[j].colores[i].nombre != src)
                        ps[j].colores[i].nombre = src;
            }
    }
    // la SELECCION de paleta del objeto activo ("Igual que el padre" o una del proyecto)
    if (propPaletaObjSel)
        propPaletaObjSel->button->text = (ObjActivo && !ObjActivo->paleta.empty())
                                             ? ObjActivo->paleta
                                             : std::string("Igual que el padre");
    // UI: la tarjeta de la raiz de la interfaz. Las filas responsive solo aparecen con
    // "como el render" apagado (value NULL / oculto: no ocupan fila).
    if (propUIver3D){
        UI* u = (ObjActivo && ObjActivo->getType() == ObjectType::ui) ? (UI*)ObjActivo : NULL;
        propUIver3D->value = u ? &u->verEn3D : NULL;
        bool resp = (u && !u->igualQueRender);
        if (propUIigualRender) propUIigualRender->value = u ? &u->igualQueRender : NULL;
        if (propUIancho) propUIancho->value = resp ? &u->ancho : NULL;
        if (propUIalto)  propUIalto->value  = resp ? &u->alto  : NULL;
        if (propUIres){     propUIres->oculto = !resp;
                            if (u) propUIres->button->text = UINombreRes(u->resPreset); }
        if (propUIaspecto){ propUIaspecto->oculto = !resp;
                            if (u) propUIaspecto->button->text = UINombreAspecto(u->aspectoPreset); }
        if (propUIrotar)    propUIrotar->oculto = !resp;
        if (propUIopac)     propUIopac->value = u ? &u->opacidad : NULL;
        if (propUIcolor)    propUIcolor->value = u ? u->color : NULL;
        if (propUIescala)   propUIescala->value = u ? &u->escalaGlobal : NULL;
    }
    // Children (padding por lado + layout + gap): del elemento 2D o UI activo
    if (propHijosPadIzq){
        float *pi = NULL, *pd = NULL, *pa = NULL, *pb = NULL;
        if (ObjActivo){
            if (ObjActivo->getType() == ObjectType::ui){
                UI* u2 = (UI*)ObjActivo;
                pi = &u2->padIzq; pd = &u2->padDer; pa = &u2->padArr; pb = &u2->padAba;
            } else if (UI2D_EsElemento2D(ObjActivo)){
                Elemento2D* e2 = (Elemento2D*)ObjActivo;
                pi = &e2->padIzq; pd = &e2->padDer; pa = &e2->padArr; pb = &e2->padAba;
            }
        }
        // el padding puede editarse con UN solo valor (uniforme; el valor unico ES padIzq
        // y el sincronizador per-frame replica a los otros 3) o POR LADO
        bool* pu = NULL;
        if (ObjActivo){
            if (ObjActivo->getType() == ObjectType::ui) pu = &((UI*)ObjActivo)->padUni;
            else if (UI2D_EsElemento2D(ObjActivo)) pu = &((Elemento2D*)ObjActivo)->padUni;
        }
        bool uni = (pu && *pu);
        if (propHijosPadUni)   propHijosPadUni->value   = pu;
        if (propHijosPadTodos) propHijosPadTodos->value = (pi && uni) ? pi : NULL;
        propHijosPadIzq->value = uni ? NULL : pi;
        propHijosPadDer->value = uni ? NULL : pd;
        propHijosPadArr->value = uni ? NULL : pa;
        propHijosPadAba->value = uni ? NULL : pb;
        int* lay = HijosLayoutDe(ObjActivo);
        if (propHijosGap) propHijosGap->value = (lay && *lay != 0) ? HijosGapDe(ObjActivo) : NULL;
        if (propHijosLayout && lay) propHijosLayout->button->text = HijosNombreLayout(*lay);
        // Fit y Align: solo con layout activo (Align ademas solo con ajuste MINIMO).
        // Distribucion: solo con ajuste MINIMO (con estirar no hay sobrante que
        // repartir); con una distribucion activa el Align no aplica y se oculta.
        int* aj = HijosAjusteDe(ObjActivo);
        int* al = HijosAlignDe(ObjActivo);
        int* di = HijosDistribDe(ObjActivo);
        if (propHijosAjuste){
            propHijosAjuste->oculto = !(lay && *lay != 0);
            if (aj) propHijosAjuste->button->text = HijosNombreAjuste(*aj);
        }
        if (propHijosDistrib){
            propHijosDistrib->oculto = !(lay && *lay != 0 && aj && *aj == 1);
            if (di) propHijosDistrib->button->text = HijosNombreDistrib(*di);
        }
        if (propHijosAlign){
            propHijosAlign->oculto = !(lay && *lay != 0 && aj && *aj == 1 &&
                                       (!di || *di == 0));
            if (al) propHijosAlign->button->text = HijosNombreAlign(*al);
        }
        bool* pgpx = HijosPadGapPxDe(ObjActivo);
        if (propHijosPx) propHijosPx->value = pgpx;
        // overflow + scroll (los Scroll X/Y solo aparecen con el scroll permitido)
        if (propHijosClipX)  propHijosClipX->value  = HijosClipXDe(ObjActivo);
        if (propHijosClipY)  propHijosClipY->value  = HijosClipYDe(ObjActivo);
        if (propHijosScroll) propHijosScroll->value = HijosScrollDe(ObjActivo);
        bool* scr = HijosScrollDe(ObjActivo);
        if (propHijosScrollX) propHijosScrollX->value = (scr && *scr) ? HijosScrollXDe(ObjActivo) : NULL;
        if (propHijosScrollY) propHijosScrollY->value = (scr && *scr) ? HijosScrollYDe(ObjActivo) : NULL;
        // unidades y rangos segun el modo (px o proporcion)
        bool enPx = !pgpx || *pgpx;
        PropFloat* pads[5] = { propHijosPadIzq, propHijosPadDer, propHijosPadArr, propHijosPadAba,
                               propHijosPadTodos };
        for (int k = 0; k < 5; k++) if (pads[k]){
            pads[k]->unit = enPx ? "px" : "";
            pads[k]->SetRango(0.0f, enPx ? 2048.0f : 0.49f);
            pads[k]->stepFino = enPx ? 1.0f : 0.005f;
            pads[k]->stepGrueso = enPx ? 10.0f : 0.05f;
            pads[k]->dragStep = enPx ? 1.0f : 0.002f;
        }
        if (propHijosGap){
            propHijosGap->unit = enPx ? "px" : "";
            propHijosGap->SetRango(0.0f, enPx ? 1024.0f : 1.0f);
            propHijosGap->stepFino = enPx ? 1.0f : 0.005f;
            propHijosGap->stepGrueso = enPx ? 10.0f : 0.05f;
            propHijosGap->dragStep = enPx ? 1.0f : 0.002f;
        }
    }
    // MARGEN + EXPANDIR del elemento activo (solo aplican con el padre en filas/columnas;
    // la visibilidad de la tarjeta se decide aparte). Uniforme: mismo criterio que padding.
    if (propMargExp){
        Elemento2D* em = NULL;
        if (ObjActivo && UI2D_EsElemento2D(ObjActivo) &&
            ObjActivo->getType() != ObjectType::expandir2d){
            int* layP = HijosLayoutDe(ObjActivo->Parent);
            if (layP && *layP != 0) em = (Elemento2D*)ObjActivo;
        }
        bool mu = (em && em->margUni);
        propMargExp->value = em ? &em->expandir : NULL;
        if (propMargUni)   propMargUni->value   = em ? &em->margUni : NULL;
        if (propMargTodos) propMargTodos->value = (em && mu)  ? &em->margIzq : NULL;
        if (propMargIzq)   propMargIzq->value   = (em && !mu) ? &em->margIzq : NULL;
        if (propMargDer)   propMargDer->value   = (em && !mu) ? &em->margDer : NULL;
        if (propMargArr)   propMargArr->value   = (em && !mu) ? &em->margArr : NULL;
        if (propMargAba)   propMargAba->value   = (em && !mu) ? &em->margAba : NULL;
        // la unidad del margen la decide el PADRE (px o proporcional, como su gap)
        bool* pgP = em ? HijosPadGapPxDe(ObjActivo->Parent) : NULL;
        bool mPx = !pgP || *pgP;
        PropFloat* ms[5] = { propMargTodos, propMargIzq, propMargDer, propMargArr, propMargAba };
        for (int k = 0; k < 5; k++) if (ms[k]){
            ms[k]->unit = mPx ? "px" : "";
            ms[k]->SetRango(0.0f, mPx ? 2048.0f : 0.49f);
            ms[k]->stepFino = mPx ? 1.0f : 0.005f;
            ms[k]->stepGrueso = mPx ? 10.0f : 0.05f;
            ms[k]->dragStep = mPx ? 1.0f : 0.002f;
        }
    }
    // si el PADRE esta en filas/columnas, la posicion del hijo no se edita (se acomoda sola)
    {
        Object* e2d = (ObjActivo && UI2D_EsElemento2D(ObjActivo)) ? ObjActivo : NULL;
        int* layPadre = e2d ? HijosLayoutDe(e2d->Parent) : NULL;
        bool enLayout = (layPadre && *layPadre != 0);
        if (enLayout){
            if (propT2dPosX)  propT2dPosX->value  = NULL;
            if (propT2dPosY)  propT2dPosY->value  = NULL;
            if (propT2dPosAbs)  propT2dPosAbs->value  = NULL;
            if (propImgPosX)  propImgPosX->value  = NULL;
            if (propImgPosY)  propImgPosY->value  = NULL;
            if (propImgPosAbs)  propImgPosAbs->value  = NULL;
            if (propRectPosX) propRectPosX->value = NULL;
            if (propRectPosY) propRectPosY->value = NULL;
            if (propRectPosAbs) propRectPosAbs->value = NULL;
            if (propContPosX) propContPosX->value = NULL;
            if (propContPosY) propContPosY->value = NULL;
            if (propContPosAbs) propContPosAbs->value = NULL;
            if (propS9PosX) propS9PosX->value = NULL;
            if (propS9PosY) propS9PosY->value = NULL;
            if (propS9PosAbs) propS9PosAbs->value = NULL;
        }
        // el PESO solo cuenta (y se muestra) cuando el padre esta en filas/columnas
        float* peso = (e2d && enLayout) ? &((Elemento2D*)e2d)->peso : NULL;
        if (propT2dPeso)  propT2dPeso->value  = (e2d && e2d->getType() == ObjectType::texto2d)  ? peso : NULL;
        if (propImgPeso)  propImgPeso->value  = (e2d && e2d->getType() == ObjectType::imagen2d) ? peso : NULL;
        if (propRectPeso) propRectPeso->value = (e2d && e2d->getType() == ObjectType::rect2d)   ? peso : NULL;
        if (propContPeso) propContPeso->value = (e2d && e2d->getType() == ObjectType::cont2d)   ? peso : NULL;
        if (propS9Peso)   propS9Peso->value   = (e2d && e2d->getType() == ObjectType::slice9)   ? peso : NULL;
        if (propVidPeso)  propVidPeso->value  = (e2d && e2d->getType() == ObjectType::video2d)  ? peso : NULL;
        if (propBtnPeso)  propBtnPeso->value  = (e2d && e2d->getType() == ObjectType::boton2d)  ? peso : NULL;
        if (propBtnPosX && e2d && e2d->getType() == ObjectType::boton2d && enLayout){
            propBtnPosX->value = NULL;
            if (propBtnPosY) propBtnPosY->value = NULL;
            if (propBtnPosAbs) propBtnPosAbs->value = NULL;
        }
    }

    // LUZ: bindea TODAS las propiedades por member (NULL si el activo no es luz -> no editable). Guard contra
    // punteros sin construir (propLightDir == el primero de los nuevos) -> nunca deref de basura.
    if (propLight && propLightDir){
        bool esLuz = ObjActivo->getType() == ObjectType::light;
        Light* l = esLuz ? static_cast<Light*>(ObjActivo) : NULL;
        propLightDir->value      = l ? &l->direccional   : NULL;
        if (l) g_lightGLIdx = (float)(l->LightID - GL_LIGHT0);
        propLightGL->value       = l ? &g_lightGLIdx     : NULL;
        propLightDiffuse->value  = l ? l->diffuse        : NULL;
        propLightAmbient->value  = l ? l->ambient        : NULL;
        propLightSpecular->value = l ? l->specular       : NULL;
        propLightAttC->value     = l ? &l->attConstant   : NULL;
        propLightAttL->value     = l ? &l->attLinear     : NULL;
        propLightAttQ->value     = l ? &l->attQuadratic  : NULL;
        propLightSpotCut->value  = l ? &l->spotCutoff    : NULL;
        propLightSpotExp->value  = l ? &l->spotExponent  : NULL;
    }

    // CAMARA: bindear el lente (fov + ortografica) a los campos del panel
    if (propCamera && propCamFov){
        Camera* c = (ObjActivo->getType() == ObjectType::camera) ? static_cast<Camera*>(ObjActivo) : NULL;
        propCamFov->value   = c ? &c->fov          : NULL;
        propCamOrtho->value = c ? &c->orthographic : NULL;
        propCamNear->value  = c ? &c->nearClip     : NULL;
        propCamFar->value   = c ? &c->farClip      : NULL;
    }

    // LOD: el checkbox bindea directo al campo del objeto activo (NULL = no editable)
    if (propLodSoloCam){
        LOD* lo = (ObjActivo->getType() == ObjectType::lod) ? static_cast<LOD*>(ObjActivo) : NULL;
        propLodSoloCam->value = lo ? &lo->soloCamaraActiva : NULL;
    }

    // CULLING: el checkbox bindea directo al campo del objeto activo (NULL = no editable)
    if (propCullSoloCam){
        Culling* cu = (ObjActivo->getType() == ObjectType::culling) ? static_cast<Culling*>(ObjActivo) : NULL;
        if (propCullActivo)  propCullActivo->value  = cu ? &cu->activo : NULL;
        propCullSoloCam->value = cu ? &cu->soloCamaraActiva : NULL;
        if (propCullDistMax) propCullDistMax->value = cu ? &cu->distanciaMax : NULL;
    }

    // PARTICULAS: numeros y checks directo a los campos del activo (textura y
    // color van por Sincronizar*, son de texto)
    if (propPartCantidad){
        Particulas* pt = (ObjActivo->getType() == ObjectType::particulas)
                       ? static_cast<Particulas*>(ObjActivo) : NULL;
        propPartCantidad->value   = pt ? &pt->cantidad   : NULL;
        propPartVida->value       = pt ? &pt->vida       : NULL;
        propPartTam->value        = pt ? &pt->tam        : NULL;
        propPartVel->value        = pt ? &pt->vel        : NULL;
        propPartDispersion->value = pt ? &pt->dispersion : NULL;
        propPartGravedad->value   = pt ? &pt->gravedad   : NULL;
        propPartVariacion->value  = pt ? &pt->variacion  : NULL;
        propPartTurbulencia->value = pt ? &pt->turbulencia : NULL;
        if (propPartRotacion) propPartRotacion->value = pt ? &pt->rotacion    : NULL;
        if (propPartVelRot)   propPartVelRot->value   = pt ? &pt->velRotacion : NULL;
        propPartAditivo->value    = pt ? &pt->aditivo    : NULL;
        if (propPartSustractivo) propPartSustractivo->value = pt ? &pt->sustractivo : NULL;
        propPartDesvanecer->value = pt ? &pt->desvanecer : NULL;
        propPartActivo->value     = pt ? &pt->activo     : NULL;
    }

    // COLLECTION: idem. OJO: la raiz Scene tambien reporta tipo collection
    // (Scene.cpp) pero NO es una Collection -> el chequeo de Parent la descarta
    // (las raices no tienen padre) y el cast nunca la toca.
    if (propCollOrdenCam && propCollOrdenUnaVez){
        Collection* col = (ObjActivo->getType() == ObjectType::collection && ObjActivo->Parent)
                        ? static_cast<Collection*>(ObjActivo) : NULL;
        propCollOrdenCam->value    = col ? &col->ordenarPorCamara : NULL;
        propCollOrdenUnaVez->value = col ? &col->ordenarUnaVez    : NULL;
    }

    Resize(width, height);
}

// Constructor
Properties::Properties() : ViewportBase() {
    // (eran inicializadores de clase: C++03)
    target = NULL;
    maxPixelsTitle = 1920;
    selectIndex = 0;
    editando = false;
    propTransform = NULL;
    propMeshParts = NULL;
    propLight = NULL;
    propTexto2D = NULL; propT2dTexto = NULL; propT2dTam = NULL;
    propT2dAlignH = NULL; propT2dAlignV = NULL; propT2dColor = NULL; propT2dFuente = NULL;
    propT2dAncla = NULL; propT2dRot = NULL; propUIcard = NULL; propUIver3D = NULL;
    propImagen2D = NULL; propImgTextura = NULL; propImgAncho = NULL; propImgAlto = NULL;
    propImgRot = NULL; propImgModo = NULL; propImgAncla = NULL;
    propUIigualRender = NULL; propUIancho = NULL; propUIalto = NULL;
    propUIres = NULL; propUIaspecto = NULL; propUIrotar = NULL;
    propT2dNombre = NULL; propT2dPosX = NULL; propT2dPosY = NULL; propT2dPosZ = NULL;
    propT2dOpac = NULL; propImgNombre = NULL; propImgPosX = NULL; propImgPosY = NULL;
    propImgPosZ = NULL; propImgOpac = NULL; propUInombre = NULL; propUIopac = NULL;
    propHijos = NULL; propHijosLayout = NULL; propHijosGap = NULL;
    propHijosPadIzq = NULL; propHijosPadDer = NULL; propHijosPadArr = NULL; propHijosPadAba = NULL;
    propHijosPx = NULL;
    propT2dPosAbs = NULL; propT2dTipo = NULL; propT2dDec = NULL; propImgPosAbs = NULL;
    propUIescala = NULL; propUIexport = NULL;
    propRect2D = NULL; propRectNombre = NULL; propRectPosX = NULL; propRectPosY = NULL;
    propRectPosZ = NULL; propRectPosAbs = NULL; propRectAncho = NULL; propRectAlto = NULL;
    propRectRot = NULL; propRectAncla = NULL; propRectOpac = NULL; propRectColor = NULL;
    propT2dPeso = NULL; propT2dLineas = NULL; propT2dAutoTam = NULL;
    propImgPeso = NULL; propRectPeso = NULL;
    propCont2D = NULL; propContNombre = NULL; propContPosX = NULL; propContPosY = NULL;
    propContPosZ = NULL; propContPosAbs = NULL; propContPeso = NULL; propContAncho = NULL;
    propContAlto = NULL; propContRot = NULL; propContAncla = NULL; propContOpac = NULL;
    propHijosAjuste = NULL; propHijosAlign = NULL; propHijosDistrib = NULL;
    propBtn2D = NULL; propBtnNombre = NULL; propBtnPosX = NULL; propBtnPosY = NULL;
    propBtnPosZ = NULL; propBtnPosAbs = NULL; propBtnPeso = NULL; propBtnTexto = NULL;
    propBtnIcono = NULL; propBtnTam = NULL; propBtnPad = NULL; propBtnAncla = NULL;
    propBtnOpac = NULL; propBtnColFondo = NULL; propBtnColTexto = NULL; propBtnColBorde = NULL;
    propExp2D = NULL; propExpNombre = NULL; propExpPeso = NULL;
    propVid2D = NULL; propVidNombre = NULL; propVidPosX = NULL; propVidPosY = NULL;
    propVidPosZ = NULL; propVidPosAbs = NULL; propVidPeso = NULL; propVidArchivo = NULL;
    propVidAncho = NULL; propVidAlto = NULL; propVidUnidad = NULL; propVidModo = NULL;
    propVidLoop = NULL; propVidAlpha = NULL; propVidPlay = NULL; propVidFiltro = NULL;
    propVidAncla = NULL; propVidRot = NULL; propVidOpac = NULL;
    propBtnRot = NULL;
    propMargen = NULL; propMargExp = NULL; propMargUni = NULL; propMargTodos = NULL;
    propMargIzq = NULL; propMargDer = NULL; propMargArr = NULL; propMargAba = NULL;
    propHijosPadUni = NULL; propHijosPadTodos = NULL;
    propPaleta = NULL; paletaFilas = -1; propPaletaSel = NULL; propPaletaNombre = NULL;
    propPaletaObj = NULL; propPaletaObjSel = NULL;
    propControl = NULL; scriptFirma = -1;
    propListScripts = NULL; propRowScript = NULL; propRowScriptMove = NULL;
    for (int i = 0; i < kMaxScriptCards; i++) propScriptCards[i] = NULL;
    propJuego = NULL; propJuegoCompilar = NULL; propJuegoPlat = NULL; propJuegoModoVent = NULL;
    propJuegoOrient = NULL; propJuegoIcono = NULL; propJuegoFisica = NULL; propJuegoSonido = NULL;
    propObjAnim = NULL; propListObjAnims = NULL;
    propAnimConservar = NULL;
    propBtnPalFondo = NULL; propBtnPalTexto = NULL; propBtnPalBorde = NULL;
    propBtnPalHover = NULL; propBtnColHover = NULL;
    propBtnTex = NULL; propBtnTexBX = NULL; propBtnTexBY = NULL; propBtnTexEsc = NULL;
    propT2dPal = NULL; propImgPal = NULL; propRectPal = NULL; propS9Pal = NULL;
    propHijosClipX = NULL; propHijosClipY = NULL; propHijosScroll = NULL;
    propHijosScrollX = NULL; propHijosScrollY = NULL;
    propImgUnidad = NULL; propImgColor = NULL; propImgAlpha = NULL;
    propRectUnidad = NULL; propContUnidad = NULL; propUIcolor = NULL;
    propS9card = NULL; propS9Nombre = NULL; propS9PosX = NULL; propS9PosY = NULL;
    propS9PosZ = NULL; propS9PosAbs = NULL; propS9Peso = NULL; propS9Textura = NULL;
    propS9Ancho = NULL; propS9Alto = NULL; propS9Unidad = NULL;
    propS9BordeX = NULL; propS9BordeY = NULL;
    propS9EscBorde = NULL; propS9Rot = NULL; propS9Ancla = NULL; propS9Opac = NULL;
    propS9Color = NULL; propImgFiltro = NULL; propS9Filtro = NULL;
    propCamera = NULL;
    propCamOrtho = NULL; propCamFov = NULL; propCamNear = NULL; propCamFar = NULL;
    propInstance = NULL;
    propLOD = NULL; propLodDist = NULL;           // objeto LOD (umbrales de distancia)
    propObjRelLines = NULL;                                            // lineas parentales (tarjeta generica)
    propLodSoloCam = NULL;                                             // objeto LOD (camara de medida)
    propCulling = NULL; propCullSoloCam = NULL; propCullDistMax = NULL; // objeto Culling
    propCollection = NULL; propCollOrdenCam = NULL; propCollOrdenUnaVez = NULL; // Collection (orden transparentes)
    propParticulas = NULL; propPartTextura = NULL; propPartCantidad = NULL;     // objeto Particulas
    propPartVida = NULL; propPartTam = NULL; propPartVel = NULL; propPartDispersion = NULL;
    propPartGravedad = NULL; propPartAditivo = NULL; propPartSustractivo = NULL; propPartColor = NULL;
    propPartDesvanecer = NULL; propPartActivo = NULL;
    propPartVariacion = NULL; propPartTurbulencia = NULL;
    propPartRotacion = NULL; propPartVelRot = NULL;
    propBtnCamTarget = NULL;
    propBtnInstTarget = NULL;
    propBtnNewMaterial = NULL;
    propBtnTextura = NULL;
    propBtnNormalTex = NULL; // (faltaba: normal map UI)
    propBtnReflectMode = NULL; // dropdown del modo de Reflection
    // luz: punteros nuevos a NULL (si no se inicializan quedan BASURA y el rebind crashea antes de ConstruirGrupos)
    propLightDir = NULL; propLightGL = NULL; propLightDiffuse = NULL; propLightAmbient = NULL; propLightSpecular = NULL;
    propLightAttC = NULL; propLightAttL = NULL; propLightAttQ = NULL; propLightSpotCut = NULL; propLightSpotExp = NULL;
    propEditItem = NULL; editPosX = editPosY = editPosZ = 0.0f;
    propUVTransform = NULL; uvPosU = uvPosV = 0.0f; // tarjeta "Transform UV" (pestania Transformar)
    propUVMaps = NULL; propColorLayers = NULL; propVertexGroups = NULL; propUVGroups = NULL; propModifiers = NULL;
    propListModifiers = NULL; propRowMod = NULL; propRowModMove = NULL; propModifierProps = NULL;
    propModVerViewport = NULL; propModVerEdit = NULL;
    propModVacio = NULL; propMirX = NULL; propMirY = NULL; propMirZ = NULL; propMirTarget = NULL; propArmTarget = NULL;
    propMirMerge = NULL; propMirDist = NULL; propMirClip = NULL; propBtnApplyMod = NULL;
    propPvsMetodo = NULL; propPvsRecalc = NULL; propPvsInfo = NULL; // modificador Culling (PVS)
    propSubSimple = NULL; propSubLevel = NULL; propSubRender = NULL;
    propScrewAngle = NULL; propScrewHeight = NULL; propScrewSteps = NULL; propScrewRender = NULL;
    propScrewAxis = NULL; propScrewStretchU = NULL; propScrewStretchV = NULL;
    propScrewSmooth = NULL; propScrewMerge = NULL; propScrewFlip = NULL;
    propListUV = NULL; propListColor = NULL; propListVertGroups = NULL; propListUVGroups = NULL; propBtnColorMode = NULL;
    propRowUVOps = NULL; propRowColorOps = NULL; propRowGroupOps = NULL; propBtnRenameGroup = NULL;
    propRowUVGroupOps = NULL; propBtnRenameUVGroup = NULL;
    propRowGroupAsig = NULL; propRowGroupSel = NULL;
    propRowUVGroupAsig = NULL; propRowUVGroupSel = NULL;
    propArmAnim = NULL; propListAnims = NULL; propBtnRenameAnim = NULL; propBtnDupAnim = NULL; propRowAnimOps = NULL;
    propArmBones = NULL; propListBones = NULL; propBoneNombre = NULL; propBoneParent = NULL;
    propBones2D = NULL; propListBones2D = NULL; propBone2DNombre = NULL; propBone2DParent = NULL;
    propListArm2Ds = NULL; propBtnRenameArm2D = NULL;
    propBone2DConectado = NULL; propBoneConectado = NULL;
    propConstraints = NULL; propListConstraints = NULL; propRowCon = NULL; propRowConMove = NULL;
    propConstraintProps = NULL; propConActivo = NULL; propConVerEdit = NULL; propConFuente = NULL;
    propConAvisoFuente = NULL; propConInfluencia = NULL;
    propConEjeX = NULL; propConEjeY = NULL; propConEjeZ = NULL; propConAvisoEjes = NULL;
    propConBBModo = NULL; propConAvisoBB = NULL;
    propRotMode = NULL;
    propMsgDefault = NULL; propSepMat = NULL;
    propMaterial = NULL; propBtnRenameMat = NULL;
    propBtnRenameUV = NULL; propBtnRenameColor = NULL; propNameObj = NULL;
    propRowPartOps = NULL; propRowDelRen = NULL; propRowPartMove = NULL;
    for (int i = 0; i < 11; i++) propMatChk[i] = NULL;   // los 11 (antes 10: [10] quedaba con basura)
    for (int i = 0; i < 3; i++) propMatCol[i] = NULL;
    propMatShin = NULL;
    pestaniaActiva = 1;      // arranca en "Objeto" (transforms); 0 = Render
    exportFormat = 2;             // por defecto glTF (el formato con rig + animaciones)
    exportSelectedOnly = true;    // por defecto ON: exporta solo lo seleccionado
    OnSeleccionarAnimClip = SincronizarAnimClipDesdeLista; // la lista de anims (tab Armature) sincroniza el timeline
    OnSeleccionarAnimVertex = SincronizarAnimVertexDesdeLista; // lista de anims del OBJETO -> timeline (kind 3)
    W3dKeyframeEstado = PropKeyEstadoHook;   // el rombo de keyframe del panel
    W3dKeyframeToggle = PropKeyToggleHook;
    ActivaAnimVertexDe = ActivaAnimVertexDeMesh;               // la lista marca la activa
    exportApplyModifiers = true;  // por defecto ON (como Blender)
    exportApplyTransforms = true; // por defecto ON
    exportLastSerial = 0;
    focoEnTabs = false;
    ConstruirGrupos(); // grupos PROPIOS: panel independiente
    BarCrear();
    // pestania 0: "Render" (icono MONITOR: la salida); 1: "Objeto" (transforms);
    // 2: contextual (Mesh/Light/Camera/Instance, solo segun el objeto activo)
    // El monitor y no una camara: la pestania 2 YA es una camara cuando el objeto activo es una Camera, y dos
    // camaras juntas en la misma barra no se entienden.
    Tab* tRender = new Tab("", IconType::guardar);   // la pestania es, al final, para GUARDAR (archivo/render)
    BarTabs.push_back(tRender);
    Tab* tObj = new Tab("", IconType::object);
    tObj->activa = true;
    BarTabs.push_back(tObj);
    Tab* tMesh = new Tab("", IconType::material);
    BarTabs.push_back(tMesh);
    // pestania 3: "Vertices" (icono mesh): UV Maps + capas de color (SOLO meshes)
    Tab* tVerts = new Tab("", IconType::mesh);
    BarTabs.push_back(tVerts);
    // pestania 4: "Modifiers" (icono llave 95,1): tarjeta Modifiers (SOLO meshes)
    Tab* tMods = new Tab("", IconType::modificador);
    BarTabs.push_back(tMods);
    // pestania 5: "Transformar" (el MISMO icono del modo de seleccion de Edit Mode): tarjetas
    // "Transform Mesh" + "Transform UV". SOLO en Edit Mode con malla activa.
    Tab* tTrans = new Tab("", IconType::selVertex);
    BarTabs.push_back(tTrans);
    // pestania 6: "ARMATURE 2D" (icono esqueleto, = el de la pestania del armature 3D): la tarjeta
    // "Armature 2D" con la lista de huesos + Name/Parent/Connected/Pos y, posando, Rotation/Scale.
    // SOLO mientras un editor UV esta en Edit Bones/Pose sobre la malla activa (ver Bones2DMeshUI).
    Tab* tArm2D = new Tab("", IconType::armature);
    BarTabs.push_back(tArm2D);
    // pestania 7: "Constraints" (icono constraint, el que ya existia del objeto Constraint viejo):
    // las tarjetas "Constraints" (el stack) y "Constraint" (el elegido). Cualquier objeto 3D.
    // VA AL FINAL, y una pestania nueva tambien: 'pestaniaActiva' son numeros literales por todo
    // el archivo (y en los tests) -- intercalar una corre TODOS.
    Tab* tCons = new Tab("", IconType::constraint);
    BarTabs.push_back(tCons);
    // pestania 8: "Scripts" (icono gamepad, el mismo del objeto Script): la tarjeta Control
    // ("Agregar script") + una tarjeta por script, para CUALQUIER objeto seleccionado (estilo
    // Unity: cada objeto tiene su .lua). El objeto Script NO la usa: sus scripts ya
    // viven en su pestania contextual (2). AL FINAL, como manda el comentario de arriba.
    Tab* tScripts = new Tab("", IconType::gamepad);
    BarTabs.push_back(tScripts);
}

// segun el objeto activo y la pestania elegida: que tab se ve, cual esta
// activa, y que grupo de propiedades se muestra
void Properties::ActualizarPestanias(){
    // la 1ra pestania ("Objeto") siempre esta (transforms). La 2da depende del
    // tipo del objeto activo: Mesh -> mesh parts (icono material); Light ->
    // color (icono luz). (Camara / objetos especiales: a futuro.)
    // pestanias: 0 = Render (export), 1 = Objeto (transforms), 2 = contextual
    int tipo = ObjActivo ? (int)ObjActivo->getType() : -1;
    bool esMesh = (tipo == (int)ObjectType::mesh);
    bool esLuz  = (tipo == (int)ObjectType::light);
    bool esCam  = (tipo == (int)ObjectType::camera);
    bool esInst = (tipo == (int)ObjectType::instance);
    bool esArm  = (tipo == (int)ObjectType::armature);
    bool esT2d  = (tipo == (int)ObjectType::texto2d);
    bool esImg  = (tipo == (int)ObjectType::imagen2d);
    bool esRect = (tipo == (int)ObjectType::rect2d);
    bool esCont = (tipo == (int)ObjectType::cont2d);
    bool esS9   = (tipo == (int)ObjectType::slice9);
    bool esBtn  = (tipo == (int)ObjectType::boton2d);
    bool esExp  = (tipo == (int)ObjectType::expandir2d);
    bool esVid  = (tipo == (int)ObjectType::video2d);
    bool esUI   = (tipo == (int)ObjectType::ui);
    // el objeto SCRIPT (gamepad) NO vive en el espacio 3D: sin transformacion; solo
    // su nombre y sus scripts (el lugar en el outliner es el orden de ejecucion)
    bool esScript = (tipo == (int)ObjectType::script);
    bool esLOD  = (tipo == (int)ObjectType::lod);
    bool esCull = (tipo == (int)ObjectType::culling);
    bool esPart = (tipo == (int)ObjectType::particulas);
    // Collection REAL (no la raiz Scene, que comparte el tipo pero no tiene Parent)
    bool esColl = (tipo == (int)ObjectType::collection && ObjActivo && ObjActivo->Parent);
    bool hayTab3 = esMesh || esLuz || esCam || esInst || esArm || esT2d || esImg || esRect || esCont || esS9 || esBtn || esExp || esVid || esUI || esScript || esLOD || esCull || esColl || esPart;

    if (BarTabs.size() >= 3){
        BarTabs[2]->visible = hayTab3;
        int icono = (int)IconType::material;          // mesh
        if (esLuz)       icono = (int)IconType::light;
        else if (esCam)  icono = (int)IconType::camera;
        else if (esArm)  icono = (int)IconType::armature;      // esqueleto: pestania Animation
        else if (esInst) icono = (int)IconoDeObjeto(ObjActivo); // instance/array/mirror
        else if (esT2d)  icono = (int)IconType::lista;           // elemento de texto 2D
        else if (esImg)  icono = (int)IconType::foto;            // elemento de imagen 2D
        else if (esRect) icono = (int)IconType::plane;           // elemento rectangulo 2D
        else if (esCont) icono = (int)IconType::carpeta;         // contenedor 2D
        else if (esS9)   icono = (int)IconType::cuadricula;      // slice 9
        else if (esBtn)  icono = (int)IconType::object;          // boton
        else if (esExp)  icono = (int)IconType::arrowRight;      // expandir
        else if (esVid)  icono = (int)IconType::camera;          // video
        else if (esUI)   icono = (int)IconType::textura;         // la raiz de la interfaz
        else if (esScript) icono = (int)IconType::gamepad;       // el objeto Script
        else if (esLOD)  icono = (int)IconType::array;           // objeto LOD (niveles)
        else if (esCull) icono = (int)IconType::visible;         // objeto Culling (que se ve)
        else if (esColl) icono = (int)IconType::archive;         // Collection (orden transparentes)
        else if (esPart) icono = (int)IconType::circle;          // objeto Particulas (emisor)
        BarTabs[2]->icon = icono;
    }
    // los objetos 2D no muestran el tab Objeto (su Nombre y Posicion viven arriba de su
    // tarjeta contextual, que era lo unico que se usaba de ahi). Sin objeto activo
    // TAMPOCO hay tab Objeto: estaria vacio (todas sus tarjetas piden seleccion).
    bool es2D = esT2d || esImg || esRect || esCont || esS9 || esBtn || esExp || esVid || esUI;
    if (BarTabs.size() >= 2) BarTabs[1]->visible = ObjActivo != NULL && !es2D && !esScript;
    if (pestaniaActiva == 1 && (es2D || esScript)) pestaniaActiva = 2;
    if (BarTabs.size() >= 4) BarTabs[3]->visible = esMesh; // pestaña Vertices: SOLO meshes
    if (BarTabs.size() >= 5) BarTabs[4]->visible = esMesh; // pestaña Modifiers: SOLO meshes
    // pestana 5 "Transformar": SOLO en Edit Mode con malla activa (la seleccion a transformar
    // vive en la edit mesh). Su icono es el del modo de seleccion (selVertex/selEdge/selFace).
    bool transTabOk = esMesh && InteractionMode == EditMode && g_editMesh == (Object*)ObjActivo;
    if (BarTabs.size() >= 6){
        BarTabs[5]->visible = transTabOk;
        BarTabs[5]->icon = (EditSelectMode == SelEdge) ? (int)IconType::selEdge :
                           (EditSelectMode == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
    }
    // pestana 6 "Armature 2D": mientras un editor UV esta en Edit Bones / Pose / MODO OBJETO sobre
    // la malla activa en Edit Mode y esa malla tiene algun rig 2D (Arm2DTarjetaMeshUI, el MISMO
    // gate de la tarjeta). En modo objeto se ve solo la LISTA de rigs (Add/Rename/Delete).
    Mesh* arm2dMesh = Arm2DTarjetaMeshUI();
    if (BarTabs.size() >= 7) BarTabs[6]->visible = (arm2dMesh != NULL);
    // pestana 7 "Constraints": cualquier objeto 3D -> la MISMA condicion que la pestania "Objeto"
    // (los constraints modifican la transform, y los 2D/Script no tienen una que modificar). El
    // 'ObjActivo != NULL' esta adentro de ObjConstraintsUI y no es opcional: sin seleccion el
    // panel no muestra NINGUNA pestania de objeto.
    Object* consObj = ObjConstraintsUI();
    if (BarTabs.size() >= 8) BarTabs[7]->visible = (consObj != NULL);
    // pestana 8 "Scripts": CUALQUIER objeto seleccionado (estilo Unity: cajas/frutas/enemigos
    // con su .lua). El objeto Script NO la necesita: sus scripts ya viven en su contextual (2).
    bool scriptsTabOk = (ObjActivo != NULL) && !esScript;
    if (BarTabs.size() >= 9) BarTabs[8]->visible = scriptsTabOk;
    if (pestaniaActiva == 2 && !hayTab3) pestaniaActiva = 1;
    if (pestaniaActiva == 3 && !esMesh)  pestaniaActiva = 1;
    if (pestaniaActiva == 4 && !esMesh)  pestaniaActiva = 1;
    if (pestaniaActiva == 5 && !transTabOk) pestaniaActiva = 1; // salir de Edit Mode cae a Objeto
    // salir de la edicion de huesos 2D (o del Edit Mode) parado en la 6 cae a Objeto, como la 5
    if (pestaniaActiva == 6 && !arm2dMesh) pestaniaActiva = 1;
    // pasar a un elemento 2D / al Script parado en Constraints cae a su tarjeta contextual (2), y
    // desde cualquier objeto 3D a Objeto (1). El "1 -> 2 si es 2D/Script" de mas arriba ya corrio
    // cuando llegamos aca, asi que mandar todo a la 1 dejaria al panel parado en una pestania
    // OCULTA (la que no existe para los 2D): es el mismo caso de la pestania fantasma de palcard.
    // Sin objeto la 1 esta bien: la agarra el fallback a la 0 de abajo.
    if (pestaniaActiva == 7 && !consObj) pestaniaActiva = (es2D || esScript) ? 2 : 1;
    // pasar al objeto Script parado en la 8 cae a su contextual (ahi estan sus scripts);
    // sin objeto el fallback general de abajo la manda a la 0.
    if (pestaniaActiva == 8 && !scriptsTabOk) pestaniaActiva = esScript ? 2 : 1;
    // sin NADA seleccionado no existe ninguna pestania de objeto: caer SIEMPRE a la 0
    // (Archivo/Render/proyecto: el panel siempre muestra algo). OJO al ORDEN: va
    // DESPUES de los fallbacks de arriba, porque 2/3/4 caen primero a la 1 -- cuando
    // esto corria antes (y solo cubria la 1), deseleccionar parado en Material/
    // Vertices/Modifiers dejaba activa la pestania Objeto sin objeto: la pestania
    // fantasma con el panel entero VACIO que se reporto.
    if (!ObjActivo && pestaniaActiva != 0) pestaniaActiva = 0;
    for (size_t i = 0; i < BarTabs.size(); i++){
        BarTabs[i]->activa = ((int)i == pestaniaActiva);
        BarTabs[i]->foco   = (focoEnTabs && (int)i == pestaniaActiva);
    }

    // File name del export por defecto = nombre de lo SELECCIONADO a exportar (mesh/armature) + extension del formato.
    // Sigue la SELECCION (ObjSelects), no solo ObjActivo: al importar, ObjActivo suele quedar en el Cube por defecto, y
    // el export mostraba "cube" en vez del modelo seleccionado. Prioriza el 1er mesh/armature seleccionado.
    { extern std::vector<Object*> ObjSelects;
      Object* expObj = NULL;
      for (size_t i = 0; i < ObjSelects.size() && !expObj; i++) if (ObjSelects[i] && (ObjSelects[i]->getType()==ObjectType::mesh || ObjSelects[i]->getType()==ObjectType::armature)) expObj = ObjSelects[i];
      if (!expObj && ObjActivo && (ObjActivo->getType()==ObjectType::mesh || ObjActivo->getType()==ObjectType::armature)) expObj = ObjActivo;
      // "es el MISMO de la vez pasada?" va por SERIAL (Object::serial), no por puntero: borrar
      // el objeto y crear otro recicla la direccion y el campo se quedaba con el nombre del
      // objeto VIEJO (se exportaba con un File name que no es el del modelo elegido).
      if (propExportName && expObj && expObj->serial != exportLastSerial){
          propExportName->field.SetText(expObj->name + ExtDeFormato(exportFormat));
          exportLastSerial = expObj->serial;
      }
    }

    // mostrar SOLO los grupos de la pestania activa. Render (0) es GLOBAL (ajustes de salida/pases): siempre
    // visible con la pestania activa, con o sin seleccion. El export OBJ SI depende de la seleccion (sin objeto
    // no hay nada que exportar).
    if (propArchivo) propArchivo->visible = (pestaniaActiva == 0);
    if (propRender)    propRender->visible    = (pestaniaActiva == 0);
    if (propAnimation) propAnimation->visible = (pestaniaActiva == 0); // tarjeta Animation: global, como Render
    if (propJuego)     propJuego->visible     = (pestaniaActiva == 0); // Juego: debajo de Animation
    // tarjeta Keyframe: SOLO si hay un keyframe elegido en el editor de curvas. Los campos se refrescan desde la
    // curva viva (que la puede haber movido el propio timeline), salvo el que se este editando a mano.
    if (propKeyframe){
        int ki; AnimProperty* kap = DopeKeyframeActivo(&ki);
        propKeyframe->visible = (pestaniaActiva == 0) && kap != NULL;
        if (propKeyframe->visible){
            const keyFrame& k = kap->keyframes[ki];
            std::string canal = DopeKeyframeActivoCanal();
            propKeyframe->name = canal.empty() ? "Keyframe" : ("Keyframe - " + canal);
            if (g_propFloatEditando != gKfFrame) g_kfFrame = (float)k.frame;
            if (g_propFloatEditando != gKfValor) g_kfValor = k.value;
            if (g_propFloatEditando != gKfInDF)  g_kfInDF  = k.inDF;
            if (g_propFloatEditando != gKfInDV)  g_kfInDV  = k.inDV;
            if (g_propFloatEditando != gKfOutDF) g_kfOutDF = k.outDF;
            if (g_propFloatEditando != gKfOutDV) g_kfOutDV = k.outDV;
            if (gKfInterp) gKfInterp->button->text = KfNombreInterp(k.Interpolation);
            if (gKfHandle) gKfHandle->button->text = KfNombreHandle(k.handleType);
            // los handles SOLO existen si el tramo es bezier, y solo se pueden tipear si el TIPO los guarda
            // (con Vector/Automatic/Auto Clamped los calcula la curva sola: mostrarlos editables seria mentir).
            // value = NULL OCULTA la fila: es el idioma del panel (Resize la mide en 0 y el teclado la saltea).
            bool bez = (k.Interpolation == KfBezier) || (ki > 0 && kap->keyframes[ki-1].Interpolation == KfBezier);
            bool editables = bez && (k.handleType == HFree || k.handleType == HAligned);
            if (gKfInDF)  gKfInDF->value  = editables ? &g_kfInDF  : NULL;
            if (gKfInDV)  gKfInDV->value  = editables ? &g_kfInDV  : NULL;
            if (gKfOutDF) gKfOutDF->value = editables ? &g_kfOutDF : NULL;
            if (gKfOutDV) gKfOutDV->value = editables ? &g_kfOutDV : NULL;
            if (gKfHandle) gKfHandle->button->visible = bez;
        }
    }
    if (propExport)    propExport->visible    = (pestaniaActiva == 0 && ObjActivo != NULL);
    // Ajustes: pestania Render, SIN depender de que haya un objeto (es config del programa, no de la escena)
    if (propAjustes)   propAjustes->visible   = (pestaniaActiva == 0);
    // los selectores muestran lo que hay puesto AHORA (el idioma se puede cambiar desde aca mismo)
    if (propAjIdioma)  propAjIdioma->button->text  = W3dIdiomaNombre(g_idioma);
    if (propAjBackend) propAjBackend->button->text = cfg.graphicsAPI;
    if (propAjSkin)    propAjSkin->button->text    = cfg.SkinName;
    // tarjeta Animation: el dropdown muestra la animacion activa (icono camara=escena / esqueleto=clip); Delete se
    // OCULTA cuando no hay nada que borrar; Render se GRISA sin animaciones. New y Rename siempre visibles.
    if (propAnimation && pestaniaActiva == 0){
        InitSceneAnimations();
        Armature* aSel = ArmActiva();
        int nClips = aSel ? (int)aSel->animations.size() : 0;
        bool clipActivo = (ActiveAnimKind == 1 && ActiveAnimArm);
        if (propBtnAnimSel && propBtnAnimSel->button){
            propBtnAnimSel->button->text = NombreAnimActiva();
            propBtnAnimSel->button->icon = (ActiveAnimKind == 2) ? (int)IconType::gamepad
                                          : clipActivo ? (int)IconType::armature : (int)IconType::camera;
        }
        // dropdown de formato del export: la etiqueta refleja el formato activo
        if (propExportFormat && propExportFormat->button)
            propExportFormat->button->text = NombreFormato(exportFormat);
        // fila New(0) | Duplicate(1) | Delete(2): Duplicate solo con un clip activo; Delete si hay algo que borrar.
        if (propRowAnimNewDel && propRowAnimNewDel->botones.size() >= 3){
            propRowAnimNewDel->botones[1]->visible = clipActivo; // Duplicate: solo si hay un clip de armature activo
            propRowAnimNewDel->botones[2]->visible = clipActivo || SceneAnimations.size() > 1 || !AnimationObjects.empty();
        }
        // Render Animation: se grisa solo si hay CERO animaciones. Siempre existe la escena "Scene" (rendea su rango
        // aunque no tenga keyframes: secuencia estatica) -> nunca se desactiva.
        if (propBtnAnimRender) propBtnAnimRender->gris = (SceneAnimations.empty() && nClips == 0);
        // MODO JUEGO: el Fin y Render Animation desaparecen (la animacion es infinita);
        // "No reemplazar estados" y su nota solo aparecen siendo un juego
        if (gPropAnimEnd) gPropAnimEnd->value = AnimEsJuego ? NULL : &g_animEndF;
        if (propBtnAnimRender) propBtnAnimRender->oculto = AnimEsJuego;
        if (propAnimConservar) propAnimConservar->value = AnimEsJuego ? &AnimConservarEstados : NULL;
    }
    // el objeto UI NO tiene transformacion: es el ORDEN DE DIBUJO (la interfaz se dibuja al
    // final, sobre la escena). No se mueve, ni rota, ni escala: su tarjeta no aplica.
    if (propTransform) propTransform->visible = (pestaniaActiva == 1 && !es2D);
    if (propObjAnim) {
        propObjAnim->visible = (pestaniaActiva == 1 && !es2D && ObjActivo != NULL);
        if (propListObjAnims)
            propListObjAnims->mesh = (ObjActivo && ObjActivo->getType() == ObjectType::mesh)
                                     ? (Mesh*)ObjActivo : NULL;
        // Inicio/Fin/FPS SOLO cuando hay una animacion PROPIA activa (kind 3) para
        // ESTE objeto: sin animacion elegida no tienen sentido (se ocultan).
        bool hayAnimActiva = (ActiveAnimKind == 3 && ActiveAnimMesh == ObjActivo);
        if (gPropObjAnimStart) gPropObjAnimStart->value = hayAnimActiva ? &g_objAnimStartF : NULL;
        if (gPropObjAnimEnd)   gPropObjAnimEnd->value   = hayAnimActiva ? &g_objAnimEndF   : NULL;
        if (gPropObjAnimFps)   gPropObjAnimFps->value   = hayAnimActiva ? &g_objAnimFpsF   : NULL;
    }
    if (propTexto2D)   propTexto2D->visible   = (pestaniaActiva == 2 && esT2d);
    if (propImagen2D)  propImagen2D->visible  = (pestaniaActiva == 2 && esImg);
    if (propRect2D)    propRect2D->visible    = (pestaniaActiva == 2 && esRect);
    if (propCont2D)    propCont2D->visible    = (pestaniaActiva == 2 && esCont);
    if (propS9card)    propS9card->visible    = (pestaniaActiva == 2 && esS9);
    if (propBtn2D)     propBtn2D->visible     = (pestaniaActiva == 2 && esBtn);
    if (propExp2D)     propExp2D->visible     = (pestaniaActiva == 2 && esExp);
    if (propVid2D)     propVid2D->visible     = (pestaniaActiva == 2 && esVid);
    if (propUIcard)    propUIcard->visible    = (pestaniaActiva == 2 && esUI);
    // la GESTION de paletas es del PROYECTO: vive en la pestania 0 (junto a
    // Render/Juego), con o sin seleccion. La tarjeta "Paleta" DEL OBJETO va en la
    // pestania DEL OBJETO: la 1 para los 3D; para 2D/Script (que no tienen tab 1,
    // su nombre/posicion viven en la contextual) esa pestania es la 2. NUNCA en la
    // 0 ni en la de data de un mesh, y SOLO con un objeto activo: sin seleccion
    // la tarjeta por-objeto no se muestra (la del proyecto si, que es global).
    if (propPaleta)    propPaleta->visible    = (pestaniaActiva == 0);
    if (propPaletaObj) propPaletaObj->visible = ObjActivo != NULL &&
        ((pestaniaActiva == 1 && !es2D) || (pestaniaActiva == 2 && (es2D || esScript)));
    // la tarjeta Control (nombre + visible + "Agregar script") y las tarjetas de scripts:
    // en la contextual (2) para el objeto Script, y en la pestania Scripts (8) para
    // CUALQUIER otro objeto (si no tiene scripts, la 8 ofrece "Agregar script" igual).
    {
        bool verScripts = (pestaniaActiva == 2 && esScript) ||
                          (pestaniaActiva == 8 && ObjActivo != NULL && !esScript);
        if (propControl) propControl->visible = verScripts;
        int nScripts = (ObjActivo && ObjActivo->scriptDatos) ? (int)ObjActivo->scriptDatos->scripts.size() : 0;
        int act = -1;
        if (nScripts > 0 && ObjActivo && ObjActivo->scriptDatos) {
            act = ObjActivo->scriptDatos->activo;
            if (act < 0) act = 0; if (act >= nScripts) act = nScripts - 1;
            ObjActivo->scriptDatos->activo = act;
        }
        if (propListScripts) propListScripts->obj = ObjActivo;   // la lista sigue al objeto activo
        if (propListScripts && act >= 0 && propListScripts->selectIndex != act) {
            propListScripts->selectIndex = act; propListScripts->AjustarVentana();
        }
        // Remove solo con algo que quitar; la fila de Move entera solo con 2 o mas
        if (propRowScript && propRowScript->botones.size() >= 2)
            propRowScript->botones[1]->visible = (nScripts > 0);
        if (propRowScriptMove && propRowScriptMove->botones.size() >= 2) {
            const bool hay2 = (nScripts >= 2);   // el orden solo importa con 2 o mas
            propRowScriptMove->botones[0]->visible = hay2;
            propRowScriptMove->botones[1]->visible = hay2;
        }
        // ABAJO se ve la tarjeta del script SELECCIONADO y nada mas (antes salian las 8
        // apiladas y el panel era una tira infinita)
        for (int i = 0; i < kMaxScriptCards; i++)
            if (propScriptCards[i])
                propScriptCards[i]->visible = verScripts && i == act;
    }
    if (propHijos)     propHijos->visible     = (pestaniaActiva == 2 && es2D);
    // la tarjeta Margen solo aplica a un ELEMENTO cuyo padre lo acomoda en filas/columnas
    if (propMargen){
        bool enFila = false;
        if (pestaniaActiva == 2 && ObjActivo && UI2D_EsElemento2D(ObjActivo) &&
            ObjActivo->getType() != ObjectType::expandir2d){
            int* layP = HijosLayoutDe(ObjActivo->Parent);
            enFila = (layP && *layP != 0);
        }
        propMargen->visible = enFila;
    }
    if (propMeshParts) propMeshParts->visible = (pestaniaActiva == 2 && esMesh);
    if (propMaterial)  propMaterial->visible  = (pestaniaActiva == 2 && esMesh);
    if (propLight)     propLight->visible     = (pestaniaActiva == 2 && esLuz);
    if (propCamera)    propCamera->visible    = (pestaniaActiva == 2 && esCam);
    if (propInstance)  propInstance->visible  = (pestaniaActiva == 2 && esInst);
    if (propLOD)       propLOD->visible       = (pestaniaActiva == 2 && esLOD);
    if (propCulling)   propCulling->visible   = (pestaniaActiva == 2 && esCull);
    if (propCollection) propCollection->visible = (pestaniaActiva == 2 && esColl);
    if (propParticulas) propParticulas->visible = (pestaniaActiva == 2 && esPart);
    // pestania ARMATURE: tarjeta "Animation" (clips del esqueleto). Bindeo + visibilidad de Delete/Move mas abajo.
    bool armTab = (pestaniaActiva == 2 && esArm);
    if (propArmAnim) propArmAnim->visible = armTab;
    if (armTab) {
        Armature* a = (Armature*)ObjActivo;
        if (propListAnims) propListAnims->arm = a;             // la lista sigue al armature activo (modo 5)
        if (propBtnDupAnim) propBtnDupAnim->oculto = !(a && !a->animations.empty()); // Duplicate: solo con clips (oculto = no ocupa fila)
        int na = (int)a->animations.size();
        if (propRowAnimOps && propRowAnimOps->botones.size() >= 3) {
            propRowAnimOps->botones[0]->visible = (na >= 1);   // Delete: con >=1 clip
            bool hay2 = (na >= 2);                             // Move Up/Down: con >=2 clips (reordenar tiene sentido)
            propRowAnimOps->botones[1]->visible = hay2;
            propRowAnimOps->botones[2]->visible = hay2;
        }
        if (propBtnRenameAnim) propBtnRenameAnim->oculto = (na < 1); // Rename: solo si hay un clip activo
    }
    // tarjeta "Bones": lista de huesos + Name editable + Parent desplegable + transform del hueso activo
    if (propArmBones) propArmBones->visible = armTab;
    if (armTab) {
        Armature* a = (Armature*)ObjActivo;
        if (propListBones) propListBones->arm = a;
        W3dBone* b = BoneActivoUI();
        // Name/Parent: solo con un hueso ACTIVO (ocultos = no ocupan fila). El texto del campo Name lo
        // maneja SincronizarNombreBone (corre por frame; respeta lo que se este tipeando).
        if (propBoneNombre) propBoneNombre->oculto = (b == NULL);
        if (propBoneParent) {
            propBoneParent->oculto = (b == NULL);
            std::string pn = (b && b->parent >= 0 && b->parent < (int)a->bones.size())
                           ? a->bones[b->parent].name : std::string(T("None"));
            if (propBoneParent->button->text != pn){ propBoneParent->button->text = pn; g_redraw = true; }
        }
        // "Connected": solo con padre (sin padre la soldadura no tiene sentido -> value=NULL
        // oculta la fila). El mirror sigue el estado EFECTIVO del hueso (flag + puntas pegadas).
        if (propBoneConectado){
            bool conPadre = (b && b->parent >= 0 && b->parent < (int)a->bones.size());
            propBoneConectado->value = conPadre ? &g_boneConectado : NULL;
            if (conPadre) g_boneConectado = BoneEsConectado(a, a->boneActivo);
        }
        // sincronizar los campos SOLO al cambiar de hueso (sino se pisaria lo que el usuario esta tipeando)
        static int lastBoneSync = -999; static Armature* lastArm = NULL;
        if (a->boneActivo != lastBoneSync || a != lastArm) {
            SincronizarCamposBone();
            lastBoneSync = a->boneActivo; lastArm = a;
        }
    }
    // tarjeta "ARMATURE 2D" (rigs 2D del mesh): vive en su PROPIA pestania (6), que existe mientras
    // un editor UV esta en Edit Bones / Pose / MODO OBJETO sobre la malla activa en Edit Mode
    // (Arm2DTarjetaMeshUI). Antes estaba al final de la pestania de DATA del mesh (2), abajo de Mesh
    // Parts y Material: el dueno no la encontraba ("no veo en propiedades las opciones para editar
    // el armature del uv"). Con tab propia + salto automatico al entrar a huesos (PropsIrAArmature2D)
    // el panel del rig 2D queda tan a mano como el del armature 3D.
    // En MODO OBJETO se ve solo la LISTA de armatures (Add/Rename/Delete): las filas de HUESO usan
    // Bones2DMeshUI (Edit Bones/Pose) y con NULL se ocultan solas (lista vacia / value = NULL).
    {
        Mesh* m2d = (pestaniaActiva == 6) ? arm2dMesh : NULL;
        if (propBones2D) propBones2D->visible = (m2d != NULL);
        if (m2d) {
            Mesh* mHuesos = Bones2DMeshUI();                    // NULL en modo objeto
            if (propListBones2D) propListBones2D->mesh = mHuesos; // sin mesh la lista no ocupa fila
            if (propListArm2Ds)  propListArm2Ds->mesh  = m2d;   // lista de armatures 2D (cual esta activo)
            W3dBone2D* b2 = Bone2DActivoUI(mHuesos);
            const bool pose2d = Bones2DEnPoseUI();
            if (propBone2DParent) {
                propBone2DParent->oculto = (b2 == NULL);
                std::string pn = (b2 && b2->padre >= 0 && b2->padre < (int)m2d->Arm2DHuesos().size())
                               ? m2d->Arm2DHuesos()[b2->padre].nombre : std::string(T("None"));
                if (propBone2DParent->button->text != pn) { propBone2DParent->button->text = pn; g_redraw = true; }
            }
            // "Connected": solo con padre (value=NULL oculta la fila). Refleja el estado EFECTIVO.
            if (propBone2DConectado) {
                bool conPadre = (b2 && b2->padre >= 0 && b2->padre < (int)m2d->Arm2DHuesos().size());
                propBone2DConectado->value = conPadre ? &g_b2dConectado : NULL;
                if (conPadre) g_b2dConectado = Bone2DEsConectado(m2d, m2d->Arm2DBoneActivo());
            }
            // Pos siempre (rest o traslacion de pose); Rotation/Scale SOLO posando el hueso
            // entero (la punta no tiene rotacion). value = NULL oculta la fila (idioma del panel).
            if (gB2dPosX) gB2dPosX->value = b2 ? &g_b2dPosX : NULL;
            if (gB2dPosY) gB2dPosY->value = b2 ? &g_b2dPosY : NULL;
            if (gB2dRot)  gB2dRot->value  = (b2 && pose2d) ? &g_b2dRot  : NULL;
            if (gB2dSclX) gB2dSclX->value = (b2 && pose2d) ? &g_b2dSclX : NULL;
            if (gB2dSclY) gB2dSclY->value = (b2 && pose2d) ? &g_b2dSclY : NULL;
            // refresco por frame (los huesos se mueven en el viewport), SIN pisar el campo que
            // se este tipeando (mismo guard g_propFloatEditando que la tarjeta Keyframe)
            if (b2) {
                if (pose2d) {
                    if (g_propFloatEditando != gB2dPosX) g_b2dPosX = b2->poseTU;
                    if (g_propFloatEditando != gB2dPosY) g_b2dPosY = b2->poseTV;
                    if (g_propFloatEditando != gB2dRot)  g_b2dRot  = b2->poseRot;
                    if (g_propFloatEditando != gB2dSclX) g_b2dSclX = b2->poseSX;
                    if (g_propFloatEditando != gB2dSclY) g_b2dSclY = b2->poseSY;
                } else {
                    float pu, pv; Bone2DPuntoUI(b2, pu, pv);
                    if (g_propFloatEditando != gB2dPosX) g_b2dPosX = pu;
                    if (g_propFloatEditando != gB2dPosY) g_b2dPosY = pv;
                }
            }
        } else {
            if (propBone2DParent) propBone2DParent->oculto = true;
            if (propBone2DConectado) propBone2DConectado->value = NULL;
        }
    }
    bool vertTab = (pestaniaActiva == 3 && esMesh);
    // pestana TRANSFORMAR (5, solo Edit Mode con malla activa): card "Transform Mesh" (X/Y/Z de la
    // seleccion; antes vivia en la pestania Vertices) + card "Transform UV" (centro X/Y de los UVs
    // de la MISMA seleccion). Recalculan el centro cada frame; el mesh mapea con la convencion
    // Z-up del panel (campo Y = local z, campo Z = local y).
    bool transTab = (pestaniaActiva == 5 && transTabOk);
    if (propEditItem) {
        bool haySel = false; float cx=0,cy=0,cz=0;
        if (transTab && InteractionMode == EditMode && g_editMesh) {
            Mesh* em = (Mesh*)g_editMesh; em->EnsureEdit();
            if (em->edit) haySel = em->edit->CentroSeleccion(cx, cy, cz);
        }
        propEditItem->visible = haySel;
        if (haySel) { editPosX = cx; editPosY = cz; editPosZ = cy; }
    }
    if (propUVTransform) {
        bool hayUV = false; float cu=0, cv=0;
        if (transTab && InteractionMode == EditMode && g_editMesh)
            hayUV = CentroUVSeleccionEdit((Mesh*)g_editMesh, cu, cv);
        propUVTransform->visible = hayUV;
        if (hayUV) { uvPosU = cu; uvPosV = cv; }
    }
    if (propUVMaps)      propUVMaps->visible      = vertTab;
    if (propColorLayers) propColorLayers->visible = vertTab;
    if (propVertexGroups) propVertexGroups->visible = vertTab;
    if (propUVGroups)     propUVGroups->visible     = vertTab;
    // pestaña Modifiers: card del stack + una 2da card con las props del modificador seleccionado (vacia).
    bool modsTab = (pestaniaActiva == 4 && esMesh);
    if (propModifiers) propModifiers->visible = modsTab;
    if (modsTab) {
        Mesh* mm = (Mesh*)ObjActivo;
        if (propListModifiers) propListModifiers->mesh = mm;   // el selector sigue a la malla activa
        int nm = (int)mm->modificadores.size();
        if (propRowMod && propRowMod->botones.size() >= 2)
            propRowMod->botones[1]->visible = (nm >= 1);        // Remove: solo si hay 1+
        if (propRowModMove && propRowModMove->botones.size() >= 2) {
            bool hay2 = (nm >= 2);                              // Move Up/Down: solo si hay 2+ (el orden importa con 2)
            propRowModMove->botones[0]->visible = hay2;
            propRowModMove->botones[1]->visible = hay2;
        }
        bool haySel = (nm > 0 && mm->modificadorActivo >= 0 && mm->modificadorActivo < nm);
        Modifier* mod = haySel ? mm->modificadores[mm->modificadorActivo] : NULL;
        bool esMirror = (mod && mod->tipo == ModifierType::Mirror);
        bool esSub    = (mod && mod->tipo == ModifierType::SubdivisionSurface);
        bool esScrew  = (mod && mod->tipo == ModifierType::Screw);
        bool esPvs    = (mod && mod->tipo == ModifierType::CullingTri);
        if (propModifierProps) {
            propModifierProps->visible = haySel;               // 2da tarjeta: solo con un modificador seleccionado
            if (mod) propModifierProps->name = mod->nombre;    // titulo = su nombre
        }
        // props del MIRROR: bindeadas al modificador activo (value=NULL las OCULTA -> solo se ven en un Mirror).
        // display toggles: para CUALQUIER modificador seleccionado (no solo Mirror)
        if (propModVerViewport) propModVerViewport->value = haySel ? &mod->mostrarViewport : NULL;
        if (propModVerEdit)     propModVerEdit->value     = haySel ? &mod->mostrarEdit : NULL;
        if (propModVacio) propModVacio->oculto = (esMirror || esSub || esScrew || esPvs || (mod && mod->tipo==ModifierType::Armature)); // "(no properties yet)" solo tipos sin params
        if (propSubSimple) propSubSimple->value = esSub ? &mod->subSimple    : NULL;
        if (propSubLevel)  propSubLevel->value  = esSub ? &mod->subLevel      : NULL;
        if (propSubRender) propSubRender->value = esSub ? &mod->subRenderLevel: NULL;
        // Screw
        if (propScrewAngle)   propScrewAngle->value   = esScrew ? &mod->screwAngle       : NULL;
        if (propScrewHeight)  propScrewHeight->value  = esScrew ? &mod->screwHeight       : NULL;
        if (propScrewSteps)   propScrewSteps->value   = esScrew ? &mod->screwSteps        : NULL;
        if (propScrewRender)  propScrewRender->value  = esScrew ? &mod->screwRenderSteps  : NULL;
        if (propScrewStretchU)propScrewStretchU->value= esScrew ? &mod->screwStretchU     : NULL;
        if (propScrewStretchV)propScrewStretchV->value= esScrew ? &mod->screwStretchV     : NULL;
        if (propScrewSmooth)  propScrewSmooth->value  = esScrew ? &mod->screwSmooth        : NULL;
        if (propScrewMerge)   propScrewMerge->value   = esScrew ? &mod->screwMerge         : NULL;
        if (propScrewFlip)    propScrewFlip->value    = esScrew ? &mod->screwFlip          : NULL;
        if (propScrewAxis){ propScrewAxis->oculto = !esScrew;
            if (esScrew) propScrewAxis->button->text = (mod->screwAxis==0)?"X":(mod->screwAxis==1)?"Y":"Z"; }
        if (propMirX) propMirX->value = esMirror ? &mod->ejeX : NULL;
        if (propMirY) propMirY->value = esMirror ? &mod->ejeY : NULL;
        if (propMirZ) propMirZ->value = esMirror ? &mod->ejeZ : NULL;
        if (propMirMerge) propMirMerge->value = esMirror ? &mod->merge : NULL;
        if (propMirDist)  propMirDist->value  = esMirror ? &mod->mergeDist : NULL;
        if (propMirClip)  propMirClip->value  = esMirror ? &mod->clipping : NULL;
        if (propMirTarget) { propMirTarget->oculto = !esMirror;
            if (esMirror) propMirTarget->button->text = mod->target ? mod->target->name : std::string("None"); }
        bool esArmMod = (mod && mod->tipo == ModifierType::Armature);
        if (propArmTarget) { propArmTarget->oculto = !esArmMod;
            if (esArmMod) propArmTarget->button->text = mod->target ? mod->target->name : std::string("None"); }
        if (propBtnOptVG) propBtnOptVG->oculto = !esArmMod; // "Optimize Vertex Groups": solo en el modificador Armature
        // Cache Animation + Frame Skip: solo en el Armature (PropBool/PropFloat se ocultan con value=NULL)
        if (propArmCache)     propArmCache->value     = esArmMod ? &mod->cacheAnim : NULL;
        if (propArmCacheSkip) propArmCacheSkip->value = esArmMod ? &mod->cacheSkip : NULL;
        if (esArmMod) ActualizarSkinArmature(mm); // mantener skinArmature en sync con el modificador
        // Culling (PVS por triangulo): metodo + Recalcular + la info del sidecar cargado
        if (propPvsMetodo) { propPvsMetodo->oculto = !esPvs;
            if (esPvs) propPvsMetodo->button->text = (mod->metodoPVS == 1) ? "BSP (pendiente)" : T("Triangulos (PVS)"); }
        if (propPvsRecalc) propPvsRecalc->oculto = !esPvs;
        if (propPvsInfo) { propPvsInfo->oculto = !esPvs;
            if (esPvs) {
                char inf[96];
                if (!mod->pvsSectores.empty())
                    snprintf(inf, sizeof(inf), "%d sectores | sector activo: %d", (int)mod->pvsSectores.size(), mod->sectorPVS);
                else
                    snprintf(inf, sizeof(inf), "%s", mod->pvsCargado ? "sin <modelo>.pvs.json (malla completa)" : "sidecar sin cargar (Recalcular)");
                propPvsInfo->name = inf;
            } }
        // Apply: con cualquier modificador seleccionado, SALVO el Culling (no hornea nada: la malla ya esta intacta)
        if (propBtnApplyMod) propBtnApplyMod->oculto = !haySel || esPvs;
    } else if (propModifierProps) propModifierProps->visible = false;

    // ===== pestania CONSTRAINTS (7): tarjeta del stack + tarjeta del constraint elegido =====
    // El BINDEO va aca y NO en RefreshTargetProperties: esa tiene un early-out por objeto y el
    // stack cambia SIN cambiar de objeto (Add/Remove/Move, y el Ctrl+Z de cualquiera de los tres).
    // La lista se bindea SIEMPRE (con NULL fuera de la pestania): Properties::Resize mide TODOS
    // los grupos, visibles o no, asi que dejarle el Object* del objeto anterior seria leer un
    // puntero que ya puede estar liberado.
    {
        const bool consTab = (pestaniaActiva == 7 && consObj != NULL);
        if (propListConstraints) propListConstraints->obj = consTab ? consObj : NULL;
        if (propConstraints) propConstraints->visible = consTab;
        if (consTab){
            const int nc = consObj->ConstraintsCount();
            if (propRowCon && propRowCon->botones.size() >= 2)
                propRowCon->botones[1]->visible = (nc >= 1);        // Remove: solo si hay 1+
            if (propRowConMove && propRowConMove->botones.size() >= 2){
                const bool hay2 = (nc >= 2);                        // Move Up/Down: el orden solo importa con 2+
                propRowConMove->botones[0]->visible = hay2;
                propRowConMove->botones[1]->visible = hay2;
            }
        }
        const bool haySel = consTab && consObj->constraintActivo >= 0 &&
                            consObj->constraintActivo < consObj->ConstraintsCount();
        W3dConstraint* c = haySel ? consObj->constraints[consObj->constraintActivo] : NULL;
        if (propConstraintProps){
            propConstraintProps->visible = haySel;   // 2da tarjeta: solo con un constraint elegido
            if (c) propConstraintProps->name = c->nombre;   // titulo = su nombre
        }
        // QUE SE VE POR TIPO (la tabla): Enabled e Influence en los 3; Source y Copy X/Y/Z en
        // Copy Location y Copy Rotation; Mode solo en el Billboard.
        const bool esCopia = (c && (c->tipo == W3dConstraintTipo::CopyLocation ||
                                    c->tipo == W3dConstraintTipo::CopyRotation));
        const bool esBB    = (c && c->tipo == W3dConstraintTipo::Billboard);
        if (propConActivo)     propConActivo->value     = c ? &c->activo      : NULL;
        if (propConVerEdit)    propConVerEdit->value    = c ? &c->mostrarEdit : NULL;
        if (propConInfluencia) propConInfluencia->value = c ? &c->influencia : NULL;
        // EL CRUCE Y<->Z DE LA POSICION: el panel muestra la posicion en Z-arriba y la rotacion
        // derecha, asi que el mismo checkbox apunta a un campo distinto segun el tipo (el por que
        // completo esta en ConstruirGrupos, arriba de propConEjeX).
        const bool esLoc = (c && c->tipo == W3dConstraintTipo::CopyLocation);
        if (propConEjeX) propConEjeX->value = esCopia ? &c->ejeX : NULL;
        if (propConEjeY) propConEjeY->value = esCopia ? (esLoc ? &c->ejeZ : &c->ejeY) : NULL;
        if (propConEjeZ) propConEjeZ->value = esCopia ? (esLoc ? &c->ejeY : &c->ejeZ) : NULL;
        if (propConFuente){
            propConFuente->oculto = !esCopia;
            // EL TEXTO SE REFRESCA POR FRAME, y ese es el punto: cuando borran el objeto fuente,
            // la puerta de refs (Object::SetRefObjeto) deja fuenteObj en NULL y el boton pasa a
            // decir "None" solo. Es donde se ve, sin abrir nada, que el vinculo se corto.
            if (esCopia){
                const std::string txt = (c->fuenteTipo == W3dConstraintFuente::Vista)
                                        ? std::string(T("View"))
                                        : (c->fuenteObj ? c->fuenteObj->name : std::string(T("None")));
                if (propConFuente->button->text != txt){ propConFuente->button->text = txt; g_redraw = true; }
            }
        }
        if (propConBBModo){
            propConBBModo->oculto = !esBB;
            if (esBB){
                const std::string txt = ConNombreModoBB(c);
                if (propConBBModo->button->text != txt){ propConBBModo->button->text = txt; g_redraw = true; }
            }
        }
        // los avisos 2, 3 y 4 son CONDICIONALES: el de la fuente solo aparece con la fuente
        // cortada (o todavia sin elegir), el de los ejes solo con los TRES destildados, y el
        // del billboard solo a influencia intermedia. Los dos primeros son los dos motivos por
        // los que W3dConEfectivo (Objects.cpp) deja un Copy* sin efecto, uno por motivo.
        if (propConAvisoFuente)
            propConAvisoFuente->oculto = !(esCopia && c->fuenteTipo == W3dConstraintFuente::Objeto && !c->fuenteObj);
        if (propConAvisoEjes)
            propConAvisoEjes->oculto = !(esCopia && !c->ejeX && !c->ejeY && !c->ejeZ);
        if (propConAvisoBB)
            propConAvisoBB->oculto = !(esBB && c->influencia > 0.0f && c->influencia < 100.0f);
    }

    // pestaña Vertices activa: las listas siguen a la malla activa (modo 1=uvmaps, 2=colors) + el toggle
    if (vertTab) {
        Mesh* mv = (Mesh*)ObjActivo;
        if (mv->uvMaps.empty() || mv->colorLayers.empty()) mv->PoblarCapas(); // crea la 1ra si falta
        if (propListUV)    propListUV->mesh    = mv;
        if (propListColor) propListColor->mesh = mv;
        if (propListVertGroups) propListVertGroups->mesh = mv; // grupos de vertices (huesos del rig 3D)
        if (propListUVGroups)   propListUVGroups->mesh   = mv; // UV groups (pesos por corner del rig 2D)
        if (propBtnColorMode && mv->colorActivo >= 0 && mv->colorActivo < (int)mv->colorLayers.size())
            propBtnColorMode->button->text =
                mv->colorLayers[mv->colorActivo]->porVertice ? "Per-Vertex" : "Per-Corner";
        // Delete | Move Up | Move Down: toda la fila solo si hay >1 elemento (borrar/reordenar necesita >=2)
        if (propRowUVOps && propRowUVOps->botones.size() >= 3){
            bool mas = (mv->uvMaps.size() > 1);
            for (int b = 0; b < 3; b++) propRowUVOps->botones[b]->visible = mas;
        }
        if (propRowColorOps && propRowColorOps->botones.size() >= 3){
            bool mas = (mv->colorLayers.size() > 1);
            for (int b = 0; b < 3; b++) propRowColorOps->botones[b]->visible = mas;
        }
        // Vertex Groups: pueden ser 0. Rename + Delete se ven con >=1; Move Up/Down con >=2.
        bool hayGrp = !mv->vertexGroups.empty();
        if (propBtnRenameGroup) propBtnRenameGroup->oculto = !hayGrp;
        if (propRowGroupOps && propRowGroupOps->botones.size() >= 3){
            propRowGroupOps->botones[0]->visible = hayGrp;                      // Delete
            propRowGroupOps->botones[1]->visible = (mv->vertexGroups.size() > 1); // Move Up
            propRowGroupOps->botones[2]->visible = (mv->vertexGroups.size() > 1); // Move Down
        }
        // Assign/Remove y Select/Deselect: solo con un grupo activo (sin grupo no hay a que asignar)
        if (propRowGroupAsig && propRowGroupAsig->botones.size() >= 2)
            for (int b = 0; b < 2; b++) propRowGroupAsig->botones[b]->visible = hayGrp;
        if (propRowGroupSel && propRowGroupSel->botones.size() >= 2)
            for (int b = 0; b < 2; b++) propRowGroupSel->botones[b]->visible = hayGrp;
        // UV Groups: MISMA regla que Vertex Groups (pueden ser 0)
        bool hayUVG = !mv->uvGroups.empty();
        if (propBtnRenameUVGroup) propBtnRenameUVGroup->oculto = !hayUVG;
        if (propRowUVGroupOps && propRowUVGroupOps->botones.size() >= 3){
            propRowUVGroupOps->botones[0]->visible = hayUVG;                   // Delete
            propRowUVGroupOps->botones[1]->visible = (mv->uvGroups.size() > 1); // Move Up
            propRowUVGroupOps->botones[2]->visible = (mv->uvGroups.size() > 1); // Move Down
        }
        if (propRowUVGroupAsig && propRowUVGroupAsig->botones.size() >= 2)
            for (int b = 0; b < 2; b++) propRowUVGroupAsig->botones[b]->visible = hayUVG;
        if (propRowUVGroupSel && propRowUVGroupSel->botones.size() >= 2)
            for (int b = 0; b < 2; b++) propRowUVGroupSel->botones[b]->visible = hayUVG;
    }

    // el boton de target muestra el objeto apuntado (se actualiza cada frame
    // para reflejar el cambio al elegirlo del desplegable)
    if (esCam || esInst){
        Target* tgt = ObjComoTarget(ObjActivo);
        PropButton* btn = esCam ? propBtnCamTarget : propBtnInstTarget;
        if (tgt && btn)
            btn->button->text = tgt->target ? tgt->target->name : std::string("None");
    }
}

// (declarada en Properties.h) el editor UV la llama al ENTRAR a la edicion de huesos 2D (Tab, menu
// Add > Armature 2D, selector de Modo): el panel ACTIVO se para SOLO en la pestania "Armature 2D",
// que es la respuesta al "no veo en propiedades las opciones para editar el armature del uv" (antes
// la tarjeta estaba escondida al final de la pestania de datos del mesh). Si la pestania no
// corresponde, el fallback de ActualizarPestanias la corrige sola (no deja el panel en el vacio).
void PropsIrAArmature2D(){
    Properties* p = PropsActivo;
    if (!p || p->BarTabs.size() < 7) return;
    g_textFieldActivo = NULL;   // cambiar de pestania des-enfoca el texto (igual que ClickTab)
    p->pestaniaActiva = 6;
    p->focoEnTabs = false;
    p->LimpiarSeleccionGrupos();
    p->ActualizarPestanias();
    p->Resize(p->width, p->height); // re-layout (scroll del grupo nuevo)
}

void Properties::ClickTab(int mx, int my){
    for (size_t i = 0; i < BarTabs.size(); i++){
        if (BarTabs[i]->visible && BarTabs[i]->Contains(mx, my)){
            g_textFieldActivo = NULL; // cambiar de pestania des-enfoca el texto
            pestaniaActiva = (int)i;
            focoEnTabs = false; // con mouse la activa va blanca (no verde)
            ActualizarPestanias();
            Resize(width, height); // re-layout (scroll del nuevo grupo)
            return;
        }
    }
}

void Properties::Resize(int newW, int newH){
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);
    ActualizarPestanias(); // visibilidad de grupos antes de medir el contenido

    if (!ObjActivo && pestaniaActiva != 0) {
        // sin objeto Y fuera de la pestania Render (global): sin contenido ni scrollbar (antes quedaba la
        // barra con el tamano viejo). En la pestania Render se mide su contenido global aunque no haya seleccion.
        PosY = 0;
        ResizeScrollbar(newW, newH, 0, 0, BarTopOffset());
        return;
    }

    // la barra de scroll solo necesita su ancho (4px) + un respiro
    int WidthCard = width - bordersGS - gapGS
        - (scrollY ? GlobalScale*8 : 0); // la reserva de la barra
    // (incluso GRANDE) solo cuando la barra existe
    int heightCard = borderGS + borderGS + borderGS + (RenglonHeightGS + gapGS)*10;
    maxPixelsTitle = WidthCard - IconSizeGS - gapGS;

    for (size_t i=0; i < GroupProperties.size(); i++){
        GroupProperties[i]->Resize(WidthCard, heightCard);
    }

    // alto REAL del contenido (antes era -2000 hardcodeado: el scroll
    // vertical se calculaba mal, tambien en PC)
    int contenidoH = borderGS + RenglonHeightGS + gapGS; // titulo
    for (size_t i=0; i < GroupProperties.size(); i++){
        if (GroupProperties[i]->visible){
            // mismo paso que el render (y que ClickEn/CentrarSeleccion)
            contenidoH += GroupProperties[i]->height + borderGS
                          + (GroupProperties[i]->open ? GlobalScale : 0);
        }
    }
    contenidoH += marginGS; // respiro abajo (la barra va por topOffset)
    ResizeScrollbar(newW, newH, 0, -contenidoH, BarTopOffset());
}

void Properties::Render(){
    if (!leftMouseDown) UndoMaterialModCommit(); // Ctrl+Z: al soltar el mouse, pushea el cambio de material (si difiere)
    RefreshTargetProperties();
    ActualizarPestanias(); // que grupo mostrar segun la pestania (Objeto/Mesh)

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // Limpiar pantalla
    w3dEngine::Enable(w3dEngine::ScissorTest);
    const int glY = W3dPantallaAlto - y - height; // arbol arriba-izq -> GL
    w3dEngine::Scissor(x, glY, width, height); // igual a tu viewport
    w3dEngine::ClearColor(
        ListaColores[static_cast<int>(ColorID::background)][0],
        ListaColores[static_cast<int>(ColorID::background)][1],
        ListaColores[static_cast<int>(ColorID::background)][2],
        ListaColores[static_cast<int>(ColorID::background)][3]
    );

    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    w3dEngine::Viewport(x, glY, width, height); // x, y, ancho, alto
    w3dEngine::Ortho(0, width, height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::ColorMaterial);

    w3dEngine::BindTexture(Textures[0]->iID);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
#ifndef W3D_SYMBIAN
    w3dEngine::TexFilter(false);
#endif

    // la pestania Render (0) tiene ajustes GLOBALES (salida/pases): se dibuja SIEMPRE, con o sin seleccion.
    // Las demas pestanias son del objeto activo -> sin seleccion no hay contenido.
    if (ObjActivo || pestaniaActiva == 0){
        // los GRUPOS son globales y otro panel de propiedades pudo
        // haberlos acomodado con OTRO ancho: relayout con el propio
        // antes de dibujar (mitigacion hasta hacerlos por-instancia)
        {
            int WidthCard = width - bordersGS - gapGS
        - (scrollY ? GlobalScale*8 : 0); // la reserva de la barra
    // (incluso GRANDE) solo cuando la barra existe
            int heightCard = borderGS * 3 + (RenglonHeightGS + gapGS) * 10;
            for (size_t i = 0; i < GroupProperties.size(); i++){
                GroupProperties[i]->Resize(WidthCard, heightCard);
            }
        }
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(PosX + borderGS, PosY + borderGS + BarTopOffset(), 0);

        // el renglon del titulo (el objeto seleccionado) SIEMPRE ocupa su fila: sin
        // seleccion se deja el hueco, porque el hit-test del click y el culling ya
        // la cuentan (sin esto todo se dibujaba una fila mas arriba y el mouse "le
        // pegaba" a la opcion de arriba, bug al abrir sin seleccion)
        if (ObjActivo) DibujarTitulo(ObjActivo, maxPixelsTitle);
        else w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);

        //render de los grupos de propiedades, con CULLING: el grupo que
        //queda completo fuera del viewport no se dibuja (solo se avanza
        //el cursor lo mismo que avanzaria su Render)
        int yLocal = PosY + borderGS + BarTopOffset() + RenglonHeightGS + gapGS;
        for (size_t i=0; i < GroupProperties.size(); i++){
            GroupPropertie* g = GroupProperties[i];
            if (!g->visible) continue;
            int paso = g->height + borderGS + (g->open ? GlobalScale : 0);
            if (yLocal + paso < 0 || yLocal > height) {
                w3dEngine::Translatef(0, (GLfloat)paso, 0); // fuera: solo avanzar
            } else {
                g->Render();
            }
            yLocal += paso;
        }


        w3dEngine::PopMatrix();
    }

    //w3dEngine::Disable(w3dEngine::ScissorTest);
    RenderBar();
    DibujarBordes(this);
    DibujarScrollbar(this);
    w3dEngine::Disable(w3dEngine::ScissorTest);
}

void Properties::CambiarTab(int dir){
    // DINAMICO: avanza al SIGUIENTE tab VISIBLE en la direccion 'dir', saltando los ocultos y envolviendo.
    // (Antes usaba n=hayTab3?3:2 -> nunca llegaba a Vertices/Modifiers por teclado: solo con el mouse.)
    int n = (int)BarTabs.size();
    if (n <= 0) return;
    for (int k = 0; k < n; k++){
        pestaniaActiva = (pestaniaActiva + dir + n) % n;
        if (BarTabs[pestaniaActiva]->visible) break; // primer tab visible en esa direccion
    }
    LimpiarSeleccionGrupos();   // la pestaña nueva entra sin nada resaltado
    ActualizarPestanias();      // visibilidad de los grupos de la nueva pestaña
    Resize(width, height);      // RECALCULA el scroll (MaxPosY) del nuevo contenido
}

// pone el foco en el primer grupo VISIBLE de la pestaña actual (al bajar de las
// pestañas a las propiedades). Sin esto el foco quedaba en un grupo invisible
// (ej: transforms cuando estas en Materiales) y la navegacion se rompia.
void Properties::EntrarPrimerGrupoVisible(){
    for (size_t i = 0; i < GroupProperties.size(); i++){
        if (GroupProperties[i]->visible){
            selectIndex = (int)i;
            GroupProperties[i]->selectIndex = -1; // cabecera del grupo
            CentrarSeleccion();
            return;
        }
    }
}

// arriba estando en las pestañas: wrap a la ULTIMA propiedad del ULTIMO grupo visible (simetrico a bajar
// desde la ultima opcion -> pestanias).
void Properties::EntrarUltimoGrupoVisible(){
    for (int i = (int)GroupProperties.size() - 1; i >= 0; i--){
        if (GroupProperties[i]->visible){
            selectIndex = i;
            GroupProperties[i]->selectLastIndexProperty(); // ultima propiedad seleccionable del grupo
            CentrarSeleccion();
            return;
        }
    }
}

// nada resaltado en las propiedades (mientras el foco esta en las pestañas)
void Properties::LimpiarSeleccionGrupos(){
    for (size_t i = 0; i < GroupProperties.size(); i++)
        GroupProperties[i]->selectIndex = -2;
}

// la propiedad seleccionada por teclado (NULL si es una cabecera o nada)
static PropertieBase* PropFilaSeleccionada(std::vector<GroupPropertie*>& gps, int selectIndex){
    if (selectIndex < 0 || selectIndex >= (int)gps.size()) return NULL;
    GroupPropertie* g = gps[selectIndex];
    if (g->selectIndex < 0 || g->selectIndex >= (int)g->properties.size()) return NULL;
    return g->properties[g->selectIndex];
}

void Properties::button_left(){
    PropsActivo = this; // este panel pasa a ser el activo
    if (focoEnTabs){ CambiarTab(-1); return; } // en las pestañas: cambiar de pestaña
    if (!editando){
        PropertieBase* p = PropFilaSeleccionada(GroupProperties, selectIndex);
        // keyframe enfocado: IZQUIERDA vuelve al valor de la propiedad
        if (p && g_kfFocoProp == (void*)p){ g_kfFocoProp = 0; g_redraw = true; return; }
        // si la fila seleccionada es una FILA DE BOTONES, mover entre ellos (NO colapsar la tarjeta)
        if (p && p->GetType() == PropertyType::ButtonRow) { p->button_left(); return; }
        SetOpenGroup(false);
    }
    else {
        GroupProperties[selectIndex]->button_left();
    }
}

void Properties::button_right(){
    PropsActivo = this; // este panel pasa a ser el activo
    if (focoEnTabs){ CambiarTab(+1); return; }
    if (!editando){
        PropertieBase* p = PropFilaSeleccionada(GroupProperties, selectIndex);
        // fila animable: DERECHA salta al KEYFRAME (borde blanco). OK/Enter lo toggla.
        if (p && p->AnimProp() >= 0 && g_kfFocoProp != (void*)p){ g_kfFocoProp = (void*)p; g_redraw = true; return; }
        if (p && p->GetType() == PropertyType::ButtonRow) { p->button_right(); return; }
        SetOpenGroup(true);
    }
    else {
        GroupProperties[selectIndex]->button_right();
    }
}

#ifndef W3D_SYMBIAN
void Properties::mouse_button_up(int boton){
    // si se apreto sobre un PropFloat y NO se arrastro (click puro) -> abrir la edicion por TEXTO (todo seleccionado,
    // tipear reemplaza + enter). Si se arrastro, el valor ya cambio y no se edita.
    if (gFloatDrag && !gFloatDragMoved && boton == W3dMB_IZQ) {
        PropsActivo = this;
        gFloatDrag->IniciarEdicionTexto(); // editor INLINE de Whisk3D (el texto entra por SDL_TEXTINPUT como siempre)
    }
    gFloatDrag = NULL; gFloatDragMoved = false; gFloatDragAccum = 0.0f;
    gListaScrollLista = NULL; // fin del drag-scroll de la lista
    if (!editando) ViewPortClickDown = false;
}
#endif

#ifndef W3D_SYMBIAN
void Properties::event_mouse_wheel(float dy, int mx, int my){
    if (editando) return;
    // rueda sobre las PESTAÑAS (barra superior) = scroll horizontal (para llegar a Modifiers cuando el
    // panel es angosto). Mismo comportamiento que la barra del viewport 3D. Fuera de la barra -> vertical.
    {
      if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; }
    // si el mouse esta sobre una LISTA (mesh parts / selector), la rueda la scrollea A
    // ELLA (antes solo scrolleaba el panel entero -> el componente "obligaba" al estilo
    // Symbian de Enter+flechas). Reusa el hover ya trackeado (PropHoverGroup/Fila).
    if (PropHoverGroup && PropHoverFila >= 0 && PropHoverFila < (int)PropHoverGroup->properties.size()) {
        PropertieBase* prop = PropHoverGroup->properties[PropHoverFila];
        if (prop->GetType() == PropertyType::List) {
            PropListMeshParts* lst = static_cast<PropListMeshParts*>(prop);
            int n = lst->ListaCount(); // parts / uv maps / colors segun el modo
            int vis = n < lst->filasMax ? n : lst->filasMax;
            if (n > vis) {
                lst->scrollFila -= (dy > 0 ? 1 : -1); // rueda arriba = subir
                if (lst->scrollFila > n - vis) lst->scrollFila = n - vis;
                if (lst->scrollFila < 0) lst->scrollFila = 0;
                g_redraw = true;
                return; // consumido por la lista: NO scrollea el panel
            }
        }
    }
    MouseWheel = true;
    ScrollY(dy*12*GlobalScale);
    MouseWheel = false;
}
#endif

// apaga el hover de TODOS los botones de fila (no solo los conocidos: si no, el
// hover de los nuevos -Render/Export- quedaba pegado al salir el mouse)
void Properties::ResetButtonHovers(){
    for (size_t i = 0; i < GroupProperties.size(); i++)
        for (size_t j = 0; j < GroupProperties[i]->properties.size(); j++)
            if (GroupProperties[i]->properties[j]->GetType() == PropertyType::Button)
                ((PropButton*)GroupProperties[i]->properties[j])->button->hover = false;
}

void Properties::ClearHover(){
    ResetButtonHovers();
    PropHoverGroup = NULL;
    PropHoverFila = -1;
}

void Properties::FindMouseOver(int mx, int my){
    PropsActivo = this; // este panel pasa a ser el activo
    // hover de FILAS (texto blanco / borde del checkbox) y de los
    // botones de fila; mismo recorrido que ClickEn
    ResetButtonHovers(); // apaga TODOS los botones (luego se prende el de la fila)
    PropHoverGroup = NULL;
    PropHoverFila = -1;
    if (mouseOverScrollY) return; // el "scrollbar area" esta reservada
    if ((!ObjActivo && pestaniaActiva != 0) || !Contains(mx, my)) return; // pestania Render (global): hover sin seleccion
    int yCursor = y + BarTopOffset() + PosY + borderGS + RenglonHeightGS + gapGS;
    for (size_t i = 0; i < GroupProperties.size(); i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        int hCabecera = borderGS + RenglonHeightGS + gapGS;
        if (g->open) {
            int yFila = yCursor + hCabecera;
            for (size_t j = 0; j < g->properties.size(); j++) {
                PropertieBase* prop = g->properties[j];
                int hFila = prop->Resize(g->width);
                if (hFila > 0 && prop->GetType() != PropertyType::Gap &&
                    prop->Seleccionable() &&
                    my >= yFila && my < yFila + hFila) {
                    PropHoverGroup = g;
                    PropHoverFila = (int)j;
                    if (prop->GetType() == PropertyType::Button) {
                        int izq = x + PosX + borderGS + borderGS;
                        Button* b2 = ((PropButton*)prop)->button;
                        b2->hover = (mx >= izq && mx < izq + b2->width);
                    }
                    return;
                }
                yFila += hFila;
            }
        }
        yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
}

// TOUCH: arrastrar 1 dedo sobre el CONTENIDO = scroll vertical. (La barra de pestañas la maneja el gesto
// lockeado en controles.cpp con BarScrollBy; aca solo el contenido.)
// TACTIL: latch del mini-listado que se esta scrolleando con el dedo (se decide en el 1er evento del gesto, cuando el
// dedo esta ~sobre el punto del down, y se mantiene hasta soltar aunque el dedo se salga del box). Separado del latch
// de mouse (gListaScrollLista) para que no se pisen. Lo limpia PropertiesTouchScrollFin() en el up.
static PropListMeshParts* gTouchScrollLista = NULL;
static bool  gTouchScrollDecidido = false;
static float gTouchScrollAccum = 0.0f;

void PropertiesTouchScrollFin(){ // llamada desde controles.cpp al soltar (fin del gesto tactil)
    gTouchScrollLista = NULL; gTouchScrollDecidido = false; gTouchScrollAccum = 0.0f;
    gListaScrollLista = NULL; // por las dudas, tambien el latch de mouse
}

bool Properties::event_finger_scroll(int px, int py, int dx, int dy){
    // 1er evento del gesto: decidir si el dedo empezo sobre un mini-listado con contenido scrolleable
    if (!gTouchScrollDecidido) {
        gTouchScrollDecidido = true;
        gTouchScrollAccum = 0.0f;
        PropListMeshParts* l = ListaBajoY(py);
        if (l) { int n = l->ListaCount(); int vis = n < l->filasMax ? n : l->filasMax;
                 gTouchScrollLista = (n > vis) ? l : NULL; }
        else gTouchScrollLista = NULL;
    }
    if (gTouchScrollLista) {
        int n = gTouchScrollLista->ListaCount();
        int vis = n < gTouchScrollLista->filasMax ? n : gTouchScrollLista->filasMax;
        if (n > vis) {
            int rowH = RenglonHeightGS + gapGS; if (rowH < 1) rowH = 1;
            gTouchScrollAccum += (float)dy;               // dedo hacia abajo (dy>0) = ver items de arriba
            int steps = (int)(gTouchScrollAccum / rowH);
            if (steps != 0) {
                gTouchScrollAccum -= (float)(steps * rowH);
                int nuevo = gTouchScrollLista->scrollFila - steps;
                if (nuevo > n - vis) nuevo = n - vis;
                if (nuevo < 0) nuevo = 0;
                if (nuevo != gTouchScrollLista->scrollFila) { gTouchScrollLista->scrollFila = nuevo; g_redraw = true; }
            }
            return true; // consumido por la lista: el panel NO scrollea
        }
    }
    ScrollByTouch(0, dy);
    return true;
}

void Properties::event_mouse_motion(int mx, int my) {
    // arrastre de un PropFloat (posicion/rotacion/escala/shininess): mover el
    // mouse en horizontal cambia el valor. Va ANTES del check de 'editando'.
    if (gFloatDrag) {
        if (!leftMouseDown) { gFloatDrag = NULL; return; }
        // un Rebind entre el down y este motion puede haber ocultado la fila
        // (value = NULL): cortar el drag (el gemelo tactil gTouchSlide ya chequea)
        if (!gFloatDrag->value) { gFloatDrag = NULL; return; }
        // ZONA MUERTA: hasta que el mouse no se movio unos pixeles NO cambia el valor -> un click puro (sin mover)
        // deja el valor intacto y al soltar abre la edicion por TEXTO. Pasado el umbral, arrastra como siempre.
        gFloatDragAccum += dx;
        if (!gFloatDragMoved) {
            if (fabsf(gFloatDragAccum) < 4.0f * GlobalScale) return; // sigue siendo un click potencial
            gFloatDragMoved = true;
        }
        ViewPortClickDown = true; // mantiene el viewport activo durante el arrastre
        // 'dx' GLOBAL = delta por evento que YA neutraliza la teletransportacion
        // del cursor (CheckWarpMouseInViewport pone dx=0 al wrappear). Por eso
        // acumulamos el delta en vez de usar la X absoluta (que saltaba).
        gFloatDrag->Set(*gFloatDrag->value + dx * gFloatDrag->dragStep);
        return;
    }

    if (editando) return;

    if (gListaResize) {
        if (!leftMouseDown) {
            gListaResize = false;
        } else if (propMeshParts && !propMeshParts->properties.empty()) {
            // arrastrar el borde inferior: 1..10 filas visibles
            PropListMeshParts* lista =
                (PropListMeshParts*)propMeshParts->properties[0];
            int filas = gListaFilas0 +
                        (my - gListaResizeY0) / (RenglonHeightGS + gapGS);
            if (filas < 1) filas = 1;
            if (filas > 10) filas = 10;
            if (filas != lista->filasMax) {
                lista->filasMax = filas;
                lista->AjustarVentana();
                Resize(width, height);
            }
        }
        return;
    }

    // DRAG-SCROLL de un mini-listado: si el press empezo sobre una lista, arrastrar vertical scrollea ESA lista
    // (scrollFila sigue al dedo) en vez del panel entero. Se suelta al levantar el dedo.
    if (gListaScrollLista) {
        if (!leftMouseDown) { gListaScrollLista = NULL; }
        else {
            int n = gListaScrollLista->ListaCount();
            int vis = n < gListaScrollLista->filasMax ? n : gListaScrollLista->filasMax;
            if (n > vis) {
                int rowH = RenglonHeightGS + gapGS; if (rowH < 1) rowH = 1;
                int nuevo = gListaScroll0 - (my - gListaScrollY0) / rowH; // dedo hacia abajo = ver items de arriba
                if (nuevo > n - vis) nuevo = n - vis;
                if (nuevo < 0) nuevo = 0;
                if (nuevo != gListaScrollLista->scrollFila) { gListaScrollLista->scrollFila = nuevo; g_redraw = true; }
            }
            ViewPortClickDown = true;
            return; // consumido por la lista: el panel NO scrollea
        }
    }

    if (middleMouseDown || leftMouseDown) {
        ViewPortClickDown = true;

        ScrollX(dx);
        ScrollY(dy);
        return;
    }
    //si no se esta haciendo click. entonces miras si el mouse esta encima de algo
    else {
        FindMouseOver(mx, my);
    }
}

#ifndef W3D_SYMBIAN
void Properties::event_key_down(int tecla, bool repeticion){
    const int key = tecla;
    if (repeticion == 0) {
        switch (key) {
            case W3dK_LEFT:
                button_left();
                break;
            case W3dK_RIGHT:
                button_right();
                break;
            case W3dK_UP:
                button_up();
                break;
            case W3dK_DOWN:
                button_down();
                break;
            case W3dK_RETURN:
                EnterPropertieSelect();
                break;
            case W3dK_ESCAPE:
                Cancel();
                break;
        };
    }
    else {
        // Evento repetido por mantener apretada
        switch (key) {
            case W3dK_LEFT:
                button_left();
                break;
            case W3dK_RIGHT:
                button_right();
                break;
            case W3dK_UP:
                button_up();
                break;
            case W3dK_DOWN:
                button_down();
                break;
        }
    }
}
#endif

// rect en pantalla del boton de la fila seleccionada (para abrir su desplegable
// alineado por teclado). Igual recorrido que CentrarSeleccion + igual cuenta de
// sx que ClickEn (x + PosX + 2*borderGS) y sy = y + BarTop + PosY + offset.
void Properties::SetRectFilaSeleccionada(){
    if (selectIndex < 0 || selectIndex >= (int)GroupProperties.size()) return;
    GroupPropertie* gsel = GroupProperties[selectIndex];
    if (!gsel->open || gsel->selectIndex < 0 ||
        gsel->selectIndex >= (int)gsel->properties.size()) return;
    PropertieBase* prop = gsel->properties[gsel->selectIndex];
    // Button (desplegable alineado) y Color (abrir el picker con teclado) necesitan la posicion
    if (prop->GetType() != PropertyType::Button && prop->GetType() != PropertyType::Color) return;
    int yFila = borderGS + RenglonHeightGS + gapGS; // titulo
    for (int i = 0; i < (int)GroupProperties.size() && i <= selectIndex; i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        if (i == selectIndex) {
            yFila += borderGS + RenglonHeightGS + gapGS; // cabecera del grupo
            for (int j = 0; j < gsel->selectIndex; j++)
                yFila += g->properties[j]->Resize(g->width);
            break;
        }
        yFila += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
    int sxFila = x + PosX + borderGS + borderGS;
    int syFila = y + BarTopOffset() + PosY + yFila;
    if (prop->GetType() == PropertyType::Button) {
        PropButton* pb = (PropButton*)prop;
        pb->button->sx = sxFila + (pb->conLabel ? gsel->colEtiqueta : 0);
        pb->button->sy = syFila;
    } else { // Color: guardo la posicion para abrir el ColorPicker desde EnterPropertieSelect
        gColorSelSx = sxFila; gColorSelSy = syFila;
    }
}

void Properties::EnterPropertieSelect(){
    PropsActivo = this; // este panel pasa a ser el activo
    // keyframe enfocado por teclado: OK/Enter pone/saca el keyframe (no edita el valor)
    { PropertieBase* p = PropFilaSeleccionada(GroupProperties, selectIndex);
      if (p && p->AnimProp() >= 0 && g_kfFocoProp == (void*)p && W3dKeyframeToggle){
          W3dKeyframeToggle(p->AnimProp(), p->AnimComp());
          return;
      } }
    SetRectFilaSeleccionada(); // desplegable alineado al botón / pos de la fila (nav por teclado)
    editando = GroupProperties[selectIndex]->EnterPropertieSelect();
    ViewPortClickDown = editando;
    // OK/Enter sobre un COLOR: abrir el ColorPicker (igual que el click del mouse) -> sin esto el
    // selector de color SOLO se podia abrir con el mouse. El picker es modal: se lleva el teclado.
    GroupPropertie* g = GroupProperties[selectIndex];
    if (g->selectIndex >= 0 && g->selectIndex < (int)g->properties.size() &&
        g->properties[g->selectIndex]->GetType() == PropertyType::Color) {
        PropColor* pc = (PropColor*)g->properties[g->selectIndex];
        if (pc->value) {
            for (int q = 0; q < 4; q++) pc->originalValue[q] = pc->value[q]; // para Cancel
            pc->editando = true;
            if (!colorPicker) colorPicker = new ColorPicker();
            colorPicker->Abrir(pc->value, gColorSelSx, gColorSelSy);
            // pestania "Pal" del picker: solo con contexto (los campos pal* del
            // elemento; los colores de material no la tienen)
            if (pc->palRef && pc->palObj)
                colorPicker->SetPaleta(pc->palRef, pc->palObj, AccionPickerPalCambio);
            gColorAbierto = pc;
            editando = true; ViewPortClickDown = true;
        }
    }
    // OK/Enter sobre un FLOAT: abrir la edicion por TEXTO (tipear el numero exacto + Enter). En el Nokia las teclas
    // 0-9 escriben y '*' es el punto. Mas rapido y preciso que ajustar con flechas. (El input llega por g_textFieldActivo.)
    if (g->selectIndex >= 0 && g->selectIndex < (int)g->properties.size() &&
        g->properties[g->selectIndex]->GetType() == PropertyType::Float) {
        PropFloat* pf = (PropFloat*)g->properties[g->selectIndex];
        if (pf->value) { pf->IniciarEdicionTexto(); editando = true; ViewPortClickDown = true; }
    }
}

void Properties::Cancel(){
    PropsActivo = this; // este panel pasa a ser el activo
    editando = GroupProperties[selectIndex]->Cancel();
    ViewPortClickDown = editando;
};

void Properties::SetOpenGroup(bool open){
    GroupProperties[selectIndex]->open = open;
    if (!open){
        GroupProperties[selectIndex]->selectIndex = -1;
    }
    Resize(width, height);
}

// centra la opcion seleccionada en el viewport (con topes arriba/abajo)
void Properties::CentrarSeleccion(){
    // y de la fila seleccionada (sin PosY): mismo recorrido que ClickEn,
    // con las alturas reales de cada fila (Resize)
    int yFila = borderGS + RenglonHeightGS + gapGS; // titulo
    for (int i = 0; i < (int)GroupProperties.size() && i <= selectIndex; i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        if (i == selectIndex) {
            if (g->open && g->selectIndex >= 0) {
                yFila += borderGS + RenglonHeightGS + gapGS; // cabecera
                for (int j = 0; j < (int)g->properties.size() && j < g->selectIndex; j++) {
                    yFila += g->properties[j]->Resize(g->width);
                }
            }
            break;
        }
        yFila += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
    // centrado en el area VISIBLE (la de abajo de la barra de botones);
    // los topes hacen que en los extremos quede pegado arriba/abajo
    PosY = -(yFila - (height - BarTopOffset()) / 2);
    if (PosY > 0) PosY = 0;
    if (PosY < MaxPosY) PosY = MaxPosY;
}

void Properties::button_up(){
    PropsActivo = this; // este panel pasa a ser el activo
    if (focoEnTabs){ // en las pestañas: ARRIBA hace wrap a la ultima propiedad (simetrico a bajar desde la ultima)
        focoEnTabs = false;
        EntrarUltimoGrupoVisible();
        return;
    }
    if (!editando){
        g_kfFocoProp = 0;                   // al moverse de fila, el foco del keyframe se suelta
        PrevSelect();                       // en el tope setea focoEnTabs
        if (!focoEnTabs) CentrarSeleccion();
    }
    else {
        GroupProperties[selectIndex]->button_up();
    }
}

void Properties::button_down(){
    PropsActivo = this; // este panel pasa a ser el activo
    if (focoEnTabs){ // bajar = entrar a las propiedades de la pestaña
        focoEnTabs = false;
        EntrarPrimerGrupoVisible(); // al 1er grupo VISIBLE (no a uno oculto)
        return;
    }
    if (!editando){
        g_kfFocoProp = 0;                   // al moverse de fila, el foco del keyframe se suelta
        NextSelect();
        CentrarSeleccion();
    }
    else {
        GroupProperties[selectIndex]->button_down();
    }
}

void Properties::NextSelect(){
    if (GroupProperties[selectIndex]->NextSelect()){
        // saltar grupos INVISIBLES (una camara no tiene mesh parts:
        // se "navegaban" opciones que no existian)
        for (size_t v = 0; v < GroupProperties.size(); v++){
            selectIndex++;
            if (selectIndex >= static_cast<int>(GroupProperties.size())){
                selectIndex = 0;
            }
            if (GroupProperties[selectIndex]->visible) break;
        }
        GroupProperties[selectIndex]->selectIndex = -1;
    }
}

void Properties::PrevSelect(){
    if (GroupProperties[selectIndex]->PrevSelect()){
        // TOPE: si es el primer grupo VISIBLE de la pestaña (no necesariamente
        // el indice 0: en Materiales el visible es otro), salir a las PESTAÑAS.
        bool hayVisibleAntes = false;
        for (int i = 0; i < selectIndex; i++)
            if (GroupProperties[i]->visible){ hayVisibleAntes = true; break; }
        if (!hayVisibleAntes){ LimpiarSeleccionGrupos(); focoEnTabs = true; return; }
        // saltar grupos INVISIBLES (idem NextSelect)
        for (size_t v = 0; v < GroupProperties.size(); v++){
            selectIndex--;
            if (selectIndex < 0){
                selectIndex = static_cast<int>(GroupProperties.size()) - 1;
            }
            if (GroupProperties[selectIndex]->visible) break;
        }

        if (GroupProperties[selectIndex]->open){
            GroupProperties[selectIndex]->selectLastIndexProperty();
        }
        else {
            GroupProperties[selectIndex]->selectIndex = -1;
        }
    }
}

#ifndef W3D_SYMBIAN
void Properties::event_key_up(int tecla){
}
#endif

// devuelve el mini-listado (PropListMeshParts) cuyo BOX cae bajo la coordenada 'py' (o NULL). Mismo recorrido de
// filas que ClickEn/PropFloatEnValueBox. Lo usa el scroll TACTIL para saber si el dedo empezo sobre una lista.
PropListMeshParts* Properties::ListaBajoY(int py) {
    if (!ObjActivo && pestaniaActiva != 0) return NULL;
    int yCursor = y + BarTopOffset() + PosY + borderGS + RenglonHeightGS + gapGS;
    for (size_t i = 0; i < GroupProperties.size(); i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        int hCabecera = borderGS + RenglonHeightGS + gapGS;
        if (py >= yCursor && py < yCursor + hCabecera) return NULL; // cabecera del grupo
        if (g->open) {
            int yFila = yCursor + hCabecera;
            for (size_t j = 0; j < g->properties.size(); j++) {
                PropertieBase* prop = g->properties[j];
                int hFila = prop->Resize(g->width);
                if (hFila > 0 && prop->GetType() == PropertyType::List &&
                    py >= yFila && py < yFila + hFila)
                    return (PropListMeshParts*)prop;
                yFila += hFila;
            }
        }
        yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
    return NULL;
}

void Properties::ClickEn(int mx, int my) {
    PropsActivo = this; // este panel pasa a ser el activo
    g_textFieldActivo = NULL; // cualquier click des-enfoca; abajo se re-enfoca si es texto
    (void)mx; // el arrastre usa el delta global 'dx', no la X del click
    if (editando) {
        // un click mientras se edita ACEPTA el cambio
        EnterPropertieSelect();
        return;
    }
    if (!ObjActivo && pestaniaActiva != 0) return; // sin objeto no hay filas (salvo pestania Render, global)
    // mismo recorrido que el render: el titulo avanza RenglonHeightGS+gapGS
    // (no marginGS) y cada fila mide lo que devuelve su Resize (PropGap es
    // gapGS, checkbox sin valor es 0): antes el mapeo quedaba corrido y el
    // click en "Vertex Color" tocaba "Transparent"
    int yCursor = y + BarTopOffset() + PosY + borderGS + RenglonHeightGS + gapGS;
    for (size_t i = 0; i < GroupProperties.size(); i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        int hCabecera = borderGS + RenglonHeightGS + gapGS;
        if (my >= yCursor && my < yCursor + hCabecera) {
            // -1 = "cabecera ACTIVA" (se pinta accent): los otros grupos
            // van a -2 o quedaban todos verdes al plegar/desplegar
            for (size_t k = 0; k < GroupProperties.size(); k++)
                GroupProperties[k]->selectIndex = -2;
            selectIndex = (int)i;
            g->selectIndex = -1; // el cursor queda en esta cabecera
            g->open = !g->open; // plegar/desplegar el grupo
            Resize(width, height);
            return;
        }
        if (g->open) {
            int yFila = yCursor + hCabecera;
            for (size_t j = 0; j < g->properties.size(); j++) {
                PropertieBase* prop = g->properties[j];
                int hFila = prop->Resize(g->width); // = alto del render
                if (hFila > 0 && prop->GetType() != PropertyType::Gap &&
                    prop->Seleccionable() &&
                    my >= yFila && my < yFila + hFila) {
                    for (size_t k = 0; k < GroupProperties.size(); k++)
                        GroupProperties[k]->selectIndex = -2; // -1 = cabecera
                    selectIndex = (int)i;
                    g->selectIndex = (int)j;
                    // Ctrl+Z de MODIFICACION de material: si se toca un checkbox o el shininess de la tarjeta
                    // Material, snapshotear el material ANTES (se commitea al soltar el mouse, en Render).
                    if (g == propMaterial && (prop->GetType() == PropertyType::Bool || prop->GetType() == PropertyType::Float))
                        UndoMaterialModIniciar(MaterialActivoUI());
                    if (prop->GetType() == PropertyType::Bool) {
                        // BOTON de keyframe a la DERECHA de todo (antes que el toggle del
                        // checkbox, sino tocar el keyframe togglaba la casilla): ultima columna
                        PropBool* pb = (PropBool*)prop;
                        if (pb->animProp >= 0 && W3dKeyframeToggle && g->keyBtnCol > 0) {
                            int xDer = x + PosX + borderGS * 2 + g->width - bordersGS;
                            if (mx >= xDer - g->keyBtnCol && mx < xDer) {
                                W3dKeyframeToggle(pb->animProp, pb->animComp);
                                return;
                            }
                        }
                        prop->EditPropertie(); // checkbox: toggle directo (+ su onChange: los de material re-Rebindean)
                    }
                    else if (prop->GetType() == PropertyType::Color) {
                        PropColor* pc = (PropColor*)prop;
                        // fila de PALETA: [nombre editable][swatch][boton X], cada zona lo suyo
                        int pidx = pc->PaletaIdx();
                        if (pidx >= 0) {
                            int cw = RenglonHeightGS + GlobalScale * 2;
                            int xBtn = x + PosX + borderGS * 2 + g->width - bordersGS - cw;
                            int xSw  = xBtn - cw - gapGS;
                            if (mx >= xBtn) {           // el boton X: borra el color
                                // INVARIANTE 2: se borra el MISMO indice en
                                // TODAS las paletas del proyecto y se corrigen
                                // TODAS las referencias (W3dPaletaBorrarColor)
                                W3dPaletaBorrarColor(pidx);
                                target = NULL;   // re-bind: la tarjeta se reconstruye
                                g_redraw = true;
                                return;
                            }
                            if (mx < xSw) {             // el nombre: se edita inline (como Name)
                                PropColorPal* pp = (PropColorPal*)pc;
                                if (pp->nom) pp->field.SetText(*pp->nom);
                                g_textFieldActivo = &pp->field;
#ifdef __EMSCRIPTEN__
                                if (g_uiTapEnCurso) SDL_StartTextInput();
#else
                                if (g_uiTapEnCurso) QwertyAbrir();
#endif
                                g_redraw = true;
                                return;
                            }
                        }
                        if (pc->value) {
                            // selector de color (popup); la fila queda
                            // con BORDE VERDE mientras se edita
                            if (!colorPicker) colorPicker = new ColorPicker();
                            // abrir CERCA del click (con el cursor DENTRO del popup, arriba-izq): antes se abria pegado
                            // al borde IZQUIERDO del panel, lejos del mouse -> al acercarse "salia del area" y se cerraba.
                            colorPicker->Abrir(pc->value, mx - GlobalScale * 10, my - GlobalScale * 6);
                            // pestania "Pal": solo campos con contexto de paleta
                            if (pc->palRef && pc->palObj)
                                colorPicker->SetPaleta(pc->palRef, pc->palObj, AccionPickerPalCambio);
                            pc->editando = true;
                            gColorAbierto = pc;
                        }
                    }
                    else if (prop->GetType() == PropertyType::Button) {
                        // rect absoluto (los desplegables abren debajo). Con label el boton
                        // vive en la COLUMNA DE VALORES: sin el corrimiento el menu abria
                        // enganchado al borde izquierdo de la fila, no al del boton.
                        PropButton* pb = (PropButton*)prop;
                        pb->button->sx = x + PosX + borderGS + borderGS +
                                         (pb->conLabel ? g->colEtiqueta : 0);
                        pb->button->sy = yFila;
                        prop->EditPropertie(); // accion del boton
                    }
                    else if (prop->GetType() == PropertyType::ButtonRow) {
                        // hit-test la CELDA por X (los botones se reparten el ancho en partes iguales)
                        PropButtonRow* row = (PropButtonRow*)prop;
                        int leftX = x + PosX + borderGS + borderGS; // borde izq del cuerpo (igual que el Button)
                        int cw = row->AnchoCelda(g->width);
                        int cx = leftX;
                        for (size_t b = 0; b < row->botones.size(); b++) {
                            if (!row->botones[b]->visible) continue;
                            if (mx >= cx && mx < cx + cw) {
                                // rect ABSOLUTO del boton (igual que el PropButton de arriba): asi un boton de la
                                // fila que abre un DESPLEGABLE (ej. "Add" de modificadores) lo abre JUSTO debajo suyo
                                // y no en una esquina (su sx/sy no se seteaban en el click de la fila -> quedaban stale).
                                row->botones[b]->sx = cx;
                                row->botones[b]->sy = yFila;
                                row->Disparar((int)b);
                                break;
                            }
                            cx += cw + gapGS;
                        }
                    }
                    else if (prop->GetType() == PropertyType::List) {
                        PropListMeshParts* lista = (PropListMeshParts*)prop;
                        if (my >= yFila + hFila - gapGS - borderGS) {
                            // agarre del BORDE INFERIOR: arrastrar cambia
                            // el alto de la lista (1..10 filas)
                            gListaResize = true;
                            gListaResizeY0 = my;
                            gListaFilas0 = lista->filasMax;
                            return;
                        }
                        // item clickeado (la ventana arranca en scrollFila)
                        int item = lista->scrollFila +
                                   (my - yFila - borderGS) / (RenglonHeightGS + gapGS);
                        int n = lista->ListaCount(); // parts / uv maps / colors segun el modo
                        if (item >= n) item = n - 1;
                        if (item >= 0 && n > 0) {
                            lista->ListaSeleccionar(item); // setea el activo + re-bind/re-bake
                            lista->AjustarVentana();
                        }
                        // armar el drag-scroll tactil de ESTA lista (si se arrastra vertical, scrollea la lista)
                        gListaScrollLista = lista;
                        gListaScrollY0 = my;
                        gListaScroll0 = lista->scrollFila;
                    }
                    else if (prop->GetType() == PropertyType::Float) {
                        // arma el POSIBLE arrastre del valor: si el mouse se mueve arrastra; si no, al soltar abre
                        // la edicion por texto (mouse_button_up). Las flechas del teclado siguen andando igual.
                        PropFloat* pf = (PropFloat*)prop;
                        // BOTON de keyframe: ultima columna a la DERECHA (g->keyBtnCol de ancho)
                        if (pf->animProp >= 0 && W3dKeyframeToggle && g->keyBtnCol > 0) {
                            int xDer = x + PosX + borderGS * 2 + g->width - bordersGS;
                            if (mx >= xDer - g->keyBtnCol && mx < xDer) {
                                W3dKeyframeToggle(pf->animProp, pf->animComp);
                                return;
                            }
                        }
#ifndef W3D_SYMBIAN
                        // TAP TACTIL (el arrastre ya lo maneja el slider aparte): edicion inline + el TECLADO
                        // NUMERICO de Whisk3D (popup abajo). Con mouse (PC) y en Symbian sigue el camino clasico.
                        if (pf->value && g_uiTapEnCurso) {
                            pf->IniciarEdicionTexto(); editando = true; ViewPortClickDown = true;
                            NumPadAbrir();
                            return;
                        }
#endif
                        if (pf->value) { gFloatDrag = pf; gFloatDragMoved = false; gFloatDragAccum = 0.0f; }
                    }
                    else if (prop->GetType() == PropertyType::Text) {
                        PropText* pt = static_cast<PropText*>(prop);
                        if (pt->onClick) { pt->onClick(); return; } // campo "Path": al clickear abre el explorador (no se edita)
                        // el campo "Name" se sincroniza con ObjActivo->name solo cuando NO esta enfocado (ver
                        // SincronizarNombreObjeto). Si se clickea el mismo frame en que se reconstruyo el panel, el
                        // campo todavia esta vacio -> lo poblamos ACA (al empezar a editar) con el nombre actual.
                        if (pt == propNameObj && ObjActivo) pt->field.SetText(ObjActivo->name);
                        prop->EditPropertie(); // ENFOCA la caja: el texto entra por SDL_TEXTINPUT
#ifdef __EMSCRIPTEN__
                        if (g_uiTapEnCurso) SDL_StartTextInput(); // solo en TAP tactil: levanta el teclado del celu
#else
                        if (g_uiTapEnCurso) QwertyAbrir(); // TAP TACTIL (Android/Symbian): teclado QWERTY de Whisk3D
#endif
                    }
                    return;
                }
                yFila += hFila;
            }
        }
        // paso al proximo grupo: igual que el net-translate del render
        yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
}

// campo numerico EN ARRASTRE TACTIL (slider), independiente del gFloatDrag del mouse. Asi el gesto tactil no
// se pisa con el flujo de mouse (era la causa de que el scroll se rompiera al quedar gFloatDrag colgado).
static PropFloat* gTouchSlide = NULL;

// PropFloat cuyo VALUE BOX (columna de valores) esta bajo (mx,my), o NULL. Mismo recorrido de filas que ClickEn.
PropFloat* Properties::PropFloatEnValueBox(int mx, int my){
    if ((!ObjActivo && pestaniaActiva != 0) || !Contains(mx, my)) return NULL;
    int yCursor = y + BarTopOffset() + PosY + borderGS + RenglonHeightGS + gapGS;
    for (size_t i = 0; i < GroupProperties.size(); i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        int hCabecera = borderGS + RenglonHeightGS + gapGS;
        if (my >= yCursor && my < yCursor + hCabecera) return NULL;  // cabecera del grupo
        if (g->open) {
            int yFila = yCursor + hCabecera;
            for (size_t j = 0; j < g->properties.size(); j++) {
                PropertieBase* prop = g->properties[j];
                int hFila = prop->Resize(g->width);
                if (hFila > 0 && prop->GetType() != PropertyType::Gap &&
                    prop->Seleccionable() && my >= yFila && my < yFila + hFila) {
                    if (prop->GetType() != PropertyType::Float) return NULL;
                    // borde izq de la columna de valores DE ESTE grupo (el global PropColEtiqueta queda con
                    // el valor del ultimo grupo renderizado -> el hit-test salia corrido en los demas)
                    int colValor = x + PosX + borderGS + borderGS + g->colEtiqueta;
                    return (mx >= colValor) ? (PropFloat*)prop : NULL; // en el label -> no es el campo
                }
                yFila += hFila;
            }
        }
        yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
    return NULL;
}

bool Properties::PuntoEnCampoNumerico(int mx, int my){ return PropFloatEnValueBox(mx, my) != NULL; }

bool Properties::TouchSliderArmar(int mx, int my){
    PropFloat* pf = PropFloatEnValueBox(mx, my);
    gTouchSlide = (pf && pf->value) ? pf : NULL;
    return gTouchSlide != NULL;
}
void Properties::TouchSliderMover(int dx){
    if (gTouchSlide && gTouchSlide->value) gTouchSlide->Set(*gTouchSlide->value + dx * gTouchSlide->dragStep);
}
void Properties::TouchSliderSoltar(){ gTouchSlide = NULL; }

void Properties::key_down_return(){
    PropsActivo = this; // este panel pasa a ser el activo
    // entra/acepta la edicion de la propiedad seleccionada (estaba vacio,
    // tambien en PC)
    EnterPropertieSelect();
}

Properties::~Properties() {
    // si este panel era el ACTIVO, limpiar el puntero global: sino queda colgando
    // (dangling) y cualquier lectura de PropsActivo crashea (ej. al reemplazar este
    // panel por un UV Editor, que lee la parte activa via PropsActivo).
    if (PropsActivo == this) PropsActivo = NULL;
}