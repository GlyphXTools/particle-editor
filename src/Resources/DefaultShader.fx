technique t0
{
    pass p0
    {
       	AlphaBlendEnable = False;
        DestBlend        = Zero;
        SrcBlend         = One;
        AlphaTestEnable  = False;
        ZWriteEnable     = True;
       	VertexShader     = Null;
        PixelShader      = Null;
        Lighting         = False;
        ColorOp[0]       = Modulate;
        ColorArg1[0]     = Diffuse;
        ColorArg2[0]     = Texture;
        AlphaOp[0]       = SelectArg1;
        AlphaArg1[0]     = Texture;
        ColorOp[1]       = Disable;
        AlphaOp[1]       = Disable;
    }
}
