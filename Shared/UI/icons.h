class IconUV {
    public:
        struct Icon {
            GLfloat uvs[8]; // cada ícono tiene sus coordenadas UV
        };

        std::map<std::string, Icon> atlas; // clave: "camera", "light", "mesh"

        IconUV(int texW, int texH) {
            // formato: addIcon(nombre, texW, texH, x, y, w, h)
            addIcon("camera",  texW, texH, 1, 117, 10, 10);
            addIcon("light",   texW, texH, 13, 117, 10, 10);
            addIcon("mesh",    texW, texH, 24, 117, 10, 10);
            addIcon("visible", texW, texH, 36, 117, 10, 10);
            addIcon("hidden",  texW, texH, 48, 117, 10, 10);
            addIcon("archive",  texW, texH, 60, 117, 10, 10);
        }

        const GLfloat* getUV(const std::string& name) const {
            auto it = atlas.find(name);
            if (it != atlas.end())
                return it->second.uvs;
            return nullptr;
        }

    private:
        void addIcon(const std::string& name, int texW, int texH, int x, int y, int w, int h) {
            GLfloat u1 = (GLfloat)x / texW;
            GLfloat u2 = (GLfloat)(x + w) / texW;
            GLfloat v1 = (GLfloat)y / texH;
            GLfloat v2 = (GLfloat)(y + h) / texH;

            Icon icon;
            icon.uvs[0] = u1; icon.uvs[1] = v1; // top-left
            icon.uvs[2] = u2; icon.uvs[3] = v1; // top-right
            icon.uvs[4] = u1; icon.uvs[5] = v2; // bottom-left
            icon.uvs[6] = u2; icon.uvs[7] = v2; // bottom-right

            atlas[name] = icon;
        }
};

IconUV icons(128, 128);