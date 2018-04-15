//filename:Tutorial02.fx
//--------------------------------------------------------------------------------------
cbuffer screenWH:register(b0) {
	int width;
	int height;
};
struct VS_INPUT{
	float3 Pos:POSITION;
};
struct PS_INPUT{
	float4 Pos:SV_POSITION;
};
//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;
	output.Pos = float4(input.Pos.xy, 0.f, 1.f);
	return output;
}
//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	float offsetx = 2.0 / width;
	float offsety = 2.0 / height;
	return float4(1.0f, 1.0f, 0.0f,1.0f);  
}