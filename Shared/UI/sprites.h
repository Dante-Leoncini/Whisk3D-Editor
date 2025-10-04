class Sprite {
	public:
	    GLfloat uvs[8] = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 1.0f
		};
		GLfloat vertices[8] = { 0,0, 1,0, 0,1, 1,1 };

		void SetUV(int texW, int texH, int x, int y, int w, int h) {
			GLfloat u1 = (GLfloat)x / texW;
			GLfloat u2 = (GLfloat)(x + w) / texW;

			GLfloat v1 = (GLfloat)y / texH;
			GLfloat v2 = (GLfloat)(y + h) / texH;

			// top-left
			uvs[0] = u1; uvs[1] = v1;
			// top-right
			uvs[2] = u2; uvs[3] = v1;
			// bottom-left
			uvs[4] = u1; uvs[5] = v2;
			// bottom-right
			uvs[6] = u2; uvs[7] = v2;
		}
};

Sprite sprite;