/*
 * ==============================================================================
 *  Name        : Whisk3DDocument.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDE FILES
#include "Whisk3DDocument.h"
#include "Whisk3DAppUi.h"
#include <f32file.h>   // RFile / TFileName (abrir .w3d desde un file manager)
#include <string>

// ================= MEMBER FUNCTIONS =======================

// -----------------------------------------------------------------------------
// CWhisk3DDocument::CWhisk3DDocument
// C++ default constructor can NOT contain any code, that
// might leave.
// -----------------------------------------------------------------------------
//
CWhisk3DDocument::CWhisk3DDocument(CEikApplication& aApp)
: CAknDocument(aApp)
    {
    }

// -----------------------------------------------------------------------------
// ?classname::ConstructL
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CWhisk3DDocument::ConstructL()
    {
    }

// -----------------------------------------------------------------------------
// CWhisk3DDocument::NewL
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CWhisk3DDocument* CWhisk3DDocument::NewL(
        CEikApplication& aApp)     // CWhisk3DApp reference
    {
    CWhisk3DDocument* self = new (ELeave) CWhisk3DDocument( aApp );
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop();

    return self;
    }

// destructor
CWhisk3DDocument::~CWhisk3DDocument()
    {
    }

// ----------------------------------------------------
// CWhisk3DDocument::CreateAppUiL()
// constructs CWhisk3DAppUi
// ----------------------------------------------------
//
CEikAppUi* CWhisk3DDocument::CreateAppUiL()
    {
    return new (ELeave) CWhisk3DAppUi;
    }

// ----------------------------------------------------
// CWhisk3DDocument::OpenFileL()
// abrir un .w3d desde un file manager (X-plore "abrir con"): NO abrimos nada aca (corre durante la
// construccion de la app; montar/parsear/reconstruir el layout ahi puede reventar). Solo encolamos la
// ruta en g_proyAbrirPendiente -> el drain del DrawCallBack lo abre en el primer frame, con el stack limpio.
// ----------------------------------------------------
//
void CWhisk3DDocument::OpenFileL(CFileStore*& aFileStore, RFile& aFile)
    {
    aFileStore = NULL;   // no manejamos un CFileStore: el editor abre el .w3d por su cuenta
    TFileName nombre;
    if (aFile.FullName(nombre) == KErrNone && nombre.Length() > 0)
        {
        char buf[260];
        TInt n = nombre.Length() > 259 ? 259 : nombre.Length();
        for (TInt i = 0; i < n; i++) buf[i] = (char)nombre[i];
        buf[n] = 0;
        extern std::string g_proyAbrirPendiente;
        g_proyAbrirPendiente = buf;
        }
    }

// End of File
