#ifndef EMITTERINSTANCE_H
#define EMITTERINSTANCE_H

#include "Engine.h"
using namespace std;

static const int NUM_VERTICES_PER_PARTICLE  = 4;
static const int NUM_TRIANGLES_PER_PARTICLE = 2;

class EmitterInstance : Object3D
{
public:
	#pragma pack(1)
	struct Vertex
	{
		D3DXVECTOR3 Position;
        D3DXVECTOR3 Normal;
		D3DXVECTOR2 TexCoord0;
		D3DXVECTOR2 TexCoord1;
		D3DCOLOR    Color;
	};

	struct Primitive
	{
		uint16_t index[3 * NUM_TRIANGLES_PER_PARTICLE];
	};
	#pragma pack()

private:
	struct Particle;
    class  ParticleBlock;

    EmitterInstance*  m_next;
    EmitterInstance** m_prev;

	IDirect3DTexture9*		 m_pColorTexture;
	IDirect3DTexture9*		 m_pNormalTexture;
	bool					 m_doneSpawning;
    TimeF                    m_nextSpawnTime;
	ParticleSystem::Emitter& m_emitter;
	Engine&					 m_engine;
	ParticleSystemInstance&	 m_system;
    unsigned long            m_nParticlesPerBurst;
	unsigned long			 m_currentBurst;
	D3DXVECTOR3				 m_acceleration;
	unsigned int			 m_textureSizeSqrt;
    D3DXVECTOR3				 m_parentSpawnPosition;
	TimeF				     m_spawnDelay;
	TimeF				     m_freezeTime;

    // Particle storage
	vector<ParticleBlock*> m_blocks;
	vector<Vertex>		   m_vertices;
	vector<Primitive>	   m_primitives;
	vector<Particle*>      m_particleIndex;
	Particle*			   m_particleList;

	// Rendering
	D3DXMATRIX			m_textureTransform;
	const D3DXMATRIX*	m_billboard;
    DWORD               m_colorOp;
	DWORD				m_alphaSrcBlend;
	DWORD				m_alphaDestBlend;

	Particle& AllocateParticle();
	void      FreeParticle(Particle& particle);

	void  SpawnParticle(TimeF currentTime);
	int   SpawnParticles(TimeF currentTime);
    void  ResetParticle(Particle& particle, TimeF currentTime);
	void  UpdateTrackCursors(Particle& particle, float relTime) const;
	float SampleTrack(const Particle& particle, int track, float relTime) const;
	float IntegrateTrack(const Particle& particle, int track, float relTime) const;
	void  UpdateParticle(Particle& particle, float relTime);
	int   KillParticle(TimeF currenTime, Particle& particle);

	bool  IsFrozen(TimeF currentTime) const;
	bool  DoneSpawning()  const   { return m_doneSpawning; }	// Are we done spawning?
	TimeF GetSpawnDelay() const   { return m_spawnDelay;   }	// The delta time when the next spawn round should occur

public:
    EmitterInstance* GetNext() { return m_next; }

	void  onParticleSystemChanged(const Engine& engine, int track);
	int   Update(TimeF currentTime);
	void  Render(IDirect3DDevice9* pDevice);
	void  StopSpawning();
	bool  IsHeatEmitter() const   { return !m_engine.GetHeatDebug() && m_emitter.isHeatParticle; }
	bool  IsRoot()        const   { return m_emitter.parent == NULL; }

	EmitterInstance(EmitterInstance* &list, TimeF currentTime, ParticleSystemInstance& system, Engine& engine, ParticleSystem::Emitter& emitter, Object3D* parent, int* numParticles);
	~EmitterInstance();
};

#endif