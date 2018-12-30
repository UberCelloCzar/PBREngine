cbuffer externalData : register(b0)
{
	float2 screenSpaceLightPos;
	float density;
	float weight;
	float decay;
	float exposure;
	int numSamples;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


// Texture-related variables
Texture2D OcclusionTexture : register(t0);
SamplerState BasicSampler : register(s0);


// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	float2 deltaTexCoord = (input.uv - screenSpaceLightPos.xy); // Pixel to light pos
	deltaTexCoord *= 1.0f / numSamples * density; // Divide by the number of samples and scale to the occluding medium density

	float3 color = float3(0.f,0.f,0.f); // Initial sample
	float2 tex = input.uv;

	float illuminationDecay = 1.0f; // Decay factor
	for (int i = 0; i < numSamples; i++) // Sum time!
	{
		tex -= deltaTexCoord; // Step along the ray
		float3 samp = OcclusionTexture.Sample(BasicSampler, tex).rgb;
		samp *= illuminationDecay * weight; // Apply attenuation and decay
		color += samp; // Accumulate color
		illuminationDecay *= decay; // Exponentiate decay
	}

	//color = OcclusionTexture.Sample(BasicSampler, input.uv).rgb;
	return float4(color * exposure, 1); // An extra measure of control
}