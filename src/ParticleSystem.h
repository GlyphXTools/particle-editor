#ifndef PARTICLESYSTEM_H
#define PARTICLESYSTEM_H

#include <string>
#include <vector>
#include <set>
#include "ChunkFile.h"

#include "files.h"

class EmitterInstance;

class ParticleSystem
{
public:
	// Group types
	static const int GT_EXACT        = 0;
	static const int GT_BOX          = 1;
	static const int GT_CUBE         = 2;
	static const int GT_SPHERE       = 3;
	static const int GT_CYLINDER     = 4;
	static const int NUM_GROUP_TYPES = 5;

	// Group IDs
	static const int GROUP_SPEED    = 0;
	static const int GROUP_LIFETIME = 1;
	static const int GROUP_POSITION = 2;
	static const int NUM_GROUPS     = 3;

	// Track IDs
	static const int TRACK_RED_CHANNEL    = 0;
	static const int TRACK_GREEN_CHANNEL  = 1;
	static const int TRACK_BLUE_CHANNEL   = 2;
	static const int TRACK_ALPHA_CHANNEL  = 3;
	static const int TRACK_SCALE          = 4;
	static const int TRACK_INDEX          = 5;
	static const int TRACK_ROTATION_SPEED = 6;
	static const int NUM_TRACKS           = 7;

    // Blend modes
    static const int BLEND_NONE                = 0;
    static const int BLEND_ADDITIVE            = 1;
    static const int BLEND_TRANSPARENT         = 2;
    static const int BLEND_INVERSE             = 3;
    static const int BLEND_DEPTH_ADDITIVE      = 4;
    static const int BLEND_DEPTH_TRANSPARENT   = 5;
    static const int BLEND_DEPTH_INVERSE       = 6;
    static const int BLEND_DIFFUSE_TRANSPARENT = 7;
    static const int BLEND_STENCIL_DARKEN      = 8;
    static const int BLEND_STENCIL_DARKEN_BLUR = 9;
    static const int BLEND_HEAT                = 10;
    static const int BLEND_BUMP                = 11;
    static const int BLEND_DECAL_BUMP          = 12;
    static const int BLEND_SCANLINES           = 13;

    // Ground behavior
    static const int GROUND_NONE      = 0;
    static const int GROUND_DISAPPEAR = 1;
    static const int GROUND_BOUNCE    = 2;
    static const int GROUND_STICK     = 3;

    // Emit mode
    static const int EMIT_DISABLE       = 0;
    static const int EMIT_RANDOM_VERTEX = 1;
    static const int EMIT_RANDOM_MESH   = 2;
    static const int EMIT_EVERY_VERTEX  = 3;

	class Emitter
	{
	public:
		struct Track
		{
			enum InterpolationType
			{
				IT_UNKNOWN = -1,
				IT_LINEAR  =  0,
				IT_SMOOTH  =  1,
				IT_STEP    =  2
			};

			struct Key
			{
				float value;
				float time;

                // Used for pure equality
                bool operator == (const Key& key) const { return time == key.time && value == key.value; }

                // Used for ordering in the set
				bool operator <  (const Key& key) const { return time < key.time; }

				Key() {}
				Key(const Key& k) : time(k.time), value(k.value) {}
				Key(float t, float v) : time(t), value(v) {}
			};

			typedef std::multiset<Key>	KeyMap;

			KeyMap			  keys;
			InterpolationType interpolation;
		};

		#pragma pack(1)
		struct Group
		{
			unsigned int type;
			float        minX, minY, minZ;
			float		 maxX, maxY, maxZ;
			float		 sideLength;
			float		 sphereRadius;
			unsigned int sphereEdge;
			float		 cylinderRadius;
			unsigned int cylinderEdge;
			float		 cylinderHeight;
			float		 valX, valY, valZ;
		};
		#pragma pack()

		// Emitter hierarchy
		size_t   spawnOnDeath;
		size_t   spawnDuringLife;
		Emitter* parent;
        bool     visible;   // Not stored, for use in editor only

		std::string name;
		std::string colorTexture;
		std::string normalTexture;

		// Random parameter groups
		Group groups[NUM_GROUPS];

		// Tracks
        // Use the 'tracks' array outside of this class. This way you can
        // alias several tracks to the same contents easily.
		Track  trackContents[NUM_TRACKS];
        Track* tracks[NUM_TRACKS];

		// Properties
	    bool  linkToSystem;
		bool  objectSpaceAcceleration;
		bool  doColorAddGrayscale;
		bool  affectedByWind;
		bool  isHeatParticle;
		bool  isWeatherParticle;
		bool  hasTail;
		bool  noDepthTest;
		bool  randomRotation;
		bool  randomRotationDirection;
		bool  isWorldOriented;
		bool  useBursts;
		int   emitFromMesh;
		float gravity;
		float lifetime;
		float initialDelay;
		float burstDelay;
		float inwardSpeed;
		float inwardAcceleration;
		float acceleration[3];
		float randomScalePerc;
		float randomLifetimePerc;
		float weatherCubeSize;
		float tailSize;
		float parentLinkStrength;
		float weatherCubeDistance;
		float randomRotationAverage;
		float randomRotationVariance;
		float randomColors[4];
		float bounciness;
		float freezeTime;
		float skipTime;
		float emitFromMeshOffset;
		float weatherFadeoutDistance;
		unsigned long nBursts;
		size_t        index;
		unsigned long blendMode;
		unsigned long textureSize;
		unsigned long nParticlesPerSecond;
		unsigned long nTriangles;
		unsigned long nParticlesPerBurst;
		unsigned long groundBehavior;

		// These have an unused function
		bool  unknown15;
		bool  unknown2b;
		bool  unknown44;
		float unknown11;
		float unknown3f;
		unsigned long unknown06;
		unsigned long unknown49;

        // We need to keep track of instances of this emitter type,
        // in case we delete this emitter type
        void registerEmitterInstance(EmitterInstance* instance);
        void unregisterEmitterInstance(EmitterInstance* instance);

        void write(ChunkWriter& writer) { write(writer, false); }
        void copy (ChunkWriter& writer) { write(writer, true); }

        Emitter(const Emitter& emitter);
		Emitter(ChunkReader& reader);
		Emitter();
        ~Emitter();

	private:
        std::set<EmitterInstance*> m_instances;

		void setDefaults();
        void write(ChunkWriter& writer, bool copy);

		void readProperties(ChunkReader& reader);
		void readTracks(ChunkReader& reader);
		void readGroups(ChunkReader& reader);

		void writeProperties(ChunkWriter& writer) const;
		void writeTracks(ChunkWriter& writer) const;
		void writeGroups(ChunkWriter& writer) const;
	};

	//
	// Functions
	//
	ParticleSystem();
	ParticleSystem(IFile* file);
	~ParticleSystem();

	// Write the particle system to a file
	void write(IFile* file);

	// Manage emitters
	Emitter*       addRootEmitter(const ParticleSystem::Emitter& emitter = ParticleSystem::Emitter());
    Emitter*       addLifetimeEmitter(Emitter* parent, const ParticleSystem::Emitter& emitter = ParticleSystem::Emitter());
    Emitter*       addDeathEmitter(Emitter* parent, const ParticleSystem::Emitter& emitter = ParticleSystem::Emitter());
    Emitter&       getEmitter(size_t index)       { return *m_emitters[index]; }
    const Emitter& getEmitter(size_t index) const { return *m_emitters[index]; }
	void           deleteEmitter(Emitter* emitter);

	// Getters
	const std::vector<Emitter*>& getEmitters()       const { return m_emitters; }
	      std::vector<Emitter*>& getEmitters()             { return m_emitters; }
	const std::string&           getName()           const { return m_name; }
	bool					 	 getLeaveParticles() const { return m_leaveParticles;  }
	
	// Setters
	void setName(const std::string& name) { m_name = name; }
	void setLeaveParticles(bool leave)    { m_leaveParticles = leave; }

private:
	bool			 	  m_leaveParticles;
	std::string           m_name;
	std::vector<Emitter*> m_emitters;
};
#endif