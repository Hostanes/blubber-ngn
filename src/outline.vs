
#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;

uniform mat4 mvp;
uniform float outlineSize;

void main()
{
    vec3 expandedPos = vertexPosition + vertexNormal * outlineSize;
    gl_Position = mvp * vec4(expandedPos, 1.0);
}
