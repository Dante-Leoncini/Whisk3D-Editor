/*
 * ==============================================================================
 *  Name        : Whisk3DApp.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDE FILES
#include    <eikstart.h>
#include    "Whisk3DApp.h"
#include    "Whisk3DDocument.h"

// ================= MEMBER FUNCTIONS =======================

// ---------------------------------------------------------
// CWhisk3DApp::AppDllUid()
// Returns application UID
// ---------------------------------------------------------
//
TUid CWhisk3DApp::AppDllUid() const {
    return KUidWhisk3D;
}


// ---------------------------------------------------------
// CWhisk3DApp::CreateDocumentL()
// Creates CWhisk3DDocument object
// ---------------------------------------------------------
//
CApaDocument* CWhisk3DApp::CreateDocumentL(){
    return CWhisk3DDocument::NewL( *this );
}


// ================= OTHER EXPORTED FUNCTIONS ==============


CApaApplication* NewApplication(){
    return new CWhisk3DApp;
}

TInt E32Main(){
	return EikStart::RunApplication(NewApplication);
}

// End of File

