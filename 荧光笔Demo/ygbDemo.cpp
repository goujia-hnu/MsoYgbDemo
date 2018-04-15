//--------------------------------------------------------------------------------------
//
//	文件名		：	ygbDemo.cpp
//	创建者		：	goujia@wps.cn
//	创建时间	    ：	2017-8-4 13:14:00
//	功能描述	    ：	GPU加速实现荧光笔动画渲染
//
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include <vector>
#include <tchar.h>
#include <iostream>
#include "resource.h"
#include "stdio.h"
using namespace DirectX;
using namespace std;
struct SimpleVertex
{
	XMFLOAT3 Pos;
};
struct Point
{
	int x;
	int y;
};
struct  ScreenWH
{
	int width;
	int height;
};
//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
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
ID3D11Buffer*           g_pScreenWH = nullptr;
char str[100];
vector<SimpleVertex>PointRecord;//记录所有的点
RECT rc;//保存客户区大小
int width, height;//客户区宽度 高度
bool isPress = false;//左键是否按下
vector<DWORD>indices;//保存索引缓存区数据
D3D11_BUFFER_DESC Vertexbuffer;
D3D11_BUFFER_DESC Indexbuffer;
Point pt, pre = { 0, 0 };//鼠标坐标点
DWORD NumPoint = 0;//鼠标坐标点个数
int Hygb = 9, Wygb = 3;//荧光笔高度宽度
bool toDraw = false;//标志开始划线
//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void UpdatePoint();//更新索引缓冲区和顶点缓冲区
void Render();

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;
	if (FAILED(InitDevice())){
		CleanupDevice();
		return 0;
	}
	indices.reserve(sizeof(WORD) * 20000);//预先分配足够大的内存 减少重分配
	PointRecord.reserve(sizeof(SimpleVertex) * 20000);

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else{
			Render();
		}
	}
	CleanupDevice();
	return (int)msg.wParam;
}
//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
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
	g_hWnd = CreateWindow(L"YgbWindowClass", L"WPP荧光笔D3D11渲染",
		WS_POPUP | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	if (!g_hWnd)
		return E_FAIL;
	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);
	return S_OK;
}
//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
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
//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
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
	UINT pNumQualityLevels = 0;
	g_pd3dDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &pNumQualityLevels);
	assert(pNumQualityLevels > 0);//因为4X MSAA总是被支持的，所以返回的质量等级总是大于0的

	// Create swap chain
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
		/*	if (pNumQualityLevels > 0)
		{
		sd.SampleDesc.Count = 4;
		sd.SampleDesc.Quality = pNumQualityLevels - 1;
		}
		else*/
		{
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
		}
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;
		hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1);
		if (SUCCEEDED(hr)){
			hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
		}
		dxgiFactory2->Release();
	}
	else{
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
		/*if (pNumQualityLevels > 0)
		{
		sd.SampleDesc.Count = 4;
		sd.SampleDesc.Quality = pNumQualityLevels - 1;
		}
		else*/
		{
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
		}
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
	hr = CompileShaderFromFile(L"anim_ygb_vs.fx", "VS", "vs_4_0", &pVSBlob);
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
	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);
	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;
	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"anim_ygb_vs.fx", "PS", "ps_4_0", &pPSBlob);
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
	Vertexbuffer.ByteWidth = sizeof(SimpleVertex) * 60000;
	Vertexbuffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Vertexbuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = g_pd3dDevice->CreateBuffer(&Vertexbuffer, NULL, &g_pVertexBuffer);
	if (FAILED(hr))
		return hr;
	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	//创建动态顶点索引
	ZeroMemory(&Indexbuffer, sizeof(Indexbuffer));
	Indexbuffer.Usage = D3D11_USAGE_DYNAMIC;
	Indexbuffer.ByteWidth = sizeof(DWORD) * 90000;
	Indexbuffer.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Indexbuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = g_pd3dDevice->CreateBuffer(&Indexbuffer, NULL, &g_pIndexBuffer);
	if (FAILED(hr))
		return hr;
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	return S_OK;

	D3D11_BUFFER_DESC bd;
	// Create the constant buffers
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ScreenWH);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pScreenWH);
	if (FAILED(hr))
		return hr;
	ScreenWH screen;
	screen.width = width;
	screen.height = height;
	g_pImmediateContext->UpdateSubresource(g_pScreenWH, 0, nullptr, &screen, 0, 0);
}
//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
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
	if (g_pScreenWH)g_pScreenWH->Release();
	if (g_pImmediateContext1) g_pImmediateContext1->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice1) g_pd3dDevice1->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}

void UpdatePoint()
{
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE vertexData;
	hr = g_pImmediateContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vertexData);
	if (FAILED(hr))
		return;
	SimpleVertex*v = reinterpret_cast<SimpleVertex*>(vertexData.pData);
	for (UINT i = 0; i < PointRecord.size(); i++)
		v[i] = PointRecord.at(i);
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
//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	SimpleVertex temp;
	switch (message){
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
			PointRecord.push_back(temp);
		}
		NumPoint++;
		indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);//102 123
		indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4);
		toDraw = true;
		pre = pt;
		Render();
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		isPress = false;
		break;
	case WM_MOUSEMOVE://鼠标移动
		if (isPress)//记录坐标值
		{
			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);
			if (abs(pt.x - pre.x) < 2.0 * Wygb && abs(pt.y - pre.y) < Hygb)
				return 0;
			float x[4], y[4];
			x[0] = pt.x - Wygb; y[0] = pt.y - Hygb;
			x[1] = pt.x - Wygb; y[1] = pt.y + Hygb;
			x[2] = pt.x + Wygb; y[2] = pt.y - Hygb;
			x[3] = pt.x + Wygb; y[3] = pt.y + Hygb;
			NumPoint++;//记录鼠标坐标点数目
			for (UINT i = 0; i < 4; i++)
			{
				temp.Pos = XMFLOAT3(x[i] * 2.0 / width - 1.0, 1.0 - y[i] * 2.0 / height, 1.0f);//坐标归一化
				PointRecord.push_back(temp);
			}
			if (pt.x >= pre.x)
			{
				if (pt.y < pre.y)//右上角
				{
					indices.push_back(0 + (NumPoint - 2) * 4); indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4);
					indices.push_back(0 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4); indices.push_back(3 + (NumPoint - 2) * 4);
				}
				else
				{
					indices.push_back(1 + (NumPoint - 2) * 4); indices.push_back(2 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
					indices.push_back(1 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4); indices.push_back(5 + (NumPoint - 2) * 4);
				}
			}
			else
			{
				if (pt.y < pre.y)
				{
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4); indices.push_back(2 + (NumPoint - 2) * 4);
					indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(2 + (NumPoint - 2) * 4); indices.push_back(1 + (NumPoint - 2) * 4);
				}
				else
				{
					indices.push_back(0 + (NumPoint - 2) * 4); indices.push_back(3 + (NumPoint - 2) * 4); indices.push_back(4 + (NumPoint - 2) * 4);
					indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(3 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4);
				}
			}
			indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(4 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4);
			indices.push_back(5 + (NumPoint - 2) * 4); indices.push_back(6 + (NumPoint - 2) * 4); indices.push_back(7 + (NumPoint - 2) * 4);
			pre = pt;//更新前一次鼠标坐标
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
//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	UpdatePoint();
	float Bgcolor[] = { 255, 255, 255, 1.0 };
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Bgcolor);//Clear the back buffer
	g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pScreenWH);
	g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
	g_pImmediateContext->DrawIndexed(indices.size(), 0, 0);
	g_pSwapChain->Present(0, 0);
}