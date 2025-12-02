class Texture { 
	public:
    	GLuint iID = -1;
    	std::string path;
};

std::vector<Texture> Textures;

// --- Cargar textura ---
#if SDL_MAJOR_VERSION == 2
    // --- Función Android/SDL2 usando SDL_Surface ---
    #ifdef __ANDROID__
    bool LoadTextureFromSurface(SDL_Surface* surf, GLuint &textureID) {
        if (!surf) return false;

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        GLint mode = (surf->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;

        glTexImage2D(GL_TEXTURE_2D, 0, mode, surf->w, surf->h, 0, mode, GL_UNSIGNED_BYTE, surf->pixels);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }
    #endif

    //Esto era para SDL2
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

        #ifdef __ANDROID__
            // Android GLES 1.1: no GLU, mipmaps opcionales
            glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0,
                        format, GL_UNSIGNED_BYTE, surface->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // sin mipmaps
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        #else
            // PC OpenGL: usar mipmaps con GLU
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gluBuild2DMipmaps(GL_TEXTURE_2D, format, surface->w, surface->h,
                            format, GL_UNSIGNED_BYTE, surface->pixels);
        #endif

        SDL_FreeSurface(surface);
        return true;
    }
#elif SDL_MAJOR_VERSION == 3
    //para SDL3
    bool LoadTexture(const char* path, GLuint &textureID){
        int width, height, channels;
        // Cargar la imagen en memoria (RGBA)
        stbi_uc* data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
        if (!data) {
            std::cerr << "Error cargando la textura: " << path << std::endl;
            return false;
        }

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // Subir los datos a OpenGL
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        // Parámetros de filtrado y wrap
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Liberar memoria
        stbi_image_free(data);

        return true;
    }
#endif