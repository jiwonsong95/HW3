#version 430

out vec4 FragColor;
in vec2 TexCoords;

// G-buffer
uniform sampler2D gPosition; 
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;

// ���� ����ü ����
struct Light {
    vec3 Position;
    vec3 Color;

    // Attenuation
    float Linear;    
    float Quadratic;
};

// [����] �ִ� 128���� ����
uniform Light lights[128];
uniform int numLights;    // ���� ���� ����

void main()
{
    vec3 FragPos = texture(gPosition, TexCoords).xyz;
    vec3 Normal  = normalize(texture(gNormal, TexCoords).xyz);
    vec3 Albedo  = texture(gAlbedo, TexCoords).rgb;

    vec3 lighting = Albedo * 0.1;

    for(int i = 0; i < numLights; i++)
    {
        vec3 L = lights[i].Position - FragPos;
        float dist = length(L);
        L = normalize(L);

        float attenuation = 1.0 / (1.0 + lights[i].Linear * dist
                                      + lights[i].Quadratic * (dist * dist));

        float diff = max(dot(Normal, L), 0.0);
        vec3 diffuse = diff * Albedo * lights[i].Color;

        vec3 viewDir = normalize(-FragPos); // view space
        vec3 halfway = normalize(L + viewDir);
        // specular ������ 32�� fix
        float spec = pow(max(dot(Normal, halfway), 0.0), 32.0);
        vec3 specular = spec * lights[i].Color;

        diffuse *= attenuation;
        specular *= attenuation;

        lighting += diffuse + specular;
    }

    vec3 adjustedLighting = clamp(lighting + vec3(0.05), 0.0, 1.0);
    FragColor = vec4(adjustedLighting, 1.0);
}