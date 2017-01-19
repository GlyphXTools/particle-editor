#ifndef ENGINE_H
#define ENGINE_H

#include "managers.h"
#include "ParticleSystem.h"
#include "utils.h"

class Object3D
{
    Object3D* m_parent;

protected:
	D3DXVECTOR3 m_position;
    D3DXVECTOR3 m_velocity;

public:
    const Object3D* GetParent() const { return m_parent; }
    Object3D* GetParent() { return m_parent; }

	D3DXVECTOR3 GetPosition() const
	{
        return (m_parent != NULL) ? m_parent->GetPosition() + m_position : m_position;
	}

	D3DXVECTOR3 GetVelocity() const
    {
        return (m_parent != NULL) ? m_parent->GetVelocity() + m_velocity : m_velocity;
    }

	const D3DXVECTOR3& GetRelativeVelocity() const { return m_velocity; }
	const D3DXVECTOR3& GetRelativePosition() const { return m_position; }

    bool Detached() const { return m_parent == NULL; }

    virtual void Detach()
    {
        if (!Detached())
        {
            m_position = GetPosition();
            m_parent   = NULL;
        }
    }

    Object3D(Object3D* parent, const D3DXVECTOR3& position = D3DXVECTOR3(0,0,0))
        : m_parent(parent), m_position(position), m_velocity(0,0,0)
    {
    }
};

typedef float TimeF;
static TimeF GetTimeF() { return GetTickCount() / 1000.0f; }

class ParticleSystemInstance;
class EmitterInstance;

class Engine
{
public:
    enum LightType
    {
	    LT_SUN,
	    LT_FILL1,
	    LT_FILL2,
    };

    struct Light
    {
	    D3DXVECTOR4 Diffuse;
	    D3DXVECTOR4 Specular;
	    D3DXVECTOR4 Position;
	    D3DXVECTOR4 Direction;
    };

    static const int NUM_SHADERS = 14;

	// Describes a camera
	struct Camera
	{
		D3DXVECTOR3 Position;
		D3DXVECTOR3 Target;
		D3DXVECTOR3 Up;
	};

	void Update();
	bool Render();

	ParticleSystemInstance* SpawnParticleSystem(const ParticleSystem& system, Object3D* parent);
    
	void DetachParticleSystem(ParticleSystemInstance* instance);
	void KillParticleSystem(ParticleSystemInstance* instance);
	void Clear();
	
	IDirect3DTexture9* GetTexture(const std::string& name) const;

	void OnParticleSystemChanged(int track);

	const D3DXMATRIX& GetProjectionMatrix()   const { return m_projection; }
	const D3DXMATRIX& GetViewMatrix()         const { return m_view; }
	const D3DXMATRIX& GetViewRotationMatrix() const { return m_viewRotation; }
	const D3DXMATRIX& GetBillboardMatrix()    const { return m_billboard; }
	void  GetViewPort(D3DVIEWPORT9* viewport) const;

	const Camera& GetCamera() const;
	void  SetCamera(const Camera& camera);

	bool     GetGround() const		{ return m_showGround; }
	bool     GetHeatDebug() const   { return m_debugHeat; }
    COLORREF GetBackground() const  { return m_background; }
	const D3DXVECTOR3& GetGravity() const { return m_gravity; }
	const D3DXVECTOR3& GetWind() const    { return m_wind; }
    Effect* GetShader(int i) const        { return m_pShaders[i]; }

    int GetNumEmitters()  const { return m_numEmitters;  }
    int GetNumParticles() const { return m_numParticles; }
    int GetNumInstances() const { return (int)m_instances.size(); }

    void OnInstanceDestroyed(ParticleSystemInstance* instance);
    void OnEmitterCreated(int numParticles)   { m_numEmitters++; m_numParticles += numParticles; }
    void OnEmitterDestroyed(int numParticles) { m_numEmitters--; m_numParticles -= numParticles; }

	void SetBackground(COLORREF color);
	void SetLight(LightType which, const Light& light);
	void SetAmbient(const D3DXVECTOR4& color);
	void SetShadow(const D3DXVECTOR4& color);
	void SetWind(const D3DXVECTOR3& wind);
	void SetGravity(const D3DXVECTOR3& gravity);
	void SetGround(bool enable);
	void SetHeatDebug(bool debug);

	void				Reset();
	Engine(HWND hFocus, HWND hDevice, ITextureManager& textureManager, IShaderManager& shaderManager);
	~Engine();

private:
	D3DMULTISAMPLE_TYPE GetMultiSampleType(DWORD* MultiSampleQuality, D3DFORMAT DisplayFormat, D3DFORMAT DepthStencilFormat, BOOL Windowed);
	D3DFORMAT           GetDepthStencilFormat(D3DFORMAT AdapterFormat, bool withStencilBuffer);
	void				ResetParameters();

	//
	// Data members
	//

	// Particle management
    std::vector<ParticleSystemInstance*> m_instances;
    int m_numParticles;
    int m_numEmitters;

	// Viewing
	Camera		m_eye;
	D3DXMATRIX	m_view;
    D3DXMATRIX	m_viewInverse;
	D3DXMATRIX  m_viewRotation;
	D3DXMATRIX	m_billboard;
	D3DXMATRIX	m_projection;
	D3DXMATRIX	m_viewProjection;

    COLORREF    m_background;
	bool		m_showGround;
	bool		m_debugHeat;
	D3DXVECTOR3 m_wind;
	D3DXVECTOR3	m_gravity;
    D3DXVECTOR4 m_ambient;
    Light       m_lights[3];
    D3DXMATRIX  m_sphLightFill[3];
    D3DXMATRIX  m_sphLightAll[3];

	// Resources
	IDirect3DTexture9*	m_pGroundTexture;
	IDirect3DTexture9*	m_pSceneTexture;
    IDirect3DSurface9*  m_pDepthStencilSurface;
	IDirect3DTexture9*	m_pDistortTexture;
	Effect*             m_pDistortShader;
    Effect*             m_pShaders[NUM_SHADERS];

	ITextureManager&				m_textureManager;
	IDirect3D9*						m_pDirect3D;
	D3DPRESENT_PARAMETERS			m_presentationParameters;
	IDirect3DDevice9*				m_pDevice;
	IDirect3DVertexDeclaration9*	m_pDeclaration;

	static D3DVERTEXELEMENT9 ParticleElements[];
    
	// Shader
	D3DXHANDLE	 p_worldViewProjection;
};

#endif