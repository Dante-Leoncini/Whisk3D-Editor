GLshort IconMesh[8] = { 0,0, 10,0, 0,10, 10,10 };
int IconSize = 10;

size_t ICON_TOTAL = 6;
enum class IconType {
	camera,
	light,
	mesh,
	visible,
	hidden,
	archive
};

struct IconRect {
    int x, y, w, h;
};

class IconUV {
    public:
        GLfloat uvs[8];
};

// ===================================================
//  Vector global de íconos UV
// ===================================================
std::vector<IconUV*> IconsUV;

void SetIconScale(int scale){
    IconSize = 10 * scale;
    IconMesh[2] = IconMesh[5] = IconMesh[6] = IconMesh[7] = 10 * scale;
}

void CrearIconos(int texW, int texH){
    // lista de posiciones dentro del atlas
    IconRect lista[ICON_TOTAL] = {
        {  1, 117, 10, 10 }, // camera
        { 13, 117, 10, 10 }, // light
        { 24, 117, 10, 10 }, // mesh
        { 36, 117, 10, 10 }, // visible
        { 48, 117, 10, 10 }, // hidden
        { 60, 117, 10, 10 }  // archive
    };

    // limpia por si se llama más de una vez
    IconsUV.clear();

    for (size_t i = 0; i < ICON_TOTAL; ++i) {
        IconUV* NewIcon = new IconUV();

        GLfloat u1 = (GLfloat)lista[i].x / texW;
        GLfloat u2 = (GLfloat)(lista[i].x + lista[i].w) / texW;
        GLfloat v1 = (GLfloat)lista[i].y / texH;
        GLfloat v2 = (GLfloat)(lista[i].y + lista[i].h) / texH;

        NewIcon->uvs[0] = u1; NewIcon->uvs[1] = v1; // top-left
        NewIcon->uvs[2] = u2; NewIcon->uvs[3] = v1; // top-right
        NewIcon->uvs[4] = u1; NewIcon->uvs[5] = v2; // bottom-left
        NewIcon->uvs[6] = u2; NewIcon->uvs[7] = v2; // bottom-right

        IconsUV.push_back(NewIcon);
    }
}