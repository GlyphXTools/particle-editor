#ifndef RESCALE_H
#define RESCALE_H

#include "ParticleSystem.h"

bool RescaleParticleSystem(HWND hOwner, ParticleSystem* system);
bool RescaleEmitter(HWND hOwner, ParticleSystem::Emitter* emitter);

#endif