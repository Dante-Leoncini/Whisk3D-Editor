enum{array, mirror,	screw, boolean};

class Modifier { 
	public:
		int type = 0;
		int Id = -1;
		int Target = -1;
		std::vector<bool> OpcionesTBool;
        //RArray<TBool> OpcionesTBool;
};

std::vector<Modifier> Modifiers;