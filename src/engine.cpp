#include <algorithm>
#include <assert.h>
#include "engine.h"
#include "exceptions.h"
#include "resource.h"
#include "ParticleSystemInstance.h"
#include "EmitterInstance.h"
#include "SphericalHarmonics.h"
using namespace std;

static const char* ShaderNames[Engine::NUM_SHADERS] = {
    "Engine\\PrimOpaque.fx",
    "Engine\\PrimAdditive.fx",
    "Engine\\PrimAlpha.fx",
    "Engine\\PrimModulate.fx",
    "Engine\\PrimDepthSpriteAdditive.fx",
    "Engine\\PrimDepthSpriteAlpha.fx",
    "Engine\\PrimDepthSpriteModulate.fx",
    "Engine\\PrimDiffuseAlpha.fx",
    "Engine\\StencilDarken.fx",
    "Engine\\StencilDarkenFinalBlur.fx",
    "Engine\\PrimHeat.fx",
    "Engine\\PrimParticleBumpAlpha.fx",
    "Engine\\PrimDecalBumpAlpha.fx",
    "Engine\\PrimAlphaScanlines.fx",
};

D3DVERTEXELEMENT9 Engine::ParticleElements[] = {
	{0, offsetof(EmitterInstance::Vertex, Position),  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, 
	{0, offsetof(EmitterInstance::Vertex, Normal),    D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0}, 
	{0, offsetof(EmitterInstance::Vertex, TexCoord0), D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0}, 
	{0, offsetof(EmitterInstance::Vertex, TexCoord1), D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1}, 
	{0, offsetof(EmitterInstance::Vertex, Color),     D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0}, 
	D3DDECL_END()
};

ParticleSystemInstance* Engine::SpawnParticleSystem(const ParticleSystem& system, Object3D* parent)
{
    int count = 0;
	ParticleSystemInstance* instance = new ParticleSystemInstance(*this, system, parent);
    m_instances.push_back(instance);
    m_numParticles += count;
	return instance;
}

void Engine::DetachParticleSystem(ParticleSystemInstance* instance)
{
    instance->Detach();
}

void Engine::KillParticleSystem(ParticleSystemInstance* instance)
{
	if (instance->GetParticleSystem().getLeaveParticles())
	{
		// Leave particles to finish; just disable it
		instance->StopSpawning();
        instance->Detach();
	}
	else
	{
    	// Don't leave particles, kill the thing now
        delete instance;
	}
}

void Engine::OnInstanceDestroyed(ParticleSystemInstance* instance)
{
    // Remove the intance from the list
    for (vector<ParticleSystemInstance*>::iterator p = m_instances.begin(); p != m_instances.end(); p++)
    {
        if (*p == instance)
        {
            m_instances.erase(p);
            break;
        }
    }
}

void Engine::Clear()
{
    while (!m_instances.empty())
    {
        delete m_instances[0];
	}
    m_numParticles = 0;
    m_numEmitters  = 0;
}

void Engine::Update()
{
	TimeF currentTime = GetTimeF();

    // Update existing instances
    for (size_t i = 0; i < m_instances.size(); i++)
    {
        ParticleSystemInstance* instance = m_instances[i];
        m_numParticles += instance->Update(currentTime);

        // The update could've killed the particle system, check for that
        if (i == m_instances.size() || m_instances[i] != instance)
        {
            i--;
        }
    }
}

static bool ParticleSystemCompare(const ParticleSystemInstance* p1, const ParticleSystemInstance* p2)
{
    return p1->GetZDistance() < p2->GetZDistance();
}

bool Engine::Render()
{
	static const D3DXMATRIX Identity(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);

	// See if we can render
	switch (m_pDevice->TestCooperativeLevel())
	{
		case D3DERR_DEVICELOST:
			return false;

		case D3DERR_DEVICENOTRESET:
			Reset();
			break;
	}

    // Set all effect parameters
    for (int i = 0; i < NUM_SHADERS; i++)
    {
        static D3DXMATRIX Identity(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);

        const Effect::Handles& handles = m_pShaders[i]->getHandles();
        ID3DXEffect* pEffect = m_pShaders[i]->getD3DEffect();

        // World, View, Projection Transforms
        pEffect->SetMatrix(handles.hWorld,               &Identity);
        pEffect->SetMatrix(handles.hWorldInverse,        &Identity);
        pEffect->SetMatrix(handles.hProjection,          &m_projection);
        pEffect->SetMatrix(handles.hViewProjection,      &m_viewProjection);
        pEffect->SetMatrix(handles.hViewInverse,         &m_viewInverse);
        pEffect->SetMatrix(handles.hView,                &m_view);
        pEffect->SetMatrix(handles.hWorldViewProjection, &m_viewProjection);
        pEffect->SetMatrix(handles.hWorldViewInverse,    &m_viewInverse);
        pEffect->SetMatrix(handles.hWorldView,           &m_view);
        pEffect->SetVector(handles.hEyePosition,         &D3DXVECTOR4(m_eye.Position.x, m_eye.Position.y, m_eye.Position.z, 1));

        // Lighting
        pEffect->SetVector(handles.hGlobalAmbient,    &m_ambient);
        pEffect->SetVector(handles.hDirLightVec0,     &m_lights[0].Position);
        pEffect->SetVector(handles.hDirLightObjVec0,  &m_lights[0].Position);
        pEffect->SetVector(handles.hDirLightDiffuse,  &m_lights[0].Diffuse);
        pEffect->SetVector(handles.hDirLightSpecular, &m_lights[0].Specular);
        pEffect->SetMatrixArray(handles.hSphLightAll,  m_sphLightAll,  3);
        pEffect->SetMatrixArray(handles.hSphLightFill, m_sphLightFill, 3);

        // Time
        pEffect->SetFloat(handles.hTime, GetTimeF());
        SAFE_RELEASE(pEffect);
    }

    // Sort the particle systems on distance from camera
    // Negative Z is further away, thus drawn first.
    // Therefore we need a normal ascending sort.
    sort(m_instances.begin(), m_instances.end(), ParticleSystemCompare);
	
	m_pDevice->BeginScene();

	IDirect3DSurface9* pScreenSurface;
	IDirect3DSurface9* pDepthSurface;
	m_pDevice->GetRenderTarget(0, &pScreenSurface);
    m_pDevice->GetDepthStencilSurface(&pDepthSurface);

    // Set the new depth buffer
    m_pDevice->SetDepthStencilSurface(m_pDepthStencilSurface);

	// Render to the scene texture
	IDirect3DSurface9* pSceneSurface;
	m_pSceneTexture->GetSurfaceLevel(0, &pSceneSurface);
	m_pDevice->SetRenderTarget(0, pSceneSurface);
	SAFE_RELEASE(pSceneSurface);

    D3DCOLOR clearColor = D3DCOLOR_XRGB(GetRValue(m_background), GetGValue(m_background), GetBValue(m_background));
	m_pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearColor, 1.0f, 0);
	if (m_showGround)
	{
		static const float TEXTURE_SCALE  = 256;
		static const float MAP_SIZE       = 80;
		static const float UNITS_PER_CELL = 20;
		static const EmitterInstance::Vertex ground[4] = {
			{D3DXVECTOR3(-UNITS_PER_CELL*MAP_SIZE/2,-UNITS_PER_CELL*MAP_SIZE/2,0), D3DXVECTOR3(0,0,1), D3DXVECTOR2(                                    0,                                     0), D3DXVECTOR2(0,0), D3DCOLOR_RGBA(255,255,255,255)},
			{D3DXVECTOR3( UNITS_PER_CELL*MAP_SIZE/2,-UNITS_PER_CELL*MAP_SIZE/2,0), D3DXVECTOR3(0,0,1), D3DXVECTOR2(MAP_SIZE*UNITS_PER_CELL/TEXTURE_SCALE,                                     0), D3DXVECTOR2(0,0), D3DCOLOR_RGBA(255,255,255,255)},
			{D3DXVECTOR3(-UNITS_PER_CELL*MAP_SIZE/2, UNITS_PER_CELL*MAP_SIZE/2,0), D3DXVECTOR3(0,0,1), D3DXVECTOR2(                                    0, MAP_SIZE*UNITS_PER_CELL/TEXTURE_SCALE), D3DXVECTOR2(0,0), D3DCOLOR_RGBA(255,255,255,255)},
			{D3DXVECTOR3( UNITS_PER_CELL*MAP_SIZE/2, UNITS_PER_CELL*MAP_SIZE/2,0), D3DXVECTOR3(0,0,1), D3DXVECTOR2(MAP_SIZE*UNITS_PER_CELL/TEXTURE_SCALE, MAP_SIZE*UNITS_PER_CELL/TEXTURE_SCALE), D3DXVECTOR2(0,0), D3DCOLOR_RGBA(255,255,255,255)}
		};

		m_pDevice->SetTexture(0, m_pGroundTexture);
		m_pDevice->SetTransform(D3DTS_TEXTURE0, &Identity);
		m_pDevice->SetTexture(1, NULL);
		m_pDevice->SetRenderState(D3DRS_ZENABLE,          TRUE);
		m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE,     TRUE);
		m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, ground, sizeof(EmitterInstance::Vertex));
	}

    for (vector<ParticleSystemInstance*>::iterator p = m_instances.begin(); p != m_instances.end(); p++)
    {
        (*p)->RenderNormal(m_pDevice);
	}
    m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

	// Now render to the heat texture
	IDirect3DSurface9* pDistortSurface;
	m_pDistortTexture->GetSurfaceLevel(0, &pDistortSurface);
	m_pDevice->SetRenderTarget(0, pDistortSurface);
	SAFE_RELEASE(pDistortSurface);

	m_pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(129,128,255), 1.0f, 0);
    for (vector<ParticleSystemInstance*>::iterator p = m_instances.begin(); p != m_instances.end(); p++)
    {
        (*p)->RenderHeat(m_pDevice);
	}

	// Now render to the screen
	m_pDevice->SetRenderTarget(0, pScreenSurface);
    m_pDevice->SetDepthStencilSurface(pDepthSurface);
	SAFE_RELEASE(pScreenSurface);
    SAFE_RELEASE(pDepthSurface);
	m_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 0.0f, 0);

	static const EmitterInstance::Vertex quad[4] = {
		{D3DXVECTOR3(-1,-1,0), D3DXVECTOR2(0, 1), D3DXVECTOR4(1,1,1,1)},
		{D3DXVECTOR3( 1,-1,0), D3DXVECTOR2(1, 1), D3DXVECTOR4(1,1,1,1)},
		{D3DXVECTOR3(-1, 1,0), D3DXVECTOR2(0, 0), D3DXVECTOR4(1,1,1,1)},
		{D3DXVECTOR3( 1, 1,0), D3DXVECTOR2(1, 0), D3DXVECTOR4(1,1,1,1)}
	};

    ID3DXEffect* pEffect = m_pDistortShader->getD3DEffect();
    m_pDevice->SetTexture(0, m_pSceneTexture);
    m_pDevice->SetTexture(1, m_pDistortTexture);
	pEffect->SetTexture("SceneTexture",      m_pSceneTexture);
	pEffect->SetTexture("DistortionTexture", m_pDistortTexture);

	UINT nPasses = 1;
	pEffect->Begin(&nPasses, 0);
	for (UINT i = 0; i < nPasses; i++)
	{
		pEffect->BeginPass(i);
		m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(EmitterInstance::Vertex));
		pEffect->EndPass();
	}
	pEffect->End();
    SAFE_RELEASE(pEffect);

	m_pDevice->EndScene();
	m_pDevice->Present(NULL, NULL, NULL, NULL);
	return true;
}

IDirect3DTexture9* Engine::GetTexture(const string& name) const
{
	return m_textureManager.getTexture(m_pDevice, name);
}

void Engine::OnParticleSystemChanged(int track)
{
    for (vector<ParticleSystemInstance*>::iterator p = m_instances.begin(); p != m_instances.end(); p++)
    {
		(*p)->onParticleSystemChanged(*this, track);
	}
}

void Engine::GetViewPort(D3DVIEWPORT9* viewport) const
{
	m_pDevice->GetViewport(viewport);
}

const Engine::Camera& Engine::GetCamera() const
{
	return m_eye;
}

void Engine::SetCamera( const Camera& camera )
{
	m_eye = camera;

	// Construct matrices
	D3DXMatrixLookAtRH(&m_view, &camera.Position, &camera.Target, &camera.Up );
	D3DXMatrixMultiply(&m_viewProjection, &m_view, &m_projection);

	// Create some resulting matrices
	m_viewRotation = m_view;
	m_viewRotation._41 = m_viewRotation._42 = m_viewRotation._43 = 0.0;
	D3DXMatrixInverse(&m_billboard,   NULL, &m_viewRotation);
    D3DXMatrixInverse(&m_viewInverse, NULL, &m_view);

    // Set matrices
	m_pDevice->SetTransform(D3DTS_VIEW,       &m_view);
	m_pDevice->SetTransform(D3DTS_PROJECTION, &m_projection);
}

void Engine::SetGround(bool enable)			        { m_showGround = enable; }
void Engine::SetBackground(COLORREF color)		    { m_background = color; }
void Engine::SetHeatDebug(bool debug)		        { m_debugHeat  = debug;  }
void Engine::SetWind(const D3DXVECTOR3& wind)       { m_wind = wind; }
void Engine::SetGravity(const D3DXVECTOR3& gravity) { D3DXVec3Normalize(&m_gravity, &gravity); }
void Engine::SetLight(LightType which, const Light& light)
{
	int index = 0;
	switch (which)
	{
		case LT_SUN:	index = 0; break;
		case LT_FILL1:	index = 1; break;
		case LT_FILL2:	index = 2; break;
	}
	m_lights[index] = light;
	
	// Calculate direction from position
    m_lights[index].Direction   = -m_lights[index].Position;
    m_lights[index].Direction.w = 0.0f;
	D3DXVec4Normalize(&m_lights[index].Direction, &m_lights[index].Direction);

	// Recalculate Spherical Harmonics matrices
	SPH_Calculate_Matrices(m_sphLightFill, &m_lights[1], 2, m_ambient);
	SPH_Calculate_Matrices(m_sphLightAll,  &m_lights[0], 3, m_ambient);
}

void Engine::SetAmbient(const D3DXVECTOR4& color)
{
	m_ambient = color;

	// Recalculate Spherical Harmonics matrices
	SPH_Calculate_Matrices(m_sphLightFill, &m_lights[1], 2, m_ambient);
	SPH_Calculate_Matrices(m_sphLightAll,  &m_lights[0], 3, m_ambient);
}

void Engine::Reset()
{
	SAFE_RELEASE(m_pDistortTexture);
	SAFE_RELEASE(m_pSceneTexture);
    SAFE_RELEASE(m_pDepthStencilSurface);

	// Reset device
	m_presentationParameters.BackBufferWidth  = 0;
    m_presentationParameters.BackBufferHeight = 0;
	m_presentationParameters.BackBufferCount  = 1;
    m_presentationParameters.Windowed         = true;

	m_pDistortShader->OnLostDevice();
    for (int i = 0; i < NUM_SHADERS; i++)
    {
        m_pShaders[i]->OnLostDevice();
    }
	if (FAILED(m_pDevice->Reset(&m_presentationParameters)))
	{
		throw wruntime_error(LoadString(IDS_ERROR_RENDERER_RESET));
	}
	m_pDistortShader->OnResetDevice();
    for (int i = 0; i < NUM_SHADERS; i++)
    {
        m_pShaders[i]->OnResetDevice();
    }

	ResetParameters();
}

void Engine::ResetParameters()
{
	if (m_presentationParameters.BackBufferWidth > 0 && m_presentationParameters.BackBufferHeight > 0)
	{
		// http://www.gamedev.net/columns/hardcore/shadowvolume/page4.asp
		float n = 1.0f;
		D3DXMatrixPerspectiveFovRH(&m_projection, D3DXToRadian(45), (float)m_presentationParameters.BackBufferWidth / m_presentationParameters.BackBufferHeight, n, 1000.0f );
		m_projection._33 = -1.0f;
		m_projection._43 = -2 * n;

		// Create dynamic textures
		if (FAILED(m_pDevice->CreateTexture(m_presentationParameters.BackBufferWidth, m_presentationParameters.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pSceneTexture, NULL)))
		{
			throw runtime_error("Unable to create texture");
		}

		if (FAILED(m_pDevice->CreateTexture(m_presentationParameters.BackBufferWidth, m_presentationParameters.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pDistortTexture, NULL)))
		{
			SAFE_RELEASE(m_pSceneTexture);
			throw runtime_error("Unable to create texture");
		}

        if (FAILED(m_pDevice->CreateDepthStencilSurface(m_presentationParameters.BackBufferWidth, m_presentationParameters.BackBufferHeight, m_presentationParameters.AutoDepthStencilFormat, D3DMULTISAMPLE_NONE, 0, TRUE, &m_pDepthStencilSurface, NULL)))
        {
            SAFE_RELEASE(m_pDistortTexture);
			SAFE_RELEASE(m_pSceneTexture);
			throw runtime_error("Unable to create depth buffer");
        }

		// Reset states
		m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

		// Reset vertex declaration
		m_pDevice->SetVertexDeclaration(m_pDeclaration);

		// Set color texture properties
		m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
		m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

		// Set normal texture properties
		m_pDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		m_pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
		m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
		m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		m_pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		m_pDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		m_pDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

		// Set world matrix
		D3DXMATRIX identity;
		D3DXMatrixIdentity(&identity);
		m_pDevice->SetTransform(D3DTS_WORLD, &identity);

		// Reset camera
		SetCamera(m_eye);
	}
}

D3DFORMAT Engine::GetDepthStencilFormat(D3DFORMAT AdapterFormat, bool withStencilBuffer)
{
	static const D3DFORMAT DepthStencilFormatsNS[7] = { D3DFMT_D32,   D3DFMT_D24S8,  D3DFMT_D24X4S4, D3DFMT_D24FS8, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D15S1 };
	static const D3DFORMAT DepthStencilFormatsS[4]  = { D3DFMT_D24S8, D3DFMT_D24FS8, D3DFMT_D24X4S4, D3DFMT_D15S1 };

	int              nFormats = (withStencilBuffer) ? 4 : 7;
	const D3DFORMAT* Formats  = (withStencilBuffer) ? DepthStencilFormatsS : DepthStencilFormatsNS;

	for (int i = 0; i < nFormats; i++)
	{
		if (SUCCEEDED(m_pDirect3D->CheckDeviceFormat     (D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, Formats[i])))
		if (SUCCEEDED(m_pDirect3D->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, AdapterFormat, Formats[i])))
		{
			return Formats[i];
		}
	}

	return D3DFMT_UNKNOWN;
}

D3DMULTISAMPLE_TYPE Engine::GetMultiSampleType(DWORD* MultiSampleQuality, D3DFORMAT DisplayFormat, D3DFORMAT DepthStencilFormat, BOOL Windowed)
{
	D3DMULTISAMPLE_TYPE MultiSampleTypes[16] = {
		D3DMULTISAMPLE_16_SAMPLES, D3DMULTISAMPLE_15_SAMPLES, D3DMULTISAMPLE_14_SAMPLES, D3DMULTISAMPLE_13_SAMPLES,
		D3DMULTISAMPLE_12_SAMPLES, D3DMULTISAMPLE_11_SAMPLES, D3DMULTISAMPLE_10_SAMPLES, D3DMULTISAMPLE_9_SAMPLES,
		D3DMULTISAMPLE_8_SAMPLES, D3DMULTISAMPLE_7_SAMPLES, D3DMULTISAMPLE_6_SAMPLES, D3DMULTISAMPLE_5_SAMPLES,
		D3DMULTISAMPLE_4_SAMPLES, D3DMULTISAMPLE_3_SAMPLES, D3DMULTISAMPLE_2_SAMPLES, D3DMULTISAMPLE_NONE
	};

    for (int i = 0; i < 16; i++)
	{
		if (SUCCEEDED(m_pDirect3D->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, DisplayFormat,      Windowed, MultiSampleTypes[i], MultiSampleQuality)))
		if (SUCCEEDED(m_pDirect3D->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, DepthStencilFormat, Windowed, MultiSampleTypes[i], MultiSampleQuality)))
		{
			(*MultiSampleQuality)--;
			return MultiSampleTypes[i];
		}
	}

	*MultiSampleQuality = 0;
	return D3DMULTISAMPLE_NONE;
}

Engine::Engine(HWND hFocus, HWND hDevice, ITextureManager& textureManager, IShaderManager& shaderManager)
    : m_textureManager(textureManager)
{
	// Initialize members
	m_showGround     = true;
	m_debugHeat      = false;
	m_gravity        = D3DXVECTOR3(0,0,-1);
	m_wind           = D3DXVECTOR3(0,0,0);
	m_eye.Position   = D3DXVECTOR3(0,-250,125);
	m_eye.Target     = D3DXVECTOR3(0,0,0);
	m_eye.Up		 = D3DXVECTOR3(0,0,1);
    m_numEmitters    = 0;
    m_numParticles   = 0;
    m_ambient        = D3DXVECTOR4(0,0,0,0);
    m_background     = RGB(0x14,0x08,0x34);

	//
	// Initialize Direct3D
	//
	m_pDirect3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (m_pDirect3D == NULL)
	{
		throw runtime_error("Unable to initialize Direct3D");
	}

	ZeroMemory(&m_presentationParameters, sizeof(m_presentationParameters));
	m_presentationParameters.BackBufferFormat       = D3DFMT_UNKNOWN;
	m_presentationParameters.SwapEffect             = D3DSWAPEFFECT_DISCARD;
	m_presentationParameters.hDeviceWindow          = hDevice;
	m_presentationParameters.Windowed               = TRUE;
	m_presentationParameters.EnableAutoDepthStencil = TRUE;
	m_presentationParameters.Flags                  = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
	m_presentationParameters.PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;

	D3DDISPLAYMODE DisplayMode;
	if (FAILED(m_pDirect3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &DisplayMode)))
	{
		SAFE_RELEASE(m_pDirect3D);
		throw runtime_error("Unable to get current display mode");
	}

	if ((m_presentationParameters.AutoDepthStencilFormat = GetDepthStencilFormat(DisplayMode.Format, false)) == D3DFMT_UNKNOWN)
	{
		SAFE_RELEASE(m_pDirect3D);
		throw runtime_error("Unable to find a matching depth buffer format");
	}

	m_presentationParameters.MultiSampleType = GetMultiSampleType(&m_presentationParameters.MultiSampleQuality, DisplayMode.Format, m_presentationParameters.AutoDepthStencilFormat, m_presentationParameters.Windowed);

	// Create device (first try hardware, then software)
	if (FAILED(m_pDirect3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hFocus, D3DCREATE_HARDWARE_VERTEXPROCESSING, &m_presentationParameters, &m_pDevice)))
	if (FAILED(m_pDirect3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hFocus, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &m_presentationParameters, &m_pDevice)))
	{
		SAFE_RELEASE(m_pDirect3D);
		throw runtime_error("Unable to create render device");
	}

	// Create vertex declaration
	if (FAILED(m_pDevice->CreateVertexDeclaration(ParticleElements, &m_pDeclaration)))
	{
		SAFE_RELEASE(m_pDevice);
		SAFE_RELEASE(m_pDirect3D);
		throw runtime_error("Unable to create vertex declaration");
	}

	// Create ground texture
	if (FAILED(D3DXCreateTextureFromResource(m_pDevice, NULL, MAKEINTRESOURCE(IDB_GROUND), &m_pGroundTexture)))
	{
		SAFE_RELEASE(m_pDeclaration);
		SAFE_RELEASE(m_pDevice);
		SAFE_RELEASE(m_pDirect3D);
		throw runtime_error("Unable to load ground texture");
	}

	// Create shaders
    ID3DXEffect* pDistortEffect = NULL;
	if (FAILED(D3DXCreateEffectFromResource(m_pDevice, NULL, MAKEINTRESOURCE(IDS_SCENEHEAT), NULL, NULL, D3DXFX_NOT_CLONEABLE, NULL, &pDistortEffect, NULL)))
	{
		SAFE_RELEASE(m_pGroundTexture);
		SAFE_RELEASE(m_pDeclaration);
		SAFE_RELEASE(m_pDevice);
		SAFE_RELEASE(m_pDirect3D);
		throw runtime_error("Unable to load a shader");
	}
    m_pDistortShader = new Effect(pDistortEffect);
    pDistortEffect->SetFloat("DistortionAmount", 0.50f);
    SAFE_RELEASE(pDistortEffect);

    for (int i = 0; i < NUM_SHADERS; i++)
	{
        m_pShaders[i] = shaderManager.getShader(m_pDevice, ShaderNames[i]);
        if (m_pShaders[i] == NULL)
        {
            for (int j = 0; j < i; j++)
            {
                SAFE_RELEASE(m_pShaders[j]);
            }
            SAFE_RELEASE(m_pDistortShader);
		    SAFE_RELEASE(m_pGroundTexture);
		    SAFE_RELEASE(m_pDeclaration);
		    SAFE_RELEASE(m_pDevice);
		    SAFE_RELEASE(m_pDirect3D);
		    throw runtime_error("Unable to a shader");
        }

        ID3DXEffect* pEffect = m_pShaders[i]->getD3DEffect();
        D3DXEFFECT_DESC desc;
        pEffect->GetDesc(&desc);
        for (UINT i = 0; i < desc.Parameters; i++)
        {
            D3DXHANDLE hParam = pEffect->GetParameter(NULL, i);
            D3DXPARAMETER_DESC desc;
            pEffect->GetParameterDesc(hParam, &desc);
            if (desc.Type == D3DXPT_TEXTURE)
            {
                D3DXHANDLE hAnnon = pEffect->GetAnnotationByName(hParam, "texture_filename");
                pEffect->GetParameterDesc(hAnnon, &desc);
                LPCSTR value = NULL;
                if (SUCCEEDED(pEffect->GetString(hAnnon, &value)) && value != NULL)
                {
                    IDirect3DTexture9* pTexture = textureManager.getTexture(m_pDevice, value);
                    pEffect->SetTexture(hParam, pTexture);
                    SAFE_RELEASE(pTexture);
                }
            }
        }
        SAFE_RELEASE(pEffect);
	}

    Light sun = {
        D3DXVECTOR4( 1.0f, 1.0f, 1.0f, 1.0f),
        D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f),
        D3DXVECTOR4( 1.0f, 0.0f, 0.0f, 0.0f),
        D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f) 
    };

    Light fill = {
        D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f),
        D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f),
        D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f),
        D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f) 
    };

    SetLight(LT_SUN,   sun);
    SetLight(LT_FILL1, fill);
    SetLight(LT_FILL2, fill);
	ResetParameters();
}

Engine::~Engine()
{
    for (int i = 0; i < NUM_SHADERS; i++)
    {
        SAFE_RELEASE(m_pShaders[i]);
    }
    SAFE_RELEASE(m_pDepthStencilSurface);
	SAFE_RELEASE(m_pDistortShader);
	SAFE_RELEASE(m_pDistortTexture);
	SAFE_RELEASE(m_pSceneTexture);
	SAFE_RELEASE(m_pGroundTexture);
	SAFE_RELEASE(m_pDeclaration);
	SAFE_RELEASE(m_pDevice);
	SAFE_RELEASE(m_pDirect3D);
}
