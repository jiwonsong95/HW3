// light and material definitions
struct _loc_light {
	GLint position;   // lights[i].Position
	GLint color;      // lights[i].Color
	GLint linear;     // lights[i].Linear
	GLint quadratic;  // lights[i].Quadratic
};

struct TigerInstance {
    int textureID;
    bool useTigerMaterial;
    glm::vec3 translation;
    glm::vec3 scale;
    float extraRotX_deg;
};