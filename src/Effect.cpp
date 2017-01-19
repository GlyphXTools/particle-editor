#include "Effect.h"

Effect::Effect(ID3DXEffect* pD3DEffect)
{
	//
	// Read effect properties
	//
	LPCSTR strPhase = NULL, strProc = NULL, strType = NULL;
	BOOL   zSort = FALSE, tangentSpace = FALSE, shadowVolume = FALSE;
	pD3DEffect->GetString("_ALAMO_RENDER_PHASE", &strPhase);
	pD3DEffect->GetString("_ALAMO_VERTEX_PROC",  &strProc);
	pD3DEffect->GetString("_ALAMO_VERTEX_TYPE",  &strType);
	pD3DEffect->GetBool  ("_ALAMO_TANGENT_SPACE", &tangentSpace);
	pD3DEffect->GetBool  ("_ALAMO_SHADOW_VOLUME", &shadowVolume);
	pD3DEffect->GetBool  ("_ALAMO_Z_SORT",        &zSort);

	int phase = NUM_PHASES;
	if (strPhase != NULL)
	{
		static const char* Phases[NUM_PHASES] = {"TerrainMesh", "Opaque", "Shadow", "Transparent", "Occluded", "Heat"};
		for (phase = 0; phase < NUM_PHASES && _stricmp(strPhase, Phases[phase]) != 0; phase++);
	}

	m_phase        = (phase < NUM_PHASES) ? phase : 0;
	m_vertexProc   = (strProc != NULL) ? strProc : "";
	m_vertexType   = (strType != NULL) ? strType : "";
	m_zSort        = (zSort        != FALSE);
	m_shadowVolume = (shadowVolume != FALSE);
	m_tangentSpace = (tangentSpace != FALSE);

	//
	// Select technique
	//
	static const int   NUM_LODS = 4;
	static const char* LODs[]   = {"DX9", "DX8", "DX8ATI", "FIXEDFUNCTION"};

	// Iterate over the technology levels (most advanced -> least advanced)
	D3DXHANDLE hTechnique = NULL;
	for (unsigned int lod = 0; lod < NUM_LODS && hTechnique == NULL; lod++)
	{
		// Iterate over all techniques to see if we've got a technique for that LOD
		for (unsigned int i = 0; (hTechnique = pD3DEffect->GetTechnique(i)) != NULL; i++)
		{
			if (SUCCEEDED(pD3DEffect->ValidateTechnique(hTechnique)))
			{
				const char* value = NULL;
				pD3DEffect->GetString(pD3DEffect->GetAnnotationByName(hTechnique, "LOD"), &value);
				if (value != NULL && strcmp(value, LODs[lod]) == 0)
				{
					pD3DEffect->SetTechnique(hTechnique);
					break;
				}
			}
		}
	}

	//
	// Get the handles
	//

	// Matrices
	m_handles.hWorld               = pD3DEffect->GetParameterBySemantic(NULL, "WORLD");
	m_handles.hWorldInverse        = pD3DEffect->GetParameterBySemantic(NULL, "WORLDINVERSE");
	m_handles.hWorldView           = pD3DEffect->GetParameterBySemantic(NULL, "WORLDVIEW");
	m_handles.hWorldViewInverse    = pD3DEffect->GetParameterBySemantic(NULL, "WORLDVIEWINVERSE");
	m_handles.hWorldViewProjection = pD3DEffect->GetParameterBySemantic(NULL, "WORLDVIEWPROJECTION");
	m_handles.hView                = pD3DEffect->GetParameterBySemantic(NULL, "VIEW");
	m_handles.hViewInverse         = pD3DEffect->GetParameterBySemantic(NULL, "VIEWINVERSE");
	m_handles.hViewProjection      = pD3DEffect->GetParameterBySemantic(NULL, "VIEWPROJECTION");
	m_handles.hProjection          = pD3DEffect->GetParameterBySemantic(NULL, "PROJECTION");
	m_handles.hEyePosition         = pD3DEffect->GetParameterBySemantic(NULL, "EYE_POSITION");
	m_handles.hEyeObjPosition      = pD3DEffect->GetParameterBySemantic(NULL, "EYE_OBJ_POSITION");
	m_handles.hTime                = pD3DEffect->GetParameterBySemantic(NULL, "TIME");
	m_handles.hResolutionConstants = pD3DEffect->GetParameterBySemantic(NULL, "RESOLUTION_CONSTANTS");

	// Lighting
	m_handles.hGlobalAmbient    = pD3DEffect->GetParameterBySemantic(NULL, "GLOBAL_AMBIENT");
	m_handles.hDirLightVec0     = pD3DEffect->GetParameterBySemantic(NULL, "DIR_LIGHT_VEC_0");
	m_handles.hDirLightObjVec0  = pD3DEffect->GetParameterBySemantic(NULL, "DIR_LIGHT_OBJ_VEC_0");
	m_handles.hDirLightDiffuse  = pD3DEffect->GetParameterBySemantic(NULL, "DIR_LIGHT_DIFFUSE_0");
	m_handles.hDirLightSpecular = pD3DEffect->GetParameterBySemantic(NULL, "DIR_LIGHT_SPECULAR_0");
	m_handles.hSphLightAll      = pD3DEffect->GetParameterBySemantic(NULL, "SPH_LIGHT_ALL");
	m_handles.hSphLightFill     = pD3DEffect->GetParameterBySemantic(NULL, "SPH_LIGHT_FILL");
	m_handles.hLightScale       = pD3DEffect->GetParameterBySemantic(NULL, "LIGHT_SCALE");
	m_handles.hFogVals          = pD3DEffect->GetParameterBySemantic(NULL, "FOG_VALS");
	m_handles.hDistanceFadeVals = pD3DEffect->GetParameterBySemantic(NULL, "DISTANCE_FADE_VALS");

	// Time
	m_handles.hTime = pD3DEffect->GetParameterBySemantic(NULL, "TIME"); 

	m_pD3DEffect = pD3DEffect;
	m_pD3DEffect->AddRef();
}

Effect::~Effect()
{
	SAFE_RELEASE(m_pD3DEffect);
}
