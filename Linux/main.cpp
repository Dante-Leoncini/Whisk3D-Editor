#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/gl.h>
#include <GL/glu.h>

//esto es solo para linux
#include <cmath>
#include <cfloat>
#include <map>
#include <algorithm>
#include <cstring>

#include <filesystem>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>


//Whisk3D imports
#include "../Shared/tablero.h"
#include "../Shared/Floor.h"
#include "../Shared/GeometriaUI.h"
#include "../Shared/Primitivas.h"

#include "../Shared/Animation.h"
#include "../Shared/clases.h"
#include "../Shared/variables.h"
#include "../Shared/colores.h"
#include "../Shared/OpcionesRender.h"

//estas son importaciones especificas para Whisk 3D
//Incluyen los modos objeto, edicion, animacion, escultura, etc.
//no van a ser cosas que otros proyectos necesiten (o si)
#include "../Shared/ObjectMode.h"

//Hice un importador OBJ. pero se podrian crear importadores de otros formatos
#include "../Shared/import_obj.h"
#include "../Shared/lectura-escritura.h"

#include "../Shared/funciones.h"
#include "../Shared/constructor.h"
#include "../Shared/controles.h"
#include "../Shared/render.h"

struct Config {
    bool fullscreen = false;
    int width = 800;
    int height = 600;
	int displayIndex = 0; // monitor 1
	std::string SkinName = "Whisk3D"; // monitor 1
};
Config cfg;

// Función simple para leer el ini
Config loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "No se pudo abrir " << filename << ", usando valores por defecto.\n";
        return cfg;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key, eq, value;
        if (iss >> key >> eq >> value && eq == "=") {
            if (key == "fullscreen") cfg.fullscreen = (value == "true");
            else if (key == "width") cfg.width = std::stoi(value);
            else if (key == "height") cfg.height = std::stoi(value);
            else if (key == "displayIndex") cfg.displayIndex = std::stoi(value);
            else if (key == "SkinName") cfg.SkinName = value;
        }
    }

    return cfg;
}

void Minimize(SDL_Window* window) {
    SDL_MinimizeWindow(window);
}

void Maximize(SDL_Window* window) {
    SDL_MaximizeWindow(window);
}

void Restore(SDL_Window* window) {
    SDL_RestoreWindow(window);
}

SDL_HitTestResult HitTestCallback(SDL_Window *win, const SDL_Point *area, void *data) {
    // Si el mouse está en el "header" de 30px de alto
    if (area->y < 30) {
        return SDL_HITTEST_DRAGGABLE;
    }
    return SDL_HITTEST_NORMAL;
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

	Config cfg = loadConfig("./config.ini");

	// Crear ventana con OpenGL
	window = SDL_CreateWindow("Whisk 3D",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		winW, winH,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE //| SDL_WINDOW_BORDERLESS
    );

	if (!window) {
		std::cerr << "Error SDL_CreateWindow: " << SDL_GetError() << std::endl;
		return -1;
	}

    // Forzar a maximizar al arrancar
    SDL_MaximizeWindow(window);
    
    /*if (SDL_SetWindowHitTest(window, HitTestCallback, nullptr) < 0) {
        std::cerr << "Error SetWindowHitTest: " << SDL_GetError() << std::endl;
    }*/

	SDL_GLContext context = SDL_GL_CreateContext(window);

    //constructor symbian, linux y windows
    ConstructUniversal();

	// Cargar texturas
	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/" + cfg.SkinName + "/font.png").c_str(), Textures[0].iID)) {
        std::cerr << "Error cargando font.png" << std::endl;
        return -1;
    }	
    
	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/"+cfg.SkinName+"/origen.png").c_str(), Textures[1].iID)) {
        std::cerr << "Error cargando origen.png" << std::endl;
        return -1;
    }

	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/"+cfg.SkinName+"/cursor3d.png").c_str(), Textures[2].iID)) {
        std::cerr << "Error cargando cursor3d.png" << std::endl;
        return -1;
    }

	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/"+cfg.SkinName+"/relationshipLine.png").c_str(), Textures[3].iID)) {
        std::cerr << "Error cargando relationshipLine.png" << std::endl;
        return -1;
    }

	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/"+cfg.SkinName+"/lamp.png").c_str(), Textures[4].iID)) {
        std::cerr << "Error cargando lamp.png" << std::endl;
        return -1;
    }

	SDL_Event e;

	running = true;

    while (running) {
        Contadores();
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT){running = false;}
            else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED ||
                    e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) 
                {
                    OnResize(e.window.data1, e.window.data2);
                }
            }
			else {
                InputUsuarioSDL(e);
            }			
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