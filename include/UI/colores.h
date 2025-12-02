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
ColorType ListaColores[17][4] = {
	{ COLOR_CONVERT(0.22), COLOR_CONVERT(0.28), COLOR_CONVERT(0.25), COLOR_CONVERT(1.0)  },   //fondo
	{ COLOR_CONVERT(1.0), COLOR_CONVERT(1.0), COLOR_CONVERT(1.0), COLOR_CONVERT(1.0)     },   //blanco
	{ COLOR_CONVERT(0.91), COLOR_CONVERT(0.50), COLOR_CONVERT(0.98), COLOR_CONVERT(1.0)  },   //acento (violeta) 
	{ COLOR_CONVERT(0.48), COLOR_CONVERT(0.30), COLOR_CONVERT(1.0), COLOR_CONVERT(1.0)  },   //acento oscuro	
	{ COLOR_CONVERT(0.0), COLOR_CONVERT(0.0), COLOR_CONVERT(0.0), COLOR_CONVERT(1.0)     },   //negro
	{ COLOR_CONVERT(0.12), COLOR_CONVERT(0.12), COLOR_CONVERT(0.12), COLOR_CONVERT(1.0)  },   //gris
	{ COLOR_CONVERT(0.94), COLOR_CONVERT(0.59), COLOR_CONVERT(0.17), COLOR_CONVERT(0.25f)},   //naranja transparente
	{ COLOR_CONVERT(0.22), COLOR_CONVERT(0.22), COLOR_CONVERT(0.22), COLOR_CONVERT(1.0)  },    //cabezera de la barra de herramientas
	{ COLOR_CONVERT(0.0), COLOR_CONVERT(0.0), COLOR_CONVERT(0.0), COLOR_CONVERT(0.25f)     },   //negroTransparente
	{ COLOR_CONVERT(0.757), COLOR_CONVERT(0.757), COLOR_CONVERT(0.757), COLOR_CONVERT(1.0)     },   //azul de los iconos seleccionados
	{ COLOR_CONVERT(0.757), COLOR_CONVERT(0.757), COLOR_CONVERT(0.757), COLOR_CONVERT(1.0)     },   //azul de los iconos seleccionados

	{ COLOR_CONVERT(0.22), COLOR_CONVERT(0.22), COLOR_CONVERT(0.22), COLOR_CONVERT(1.0) },  //LineaPiso
	{ COLOR_CONVERT(0.56), COLOR_CONVERT(0.23), COLOR_CONVERT(0.28), COLOR_CONVERT(1.0) },  //LineaPisoRoja
	{ COLOR_CONVERT(0.38), COLOR_CONVERT(0.53), COLOR_CONVERT(0.15), COLOR_CONVERT(1.0) },  //LineaPisoVerde

	{ COLOR_CONVERT(0.88), COLOR_CONVERT(0.48), COLOR_CONVERT(0.54), COLOR_CONVERT(1.0) },  //ColorTransformX
	{ COLOR_CONVERT(0.65), COLOR_CONVERT(0.81), COLOR_CONVERT(0.38), COLOR_CONVERT(1.0) },  //ColorTransformY
	{ COLOR_CONVERT(0.46), COLOR_CONVERT(0.67), COLOR_CONVERT(0.89), COLOR_CONVERT(1.0) },  //ColorTransformZ
};

GLfixed ListaColoresX[17][4] = {
	{ COLOR_CONVERT_FIXED(0.22), COLOR_CONVERT_FIXED(0.28), COLOR_CONVERT_FIXED(0.25), COLOR_CONVERT_FIXED(1.0)  },   //fondo
	{ COLOR_CONVERT_FIXED(1.0), COLOR_CONVERT_FIXED(1.0), COLOR_CONVERT_FIXED(1.0), COLOR_CONVERT_FIXED(1.0)     },   //blanco
	{ COLOR_CONVERT_FIXED(0.91), COLOR_CONVERT_FIXED(0.50), COLOR_CONVERT_FIXED(0.98), COLOR_CONVERT_FIXED(1.0)  },   //acento (violeta) 
	{ COLOR_CONVERT_FIXED(0.48), COLOR_CONVERT_FIXED(0.30), COLOR_CONVERT_FIXED(1.0), COLOR_CONVERT_FIXED(1.0)  },   //acento oscuro	
	{ COLOR_CONVERT_FIXED(0.0), COLOR_CONVERT_FIXED(0.0), COLOR_CONVERT_FIXED(0.0), COLOR_CONVERT_FIXED(1.0)     },   //negro
	{ COLOR_CONVERT_FIXED(0.12), COLOR_CONVERT_FIXED(0.12), COLOR_CONVERT_FIXED(0.12), COLOR_CONVERT_FIXED(1.0)  },   //gris
	{ COLOR_CONVERT_FIXED(0.94), COLOR_CONVERT_FIXED(0.59), COLOR_CONVERT_FIXED(0.17), COLOR_CONVERT_FIXED(0.25f)},   //naranja transparente
	{ COLOR_CONVERT_FIXED(0.22), COLOR_CONVERT_FIXED(0.22), COLOR_CONVERT_FIXED(0.22), COLOR_CONVERT_FIXED(1.0)  },    //cabezera de la barra de herramientas
	{ COLOR_CONVERT_FIXED(0.0), COLOR_CONVERT_FIXED(0.0), COLOR_CONVERT_FIXED(0.0), COLOR_CONVERT_FIXED(0.25f)     },   //negroTransparente
	{ COLOR_CONVERT_FIXED(0.757), COLOR_CONVERT_FIXED(0.757), COLOR_CONVERT_FIXED(0.757), COLOR_CONVERT_FIXED(1.0)     },   //azul de los iconos seleccionados
	{ COLOR_CONVERT_FIXED(0.757), COLOR_CONVERT_FIXED(0.757), COLOR_CONVERT_FIXED(0.757), COLOR_CONVERT_FIXED(1.0)     },   //azul de los iconos seleccionados

	{ COLOR_CONVERT_FIXED(0.22), COLOR_CONVERT_FIXED(0.22), COLOR_CONVERT_FIXED(0.22), COLOR_CONVERT_FIXED(1.0) },  //LineaPiso
	{ COLOR_CONVERT_FIXED(0.56), COLOR_CONVERT_FIXED(0.23), COLOR_CONVERT_FIXED(0.28), COLOR_CONVERT_FIXED(1.0) },  //LineaPisoRoja
	{ COLOR_CONVERT_FIXED(0.38), COLOR_CONVERT_FIXED(0.53), COLOR_CONVERT_FIXED(0.15), COLOR_CONVERT_FIXED(1.0) },  //LineaPisoVerde

	{ COLOR_CONVERT_FIXED(0.88), COLOR_CONVERT_FIXED(0.48), COLOR_CONVERT_FIXED(0.54), COLOR_CONVERT_FIXED(1.0) },  //ColorTransformX
	{ COLOR_CONVERT_FIXED(0.65), COLOR_CONVERT_FIXED(0.81), COLOR_CONVERT_FIXED(0.38), COLOR_CONVERT_FIXED(1.0) },  //ColorTransformY
	{ COLOR_CONVERT_FIXED(0.46), COLOR_CONVERT_FIXED(0.67), COLOR_CONVERT_FIXED(0.89), COLOR_CONVERT_FIXED(1.0) },  //ColorTransformZ
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