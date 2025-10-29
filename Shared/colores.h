enum ColorID {
	background,
	blanco,
	accent,
	accentDark,
	negro,
	gris,
	naranjaFace,
	headerColor,
	negroTransparente,
	grisUI,
	rojoEje,

	LineaPiso,
	LineaPisoRoja,
	LineaPisoVerde,
	ColorTransformX,
	ColorTransformY,
	ColorTransformZ
};

//array de colores
GLfloat ListaColores[17][4] = {
	{ 0.22, 0.28, 0.25, 1.0  },   //fondo
	{ 1.0, 1.0, 1.0, 1.0     },   //blanco
	{ 0.91, 0.50, 0.98, 1.0  },   //acento (violeta) 
	{ 0.48, 0.30, 1.0, 1.0  },   //acento oscuro	
	{ 0.0, 0.0, 0.0, 1.0     },   //negro
	{ 0.12, 0.12, 0.12, 1.0  },   //gris
	{ 0.94, 0.59, 0.17, 0.25f},   //naranja transparente
	{ 0.22, 0.22, 0.22, 1.0  },    //cabezera de la barra de herramientas
	{ 0.0, 0.0, 0.0, 0.25f     },   //negroTransparente
	{ 0.757, 0.757, 0.757, 1.0     },   //azul de los iconos seleccionados
	{ 0.757, 0.757, 0.757, 1.0     },   //azul de los iconos seleccionados

	{ 0.22, 0.22, 0.22, 1.0 },  //LineaPiso
	{ 0.56, 0.23, 0.28, 1.0 },  //LineaPisoRoja
	{ 0.38, 0.53, 0.15, 1.0 },  //LineaPisoVerde

	{ 0.88, 0.48, 0.54, 1.0 },  //ColorTransformX
	{ 0.65, 0.81, 0.38, 1.0 },  //ColorTransformY
	{ 0.46, 0.67, 0.89, 1.0 },  //ColorTransformZ
};

GLubyte ListaColoresUbyte[17][4] = {
	{ (GLubyte)(0.22*255), (GLubyte)(0.28*255), (GLubyte)(0.25*255), 255  },    //fondo
	{ 255, 255, 255, 255     },                                                 //blanco
	{ (GLubyte)(0.91*255), (GLubyte)(0.50*255), (GLubyte)(0.98*255), 255  },    //acento (violeta) 
	{ (GLubyte)(0.48*255), (GLubyte)(0.30*255), 255, 255  },                    //acento oscuro	
	{ 0, 0, 0, 255     },                                                       //negro
	{ (GLubyte)(0.12*255), (GLubyte)(0.12*255), (GLubyte)(0.12*255), 255  },    //gris
	{ (GLubyte)(0.94*255), (GLubyte)(0.59*255), (GLubyte)(0.17*255), (GLubyte)(0.25f*255)},   //naranja transparente
	{ (GLubyte)(0.22*255), (GLubyte)(0.22*255), (GLubyte)(0.22*255), 255  },    //cabezera de la barra de herramientas
	{ 0, 0, 0, (GLubyte)(0.25f*255)     },                                           //negroTransparente
	{ (GLubyte)(0.757*255), (GLubyte)(0.757*255), (GLubyte)(0.757*255), 255     },   //azul de los iconos seleccionados
	{ (GLubyte)(0.757*255), (GLubyte)(0.757*255), (GLubyte)(0.757*255), 255     },   //azul de los iconos seleccionados

	{ (GLubyte)(0.22*255), (GLubyte)(0.22*255), (GLubyte)(0.22*255), 255 },  //LineaPiso
	{ (GLubyte)(0.56*255), (GLubyte)(0.23*255), (GLubyte)(0.28*255), 255 },  //LineaPisoRoja
	{ (GLubyte)(0.38*255), (GLubyte)(0.53*255), (GLubyte)(0.15*255), 255 },  //LineaPisoVerde

	{ (GLubyte)(0.88*255), (GLubyte)(0.48*255), (GLubyte)(0.54*255), 255 },  //ColorTransformX
	{ (GLubyte)(0.65*255), (GLubyte)(0.81*255), (GLubyte)(0.38*255), 255 },  //ColorTransformY
	{ (GLubyte)(0.46*255), (GLubyte)(0.67*255), (GLubyte)(0.89*255), 255 },  //ColorTransformZ
};