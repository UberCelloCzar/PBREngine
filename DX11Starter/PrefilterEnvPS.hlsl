#include <IBLFunctions.hlsli>

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 uvw			: TEXCOORD;
};

cbuffer ExternalData : register(b0)
{
	float roughness;
}

TextureCube EnvironmentCubemap : register(t0);

SamplerState BasicSampler	: register(s0);


float NormalDistributionGGXTR(float3 normal, float3 halfway, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(normal, halfway), 0.0);
	NdotH = NdotH * NdotH;

	float denom = (NdotH * (a2 - 1.0f) + 1.0f);

	return a2 / (PI * denom * denom);
}

float4 main(VertexToPixel input) : SV_TARGET
{
	float3 normal = normalize(input.uvw);
	float3 irradiance = float3(0.f, 0.f, 0.f);

	float3 color = float3(0.f, 0.f, 0.f);
	float totalWeight = 0.f;
	const uint numSamples = 1024;

	for (uint i = 0; i < numSamples; ++i)
	{
		float2 Xi = Hammersley(i, numSamples);
		float3 halfway = ImportanceSampleGGX(Xi, roughness, normal);
		float3 light = 2.f * dot(normal, halfway) * halfway - normal; // Normal is the same as view direction for the puropses of the prefilter
		float NdotL = saturate(dot(normal, light));
		if (NdotL > 0)
		{
			float D = NormalDistributionGGXTR(normal, halfway, roughness); // Sample from other mips to reduce patchiness
			float NdotH = max(dot(normal, halfway), 0.f); // Since view is the same as normal, HdotV = NdotH
			float pdf = D * NdotH / (4.f * NdotH) + .0001f;

			float resolution = 1024.f; // Resolution of source cube face
			float saTexel = 4.f * PI / (6.f * resolution * resolution);
			float saSample = 1.f / (float(numSamples) * pdf + .0001f);

			float mipLevel = roughness == 0.f ? 0.f : .5f * log2(saSample / saTexel);

			color += EnvironmentCubemap.SampleLevel(BasicSampler, light, mipLevel).rgb * NdotL;
			totalWeight += NdotL;
		}
	}

	color = color / totalWeight;
	return float4(color, 1.0f);
}