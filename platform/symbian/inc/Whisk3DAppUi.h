/*
 * ==============================================================================
 *  Name        : Whisk3DAppUi.h
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

#ifndef WHISK3DAPPUI_H
#define WHISK3DAPPUI_H

// INCLUDES
#include <eikapp.h>
#include <eikdoc.h>
#include <e32std.h>
#include <coeccntx.h>
#include <aknappui.h>

// FORWARD DECLARATIONS
class CWhisk3DContainer;


// CLASS DECLARATION

/**
 * Application UI class that is contains the CWhisk3DContainer
 * (as required by the Symbian UI application architecture).
 */
class CWhisk3DAppUi : public CAknAppUi
    {
    public: // Constructors and destructor

        /**
         * Second phase constructor. Creates the CWhisk3DContainer
         * and adds it to the control stack.
         */
        void ConstructL();

        /**
         * Destructor. Removes CWhisk3DContainer from the control
         * stack and destroys it.
         */
        virtual ~CWhisk3DAppUi();

    private: // Functions from base classes
        /**
         * This method is called by the EIKON framework just before it displays
         * a menu pane. Builds the dynamic menuitems for toggling the lights on and
         * off in the example.
         */
        void DynInitMenuPaneL(TInt aResourceId,CEikMenuPane* aMenuPane);
        

        /**
         * CEikAppUi method that is used to handle user commands.
         * Handles menu commands and application exit request.
         * @param aCommand Command to be handled.
         */
        void HandleCommandL(TInt aCommand);

        /**
         * CEikAppUi method that is used to handle key events.
         * Does nothing in this implemenation.
         * @param aKeyEvent Event to handled (ignored by this implementation).
         * @param aType Type of the key event (ignored by this implementation).
         * @return Response code. Always EKeyWasNotConsumed in this implementation.
         */
        virtual TKeyResponse HandleKeyEventL(
            const TKeyEvent& aKeyEvent,TEventCode aType);

    private: //Data
        /** GUI container that resides in this application UI. */
        CWhisk3DContainer* iAppContainer;
    };

#endif

// End of File
