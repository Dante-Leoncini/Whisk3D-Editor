class Texture { 
	public:
    	GLuint iID = -1;
    	std::string path;
};

std::vector<Texture> Textures;

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