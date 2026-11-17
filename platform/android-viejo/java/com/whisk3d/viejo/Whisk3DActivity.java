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

    // EL TECLADO DEL SISTEMA: cuanto de la superficie de SDL tapa, en pixeles (W3dTeclado.cpp). La app va en pantalla
    // completa y ahi Android no achica la ventana con el teclado (ni SDL 2.0.7 sabe salir de pantalla completa): sin
    // esto el editor no se enteraba y el campo quedaba debajo del teclado. Se avisa solo cuando cambia
    private static native void nativeTecladoTapado(int px);
    private int tapadoAvisado = -1;

    @Override
    protected void onCreate(android.os.Bundle b) {
        super.onCreate(b);
        final android.view.View decor = getWindow().getDecorView();
        decor.getViewTreeObserver().addOnGlobalLayoutListener(new android.view.ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                // (SDLSurface no es publica en SDL 2.0.7: se la mira como View)
                android.view.View sup = (android.view.View) (Object) mSurface;
                if (sup == null) return;
                android.graphics.Rect r = new android.graphics.Rect();
                decor.getWindowVisibleDisplayFrame(r);
                int[] p = new int[2];
                sup.getLocationOnScreen(p);
                int h = sup.getHeight();
                int tapado = p[1] + h - r.bottom;
                if (tapado < h * 15 / 100) tapado = 0;   // (una barra del sistema no es el teclado)
                if (tapado == tapadoAvisado) return;
                try { nativeTecladoTapado(tapado); tapadoAvisado = tapado; } catch (Throwable t) { /* (sin libmain todavia) */ }
            }
        });
    }

    // CERRAR DE VERDAD (como la actividad del Android de siempre). Cuando main() termina ("Cerrar" despues de Atras) SDL
    // cierra la actividad pero el PROCESO sigue vivo con los globales de C++ del .so: al volver a abrir, main() arrancaba
    // otra vez con la escena de antes a medias (una esfera sola, sin el fondo ni el resto). Cerrada la actividad, se
    // termina el proceso (despues de que Android termine de destruirla)
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (isFinishing()) {
            new android.os.Handler(android.os.Looper.getMainLooper()).post(new Runnable() {
                @Override
                public void run() { System.exit(0); }
            });
        }
    }

    // la pantalla tiene recorte (notch)? Lo pregunta el editor para ir a pantalla completa (W3dTeclado.cpp). En la
    // epoca de Android 2.3 ninguna lo tenia
    public boolean tieneNotch() {
        return false;
    }

    // la VIBRACION (input/W3dVibrar.cpp la llama por JNI): el agarre de la biblioteca (pulsacion larga) avisa asi
    // que ya se puede arrastrar
    public void vibrar(int ms) {
        try {
            android.os.Vibrator v = (android.os.Vibrator) getSystemService(android.content.Context.VIBRATOR_SERVICE);
            if (v != null) v.vibrate(ms);
        } catch (Exception e) {}
    }
}
