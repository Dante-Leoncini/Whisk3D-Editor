package com.whisk3d;

import org.libsdl.app.SDLActivity;

// Actividad principal de Whisk3D. Extiende SDLActivity (SDL2). Antes de arrancar,
// SDL carga las librerias nativas que devuelve getLibraries(): libSDL2.so + libmain.so.
public class Whisk3DActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[]{
                "SDL2",
                "main",
        };
    }
}
