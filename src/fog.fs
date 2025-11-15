
#version 330
in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D screenTexture;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

void main()
{
    vec4 sceneColor = texture(screenTexture, fragTexCoord);
    float depth = sceneColor.a; // if you store depth in alpha, or use separate depth texture

    float fogFactor = clamp((fogEnd - depth) / (fogEnd - fogStart), 0.0, 1.0);
    fragColor = vec4(mix(fogColor, sceneColor.rgb, fogFactor), 1.0);
}
