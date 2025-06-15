#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "visualizer/viewport.hpp"
#include "visualizer/shader.hpp"
#include "visualizer/framebuffer.hpp"


class ScreenQuadRenderer {

    GLuint quadVAO;
    GLuint quadVBO;

    std::shared_ptr<FrameBuffer> framebuffer;

public:

    ScreenQuadRenderer(){

        framebuffer = std::make_shared<FrameBuffer>();

        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
            1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
            1.0f, -1.0f,  1.0f, 0.0f,
            1.0f,  1.0f,  1.0f, 1.0f
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);

        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);

    }

    void render(std::shared_ptr<Shader> shader, const Viewport& viewport) const {
        
        shader->bind();

        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, framebuffer->getFrameTexture());
        glUniform1i(glGetUniformLocation(shader->programID(), "screenTexture"), 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        shader->unbind();

    }


    void uploadImage(const unsigned char* data, int width_, int height_){
        framebuffer->uploadImage(data, width_, height_);
    }
};

#endif 