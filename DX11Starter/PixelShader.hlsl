#include <IBLFunctions.hlsli>

cbuffer externalData : register(b0)
{
	float3 LightPos1;
	float3 LightPos2;
	float3 LightPos3;
	float3 LightPos4;
	float3 LightColor1;

	float3 CameraPosition;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
	float3 worldPos		: POSITION; // The world position of this PIXEL
};

Texture2D AlbedoMap     	 : register(t0);
Texture2D NormalMap			 : register(t1);
Texture2D MetallicMap        : register(t2);
Texture2D RoughnessMap       : register(t3);
Texture2D AOMap              : register(t4);
//Texture2D BRDFLookup		 : register(t5);
//TextureCube EnvIrradianceMap : register(t6);
//TextureCube EnvPrefilterMap	 : register(t7);

SamplerState BasicSampler	: register(s0);


float GeometrySchlickGGX(float NdotV, float roughness)  // k is a remapping of roughness based on direct lighting or IBL lighting
{
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f;

	return NdotV / (NdotV * (1.0f - k) + k);
}


float GeometrySmith(float3 normal, float3 view, float3 light, float k)
{
	float NdotV = max(dot(normal, view), 0.0f);
	float NdotL = max(dot(normal, light), 0.0f);
	float ggx1 = GeometrySchlickGGX(NdotV, k);
	float ggx2 = GeometrySchlickGGX(NdotL, k);

	return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)   // cosTheta is n.v and F0 is the base reflectivity
{
	return (F0 + (1.f-F0) * pow(1.f-cosTheta, 5.f));
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)   // cosTheta is n.v and F0 is the base reflectivity
{
	return F0 + (max(float3(1.f-roughness, 1.f-roughness, 1.f-roughness), F0) - F0) * pow(1.f - cosTheta, 5.f);
}

void CalculateRadiance(VertexToPixel input, float3 view, float3 normal, float3 albedo, float roughness, float metalness, float3 lightPosition, float3 lightColor, float3 F0, out float3 rad)
{

	float3 light = normalize(lightPosition - input.worldPos); // Get the base radiance
	float3 halfway = normalize(view + light);
	float distance = length(lightPosition - input.worldPos);
	float attenuation = 1.0f / (distance * distance);
	float3 radiance = lightColor * attenuation;

	//Cook-Torrance BRDF
	float3 F = FresnelSchlick(max(dot(halfway, view), 0.0f), F0);
	float D = NormalDistributionGGXTR(normal, halfway, roughness);
	float G = GeometrySmith(normal, view, light, roughness);

	float denom = 4.0f * max(dot(normal, view), 0.0f) * max(dot(normal, light), 0.0);
	float3 specular = (D * G * F) / max(denom, .001);
	
	float3 kD = float3(1.0f, 1.0f, 1.0f) - F; // F = kS
	kD *= 1.0 - metalness;


	//Add to outgoing radiance Lo
	float NdotL = max(dot(normal, light), 0.0f);
	rad = (((kD * albedo / PI) + specular) * radiance * NdotL);
}


float4 main(VertexToPixel input) : SV_TARGET
{
	float3 albedo = pow(AlbedoMap.Sample(BasicSampler, input.uv).rgb, 2.2); // Sample any and all textures
	float metalness = MetallicMap.Sample(BasicSampler, input.uv).r; // Metallic
	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r; // Rough
	float3 normalFromTexture = NormalMap.Sample(BasicSampler, input.uv).xyz * 2 - 1; // Sample and unpack normal
	float ao = AOMap.Sample(BasicSampler, input.uv).r;

	input.normal = normalize(input.normal); // Re-normalize any interpolated values
	input.tangent = normalize(input.tangent);

	// Create the TBN matrix which allows us to go from TANGENT space to WORLD space
	float3 T = normalize(input.tangent - input.normal * dot(input.tangent, input.normal)); // Adjust tangent to be orthogonal if normal isn't already
	float3 B = cross(T, input.normal);
	float3x3 TBN = float3x3(T, B, input.normal);
	input.normal = normalize(mul(normalFromTexture, TBN)); // Calculate the adjusted normal

	float3 view = normalize(CameraPosition - input.worldPos); // View vector
	float3 reflection = reflect(-view, input.normal); // Reflection vector

	float3 F0 = float3(0.04f, 0.04f, 0.04f); // Approximates a base dielectric reflectivity
	F0 = lerp(F0, albedo, metalness); // The more metallic a surface is, the more its surface color is just full reflection
	
	float3 radiance = float3(0.0f, 0.0f, 0.0f);
	float3 Lo = float3(0.0f, 0.0f, 0.0f);

	CalculateRadiance(input, view, input.normal, albedo, roughness, metalness, LightPos1, LightColor1, F0, radiance);
	Lo += radiance;
	CalculateRadiance(input, view, input.normal, albedo, roughness, metalness, LightPos2, LightColor1, F0, radiance);
	Lo += radiance;
	CalculateRadiance(input, view, input.normal, albedo, roughness, metalness, LightPos3, LightColor1, F0, radiance);
	Lo += radiance;
	CalculateRadiance(input, view, input.normal, albedo, roughness, metalness, LightPos4, LightColor1, F0, radiance);
	Lo += radiance;

	float3 kS = FresnelSchlickRoughness(max(dot(input.normal, view), 0.f), F0, roughness);
	float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
	kD *= 1.0 - metalness;
	//float3 irradiance = EnvIrradianceMap.Sample(BasicSampler, input.normal).rgb;
	//float3 diffuse = albedo * irradiance;


	//const float MAX_REF_LOD = 4.0f;
	//float3 prefilteredColor = skyPrefilter.SampleLevel(basicSampler, R, roughness * MAX_REF_LOD).rgb;
	//float2 brdf = brdfLUT.Sample(basicSampler, float2(max(dot(input.normal, view), 0.0f), roughness)).rg;
	//float3 specular = prefilteredColor * (kS * brdf.x + brdf.y);

	//float3 ambient = (kD * diffuse) * ao;
	//float3 color = ambient + Lo;
	float3 color = Lo + float3(.2f, .2f, .2f);
	color = color / (color + float3(1.0f, 1.0f, 1.0f));
	color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));

	return float4(color, 1.0f);
}