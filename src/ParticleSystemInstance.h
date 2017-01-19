#ifndef PARTICLESYSTEMINSTANCE_H
#define PARTICLESYSTEMINSTANCE_H

#include "Engine.h"

class ParticleSystemInstance : public Object3D
{
	Engine&				     m_engine;
	const ParticleSystem&    m_system;
	EmitterInstance*         m_emitters;
    ParticleSystemInstance*  m_next;
    ParticleSystemInstance** m_prev;
    bool                     m_deleting;
    float                    m_zDistance;

public:
    const ParticleSystem& GetParticleSystem() { return m_system; }

    void SetPosition(const D3DXVECTOR3& position);
    void Detach();

    float GetZDistance() const { return m_zDistance; }

    ParticleSystemInstance* GetNext() { return m_next; }

    void Clear();
    void onParticleSystemChanged(const Engine& engine, int track);
	int  Update(TimeF currentTime);
	void RenderNormal(IDirect3DDevice9* pDevice);
	void RenderHeat(IDirect3DDevice9* pDevice);
	void StopSpawning();
    void OnEmitterDestroyed(int numParticles);
	EmitterInstance* SpawnEmitter(TimeF currentTime, size_t idxEmitter, Object3D* parent);

	ParticleSystemInstance(Engine& engine, const ParticleSystem& system, Object3D* parent);
	~ParticleSystemInstance();
};

#endif