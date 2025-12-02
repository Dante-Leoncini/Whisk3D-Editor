package pvallet.com.github.hello_sdl2;

import org.libsdl.app.SDLActivity;

public class HelloSDL2Activity extends SDLActivity
{
	 /**
     * This method is called by SDL before loading the native shared libraries.
     * It can be overridden to provide names of shared libraries to be loaded.
     * The default implementation returns the defaults. It never returns null.
     * An array returned by a new implementation must at least contain "SDL2".
     * Also keep in mind that the order the libraries are loaded may matter.
     *
     * @return names of shared libraries to be loaded (e.g. "SDL2", "main").
     */
    @Override
    protected String[] getLibraries() {
        return new String[]{
                "SDL2",
                "SDL2_image",
                "hello_sdl2"
        };
    }
}
