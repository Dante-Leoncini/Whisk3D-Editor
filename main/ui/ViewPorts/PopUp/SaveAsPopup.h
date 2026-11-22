#ifndef SAVE_AS_POPUP_H
#define SAVE_AS_POPUP_H
// ============================================================================
//  SaveAsPopup.h - "Save As..." de un archivo del proyecto (un script del IDE, una textura del editor UV).
//  Nombre + "Dentro del .w3d" + carpeta:
//    - adentro: una carpeta de la BIBLIOTECA del proyecto (las mismas carpetas del outliner; lista desplegable)
//    - afuera : una carpeta del disco (se elige con el explorador; al volver el formulario se reabre con lo tipeado)
//  Quien lo abre pasa la ACCION que guarda de verdad; si el destino ya existe la accion lo avisa y el popup pide
//  confirmacion antes de pisarlo.
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include <string>
#include <vector>
class PropText; class PropBool; class PropButton; class PropButtonRow; class Card;

struct SaveAsDestination {
    std::string name;           // con la extension
    bool        internal;       // dentro del .w3d
    std::string folderProject;  // carpeta de la biblioteca ("" = la raiz)
    std::string folderDisk;     // carpeta del disco (afuera)
};
// guarda en 'd'. forzar=false y el destino ya existe (y no es el mismo archivo) -> *existe=true y no hace nada.
// msg = el error (en ingles, se traduce al mostrarlo)
typedef bool (*SaveAsAction)(const SaveAsDestination& d, bool force, bool* exists, std::string& msg);

class SaveAsPopup : public PopUpBase {
    public:
        SaveAsPopup();
        ~SaveAsPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Wheel(int delta) W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        GroupPropertie* group;
        PropText* pName; PropBool* pInternal; PropButton* pFolder; PropButtonRow* pButtons;
        bool listOpen; int listSel; int listScroll; Card* list;
        void ResizeGroup();
        void Relocate();
        int  RowY(int idx) const;
        void ListRect(int& lx, int& ly, int& lw, int& lh) const;
        int  VisibleList() const;
        void SyncFolder();
};
// ext = ".lua" / ".png" (se agrega al nombre si falta). interno = la opcion inicial.
void OpenSaveAsPopup(const std::string& title, const std::string& name, const std::string& ext, bool internal,
                           const std::string& folderProject, SaveAsAction action);
// (harness) guarda con lo que dice el formulario abierto; forzar = pisar si existe
bool SaveAsNow(bool force, std::string& msg);
bool SaveAsOpen();
// (harness) completar el formulario abierto: interno -1 = no tocar
void SaveAsSet(const std::string& name, int internal, const std::string& folderProj, const std::string& folderDisk);
#endif // SAVE_AS_POPUP_H
