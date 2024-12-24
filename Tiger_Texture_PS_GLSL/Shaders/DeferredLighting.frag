#version 430

out vec4 FragColor;
in vec2 TexCoords;

// G-buffer
uniform sampler2D gPosition; 
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;

struct Light {
    vec3 Position;
    vec3 Color;

    // Attenuation
    float Linear;       // K_l
    float Quadratic;    // K_q
};

// [����] �ִ� 128���� ����
uniform Light lights[128];
uniform int numLights;    // ���� ���� ����

const float Kc = 1.0;  
const float minBrightness = 5.0 / 256.0;

void main()
{
    vec3 FragPos = texture(gPosition, TexCoords).xyz;
    vec3 Normal  = normalize(texture(gNormal, TexCoords).xyz);
    vec3 Albedo  = texture(gAlbedo, TexCoords).rgb;

    vec3 lighting = Albedo * 0.1;

    for(int i = 0; i < numLights; i++)
    {
        // --- 1) �Ÿ� �� ���� ��� ---
        vec3 L = lights[i].Position - FragPos;
        float dist = length(L);
        L = normalize(L);

        float Imax = max(lights[i].Color.r, max(lights[i].Color.g, lights[i].Color.b));

        float Kl = lights[i].Linear;
        float Kq = lights[i].Quadratic;
        
        float radius = 50.0;

        // --- 2) �ݰ� �ȿ� ���� ���� ���� ��� ---
        float attenuation = 0.0;
        if (dist < radius)
        {
            attenuation = 1.0 / (Kc + Kl * dist + Kq * dist * dist);
        }

        // --- 3) ���� ��� ---
        float diff = max(dot(Normal, L), 0.0);
        vec3 diffuse = diff * Albedo * lights[i].Color;
        
        vec3 viewDir = normalize(-FragPos);
        vec3 halfway = normalize(L + viewDir);
        float spec = pow(max(dot(Normal, halfway), 0.0), 32.0);
        vec3 specular = spec * lights[i].Color;

        // ���� ����
        diffuse  *= attenuation;
        specular *= attenuation;

        // ���� �ݿ�
        lighting += diffuse + specular;
    }

    vec3 adjustedLighting = clamp(lighting + vec3(0.05), 0.0, 1.0);
    FragColor = vec4(adjustedLighting, 1.0);
}
