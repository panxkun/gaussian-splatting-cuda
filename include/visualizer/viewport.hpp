#ifndef __VIEWPORT_H__
#define __VIEWPORT_H__

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <GL/glu.h>

class Viewport {

class CameraMotion {
public:
    CameraMotion() = default;

    CameraMotion(Viewport* viewport): 
        viewport(viewport), 
        transform(1.0f),
        deltaTransform(1.0f),
        last_z(0.98f),
        intersection_center(0.0f) {}

    void rotate(const glm::vec2& pos) {
        glm::vec2 offset = pos - prevPos;
        float rf = 0.005f;

        glm::mat4 T1(1.0f);
        glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), offset.y * rf, glm::vec3(1, 0, 0));
        glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), offset.x * rf, glm::vec3(0, 1, 0));
        T1 = Rx * Ry;
        T1[3] = glm::vec4(-intersection_center, 1.0f);

        glm::mat4 T2 = glm::translate(intersection_center);

        deltaTransform = T2 * T1;
        transform = transform * glm::inverse(deltaTransform);
        deltaTransform = glm::mat4(1.0f);
        prevPos = pos;
    }

    void translate(const glm::vec2& pos) {
        glm::vec2 offset = pos - prevPos;
        float tf = 0.1f;
        if (last_z == 1) {
            deltaTransform[3] = glm::vec4(offset.x * tf, -offset.y * tf, 0, 0);
        } else {
            glm::vec3 Pw, Pc;
            viewport->pixelUnproject(pos, last_z, Pw, Pc);
            glm::vec3 delta = Pc - intersection_center;
            delta.z = 0;
            deltaTransform[3] = glm::vec4(delta, 0.0f);
            intersection_center = Pc;
        }
        transform = transform * glm::inverse(deltaTransform);
        deltaTransform = glm::mat4(1.0f);
        prevPos = pos;
    }

    void zoom(float delta) {
        deltaTransform[3].z = (delta > 0 ? 1.0f : -1.0f);
        transform = transform * glm::inverse(deltaTransform);
        deltaTransform = glm::mat4(1.0f);
    }

    void initScreenPos(const glm::vec2& pos) {
        float zNDC;
        glm::vec3 pw, pc;
        viewport->getPixelPosition(pos, zNDC, pw, pc, last_z);

        if (zNDC != 1) {
            last_z = zNDC;
            intersection_center = pc;
        }
        prevPos = pos;
    }

    void initTransformation(const glm::mat4& transform) {
        this->transform = transform;
    }

    glm::mat3 getRotation() const {
        return glm::mat3(transform);
    }

    glm::vec3 getPosition() const {
        return glm::vec3(transform[3]);
    }

    glm::mat4 getTransformation() const {
        return transform;
    }

private:
    float last_z;
    glm::vec3 intersection_center;
    glm::mat4 transform;
    glm::mat4 deltaTransform;
    glm::vec2 prevPos;
    Viewport* viewport = nullptr;
};

public:
    glm::ivec2 windowSize;
    glm::ivec2 frameBufferSize;
    glm::mat4 openGLTransform;
    float zNear = 0.1f;
    float zFar = 100.0f;
    float fov = 90.0f;
    CameraMotion camera;

    Viewport(size_t width = 1280, size_t height = 720,
             glm::vec3 eye = glm::vec3(1,1,1),
             glm::vec3 center = glm::vec3(0,0,0),
             glm::vec3 up = glm::vec3(0,0,1)) {

        windowSize = glm::ivec2(width, height);
        glm::vec3 zAxis = glm::normalize(center - eye);
        glm::vec3 xAxis = glm::normalize(glm::cross(up, zAxis));
        glm::vec3 yAxis = glm::cross(zAxis, xAxis);

        glm::mat4 transform(1.0f);
        transform[0] = glm::vec4(xAxis, 0.0f);
        transform[1] = glm::vec4(yAxis, 0.0f);
        transform[2] = glm::vec4(zAxis, 0.0f);
        transform[3] = glm::vec4((eye - center) * 3.0f + center, 1.0f);

        camera = CameraMotion(this);
        camera.initTransformation(transform);

        openGLTransform = glm::mat4(1.0f);
        openGLTransform[1][1] = -1;
        openGLTransform[2][2] = -1;
    }

    float getFocal() const {
        return windowSize.y / (2.0f * tan(glm::radians(fov / 2.0f)));
    }

    glm::vec2 getTanXY() const {
        float tanHalfFov = tan(glm::radians(fov / 2.0f));
        float aspect = static_cast<float>(frameBufferSize.x) / frameBufferSize.y;
        return glm::vec2(aspect * tanHalfFov, tanHalfFov);
    }

    glm::vec3 getCameraPosition() const {
        return camera.getPosition();
    }

    glm::mat3 getCameraRotation() const {
        return camera.getRotation();
    }

    glm::ivec2 getFrameBufferSize() const {
        return frameBufferSize;
    }

    void getPixelPosition(const glm::vec2& pos, float& zNDC, glm::vec3& Pw, glm::vec3& Pc, float default_z) {
        glm::ivec2 pick_pos = glm::ivec2(int(pos.x), int(windowSize.y - pos.y));
        glReadPixels(pick_pos.x, pick_pos.y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &zNDC);

        if (zNDC == 1) zNDC = default_z;
        pixelUnproject(pos, zNDC, Pw, Pc);
    }

    void pixelUnproject(const glm::vec2& pos, const float& zNDC, glm::vec3& Pw, glm::vec3& Pc) {
        // glm::dvec3 Pc_;
        // glm::mat4 Identity(1.0f);
        // glm::mat4 Projmatrix = getProjectionMatrix();
        // GLint viewport[4] = { 0, 0, windowSize.x, windowSize.y };
        // GLint x = int(pos.x), y = int(windowSize.y - pos.y);
        // gluUnProject(x, y, zNDC, glm::value_ptr(Identity), glm::value_ptr(Projmatrix), viewport, &Pc_.x, &Pc_.y, &Pc_.z);
        // Pc = glm::vec3(Pc_);
        // Pw = camera.getRotation() * Pc + camera.getPosition();
    }

    glm::mat4 getViewMatrix() const {
        return glm::inverse(camera.getTransformation());
    }

    glm::mat4 getProjectionMatrix() const {
        float aspect = static_cast<float>(frameBufferSize.x) / frameBufferSize.y;
        return glm::perspective(glm::radians(fov), aspect, zNear, zFar);
    }

    void setFoV(float fov) {
        this->fov = fov;
    }

    void setViewMatrix(glm::mat4 pose, float dist) {
        glm::mat4 delta = glm::translate(glm::vec3(0, 0, -dist));
        camera.initTransformation(pose * delta * openGLTransform);
    }

    void setViewMatrix(glm::mat4 viewmat) {
        camera.initTransformation(viewmat * openGLTransform);
    }

    void setProjectionMatrix(float zNear, float zFar, float fov) {
        this->zNear = zNear;
        this->zFar = zFar;
        this->fov = fov;
    }
};

#endif