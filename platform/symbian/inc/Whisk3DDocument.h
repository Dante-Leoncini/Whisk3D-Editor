/*
 * ==============================================================================
 *  Name        : Whisk3DDocument.h
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

#ifndef WHISK3DDOCUMENT_H
#define WHISK3DDOCUMENT_H

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class  CEikAppUi;

// CLASS DECLARATION

/**
 * Document class that is just used as the container for the application
 * (as required by the Symbian UI application architecture).
 */
class CWhisk3DDocument : public CAknDocument
    {
    public: // Constructors and destructor

        /**
         * Factory method for creating a new CWhisk3DDocument object.
         */
        static CWhisk3DDocument* NewL(CEikApplication& aApp);

        /**
         * Destructor. Does nothing.
         */
        virtual ~CWhisk3DDocument();

    private:  // Functions from base classes

        /**
         * C++ constructor. Just passes the given application reference to the baseclass.
         */
        CWhisk3DDocument(CEikApplication& aApp);

        /**
         * Second phase constructor. Does nothing.
         */
        void ConstructL();

    private: // New functions

        /**
         * From CEikDocument, creates and returns CWhisk3DAppUi application UI object.
         */
        CEikAppUi* CreateAppUiL();
    };

#endif

// End of File

