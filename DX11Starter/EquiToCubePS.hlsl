struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 uvw			: TEXCOORD;
};

Texture2D EquirectMap     	: register(t0);

SamplerState BasicSampler	: register(s0);

static const float PI = 3.14159265359;
static const float TWOPI = PI*2;

float2 SampleSphericalMap(float3 v)
{
	float theta = acos(v.y);
	float phi = atan2(v.x, v.z);
	return float2((PI + phi) / TWOPI, theta / PI); // Convert from polar
}

float4 main(VertexToPixel input) : SV_TARGET
{
	float2 uv = SampleSphericalMap(normalize(input.uvw).xyz);
	float3 color = EquirectMap.Sample(BasicSampler, uv).rgb;
	return float4(color, 1.0f);
}