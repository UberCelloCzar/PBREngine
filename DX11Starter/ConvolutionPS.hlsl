struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 uvw			: TEXCOORD;
};

TextureCube EnvironmentCubemap : register(t0);

SamplerState BasicSampler	: register(s0);

static const float PI = 3.14159265359;
static const float TWOPI = PI*2.f;
static const float HALFPI = PI / 2.f;

float4 main(VertexToPixel input) : SV_TARGET
{
	float3 normal = normalize(input.uvw);
	float3 irradiance = float3(0.f, 0.f, 0.f);

	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 right = cross(up, normal);
	up = cross(normal, right);

	float sampleDelta = 0.025f;
	float nrSamples = 0.0f;
	for (float phi = 0.0f; phi < TWOPI; phi += sampleDelta)
	{
		for (float theta = 0.0f; theta < HALFPI; theta += sampleDelta)
		{
			float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			float3 sampleVec = (tangentSample.x * right) + (tangentSample.y * up) + (tangentSample.z * normal);

			irradiance += EnvironmentCubemap.SampleLevel(BasicSampler, sampleVec, 0).rgb * cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	irradiance = PI * irradiance * (1.f / nrSamples);
	return float4(irradiance, 1.0f);
}