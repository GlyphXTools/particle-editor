texture SceneTexture;
texture DistortionTexture;

float DistortionAmount = 0.25;

sampler2D DistortionSampler = sampler_state
{
	Texture = (DistortionTexture);
};

sampler2D SceneSampler = sampler_state
{
	Texture  = (SceneTexture);
	AddressU = CLAMP;
	AddressV = CLAMP;
};

struct VS_OUTPUT
{
	float4 Pos  : POSITION;
	float2 Tex0 : TEXCOORD0;
};

VS_OUTPUT vs_main(float3 Pos: POSITION)
{
	VS_OUTPUT Out;
	Out.Pos  = float4(Pos.xy, 0.5, 1);
	Out.Tex0 = Pos * float2(0.5,-0.5) + float2(0.5, 0.5);
	return Out;
}

float4 ps_main(float2 Tex0: TEXCOORD0) : COLOR
{
    float2 distortion = tex2D(DistortionSampler, Tex0) - float2(0.5, 0.5);
    return tex2D(SceneSampler, Tex0 + distortion * DistortionAmount);
}

technique t0
{
	pass p0
	{
        AlphaBlendEnable = False;
        AlphaTestEnable  = False;
        ZWriteEnable     = False;
        ZFunc            = Always;
		VertexShader     = compile vs_1_1 vs_main();
		PixelShader      = compile ps_2_0 ps_main();
        //Texture[0]       = (DistortionTexture);
        //Texture[1]       = (SceneTexture);
        //AddressU[1]      = Clamp;
        //AddressV[1]      = Clamp;
	}
}