cbuffer externalData : register(b0)
{
	float3 color;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
	float3 worldPos		: POSITION; // The world position of this vertex
};

float4 main(VertexToPixel input) : SV_TARGET
{
	return float4(color, 1.f);
}