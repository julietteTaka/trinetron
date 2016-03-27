#version 410 core

in block
{
    vec2 Texcoord;
} In; 

uniform sampler2D Color;
uniform float t;
uniform float startTime;

layout(location = 0, index = 0) out vec4 OutColor;

void main(void)
{
    float fadeCoef = max(0, (startTime +3 -t) / 3.0);
    vec4 color = texture(Color, In.Texcoord);
    OutColor = vec4(vec3(fadeCoef * color),1);
}