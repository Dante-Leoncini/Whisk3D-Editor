/*
 * ==============================================================================
 *  Name        : Whisk3D.h
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

#ifndef WHISK3D_H
#define WHISK3D_H

// INCLUDES
#include <e32base.h> // for CBase definition
#include <GLES/gl.h> // OpenGL ES header file
#include "Whisk3Dinput.h"
#include "hidmonitor.h" // mouse/teclado bluetooth (MHidObserver)

//para el cuadro de wait

//acelerometro
/*#include <SensrvChannel.h>
#include <SensrvChannelFinder.h>
#include <SensrvChannelInfo.h>
#include <SensrvListener.h>
#include <SensrvChannel.h>
#include <SensrvAccelerometerAxisData.h>*/


// MACROS
#define TEX_WIDTH     256 // Texture size must be power of two and max size
#define TEX_HEIGHT    256 // for textures is 256 x 256

#define FRUSTUM_LEFT   -1.f     //left vertical clipping plane
#define FRUSTUM_RIGHT   1.f     //right vertical clipping plane
#define FRUSTUM_BOTTOM -1.f     //bottom horizontal clipping plane
#define FRUSTUM_TOP     1.f     //top horizontal clipping plane
#define FRUSTUM_NEAR    3.f     //near depth clipping plane
#define FRUSTUM_FAR  1000.f     //far depth clipping plane

// (borrado: camara vieja sin uso)

// CLASS DECLARATION
class Mesh;

/**
 * Class that does the actual OpenGL ES rendering.
 */
class CWhisk3D : public MHidObserver
    {
    public:  // Constructors and destructor

        /**
         * Factory method for creating a new CSimpleCube object.
         * @param iInputHandler Input handler that maps keycodes to inputs and stores the current state for each key.
         */
        static CWhisk3D* NewL( TUint aWidth, TUint aHeight, CWhisk3DInput* aInputHandler);

        /**
         * Destructor. Does nothing.
         */
        virtual ~CWhisk3D();
            
    public:  // New functions
        
        //RArray<Material> MaterialesOld;
        /**
		* CallbackIncrementProgressNoteL
		* Callback function to increment progress note
		* @param aThis
		* @return TInt Return 0 when work is done, otherwise return 1.
		*/

        /**
         * Initializes OpenGL ES, sets the vertex and color
         * arrays and pointers. Also selects the shading mode.
         */
        void AppInit( void );

        /**
         * Called upon application exit. Does nothing.
         */

        /**
         * Renders one frame.
         * @param aFrame Number of the frame to be rendered.
         */

    	/**
    	 * Called when the finite state machine enters a new state.
    	 * Does nothing in this implementation.
    	 * @param aState State that is about to be entered.
    	 */

        // --- MHidObserver: eventos del mouse/teclado bluetooth ---
        // mover = cursor virtual; arrastrar con boton del medio = orbitar la
        // camara; rueda = zoom. Llegan desde CHidMonitor (container).
        void HidMouseMove(TInt aDx, TInt aDy);
        void HidMouseButton(TInt aButton, TBool aDown);
        void HidMouseWheel(TInt aDelta);
        void HidKey(TInt aScanCode, TBool aDown);
        // transformar (G/R/S) siguiendo el mouse, proyectado al plano de la
        // camara; con eje bloqueado maneja ese eje (ver Whisk3D.cpp)

        // --- Fase 1 unificacion (ver PLAN-UNIFICACION.md) ---
        // renderiza la escena confinada al rectangulo de un viewport del
        // layout compartido (x/y en coordenadas arriba-izquierda).
        // aActivo: solo el viewport activo guarda su rect para el picking.
        // dibuja el cursor del mouse encima de todo (autocontenido: setea
        // su propio viewport/proyeccion). Es lo unico que sobrevive de la
        // UI 2D vieja de Symbian.
        void DrawMouseCursor();

        // --- seleccion con click izquierdo (color picking estilo Blender) ---
        // render invisible con un color por objeto + lectura del pixel del
        // click; clicks repetidos en el mismo lugar ciclan entre solapados
        void ClickSelect();
        // lapiz + OK con el mouse virtual: loop select en la posicion del cursor
        void ClickLoopSelect();

        void SetRotacion( void );
        void SetEscala( void );
        void SetPosicion( void );
        void ChangeEje ( void );

        

        //nueva forma de editar vertices

        void SetMouse();
        void TecladoNumerico(TInt numero);



        void EventKeyDown(TInt scan);



        //void ShowProgressNoteUnderSingleProcessL( TInt aResourceId, TInt aControlId );
        //TInt DialogSelectOption(const TDesC& aPrompt, CDesCArray& aOptions);
        
        //cambiar el shader en el viewport

        /**
         * Notifies that the screen size has dynamically changed during execution of
         * this program. Resets the viewport to this new size.
         * @param aWidth New width of the screen.
         * @param aHeight New height of the screen.
         */
        void SetScreenSize( TUint aWidth, TUint aHeight );

    protected: // New functions

        /**
         * Standard constructor that must never Leave.
         * Stores the given screen width and height.
         * @param aWidth Width of the screen.
         * @param aHeight Height of the screen.
         */
        CWhisk3D( TUint aWidth, TUint aHeight, CWhisk3DInput* aInputHandler );

        /**
         * Helper function that is used to make the duck 'quak'.
         * Calculates the sine for the given angleOld. Returns 0 if any
         * errors occur while calling the Math::Sin() method.
         * @param aRad Radian angleOld whose sine is to be calculated.
         * @return The sine of the given angleOld or 0 if error occured while calculating the sine.
         */

    public:  // Data
        /** para el menu de la app */
// (borrado: show* del menu Overlay S60 retirado; el estado real es del Viewport3D)
        TBool iShiftPressed; 
        TBool iAltPressed;
        TBool iCtrlPressed;

        

    private:  // Data
        //CSensrvChannel* iAccelerometerChannel;  // Canal del acelerómetro

        //CWhisk3DContainer*    iContainer;
		// (iTextureManager se retiro: las texturas de UI se cargan sincronico
		//  en AppInit con el cargador del motor)
		// (tamano de pantalla: globales COMPARTIDOS winW/winH en variables.cpp)

		/**
		 * Input handler that maps keycodes to inputs and stores the current state
		 * for each key. Owned by the C#Name#Container.
		 */
		CWhisk3DInput* iInputHandler;
    };

#endif // WHISK3D_H
