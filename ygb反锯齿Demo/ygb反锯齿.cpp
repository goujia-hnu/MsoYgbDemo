//--------------------------------------------------------------------------------------
//
//	文件名		：	ygb反锯齿.cpp
//	创建者		：	goujia@wps.cn
//	创建时间		：	2017-12-25 13:14:00
//	功能描述		：	WPP_DX11墨迹反锯齿DEMO
//
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include <vector>
#include <list>
#include <tchar.h>
#include <iostream>
#include "resource.h"
#include "stdio.h"
using namespace DirectX;
using namespace std;
struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Equation;//ax+by+c<  d
};
struct Point
{
	int x;
	int y;
	Point(int xx, int yy)
	{
		x = xx;
		y = yy;
	}
	Point() {}
};
struct ConstantYgbColor
{
	XMFLOAT4 Color;
};
struct InkCache
{
	vector<SimpleVertex>point;
	vector<WORD>indices;
};
HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice = nullptr;
ID3D11Device1*          g_pd3dDevice1 = nullptr;
ID3D11DeviceContext*    g_pImmediateContext = nullptr;
ID3D11DeviceContext1*   g_pImmediateContext1 = nullptr;
IDXGISwapChain*         g_pSwapChain = nullptr;
IDXGISwapChain1*        g_pSwapChain1 = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader*     g_pVertexShader = nullptr;
ID3D11PixelShader*      g_pPixelShader = nullptr;
ID3D11InputLayout*      g_pVertexLayout = nullptr;
ID3D11Buffer*           g_pVertexBuffer = nullptr;
ID3D11Buffer*			g_pIndexBuffer = nullptr;
ID3D11Buffer*           g_pConstantColor = nullptr;
ID3D11BlendState*		g_pBlendState = nullptr;

list<InkCache>inkcache;
RECT rc;//客户区大小
int width, height;//客户区宽度 高度
bool isPress = false;//左键是否按下
D3D11_BUFFER_DESC Vertexbuffer;
D3D11_BUFFER_DESC Indexbuffer;
Point pt, pre;//鼠标坐标点
DWORD NumPoint = 0;//鼠标坐标点个数
float Hygb = 3, Wygb = 3;
int preState = 0;//0 1 2 3:右上 右下 左上 左下
Point p1, p2;//前面两个点

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void UpdatePoint(vector<SimpleVertex>& vertex, vector<WORD>& indices);
void Render();
XMFLOAT2 CalInterSection(float s1, float t1, float s2, float t2, float s3, float t3, float s4, float t4, int flages);
XMFLOAT2 CalInterSection(Point p1, Point p2, Point p3, Point p4, int flages);
XMFLOAT4 CalLineEquation(Point p1, Point p2);
float dist(Point p1, Point p2);
float distance(Point p, XMFLOAT4 e);
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;
	if (FAILED(InitDevice())) {
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			Render();
		}
	}
	CleanupDevice();
	return (int)msg.wParam;
}

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(hInstance, (LPCWSTR)IDC_CURSOR1);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"YgbWindowClass";
	wcex.hIconSm = NULL;
	if (!RegisterClassEx(&wcex))
		return E_FAIL;
	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
	AdjustWindowRect(&rc, WS_MAXIMIZE, TRUE);
	g_hWnd = CreateWindow(L"YgbWindowClass", L"WPP-ygb-D3D11-反锯齿",
		WS_POPUP | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	if (!g_hWnd)
		return E_FAIL;
	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);
	return S_OK;
}

HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();
	return S_OK;
}

HRESULT InitDevice()
{
	HRESULT hr = S_OK;
	GetClientRect(g_hWnd, &rc);
	width = rc.right - rc.left;
	height = rc.bottom - rc.top;
	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);
	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		}
		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;
	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;
	// 检查一下支持的MSAA倍数
	UINT sampleCount, sampleQuality = 0;
	//static const UINT ExpectedSampleCount[] = { 4, 2, 1 };
	//bool bMSAA_Available = false;
	//for (int i = 0; i < _countof(ExpectedSampleCount); i++)
	//{
	//	sampleCount = ExpectedSampleCount[i];
	//	hr = g_pd3dDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_B8G8R8A8_UNORM, sampleCount, &sampleQuality);
	//	if (sampleQuality != 0)
	//	{
	//		bMSAA_Available = true;
	//		break;
	//	}
	//}
	//if (!bMSAA_Available)
	//{
	sampleCount = 1;
	sampleQuality = 0;
	/*}
	else
	{
	sampleQuality--;
	assert(sampleQuality >= 0);
	}*/
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		// DirectX 11.1 or later
		hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1));
		if (SUCCEEDED(hr))
		{
			(void)g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1));
		}
		DXGI_SWAP_CHAIN_DESC1 sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = sampleCount;
		sd.SampleDesc.Quality = sampleQuality;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;
		hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1);
		if (SUCCEEDED(hr)) {
			hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
		}
		dxgiFactory2->Release();
	}
	else {
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = g_hWnd;
		sd.SampleDesc.Count = sampleCount;
		sd.SampleDesc.Quality = sampleQuality;
		sd.Windowed = TRUE;
		hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
	}
	// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);
	dxgiFactory->Release();
	if (FAILED(hr))
		return hr;
	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;
	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = g_pd3dDevice->CreateBlendState(&blendDesc, &g_pBlendState);
	if (FAILED(hr))
		return hr;

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);
	// Compile the vertex shader
	ID3DBlob* pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"Shader.fx", "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr, L"The FX file cannot be compiled.Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}
	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"Shader.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr, L"The FX file cannot be compiled.Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}
	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	//动态顶点缓冲区
	ZeroMemory(&Vertexbuffer, sizeof(Vertexbuffer));
	Vertexbuffer.Usage = D3D11_USAGE_DYNAMIC;
	Vertexbuffer.ByteWidth = sizeof(SimpleVertex) * 20000;
	Vertexbuffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Vertexbuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = g_pd3dDevice->CreateBuffer(&Vertexbuffer, NULL, &g_pVertexBuffer);
	if (FAILED(hr))
		return hr;
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	//创建动态顶点索引
	ZeroMemory(&Indexbuffer, sizeof(Indexbuffer));
	Indexbuffer.Usage = D3D11_USAGE_DYNAMIC;
	Indexbuffer.ByteWidth = sizeof(DWORD) * 30000;
	Indexbuffer.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Indexbuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = g_pd3dDevice->CreateBuffer(&Indexbuffer, NULL, &g_pIndexBuffer);
	if (FAILED(hr))
		return hr;
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D11_BUFFER_DESC bd;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantYgbColor);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantColor);
	if (FAILED(hr))
		return hr;
}

void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pVertexLayout) g_pVertexLayout->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain1) g_pSwapChain1->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pConstantColor)g_pConstantColor->Release();
	if (g_pImmediateContext1) g_pImmediateContext1->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pBlendState)g_pBlendState->Release();
	if (g_pd3dDevice1) g_pd3dDevice1->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();

	list<InkCache>::iterator it = inkcache.begin();
	for (; it != inkcache.end(); it++)
	{
		it->indices.clear();
		it->point.clear();
	}
	inkcache.clear();
}

void UpdatePoint(vector<SimpleVertex>& vertex, vector<WORD>& indices)
{
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE vertexData;
	hr = g_pImmediateContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vertexData);
	if (FAILED(hr))
		return;
	SimpleVertex*v = reinterpret_cast<SimpleVertex*>(vertexData.pData);
	for (UINT i = 0; i < vertex.size(); i++)
		v[i] = vertex.at(i);
	g_pImmediateContext->Unmap(g_pVertexBuffer, 0);
	D3D11_MAPPED_SUBRESOURCE indexData;
	hr = g_pImmediateContext->Map(g_pIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &indexData);
	if (FAILED(hr))
		return;
	WORD*index = reinterpret_cast<WORD*>(indexData.pData);
	for (UINT i = 0; i < indices.size(); i++)
		index[i] = indices.at(i);
	g_pImmediateContext->Unmap(g_pIndexBuffer, 0);
}

//补三角形最终完善版
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	SimpleVertex temp;
	InkCache ink;
	list<InkCache>::iterator it;
	XMFLOAT4 line;
	switch (message)
	{
	case WM_KEYDOWN://按下键盘消息
		switch (wParam)
		{
		case VK_ESCAPE:           //按下Esc键  
			PostQuitMessage(0);
			break;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_LBUTTONDOWN://按下左键
		SetCapture(hWnd);
		isPress = true;
		pt.x = LOWORD(lParam);
		pt.y = HIWORD(lParam);
		float x[4], y[4];
		x[0] = pt.x - Wygb; y[0] = pt.y - Hygb;
		x[1] = pt.x - Wygb; y[1] = pt.y + Hygb;
		x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
		x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
		for (UINT i = 0; i < 4; i++)
		{
			temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
			temp.Equation = XMFLOAT4(pt.x, pt.y, Hygb, -1.0);
			ink.point.push_back(temp);
		}
		ink.indices.push_back(1); ink.indices.push_back(0); ink.indices.push_back(2);
		ink.indices.push_back(1); ink.indices.push_back(2); ink.indices.push_back(3);
		p1 = pt;
		p2 = pt;
		NumPoint = 3;
		inkcache.push_back(ink);
		preState = 0;
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		isPress = false;
		it = inkcache.end();
		it--;
		x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
		x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
		x[2] = p1.x + Wygb; y[2] = p1.y - Hygb;
		x[3] = p1.x + Wygb; y[3] = p1.y + Hygb;
		for (UINT i = 0; i < 4; i++)
		{
			temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
			temp.Equation = XMFLOAT4(p1.x, p1.y, Hygb, -1.0);
			it->point.push_back(temp);
		}
		it->indices.push_back(2 + NumPoint); it->indices.push_back(1 + NumPoint); it->indices.push_back(3 + NumPoint);
		it->indices.push_back(2 + NumPoint); it->indices.push_back(3 + NumPoint); it->indices.push_back(4 + NumPoint);
		NumPoint = 0;
		break;
	case WM_MOUSEMOVE://鼠标移动
		if (isPress)//记录坐标值
		{
			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);
			if (abs(pt.x - p1.x) <= 2.5 * Wygb && abs(pt.y - p1.y) <= 2.5 * Hygb)
				return 0;
			it = inkcache.end();
			it--;
			vector<SimpleVertex>& vertex = it->point;
			vector<WORD>& indices = it->indices;
			float x[4], y[4], X0, Y0;
			line = CalLineEquation(p1, pt);
			if (pt.x >= p1.x)//向右画线&&垂直上下
			{
				if (pt.y <= p1.y)//右上角 垂直向上
				{
					if (preState == 1 && NumPoint>3)//右下到右上
					{
						Point P1(p2.x + Wygb, p2.y - Hygb), P2(p1.x + Wygb, p1.y - Hygb), P3(p1.x - Wygb, p1.y - Hygb), P4(pt.x - Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						vertex.pop_back(); vertex.pop_back();
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						line = iter->Equation;
						x[0] = X0; y[0] = Y0;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						float AF = distance(Point(X0, Y0), line);
						x[2] = p1.x + Wygb; y[2] = p1.y + Hygb;
						line = CalLineEquation(Point((x[0] + x[2]) / 2, (y[0] + y[2]) / 2), Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2));
						float AE = distance(Point(X0, Y0), line);
						float h = Hygb*AE / AF;line.w = h;
						for (UINT i = 0; i < 3; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						indices.push_back(NumPoint + 2); indices.push_back(NumPoint + 1); indices.push_back(NumPoint + 3);
						NumPoint += 3;
						x[0] = X0; y[0] = Y0;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
						x[2] = pt.x - Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
						line = CalLineEquation(p1/*Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2)*/, pt);
					}
					else if (preState == 2 && NumPoint>3)//左上到右上
					{
						Point P1(p2.x + Wygb, p2.y - Hygb), P2(p1.x + Wygb, p1.y - Hygb), P3(p1.x + Wygb, p1.y + Hygb), P4(pt.x + Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						vertex.pop_back(); vertex.pop_back();
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						line = iter->Equation;
						x[0] = X0; y[0] = Y0;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						float AF = distance(Point(X0, Y0), line);
						x[2] = p1.x - Wygb; y[2] = p1.y - Hygb;
						line = CalLineEquation(Point((x[0] + x[2]) / 2, (y[0] + y[2]) / 2), Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2));
						float AE = distance(Point(X0, Y0), line);
						float h = Hygb*AE / AF; line.w = h;
						for (UINT i = 0; i < 3; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						indices.push_back(NumPoint + 1); indices.push_back(NumPoint + 2); indices.push_back(NumPoint + 3);
						NumPoint += 3;
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = X0; y[1] = Y0;
						x[2] = pt.x - Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
						line = CalLineEquation(/*Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2)*/p1, pt);
					}
					else
					{
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
						x[2] = pt.x - Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
					}
					indices.push_back(2 + NumPoint); indices.push_back(1 + NumPoint); indices.push_back(3 + NumPoint);
					indices.push_back(2 + NumPoint); indices.push_back(3 + NumPoint); indices.push_back(4 + NumPoint);
					NumPoint += 4;
					preState = 0;
				}
				else//右下角 垂直向下
				{
					if (preState == 0 && NumPoint>3)//右上到右下
					{
						Point P1(p2.x + Wygb, p2.y + Hygb), P2(p1.x + Wygb, p1.y + Hygb), P3(p1.x - Wygb, p1.y + Hygb), P4(pt.x - Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						vertex.pop_back(); vertex.pop_back();
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						line = iter->Equation;
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = X0; y[1] = Y0;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						float AF = distance(Point(X0, Y0), line);
						x[2] = p1.x + Wygb; y[2] = p1.y - Hygb;
						line = CalLineEquation(Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2), Point((x[2] + x[1]) / 2, (y[2] + y[1]) / 2));
						float AE = distance(Point(X0, Y0), line);
						float h = Hygb*AE / AF; line.w = h;
						for (UINT i = 0; i < 3; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						indices.push_back(NumPoint + 2); indices.push_back(NumPoint + 1); indices.push_back(NumPoint + 3);
						NumPoint += 3;
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = X0; y[1] = Y0;
						x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
						line = CalLineEquation(/*Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2)*/p1, pt);
					}
					else if (preState == 3 && NumPoint>3)//左下到右下
					{
						Point P1(p2.x + Wygb, p2.y + Hygb), P2(p1.x + Wygb, p1.y + Hygb), P3(p1.x + Wygb, p1.y - Hygb), P4(pt.x + Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						vertex.pop_back(); vertex.pop_back();
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						line = iter->Equation;
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = X0; y[1] = Y0;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}

						float AF = distance(Point(X0, Y0), line);
						x[2] = p1.x - Wygb; y[2] = p1.y + Hygb;
						line = CalLineEquation(Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2), Point((x[2] + x[1]) / 2, (y[2] + y[1]) / 2));
						float AE = distance(Point(X0, Y0), line);
						float h = Hygb*AE / AF; line.w = h;
						for (UINT i = 0; i < 3; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						indices.push_back(NumPoint + 1); indices.push_back(NumPoint + 2); indices.push_back(NumPoint + 3);
						NumPoint += 3;	
						x[0] = X0; y[0] = Y0;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
						x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
						line = CalLineEquation(/*Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2)*/p1, pt);
					}
					else
					{
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
						x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
					}
					indices.push_back(2 + NumPoint); indices.push_back(1 + NumPoint); indices.push_back(3 + NumPoint);
					indices.push_back(2 + NumPoint); indices.push_back(3 + NumPoint); indices.push_back(4 + NumPoint);
					NumPoint += 4;
					preState = 1;
				}
			}
			else//向左画线
			{
				if (pt.y < p1.y)//左上角
				{
					if (preState == 3 && NumPoint>3)//左下到左上
					{
						Point P1(p2.x - Wygb, p2.y - Hygb), P2(p1.x - Wygb, p1.y - Hygb), P3(p1.x + Wygb, p1.y - Hygb), P4(pt.x + Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						vertex.pop_back(); vertex.pop_back();
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						line = iter->Equation;
						x[0] = X0; y[0] = Y0;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						float AF = distance(Point(X0, Y0), line);
						x[2] = p1.x - Wygb; y[2] = p1.y + Hygb;
						line = CalLineEquation(Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2), Point((x[2] + x[0]) / 2, (y[2] + y[0]) / 2));
						float AE = distance(Point(X0, Y0), line);
						float h = Hygb*AE / AF; line.w = h;
						for (UINT i = 0; i < 3; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						indices.push_back(NumPoint + 1); indices.push_back(NumPoint + 2); indices.push_back(NumPoint + 3);
						NumPoint += 3;
						x[0] = X0; y[0] = Y0;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
						x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
						line = CalLineEquation(/*Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2)*/p1, pt);
					}
					else if (preState == 0 && NumPoint>3)//右上到左上
					{
						Point P1(p2.x - Wygb, p2.y - Hygb), P2(p1.x - Wygb, p1.y - Hygb), P3(p1.x - Wygb, p1.y + Hygb), P4(pt.x - Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						vertex.pop_back(); vertex.pop_back();
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						line = iter->Equation;
						x[0] = X0; y[0] = Y0;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						float AF = distance(Point(X0, Y0), line);
						x[2] = p1.x + Wygb; y[2] = p1.y - Hygb;
						line = CalLineEquation(Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2), Point((x[2] + x[0]) / 2, (y[2] + y[0]) / 2));
						float AE = distance(Point(X0, Y0), line);
						float h = Hygb*AE / AF; line.w = h;
						for (UINT i = 0; i < 3; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						indices.push_back(NumPoint + 2); indices.push_back(NumPoint + 1); indices.push_back(NumPoint + 3);
						NumPoint += 3;
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = X0; y[1] = Y0;
						x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
						line = CalLineEquation(/*Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2)*/p1, pt);
					}
					else
					{
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
						x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
					}
					indices.push_back(1 + NumPoint); indices.push_back(2 + NumPoint); indices.push_back(3 + NumPoint);
					indices.push_back(2 + NumPoint); indices.push_back(4 + NumPoint); indices.push_back(3 + NumPoint);
					NumPoint += 4;
					preState = 2;
				}
				else//左下角 
				{
					if (preState == 2 && NumPoint>3)//左上到左下
					{
						Point P1(p2.x - Wygb, p2.y + Hygb), P2(p1.x - Wygb, p1.y + Hygb), P3(p1.x + Wygb, p1.y + Hygb), P4(pt.x + Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						vertex.pop_back(); vertex.pop_back();
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						line = iter->Equation;
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = X0; y[1] = Y0;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						float AF = distance(Point(X0, Y0), line);
						x[2] = p1.x - Wygb; y[2] = p1.y - Hygb;
						line = CalLineEquation(Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2), Point((x[2] + x[1]) / 2, (y[2] + y[1]) / 2));
						float AE = distance(Point(X0, Y0), line);
						float h = Hygb*AE / AF; line.w = h;
						for (UINT i = 0; i < 3; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						indices.push_back(NumPoint + 1); indices.push_back(NumPoint + 2); indices.push_back(NumPoint + 3);
						NumPoint += 3;
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = X0; y[1] = Y0;
						x[2] = pt.x - Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
						line = CalLineEquation(/*Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2)*/p1, pt);
					}
					else if (preState == 1 && NumPoint>3)//右下到左下
					{
						Point P1(p2.x - Wygb, p2.y + Hygb), P2(p1.x - Wygb, p1.y + Hygb), P3(p1.x - Wygb, p1.y - Hygb), P4(pt.x - Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						vertex.pop_back(); vertex.pop_back();
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						line = iter->Equation;
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = X0; y[1] = Y0;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						float AF = distance(Point(X0, Y0), line);
						x[2] = p1.x + Wygb; y[2] = p1.y + Hygb;
						line = CalLineEquation(Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2), Point((x[2] + x[1]) / 2, (y[2] + y[1]) / 2));
						float AE = distance(Point(X0, Y0), line);
						float h = Hygb*AE / AF; line.w = h;
						for (UINT i = 0; i < 3; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						indices.push_back(NumPoint + 2); indices.push_back(NumPoint + 1); indices.push_back(NumPoint + 3);
						NumPoint += 3;
						x[0] = X0; y[0] = Y0;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
						x[2] = pt.x - Wygb; y[2] = pt.y - Hygb;
						x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
						line = CalLineEquation(/*Point((x[0] + x[1]) / 2, (y[0] + y[1]) / 2)*/p1, pt);
					}
					else
					{
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb; x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
						x[2] = pt.x - Wygb; y[2] = pt.y - Hygb; x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
					}
					indices.push_back(1 + NumPoint); indices.push_back(2 + NumPoint); indices.push_back(3 + NumPoint);
					indices.push_back(2 + NumPoint); indices.push_back(4 + NumPoint); indices.push_back(3 + NumPoint);
					NumPoint += 4;
					preState = 3;
				}
			}
			for (UINT i = 0; i < 4; i++)
			{
				temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
				temp.Equation = line;
				vertex.push_back(temp);
			}
			p2 = p1; p1 = pt;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
//不补三角形加强版版
LRESULT CALLBACK WndProc0(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	SimpleVertex temp;
	InkCache ink;
	XMFLOAT4 line;
	list<InkCache>::iterator it;
	switch (message)
	{
	case WM_KEYDOWN://按下键盘消息
		switch (wParam)
		{
		case VK_ESCAPE:           //按下Esc键  
			PostQuitMessage(0);
			break;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_LBUTTONDOWN://按下左键
		SetCapture(hWnd);
		isPress = true;
		pt.x = LOWORD(lParam);
		pt.y = HIWORD(lParam);
		float x[4], y[4];
		x[0] = pt.x - Wygb; y[0] = pt.y - Hygb;
		x[1] = pt.x - Wygb; y[1] = pt.y + Hygb;
		x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
		x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
		for (UINT i = 0; i < 4; i++)
		{
			temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
			temp.Equation = XMFLOAT4(pt.x, pt.y, Hygb, -1.0);
			ink.point.push_back(temp);
		}
		NumPoint++;
		ink.indices.push_back(1); ink.indices.push_back(0); ink.indices.push_back(2);
		ink.indices.push_back(1); ink.indices.push_back(2); ink.indices.push_back(3);
		p1 = pt; p2 = pt;
		inkcache.push_back(ink);
		preState = 0;
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		isPress = false;
		it = inkcache.end();
		it--;
		x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
		x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
		x[2] = p1.x + Wygb; y[2] = p1.y - Hygb;
		x[3] = p1.x + Wygb; y[3] = p1.y + Hygb;
		for (UINT i = 0; i < 4; i++)
		{
			temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
			temp.Equation = XMFLOAT4(p1.x, p1.y, Hygb, -1.0);
			it->point.push_back(temp);
		}
		NumPoint++;
		it->indices.push_back(5 + (NumPoint - 2) * 4); it->indices.push_back(4 + (NumPoint - 2) * 4); it->indices.push_back(6 + (NumPoint - 2) * 4);
		it->indices.push_back(5 + (NumPoint - 2) * 4); it->indices.push_back(6 + (NumPoint - 2) * 4); it->indices.push_back(7 + (NumPoint - 2) * 4);
		NumPoint = 0;
		break;
	case WM_MOUSEMOVE://鼠标移动
		if (isPress)//记录坐标值
		{
			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);
			if (abs(pt.x - p1.x) < 2.0 * Wygb && abs(pt.y - p1.y) < 2.0 * Hygb)
				return 0;
			it = inkcache.end();
			it--;
			vector<SimpleVertex>& vertex = it->point;
			vector<WORD>& indices = it->indices;
			float x[4], y[4], X0, Y0, X1, Y1;
			NumPoint++;
			if (pt.x >= p1.x)//向右画线&&垂直上下
			{
				if (pt.y <= p1.y)//右上角 垂直向上 水平向右
				{
					if (preState == 1 && NumPoint>2)//右下到右上
					{
						Point P1(p2.x + Wygb, p2.y - Hygb), P2(p1.x + Wygb, p1.y - Hygb), P3(p1.x - Wygb, p1.y - Hygb), P4(pt.x - Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						X1 = (p2.x == p1.x) ? (p1.x - Wygb) : p1.x;
						Y1 = p1.y + Hygb;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vertex.pop_back(); vertex.pop_back();
						line = CalLineEquation(p2, Point((X0 + X1) / 2, (Y0 + Y1) / 2));
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						iter->Equation = line; iter--;
						iter->Equation = line;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						line = CalLineEquation(Point((X0 + X1) / 2, (Y0 + Y1) / 2), pt);
						p2 = Point((X0 + X1) / 2, (Y0 + Y1) / 2);
					}
					else if (preState == 2 && NumPoint>2)//左上到右上
					{
						Point P1(p2.x + Wygb, p2.y - Hygb), P2(p1.x + Wygb, p1.y - Hygb), P3(p1.x + Wygb, p1.y + Hygb), P4(pt.x + Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						X1 = p1.x - Wygb; Y1 = p1.y;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vertex.pop_back(); vertex.pop_back();
						line = CalLineEquation(p2, Point((X0 + X1) / 2, (Y0 + Y1) / 2));
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						iter->Equation = line; iter--;
						iter->Equation = line;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						line = CalLineEquation(Point((X0 + X1) / 2, (Y0 + Y1) / 2), pt);
						swap(x[0], x[1]); swap(y[0], y[1]);
						p2 = Point((X0 + X1) / 2, (Y0 + Y1) / 2);
					}
					else
					{
						line = CalLineEquation(p1, pt);
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
						p2 = p1;
					}
					x[2] = pt.x - Wygb; y[2] = pt.y - Hygb;
					x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4);
					preState = 0;
				}
				else//右下角 垂直向下
				{
					if (preState == 0 && NumPoint>2)//右上到右下
					{
						X0 = p1.x; Y0 = p1.y - Hygb;
						Point P1(p2.x + Wygb, p2.y + Hygb), P2(p1.x + Wygb, p1.y + Hygb), P3(p1.x - Wygb, p1.y + Hygb), P4(pt.x - Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X1 = point.x; Y1 = point.y;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vertex.pop_back(); vertex.pop_back();
						line = CalLineEquation(p2, Point((X0 + X1) / 2, (Y0 + Y1) / 2));
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						iter->Equation = line;
						iter--; iter->Equation = line;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						line = CalLineEquation(Point((X0 + X1) / 2, (Y0 + Y1) / 2), pt);
						p2 = Point((X0 + X1) / 2, (Y0 + Y1) / 2);
					}
					else if (preState == 3 && NumPoint>2)//左下到右下
					{
						X0 = p1.x - Wygb; Y0 = p1.y;
						Point P1(p2.x + Wygb, p2.y + Hygb), P2(p1.x + Wygb, p1.y + Hygb), P3(p1.x + Wygb, p1.y - Hygb), P4(pt.x + Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X1 = point.x; Y1 = point.y;
						x[0] = X0; y[0] = Y0;
						x[1] = X1; y[1] = Y1;
						vertex.pop_back(); vertex.pop_back();
						line = CalLineEquation(p2, Point((X0 + X1) / 2, (Y0 + Y1) / 2));
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						iter->Equation = line;
						iter--; iter->Equation = line;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						line = CalLineEquation(Point((X0 + X1) / 2, (Y0 + Y1) / 2), pt);
						swap(x[0], x[1]); swap(y[0], y[1]);
						p2 = Point((X0 + X1) / 2, (Y0 + Y1) / 2);
					}
					else
					{
						line = CalLineEquation(p1, pt);
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
						p2 = p1;
					}
					x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
					x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4);
					preState = 1;
				}
			}
			else//向左画线
			{
				if (pt.y < p1.y)//左上角
				{
					if (preState == 3 && NumPoint>2)//左下到左上
					{
						Point P1(p2.x - Wygb, p2.y - Hygb), P2(p1.x - Wygb, p1.y - Hygb), P3(p1.x + Wygb, p1.y - Hygb), P4(pt.x + Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						X1 = p1.x; Y1 = p1.y + Hygb;
						x[0] = X0; y[0] = Y0;
						x[1] = X1; y[1] = Y1;
						vertex.pop_back(); vertex.pop_back();
						line = CalLineEquation(p2, Point((X0 + X1) / 2, (Y0 + Y1) / 2));
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						iter->Equation = line;
						iter--; iter->Equation = line;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						line = CalLineEquation(Point((X0 + X1) / 2, (Y0 + Y1) / 2), pt);
						p2 = Point((X0 + X1) / 2, (Y0 + Y1) / 2);
					}
					else if (preState == 0 && NumPoint>2)//右上到左上
					{
						Point P1(p2.x - Wygb, p2.y - Hygb), P2(p1.x - Wygb, p1.y - Hygb), P3(p1.x - Wygb, p1.y + Hygb), P4(pt.x - Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						X1 = p1.x + Wygb; Y1 = p1.y;
						x[0] = X0; y[0] = Y0;
						x[1] = X1; y[1] = Y1;
						vertex.pop_back(); vertex.pop_back();
						line = CalLineEquation(p2, Point((X0 + X1) / 2, (Y0 + Y1) / 2));
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						iter->Equation = line;
						iter--; iter->Equation = line;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						line = CalLineEquation(Point((X0 + X1) / 2, (Y0 + Y1) / 2), pt);
						swap(x[0], x[1]); swap(y[0], y[1]);
						p2 = Point((X0 + X1) / 2, (Y0 + Y1) / 2);
					}
					else
					{
						line = CalLineEquation(p1, pt);
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
						p2 = p1;
					}
					x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
					x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
					indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					preState = 2;
				}
				else//左下角 
				{
					if (preState == 2 && NumPoint>2)//左上到左下
					{
						X0 = p1.x; Y0 = p1.y - Hygb;
						Point P1(p2.x - Wygb, p2.y + Hygb), P2(p1.x - Wygb, p1.y + Hygb), P3(p1.x + Wygb, p1.y + Hygb), P4(pt.x + Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X1 = point.x; Y1 = point.y;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vertex.pop_back(); vertex.pop_back();
						line = CalLineEquation(p2, Point((X0 + X1) / 2, (Y0 + Y1) / 2));
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						iter->Equation = line;
						iter--; iter->Equation = line;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						line = CalLineEquation(Point((X0 + X1) / 2, (Y0 + Y1) / 2), pt);
						p2 = Point((X0 + X1) / 2, (Y0 + Y1) / 2);
					}
					else if (preState == 1 && NumPoint>2)//右下到左下
					{
						Y0 = (p1.x == p2.x) ? p1.y + Hygb : p1.y;
						X0 = p1.x + Wygb;
						Point P1(p2.x - Wygb, p2.y + Hygb), P2(p1.x - Wygb, p1.y + Hygb), P3(p1.x - Wygb, p1.y - Hygb), P4(pt.x - Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X1 = point.x; Y1 = point.y;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vertex.pop_back(); vertex.pop_back();
						line = CalLineEquation(p2, Point((X0 + X1) / 2, (Y0 + Y1) / 2));
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						iter->Equation = line;
						iter--; iter->Equation = line;
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = line;
							vertex.push_back(temp);
						}
						line = CalLineEquation(Point((X0 + X1) / 2, (Y0 + Y1) / 2), pt);
						swap(x[0], x[1]); swap(y[0], y[1]);
						p2 = Point((X0 + X1) / 2, (Y0 + Y1) / 2);
					}
					else
					{
						line = CalLineEquation(p1, pt);
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
						p2 = p1;
					}
					x[2] = pt.x - Wygb; y[2] = pt.y - Hygb; x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
					indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					preState = 3;
				}
			}
			for (UINT i = 0; i < 4; i++)
			{
				temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
				temp.Equation = line;
				vertex.push_back(temp);
			}
			p1 = pt;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
//不补三角形初版
LRESULT CALLBACK WndProc1(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	SimpleVertex temp;
	InkCache ink;
	XMFLOAT4 line;
	list<InkCache>::iterator it;
	switch (message)
	{
	case WM_KEYDOWN://按下键盘消息
		switch (wParam)
		{
		case VK_ESCAPE:           //按下Esc键  
			PostQuitMessage(0);
			break;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_LBUTTONDOWN://按下左键
		SetCapture(hWnd);
		isPress = true;
		pt.x = LOWORD(lParam);
		pt.y = HIWORD(lParam);
		float x[4], y[4];
		x[0] = pt.x - Wygb; y[0] = pt.y - Hygb;
		x[1] = pt.x - Wygb; y[1] = pt.y + Hygb;
		x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
		x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
		for (UINT i = 0; i < 4; i++)
		{
			temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
			temp.Equation = XMFLOAT4(pt.x, pt.y, Hygb, -1.0);
			ink.point.push_back(temp);
		}
		NumPoint++;
		ink.indices.push_back(1); ink.indices.push_back(0); ink.indices.push_back(2);
		ink.indices.push_back(1); ink.indices.push_back(2); ink.indices.push_back(3);
		p1 = pt; p2 = pt;
		inkcache.push_back(ink);
		preState = 0;
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		isPress = false;
		it = inkcache.end();
		it--;
		x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
		x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
		x[2] = p1.x + Wygb; y[2] = p1.y - Hygb;
		x[3] = p1.x + Wygb; y[3] = p1.y + Hygb;
		for (UINT i = 0; i < 4; i++)
		{
			temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
			temp.Equation = XMFLOAT4(p1.x, p1.y, Hygb, -1.0);
			it->point.push_back(temp);
		}
		NumPoint++;
		it->indices.push_back(5 + (NumPoint - 2) * 4); it->indices.push_back(4 + (NumPoint - 2) * 4); it->indices.push_back(6 + (NumPoint - 2) * 4);
		it->indices.push_back(5 + (NumPoint - 2) * 4); it->indices.push_back(6 + (NumPoint - 2) * 4); it->indices.push_back(7 + (NumPoint - 2) * 4);
		NumPoint = 0;
		break;
	case WM_MOUSEMOVE://鼠标移动
		if (isPress)//记录坐标值
		{
			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);
			if (abs(pt.x - p1.x) < 2.0 * Wygb && abs(pt.y - p1.y) < 2.0 * Hygb)
				return 0;
			it = inkcache.end();
			it--;
			vector<SimpleVertex>& vertex = it->point;
			vector<WORD>& indices = it->indices;
			float x[4], y[4], X0, Y0, X1, Y1;
			line = CalLineEquation(p1, pt);
			NumPoint++;
			if (pt.x >= p1.x)//向右画线&&垂直上下
			{
				if (pt.y <= p1.y)//右上角 垂直向上 水平向右
				{
					if (preState == 1 && NumPoint>2)//右下到右上
					{
						Point P1(p2.x + Wygb, p2.y - Hygb), P2(p1.x + Wygb, p1.y - Hygb), P3(p1.x - Wygb, p1.y - Hygb), P4(pt.x - Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						if (p2.x == p1.x)
							X1 = p1.x - Wygb;
						else
							X1 = p1.x;
						Y1 = p1.y + Hygb;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						temp = *iter;
						XMFLOAT4 equation = temp.Equation;
						vertex.pop_back(); vertex.pop_back();
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = equation;
							vertex.push_back(temp);
						}
					}
					else if (preState == 2 && NumPoint>2)//左上到右上
					{
						Point P1(p2.x + Wygb, p2.y - Hygb), P2(p1.x + Wygb, p1.y - Hygb), P3(p1.x + Wygb, p1.y + Hygb), P4(pt.x + Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						X1 = p1.x - Wygb; Y1 = p1.y;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						temp = *iter;
						XMFLOAT4 equation = temp.Equation;
						vertex.pop_back(); vertex.pop_back();
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = equation;
							vertex.push_back(temp);
						}
						swap(x[0], x[1]); swap(y[0], y[1]);
					}
					else
					{
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
					}
					x[2] = pt.x - Wygb; y[2] = pt.y - Hygb;
					x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4);
					preState = 0;
				}
				else//右下角 垂直向下
				{
					if (preState == 0 && NumPoint>2)//右上到右下
					{
						X0 = p1.x; Y0 = p1.y - Hygb;
						Point P1(p2.x + Wygb, p2.y + Hygb), P2(p1.x + Wygb, p1.y + Hygb), P3(p1.x - Wygb, p1.y + Hygb), P4(pt.x - Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X1 = point.x; Y1 = point.y;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						temp = *iter;
						XMFLOAT4 equation = temp.Equation;
						vertex.pop_back(); vertex.pop_back();
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = equation;
							vertex.push_back(temp);
						}
					}
					else if (preState == 3 && NumPoint>2)//左下到右下
					{
						X0 = p1.x - Wygb; Y0 = p1.y;
						Point P1(p2.x + Wygb, p2.y + Hygb), P2(p1.x + Wygb, p1.y + Hygb), P3(p1.x + Wygb, p1.y - Hygb), P4(pt.x + Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X1 = point.x; Y1 = point.y;
						x[0] = X0; y[0] = Y0;
						x[1] = X1; y[1] = Y1;
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						temp = *iter;
						XMFLOAT4 equation = temp.Equation;
						vertex.pop_back(); vertex.pop_back();
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = equation;
							vertex.push_back(temp);
						}
						swap(x[0], x[1]); swap(y[0], y[1]);
					}
					else
					{
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
					}
					x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
					x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4);
					preState = 1;
				}
			}
			else//向左画线
			{
				if (pt.y < p1.y)//左上角
				{
					if (preState == 3 && NumPoint>2)//左下到左上
					{
						Point P1(p2.x - Wygb, p2.y - Hygb), P2(p1.x - Wygb, p1.y - Hygb), P3(p1.x + Wygb, p1.y - Hygb), P4(pt.x + Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						X1 = p1.x; Y1 = p1.y + Hygb;
						x[0] = X0; y[0] = Y0;
						x[1] = X1; y[1] = Y1;
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						temp = *iter;
						XMFLOAT4 equation = temp.Equation;
						vertex.pop_back(); vertex.pop_back();
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = equation;
							vertex.push_back(temp);
						}
					}
					else if (preState == 0 && NumPoint>2)//右上到左上
					{
						Point P1(p2.x - Wygb, p2.y - Hygb), P2(p1.x - Wygb, p1.y - Hygb), P3(p1.x - Wygb, p1.y + Hygb), P4(pt.x - Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X0 = point.x; Y0 = point.y;
						X1 = p1.x + Wygb; Y1 = p1.y;
						x[0] = X0; y[0] = Y0;
						x[1] = X1; y[1] = Y1;
						vector<SimpleVertex>::iterator it = vertex.end() - 1;
						temp = *it;
						XMFLOAT4 equation = temp.Equation;
						vertex.pop_back(); vertex.pop_back();
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = equation;
							vertex.push_back(temp);
						}
						swap(x[0], x[1]); swap(y[0], y[1]);
					}
					else
					{
						x[0] = p1.x + Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x - Wygb; y[1] = p1.y + Hygb;
					}
					x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
					x[3] = pt.x - Wygb; y[3] = pt.y + Hygb;
					indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					preState = 2;
				}
				else//左下角 
				{
					if (preState == 2 && NumPoint>2)//左上到左下
					{
						X0 = p1.x; Y0 = p1.y - Hygb;
						Point P1(p2.x - Wygb, p2.y + Hygb), P2(p1.x - Wygb, p1.y + Hygb), P3(p1.x + Wygb, p1.y + Hygb), P4(pt.x + Wygb, pt.y + Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X1 = point.x; Y1 = point.y;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						temp = *iter;
						XMFLOAT4 equation = temp.Equation;
						vertex.pop_back(); vertex.pop_back();
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = equation;
							vertex.push_back(temp);
						}
					}
					else if (preState == 1 && NumPoint>2)//右下到左下
					{
						if (p1.x == p2.x)
							Y0 = p1.y + Hygb;
						else
							Y0 = p1.y;
						X0 = p1.x + Wygb;
						Point P1(p2.x - Wygb, p2.y + Hygb), P2(p1.x - Wygb, p1.y + Hygb), P3(p1.x - Wygb, p1.y - Hygb), P4(pt.x - Wygb, pt.y - Hygb);
						XMFLOAT2 point = CalInterSection(P1, P2, P3, P4, preState);
						X1 = point.x; Y1 = point.y;
						x[0] = X0; y[0] = Y0; x[1] = X1; y[1] = Y1;
						vector<SimpleVertex>::iterator iter = vertex.end() - 1;
						temp = *iter;
						XMFLOAT4 equation = temp.Equation;
						vertex.pop_back(); vertex.pop_back();
						for (UINT i = 0; i < 2; i++)
						{
							temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
							temp.Equation = equation;
							vertex.push_back(temp);
						}
						swap(x[0], x[1]); swap(y[0], y[1]);
					}
					else
					{
						x[0] = p1.x - Wygb; y[0] = p1.y - Hygb;
						x[1] = p1.x + Wygb; y[1] = p1.y + Hygb;
					}
					x[2] = pt.x - Wygb; y[2] = pt.y - Hygb; x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
					indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					preState = 3;
				}
			}
			for (UINT i = 0; i < 4; i++)
			{
				temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);
				temp.Equation = line;
				vertex.push_back(temp);
			}
			p2 = p1; p1 = pt;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
void Render()
{
	float Bgcolor[] = { 255, 255, 255, 1.0 };
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Bgcolor);
	list<InkCache>::iterator it = inkcache.begin();
	for (; it != inkcache.end(); it++)
	{
		UpdatePoint(it->point, it->indices);
		ConstantYgbColor color;
		color.Color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0);
		g_pImmediateContext->UpdateSubresource(g_pConstantColor, 0, nullptr, &color, 0, 0);

		g_pImmediateContext->OMSetBlendState(g_pBlendState, 0, 0xffffffff);
		g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
		g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantColor);
		g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
		g_pImmediateContext->DrawIndexed(it->indices.size(), 0, 0);
	}
	g_pSwapChain->Present(0, 0);
}
//XMFLOAT2 CalInterSection(float s1, float t1, float s2, float t2, float s3, float t3, float s4, float t4, int flages)
//{
//	//点G(S1, T1), H(S2, T2) 表示的直线为：T = K*S + B
//	//点P(S3, T3), Q(S4, T4) 表示的直线为： T = M*S + D
//	float K, B, M, D, X, Y;
//	if (s1 == s2)
//	{
//		X = s1; M = (t3 - t4) / (s3 - s4);
//		D = t3 - M*s3; Y = M*X + D;
//	}
//	else if (s3 == s4)
//	{
//		X = s3; K = (t1 - t2) / (s1 - s2);
//		B = t1 - K*s1; Y = K*X + B;
//	}
//	else
//	{
//		K = (t1 - t2) / (s1 - s2);
//		M = (t3 - t4) / (s3 - s4);
//		B = t1 - K*s1; D = t3 - M*s3;
//		X = (D - B) / (K - M);
//		Y = K*(D - B) / (K - M) + B;
//	}
//	//交点矫正 不在正常范围的点取中间值
//	if (flages == 0)
//	{
//		if (t3 >= t4)//右上到左上
//		{
//			if (max(s1, s4) <= X&&Y <= min(t3, t1) && Y >= max(t2, t4))
//				return XMFLOAT2(X, Y);
//			else
//				return XMFLOAT2((max(s1, s4) + s3) / 2, (min(t3, t1) + max(t2, t4)) / 2);
//		}
//		else//右上到右下
//		{
//			if (Y <= min(t1, t4) && X >= max(s3,s1)&&X <= min(s2,s4))
//				return XMFLOAT2(X, Y);
//			else
//				return XMFLOAT2((max(s3, s1) + min(s2, s4)) / 2, (min(t1, t4) + t3) / 2);
//		}
//	}
//	else if (flages == 1)
//	{
//		if (t3 >= t4)//右下到右上
//		{
//			if (Y >= max(t1, t4) && max(s3, s1) <= X&&X <= min(s2, s4))
//				return XMFLOAT2(X, Y);
//			else
//				return XMFLOAT2((max(s1,s3)+min(s2,s4))/2,(max(t1,t4)+t2)/2);
//		}
//		else//右下到左下
//		{
//			if (max(s1, s4) <= X&&Y >= max(t3, t1) && Y <= min(t2, t4))
//				return XMFLOAT2(X, Y);
//			else
//				return XMFLOAT2((max(s1, s4) + s3) / 2, (max(t3, t1) + min(t2, t4)) / 2);
//		}
//	}
//	else if (flages == 2)
//	{
//		if (t3 >= t4)//左上到右上
//		{
//			if (X <= min(s1, s4) && Y <= min(t3,t1)&&Y >= max(t2,t4))
//				return XMFLOAT2(X, Y);
//			else
//				return XMFLOAT2((s2 + min(s1, s4)) / 2, (min(t3, t1) + max(t2, t4)) / 2);
//		}
//		else//左上到左下
//		{
//			if (Y <= min(t4, t1) && X <= min(s3,s1)&&X >= max(s2,s4))
//				return XMFLOAT2(X, Y);
//			else
//				return XMFLOAT2((min(s3, s1) + max(s2, s4)) / 2, (min(t4, t1) + t2) / 2);
//		}
//	}
//	else if (flages == 3)
//	{
//		if (t3 >= t4)//左下到左上
//		{
//			if (Y >= max(t1, t4) && X <= min(s3,s1)&&X >= max(s2,s4))
//				return XMFLOAT2(X, Y);
//			else
//				return XMFLOAT2((min(s3, s1) + max(s2, s4)) / 2, (t2 + max(t1, t4)) / 2);
//		}
//		else//左下到右下
//		{
//			if (X <= min(s1, s4) && Y >= max(t3,t1)&&Y <= min(t2,t4))
//				return XMFLOAT2(X, Y);
//			else
//				return XMFLOAT2((s2 + min(s1, s4)) / 2, (max(t3, t1) + min(t2, t4)) / 2);
//		}
//	}
//	else
//		return XMFLOAT2(X, Y);
//}
//http://blog.csdn.net/fu_shuwu/article/details/78376803

//点G(S1, T1), H(S2, T2) 表示的直线为：T = K*S + B
//点P(S3, T3), Q(S4, T4) 表示的直线为： T = M*S + D
XMFLOAT2 CalInterSection(float s1, float t1, float s2, float t2, float s3, float t3, float s4, float t4, int flages)
{
	float K, B, M, D, X, Y;
	if (s1 == s2)
	{
		X = s1; M = (t3 - t4) / (s3 - s4);
		D = t3 - M*s3; Y = M*X + D;
	}
	else if (s3 == s4)
	{
		X = s3; K = (t1 - t2) / (s1 - s2);
		B = t1 - K*s1; Y = K*X + B;
	}
	else
	{
		K = (t1 - t2) / (s1 - s2);
		M = (t3 - t4) / (s3 - s4);
		B = t1 - K*s1; D = t3 - M*s3;
		X = (D - B) / (K - M);
		Y = K*(D - B) / (K - M) + B;
	}
	if (flages == 0)
		if (t3 >= t4)//右上到左上
			if (max(s1, s4) <= X&&Y <= min(t3, t1) && Y >= max(t2, t4))
				return XMFLOAT2(X, Y);
			else
				return XMFLOAT2((s1 + s2) / 2, (t1 + t2) / 2);
		else//右上到右下
			if (Y <= min(t1, t4) && X >= max(s3, s1) && X <= min(s2, s4))
				return XMFLOAT2(X, Y);
			else
				return XMFLOAT2((s1 + s2) / 2, (t1 + t2) / 2);
	else if (flages == 1)
		if (t3 >= t4)//右下到右上
			if (Y >= max(t1, t4) && max(s3, s1) <= X&&X <= min(s2, s4))
				return XMFLOAT2(X, Y);
			else
				return XMFLOAT2((s1 + s2) / 2, (t1 + t2) / 2);
		else//右下到左下
			if (max(s1, s4) <= X&&Y >= max(t3, t1) && Y <= min(t2, t4))
				return XMFLOAT2(X, Y);
			else
				return XMFLOAT2((s1 + s2) / 2, (t1 + t2) / 2);
	else if (flages == 2)
		if (t3 >= t4)//左上到右上
			if (X <= min(s1, s4) && Y <= min(t3, t1) && Y >= max(t2, t4))
				return XMFLOAT2(X, Y);
			else
				return XMFLOAT2((s1 + s2) / 2, (t1 + t2) / 2);
		else//左上到左下
			if (Y <= min(t4, t1) && X <= min(s3, s1) && X >= max(s2, s4))
				return XMFLOAT2(X, Y);
			else
				return XMFLOAT2((s1 + s2) / 2, (t1 + t2) / 2);
	else if (flages == 3)
		if (t3 >= t4)//左下到左上
			if (Y >= max(t1, t4) && X <= min(s3, s1) && X >= max(s2, s4))
				return XMFLOAT2(X, Y);
			else
				return XMFLOAT2((s1 + s2) / 2, (t1 + t2) / 2);
		else//左下到右下
			if (X <= min(s1, s4) && Y >= max(t3, t1) && Y <= min(t2, t4))
				return XMFLOAT2(X, Y);
			else
				return XMFLOAT2((s1 + s2) / 2, (t1 + t2) / 2);
	else
		return XMFLOAT2(X, Y);
}

XMFLOAT2 CalInterSection(Point p1, Point p2, Point p3, Point p4, int flages)
{
	return CalInterSection(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y, flages);
}

XMFLOAT4 CalLineEquation(Point p1, Point p2)
{
	float a, b, c, d;
	if (p2.x == p1.x)
	{
		a = 1; b = 0;
		c = -p2.x;
		d = Hygb;
	}
	else
	{
		a = (float)(p2.y - p1.y) / (p2.x - p1.x);
		b = -1.0;
		c = p1.y - a*p1.x;
		d = Hygb;
	}
	return XMFLOAT4(a, b, c, d);
}


float dist(Point p1, Point p2)
{
	float d = sqrt((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y));
	return d;
}

float distance(Point p, XMFLOAT4 e)
{
	float temp = fabs((e.x*p.x + e.y*p.y + e.z) / sqrt(e.x*e.x + e.y*e.y));
	return temp;
}