#version 410 core

#define POSITION	0
#define NORMAL		1
#define TEXCOORD	2
#define COLOR	    0

const float PI = 3.14159265359;
const float TWOPI = 6.28318530718;
const float PI_2 = 1.57079632679;
const float DEG2RAD = TWOPI / 360.0;

precision highp int;

uniform float Time;
uniform sampler2D Diffuse;
uniform sampler2D Specular;
uniform float SpecularPower;
uniform vec3 DiffuseColor;

layout(location = COLOR ) out vec4 Color;
layout(location = NORMAL) out vec4 Normal;

in block
{
	vec2 Texcoord;
	vec3 CameraSpacePosition;
	vec3 CameraSpaceNormal;
} In; 


subroutine vec3 diffuseColor();


subroutine (diffuseColor) vec3 diffuseUniform()
{
	return DiffuseColor;
}

subroutine (diffuseColor) vec3 diffuseTexture()
{
	return texture(Diffuse, vec2(In.Texcoord.x, 1. - In.Texcoord.y) ).rgb;
}


subroutine uniform diffuseColor DiffuseColorSub;

void main()
{
	vec3 n = normalize(In.CameraSpaceNormal);
	// gl_FrontFacing is not working on MacOSX Intel cards
	// Try this instead :
#if 0	
	if (dot(n, normalize(-In.CameraSpacePosition)) < 0.)
#else		
	if (!gl_FrontFacing)
#endif		
		n = -n;
	//vec3  diffuseColor = texture(Diffuse, In.Texcoord).rgb;
	vec3  diffuseColor = DiffuseColorSub();
	//float specularColor = texture(Specular, In.Texcoord).r;
	Color = vec4(diffuseColor, 0.0);
	Normal = vec4(n, SpecularPower);
}
