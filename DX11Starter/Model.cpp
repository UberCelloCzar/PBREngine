#include "Model.h"
#include <iostream>
#include "Vertex.h"

using namespace DirectX;

Model::~Model()
{
	for (auto& m : meshes)
	{
		delete m;
	}
}

void Model::loadModel(std::string path, ID3D11Device* device)
{
	Assimp::Importer importer;
	const aiScene *scene = importer.ReadFile(path, 0); // May need to not flip UVs here, that may just be an opengl thing

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // If we don't have a scene, the scene is incomplete, or we have no root node
	{
		std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return;
	}
	directory = path.substr(0, path.find_last_of('/'));

	processNode(scene->mRootNode, scene, device);
}


void Model::processNode(aiNode *node, const aiScene *scene, ID3D11Device* device)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++) // Bring in the node's meshes
	{
		aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene, device));
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) // Recursively process child nodes until done
	{
		processNode(node->mChildren[i], scene, device);
	}
}

Mesh* Model::processMesh(aiMesh *mesh, const aiScene *scene, ID3D11Device* device)
{
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	//std::cout << "NumVerts: " << mesh->mNumVertices << std::endl;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) // Process vertex data
	{
		Vertex vertex;
		vertex.Position = XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		//std::cout << "Vertex: " << vertex.Position.x << ", " << vertex.Position.y << ", " << vertex.Position.z << std::endl;
		vertex.Normal = XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		if (mesh->mTextureCoords[0]) // Meshes can straight up not have texture coords
		{
			vertex.UV = XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}
		else vertex.UV = XMFLOAT2(0.0f, 0.0f);
		vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++) // Grab the indices
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
			//std::cout << "Index: " << face.mIndices[j] << std::endl;
		}
	}
	//for (int i = 0; i < vertices.size(); i++)
	//{
	//	std::cout << "Vertex " << i << std::endl;
	//	std::cout << "Pos: " << vertices[i].Position.x << ", " << vertices[i].Position.y << ", " << vertices[i].Position.z << std::endl;
	//	std::cout << "Nor: " << vertices[i].Normal.x << ", " << vertices[i].Normal.y << ", " << vertices[i].Normal.z << std::endl;
	//	std::cout << "UV : " << vertices[i].UV.x << ", " << vertices[i].UV.y << std::endl;
	//}
	//for (int i = 0; i < indices.size(); i++)
	//{
	//	std::cout << "Index: " << indices[i] << std::endl;
	//}
	//std::cout << "Vert size: " << vertices.size() << std::endl;
	//std::cout << "Ind size: " << vertices.size() << std::endl;
	return new Mesh(vertices.data(), vertices.size(), indices.data(), indices.size(), device);
}
