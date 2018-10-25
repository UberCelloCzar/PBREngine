#include "Game.h"
#include "Vertex.h"

#include "WICTextureLoader.h"
#include "DDSTextureLoader.h"
#include <iostream>
#include <DirectXTex.h>

// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{
	// Initialize fields
	vertexBuffer = 0;
	indexBuffer = 0;
	vertexShader = 0;
	pixelShader = 0;
	camera = 0;

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.");
#endif
	
}

Game::~Game()
{
	// Release any (and all!) DirectX objects
	// we've made in the Game class
	if (vertexBuffer) { vertexBuffer->Release(); }
	if (indexBuffer) { indexBuffer->Release(); }

	// Clean up other resources
	for (int i = 0; i < 11; i++)
	{
		albedoMapSRVs[i]->Release();
		normalMapSRVs[i]->Release();
		metalnessMapSRVs[i]->Release();
		roughnessMapSRVs[i]->Release();
	}
	for (auto& ao : aoMapSRVs) ao->Release();
	sampler->Release();

	// Clean up sky stuff
	delete skyVS;
	delete skyPS;
	skyRasterState->Release();
	skyDepthState->Release();

	hdrEquiSRVs[0]->Release();
	hdrCubeSRVs[0]->Release();
	irradianceMapSRVs[0]->Release();
	//envPrefilterSRVs[0]->Release();

	// Delete our simple shader objects, which
	// will clean up their own internal DirectX stuff
	delete vertexShader;
	delete pixelShader;
	delete equirectangularToCubemapVS;
	delete equirectangularToCubemapPS;
	delete irradianceConvolutionPS;
	delete prefilterEnvironmentPS;

	// Clean up our other resources
	for (auto& m : models) { delete m; }
	for (auto& e : entities) { delete e; }
	delete camera;
}

void Game::Init()
{
	LoadShaders();
	CreateMatrices();
	LoadModels();
	LoadTextures();
	CreateGameEntities();

	// Create a sampler state that holds options for sampling
	// The descriptions should always just be local variables
	D3D11_SAMPLER_DESC samplerDesc = {}; // The {} part zeros out the struct!
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX; // Setting this allows for mip maps to work! (if they exist)

	// Ask DirectX for the actual object
	device->CreateSamplerState(&samplerDesc, &sampler);

	/* Skybox states */
	// Rasterize state for drawing the "inside"
	D3D11_RASTERIZER_DESC rd = {}; // Remember to zero it out!
	rd.CullMode = D3D11_CULL_FRONT;
	rd.FillMode = D3D11_FILL_SOLID;
	rd.DepthClipEnable = true;
	device->CreateRasterizerState(&rd, &skyRasterState);

	// Depth state for accepting pixels with depth EQUAL to existing depth
	D3D11_DEPTH_STENCIL_DESC ds = {};
	ds.DepthEnable = true;
	ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	device->CreateDepthStencilState(&ds, &skyDepthState);

	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Convert equirectangular maps to all the necessary parts for a PBR environment 
	ConvertEquisToEnvironments(0);


	context->OMSetRenderTargets(1, &backBufferRTV, depthStencilView);
	context->RSSetViewports(1, &viewport);
}

void Game::LoadShaders()
{
	vertexShader = new SimpleVertexShader(device, context);
	vertexShader->LoadShaderFile(L"VertexShader.cso");

	pixelShader = new SimplePixelShader(device, context);
	pixelShader->LoadShaderFile(L"PixelShader.cso");

	equirectangularToCubemapVS = new SimpleVertexShader(device, context);
	equirectangularToCubemapVS->LoadShaderFile(L"EquiToCubeVS.cso");

	equirectangularToCubemapPS = new SimplePixelShader(device, context);
	equirectangularToCubemapPS->LoadShaderFile(L"EquiToCubePS.cso");

	irradianceConvolutionPS = new SimplePixelShader(device, context);
	irradianceConvolutionPS->LoadShaderFile(L"ConvolutionPS.cso");

	prefilterEnvironmentPS = new SimplePixelShader(device, context);
	prefilterEnvironmentPS->LoadShaderFile(L"PrefilterEnvPS.cso");

	skyVS = new SimpleVertexShader(device, context);
	skyVS->LoadShaderFile(L"SkyVS.cso");

	skyPS = new SimplePixelShader(device, context);
	skyPS->LoadShaderFile(L"SkyPS.cso");
}

void Game::CreateMatrices()
{
	camera = new Camera(0, 0, -5);
	camera->UpdateProjectionMatrix((float)width / height);
}

void Game::LoadModels()
{
	models[0] = new Model("Models/cube.obj", device);
	models[1] = new Model("Models/sphere.obj", device);
	models[2] = new Model("Models/helix.obj", device);
	models[3] = new Model("Models/cone.obj", device);
	models[4] = new Model("Models/cylinder.obj", device);
	models[5] = new Model("Models/torus.obj", device);
	models[6] = new Model("Models/Cerberus_LP.FBX", device);
}

void Game::LoadTextures()
{
	CreateWICTextureFromFile(device, context, L"Textures/AluminiumInsulator_Albedo.png", 0, &albedoMapSRVs[0]);
	CreateWICTextureFromFile(device, context, L"Textures/AluminiumInsulator_Normal.png", 0, &normalMapSRVs[0]);
	CreateWICTextureFromFile(device, context, L"Textures/AluminiumInsulator_Metallic.png", 0, &metalnessMapSRVs[0]);
	CreateWICTextureFromFile(device, context, L"Textures/AluminiumInsulator_Roughness.png", 0, &roughnessMapSRVs[0]);

	CreateWICTextureFromFile(device, context, L"Textures/Gold_Albedo.png", 0, &albedoMapSRVs[1]);
	CreateWICTextureFromFile(device, context, L"Textures/Gold_Normal.png", 0, &normalMapSRVs[1]);
	CreateWICTextureFromFile(device, context, L"Textures/Gold_Metallic.png", 0, &metalnessMapSRVs[1]);
	CreateWICTextureFromFile(device, context, L"Textures/Gold_Roughness.png", 0, &roughnessMapSRVs[1]);

	CreateWICTextureFromFile(device, context, L"Textures/GunMetal_Albedo.png", 0, &albedoMapSRVs[2]);
	CreateWICTextureFromFile(device, context, L"Textures/GunMetal_Normal.png", 0, &normalMapSRVs[2]);
	CreateWICTextureFromFile(device, context, L"Textures/GunMetal_Metallic.png", 0, &metalnessMapSRVs[2]);
	CreateWICTextureFromFile(device, context, L"Textures/GunMetal_Roughness.png", 0, &roughnessMapSRVs[2]);

	CreateWICTextureFromFile(device, context, L"Textures/Leather_Albedo.png", 0, &albedoMapSRVs[3]);
	CreateWICTextureFromFile(device, context, L"Textures/Leather_Normal.png", 0, &normalMapSRVs[3]);
	CreateWICTextureFromFile(device, context, L"Textures/Leather_Metallic.png", 0, &metalnessMapSRVs[3]);
	CreateWICTextureFromFile(device, context, L"Textures/Leather_Roughness.png", 0, &roughnessMapSRVs[3]);

	CreateWICTextureFromFile(device, context, L"Textures/SuperHeroFabric_Albedo.png", 0, &albedoMapSRVs[4]);
	CreateWICTextureFromFile(device, context, L"Textures/SuperHeroFabric_Normal.png", 0, &normalMapSRVs[4]);
	CreateWICTextureFromFile(device, context, L"Textures/SuperHeroFabric_Metallic.png", 0, &metalnessMapSRVs[4]);
	CreateWICTextureFromFile(device, context, L"Textures/SuperHeroFabric_Roughness.png", 0, &roughnessMapSRVs[4]);

	CreateWICTextureFromFile(device, context, L"Textures/CamoFabric_Albedo.png", 0, &albedoMapSRVs[5]);
	CreateWICTextureFromFile(device, context, L"Textures/CamoFabric_Normal.png", 0, &normalMapSRVs[5]);
	CreateWICTextureFromFile(device, context, L"Textures/CamoFabric_Metallic.png", 0, &metalnessMapSRVs[5]);
	CreateWICTextureFromFile(device, context, L"Textures/CamoFabric_Roughness.png", 0, &roughnessMapSRVs[5]);

	CreateWICTextureFromFile(device, context, L"Textures/GlassVisor_Albedo.png", 0, &albedoMapSRVs[6]);
	CreateWICTextureFromFile(device, context, L"Textures/GlassVisor_Normal.png", 0, &normalMapSRVs[6]);
	CreateWICTextureFromFile(device, context, L"Textures/GlassVisor_Metallic.png", 0, &metalnessMapSRVs[6]);
	CreateWICTextureFromFile(device, context, L"Textures/GlassVisor_Roughness.png", 0, &roughnessMapSRVs[6]);

	CreateWICTextureFromFile(device, context, L"Textures/IronOld_Albedo.png", 0, &albedoMapSRVs[7]);
	CreateWICTextureFromFile(device, context, L"Textures/IronOld_Normal.png", 0, &normalMapSRVs[7]);
	CreateWICTextureFromFile(device, context, L"Textures/IronOld_Metallic.png", 0, &metalnessMapSRVs[7]);
	CreateWICTextureFromFile(device, context, L"Textures/IronOld_Roughness.png", 0, &roughnessMapSRVs[7]);

	CreateWICTextureFromFile(device, context, L"Textures/Rubber_Albedo.png", 0, &albedoMapSRVs[8]);
	CreateWICTextureFromFile(device, context, L"Textures/Rubber_Normal.png", 0, &normalMapSRVs[8]);
	CreateWICTextureFromFile(device, context, L"Textures/Rubber_Metallic.png", 0, &metalnessMapSRVs[8]);
	CreateWICTextureFromFile(device, context, L"Textures/Rubber_Roughness.png", 0, &roughnessMapSRVs[8]);

	CreateWICTextureFromFile(device, context, L"Textures/Wood_Albedo.png", 0, &albedoMapSRVs[9]);
	CreateWICTextureFromFile(device, context, L"Textures/Wood_Normal.png", 0, &normalMapSRVs[9]);
	CreateWICTextureFromFile(device, context, L"Textures/Wood_Metallic.png", 0, &metalnessMapSRVs[9]);
	CreateWICTextureFromFile(device, context, L"Textures/Wood_Roughness.png", 0, &roughnessMapSRVs[9]);

	CreateWICTextureFromFile(device, context, L"Textures/Gold_Metallic.png", 0, &aoMapSRVs[0]);

	/* Load skybox and irradiance maps */
	ScratchImage hdrScratch;
	LoadFromHDRFile(L"Textures/Winter_Forest/WinterForest_Ref.hdr", nullptr, hdrScratch);
	if (hdrScratch.GetPixelsSize() > 0) CreateShaderResourceView(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(), &hdrEquiSRVs[0]);

	LoadFromTGAFile(L"Textures/Cerberus/Cerberus_A.tga", nullptr, hdrScratch);
	if (hdrScratch.GetPixelsSize() > 0) CreateShaderResourceView(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(), &albedoMapSRVs[10]);

	LoadFromTGAFile(L"Textures/Cerberus/Cerberus_N.tga", nullptr, hdrScratch);
	if (hdrScratch.GetPixelsSize() > 0) CreateShaderResourceView(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(), &normalMapSRVs[10]);

	LoadFromTGAFile(L"Textures/Cerberus/Cerberus_M.tga", nullptr, hdrScratch);
	if (hdrScratch.GetPixelsSize() > 0) CreateShaderResourceView(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(), &metalnessMapSRVs[10]);

	LoadFromTGAFile(L"Textures/Cerberus/Cerberus_R.tga", nullptr, hdrScratch);
	if (hdrScratch.GetPixelsSize() > 0) CreateShaderResourceView(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(), &roughnessMapSRVs[10]);

	LoadFromTGAFile(L"Textures/Cerberus/Cerberus_AO.tga", nullptr, hdrScratch);
	if (hdrScratch.GetPixelsSize() > 0) CreateShaderResourceView(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(), &aoMapSRVs[1]);
}

void Game::CreateGameEntities()
{
	// Make some entities
	GameEntity* cube = new GameEntity(0, 0, 0);
	GameEntity* sphere = new GameEntity(1, 1, 0);
	GameEntity* helix = new GameEntity(2, 3, 0);
	GameEntity* cerberus = new GameEntity(6, 0, 0);
	entities.push_back(cube);
	entities.push_back(cerberus);
	entities.push_back(sphere);
	entities.push_back(helix);


	sphere->SetScale(1.5f, 1.5f, 1.5f);
	helix->SetScale(1.5f, 1.5f, 1.5f);
	cerberus->SetScale(.05f, .05f, .05f);
	cerberus->SetRotation(-1.57f, 0.f, 0.f);

	currentEntity = 0;
}

void Game::ConvertEquisToEnvironments(int hdrInd)
{
	/* Set up the texture to render to */
	ID3D11Texture2D* captureTexture;
	D3D11_TEXTURE2D_DESC captureTextureDesc = {};
	captureTextureDesc.Width = 1024;
	captureTextureDesc.Height = 1024;
	captureTextureDesc.ArraySize = 6;
	captureTextureDesc.MipLevels = 1;
	captureTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	captureTextureDesc.SampleDesc.Count = 1;
	captureTextureDesc.SampleDesc.Quality = 0;
	captureTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	captureTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	captureTextureDesc.CPUAccessFlags = 0;
	captureTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	device->CreateTexture2D(&captureTextureDesc, 0, &captureTexture);

	/* SRV */
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = captureTextureDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = 1;

	/* Set up the render targets */
	ID3D11RenderTargetView* captureRTVs[6];
	D3D11_RENDER_TARGET_VIEW_DESC captureRTVDesc = {};
	captureRTVDesc.Format = captureTextureDesc.Format;
	captureRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	captureRTVDesc.Texture2DArray.MipSlice = 0;
	captureRTVDesc.Texture2DArray.ArraySize = 1;

	/* Viewport */
	D3D11_VIEWPORT captureViewport = {};
	captureViewport.Width = ((FLOAT)captureTextureDesc.Width);
	captureViewport.Height = ((FLOAT)captureTextureDesc.Height);
	captureViewport.MinDepth = 0.0f;
	captureViewport.MaxDepth = 1.0f;
	captureViewport.TopLeftX = 0.0f;
	captureViewport.TopLeftY = 0.0f;

	/* Model and camera data */
	const float color[4] = { 0,0,0,0 };
	XMFLOAT4X4 projection;
	XMStoreFloat4x4(&projection, XMMatrixTranspose(XMMatrixPerspectiveFovLH(1.5708f, 1, 0.1f, 10.0f))); // 90 degrees
	XMFLOAT4X4 captureViews[6];
	XMStoreFloat4x4(&captureViews[0], XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0, 0, 0, 0), XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0))));
	XMStoreFloat4x4(&captureViews[1], XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0, 0, 0, 0), XMVectorSet(0, 0, -1, 0), XMVectorSet(0, 1, 0, 0))));
	XMStoreFloat4x4(&captureViews[2], XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0, 0, 0, 0), XMVectorSet(0, 1, 0, 0), XMVectorSet(1, 0, 0, 0))));
	XMStoreFloat4x4(&captureViews[3], XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0, 0, 0, 0), XMVectorSet(0, -1, 0, 0), XMVectorSet(-1, 0, 0, 0))));
	XMStoreFloat4x4(&captureViews[4], XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0, 0, 0, 0), XMVectorSet(-1, 0, 0, 0), XMVectorSet(0, 1, 0, 0))));
	XMStoreFloat4x4(&captureViews[5], XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0, 0, 0, 0), XMVectorSet(1, 0, 0, 0), XMVectorSet(0, 1, 0, 0))));

	GameEntity* ge = entities[0];
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	Model* model = models[ge->GetModel()];
	// Grab the data from the mesh
	ID3D11Buffer* vb = model->meshes[0]->GetVertexBuffer();
	ID3D11Buffer* ib = model->meshes[0]->GetIndexBuffer();

	for (int i = 0; i < 6; i++)
	{
		captureRTVDesc.Texture2DArray.FirstArraySlice = i; // Create a render target to hold each face
		device->CreateRenderTargetView(captureTexture, &captureRTVDesc, &captureRTVs[i]);

		context->OMSetRenderTargets(1, &captureRTVs[i], 0);
		context->RSSetViewports(1, &captureViewport);
		context->ClearRenderTargetView(captureRTVs[i], color);

		context->IASetVertexBuffers(0, 1, &vb, &stride, &offset); // Set buffers
		context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);

		equirectangularToCubemapVS->SetMatrix4x4("view", captureViews[i]); // Vertex 
		equirectangularToCubemapVS->SetMatrix4x4("projection", projection);
		equirectangularToCubemapVS->CopyAllBufferData();
		equirectangularToCubemapVS->SetShader();

		equirectangularToCubemapPS->SetShaderResourceView("EquirectMap", hdrEquiSRVs[hdrInd]); // Pixel
		equirectangularToCubemapPS->SetSamplerState("BasicSampler", sampler);
		equirectangularToCubemapPS->CopyAllBufferData();
		equirectangularToCubemapPS->SetShader();

		context->RSSetState(skyRasterState);
		context->OMSetDepthStencilState(skyDepthState, 0);

		context->DrawIndexed(model->meshes[0]->GetIndexCount(), 0, 0);
	}

	/* Generate mips then transfer to usable cubemap */
	device->CreateShaderResourceView(captureTexture, &srvDesc, &hdrCubeSRVs[hdrInd]);

	//context->GenerateMips(hdrCubeSRVs[hdrInd]);
	//ScratchImage hdrScratch;
	//CaptureTexture(device, context, captureTexture, hdrScratch);

	//CreateShaderResourceViewEx(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(), 
	//	D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TEXTURECUBE, 
	//	false, &hdrCubeSRVs[hdrInd]);

	/* Clean Up */
	captureTexture->Release();
	captureRTVs[0]->Release();
	captureRTVs[1]->Release();
	captureRTVs[2]->Release();
	captureRTVs[3]->Release();
	captureRTVs[4]->Release();
	captureRTVs[5]->Release();

	/* Compute the environment's irradiance map */
	captureTextureDesc.Width = 256;
	captureTextureDesc.Height = 256;
	captureTextureDesc.MipLevels = 1;
	captureTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	device->CreateTexture2D(&captureTextureDesc, 0, &captureTexture);
	srvDesc.TextureCube.MipLevels = 1;
	captureViewport.Width = ((FLOAT)captureTextureDesc.Width);
	captureViewport.Height = ((FLOAT)captureTextureDesc.Height);

	for (int i = 0; i < 6; i++)
	{
		captureRTVDesc.Texture2DArray.FirstArraySlice = i; // Create a render target to hold each face
		device->CreateRenderTargetView(captureTexture, &captureRTVDesc, &captureRTVs[i]);
		context->OMSetRenderTargets(1, &captureRTVs[i], 0);
		context->RSSetViewports(1, &captureViewport);
		context->ClearRenderTargetView(captureRTVs[i], color);

		context->IASetVertexBuffers(0, 1, &vb, &stride, &offset); // Set buffers
		context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);

		equirectangularToCubemapVS->SetMatrix4x4("view", captureViews[i]); // Vertex 
		equirectangularToCubemapVS->SetMatrix4x4("projection", projection);
		equirectangularToCubemapVS->CopyAllBufferData();
		equirectangularToCubemapVS->SetShader();

		irradianceConvolutionPS->SetShaderResourceView("EnvironmentCubemap", hdrCubeSRVs[hdrInd]); // Pixel
		irradianceConvolutionPS->SetSamplerState("BasicSampler", sampler);
		irradianceConvolutionPS->CopyAllBufferData();
		irradianceConvolutionPS->SetShader();

		context->RSSetState(skyRasterState);
		context->OMSetDepthStencilState(skyDepthState, 0);

		context->DrawIndexed(model->meshes[0]->GetIndexCount(), 0, 0);
	}

	device->CreateShaderResourceView(captureTexture, &srvDesc, &irradianceMapSRVs[hdrInd]);
	//CaptureTexture(device, context, captureTexture, hdrScratch);

	//CreateShaderResourceViewEx(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(),
	//	D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TEXTURECUBE,
	//	false, &irradianceMapSRVs[hdrInd]);

	/* Clean Up */
	captureTexture->Release();
	captureRTVs[0]->Release();
	captureRTVs[1]->Release();
	captureRTVs[2]->Release();
	captureRTVs[3]->Release();
	captureRTVs[4]->Release();
	captureRTVs[5]->Release();

	/* Prefilter for Specular Mips */
	//captureTextureDesc.MipLevels = 5;
	////captureTextureDesc.Width = 256;
	////captureTextureDesc.Height = 256;
	////captureTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	////captureTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	//device->CreateTexture2D(&captureTextureDesc, 0, &captureTexture);

	//for (int m = 0; m < 5; m++)
	//{
	//	captureRTVDesc.Texture2DArray.MipSlice = m;

	//	captureViewport.Width = ((FLOAT)(captureTextureDesc.Width*powf(.5f, ((float)m))));
	//	captureViewport.Height = ((FLOAT)(captureTextureDesc.Height*powf(.5f, ((float)m))));
	//	float roughness = (float)m / 4.f;

	//	for (int i = 0; i < 6; i++)
	//	{
	//		captureRTVDesc.Texture2DArray.FirstArraySlice = i; // Create a render target to hold each face
	//		device->CreateRenderTargetView(captureTexture, &captureRTVDesc, &captureRTVs[i]);
	//		context->OMSetRenderTargets(1, &captureRTVs[i], 0);
	//		context->RSSetViewports(1, &captureViewport);
	//		context->ClearRenderTargetView(captureRTVs[i], color);

	//		context->IASetVertexBuffers(0, 1, &vb, &stride, &offset); // Set buffers
	//		context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);

	//		equirectangularToCubemapVS->SetMatrix4x4("view", captureViews[i]); // Vertex 
	//		equirectangularToCubemapVS->SetMatrix4x4("projection", projection);
	//		equirectangularToCubemapVS->CopyAllBufferData();
	//		equirectangularToCubemapVS->SetShader();

	//		prefilterEnvironmentPS->SetShaderResourceView("EnvironmentCubemap", hdrCubeSRVs[hdrInd]); // Pixel
	//		prefilterEnvironmentPS->SetSamplerState("BasicSampler", sampler);
	//		prefilterEnvironmentPS->CopyAllBufferData();
	//		prefilterEnvironmentPS->SetShader();

	//		context->RSSetState(skyRasterState);
	//		context->OMSetDepthStencilState(skyDepthState, 0);

	//		context->DrawIndexed(model->meshes[0]->GetIndexCount(), 0, 0);
	//	}
	//}

	//device->CreateShaderResourceView(captureTexture, &srvDesc, &envPrefilterSRVs[hdrInd]);
	////CaptureTexture(device, context, captureTexture, hdrScratch);

	////CreateShaderResourceViewEx(device, hdrScratch.GetImages(), hdrScratch.GetImageCount(), hdrScratch.GetMetadata(),
	////	D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TEXTURECUBE,
	////	false, &envPrefilterSRVs[hdrInd]);

	///* Clean Up */
	//captureTexture->Release();
	//captureRTVs[0]->Release();
	//captureRTVs[1]->Release();
	//captureRTVs[2]->Release();
	//captureRTVs[3]->Release();
	//captureRTVs[4]->Release();
	//captureRTVs[5]->Release();
	context->RSSetState(0);
	context->OMSetDepthStencilState(0, 0);
}

void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update the projection matrix assuming the camera exists
	if (camera)
		camera->UpdateProjectionMatrix((float)width / height);
}

void Game::Update(float deltaTime, float totalTime)
{
	// Quit if the escape key is pressed
	if (GetAsyncKeyState(VK_ESCAPE))
		Quit();

	// Update the camera
	camera->Update(deltaTime);

	// Check for entity swap
	bool currentTab = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
	if (currentTab && !prevTab)
		currentEntity = (currentEntity + 1) % entities.size();
	prevTab = currentTab;

	// Spin current entity
	//entities[currentEntity]->Rotate(0, deltaTime * 0.2f, 0);
	
	// Always update current entity's world matrix
	entities[currentEntity]->UpdateWorldMatrix();
}

void Game::Draw(float deltaTime, float totalTime)
{
	// Background color (Black in this case) for clearing
	const float color[4] = { 0,0,0,0 };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	//context->ClearRenderTargetView(backBufferRTV, color);
	//context->ClearDepthStencilView(
	//	depthStencilView,
	//	D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
	//	1.0f,
	//	0);

	// Grab the data from the first entity's mesh
	GameEntity* ge = entities[currentEntity];
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	Model* model = models[ge->GetModel()];
	//for (unsigned int i = 0; i < model->meshes.size(); i++)
	//{
	//	// Grab the data from the mesh
	//	ID3D11Buffer* vb = model->meshes[i]->GetVertexBuffer();
	//	ID3D11Buffer* ib = model->meshes[i]->GetIndexBuffer();

	//	// Set buffers in the input assembler
	//	context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	//	context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);

	//	vertexShader->SetMatrix4x4("world", *ge->GetWorldMatrix());
	//	vertexShader->SetMatrix4x4("view", camera->GetView());
	//	vertexShader->SetMatrix4x4("projection", camera->GetProjection());

	//	vertexShader->CopyAllBufferData();
	//	vertexShader->SetShader();

	//	pixelShader->SetFloat3("LightPos1", XMFLOAT3(2, 0, 0));
	//	pixelShader->SetFloat3("LightPos2", XMFLOAT3(0, 2, 0));
	//	pixelShader->SetFloat3("LightPos3", XMFLOAT3(0, 0, 2));
	//	pixelShader->SetFloat3("LightPos4", XMFLOAT3(0, -2, 0));
	//	pixelShader->SetFloat3("LightColor1", XMFLOAT3(0.95f, 0.95f, 0.95f));
	//	pixelShader->SetFloat3("CameraPosition", camera->GetPosition());

	//	// Send texture-related stuff
	//	int ind = ge->GetTextures();
	//	pixelShader->SetShaderResourceView("AlbedoMap", albedoMapSRVs[ind]);
	//	pixelShader->SetShaderResourceView("NormalMap", normalMapSRVs[ind]);
	//	pixelShader->SetShaderResourceView("MetallicMap", metalnessMapSRVs[ind]);
	//	pixelShader->SetShaderResourceView("RoughnessMap", roughnessMapSRVs[ind]);
	//	pixelShader->SetShaderResourceView("AOMap", aoMapSRVs[0]);
	//	//pixelShader->SetShaderResourceView("EnvIrradianceMap", irradianceMapSRVs[0]);
	//	pixelShader->SetSamplerState("BasicSampler", sampler);

	//	pixelShader->CopyAllBufferData(); // Remember to copy to the GPU!!!!
	//	pixelShader->SetShader();

	//	// Finally do the actual drawing
	//	context->DrawIndexed(model->meshes[i]->GetIndexCount(), 0, 0);
	//}

	// Draw the sky LAST - Ideally, we've set this up so that it
	// only keeps pixels that haven't been "drawn to" yet (ones that
	// have a depth of 1.0)
	//ID3D11Buffer* skyVB = models[0]->meshes[0]->GetVertexBuffer();
	//ID3D11Buffer* skyIB = models[0]->meshes[0]->GetIndexBuffer();

	//// Set the buffers
	//context->IASetVertexBuffers(0, 1, &skyVB, &stride, &offset);
	//context->IASetIndexBuffer(skyIB, DXGI_FORMAT_R32_UINT, 0);

	//// Set up the sky shaders
	//skyVS->SetMatrix4x4("view", camera->GetView());
	//skyVS->SetMatrix4x4("projection", camera->GetProjection());
	//skyVS->CopyAllBufferData();
	//skyVS->SetShader();

	////skyPS->SetShaderResourceView("SkyTexture", hdrCubeSRVs[0]);
	//skyPS->SetSamplerState("BasicSampler", sampler);
	//skyPS->SetShader();

	//// Set up the render state options
	//context->RSSetState(skyRasterState);
	//context->OMSetDepthStencilState(skyDepthState, 0);

	//// Finally do the actual drawing
	//context->DrawIndexed(models[0]->meshes[0]->GetIndexCount(), 0, 0);


	// Reset any states we've changed for the next frame!
	//context->RSSetState(0);
	//context->OMSetDepthStencilState(0, 0);


	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	//swapChain->Present(0, 0);
}


#pragma region Mouse Input

// --------------------------------------------------------
// Helper method for mouse clicking.  We get this information
// from the OS-level messages anyway, so these helpers have
// been created to provide basic mouse input if you want it.
// --------------------------------------------------------
void Game::OnMouseDown(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...

	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;

	// Caputure the mouse so we keep getting mouse move
	// events even if the mouse leaves the window.  we'll be
	// releasing the capture once a mouse button is released
	SetCapture(hWnd);
}

// --------------------------------------------------------
// Helper method for mouse release
// --------------------------------------------------------
void Game::OnMouseUp(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...

	// We don't care about the tracking the cursor outside
	// the window anymore (we're not dragging if the mouse is up)
	ReleaseCapture();
}

// --------------------------------------------------------
// Helper method for mouse movement.  We only get this message
// if the mouse is currently over the window, or if we're 
// currently capturing the mouse.
// --------------------------------------------------------
void Game::OnMouseMove(WPARAM buttonState, int x, int y)
{
	// Check left mouse button
	if (buttonState & 0x0001)
	{
		float xDiff = (x - prevMousePos.x) * 0.005f;
		float yDiff = (y - prevMousePos.y) * 0.005f;
		camera->Rotate(yDiff, xDiff);
	}

	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;
}

// --------------------------------------------------------
// Helper method for mouse wheel scrolling.  
// WheelDelta may be positive or negative, depending 
// on the direction of the scroll
// --------------------------------------------------------
void Game::OnMouseWheel(float wheelDelta, int x, int y)
{
	// Add any custom code here...
}
#pragma endregion