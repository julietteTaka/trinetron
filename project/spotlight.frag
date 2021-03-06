#version 410 core

const float PI = 3.14159265359;
const float TWOPI = 6.28318530718;
const float PI_2 = 1.57079632679;
const float DEG2RAD = TWOPI / 360.0;

in block
{
	vec2 Texcoord;
} In; 

uniform sampler2D ColorBuffer;
uniform sampler2D NormalBuffer;
uniform sampler2D DepthBuffer;

uniform int SampleCount;
uniform float Spread;

#if 0
uniform sampler2D Shadow;
#else
uniform sampler2DShadow Shadow;
#endif

layout(location = 0, index = 0) out vec4 Color;

uniform mat4 InverseProjection;

uniform light
{
	vec3 Position;
	float Angle;
	vec3 Direction;
	float PenumbraAngle;
	vec3 Color;
	float Intensity;
	mat4 WorldToLightScreen;
} SpotLight;

vec3 spotLight( in vec3 p, in vec3 n, in vec3 v, in vec3 diffuseColor, in vec3 specularColor, in float specularPower)
{
	vec3 l = normalize(SpotLight.Position - p);
	float a = cos(SpotLight.Angle * DEG2RAD);
	float pa = cos(SpotLight.PenumbraAngle * DEG2RAD);
	float ndotl = max(dot(n, l), 0.0);
	float ldotd = dot(-l, normalize(SpotLight.Direction));
	vec3 h = normalize(l+v);
	float ndoth = max(dot(n, h), 0.0);
	float fallof = clamp(pow( (ldotd - a) / (a-pa), 4), 0.0, 1.0);
	float d = distance(SpotLight.Position, p);
	float att = 1.f / (d*d);
	return att * fallof * SpotLight.Color * SpotLight.Intensity * (diffuseColor * ndotl + specularColor * pow(ndoth, specularPower));
}

vec2 poissonDisk[16] = vec2[](
        vec2( -0.94201624, -0.39906216 ),
        vec2( 0.94558609, -0.76890725 ),
        vec2( -0.094184101, -0.92938870 ),
        vec2( 0.34495938, 0.29387760 ),
        vec2( -0.91588581, 0.45771432 ),
        vec2( -0.81544232, -0.87912464 ),
        vec2( -0.38277543, 0.27676845 ),
        vec2( 0.97484398, 0.75648379 ),
        vec2( 0.44323325, -0.97511554 ),
        vec2( 0.53742981, -0.47373420 ),
        vec2( -0.26496911, -0.41893023 ),
        vec2( 0.79197514, 0.19090188 ),
        vec2( -0.24188840, 0.99706507 ),
        vec2( -0.81409955, 0.91437590 ),
        vec2( 0.19984126, 0.78641367 ),
        vec2( 0.14383161, -0.14100790 )
);
float random(vec4 seed)
{
	float dot_product = dot(seed, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

void main(void)
{
	vec4 colorBuffer = texture(ColorBuffer, In.Texcoord).rgba;
	vec4 normalBuffer = texture(NormalBuffer, In.Texcoord).rgba;
	float depth = texture(DepthBuffer, In.Texcoord).r;

	vec3 n = normalBuffer.rgb;
	vec3 diffuseColor = colorBuffer.rgb;
	vec3 specularColor = colorBuffer.aaa;
	float specularPower = normalBuffer.a;

	vec2 xy = In.Texcoord * 2.0 -1.0;
	vec4 wP = InverseProjection * vec4(xy, depth * 2.0 -1.0, 1.0);
	vec3 p = vec3(wP.xyz / wP.w);
	vec3 v = normalize(-p);

	vec4 wlP = SpotLight.WorldToLightScreen * vec4(p, 1.0);
	vec3 lP = vec3(wlP / wlP.w) * 0.5 + 0.5;
	float d = distance(SpotLight.Position, p);

#if 0
#if 0
	float shadowDepth = texture(Shadow, lP.xy).r;
	if (shadowDepth  + 0.005 < lP.z)
		Color = vec4(vec3(0.0, 0.0, 0.0), 1.0);
		//Color = vec4(vec3(1.0, 0.0, 1.0), 1.0);
	else
		Color = vec4(vec3(1.0, 0.0, 1.0), 1.0);
		//Color = vec4(spotLight(p, n, v, diffuseColor, specularColor, specularPower), 1.0);
#else
	// float shadowDepth = textureProj(Shadow, vec4(lP.xy * lP.z, lP.z * lP.z, lP.z), 0.05);
	float shadowDepth = textureProj(Shadow, vec4(lP.xy, lP.z -0.005, 1.0), 0.0);
	Color = shadowDepth * vec4(vec3(1.0, 0.0, 1.0), 1.0);
	Color = shadowDepth * vec4(spotLight(p, n, v, diffuseColor, specularColor, specularPower), 1.0);
#endif	
#else
		vec3 spotColor = spotLight(p, n, v, diffuseColor, specularColor, specularPower);
		float shadowDepth = 1.0;
		if (any(greaterThan(spotColor, vec3(0.005))))
		{

			shadowDepth = 0.0;
			float samplesf = float(SampleCount);
			for (int i=0;i<SampleCount;i++)
			{
				int index = int(samplesf * random(vec4(gl_FragCoord.xyy, i)))%SampleCount;
				shadowDepth += textureProj(Shadow, vec4(lP.xy + poissonDisk[index]/(Spread * 1.f/d), lP.z -0.005, 1.0), 0.0) / samplesf;
				//shadowDepth += (texture( Shadow, vec3(lP.xy + poissonDisk[index]/Spread,  (lP.z-0.005))))/ samplesf;
			}
			Color = shadowDepth * vec4(vec3(1.0, 0.0, 1.0), 1.0);
		}
		Color = shadowDepth * vec4(spotLight(p, n, v, diffuseColor, specularColor, specularPower), 1.0);
#endif		

//	Color = 
	//Color = vec4(vec3(lP.z), 1.0);
	//Color = vec4(vec3(shadowDepth), 1.0);
	//Color = vec4(vec3(lP.xy, 0.0), 1.0);
	//Color = vec4(vec3(SpotLight.WorldToLightScreen[3]), 1.0);
}