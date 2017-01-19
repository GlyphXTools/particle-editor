#ifndef SPHERICALHARMONICS_H
#define SPHERICALHARMONICS_H

#include "engine.h"

void SPH_Calculate_Matrices(D3DXMATRIX* matrices, const Engine::Light* lights, int nLights, const D3DXVECTOR4& ambient);

#endif