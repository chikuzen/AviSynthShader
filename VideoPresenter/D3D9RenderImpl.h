#pragma once

#define D3D_DEBUG_INFO
#include "d3d9.h"
#include "Vertex.h"
#include "atlbase.h"
#include "Macros.h"
#include <windows.h>

struct InputTexture {
	CComPtr<IDirect3DSurface9> Memory;
	CComPtr<IDirect3DTexture9> Texture;
	CComPtr<IDirect3DSurface9> Surface;
};

class D3D9RenderImpl // : public IRenderable
{
public:
	D3D9RenderImpl();
	~D3D9RenderImpl(void);

	HRESULT Initialize(HWND hDisplayWindow, int width, int height, int precision);
	HRESULT CheckFormatConversion(D3DFORMAT format);
	HRESULT CreateInputTexture(int index);
	HRESULT CopyToBuffer(const byte* src, int srcPitch, int index);
	HRESULT ProcessFrame(byte* dst, int dstPitch);

	HRESULT SetPixelShader(LPCSTR pPixelShaderName, LPCSTR entryPoint, LPCSTR shaderModel, LPSTR* ppError);
	HRESULT SetPixelShader(DWORD* buffer);
	HRESULT SetPixelShaderIntConstant(LPCSTR name, int value);
	HRESULT SetPixelShaderFloatConstant(LPCSTR name, float value);
	HRESULT SetPixelShaderBoolConstant(LPCSTR name, bool value);
	HRESULT SetPixelShaderConstant(LPCSTR name, LPVOID value, UINT size);

	HRESULT SetVertexShader(LPCSTR pVertexShaderName, LPCSTR entryPoint, LPCSTR shaderModel, LPSTR* ppError);
	HRESULT SetVertexShader(DWORD* buffer);
	HRESULT SetVertexShaderConstant(LPCSTR name, LPVOID value, UINT size);
	HRESULT ApplyWorldViewProj(LPCSTR matrixName);

	HRESULT SetVertexShaderMatrix(D3DXMATRIX* matrix, LPCSTR name);
	HRESULT SetPixelShaderMatrix(D3DXMATRIX* matrix, LPCSTR name);
	HRESULT SetVertexShaderVector(D3DXVECTOR4* vector, LPCSTR name);
	HRESULT SetPixelShaderVector(LPCSTR name, D3DXVECTOR4* vector);

	HRESULT ClearPixelShader();
	HRESULT ClearVertexShader();


private:
	HRESULT SetupMatrices(int width, int height);
	HRESULT CreateScene(void);
	HRESULT CheckDevice(void);
	HRESULT CopyFromRenderTarget(byte* dst, int dstPitch);
	HRESULT CreateRenderTarget();
	HRESULT Present(void);	
	HRESULT GetPresentParams(D3DPRESENT_PARAMETERS* params, BOOL bWindowed);
	HRESULT DiscardResources();
	HRESULT CreateResources();

	CComPtr<IDirect3D9>             m_pD3D9;
	CComPtr<IDirect3DDevice9>       m_pDevice;
	CComPtr<IDirect3DTexture9>      m_pRenderTarget;
	CComPtr<IDirect3DSurface9>      m_pRenderTargetSurface;
	CComPtr<IDirect3DVertexBuffer9> m_pVertexBuffer;
	CComPtr<IDirect3DVertexShader9> m_pVertexShader;
	CComPtr<ID3DXConstantTable>     m_pVertexConstantTable;
	CComPtr<ID3DXConstantTable>     m_pPixelConstantTable;
	CComPtr<IDirect3DPixelShader9>  m_pPixelShader;
	static const int maxTextures = 5;
	InputTexture					m_InputTextures[maxTextures];

	D3DDISPLAYMODE m_displayMode;
	HWND m_hDisplayWindow;
	int m_videoWidth;
	int m_videoHeight;
	int m_precision;
	D3DFORMAT m_format;
	D3DPRESENT_PARAMETERS m_presentParams;
};