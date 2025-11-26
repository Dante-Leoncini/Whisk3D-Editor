/*
 * ==============================================================================
 *  Name        : Whisk3DDocument.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDE FILES
#include "Whisk3DDocument.h"
#include "Whisk3DAppUi.h"

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

// End of File
