#pragma once

#include <d3d11.h>

#include "Vertex.h"


class Mesh
{
public:
	Mesh(Vertex* vertArray, unsigned int numVerts, unsigned int* indexArray, unsigned int numIndices, ID3D11Device* device);
	Mesh(const char* objFile, ID3D11Device* device);
	~Mesh(void);

	ID3D11Buffer* GetVertexBuffer() { return vb; }
	ID3D11Buffer* GetIndexBuffer() { return ib; }
	int GetIndexCount() { return numIndices; }

private:
	ID3D11Buffer* vb;
	ID3D11Buffer* ib;
	int numIndices;

	void CalculateTangents(Vertex* verts, unsigned int numVerts, unsigned int* indices, unsigned int numIndices);
	void CreateBuffers(Vertex* vertArray, unsigned int numVerts, unsigned int* indexArray, unsigned int numIndices, ID3D11Device* device);
};

