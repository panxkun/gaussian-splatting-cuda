#version 430 core

out vec4 FragColor;

in vec2 TexCoord;
uniform sampler2D screenTexture;

void main(){
    FragColor = texture(screenTexture, vec2(TexCoord.x, 1.0 - TexCoord.y));
}