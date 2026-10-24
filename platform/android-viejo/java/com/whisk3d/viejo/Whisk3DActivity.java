package com.whisk3d.viejo;

import org.libsdl.app.SDLActivity;

// Whisk3D en Android 2.3: la actividad de SDL 2.0.7 (el ultimo SDL que anda en la 2.3), que carga libSDL2.so y
// libmain.so (el editor) y llama a su main. Sin el pedido de permisos ni el "Acceso a todos los archivos" del Android
// de siempre: en la 2.3 los permisos se otorgan al instalar.
public class Whisk3DActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    // un .w3d que llega en el intent se abre al arrancar ("whisk3d --open"): sin tarjeta SD no hay donde buscarlo con
    // el explorador, asi que se le pasa desde la PC: adb shell am start -n ... -d file:///data/local/tmp/x.w3d
    @Override
    protected String[] getArguments() {
        android.net.Uri u = getIntent() != null ? getIntent().getData() : null;
        if (u != null && "file".equals(u.getScheme()) && u.getPath() != null)
            return new String[] { "--open", u.getPath() };
        return new String[0];
    }

    // la pantalla tiene recorte (notch)? Lo pregunta el editor para ir a pantalla completa (W3dTeclado.cpp). En la
    // epoca de Android 2.3 ninguna lo tenia
    public boolean tieneNotch() {
        return false;
    }
}
