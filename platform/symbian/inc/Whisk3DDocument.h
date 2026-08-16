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
class  CFileStore;
class  RFile;

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

        /**
         * Abrir un .w3d desde un file manager (X-plore, "abrir con"). Recibe el archivo y solo
         * ENCOLA su ruta (g_proyAbrirPendiente): el editor lo abre en el primer frame (diferido),
         * porque abrir aca -durante la construccion de la app- puede reventar. NO registramos
         * datatype (eso rompio X-plore antes): el file manager elige la app por su cuenta.
         */
        void OpenFileL(CFileStore*& aFileStore, RFile& aFile);
    };

#endif

// End of File

