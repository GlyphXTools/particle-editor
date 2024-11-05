#include <cassert>
#include "EmitterInstance.h"
#include "ParticleSystemInstance.h"
using namespace std;

struct EmitterInstance::Particle : public Object3D
{
	struct TrackCursor
	{
		// The cursor is always between these two keys
		ParticleSystem::Emitter::Track::KeyMap::const_iterator prev;
		ParticleSystem::Emitter::Track::KeyMap::const_iterator next;
	};

	Particle*	   m_next;
	Particle*	   m_prev;
    ParticleBlock* m_block;
    size_t         m_index;

	D3DXVECTOR3 m_initialPosition;
	D3DXVECTOR3 m_systemSpawnPosition;
    D3DXVECTOR3 m_parentSpawnPosition;
	D3DXVECTOR3 m_initialSpeed;
	D3DXVECTOR3	m_acceleration;
	D3DXVECTOR4	m_baseColor;
	float		m_baseScale;
	float       m_rotationDirection;
	float       m_baseRotation;
    TimeF       m_positionTime;
    TimeF       m_bounceTime;
	TimeF		m_spawnTime;
	TimeF		m_deathTime;
    EmitterInstance* m_childEmitter;
	
	TrackCursor m_cursors[ParticleSystem::NUM_TRACKS];

	size_t      m_verticesIndex;
	size_t      m_indicesIndex;

	// Make the (inherited) position public
	void setPosition(const D3DXVECTOR3& position) { m_position = position; }
	void setVelocity(const D3DXVECTOR3& velocity) { m_velocity = velocity; }

    Particle() : Object3D(NULL), m_childEmitter(NULL)
    {
    }

    ~Particle()
    {
        // If we have a child attached, detach it first
        if (m_childEmitter != NULL)
        {
            m_childEmitter->Detach();
            m_childEmitter->StopSpawning();
        }
    }
};

//
// This class holds and manages a block of particles.
// From this block, particles can be allocated and freed. Particles are
// guaranteed not to move in memory after allocated.
// Create ParticleBlocks with new, never on the stack
//
class EmitterInstance::ParticleBlock
{
    uint32_t* m_freeMap;
    Particle* m_particles;
    size_t    m_size;
    size_t    m_base;

public:
    // Allocate a particle. Returns NULL if there are no free particles
    // in this block
    Particle* AllocateParticle()
    {
	    // Find a free group in the map
	    for (size_t i = 0; i < m_size / 32; i++)
	    {
        	size_t   c = 0;
		    uint32_t x = m_freeMap[i];
		    if (x != 0)
		    {
			    if ((x & 0xFFFF) == 0) { c += 16; x >>= 16; }
			    if ((x & 0x00FF) == 0) { c +=  8; x >>=  8; }
			    if ((x & 0x000F) == 0) { c +=  4; x >>=  4; }
			    if ((x & 0x0003) == 0) { c +=  2; x >>=  2; }
			    if ((x & 0x0001) == 0) { c +=  1; }
	            m_freeMap[i] &= ~(1 << c);
	            return &m_particles[i * 32 + c];
		    }
	    }
        return NULL;
    }
    
    // Free the particle
    void FreeParticle(Particle* particle)
    {
        assert(particle->m_block == this);
        size_t index = particle->m_index - m_base;
    	m_freeMap[index / 32] |= (1 << (index % 32));
    }

    // Creates a particle block with the specified size.
    // Allocated particles use the specified base as index into
    // the vertex and index arrays.
    ParticleBlock(size_t base, size_t size)
    {
        m_base      = base;
        m_size      = (size + 31) & -32;
        m_freeMap   = new uint32_t[m_size / 32];
        m_particles = new Particle[m_size];

        for (size_t i = 0; i < m_size / 32; i++)
        {
            m_freeMap[i] = 0xFFFFFFFF;
        }

        for (size_t i = 0; i < m_size; i++)
        {
            m_particles[i].m_block = this;
            m_particles[i].m_index = m_base + i;
        }
    }

    ~ParticleBlock()
    {
        delete[] m_freeMap;
        delete[] m_particles;
    }
};

EmitterInstance::Particle& EmitterInstance::AllocateParticle()
{
    Particle* particle = NULL;
    for (size_t i = 0; i < m_blocks.size(); i++)
    {
        particle = m_blocks[i]->AllocateParticle();
        if (particle != NULL)
        {
            break;
        }
    }

    if (particle == NULL)
    {
	    // We couldn't find a free spot, allocate new particles
        ParticleBlock* block = new ParticleBlock(m_primitives.capacity(), m_primitives.capacity());
        m_blocks.push_back(block);

        m_vertices     .resize (m_vertices     .size()     * 2);
	    m_primitives   .reserve(m_primitives   .capacity() * 2);
	    m_particleIndex.reserve(m_particleIndex.capacity() * 2);

        particle = block->AllocateParticle();
    }

	// Link the particle into the list
	particle->m_next = m_particleList;
	particle->m_prev = NULL;
	if (particle->m_next != NULL)
	{
		particle->m_next->m_prev = particle;
	}
	m_particleList = particle;
    return *particle;
}

void EmitterInstance::FreeParticle(Particle& particle)
{
	// Unlink from list (keep m_next intact though)
	if (particle.m_next != NULL) particle.m_next->m_prev = particle.m_prev;
	if (particle.m_prev != NULL) particle.m_prev->m_next = particle.m_next;
	else                         m_particleList = particle.m_next;

	particle.m_block->FreeParticle(&particle);
}

static void GenerateRandomProperty(const ParticleSystem::Emitter::Group& group, D3DXVECTOR3& value)
{
	switch (group.type)
	{
		case ParticleSystem::GT_EXACT:
			value.x = group.valX;
			value.y = group.valY;
			value.z = group.valZ;
			break;

		case ParticleSystem::GT_BOX:
			value.x = GetRandom(group.minX, group.maxX);
			value.y = GetRandom(group.minY, group.maxY);
			value.z = GetRandom(group.minZ, group.maxZ);
			break;

		case ParticleSystem::GT_CUBE:
			value.x = GetRandom(-group.sideLength, group.sideLength) / 2;
			value.y = GetRandom(-group.sideLength, group.sideLength) / 2;
			value.z = GetRandom(-group.sideLength, group.sideLength) / 2;
			break;

		case ParticleSystem::GT_SPHERE:
		{
			float angleXY = GetRandom(D3DXToRadian(-180), D3DXToRadian(180));
			float angleZ  = GetRandom(D3DXToRadian(- 90), D3DXToRadian( 90));
			float radius  = (group.sphereEdge ? 1.0f : GetRandom(0.0f, 1.0f)) * group.sphereRadius;
			value.x = radius * cosf(angleZ) * cosf(angleXY);
			value.y = radius * cosf(angleZ) * sinf(angleXY);
			value.z = radius * sinf(angleZ);
			break;
		}

		case ParticleSystem::GT_CYLINDER:
		{
			float angleXY = GetRandom(D3DXToRadian(-180), D3DXToRadian(180));
			float radius  = (group.cylinderEdge ? 1.0f : GetRandom(0.0f, 1.0f)) * group.cylinderRadius;
			value.x = radius * cosf(angleXY);
			value.y = radius * sinf(angleXY);
			value.z = GetRandom(0.0f, group.cylinderHeight);
			break;
		}
	}
}

// Resets a particle's appearance and lifetime
void EmitterInstance::ResetParticle(Particle& particle, TimeF currentTime)
{
    particle.m_positionTime = 0;
	particle.m_spawnTime    = currentTime;
	particle.m_deathTime    = particle.m_spawnTime + m_emitter.lifetime * GetRandom(1.0f - m_emitter.randomLifetimePerc, 1.0f);

    particle.m_initialPosition   = particle.GetPosition();

	particle.m_baseScale         = GetRandom(1.0f - m_emitter.randomScalePerc, 1.0f);
	particle.m_rotationDirection = (!m_emitter.randomRotationDirection || GetRandom(0.0, 1.0f) < 0.5) ? 1.0f : -1.0f;
	particle.m_baseRotation      = m_emitter.randomRotation ? m_emitter.randomRotationAverage * (1 + GetRandom(-m_emitter.randomRotationVariance, m_emitter.randomRotationVariance)) : 0.0f;
	if (m_emitter.doColorAddGrayscale)
	{
		particle.m_baseColor.x = particle.m_baseColor.y = particle.m_baseColor.z =
		particle.m_baseColor.w = GetRandom(0.0f, m_emitter.randomColors[0]);
	}
	else
	{
		particle.m_baseColor.x = GetRandom(0.0f, m_emitter.randomColors[0]);
		particle.m_baseColor.y = GetRandom(0.0f, m_emitter.randomColors[1]);
		particle.m_baseColor.z = GetRandom(0.0f, m_emitter.randomColors[2]);
		particle.m_baseColor.w = GetRandom(0.0f, m_emitter.randomColors[3]);
	}

	// Initialize track 'cursors'
	for (int i = 0; i < ParticleSystem::NUM_TRACKS; i++)
	{
		particle.m_cursors[i].next =
		particle.m_cursors[i].prev = m_emitter.tracks[i]->keys.begin();
	}
}

// Spawn a single particle
void EmitterInstance::SpawnParticle(TimeF currentTime)
{
	Particle& particle = AllocateParticle();
	particle.m_verticesIndex = particle.m_index * NUM_VERTICES_PER_PARTICLE;

    // Set and generate properties
    particle.m_systemSpawnPosition = m_system.GetPosition();
    particle.m_parentSpawnPosition = GetPosition();

    GenerateRandomProperty(m_emitter.groups[ParticleSystem::GROUP_SPEED], particle.m_initialSpeed);
	if (m_emitter.affectedByWind)
	{
		particle.m_initialSpeed += m_engine.GetWind();
	}

    if (m_emitter.isWeatherParticle)
    {
        particle.m_acceleration = D3DXVECTOR3(0, 0, 0);
        particle.m_initialPosition.x = GetRandom(-m_emitter.weatherCubeSize / 2, m_emitter.weatherCubeSize / 2);
        particle.m_initialPosition.y = GetRandom(-m_emitter.weatherCubeSize / 2, m_emitter.weatherCubeSize / 2);
        particle.m_initialPosition.z = GetRandom(-m_emitter.weatherCubeSize / 2, m_emitter.weatherCubeSize / 2);

        // Move to weather cube center
        const Engine::Camera& camera = m_engine.GetCamera();
        D3DXVECTOR3 looking = camera.Target - camera.Position;
        D3DXVec3Normalize(&looking, &looking);
        particle.m_initialPosition += camera.Position + looking * m_emitter.weatherCubeDistance;
    }
    else
    {
        D3DXVECTOR3 normpos;
	    GenerateRandomProperty(m_emitter.groups[ParticleSystem::GROUP_POSITION], particle.m_initialPosition);
	    D3DXVec3Normalize(&normpos, &particle.m_initialPosition);

	    particle.m_initialSpeed    -= normpos * m_emitter.inwardSpeed;
	    particle.m_acceleration     = m_acceleration - normpos * m_emitter.inwardAcceleration;
    	particle.m_initialPosition += particle.m_parentSpawnPosition;
    }

    if (m_emitter.groundBehavior == ParticleSystem::GROUND_BOUNCE)
    {
        if (particle.m_acceleration.z != 0)
        {
            // Parabola;
            // Solve x(t) = x(0) + v(0) * t + 0.5 * a * t * t = 0 for t:
            // t = (-b +/- sqrt(b^2 - 4ac)) / 2a =>
            // t = (-v + sqrt(v*v - 2*a*x)) / a
            float D  = sqrtf(particle.m_initialSpeed.z * particle.m_initialSpeed.z - 2 * particle.m_acceleration.z * particle.m_initialPosition.z);
            float t0 = (-particle.m_initialSpeed.z - D) / particle.m_acceleration.z;
            float t1 = (-particle.m_initialSpeed.z + D) / particle.m_acceleration.z;
            particle.m_bounceTime = max(t0, t1);
        }
        else if (particle.m_initialSpeed.z != 0)
        {
            // Linear system;
            // Solve x(t) = x(0) + v(0) * t = 0 for t
            particle.m_bounceTime = -particle.m_initialPosition.z / particle.m_initialSpeed.z;
            if (particle.m_bounceTime < 0)
            {
                // Never bounces
                particle.m_bounceTime = FLT_MAX;
            }
        }
        else
        {
            // Never bounces
            particle.m_bounceTime = FLT_MAX;
        }
    }

    particle.setPosition(particle.m_initialPosition);
    ResetParticle(particle, currentTime);

    // Spawn the child emitter
    particle.m_childEmitter = NULL;
    if (m_emitter.spawnDuringLife != -1)
    {
        particle.m_childEmitter = m_system.SpawnEmitter(currentTime, m_emitter.spawnDuringLife, &particle);
    }

	// Create index
	Primitive prim;
	prim.index[0] = (uint16_t)particle.m_verticesIndex + 0;
	prim.index[1] = (uint16_t)particle.m_verticesIndex + 2;
	prim.index[2] = (uint16_t)particle.m_verticesIndex + 3;
	prim.index[3] = (uint16_t)particle.m_verticesIndex + 2;
	prim.index[4] = (uint16_t)particle.m_verticesIndex + 0;
	prim.index[5] = (uint16_t)particle.m_verticesIndex + 1;
	particle.m_indicesIndex = m_primitives.size();
	m_primitives.push_back(prim);
	m_particleIndex.push_back(&particle);
}

void EmitterInstance::UpdateTrackCursors(Particle& particle, float relTime) const
{
	for (int i = 0; i < ParticleSystem::NUM_TRACKS; i++)
	{
		Particle::TrackCursor& cursor = particle.m_cursors[i];
		while (relTime > cursor.next->time)
		{
			if (!m_emitter.randomRotation && i == ParticleSystem::TRACK_ROTATION_SPEED)
			{
				particle.m_baseRotation += IntegrateTrack(particle, ParticleSystem::TRACK_ROTATION_SPEED, cursor.next->time);
			}

			cursor.prev = cursor.next;
			cursor.next++;
			if (cursor.next == m_emitter.tracks[i]->keys.end())
			{
				cursor.next = cursor.prev;
				break;
			}
		}
	}
}

float EmitterInstance::IntegrateTrack(const Particle& particle, int track, float relTime) const
{
	const Particle::TrackCursor& cursor = particle.m_cursors[track];
	
	float v = 0.0f;
	if (cursor.next->time != cursor.prev->time)
	{
		// Normalize time
		float u = (relTime - cursor.prev->time) / (cursor.next->time - cursor.prev->time);
		switch (m_emitter.tracks[track]->interpolation)
		{
			case ParticleSystem::Emitter::Track::IT_SMOOTH:
				// Integration of cubic interpolation:
				// F(u) = 0.5(a - b)u^4 + (b - a)u^3 + a u + C
				v = (cursor.prev->value - cursor.next->value) * u*u*u*u / 2 + (cursor.next->value - cursor.prev->value) * u*u*u + cursor.prev->value * u;
				break;

			case ParticleSystem::Emitter::Track::IT_LINEAR:
				// Integration of linear interpolation:
				// F(u) = a u + 0.5 (b - a) u^2 + C
				v = u * (cursor.prev->value + u * (cursor.next->value - cursor.prev->value) / 2);
				break;

			case ParticleSystem::Emitter::Track::IT_STEP:
				// Integration of step interpolation:
				// F(u) = a u + C
				v = cursor.prev->value * u;
				break;
		}
		// Denormalize time
		v = v * (cursor.next->time - cursor.prev->time) / 100 * (particle.m_deathTime - particle.m_spawnTime);
	}
	return v;
}

float EmitterInstance::SampleTrack(const Particle& particle, int track, float relTime) const
{
	const Particle::TrackCursor& cursor = particle.m_cursors[track];
	if (cursor.next->time == cursor.prev->time)
	{
		return cursor.next->value;
	}

	// See: http://www.gamedev.net/reference/articles/article1497.asp
	switch (m_emitter.tracks[track]->interpolation)
	{
		case ParticleSystem::Emitter::Track::IT_SMOOTH:
		{
			// Cubic interpolation between keys
			float u = (relTime - cursor.prev->time) / (cursor.next->time - cursor.prev->time);
			return cursor.prev->value * (2*u*u*u - 3*u*u + 1) + cursor.next->value * (3*u*u - 2*u*u*u);
		}

		case ParticleSystem::Emitter::Track::IT_LINEAR:
		{
			// Linear interpolation between keys
			float u = (relTime - cursor.prev->time) / (cursor.next->time - cursor.prev->time);
			return cursor.prev->value + u * (cursor.next->value - cursor.prev->value);
		}

		case ParticleSystem::Emitter::Track::IT_STEP:
			return cursor.prev->value;
	}
	return 0.0f;
}

void EmitterInstance::UpdateParticle(Particle& particle, float t)
{
	static const float PI = 3.1415926535897932384626433832795f;

	// Convert to percentage time
	float relTime = t * 100 / (particle.m_deathTime - particle.m_spawnTime);

	UpdateTrackCursors(particle, relTime);

    if (m_emitter.groundBehavior == ParticleSystem::GROUND_BOUNCE)
    {
        while (t > particle.m_bounceTime)
        {
            // The particle has bounced
            float bt = particle.m_bounceTime - particle.m_positionTime;
            particle.m_initialPosition =  particle.m_initialPosition + (particle.m_initialSpeed + 0.5 * particle.m_acceleration * bt) * bt;
            particle.m_initialSpeed    =  particle.m_initialSpeed + particle.m_acceleration * bt;
            particle.m_initialSpeed.z  = -particle.m_initialSpeed.z * m_emitter.bounciness;
            particle.m_positionTime    =  particle.m_bounceTime;

            // Calculate new bounce time
            if (particle.m_acceleration.z == 0 || particle.m_initialSpeed.z == 0)
            {
                // No more bounces
                particle.m_bounceTime = FLT_MAX;
            }
            else
            {
                // Calculate the new parabola
                // We know x(0) is 0, so the problem becomes a lot simpler
                particle.m_bounceTime += 2 * -particle.m_initialSpeed.z / particle.m_acceleration.z;
            }
        }
    }

	float offset = particle.m_baseScale * SampleTrack(particle, ParticleSystem::TRACK_SCALE, relTime) / 2;

    // Calculate position with constant acceleration:
	// x(t) = x(0) + v(0) * t + 0.5 * a * t * t
    float pt = t - particle.m_positionTime;
	D3DXVECTOR3 position = particle.m_initialPosition + (particle.m_initialSpeed + 0.5 * particle.m_acceleration * pt) * pt;
    position += (m_system.GetPosition() - particle.m_systemSpawnPosition) * (m_emitter.linkToSystem ? 1.0f : 0.0f);
	position += (GetPosition()          - particle.m_parentSpawnPosition) * m_emitter.parentLinkStrength;

    if (m_emitter.isWeatherParticle)
    {
        // Move to weather cube center, modulo box size.
        const Engine::Camera& camera = m_engine.GetCamera();
        D3DXVECTOR3 looking = camera.Target - camera.Position;
        D3DXVec3Normalize(&looking, &looking);
        D3DXVECTOR3 center = camera.Position + looking * m_emitter.weatherCubeDistance;
        
        float w = m_emitter.weatherCubeSize;
        position.x = fmodf(fmodf(position.x - center.x + w/2, w) + w, w) - w/2 + center.x;
        position.y = fmodf(fmodf(position.y - center.y + w/2, w) + w, w) - w/2 + center.y;
        position.z = fmodf(fmodf(position.z - center.z + w/2, w) + w, w) - w/2 + center.z;
    }
    else switch (m_emitter.groundBehavior)
    {
        case ParticleSystem::GROUND_DISAPPEAR: // Disappear
            if (position.z < 0.0f) offset = 0.0f;
            break;

        case ParticleSystem::GROUND_STICK: // Stick
            if (position.z < 0.0f) position.z = 0.0f;
            break;

        default: break;
    }
	particle.setPosition(position);

	float rotation = particle.m_baseRotation;
	if (!m_emitter.randomRotation)
	{
		rotation += IntegrateTrack(particle, ParticleSystem::TRACK_ROTATION_SPEED, relTime);
	}
	float angle = 2 * PI * rotation * particle.m_rotationDirection;

	Vertex* verts = &m_vertices[particle.m_verticesIndex];
	verts[0].Position = D3DXVECTOR3(-offset,-offset,0);
	verts[1].Position = D3DXVECTOR3( offset,-offset,0);
	verts[2].Position = D3DXVECTOR3( offset, offset,0);
	verts[3].Position = D3DXVECTOR3(-offset, offset,0);
    
	// Calculate velocity with constant acceleration:
	// v(t) = v(0) + a * t
    D3DXVECTOR3 velocity = particle.m_initialSpeed + particle.m_acceleration * t;
    if (m_emitter.parentLinkStrength != 0.0f)
    {
        velocity += GetVelocity() * m_emitter.parentLinkStrength;
    }
    particle.setVelocity(velocity);

	if (m_emitter.hasTail)
	{
		float length = D3DXVec3Length(&velocity);

        if (length > 0)
        {
            float mult = (m_emitter.parentLinkStrength != 0.0f) ? length / 1000.0f : 1.0f;

            if (!m_emitter.isWorldOriented)
            {
    		    // Transform world-velocity into screen-velocity
		        D3DXVec3TransformCoord(&velocity, &velocity, &m_engine.GetViewRotationMatrix());
            }
		    angle += atan2f(velocity.y, velocity.x) + PI / 4;
		    velocity.z = 0.0f;
		    length = m_emitter.tailSize * mult * D3DXVec3Length(&velocity) / length ;
        }
        verts[3].Position *= max(1.0f, sqrtf(length * length / 2) );
	}

    // Set Normal vector
    verts[0].Normal = D3DXVECTOR3(0,0,1);
    if (!m_emitter.isWorldOriented)
	{
	    // Rotate towards camera
		D3DXVec3TransformCoord(&verts[0].Normal, &verts[0].Normal, &m_engine.GetBillboardMatrix());
    }
    verts[3].Normal = verts[2].Normal = verts[1].Normal = verts[0].Normal;

	for (int i = 0; i < 4; i++)
	{
		// Rotate particle
		float x = verts[i].Position.x;
		verts[i].Position.x = cosf(angle) * x - sinf(angle) * verts[i].Position.y;
		verts[i].Position.y = sinf(angle) * x + cosf(angle) * verts[i].Position.y;

		if (!m_emitter.isWorldOriented)
		{
			// Rotate towards camera
			D3DXVec3TransformCoord(&verts[i].Position, &verts[i].Position, &m_engine.GetBillboardMatrix());
            D3DXVec3TransformCoord(&verts[i].Normal,   &verts[i].Normal,   &m_engine.GetBillboardMatrix());
		}

    	    // Move into position
        verts[i].Position += position;
    }

	// Texture coordinates
	unsigned int texIndex = (unsigned int)floor(SampleTrack(particle, ParticleSystem::TRACK_INDEX, relTime));
    float d = 1.0f / m_textureSizeSqrt;
	float u = (float)(texIndex % m_textureSizeSqrt) / m_textureSizeSqrt;
	float v = (float)(texIndex / m_textureSizeSqrt) / m_textureSizeSqrt;
	verts[3].TexCoord1 = verts[3].TexCoord0 = D3DXVECTOR2(u    , v    );
	verts[2].TexCoord1 = verts[2].TexCoord0 = D3DXVECTOR2(u + d, v    );
	verts[1].TexCoord1 = verts[1].TexCoord0 = D3DXVECTOR2(u + d, v + d);
	verts[0].TexCoord1 = verts[0].TexCoord0 = D3DXVECTOR2(u,     v + d);

	// Color
    D3DXVECTOR4 color = particle.m_baseColor;
    if (m_emitter.blendMode == ParticleSystem::BLEND_BUMP || m_emitter.blendMode == ParticleSystem::BLEND_DECAL_BUMP)
    {
        // For these blend modes, the RGB components of the vertex color contain
        // the tangent vector, which rotates along with the particle
        color.x = 0.5f * cosf(angle) + 0.5f;
	    color.y = 0.5f * sinf(angle) + 0.5f;
        color.z = 0;
    }
    else
    {
    	color.x += SampleTrack(particle, ParticleSystem::TRACK_RED_CHANNEL,   relTime);
    	color.y += SampleTrack(particle, ParticleSystem::TRACK_GREEN_CHANNEL, relTime);
    	color.z += SampleTrack(particle, ParticleSystem::TRACK_BLUE_CHANNEL,  relTime);
    }
	color.w += SampleTrack(particle, ParticleSystem::TRACK_ALPHA_CHANNEL, relTime);
	verts[3].Color = verts[2].Color = verts[1].Color = verts[0].Color = D3DCOLOR_COLORVALUE(color.x, color.y, color.z, color.w);
}

// Kill a particle
int EmitterInstance::KillParticle(TimeF currentTime, Particle& particle)
{
    if (particle.m_childEmitter != NULL)
    {
        // Detach and stop child emitter
        particle.m_childEmitter->Detach();
        particle.m_childEmitter->StopSpawning();
        particle.m_childEmitter = NULL;
    }

    int numParticles = 0;
    if (m_emitter.spawnOnDeath != -1)
    {
        // Spawn child emitter
        EmitterInstance* emitter = m_system.SpawnEmitter(currentTime, m_emitter.spawnOnDeath, &particle);
        emitter->Detach();
        emitter->StopSpawning();
    }

	FreeParticle(particle);
    return numParticles;
}

void EmitterInstance::onParticleSystemChanged(const Engine& engine, int track)
{
	if (track == -1)
	{
		// Recalculate composite values
        m_nParticlesPerBurst = (!m_emitter.useBursts) ? 1 : m_emitter.nParticlesPerBurst;
		m_spawnDelay         = (!m_emitter.useBursts) ? 1.0f / m_emitter.nParticlesPerSecond : max(0.01f, m_emitter.burstDelay);   // Ensure burst delay isn't 0
		m_acceleration       = D3DXVECTOR3(m_emitter.acceleration) + m_emitter.gravity * engine.GetGravity();
		m_textureSizeSqrt    = (int)floor(sqrtf((float)max(1, m_emitter.textureSize)));

		// Reload resources
		SAFE_RELEASE(m_pColorTexture);
		SAFE_RELEASE(m_pNormalTexture);
		m_pColorTexture  = engine.GetTexture(m_emitter.colorTexture);
		m_pNormalTexture = engine.GetTexture(m_emitter.normalTexture);

        // Default texture stage settings:
        // ColorOp[0]   = Modulate;
        // ColorArg1[0] = Texture;
        // ColorArg2[0] = Diffuse;
        //
        // AlphaOp[0]   = SelectArg1;
        // AlphaArg1[0] = Texture;
        //
        // ColorOp[1]   = Disable;
        // AlphaOp[1]   = Disable;

		// Set blend options
		switch (m_emitter.blendMode)
		{
			default:
				// Opaque
                m_colorOp        = D3DTOP_MODULATE;
				m_alphaSrcBlend  = D3DBLEND_ONE;
				m_alphaDestBlend = D3DBLEND_ZERO;
				break;

			case ParticleSystem::BLEND_ADDITIVE:
				// Additive
                m_colorOp        = D3DTOP_MODULATE;
				m_alphaSrcBlend  = D3DBLEND_ONE;
				m_alphaDestBlend = D3DBLEND_ONE;
				break;

			case ParticleSystem::BLEND_TRANSPARENT:
				// Transparent
                m_colorOp        = D3DTOP_MODULATE;
				m_alphaSrcBlend  = D3DBLEND_SRCALPHA;
				m_alphaDestBlend = D3DBLEND_INVSRCALPHA;
				break;

            case 3:
                m_colorOp        = D3DTOP_ADD;
				m_alphaSrcBlend  = D3DBLEND_ZERO;
				m_alphaDestBlend = D3DBLEND_SRCCOLOR;
                break;

            case 4:
                m_colorOp        = D3DTOP_MODULATE;
				m_alphaSrcBlend  = D3DBLEND_ONE;
				m_alphaDestBlend = D3DBLEND_ONE;
                break;

            case 5:
                m_colorOp        = D3DTOP_MODULATE;
				m_alphaSrcBlend  = D3DBLEND_SRCALPHA;
				m_alphaDestBlend = D3DBLEND_INVSRCALPHA;
                break;

            case 6:
                m_colorOp        = D3DTOP_ADD;
				m_alphaSrcBlend  = D3DBLEND_ZERO;
				m_alphaDestBlend = D3DBLEND_SRCCOLOR;

            case 7:
                m_colorOp        = D3DTOP_MODULATE2X;
				m_alphaSrcBlend  = D3DBLEND_SRCALPHA;
				m_alphaDestBlend = D3DBLEND_INVSRCALPHA;
                break;
		}
	}
	else
	{
		TimeF currentTime = GetTimeF();

		// Reload track cursors on all particles
		for (Particle* particle = m_particleList; particle != NULL; particle = particle->m_next)
		{
			float relTime = (float)(currentTime - particle->m_spawnTime) * 100 / (float)(particle->m_deathTime - particle->m_spawnTime);
			
			Particle::TrackCursor& cursor = particle->m_cursors[track];
			cursor.prev = cursor.next = m_emitter.tracks[track]->keys.begin();
			while (cursor.next->time < relTime)
			{
				cursor.prev = cursor.next;
				if (++cursor.next == m_emitter.tracks[track]->keys.end())
				{
					cursor.next = cursor.prev;
					break;
				}
			}
		}
	}
}

int EmitterInstance::Update(TimeF currentTime)
{
    int numParticles = 0;

    if (IsFrozen(currentTime))
	{
		currentTime = m_freezeTime;
	}

    if (!m_emitter.isWeatherParticle)
    {
        // Spawn new particles
        while (!DoneSpawning() && currentTime > m_nextSpawnTime)
        {
            numParticles += SpawnParticles(m_nextSpawnTime);
        }
    }

	// We make this static so we don't reallocate every single frame
	static set<size_t> kills;

	for (Particle* particle = m_particleList; particle != NULL; particle = particle->m_next)
	{
		if (particle->m_deathTime < currentTime)
		{
			// It's dead
            if (!m_emitter.isWeatherParticle || DoneSpawning())
            {
                // Remove it (m_next remains intact)
			    numParticles += KillParticle(currentTime, *particle);
			    kills.insert(particle->m_indicesIndex);
			    continue;
            }

            // Weather particles get reset on death
            ResetParticle(*particle, currentTime);
		}

		float t = (float)(currentTime - particle->m_spawnTime);
		UpdateParticle(*particle, t);
	}

	if (!kills.empty())
	{
        size_t firstKilled = *kills.begin();
		for (set<size_t>::reverse_iterator i = kills.rbegin(); i != kills.rend(); i++)
		{
			m_primitives   .erase(m_primitives   .begin() + *i);
			m_particleIndex.erase(m_particleIndex.begin() + *i);
            numParticles--;
		}
		kills.clear();

		if (!m_primitives.empty())
		{
			// Reassign indices
			for (size_t i = firstKilled; i < m_primitives.size(); i++)
			{
				m_particleIndex[i]->m_indicesIndex = i;
			}
		}
	}
    return numParticles;
}

void EmitterInstance::StopSpawning()
{
    m_doneSpawning = true;
}

void EmitterInstance::Render(IDirect3DDevice9* pDevice)
{
    if (!m_primitives.empty() && m_emitter.visible)
	{
		pDevice->SetTexture(0, m_pColorTexture);
		pDevice->SetTexture(1, m_pNormalTexture);
		pDevice->SetRenderState(D3DRS_ZENABLE,     !m_emitter.noDepthTest);
		if (IsHeatEmitter())
		{
			pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			pDevice->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
			pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    		pDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, (UINT)m_vertices.size(), 2 * (UINT)m_primitives.size(), &m_primitives[0], D3DFMT_INDEX16, &m_vertices[0], sizeof(Vertex));
		}
		else
		{
            D3DXVECTOR3 position = GetPosition();
            D3DXVECTOR4 eyeObjPosition(
                m_engine.GetCamera().Position.x - position.x,
                m_engine.GetCamera().Position.y - position.y,
                m_engine.GetCamera().Position.z - position.z, 
                0);
            
            Effect* pShader = m_engine.GetShader(m_emitter.blendMode);
            const Effect::Handles& handles = pShader->getHandles();
            ID3DXEffect* pEffect = pShader->getD3DEffect();
            pEffect->SetVector(handles.hEyeObjPosition, &eyeObjPosition);

            UINT nPasses;
            pEffect->Begin(&nPasses, 0);
            for (UINT i = 0; i < nPasses; i++)
            {
                pEffect->BeginPass(i);
    		    pDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, (UINT)m_vertices.size(), 2 * (UINT)m_primitives.size(), &m_primitives[0], D3DFMT_INDEX16, &m_vertices[0], sizeof(Vertex));
                pEffect->EndPass();
            }
            pEffect->End();
            SAFE_RELEASE(pEffect);
		}
	}
}

// Spawns another round of particles
int EmitterInstance::SpawnParticles(TimeF spawnTime)
{
    if (m_emitter.useBursts && m_emitter.nBursts > 0)
	{
		if (++m_currentBurst == m_emitter.nBursts)
		{
			m_doneSpawning = true;
		}
	}

    int numParticles = 0;
	for (unsigned long i = 0; i < m_nParticlesPerBurst; i++)
	{
        SpawnParticle(spawnTime);
        numParticles++;
	}

    m_nextSpawnTime = spawnTime + GetSpawnDelay();

    // If the spawn delay beyond the FP addition accuracy,
    // we increase the spawn delay and particles spawned.
    while (m_nextSpawnTime == spawnTime)
    {
        m_spawnDelay         *= 2;
        m_nParticlesPerBurst *= 2;
        m_nextSpawnTime = spawnTime + GetSpawnDelay();
    }

    return numParticles;
}

bool EmitterInstance::IsFrozen(TimeF currentTime) const
{
	return m_freezeTime > 0.0f && currentTime >= m_freezeTime;
}

int EmitterInstance::Kill()
{
	// Stop spawning
	m_doneSpawning = true;

	// And destroy any live particles
	int numParticles = -static_cast<int>(m_primitives.size());
	m_primitives.clear();
	m_particleIndex.clear();

	m_particleList = nullptr;
	return numParticles;
}

EmitterInstance::EmitterInstance(TimeF currentTime, ParticleSystemInstance& system, Engine& engine, ParticleSystem::Emitter& emitter, Object3D* parent, int* numParticles)
	: Object3D(parent), m_engine(engine), m_system(system), m_emitter(emitter)
{
	m_doneSpawning        = false;
	m_particleList        = NULL;
	m_currentBurst        = 0;
	m_pColorTexture       = NULL;
	m_pNormalTexture      = NULL;
	m_parentSpawnPosition = parent->GetPosition();
	m_freezeTime          = (m_emitter.freezeTime > 0.0f && m_emitter.freezeTime >= m_emitter.skipTime) ? currentTime + m_emitter.freezeTime - m_emitter.skipTime : 0.0f;
	
    // Initial array size (32 particles)
    m_blocks.push_back(new ParticleBlock(0,32));
	m_vertices     .resize(32 * NUM_VERTICES_PER_PARTICLE);
	m_primitives   .reserve(32);
	m_particleIndex.reserve(32);

	onParticleSystemChanged(engine, -1);

	// Spawn initial particles
    if (m_emitter.isWeatherParticle)
    {
        // Spawn all particles immediately for weather particles
        for (unsigned long i = 0; i < m_emitter.nParticlesPerSecond; i++)
	    {
		    SpawnParticle(currentTime);
        }
        *numParticles = m_emitter.nParticlesPerSecond;
    }
    else 
    {
    	TimeF skipped = m_emitter.initialDelay;
	    currentTime  -= m_emitter.skipTime;

        *numParticles = 0;
	    while (skipped <= m_emitter.skipTime && !DoneSpawning())
	    {
		    *numParticles += SpawnParticles(currentTime + skipped);
		    skipped += GetSpawnDelay();
	    }
    	
        if (!DoneSpawning())
	    {
		    // Plan the next spawn
            m_nextSpawnTime = currentTime + skipped;
	    }
    }

    m_emitter.registerEmitterInstance(this);
}

EmitterInstance::~EmitterInstance()
{
	SAFE_RELEASE(m_pColorTexture);
	SAFE_RELEASE(m_pNormalTexture);

    for (size_t i = 0; i < m_blocks.size(); i++)
    {
        delete m_blocks[i];
    }

    Particle* parent = dynamic_cast<Particle*>(GetParent());
    if (parent != NULL)
    {
        // Our parent is a particle, clear the child emitter link
        parent->m_childEmitter = NULL;
    }

    m_emitter.unregisterEmitterInstance(this);
}
