

// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 uvw			: TEXCOORD0;
};


// Texture-related variables
TextureCube SkyTexture		: register(t0);
SamplerState BasicSampler	: register(s0);


// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	float3 color = SkyTexture.Sample(BasicSampler, input.uvw).rgb;
	color = color / (color + float3(1.0, 1.0, 1.0));
	color = pow(color, float3(0.45454545f, 0.45454545f, 0.45454545f));
	return float4(color, 1.f);
}