static const float PI = 3.14159265359;
static const float TWOPI = PI * 2.f;
static const float HALFPI = PI / 2.f;


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

