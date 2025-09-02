typedef enum { Constant, Linear, EaseInOut, EaseIn, EaseOut };
class keyFrame { 
	public:
		TInt frame;
		//TInt value;
		GLfloat valueX;
		GLfloat valueY;
		GLfloat valueZ;
		//TBool boolean;
		TInt Interpolation;
};

// Función de intercambio
void Swap(keyFrame& a, keyFrame& b) {
    keyFrame temp = a;
    a = b;
    b = temp;
}

// Partición para quicksort
TInt Partition(RArray<keyFrame>& arr, TInt low, TInt high) {
    TInt pivot = arr[high].frame;
    TInt i = low - 1;

    for (TInt j = low; j < high; j++) {
        if (arr[j].frame < pivot) {
            i++;
            Swap(arr[i], arr[j]);
        }
    }
    Swap(arr[i + 1], arr[high]);
    return i + 1;
}

// Implementación de quicksort
void QuickSort(RArray<keyFrame>& arr, TInt low, TInt high) {
    if (low < high) {
        TInt pi = Partition(arr, low, high);
        QuickSort(arr, low, pi - 1);
        QuickSort(arr, pi + 1, high);
    }
}

TBool compareKeyFrames(const keyFrame& a, const keyFrame& b) {
    return a.frame < b.frame;
}

typedef enum { AnimPosition, AnimRotation, AnimScale };
class AnimProperty { 
	public:
		TInt Property;
        TInt firstFrameIndex;
        TInt lastFrameIndex;
		RArray<keyFrame> keyframes;

        void AnimProperty::SortKeyFrames() {
            QuickSort(keyframes, 0, keyframes.Count() - 1);
        }
};

class AnimationObject { 
	public:
		TInt Id; //id del objeto al que afecta
		TInt FirstKeyFrame; //id del objeto al que afecta
		TInt LastKeyFrame; //id del objeto al que afecta
		RArray<AnimProperty> Propertys;
        
		void AnimationObject::UpdateFirstLastFrame(){
            FirstKeyFrame = 100000000;
            LastKeyFrame = 0;
			for(TInt pr = 0; pr < Propertys.Count(); pr++) {	
			    for(TInt kf = 0; kf < Propertys[pr].keyframes.Count(); kf++) {	
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