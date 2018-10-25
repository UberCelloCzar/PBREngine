#pragma once
#include "Mesh.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <d3d11.h>

class Model
{
public:
	Model(char* path, ID3D11Device* device)
	{
		loadModel(path, device);
	}
	~Model();

	std::vector<Mesh*> meshes;
private:
	std::string directory;
	void loadModel(std::string path, ID3D11Device* device);
	void processNode(aiNode *node, const aiScene *scene, ID3D11Device* device);
	Mesh* processMesh(aiMesh *mesh, const aiScene *scene, ID3D11Device* device);
};

