#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "variables.h"
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
#include "io/Fuente2D.h"
#include "io/Textura2D.h"      // tamano natural del archivo al elegir la textura
#include "io/UI2DFormato.h"    // guardar/cargar interfaces (.w3dui)
#include "objects/UI.h"
#include "render/UIOverlay.h"   // UI2D_PuntoAncla (rebase del ancla)   // T(): los textos salen en el idioma del sistema
#include "ViewPorts/Timeline.h"   // keyframe ACTIVO + InvalidarAnimYRedraw (tarjeta "Keyframe")
#include "Properties.h"
#include "Undo.h" // Ctrl+Z: capturar rename
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
#include "edit/Modifier.h"    // ModifierType (ids del menu Add del stack de modificadores)
#include "objects/Armature.h" // pestania Animation: clips del esqueleto
#include "animation/SkeletalAnimation.h" // CrearAnimacion/BorrarAnimacionActiva/MoverAnimacionActiva
#include "importers/import_obj.h" // ExportOBJ (boton Wavefront.obj de la tarjeta Export)
#include "importers/export_gltf.h" // ExportGLTF (glTF/GLB: rig + animaciones, sin hornear el skinning)
#include "render/OpcionesRender.h" // g_redraw (scroll de la lista con la rueda)
#include "ViewPorts/ViewPort3D.h"  // Viewport3D::RenderAPNG + Viewport3DActive (render a PNG)
#include <cstdio>
#include <string>
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
static std::string NombreMaterialLibre(){
    if (!BuscarMaterialPorNombre("Material")) return "Material";
    char buf[32];
    for (int n = 1; n < 1000; n++){
        sprintf(buf, "Material.%03d", n);
        if (!BuscarMaterialPorNombre(buf)) return std::string(buf);
    }
    return "Material.999";
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
// seleccion visual: el modificador movido queda resaltado, como pidio Dante).
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
// nombre, para que renombrar al mismo valor no choque). Cada scope junta los punteros a sus nombres. =====
static std::string UniqueNombre(const std::string& n, std::string* excl, const std::vector<std::string*>& nombres){
    std::string cand = n; int suf = 0;
    for (;;){
        bool choca = false;
        for (size_t i = 0; i < nombres.size(); i++)
            if (nombres[i] != excl && *nombres[i] == cand){ choca = true; break; }
        if (!choca) return cand;
        ++suf; char b[16]; sprintf(b, ".%03d", suf); cand = n + b;
    }
}
static std::string UniqMaterial(const std::string& n, std::string* excl){
    std::vector<std::string*> v; for (size_t i = 0; i < Materials.size(); i++) v.push_back(&Materials[i]->name);
    return UniqueNombre(n, excl, v);
}
static std::string UniqUVMap(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->uvMaps.size(); i++) v.push_back(&m->uvMaps[i]->nombre); }
    return UniqueNombre(n, excl, v);
}
static std::string UniqColor(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->colorLayers.size(); i++) v.push_back(&m->colorLayers[i]->nombre); }
    return UniqueNombre(n, excl, v);
}
static std::string UniqVGroup(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->vertexGroups.size(); i++) v.push_back(&m->vertexGroups[i]->nombre); }
    return UniqueNombre(n, excl, v);
}
// armature activo (o NULL): fuente de la pestania Animation
static Armature* ArmActiva(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::armature) ? (Armature*)ObjActivo : NULL;
}
static std::string UniqAnim(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (Armature* a = ArmActiva())
        for (size_t i = 0; i < a->animations.size(); i++) v.push_back(&a->animations[i]->name);
    return UniqueNombre(n, excl, v);
}
static void RecolectarNombresObj(Object* o, std::vector<std::string*>& v){
    v.push_back(&o->name);
    for (size_t i = 0; i < o->Childrens.size(); i++) RecolectarNombresObj(o->Childrens[i], v);
}
static std::string UniqObjeto(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (SceneCollection) for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
        RecolectarNombresObj(SceneCollection->Childrens[i], v);
    return UniqueNombre(n, excl, v);
}

// ===== Rename: el BOTON se vuelve un INPUT in-place (no un campo abajo). Al ACEPTAR escribe el nombre
// (uniquificado segun el scope); cancelar lo descarta. El input lo rutea controles.cpp a g_textFieldActivo. =====
static std::string* g_renameTarget = NULL; // NULL = no hay rename en curso
static TextField    g_renameField;         // el texto que se edita (se dibuja DENTRO del boton)
static Button*      g_renameBoton = NULL;  // el boton que se volvio input
static std::string (*g_renameUniq)(const std::string&, std::string*) = NULL; // uniquificador del scope (o NULL)

bool RenameActivo(){ return g_renameTarget != NULL; }

static void RenameLimpiar(){
    if (g_renameBoton) g_renameBoton->editField = NULL; // el boton vuelve a ser boton
    g_renameTarget = NULL;
    g_renameBoton = NULL;
    g_renameUniq = NULL;
    g_textFieldActivo = NULL;
}
void RenameCommit(){ // ACEPTAR: escribe el texto (uniquificado) en el nombre destino
    if (g_renameTarget) {
        UndoCapturarRename(g_renameTarget); // Ctrl+Z: guarda el nombre PREVIO antes de escribir
        *g_renameTarget = g_renameUniq ? g_renameUniq(g_renameField.text, g_renameTarget) : g_renameField.text;
    }
    RenameLimpiar();
    RebindMaterialMeshPart(); // refresca el texto mostrado (boton de material / lista de parts)
    g_redraw = true;
}
void RenameCancel(){ RenameLimpiar(); g_redraw = true; } // CANCELAR: no escribe nada

// 'boton' se vuelve input (con TODO seleccionado). 'uniq' = uniquificador del scope (NULL = sin chequeo).
static void RenameIniciar(Button* boton, std::string* destino, std::string (*uniq)(const std::string&, std::string*) = NULL){
    if (!boton || !destino) return;
    g_renameTarget = destino;
    g_renameBoton = boton;
    g_renameUniq = uniq;
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
static void AccionRenameMeshPart(){ // las PARTES si pueden repetir nombre (no uniquifica)
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0 || !PropsActivo) return;
    if (!PropsActivo->propRowDelRen || PropsActivo->propRowDelRen->botones.size() < 2) return;
    RenameIniciar(PropsActivo->propRowDelRen->botones[1], &m->materialsGroup[idx].name); // [1] = Rename
}
static void AccionRenameMaterial(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0 || !PropsActivo) return;
    Material* mat = m->materialsGroup[idx].material;
    if (!mat || mat == MaterialDefecto) return; // el material POR DEFECTO no se renombra (es global)
    if (!PropsActivo->propBtnRenameMat) return;
    RenameIniciar(PropsActivo->propBtnRenameMat->button, &mat->name, UniqMaterial); // GLOBAL unico
}
static void AccionRenameUVMap(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameUV) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->uvMapActivo < 0 || m->uvMapActivo >= (int)m->uvMaps.size()) return;
    RenameIniciar(PropsActivo->propBtnRenameUV->button, &m->uvMaps[m->uvMapActivo]->nombre, UniqUVMap);
}
static void AccionRenameColor(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameColor) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->colorActivo < 0 || m->colorActivo >= (int)m->colorLayers.size()) return;
    RenameIniciar(PropsActivo->propBtnRenameColor->button, &m->colorLayers[m->colorActivo]->nombre, UniqColor);
}
static void AccionRenameGroup(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameGroup) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->grupoActivo < 0 || m->grupoActivo >= (int)m->vertexGroups.size()) return;
    RenameIniciar(PropsActivo->propBtnRenameGroup->button, &m->vertexGroups[m->grupoActivo]->nombre, UniqVGroup);
}
static void AccionRenameAnim(){
    Armature* a = ArmActiva();
    if (!a || !PropsActivo || !PropsActivo->propBtnRenameAnim) return;
    if (a->animActiva < 0 || a->animActiva >= (int)a->animations.size()) return;
    RenameIniciar(PropsActivo->propBtnRenameAnim->button, &a->animations[a->animActiva]->name, UniqAnim);
}
// NOMBRE del objeto: el campo propNameObj muestra ObjActivo->name (cuando NO se edita) y, al perder el foco,
// escribe lo tipeado (uniquificado) en ObjActivo->name. Se llama por frame desde RefreshTargetProperties.
static std::string* g_nameEditTarget = NULL; // != NULL mientras se edita el nombre (captura el destino al enfocar)
static PropText*    g_nameEditCampo  = NULL; // CUAL campo Name esta editando (hay uno por tarjeta contextual)
// sincroniza UN campo Name contra ObjActivo->name: muestra el nombre (cuando no se edita) y al
// perder el foco commitea lo tipeado (uniquificado). Corre ANTES de dibujar los campos
// (RefreshTargetProperties al inicio de Render) -> el nombre ya se ve en el primer frame.
static void SincronizarNombreCampo(PropText* pt){
    if (!pt) return;
    bool foco = (g_textFieldActivo == &pt->field);
    if (foco && !g_nameEditTarget && ObjActivo){ g_nameEditTarget = &ObjActivo->name; g_nameEditCampo = pt; }
    if (!foco && g_nameEditTarget && g_nameEditCampo == pt){   // termino -> commit uniquificado
        UndoCapturarRename(g_nameEditTarget);
        *g_nameEditTarget = UniqObjeto(pt->field.text, g_nameEditTarget);
        g_nameEditTarget = NULL; g_nameEditCampo = NULL;
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
    SincronizarNombreCampo(p->propUInombre);
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
    ((Mesh*)ObjActivo)->AgregarModificador(id); // id = ModifierType (Screw/Mirror/Array/SubSurf/Boolean)
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
        MenuAddModifier->Agregar(T("Armature"),            ModifierType::Armature, (int)IconType::armature);
    }
    if (PropsActivo->propRowMod && !PropsActivo->propRowMod->botones.empty()){
        AbrirMenuBajoBoton(MenuAddModifier, PropsActivo->propRowMod->botones[0]); // el boton "Add"
    }
}
static void AccionRemoveModifier(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->QuitarModificadorActivo();
    SelEnListaModificador();
    PropertiesLayoutDirty = true;
}
static void AccionModifierUp(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->MoverModificador(-1); // sube en el stack (el orden importa)
    SelEnListaModificador();                  // mantiene seleccionado el modificador MOVIDO
    PropertiesLayoutDirty = true;
}
static void AccionModifierDown(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->MoverModificador(+1); // baja en el stack
    SelEnListaModificador();
    PropertiesLayoutDirty = true;
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
    // (pedido de Dante: el elemento salta al nuevo punto, sin tocarle los valores)
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
static void T2dFuenteElegida(const std::string& ruta){
    Texto2D* t = T2dActivo(); if (!t) return;
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
static void ImgTexturaElegida(const std::string& ruta){
    Imagen2D* im = Img2dActiva(); if (!im) return;
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
    std::string ruta = carpeta + "/" + u->name + ".w3dui";
    if (UI2DGuardar(u, ruta)) Notificar(std::string(T("Saved: ")) + ruta, false);
    else                      Notificar(T("Could not write the file"), true);
}
static void AccionUIexportar(){
    if (!UIActivaProps()) return;
    AbrirFileBrowser(T("Export UI to..."), T("Select Folder"), ".w3dui", UIExportCarpetaElegida, true);
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
static void AccionTamPxToggle(){
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: unidades y rangos cambian
    g_redraw = true;
}
// ajusta unidad/rango/pasos de una fila Width/Height segun el modo (px o relativo)
static void AjustarFilaTam(PropFloat* f, bool px){
    if (!f) return;
    f->unit = px ? "px" : "";
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

// fila COMPACTA de la paleta: nombre a la IZQUIERDA (label) + swatch a la DERECHA
// (lo de siempre de PropColor) + un cuadradito con CRUZ para borrar la entrada.
class PropColorPal : public PropColor {
public:
    int idx;
    PropColorPal(const std::string& nom, int i) : PropColor(nom), idx(i) {}
    int PaletaIdx() const override { return idx; }
    void RenderPropertiValue(Card* propertiBox) override {
        // la CRUZ: un cuadradito a la IZQUIERDA del swatch (que esta pegado a la derecha)
        int cw = RenglonHeightGS + GlobalScale * 2;
        float xCruz = (float)(width - PropColEtiqueta - cw * 2 - gapGS - bordersGS);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(xCruz, 0, 0);
        RenderBitmapText("x", textAlign::center, cw);
        w3dEngine::PopMatrix();
        PropColor::RenderPropertiValue(propertiBox);
    }
};

// ============================ PALETA DE COLORES ============================
static Imagen2D* Img2dActiva();      // (definidos mas abajo; las acciones de paleta los usan)
static Rect2D* Rect2dActivo();
static Slice9* S9Activo();
static Boton2D* Btn2dActivo();
static UI* UIActivaProps();
// el UI cuya paleta manda para el elemento activo (su raiz, o el mismo si es un UI)
static UI* PaletaUIDe(Object* o){
    for (Object* p = o; p; p = p->Parent)
        if (p->getType() == ObjectType::ui) return (UI*)p;
    return NULL;
}
static const char* PalNombre(UI* u, int idx){
    if (!u || idx < 0) return "Propio";
    std::vector<PaletaColor>& cs = u->Colores();
    if (idx >= (int)cs.size()) return "Propio";
    return cs[idx].nombre.c_str();
}
// que indice esta editando el menu de paleta abierto (apunta al campo int del elemento)
static int* g_palTarget = NULL;
static PropButton* g_palBoton = NULL;
static PopupMenu* MenuPal = NULL;
static void AccionPalElegida(int id){
    if (!g_palTarget) return;
    *g_palTarget = id;   // -1 = color propio
    if (g_palBoton) g_palBoton->button->text = PalNombre(PaletaUIDe(ObjActivo), id);
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: el swatch aparece/desaparece
    g_redraw = true;
}
static void AbrirMenuPal(int* target, PropButton* boton){
    UI* u = PaletaUIDe(ObjActivo);
    if (!u || !boton) return;
    g_palTarget = target; g_palBoton = boton;
    if (!MenuPal){ MenuPal = new PopupMenu(); MenuPal->action = AccionPalElegida; }
    MenuPal->Limpiar();
    MenuPal->titulo = T("Palette");
    MenuPal->Agregar("Propio", -1);
    std::vector<PaletaColor>& cs = u->Colores();
    for (size_t i = 0; i < cs.size(); i++)
        MenuPal->Agregar(cs[i].nombre, (int)i);
    AbrirMenuBajoBoton(MenuPal, boton->button);
}
static void AccionPalT2d(){  Texto2D* t = T2dActivo();   if (t && PropsActivo) AbrirMenuPal(&t->palColor, PropsActivo->propT2dPal); }
static void AccionPalImg(){  Imagen2D* i = Img2dActiva();if (i && PropsActivo) AbrirMenuPal(&i->palTinte, PropsActivo->propImgPal); }
static void AccionPalRect(){ Rect2D* r = Rect2dActivo(); if (r && PropsActivo) AbrirMenuPal(&r->palColor, PropsActivo->propRectPal); }
static void AccionPalS9(){   Slice9* s = S9Activo();     if (s && PropsActivo) AbrirMenuPal(&s->palTinte, PropsActivo->propS9Pal); }
static void AccionPalBtnFondo(){ Boton2D* b = Btn2dActivo(); if (b && PropsActivo) AbrirMenuPal(&b->palFondo, PropsActivo->propBtnPalFondo); }
static void AccionPalBtnTexto(){ Boton2D* b = Btn2dActivo(); if (b && PropsActivo) AbrirMenuPal(&b->palTexto, PropsActivo->propBtnPalTexto); }
static void AccionPalBtnBorde(){ Boton2D* b = Btn2dActivo(); if (b && PropsActivo) AbrirMenuPal(&b->palBorde, PropsActivo->propBtnPalBorde); }
// agregar un color a la paleta del UI activo
static void AccionPaletaAgregar(){
    UI* u = UIActivaProps(); if (!u) return;
    char nom[24];
    snprintf(nom, sizeof(nom), "Color %d", (int)u->Colores().size() + 1);
    float blanco[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    u->AgregarPaleta(nom, blanco);
    if (PropsActivo) PropsActivo->target = NULL;   // re-bind: la tarjeta se reconstruye
    g_redraw = true;
}
// los nombres de la paleta se editan EN VIVO (los PropText dinamicos de la tarjeta)
// desplegable de PALETAS: elegir la activa (recolorea todo) o crear una nueva (copia)
static PopupMenu* MenuPaletas = NULL;
static void AccionPaletasElegida(int id){
    UI* u = UIActivaProps(); if (!u) return;
    if (id >= (int)u->paletas.size()) {          // "Nueva paleta": copia de la activa
        char nom[24];
        snprintf(nom, sizeof(nom), "Paleta %d", (int)u->paletas.size() + 1);
        u->NuevaPaleta(nom);
    } else u->paletaActiva = id;
    if (PropsActivo) PropsActivo->target = NULL;
    g_redraw = true;
}
static void AccionMenuPaletas(){
    UI* u = UIActivaProps(); if (!u || !PropsActivo || !PropsActivo->propPaletaSel) return;
    if (!MenuPaletas){ MenuPaletas = new PopupMenu(); MenuPaletas->action = AccionPaletasElegida; }
    MenuPaletas->Limpiar();
    MenuPaletas->titulo = T("Palette");
    for (size_t i = 0; i < u->paletas.size(); i++)
        MenuPaletas->Agregar(u->paletas[i].nombre, (int)i);
    MenuPaletas->Agregar(T("New Palette"), (int)u->paletas.size());
    AbrirMenuBajoBoton(MenuPaletas, PropsActivo->propPaletaSel->button);
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
static void BtnTexElegida(const std::string& ruta){
    Boton2D* b = Btn2dActivo(); if (!b) return;
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
static void BtnIconoElegido(const std::string& ruta){
    Boton2D* b = Btn2dActivo(); if (!b) return;
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
static void S9TexturaElegida(const std::string& ruta){
    Slice9* s9 = S9Activo(); if (!s9) return;
    s9->textura = ruta;
    int w = 0, h = 0;
    if (Textura2DObtener(ruta, &w, &h) && w > 0 && h > 0 && s9->tamPx) {
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
    if ((Object*)m->skinArmature != arm){ m->skinArmature = (Armature*)arm; m->lastSkinFrame = -999999; g_redraw = true; }
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
// de salida por defecto (Android: Descargas). El nombre se limpia de carpetas por las dudas.
static std::string CombinarSalida(const std::string& dir, const std::string& nombre, const char* nombreDefecto){
    std::string carpeta = dir.empty() ? w3dFileSystem::GetDefaultOutputDir() : dir;
    std::string nm = SoloNombre(nombre);
    if (nm.empty()) nm = nombreDefecto;
    return w3dFileSystem::JoinPath(carpeta, nm);
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
    std::string full   = CombinarSalida(dir, nombre, porDefecto.c_str());
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
    // si el usuario eligio un archivo de modelo existente -> separar carpeta y nombre
    size_t dot = elegido.find_last_of('.');
    bool esArchivo = (dot != std::string::npos && dot + 1 < elegido.size() && elegido.find_last_of("/\\") < dot);
    if (esArchivo) {
        PropsActivo->propExportPath->field.SetText(w3dFileSystem::ParentPath(elegido));
        if (PropsActivo->propExportName) PropsActivo->propExportName->field.SetText(SoloNombre(elegido));
    } else {
        PropsActivo->propExportPath->field.SetText(elegido);
    }
}
// boton de la carpeta: abre el explorador para elegir la carpeta de salida
static void AccionBrowseExport(){
    AbrirFileBrowser("Export to...", "Select Folder", ".obj .gltf .glb .fbx", ExportFolderElegido, true);
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
    std::string full = CombinarSalida(dir, nm, "render.png");
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
    bool esPng = (elegido.size() >= 4 && elegido.substr(elegido.size() - 4) == ".png");
    if (esPng) {
        PropsActivo->propRenderPath->field.SetText(w3dFileSystem::ParentPath(elegido));
        if (PropsActivo->propRenderOutput) PropsActivo->propRenderOutput->field.SetText(SoloNombre(elegido));
    } else {
        PropsActivo->propRenderPath->field.SetText(elegido);
    }
}
static void AccionBrowseRender(){
    AbrirFileBrowser("Render to...", "Select Folder", ".png", RenderFolderElegido, true);
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
static void AccionAnimStart(){ int v = (int)(g_animStartF + 0.5f); if (v < 0) v = 0; AnimSetStart(v); }
static void AccionAnimEnd(){ int v = (int)(g_animEndF + 0.5f); if (v < 1) v = 1; AnimSetEnd(v); }
// los campos SIEMPRE reflejan los globales reales (que el import / el timeline cambian). Sin esto el display
// mostraba 30 pero se reproducia a 24. Lo llama RefreshTargetProperties cada frame (salvo el campo en edicion).
void SincronizarAnimFps(){
    if (gPropAnimFps   && g_propFloatEditando != gPropAnimFps)   g_animFpsF   = (float)AnimFPS;
    if (gPropAnimStart && g_propFloatEditando != gPropAnimStart) g_animStartF = (float)StartFrame;
    if (gPropAnimEnd   && g_propFloatEditando != gPropAnimEnd)   g_animEndF   = (float)EndFrame;
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
// ARMATURE: crear / borrar / mover el clip de animacion activo (mismo patron que los vertex groups)
static void InvalidarSkinEscena(); // def mas abajo (re-deforma las mallas skinneadas a la pose actual)
static void AccionAnimAdd()  { Armature* a = ArmActiva(); if (a) { CrearAnimacion(a); InvalidarSkinEscena(); PropertiesLayoutDirty = true; g_redraw = true; } } // New Animation: clip vacio en pose reset
static void AccionAnimDup()  { Armature* a = ArmActiva(); if (a && a->animActiva >= 0) { DuplicarAnimacionActiva(a); PropertiesLayoutDirty = true; g_redraw = true; } } // Duplicate: copia el clip activo
static void AccionAnimDel()  { Armature* a = ArmActiva(); if (a) { BorrarAnimacionActiva(a);  PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionAnimUp()   { Armature* a = ArmActiva(); if (a) { MoverAnimacionActiva(a,-1); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionAnimDown() { Armature* a = ArmActiva(); if (a) { MoverAnimacionActiva(a,+1); PropertiesLayoutDirty = true; g_redraw = true; } }

// ===== tarjeta ANIMATION: selector de la animacion ACTIVA (Scene(s) / clips del armature seleccionado) + New/Delete
// + Rename + Render Animation. La seleccion es APP-WIDE (ActiveAnimKind/ActiveAnimArm/SceneAnimActiva en el core): la
// comparten esta card y el Timeline, y NO depende del objeto seleccionado (clickear un armature no la cambia). =====
// El menu del selector es JERARQUICO (sino un esqueleto con 200 clips seria inmanejable): un submenu "Scenes" con
// todas las animaciones de escena, y un submenu por ARMADURA (con el nombre del esqueleto) con SUS clips. Asi las
// animaciones de todas las armaduras estan disponibles sin tener que seleccionar el objeto.
// ids: [0..) = escena; [BASE + armIdx*STRIDE + clipIdx] = clip. armIdx indexa g_animMenuArms (llenado al construir).
static const int ANIM_CLIP_BASE = 100000;
static const int ANIM_CLIP_STRIDE = 1000; // hasta 1000 clips por armadura
static std::vector<PopupMenu*> g_animSubmenus; // pool reutilizable (0 = Scenes, 1.. = por armadura); persiste entre aperturas
static std::vector<Armature*>  g_animMenuArms; // armaduras (con clips) en el orden del menu, para decodificar el id
static std::string NombreAnimActiva(){
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size() &&
        ActiveAnimArm->animations[ActiveAnimArm->animActiva])
        return ActiveAnimArm->animations[ActiveAnimArm->animActiva]->name;
    return NombreEscenaActiva();
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
void ConstruirMenuAnim(PopupMenu* menu){
    menu->Limpiar();
    InitSceneAnimations();
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
static void AccionAnimNewCard(){
    if (ActiveAnimKind == 1 && ActiveAnimArm){ CrearAnimacion(ActiveAnimArm); InvalidarSkinEscena(); } // nuevo clip (arranca en pose reset)
    else { NuevaEscena(); ActiveAnimKind = 0; }                             // nueva animacion de ESCENA
    AnimCargarRangoActivo(); // la animacion nueva arranca con su rango/fps por defecto
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionAnimDupCard(){ // Duplicate: copia el clip activo del armature (solo si hay clip)
    if (ActiveAnimKind == 1 && ActiveAnimArm && ActiveAnimArm->animActiva >= 0){
        DuplicarAnimacionActiva(ActiveAnimArm); AnimCargarRangoActivo();
    }
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionAnimDelCard(){
    if (ActiveAnimKind == 1 && ActiveAnimArm){
        BorrarAnimacionActiva(ActiveAnimArm);
        if (ActiveAnimArm->animations.empty()){ ActiveAnimKind = 0; ActiveAnimArm = NULL; } // sin clips -> volver a escena
    } else BorrarEscenaActiva();
    AnimCargarRangoActivo(); // cargar el rango/fps de la animacion que quedo activa
    PropertiesLayoutDirty = true; g_redraw = true;
}
static void AccionAnimRenameCard(){                                          // renombra la animacion activa in-place
    if (!PropsActivo || !PropsActivo->propBtnAnimRename) return;
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size())
        RenameIniciar(PropsActivo->propBtnAnimRename->button, &ActiveAnimArm->animations[ActiveAnimArm->animActiva]->name, UniqAnim);
    else { InitSceneAnimations(); RenameIniciar(PropsActivo->propBtnAnimRename->button, &SceneAnimations[SceneAnimActiva]->name); }
}
// ARMATURE / Pose Mode: transform del HUESO activo. Los campos PropFloat escriben en estos mirrors; el callback los
// vuelca a restT/restR/restS del hueso (la pose de rest, que es la que se ve cuando el clip activo no anima ese hueso).
static float g_bonePosX=0, g_bonePosY=0, g_bonePosZ=0;
static float g_boneRotX=0, g_boneRotY=0, g_boneRotZ=0;
static float g_boneSclX=1, g_boneSclY=1, g_boneSclZ=1;
static void BoneParentNoop(){} // el campo "Parent" es solo lectura (onClick no-op -> no se edita)
static W3dBone* BoneActivoUI(){
    Armature* a = ArmActiva();
    if (!a || a->boneActivo < 0 || a->boneActivo >= (int)a->bones.size()) return NULL;
    return &a->bones[a->boneActivo];
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

void Properties::ConstruirGrupos(){
    propTransform = new GroupPropertie(T("Transform"));

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

    GroupProperties.push_back(propTransform);

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
    propT2dRot = new PropFloat(T("Rotation"), "o");   // debajo de Tamano (pedido Dante)
    propTexto2D->properties.push_back(propT2dRot);
    // desplegables: label a la IZQUIERDA, el boton del menu a la DERECHA (conLabel)
    // la FUENTE va entre Rotacion y Align X (pedido Dante: molestaba entre ancla y alineacion)
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
    propT2dColor = new PropColor(T("Color"));   // el color al FINAL de la lista (pedido Dante)
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
    propImgTamPx = new PropBool(T("Pixels"));   // tamano en px o relativo al padre
    propImgTamPx->onChange = AccionTamPxToggle;
    propImagen2D->properties.push_back(propImgTamPx);
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
    propRectTamPx = new PropBool(T("Pixels"));
    propRectTamPx->onChange = AccionTamPxToggle;
    propRect2D->properties.push_back(propRectTamPx);
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
    propContTamPx = new PropBool(T("Pixels"));
    propContTamPx->onChange = AccionTamPxToggle;
    propCont2D->properties.push_back(propContTamPx);
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
    propS9TamPx = new PropBool(T("Pixels"));
    propS9TamPx->onChange = AccionTamPxToggle;
    propS9card->properties.push_back(propS9TamPx);
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
    propBtnOpac = new PropFloat(T("Opacity"));
    propBtnOpac->SetRango(0.0f, 1.0f);
    propBtnOpac->stepFino = 0.01f; propBtnOpac->stepGrueso = 0.1f; propBtnOpac->dragStep = 0.005f;
    propBtn2D->properties.push_back(propBtnOpac);
    // cada color puede ser PROPIO o apuntar a la PALETA del UI (un puntero, no una copia)
    propBtnPalFondo = new PropButton(T("Background"));
    propBtnPalFondo->conLabel = true;
    propBtnPalFondo->button->desplegable = true;
    propBtnPalFondo->action = AccionPalBtnFondo;
    propBtn2D->properties.push_back(propBtnPalFondo);
    propBtnColFondo = new PropColor("");
    propBtn2D->properties.push_back(propBtnColFondo);
    propBtnPalTexto = new PropButton(T("Text Color"));
    propBtnPalTexto->conLabel = true;
    propBtnPalTexto->button->desplegable = true;
    propBtnPalTexto->action = AccionPalBtnTexto;
    propBtn2D->properties.push_back(propBtnPalTexto);
    propBtnColTexto = new PropColor("");
    propBtn2D->properties.push_back(propBtnColTexto);
    propBtnPalBorde = new PropButton(T("Border Color"));
    propBtnPalBorde->conLabel = true;
    propBtnPalBorde->button->desplegable = true;
    propBtnPalBorde->action = AccionPalBtnBorde;
    propBtn2D->properties.push_back(propBtnPalBorde);
    propBtnColBorde = new PropColor("");
    propBtn2D->properties.push_back(propBtnColBorde);
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

    // ===== Tarjeta "Paleta" (del UI): colores con NOMBRE que los componentes referencian.
    // Las filas se reconstruyen en el rebind cuando cambia la cantidad (RefreshPaleta).
    propPaleta = new GroupPropertie(T("Palette"));
    GroupProperties.push_back(propPaleta);

    // ===== Tarjeta "Children": afecta a los HIJOS del seleccionado. El padding encoge el
    // area donde se enganchan las anclas de bordes/esquinas (la linea transparente se ve en
    // el Editor 2D cuando el UI esta seleccionado). =====
    propHijos = new GroupPropertie(T("Children"));
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
    GroupProperties.push_back(propHijos);

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

    // ===== Tarjeta APARTE "Material". Orden (pedido Dante): New Material + Rename Material (las opciones
    // del material), LINEA separadora, y abajo la textura + sus opciones. Los props se guardan en arrays
    // (propMatChk/propMatCol/propMatShin) -> Rebind los setea por nombre, NO por indice (reordenar = libre).
    propMaterial = new GroupPropertie("Material");
    propMaterial->anchoValores = 0.30f;
    propBtnNewMaterial = new PropButton(T("New Material"), IconType::material);
    propBtnNewMaterial->button->desplegable = true;
    propBtnNewMaterial->action = AccionMenuMateriales;
    propMaterial->properties.push_back(propBtnNewMaterial);
    propBtnRenameMat = new PropButton(T("Rename Material"), -1); // ANTES de la textura, SIN icono (pedido Dante)
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
    // orden VISUAL, que se reorganizo (pedido Dante): Lighting arriba de todo, Vertex Color sobre Base Color, y los
    // "pro" (Culling / Depth Test) abajo de todo.
    const char* nombresCol[3] = { "Base Color","Specular","Emission" };
    for (int i = 0; i < 3; i++) propMatCol[i] = new PropColor(nombresCol[i]);
    propMatShin = new PropFloat(T("Shininess"));
    propMatShin->SetRango(0.0f, 255.0f);
    propMatShin->stepFino = 1.0f; propMatShin->stepGrueso = 10.0f; propMatShin->dragStep = 1.0f;
    propMatShin->entero = true;   // es un entero (Dante: "que sea entero"), no float
    propMatShin->acelera = true;  // izq/der arranca en 1 y acelera (Dante: "empieza lento y despues acelera")
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
    // --- ORDEN VISUAL (pedido Dante) ---
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
    propMaterial->properties.push_back(propMatChk[5]);  // Culling    (ABAJO DE TODO: pro)
    propMaterial->properties.push_back(propMatChk[6]);  // Depth Test (ABAJO DE TODO: pro)
    GroupProperties.push_back(propMaterial);

    // pestania de LUZ: TODAS las propiedades editables de la luz de OpenGL (pedido Dante). Se ve solo si el
    // objeto activo es una luz. OpenGL = UN tipo de luz configurable: Direccional / Puntual / Spot (ver Light.h).
    propLight = new GroupPropertie(T("Light"));
    propLightDir = new PropBool(T("Directional"));                 // w=0 (sol) vs puntual/spot
    propLight->properties.push_back(propLightDir);
    propLightGL = new PropFloat(T("GL Light"));                    // numero de GL light (0..7), entero editable
    propLightGL->SetRango(0.0f, 7.0f); propLightGL->entero = true;
    propLightGL->stepFino = 1.0f; propLightGL->stepGrueso = 1.0f; propLightGL->dragStep = 1.0f;
    propLightGL->onChange = OnLightGLChange;
    propLight->properties.push_back(propLightGL);
    propLightDiffuse = new PropColor(T("Diffuse"));  propLight->properties.push_back(propLightDiffuse);
    propLightAmbient = new PropColor(T("Ambient"));  propLight->properties.push_back(propLightAmbient);
    propLightSpecular = new PropColor(T("Specular")); propLight->properties.push_back(propLightSpecular);
    // atenuacion 1/(C + L*d + Q*d^2) (afecta a la puntual/spot)
    propLightAttC = new PropFloat(T("Att Constant")); propLightAttC->SetRango(0.0f, 5.0f);
    propLightAttC->stepFino = 0.02f; propLightAttC->stepGrueso = 0.1f; propLightAttC->dragStep = 0.01f;
    propLight->properties.push_back(propLightAttC);
    propLightAttL = new PropFloat(T("Att Linear")); propLightAttL->SetRango(0.0f, 2.0f);
    propLightAttL->stepFino = 0.01f; propLightAttL->stepGrueso = 0.05f; propLightAttL->dragStep = 0.005f;
    propLight->properties.push_back(propLightAttL);
    propLightAttQ = new PropFloat(T("Att Quadratic")); propLightAttQ->SetRango(0.0f, 1.0f);
    propLightAttQ->stepFino = 0.005f; propLightAttQ->stepGrueso = 0.02f; propLightAttQ->dragStep = 0.002f;
    propLight->properties.push_back(propLightAttQ);
    // spotlight: cono (grados) + concentracion del haz
    propLightSpotCut = new PropFloat(T("Spot Cutoff")); propLightSpotCut->SetRango(1.0f, 180.0f); propLightSpotCut->entero = true;
    propLightSpotCut->stepFino = 1.0f; propLightSpotCut->stepGrueso = 5.0f; propLightSpotCut->dragStep = 1.0f;
    propLight->properties.push_back(propLightSpotCut);
    propLightSpotExp = new PropFloat(T("Spot Exponent")); propLightSpotExp->SetRango(0.0f, 128.0f); propLightSpotExp->entero = true;
    propLightSpotExp->stepFino = 1.0f; propLightSpotExp->stepGrueso = 8.0f; propLightSpotExp->dragStep = 1.0f;
    propLight->properties.push_back(propLightSpotExp);
    GroupProperties.push_back(propLight);

    // pestania de CAMARA: el target (look-at)
    propCamera = new GroupPropertie(T("Camera"));
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

    // ===== pestania RENDER: tarjeta "Render" (arriba) + tarjeta "Export" (abajo) =====
    propRender = new GroupPropertie("Render");
    propRender->anchoValores = 0.62f; // columna de valor ANCHA (paths)
    // salida partida en dos campos: Path (carpeta) + File name (nombre.png), como pidio Dante.
    // Default del path: carpeta de salida por defecto (Android = Descargas).
    propRenderPath = new PropText(T("Path"), w3dFileSystem::GetDefaultOutputDir());
    propRenderPath->onClick = AccionBrowseRender; // el campo Path ES el "Browse folder": al clickear abre el explorador
    propRender->properties.push_back(propRenderPath);
    propRenderOutput = new PropText(T("File name"), "render.png");
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
    PropButton* pbRenderImg = new PropButton(T("Render Image"), IconType::textura); // icono de textura (imagen)
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
    // New | Delete en UNA fila
    propRowAnimNewDel = new PropButtonRow();
    propRowAnimNewDel->Agregar(T("New"), AccionAnimNewCard, IconType::armature);
    propRowAnimNewDel->Agregar(T("Duplicate"), AccionAnimDupCard); // duplica el clip activo (se oculta sin clips)
    propRowAnimNewDel->Agregar(T("Delete"), AccionAnimDelCard);
    propAnimation->properties.push_back(propRowAnimNewDel);
    // Rename de la animacion activa (escena o clip): el boton se vuelve input in-place
    propBtnAnimRename = new PropButton(T("Rename"), -1);
    propBtnAnimRename->action = AccionAnimRenameCard;
    propAnimation->properties.push_back(propBtnAnimRename);
    propBtnAnimRender = new PropButton(T("Render Animation"), IconType::foto);
    propBtnAnimRender->action = AccionRenderAnimation;
    propAnimation->properties.push_back(propBtnAnimRender);
    GroupProperties.push_back(propAnimation);

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
    PropButton* pbExp = new PropButton(T("Export"), IconType::mesh);
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

    { PropButton* pbSave = new PropButton(T("Save Changes"), IconType::archive);
      pbSave->action = AccionGuardarConfig;
      propAjustes->properties.push_back(pbSave); }

    GroupProperties.push_back(propAjustes);   // ULTIMA -> queda abajo de todo

    // pestaña VERTICES (icono mesh): card "Transform" (posicion X/Y/Z del centro de la seleccion, editable ->
    // traslada rigido) ARRIBA + 3 tarjetas (UV/Color/Anim). Solo en Edit Mode con algo seleccionado.
    propEditItem = new GroupPropertie(T("Transform"));
    { PropFloat* px = new PropFloat("X"); px->value = &editPosX; px->onChange = AccionEditPos; propEditItem->properties.push_back(px);
      PropFloat* py = new PropFloat("Y"); py->value = &editPosY; py->onChange = AccionEditPos; propEditItem->properties.push_back(py);
      PropFloat* pz = new PropFloat("Z"); pz->value = &editPosZ; pz->onChange = AccionEditPos; propEditItem->properties.push_back(pz); }
    GroupProperties.push_back(propEditItem);

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
    propRowGroupOps = new PropButtonRow();
    propRowGroupOps->Agregar(T("Delete"),    AccionVertDelGroup);
    propRowGroupOps->Agregar(T("Move Up"),   AccionVertGroupUp);
    propRowGroupOps->Agregar(T("Move Down"), AccionVertGroupDown);
    propVertexGroups->properties.push_back(propRowGroupOps);
    GroupProperties.push_back(propVertexGroups);

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

    // ===== pestania ARMATURE: tarjeta "Bones" (Pose Mode): lista de huesos + parent + pos/rot/scale del hueso activo =====
    propArmBones = new GroupPropertie(T("Bones"));
    propListBones = new PropListMeshParts("Bones"); propListBones->modo = 6; // lee arm->bones (mismo componente de lista)
    propArmBones->properties.push_back(propListBones);
    propBoneParent = new PropText(T("Parent"), ""); propBoneParent->onClick = BoneParentNoop; // solo lectura (onClick != NULL)
    propArmBones->properties.push_back(propBoneParent);
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

    propVertexAnim = new GroupPropertie(T("Vertex Animation"));
    propVertexAnim->properties.push_back(new PropLabel("(coming soon)")); // placeholder
    GroupProperties.push_back(propVertexAnim);

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
    // Apply Modifier (cualquier modificador): hornea la malla generada en la editable
    propBtnApplyMod = new PropButton(T("Apply Modifier"));
    propBtnApplyMod->action = AccionAplicarModificador;
    propModifierProps->properties.push_back(propBtnApplyMod);
    GroupProperties.push_back(propModifierProps);
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

    // Tarjeta "Material" (propMaterial): [0] New Material, [1..3] aviso default, [4] textura,
    // [5] Filtering, [6..12] checkboxes, [13..15] colores, [16] Shininess. El material POR DEFECTO
    // no se edita: se ocultan sus filas (value=NULL) y se muestra el aviso.
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
    propMatChk[6]->value = esDefault ? NULL : &material->depth_test;
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
    return;

    // (codigo viejo inalcanzable abajo: se poda en la proxima pasada)
    MaterialGroup& mg = mesh->materialsGroup[0];
    if (!mg.material) return;
    Material* material = mg.material;

    static_cast<PropBool*>(propMeshParts->properties[1])->value = &material->transparent;
    static_cast<PropBool*>(propMeshParts->properties[2])->value = &material->transparent;
    static_cast<PropBool*>(propMeshParts->properties[3])->value = &material->vertexColor;
    static_cast<PropBool*>(propMeshParts->properties[4])->value = &material->lighting;
    static_cast<PropBool*>(propMeshParts->properties[5])->value = &material->repeat;
    static_cast<PropBool*>(propMeshParts->properties[6])->value = &material->culling;
    static_cast<PropBool*>(propMeshParts->properties[7])->value = &material->depth_test;

    static_cast<PropColor*>(propMeshParts->properties[8])->value = material->diffuse;
    static_cast<PropColor*>(propMeshParts->properties[9])->value = material->specular;
    static_cast<PropColor*>(propMeshParts->properties[10])->value = material->emission;

    /*GLfloat diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat specular[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};*/

    /*
    bool culling = true;
    bool uv8bit = false;
    int interpolacion = 0;
    Texture* texture = NULL;
    GLfloat diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat specular[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    std::string name = "";*/
};

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
        // las propiedades quedaban "trabadas" (el color seguia activo). Pedido Dante.
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
    SincronizarTextoBoton(this);   // y el del boton 2D
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
        pw->name = "Rotation W"; pw->value = &ObjActivo->rot.w; pw->unit = "";
        px->name = "X"; px->value = &ObjActivo->rot.x; px->unit = "";
        py->value = &ObjActivo->rot.y; py->unit = "";
        pz->value = &ObjActivo->rot.z; pz->unit = "";
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

    //Mesh Parts
    RefreshPropMeshParts();

    // TEXTO 2D: bindea la tarjeta (NULL/labels segun el activo sea o no un Texto2D)
    if (propTexto2D && propT2dTam){
        Texto2D* t = (ObjActivo && ObjActivo->getType() == ObjectType::texto2d) ? (Texto2D*)ObjActivo : NULL;
        propT2dTam->value   = t ? &t->tam   : NULL;
        propT2dColor->value = (t && t->palColor < 0) ? t->color : NULL;
        if (propT2dPal && t) propT2dPal->button->text = PalNombre(PaletaUIDe(t), t->palColor);
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
        if (propImgTamPx) propImgTamPx->value = im ? &im->tamPx : NULL;
        if (propImgColor) propImgColor->value = (im && im->palTinte < 0) ? im->color : NULL;
        if (propImgPal && im) propImgPal->button->text = PalNombre(PaletaUIDe(im), im->palTinte);
        if (propImgAlpha) propImgAlpha->value = im ? &im->usarAlpha : NULL;
        if (propImgFiltro) propImgFiltro->value = im ? &im->filtrado : NULL;
        AjustarFilaTam(propImgAncho, !im || im->tamPx);
        AjustarFilaTam(propImgAlto,  !im || im->tamPx);
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
        if (propRectColor) propRectColor->value = (r && r->palColor < 0) ? r->color : NULL;
        if (propRectPal && r) propRectPal->button->text = PalNombre(PaletaUIDe(r), r->palColor);
        if (propRectTamPx) propRectTamPx->value = r ? &r->tamPx : NULL;
        AjustarFilaTam(propRectAncho, !r || r->tamPx);
        AjustarFilaTam(propRectAlto,  !r || r->tamPx);
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
        if (propContTamPx) propContTamPx->value = c ? &c->tamPx : NULL;
        AjustarFilaTam(propContAncho, !c || c->tamPx);
        AjustarFilaTam(propContAlto,  !c || c->tamPx);
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
        if (propS9TamPx) propS9TamPx->value = s9 ? &s9->tamPx : NULL;
        if (propS9BordeX) propS9BordeX->value = s9 ? &s9->bordeX : NULL;
        if (propS9BordeY) propS9BordeY->value = s9 ? &s9->bordeY : NULL;
        if (propS9EscBorde) propS9EscBorde->value = s9 ? &s9->escalaBorde : NULL;
        if (propS9Opac) propS9Opac->value = s9 ? &s9->opacidad : NULL;
        if (propS9Color) propS9Color->value = (s9 && s9->palTinte < 0) ? s9->color : NULL;
        if (propS9Pal && s9) propS9Pal->button->text = PalNombre(PaletaUIDe(s9), s9->palTinte);
        if (propS9Filtro) propS9Filtro->value = s9 ? &s9->filtrado : NULL;
        AjustarFilaTam(propS9Ancho, !s9 || s9->tamPx);
        AjustarFilaTam(propS9Alto,  !s9 || s9->tamPx);
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
        if (propBtnOpac) propBtnOpac->value = b ? &b->opacidad : NULL;
        if (propBtnColFondo) propBtnColFondo->value = (b && b->palFondo < 0) ? b->colorFondo : NULL;
        if (propBtnColTexto) propBtnColTexto->value = (b && b->palTexto < 0) ? b->colorTexto : NULL;
        if (propBtnColBorde) propBtnColBorde->value = (b && b->palBorde < 0) ? b->colorBorde : NULL;
        if (b){
            if (propBtnPalFondo) propBtnPalFondo->button->text = PalNombre(PaletaUIDe(b), b->palFondo);
            if (propBtnPalTexto) propBtnPalTexto->button->text = PalNombre(PaletaUIDe(b), b->palTexto);
            if (propBtnPalBorde) propBtnPalBorde->button->text = PalNombre(PaletaUIDe(b), b->palBorde);
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
    // EXPANDIR
    if (propExp2D && propExpPeso){
        Expandir2D* ex = (ObjActivo && ObjActivo->getType() == ObjectType::expandir2d) ? (Expandir2D*)ObjActivo : NULL;
        propExpPeso->value = ex ? &ex->peso : NULL;
    }
    // PALETA: reconstruir las filas si cambio la cantidad (o la paleta activa)
    if (propPaleta){
        UI* u = (ObjActivo && ObjActivo->getType() == ObjectType::ui) ? (UI*)ObjActivo : NULL;
        int n = u ? (int)u->Colores().size() : 0;
        int firma = u ? (n + u->paletaActiva * 1000 + (int)u->paletas.size() * 100000) : 0;
        if (firma != paletaFilas){
            for (size_t i = 0; i < propPaleta->properties.size(); i++)
                delete propPaleta->properties[i];
            propPaleta->properties.clear();
            propPaletaSel = NULL;
            if (u){
                // [0] el desplegable de paletas (elegir la activa o crear una nueva)
                propPaletaSel = new PropButton(T("Palette"));
                propPaletaSel->conLabel = true;
                propPaletaSel->button->desplegable = true;
                propPaletaSel->button->text = u->paletas.empty() ? "" : u->paletas[u->paletaActiva].nombre;
                propPaletaSel->action = AccionMenuPaletas;
                propPaleta->properties.push_back(propPaletaSel);
                // una fila COMPACTA por color: nombre + swatch + cruz de borrar
                std::vector<PaletaColor>& cs = u->Colores();
                for (int i = 0; i < n; i++){
                    PropColorPal* col = new PropColorPal(cs[i].nombre, i);
                    col->value = cs[i].rgba;   // puntero ESTABLE (colores con reserve)
                    propPaleta->properties.push_back(col);
                }
                PropButton* mas = new PropButton(T("Add Color"), IconType::material);
                mas->action = AccionPaletaAgregar;
                propPaleta->properties.push_back(mas);
            }
            paletaFilas = firma;
        }
    }
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
        propHijosPadIzq->value = pi;
        propHijosPadDer->value = pd;
        propHijosPadArr->value = pa;
        propHijosPadAba->value = pb;
        int* lay = HijosLayoutDe(ObjActivo);
        if (propHijosGap) propHijosGap->value = (lay && *lay != 0) ? HijosGapDe(ObjActivo) : NULL;
        if (propHijosLayout && lay) propHijosLayout->button->text = HijosNombreLayout(*lay);
        // Fit y Align: solo con layout activo (Align ademas solo con ajuste MINIMO)
        int* aj = HijosAjusteDe(ObjActivo);
        int* al = HijosAlignDe(ObjActivo);
        if (propHijosAjuste){
            propHijosAjuste->oculto = !(lay && *lay != 0);
            if (aj) propHijosAjuste->button->text = HijosNombreAjuste(*aj);
        }
        if (propHijosAlign){
            propHijosAlign->oculto = !(lay && *lay != 0 && aj && *aj == 1);
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
        PropFloat* pads[4] = { propHijosPadIzq, propHijosPadDer, propHijosPadArr, propHijosPadAba };
        for (int k = 0; k < 4; k++) if (pads[k]){
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
    propHijosAjuste = NULL; propHijosAlign = NULL;
    propBtn2D = NULL; propBtnNombre = NULL; propBtnPosX = NULL; propBtnPosY = NULL;
    propBtnPosZ = NULL; propBtnPosAbs = NULL; propBtnPeso = NULL; propBtnTexto = NULL;
    propBtnIcono = NULL; propBtnTam = NULL; propBtnPad = NULL; propBtnAncla = NULL;
    propBtnOpac = NULL; propBtnColFondo = NULL; propBtnColTexto = NULL; propBtnColBorde = NULL;
    propExp2D = NULL; propExpNombre = NULL; propExpPeso = NULL;
    propPaleta = NULL; paletaFilas = -1; propPaletaSel = NULL;
    propBtnPalFondo = NULL; propBtnPalTexto = NULL; propBtnPalBorde = NULL;
    propBtnTex = NULL; propBtnTexBX = NULL; propBtnTexBY = NULL; propBtnTexEsc = NULL;
    propT2dPal = NULL; propImgPal = NULL; propRectPal = NULL; propS9Pal = NULL;
    propHijosClipX = NULL; propHijosClipY = NULL; propHijosScroll = NULL;
    propHijosScrollX = NULL; propHijosScrollY = NULL;
    propImgTamPx = NULL; propImgColor = NULL; propImgAlpha = NULL;
    propRectTamPx = NULL; propContTamPx = NULL; propUIcolor = NULL;
    propS9card = NULL; propS9Nombre = NULL; propS9PosX = NULL; propS9PosY = NULL;
    propS9PosZ = NULL; propS9PosAbs = NULL; propS9Peso = NULL; propS9Textura = NULL;
    propS9Ancho = NULL; propS9Alto = NULL; propS9TamPx = NULL;
    propS9BordeX = NULL; propS9BordeY = NULL;
    propS9EscBorde = NULL; propS9Rot = NULL; propS9Ancla = NULL; propS9Opac = NULL;
    propS9Color = NULL; propImgFiltro = NULL; propS9Filtro = NULL;
    propCamera = NULL;
    propInstance = NULL;
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
    propUVMaps = NULL; propColorLayers = NULL; propVertexGroups = NULL; propVertexAnim = NULL; propModifiers = NULL;
    propListModifiers = NULL; propRowMod = NULL; propRowModMove = NULL; propModifierProps = NULL;
    propModVerViewport = NULL; propModVerEdit = NULL;
    propModVacio = NULL; propMirX = NULL; propMirY = NULL; propMirZ = NULL; propMirTarget = NULL; propArmTarget = NULL;
    propMirMerge = NULL; propMirDist = NULL; propMirClip = NULL; propBtnApplyMod = NULL;
    propSubSimple = NULL; propSubLevel = NULL; propSubRender = NULL;
    propScrewAngle = NULL; propScrewHeight = NULL; propScrewSteps = NULL; propScrewRender = NULL;
    propScrewAxis = NULL; propScrewStretchU = NULL; propScrewStretchV = NULL;
    propScrewSmooth = NULL; propScrewMerge = NULL; propScrewFlip = NULL;
    propListUV = NULL; propListColor = NULL; propListVertGroups = NULL; propBtnColorMode = NULL;
    propRowUVOps = NULL; propRowColorOps = NULL; propRowGroupOps = NULL; propBtnRenameGroup = NULL;
    propArmAnim = NULL; propListAnims = NULL; propBtnRenameAnim = NULL; propBtnDupAnim = NULL; propRowAnimOps = NULL;
    propArmBones = NULL; propListBones = NULL; propBoneParent = NULL;
    propRotMode = NULL;
    propMsgDefault = NULL; propSepMat = NULL;
    propMaterial = NULL; propBtnRenameMat = NULL;
    propBtnRenameUV = NULL; propBtnRenameColor = NULL; propNameObj = NULL;
    propRowPartOps = NULL; propRowDelRen = NULL; propRowPartMove = NULL;
    for (int i = 0; i < 10; i++) propMatChk[i] = NULL;
    for (int i = 0; i < 3; i++) propMatCol[i] = NULL;
    propMatShin = NULL;
    pestaniaActiva = 1;      // arranca en "Objeto" (transforms); 0 = Render
    exportFormat = 2;             // por defecto glTF (el formato con rig + animaciones)
    exportSelectedOnly = true;    // por defecto ON (pedido Dante): exporta solo lo seleccionado
    OnSeleccionarAnimClip = SincronizarAnimClipDesdeLista; // la lista de anims (tab Armature) sincroniza el timeline
    exportApplyModifiers = true;  // por defecto ON (como Blender)
    exportApplyTransforms = true; // por defecto ON
    exportLastObj = NULL;
    focoEnTabs = false;
    ConstruirGrupos(); // grupos PROPIOS: panel independiente
    BarCrear();
    // pestania 0: "Render" (icono MONITOR: la salida); 1: "Objeto" (transforms);
    // 2: contextual (Mesh/Light/Camera/Instance, solo segun el objeto activo)
    // El monitor y no una camara: la pestania 2 YA es una camara cuando el objeto activo es una Camera, y dos
    // camaras juntas en la misma barra no se entienden.
    Tab* tRender = new Tab("", IconType::monitor);
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
    bool esUI   = (tipo == (int)ObjectType::ui);
    bool hayTab3 = esMesh || esLuz || esCam || esInst || esArm || esT2d || esImg || esRect || esCont || esS9 || esBtn || esExp || esUI;

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
        else if (esUI)   icono = (int)IconType::textura;         // la raiz de la interfaz
        BarTabs[2]->icon = icono;
    }
    // los objetos 2D no muestran el tab Objeto (su Nombre y Posicion viven arriba de su
    // tarjeta contextual, que era lo unico que se usaba de ahi)
    bool es2D = esT2d || esImg || esRect || esCont || esS9 || esBtn || esExp || esUI;
    if (BarTabs.size() >= 2) BarTabs[1]->visible = !es2D;
    if (pestaniaActiva == 1 && es2D) pestaniaActiva = 2;
    if (BarTabs.size() >= 4) BarTabs[3]->visible = esMesh; // pestaña Vertices: SOLO meshes
    if (BarTabs.size() >= 5) BarTabs[4]->visible = esMesh; // pestaña Modifiers: SOLO meshes
    if (pestaniaActiva == 2 && !hayTab3) pestaniaActiva = 1;
    if (pestaniaActiva == 3 && !esMesh)  pestaniaActiva = 1;
    if (pestaniaActiva == 4 && !esMesh)  pestaniaActiva = 1;
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
      if (propExportName && expObj && expObj != exportLastObj){
          propExportName->field.SetText(expObj->name + ExtDeFormato(exportFormat));
          exportLastObj = expObj;
      }
    }

    // mostrar SOLO los grupos de la pestania activa. Render (0) es GLOBAL (ajustes de salida/pases): siempre
    // visible con la pestania activa, con o sin seleccion. El export OBJ SI depende de la seleccion (sin objeto
    // no hay nada que exportar).
    if (propRender)    propRender->visible    = (pestaniaActiva == 0);
    if (propAnimation) propAnimation->visible = (pestaniaActiva == 0); // tarjeta Animation: global, como Render
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
            propBtnAnimSel->button->icon = clipActivo ? (int)IconType::armature : (int)IconType::camera;
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
    }
    // el objeto UI NO tiene transformacion: es el ORDEN DE DIBUJO (la interfaz se dibuja al
    // final, sobre la escena). No se mueve, ni rota, ni escala: su tarjeta no aplica.
    if (propTransform) propTransform->visible = (pestaniaActiva == 1 && !es2D);
    if (propTexto2D)   propTexto2D->visible   = (pestaniaActiva == 2 && esT2d);
    if (propImagen2D)  propImagen2D->visible  = (pestaniaActiva == 2 && esImg);
    if (propRect2D)    propRect2D->visible    = (pestaniaActiva == 2 && esRect);
    if (propCont2D)    propCont2D->visible    = (pestaniaActiva == 2 && esCont);
    if (propS9card)    propS9card->visible    = (pestaniaActiva == 2 && esS9);
    if (propBtn2D)     propBtn2D->visible     = (pestaniaActiva == 2 && esBtn);
    if (propExp2D)     propExp2D->visible     = (pestaniaActiva == 2 && esExp);
    if (propUIcard)    propUIcard->visible    = (pestaniaActiva == 2 && esUI);
    if (propPaleta)    propPaleta->visible    = (pestaniaActiva == 2 && esUI);
    if (propHijos)     propHijos->visible     = (pestaniaActiva == 2 && es2D);
    if (propMeshParts) propMeshParts->visible = (pestaniaActiva == 2 && esMesh);
    if (propMaterial)  propMaterial->visible  = (pestaniaActiva == 2 && esMesh);
    if (propLight)     propLight->visible     = (pestaniaActiva == 2 && esLuz);
    if (propCamera)    propCamera->visible    = (pestaniaActiva == 2 && esCam);
    if (propInstance)  propInstance->visible  = (pestaniaActiva == 2 && esInst);
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
    // tarjeta "Bones" (Pose Mode): lista de huesos + parent + transform del hueso activo
    if (propArmBones) propArmBones->visible = armTab;
    if (armTab) {
        Armature* a = (Armature*)ObjActivo;
        if (propListBones) propListBones->arm = a;
        // sincronizar los campos SOLO al cambiar de hueso (sino se pisaria lo que el usuario esta tipeando)
        static int lastBoneSync = -999; static Armature* lastArm = NULL;
        if (a->boneActivo != lastBoneSync || a != lastArm) {
            SincronizarCamposBone();
            lastBoneSync = a->boneActivo; lastArm = a;
            if (propBoneParent) {
                W3dBone* b = BoneActivoUI();
                std::string pn = (b && b->parent >= 0 && b->parent < (int)a->bones.size()) ? a->bones[b->parent].name : std::string("-");
                propBoneParent->field.SetText(pn);
            }
        }
    }
    bool vertTab = (pestaniaActiva == 3 && esMesh);
    // card "Transform" (X/Y/Z de la seleccion): solo en Edit Mode con algo seleccionado. Recalcula el centro LOCAL
    // cada frame y lo mapea a los campos con la convencion Z-up del panel (campo Y = local z, campo Z = local y).
    if (propEditItem) {
        bool haySel = false; float cx=0,cy=0,cz=0;
        if (vertTab && InteractionMode == EditMode && g_editMesh) {
            Mesh* em = (Mesh*)g_editMesh; em->EnsureEdit();
            if (em->edit) haySel = em->edit->CentroSeleccion(cx, cy, cz);
        }
        propEditItem->visible = haySel;
        if (haySel) { editPosX = cx; editPosY = cz; editPosZ = cy; }
    }
    if (propUVMaps)      propUVMaps->visible      = vertTab;
    if (propColorLayers) propColorLayers->visible = vertTab;
    if (propVertexGroups) propVertexGroups->visible = vertTab;
    if (propVertexAnim)  propVertexAnim->visible  = vertTab;
    // pestaña Modifiers: card del stack + una 2da card con las props del modificador seleccionado (vacia).
    bool modsTab = (pestaniaActiva == 4 && esMesh);
    if (propModifiers) propModifiers->visible = modsTab;
    if (modsTab) {
        Mesh* mm = (Mesh*)ObjActivo;
        if (propListModifiers) propListModifiers->mesh = mm;   // el selector sigue a la malla activa
        int nm = (int)mm->modificadores.size();
        if (propRowMod && propRowMod->botones.size() >= 2)
            propRowMod->botones[1]->visible = (nm >= 1);        // Remove: solo si hay 1+ (Dante)
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
        if (propModifierProps) {
            propModifierProps->visible = haySel;               // 2da tarjeta: solo con un modificador seleccionado
            if (mod) propModifierProps->name = mod->nombre;    // titulo = su nombre
        }
        // props del MIRROR: bindeadas al modificador activo (value=NULL las OCULTA -> solo se ven en un Mirror).
        // display toggles: para CUALQUIER modificador seleccionado (no solo Mirror)
        if (propModVerViewport) propModVerViewport->value = haySel ? &mod->mostrarViewport : NULL;
        if (propModVerEdit)     propModVerEdit->value     = haySel ? &mod->mostrarEdit : NULL;
        if (propModVacio) propModVacio->oculto = (esMirror || esSub || esScrew || (mod && mod->tipo==ModifierType::Armature)); // "(no properties yet)" solo tipos sin params
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
        if (propBtnApplyMod) propBtnApplyMod->oculto = !haySel; // Apply: con cualquier modificador seleccionado
    } else if (propModifierProps) propModifierProps->visible = false;

    // pestaña Vertices activa: las listas siguen a la malla activa (modo 1=uvmaps, 2=colors) + el toggle
    if (vertTab) {
        Mesh* mv = (Mesh*)ObjActivo;
        if (mv->uvMaps.empty() || mv->colorLayers.empty()) mv->PoblarCapas(); // crea la 1ra si falta
        if (propListUV)    propListUV->mesh    = mv;
        if (propListColor) propListColor->mesh = mv;
        if (propListVertGroups) propListVertGroups->mesh = mv; // grupos de vertices (huesos del rig)
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

        if (ObjActivo) DibujarTitulo(ObjActivo, maxPixelsTitle); // sin seleccion (pestania Render global): sin titulo de objeto

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
// desde la ultima opcion -> pestañas). Pedido Dante.
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
        // si la fila seleccionada es una FILA DE BOTONES, mover entre ellos (NO colapsar la tarjeta)
        PropertieBase* p = PropFilaSeleccionada(GroupProperties, selectIndex);
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
    SetRectFilaSeleccionada(); // desplegable alineado al botón / pos de la fila (nav por teclado)
    editando = GroupProperties[selectIndex]->EnterPropertieSelect();
    ViewPortClickDown = editando;
    // OK/Enter sobre un COLOR: abrir el ColorPicker (igual que el click del mouse) -> sin esto el
    // selector de color SOLO se podia abrir con el mouse (Dante). El picker es modal: se lleva el teclado.
    GroupPropertie* g = GroupProperties[selectIndex];
    if (g->selectIndex >= 0 && g->selectIndex < (int)g->properties.size() &&
        g->properties[g->selectIndex]->GetType() == PropertyType::Color) {
        PropColor* pc = (PropColor*)g->properties[g->selectIndex];
        if (pc->value) {
            for (int q = 0; q < 4; q++) pc->originalValue[q] = pc->value[q]; // para Cancel
            pc->editando = true;
            if (!colorPicker) colorPicker = new ColorPicker();
            colorPicker->Abrir(pc->value, gColorSelSx, gColorSelSy);
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
                        prop->EditPropertie(); // checkbox: toggle directo (+ su onChange: los de material re-Rebindean)
                    }
                    else if (prop->GetType() == PropertyType::Color) {
                        PropColor* pc = (PropColor*)prop;
                        // fila de PALETA: la crucecita a la izquierda del swatch BORRA la entrada
                        int pidx = pc->PaletaIdx();
                        if (pidx >= 0) {
                            int cw = RenglonHeightGS + GlobalScale * 2;
                            int xCruz = x + PosX + borderGS * 2 + g->width - bordersGS
                                        - cw * 2 - gapGS;
                            if (mx >= xCruz && mx < xCruz + cw) {
                                UI* u2 = (ObjActivo && ObjActivo->getType() == ObjectType::ui) ? (UI*)ObjActivo : NULL;
                                if (u2 && pidx < (int)u2->Colores().size()) {
                                    u2->Colores().erase(u2->Colores().begin() + pidx);
                                    target = NULL;   // re-bind: la tarjeta se reconstruye
                                    g_redraw = true;
                                }
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