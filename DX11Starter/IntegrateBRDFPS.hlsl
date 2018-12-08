#include <IBLFunctions.hlsli>

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
	float3 worldPos		: POSITION; // The world position of this vertex
};

float IBLGeometrySchlickGGX(float NdotV, float roughness)
{
	float k = (roughness * roughness) / 2.0f;

	float denom = NdotV * (1.0f - k) + k;

	return NdotV / denom;
}

float IBLGeometrySmith(float3 normalVec, float3 viewDir, float3 lightDir, float k)
{
	float NdotV = max(dot(normalVec, viewDir), 0.0f);
	float NdotL = max(dot(normalVec, lightDir), 0.0f);
	float ggx1 = IBLGeometrySchlickGGX(NdotV, k);
	float ggx2 = IBLGeometrySchlickGGX(NdotL, k);

	return ggx1 * ggx2;
}

float2 main(VertexToPixel input) : SV_TARGET
{
	float3 view = float3(sqrt(1.0f - (input.uv.y * input.uv.y)), 0.f, input.uv.y); // Sin, 0, Cos
	
	float A = 0.0f;
	float B = 0.0f;
	float3 normal = float3(0.0f, 0.0f, 1.0f);
	const uint numSamples = 1024;
	
	// Generates a sample vector that's biased towards the preferred alignment direction (importance sampling)
	for (uint i = 0; i < numSamples; ++i)
	{
		float2 Xi = Hammersley(i, numSamples);
		float3 halfway = ImportanceSampleGGX(Xi, input.uv.x, normal);
		float3 light = normalize((2.0f * dot(view, halfway) * halfway) - view);

		float NdotL = saturate(light.z);
		float NdotH = saturate(halfway.z);
		float VdotH = saturate(dot(view, halfway));

		if (NdotL > 0)
		{
			float G = IBLGeometrySmith(normal, view, light, input.uv.x);
			float G_Vis = (G * VdotH) / (NdotH * input.uv.y);
			float Fc = pow(1.0f - VdotH, 5.0f);

			A += (1.0f - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	return float2(A, B) / numSamples;

}