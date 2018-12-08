#pragma once

#include "DXCore.h"
#include "SimpleShader.h"
#include <DirectXMath.h>

#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "Model.h"

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);

	// Overridden mouse input helper methods
	void OnMouseDown (WPARAM buttonState, int x, int y);
	void OnMouseUp	 (WPARAM buttonState, int x, int y);
	void OnMouseMove (WPARAM buttonState, int x, int y);
	void OnMouseWheel(float wheelDelta,   int x, int y);
private:

	// Input and mesh swapping
	bool prevTab;
	unsigned int currentEntity;
	bool prevB;
	unsigned int currentEnv;

	// Keep track of "stuff" to clean up
	Model* models[8];
	std::vector<GameEntity*> entities;
	Camera* camera;

	// Initialization helper methods - feel free to customize, combine, etc.
	void LoadShaders(); 
	void CreateMatrices();
	void LoadModels();
	void LoadTextures();
	void CreateGameEntities();
	void CreateBRDFLUT();
	void ConvertEquisToEnvironments(int hdrInd);

	// Buffers to hold actual geometry data
	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;

	// Wrappers for DirectX shaders to provide simplified functionality
	SimpleVertexShader* vertexShader;
	SimplePixelShader* pixelShader;
	SimpleVertexShader* equirectangularToCubemapVS;
	SimplePixelShader* equirectangularToCubemapPS;
	SimplePixelShader* irradianceConvolutionPS;
	SimplePixelShader* prefilterEnvironmentPS;
	SimplePixelShader* integrateBRDFPS;

	// The matrices to go from model space to screen space
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;

	// Skybox stuff --------------------------
	SimpleVertexShader* skyVS;
	SimplePixelShader* skyPS;

	ID3D11RasterizerState* skyRasterState;
	ID3D11DepthStencilState* skyDepthState;

	// Texture map SRVs
	ID3D11ShaderResourceView* albedoMapSRVs[11];
	ID3D11ShaderResourceView* normalMapSRVs[11];
	ID3D11ShaderResourceView* metalnessMapSRVs[11];
	ID3D11ShaderResourceView* roughnessMapSRVs[11];
	ID3D11ShaderResourceView* aoMapSRVs[2];

	ID3D11ShaderResourceView* hdrEquiSRVs[3];
	//ID3D11ShaderResourceView* hdrIrrEquiSRVs[3];
	ID3D11ShaderResourceView* hdrCubeSRVs[3];
	ID3D11ShaderResourceView* irradianceMapSRVs[3];
	ID3D11ShaderResourceView* envPrefilterSRVs[3];
	ID3D11ShaderResourceView* brdfLUTSRV;


	// Needed for sampling options (like filter and address modes)
	ID3D11SamplerState* sampler;

	// Keeps track of the old mouse position.  Useful for 
	// determining how far the mouse moved in a single frame.
	POINT prevMousePos;
};

