#ifndef EXPORT_UV_POPUP_H
#define EXPORT_UV_POPUP_H
// ============================================================================
//  ExportUVPopup.h - "Export UV Layout" del editor UV: nombre, carpeta y tamano del PNG con el mapa UV
//  (io/UVExport.h). La carpeta se elige con el explorador (al volver, el formulario se reabre con lo tipeado).
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
class PropText; class PropButtonRow;

class ExportUVPopup : public PopUpBase {
    public:
        ExportUVPopup();
        ~ExportUVPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        bool Dragging() W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        GroupPropertie* group;
        PropText* pName; PropText* pFolder; PropFloat* pWidth; PropFloat* pHeight; PropButtonRow* pButtons;
        PropFloat* dragField; bool dragMoved; int lastDragMx;
        void ResizeGroup();
        void Relocate();
        int  RowY(int idx) const;
};
// abre el formulario con los valores por defecto (textura mostrada / 256 en el N95 / 512)
void OpenExportUVPopup();
// (harness) exporta con los valores actuales del formulario, sin confirmar sobrescritura
bool ExportUVNow(std::string& pathOut, std::string& msg);
#endif // EXPORT_UV_POPUP_H
