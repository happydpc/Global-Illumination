#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2) uniform UniformBufferObjectLights
{
	vec4 camPos;
	
	vec4 dirDirLight;
	vec4 colorDirLight;

	float usePCF;
	float ambient;
} uboLights;

layout(binding = 3) uniform sampler2D texAlbedo;
layout(binding = 4) uniform sampler2D texNormal;
layout(binding = 5) uniform sampler2D texRoughness;
layout(binding = 6) uniform sampler2D texMetal;
layout(binding = 7) uniform sampler2D texAO;

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in mat3 tbn;

layout(location = 0) out vec4 outColor;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

const float PI = 3.14159265359;

void main() 
{
	vec3 albedo = texture(texAlbedo, fragTexCoord).xyz;
	float roughness = texture(texRoughness, fragTexCoord).x;
	float metallic = texture(texMetal, fragTexCoord).x;
	float ao = texture(texAO, fragTexCoord).x;
	vec3 normal = (texture(texNormal, fragTexCoord).xyz * 2.0 - vec3(1.0)) * tbn;

	vec3 N = normalize(normal); 
    vec3 V = normalize(uboLights.camPos.xyz - worldPos);
	vec3 R = reflect(-V, N);  

	vec3 F0 = vec3(0.04); 
    F0 = mix(F0,albedo, metallic);
	
	vec3 Lo = vec3(0.0);
	
	// calculate per-light radiance
	vec3 L = normalize(-uboLights.dirDirLight.xyz);
	vec3 H = normalize(V + L);
	vec3 radiance     = uboLights.colorDirLight.xyz;        
	
	// cook-torrance brdf
	float NDF = DistributionGGX(N, H, roughness);        
	float G   = GeometrySmith(N, V, L, roughness);      
	vec3 F    = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);       
	
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;	  
	
	vec3 nominator    = NDF * G * F;
	float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0); 
	vec3 specular     = nominator / max(denominator, 0.001);
		
	// add to outgoing radiance Lo
	float NdotL = max(dot(N, L), 0.0);                
	Lo += (kD * albedo / PI + specular) * radiance * NdotL;

	vec3 ambient = vec3(uboLights.ambient) * albedo;

    vec3 color = ambient + Lo;
	
    float exposure = 0.5;
    color = vec3(1.0) - exp(-color * exposure);
	color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, texture(texAlbedo, fragTexCoord).a);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}