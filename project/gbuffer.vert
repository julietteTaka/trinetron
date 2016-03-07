#version 410 core

#define POSITION	0
#define NORMAL		1
#define TEXCOORD	2

layout(location = POSITION) in vec3 Position;
layout(location = NORMAL) in vec3 Normal;
layout(location = TEXCOORD) in vec2 TexCoord;

uniform float Time;
uniform mat4 MVP;
uniform mat4 MV;


out block
{
	vec2 Texcoord;
	vec3 CameraSpacePosition;
	vec3 CameraSpaceNormal;
} Out;

void main()
{	
	Out.Texcoord = TexCoord;
	vec3 p = Position;
	vec3 n = Normal;
	float t = Time + gl_InstanceID;
	if (Time > 0) {
		vec3 tempPos = Position;
		vec3 tempN;

		// réduction du cube
		tempPos.x = Position.x*0.05;
		tempPos.y = Position.y*0.05;
		tempPos.z = Position.z*0.05;

		float ci = cos(gl_InstanceID*342);
		float si = sin(gl_InstanceID*342);
		float ct = cos(t);
		float st = sin(t);

		// rotation autour de l'axe x;
		tempPos.z += 3;
		p.x = tempPos.x;
		p.y = tempPos.y * ct + tempPos.z * st;
		p.z = -tempPos.y * st + tempPos.z * ct;
		n.x = Normal.x;
		n.y = Normal.y * ct + Normal.z * st;
		n.z = -Normal.y * st + Normal.z * ct;
		p.z -= 3;

		tempPos = p;
		tempN = n;

		p.x = tempPos.x * ci + tempPos.z * si;
		p.z = -tempPos.x * si + tempPos.z * ci;
		n.x = tempN.x * ci + tempN.z * si;
		n.z = -tempN.x * si + tempN.z * ci;
		n.y = tempN.y;

		tempPos = p;
		tempN = n;

		p.y-=0.5;

		// float square = sqrt(2500);
		// float square_delta = 2.0;
		// p.x +=  square_delta * floor(gl_InstanceID / square) - square;
		// p.z +=  square_delta * (gl_InstanceID - (square * floor(gl_InstanceID/square))) - square;
		// p.y +=  sin(floor(gl_InstanceID / square)) + sin((gl_InstanceID - (square * floor(gl_InstanceID/square)))) ;
	}

	Out.CameraSpacePosition = vec3(MV * vec4(p, 1.0));
	Out.CameraSpaceNormal = vec3(MV * vec4(n, 0.0));
	gl_Position = MVP * vec4(p, 1.0);
}