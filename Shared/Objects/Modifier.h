class Modifier {
    public:
		std::string targetName = "";
		Object* target = nullptr;

        void SetTarget(const std::string& NewValue){
            targetName = NewValue;
        }

        void ReloadTarget(){
            Object* FindTarget = FindObjectByName(SceneCollection, targetName);
            if (FindTarget){
                target = FindTarget;
            }
        }
};