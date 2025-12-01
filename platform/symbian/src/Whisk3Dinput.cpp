/*
 * ==============================================================================
 *  Name        : Whisk3Dinput.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDE FILES
#include <aknnotewrappers.h>
#include "whisk3Dinput.h"

// MACROS
/*#define FRUSTUM_LEFT   -1.f     //left vertical clipping plane
#define FRUSTUM_RIGHT  +1.f     //right vertical clipping plane
#define FRUSTUM_BOTTOM -1.f     //bottom horizontal clipping plane
#define FRUSTUM_TOP    +1.f     //top horizontal clipping plane
#define FRUSTUM_NEAR   +3.f     //near depth clipping plane
#define FRUSTUM_FAR    +1000.f  //far depth clipping plane*/

// CONSTANTS

// ============================= MEMBER FUNCTIONS ==============================
// ============================= LOCAL FUNCTIONS ===============================
// ============================= MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CWhisk3DInput::NewL
// Symbian 2-phase constructor. Can leave.
// -----------------------------------------------------------------------------
//
CWhisk3DInput* CWhisk3DInput::NewL(){
    // Symbian 2-phase constructor. Calls both the default
    // C++ constructor and Symbian ConstructL methods
    CWhisk3DInput* self = new (ELeave) CWhisk3DInput();
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop();

    return self;
}

// ----------------------------------------------------------------------------
// CWhisk3DInput::CWhisk3DInput
// C++ default constructor can NOT contain any code, that
// might leave.
// ----------------------------------------------------------------------------
//
CWhisk3DInput::CWhisk3DInput(){
}

// ----------------------------------------------------------------------------
// CWhisk3DInput::ConstructL
// Symbian 2nd phase constructor can leave.
// ----------------------------------------------------------------------------
//
void CWhisk3DInput::ConstructL( void ){
    iInputPressed = new (ELeave) TBool[ MAX_NUM_INPUTS ];
    for( int i = 0; i < MAX_NUM_INPUTS; i++ ){
        iInputPressed[i] = EFalse;
    }
}

// ----------------------------------------------------------------------------
// CWhisk3DInput::~CWhisk3DInput()
// Destructor.
// ----------------------------------------------------------------------------
//
CWhisk3DInput::~CWhisk3DInput(){
    delete iInputPressed;
}

// ----------------------------------------------------------------------------
// CWhisk3DInput::KeyDown( const int aKeyCode )
// Sets the given keycode to pressed state.
// ----------------------------------------------------------------------------
//
TKeyResponse CWhisk3DInput::KeyDown( const int aKeyCode ){
    TInt inputIdx = -1;
    for( int i = 0; i < KKeyCodeMapSize; i +=2 ){
        if( KKeyCodeMap[i] == aKeyCode ){
            inputIdx = KKeyCodeMap[i + 1];
            break;
        }
    }

    if( inputIdx >= 0 ){
        iInputPressed[ inputIdx ] = ETrue;
		return EKeyWasConsumed;
    }

    return EKeyWasNotConsumed;
}

// ----------------------------------------------------------------------------
// CWhisk3DInput::KeyUp( const int aKeyCode )
// Sets the given keycode to non-pressed state.
// ----------------------------------------------------------------------------
//
TKeyResponse CWhisk3DInput::KeyUp( const int aKeyCode ){
    TInt inputIdx = -1;
    for( int i = 0; i < KKeyCodeMapSize; i +=2 ){
        if( KKeyCodeMap[i] == aKeyCode ){
            inputIdx = KKeyCodeMap[i + 1];
            break;
            }
        }

    if( inputIdx >= 0 ){
        iInputPressed[ inputIdx ] = EFalse;
		return EKeyWasConsumed;
    }

    return EKeyWasNotConsumed;
}

// ----------------------------------------------------------------------------
// CWhisk3DInput::IsInputPressed( Input aInput )
// Returns whether or not a given input is pressed at the moment or not.
// ----------------------------------------------------------------------------
//
TBool CWhisk3DInput::IsInputPressed( TUint aInput ){
    return iInputPressed[ aInput ];
}


// End of File
