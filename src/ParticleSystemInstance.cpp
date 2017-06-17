#include "ParticleSystemInstance.h"
#include "EmitterInstance.h"
using namespace std;

void ParticleSystemInstance::onParticleSystemChanged(const Engine& engine, int track)
{
    for (auto& emitter : m_emitters)
	{
        emitter->onParticleSystemChanged(engine, track);
	}
}

int ParticleSystemInstance::Update(TimeF currentTime)
{
    // Calculate Z-Distance
    const D3DXMATRIX& view = m_engine.GetViewMatrix();
    D3DXVECTOR3 pos = GetPosition();
    m_zDistance = (pos.x * view._13 + pos.y * view._23 + pos.z * view._33 + view._43) /     // Z
                  (pos.x * view._14 + pos.y * view._24 + pos.z * view._34 + view._44);      // W

    // Update emitters
    int nParticles = 0;
    for (auto it = m_emitters.begin(); it != m_emitters.end();)
	{
		nParticles += (*it)->Update(currentTime);

		// If it's dead and no longer needed (either detached, or we're its parent), then remove it
		if ((*it)->IsDead() && ((*it)->Detached() || (*it)->GetParent() == this))
		{
			it = m_emitters.erase(it);
			m_engine.OnEmitterDestroyed();
		}
		else
		{
			++it;
		}
	}
    return nParticles;
}

void ParticleSystemInstance::RenderNormal(IDirect3DDevice9* pDevice)
{
    for (auto& emitter : m_emitters)
	{
        if (!emitter->IsHeatEmitter())
        {
            emitter->Render(pDevice);
        }
	}
}

void ParticleSystemInstance::RenderHeat(IDirect3DDevice9* pDevice)
{
    for (auto& emitter : m_emitters)
	{
        if (emitter->IsHeatEmitter())
        {
            emitter->Render(pDevice);
        }
	}
}

void ParticleSystemInstance::SetPosition(const D3DXVECTOR3& position)
{
    m_position = position;
}

void ParticleSystemInstance::StopSpawning()
{
	for (auto& emitter : m_emitters)
	{
		if (emitter->IsRoot())
		{
			emitter->StopSpawning();
		}
	}
}

int ParticleSystemInstance::Kill()
{
	int numParticles = 0;
	for (auto& emitter : m_emitters)
	{
		numParticles += emitter->Kill();
	}
	return numParticles;
}

EmitterInstance* ParticleSystemInstance::SpawnEmitter(TimeF currentTime, size_t idxEmitter, Object3D* parent)
{
    int numParticles;
	ParticleSystem::Emitter* emitter = m_system.getEmitters()[idxEmitter];
    auto instance = std::make_unique<EmitterInstance>(currentTime, *this, m_engine, *emitter, parent, &numParticles);
	m_emitters.push_back(std::move(instance));
    m_engine.OnEmitterCreated(numParticles);
	return m_emitters.back().get();
}

ParticleSystemInstance::ParticleSystemInstance(Engine& engine, const ParticleSystem& system, Object3D* parent)
	: Object3D(parent), m_engine(engine), m_system(system)
{		
	TimeF now  = GetTimeF();

	// Spawn all root emitters
	const vector<ParticleSystem::Emitter*>& emitters = m_system.getEmitters();
	for (size_t i = 0; i < emitters.size(); i++)
	{
		if (emitters[i]->parent == NULL)
		{
            SpawnEmitter(now, i, this);
		}
	}
}

ParticleSystemInstance::~ParticleSystemInstance()
{
}