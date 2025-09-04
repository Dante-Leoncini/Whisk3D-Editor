class keyFrame { 
	public:
		int frame;
		//TInt value;
		GLfloat valueX;
		GLfloat valueY;
		GLfloat valueZ;
		//TBool boolean;
		int Interpolation;
};

// Función de intercambio
void Swap(keyFrame& a, keyFrame& b) {
    keyFrame temp = a;
    a = b;
    b = temp;
}

// Partición para quicksort
//int Partition(RArray<keyFrame>& arr, int low, int high) {
int Partition(std::vector<keyFrame>& arr, int low, int high) {
    int pivot = arr[high].frame;
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j].frame < pivot) {
            i++;
            Swap(arr[i], arr[j]);
        }
    }
    Swap(arr[i + 1], arr[high]);
    return i + 1;
}

// Implementación de quicksort
//void QuickSort(RArray<keyFrame>& arr, int low, int high) {
void QuickSort(std::vector<keyFrame>& arr, int low, int high) {
    if (low < high) {
        int pi = Partition(arr, low, high);
        QuickSort(arr, low, pi - 1);
        QuickSort(arr, pi + 1, high);
    }
}

bool compareKeyFrames(const keyFrame& a, const keyFrame& b) {
    return a.frame < b.frame;
}

class AnimProperty { 
	public:
		int Property;
        int firstFrameIndex;
        int lastFrameIndex;
		//RArray<keyFrame> keyframes;
        std::vector<keyFrame> keyframes;

        void SortKeyFrames() {
            QuickSort(keyframes, 0, keyframes.size() - 1);
        }
};

class AnimationObject { 
	public:
		int Id; //id del objeto al que afecta
		int FirstKeyFrame; //id del objeto al que afecta
		int LastKeyFrame; //id del objeto al que afecta
		//RArray<AnimProperty> Propertys;
        std::vector<AnimProperty> Propertys;
        
		void UpdateFirstLastFrame(){
            FirstKeyFrame = 100000000;
            LastKeyFrame = 0;
			for(size_t pr = 0; pr < Propertys.size(); pr++) {	
			    for(size_t kf = 0; kf < Propertys[pr].keyframes.size(); kf++) {	
                    if (Propertys[pr].keyframes[kf].frame > LastKeyFrame){
                        LastKeyFrame = Propertys[pr].keyframes[kf].frame;	
                    }
                    if (Propertys[pr].keyframes[kf].frame < FirstKeyFrame){
                        FirstKeyFrame = Propertys[pr].keyframes[kf].frame;	
                    }
                }	
			}
        }
};