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


// VanDerCorput calculation
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html

float RadicalInverse_VdC(uint bits) // Mirrors the bits of a 32bit decimal around the decimal point
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 normal)
{
	float a = roughness * roughness;
	float phi = 2.f * PI * Xi.x;
	float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a*a - 1.f) * Xi.y));
	float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	float3 halfway;
	halfway.x = sinTheta * cos(phi); // Spherical to cartesian
	halfway.y = sinTheta * sin(phi);
	halfway.z = cosTheta;

	float3 up = abs(normal.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f); // Tangent H to world vec
	float3 tangentX = normalize(cross(up, normal));
	float3 tangentY = cross(normal, tangentX);

	return normalize((tangentX * halfway.x) + (tangentY * halfway.y) + (normal * halfway.z)); // Tangent to world
}

float4 main(VertexToPixel input) : SV_TARGET
{
	float3 normal = normalize(input.uvw);
	float3 irradiance = float3(0.f, 0.f, 0.f);

	float3 color = float3(0.f, 0.f, 0.f);
	float totalWeight = 0.f;
	const uint numSamples = 1024;

	for (uint i = 0; i < numSamples; i++)
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

			float resolution = 512.f; // Resolution of source cube face
			float saTexel = 4.f * PI / (6.f * resolution * resolution);
			float saSample = 1.f / (float(numSamples) * pdf + .0001f);

			float mipLevel = roughness == 0.f ? 0.f : .5f * log2(saSample / saTexel);

			//color += EnvironmentCubemap.SampleLevel(BasicSampler, light, mipLevel).rgb * NdotL;
			color += EnvironmentCubemap.Sample(BasicSampler, light).rgb * NdotL;
			totalWeight += NdotL;
		}
	}

	color = color / totalWeight;
	return float4(color, 1.0f);
}