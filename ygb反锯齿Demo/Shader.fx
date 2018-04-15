cbuffer ConstantYgbColor:register(b0)
{
	float4 COLOR;
}
struct VS_INPUT
{
	float3 Pos:POSITION;
	float4 Equation:COLOR;
};
struct PS_INPUT
{
	float4 Pos:SV_POSITION;
	float4 Equation:COLOR;
};
PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;
	output.Pos = float4(input.Pos.xy, 0.f, 1.f);
	output.Equation = input.Equation;
	return output;
}
float4 PS(PS_INPUT input) : SV_Target
{
	//return COLOR;
	float a[11] = {/* 1.0, 0.9, 0.9, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.0*/1.0, 0.9, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.2, 0.1, 0.0 };
	if (input.Equation.w <= 0)
	{
		float h = length(float2(input.Pos.x - input.Equation.x, input.Pos.y - input.Equation.y));
		input.Equation.w = input.Equation.z;
		int scale;
		if (h > input.Equation.w)
			scale = 10;
		else
			scale = 10 * (h / input.Equation.w);
		return float4(COLOR.xyz, a[scale]);
	}
	//else
	//{
	//	h = abs((input.Equation.x*input.Pos.x + input.Equation.y*input.Pos.y + input.Equation.z) / length(float2(input.Equation.x, input.Equation.y)));
	//	if (h > input.Equation.w)
	//		return float4(COLOR.xyz, 0.0f);
	//	return float4(COLOR.xyz, 1.0 - h / input.Equation.w);
	//	/*int scale = 10 * (h / input.Equation.w);
	//	return float4(COLOR.xyz, a[scale]);*/
	//}

	float dist = abs((input.Equation.x*input.Pos.x + input.Equation.y*input.Pos.y + input.Equation.z) / length(float2(input.Equation.x, input.Equation.y)));
	float threshold = input.Equation.w;
	float alpha = saturate(1.0 - dist / threshold);
	//if (dist / threshold < 0.1)
	//	return float4(0.0f, 1.0f, 0.0f, 1.0f);
	alpha = saturate(alpha >= 0.33 ? alpha + 0.17 : alpha);
	return float4(COLOR.xyz, 1.0*alpha);
}