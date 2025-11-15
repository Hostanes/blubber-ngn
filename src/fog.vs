
#version 330
in vec2 vertexPosition;
out vec2 fragTexCoord;

void main()
{
    fragTexCoord = vertexPosition * 0.5 + 0.5;
    gl_Position = vec4(vertexPosition, 0.0, 1.0);
}
