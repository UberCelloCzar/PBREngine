static const float PI = 3.14159265359;
static const float TWOPI = PI * 2.f;
static const float HALFPI = PI / 2.f;

float NormalDistributionGGXTR(float3 normal, float3 halfway, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(normal, halfway), 0.0);
	NdotH = NdotH * NdotH;

	float denom = (NdotH * (a2 - 1.0f) + 1.0f);

	return a2 / (PI * denom * denom);
}
