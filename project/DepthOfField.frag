#version 410 core

in block
{
    vec2 Texcoord;
} In; 

uniform sampler2D Color;
uniform sampler2D Blur;
uniform sampler2D CoC;

layout(location = 0, index = 0) out vec4 OutColor;

void main(void)
{
    float blurCoef = texture(CoC, In.Texcoord).r;
    vec4 color = texture(Color, In.Texcoord);
    vec4 blur = texture(Blur, In.Texcoord);
    OutColor = vec4(vec3(blurCoef * blur + (1-blurCoef)*color),1);
    //OutColor = vec4(vec3(blur),1);
}