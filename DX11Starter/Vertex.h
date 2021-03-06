#pragma once

#include <DirectXMath.h>

// --------------------------------------------------------
// A custom vertex definition
//
// - Updated to support lighting and eventually textures
// - "Color" was removed, as it is rarely used for basic rendering
// --------------------------------------------------------
struct Vertex
{
	DirectX::XMFLOAT3 Position;	    // The position of the vertex
	DirectX::XMFLOAT2 UV;           // UV Coordinate for texturing (soon)
	DirectX::XMFLOAT3 Normal;       // Normal for lighting
	DirectX::XMFLOAT3 Tangent;		// Tangent is REQUIRED for normal mapping!
};