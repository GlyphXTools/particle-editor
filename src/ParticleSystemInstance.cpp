#include "ParticleSystemInstance.h"
#include "EmitterInstance.h"
using namespace std;

void ParticleSystemInstance::onParticleSystemChanged(const Engine& engine, int track)
{
    for (EmitterInstance* emitter = m_emitters; emitter != NULL; emitter = emitter->GetNext())
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
    EmitterInstance* emitter = m_emitters;
    while (emitter != NULL)
	{
        EmitterInstance* next = emitter->GetNext();
		nParticles += emitter->Update(currentTime);
        emitter = next;
	}
    return nParticles;
}

void ParticleSystemInstance::RenderNormal(IDirect3DDevice9* pDevice)
{
    for (EmitterInstance* emitter = m_emitters; emitter != NULL; emitter = emitter->GetNext())
	{
        if (!emitter->IsHeatEmitter())
        {
            emitter->Render(pDevice);
        }
	}
}

void ParticleSystemInstance::RenderHeat(IDirect3DDevice9* pDevice)
{
    for (EmitterInstance* emitter = m_emitters; emitter != NULL; emitter = emitter->GetNext())
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
    EmitterInstance* emitter = m_emitters;
    while (emitter != NULL)
	{
        EmitterInstance* next = emitter->GetNext();
		if (emitter->IsRoot())
		{
			emitter->StopSpawning();
		}
        emitter = next;
	}
}

void ParticleSystemInstance::Detach()
{
    Object3D::Detach();
    if (m_emitters == NULL && !m_deleting)
    {
        delete this;
    }
}

void ParticleSystemInstance::OnEmitterDestroyed(int numParticles)
{
    m_engine.OnEmitterDestroyed(numParticles);
    if (m_emitters == NULL && Detached() && !m_deleting)
    {
        delete this;
    }
}

EmitterInstance* ParticleSystemInstance::SpawnEmitter(TimeF currentTime, size_t idxEmitter, Object3D* parent)
{
    int numParticles;
	ParticleSystem::Emitter* emitter = m_system.getEmitters()[idxEmitter];
    EmitterInstance* instance = new EmitterInstance(m_emitters, currentTime, *this, m_engine, *emitter, parent, &numParticles);
    m_engine.OnEmitterCreated(numParticles);
    return instance;
}

ParticleSystemInstance::ParticleSystemInstance(Engine& engine, const ParticleSystem& system, Object3D* parent)
	: Object3D(parent), m_engine(engine), m_system(system)
{		
	TimeF now  = GetTimeF();
    m_emitters = NULL;
    m_deleting = false;

	// Spawn all root emitters
	const vector<ParticleSystem::Emitter*>& emitters = m_system.getEmitters();
	for (size_t i = 0; i < emitters.size(); i++)
	{
		if (emitters[i]->parent == NULL)
		{
            EmitterInstance* emitter = SpawnEmitter(now, i, this);
		}
	}
}

ParticleSystemInstance::~ParticleSystemInstance()
{
    m_deleting = true;

 	while (m_emitters != NULL)
	{
		delete m_emitters;
	}

    m_engine.OnInstanceDestroyed(this);
}