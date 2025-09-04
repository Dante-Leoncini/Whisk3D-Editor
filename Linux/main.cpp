#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>

//esto es solo para linux
#include <vector>
#include <cmath>

//mallas 3d
#include "../Shared/tablero.h"
#include "../Shared/Floor.h"
#include "../Shared/GeometriaUI.h"
#include "../Shared/Primitivas.h"

#include "../Shared/Animation.h"
#include "../Shared/clases.h"
#include "../Shared/lectura-escritura.h"
#include "../Shared/variables.h"
#include "../Shared/funciones.h"
#include "../Shared/constructor.h"
#include "../Shared/controles.h"
#include "../Shared/render.h"


// --- Cargar textura ---
bool LoadTexture(const char* filename, GLuint &textureID) {
    SDL_Surface* surface = IMG_Load(filename);
    if (!surface) {
        std::cerr << "Error cargando textura: " << IMG_GetError() << std::endl;
        return false;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Configurar filtros
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Determinar formato
    GLenum format = (surface->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0,
                 format, GL_UNSIGNED_BYTE, surface->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D, format, surface->w, surface->h,
                    format, GL_UNSIGNED_BYTE, surface->pixels);

    SDL_FreeSurface(surface);
    return true;
}

int main(int argc, char* argv[]) {
	// Inicializar SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
		return -1;
	}

	// ---- CONFIGURAR ANTIALIASING ----
	// Pedir un framebuffer con multisampling
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // 4x MSAA (puedes probar 8, 16 si tu GPU soporta)

	// Crear ventana con OpenGL
	SDL_Window* window = SDL_CreateWindow("SDL2 OpenGL Cube",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		800, 600,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	if (!window) {
		std::cerr << "Error SDL_CreateWindow: " << SDL_GetError() << std::endl;
		return -1;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);

    //constructor symbian, linux y windows
    ConstructUniversal();

	//ConvertirTexData(objTexdataDado, objTexdataDadoF, 248 * 2);

	// Cargar texturas
	Textures.push_back(Texture());
    /*if (!LoadTexture("../Shared/tablero.jpg", Textures[0].iID)) {
        std::cerr << "Error cargando tablero.jpg" << std::endl;
        return -1;
    }*/
	Textures.push_back(Texture());
    if (!LoadTexture("../Shared/origen.png", Textures[1].iID)) {
        std::cerr << "Error cargando origen.png" << std::endl;
        return -1;
    }
	Textures.push_back(Texture());
    if (!LoadTexture("../Shared/cursor3d.png", Textures[2].iID)) {
        std::cerr << "Error cargando cursor3d.png" << std::endl;
        return -1;
    }
	Textures.push_back(Texture());
    if (!LoadTexture("../Shared/relationshipLine.png", Textures[3].iID)) {
        std::cerr << "Error cargando relationshipLine.png" << std::endl;
        return -1;
    }
	Textures.push_back(Texture());
    if (!LoadTexture("../Shared/lamp.png", Textures[4].iID)) {
        std::cerr << "Error cargando lamp.png" << std::endl;
        return -1;
    }

	SDL_Event e;

	running = true;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT){running = false;}
			else {InputUsuarioSDL(e);}			
        }

		// Renderizar si hay cambios
        //if (redibujar){
			//std::cout << "redibujar: " << std::boolalpha << redibujar << std::endl;
			Render();
			SDL_GL_SwapWindow(window);
		//}

        // Animación
        SDL_Delay(16); // ~60 fps
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}