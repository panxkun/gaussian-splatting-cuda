#version 430 core

in vec2 TexCoord;
uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform int colorType;  // 0: RGB, 1: Depth

out vec4 FragColor;

void main()
{
    float depth = texture(depthTexture, TexCoord).r;

    if (colorType == 0)
        FragColor = texture(screenTexture, vec2(TexCoord.x, 1.0 - TexCoord.y));
    else if (colorType == 1)
        FragColor = vec4(vec3(depth), 1.0);
    else
        FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}