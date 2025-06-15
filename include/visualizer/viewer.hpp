#ifndef __VIEWER_H__
#define __VIEWER_H__

#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <deque>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <torch/torch.h>
#include "core/rasterizer.hpp"
#include "core/image_io.hpp"
#include "visualizer/renderer.hpp"


struct RenderingConfig{

enum RenderType{
    Color,
    Depth
};

    RenderType type = Color;

    glm::mat4 viewmat = glm::mat4(1.0f);
    glm::mat4 intrmat = glm::mat4(1.0f);

    glm::vec2 reso;

    float scaling_modifier = 1.0f;

    bool isTraining = false;
};

struct TrainingInfo{

    int curr_iterations_ = 0;
    int total_iterations_ = 0;

    int num_splats_ = 0;
    int max_loss_points_ = 200;

    std::deque<float> loss_buffer_;

    void updateProgress(int iter, int total_iterations) {
        curr_iterations_ = iter;
        total_iterations_ = total_iterations;
    }

    void updateNumSplats(int num_splats) {
        num_splats_ = num_splats;
    }

    void updateLoss(float loss) {
        loss_buffer_.push_back(loss);
        while(loss_buffer_.size() > max_loss_points_) {
            loss_buffer_.pop_front();
        }
    }
};


class Viewer {

public:

    std::string title_;
    GLFWwindow* window_;
    ImGuiWindowFlags window_flags = 0;
    bool any_window_active = false;

    static Viewer* detail_;

    Viewport viewport_;

    RenderingConfig config_;

    TrainingInfo info_;

    gs::RenderOutput renderOutput_;

    std::shared_ptr<ScreenQuadRenderer> screen_renderer_;

    std::thread viewer_thread_;

    Viewer(std::string title, int width, int height): title_(title), viewport_(width, height){
        detail_ = this;
    }

    ~Viewer() {
        std::cout << "Viewer destroyed." << std::endl;
    }


    bool init(){
        
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW!" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_SAMPLES, 8);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

        window_ = glfwCreateWindow(
            detail_->viewport_.windowSize.x, 
            detail_->viewport_.windowSize.y,
            detail_->title_.c_str(), NULL, NULL);

        if (window_ == NULL){
            std::cerr << "Failed to create GLFW window!" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window_);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "GLAD init failed" << std::endl;
            glfwTerminate();
            return -1;
        }

        glfwSwapInterval(1); // Enable vsync

        glfwSetMouseButtonCallback(window_, mouseButtonCallback);
        glfwSetCursorPosCallback(window_, cursorPosCallback);
        glfwSetScrollCallback(window_, scrollCallback);
        glfwSetKeyCallback(window_, keyCallback);

        glEnable(GL_LINE_SMOOTH);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);  
        glEnable(GL_PROGRAM_POINT_SIZE);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        const char* glsl_version = "#version 430";
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

        // Set Fonts
        std::string font_path = "include/visualizer/assets/JetBrainsMono-Regular.ttf";
        io.Fonts->AddFontFromFileTTF(font_path.c_str(), 14.0f);

        // Set Windows option
        window_flags |= ImGuiWindowFlags_NoScrollbar;
        window_flags |= ImGuiWindowFlags_NoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowPadding = ImVec2(6.0f, 6.0f);
        style.WindowRounding = 6.0f;
        style.WindowBorderSize = 0.0f;

        return true;
    }

    void configuration() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        any_window_active = ImGui::IsAnyItemActive() ;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        ImGui::Begin("Rendering Setting", nullptr,  window_flags); 
        ImGui::SetWindowSize(ImVec2(300, 0));

        // a button, when first, it show start, while it start, the button show trainging... while it is pressed, then it change to stopped
        if (ImGui::Button("Start Training", ImVec2(-1, 0))) {
            std::cout << "Start Training button clicked." << std::endl;
        }

        static const char* mode_items[] = {"Color", "Depth"};
        static int current_item = config_.type;
        ImGui::SetNextItemWidth(80);
        if (ImGui::Combo("##render_mode", &current_item, mode_items, 2)) {
            config_.type = static_cast<RenderingConfig::RenderType>(current_item);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::SliderFloat("##scale_slider", &config_.scaling_modifier, 0.01f, 3.0f, "Scale=%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##scale", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            config_.scaling_modifier = 1.0f;
        }

        float fraction = float(info_.curr_iterations_) / float(info_.total_iterations_);
        char overlay_text[64];
        std::snprintf(overlay_text, sizeof(overlay_text), "%d / %d", info_.curr_iterations_, info_.total_iterations_);
        ImGui::ProgressBar(fraction, ImVec2(-1, 20), overlay_text);        

        std::vector<float> loss_data(info_.loss_buffer_.begin(), info_.loss_buffer_.end());
        auto [min_it, max_it] = std::minmax_element(loss_data.begin(), loss_data.end());
        float min_val = *min_it, max_val = *max_it;

        if (min_val == max_val) {
            min_val -= 1.0f; max_val += 1.0f;
        } else {
            float margin = (max_val - min_val) * 0.05f;
            min_val -= margin; max_val += margin;
        }

        float latest_loss = loss_data.back();
        char loss_label[64];
        std::snprintf(loss_label, sizeof(loss_label), "Loss: %.4f", latest_loss);

        ImGui::PlotLines("##Loss", loss_data.data(), static_cast<int>(loss_data.size()),
                        0, loss_label, min_val, max_val, ImVec2(-1, 50));

        ImGui::Text("num Splats: %d", info_.num_splats_);

        ImGui::End();
        ImGui::PopStyleColor();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    }

    void updateWindowSize() {
        int winW, winH, fbW, fbH;
        glfwGetWindowSize(window_, &winW, &winH);
        glfwGetFramebufferSize(window_, &fbW, &fbH);
        viewport_.windowSize = glm::ivec2(winW, winH);
        viewport_.frameBufferSize = glm::ivec2(fbW, fbH);
        glViewport(0, 0, fbW, fbH);
    }

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        if (detail_->any_window_active)
            return;

        if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_PRESS) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            detail_->viewport_.camera.initScreenPos(glm::vec2(xpos, ypos));
        }
    }

    static void cursorPosCallback(GLFWwindow* window, double x, double y) {
        if (detail_->any_window_active)
            return;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            detail_->viewport_.camera.translate(glm::vec2(x, y));
        } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            detail_->viewport_.camera.rotate(glm::vec2(x, y));
        }
    }

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        if (detail_->any_window_active)
            return;

        float delta = static_cast<float>(yoffset);
        if (std::abs(delta) < 1.0e-2f) return;

        detail_->viewport_.camera.zoom(delta);
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
        if(detail_->any_window_active)
            return;

        auto &viewport_ = detail_->viewport_;

        // TODO: not implemented yet (pxk)
    }

    void run() {
        
        if (!init()) {
            std::cerr << "Viewer initialization failed!" << std::endl;
            return;
        }

        std::shared_ptr<Shader> shader = std::make_shared<Shader>(
            "include/visualizer/shaders/screen_quad.vert",
            "include/visualizer/shaders/screen_quad.frag",
            true
        );
        
        screen_renderer_ = std::make_shared<ScreenQuadRenderer>();

        while (!glfwWindowShouldClose(window_)) {

            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            glClearBufferfv(GL_COLOR, 0, clearColor);

            updateWindowSize();
            
            if(screen_renderer_) {
                torch::Tensor image_uchar = (renderOutput_.image * 255).to(torch::kCPU).to(torch::kU8);
                torch::Tensor depth = renderOutput_.depths.to(torch::kCPU).to(torch::kFloat32).contiguous();
                
                // normalize depth

                screen_renderer_->uploadImage(image_uchar.data_ptr<unsigned char>(), renderOutput_.width, renderOutput_.height);
                screen_renderer_->uploadDepth(depth.data_ptr<float>(), renderOutput_.width, renderOutput_.height);
                screen_renderer_->render(shader, viewport_);
            }

            configuration();

            glfwSwapBuffers(window_);
            glfwPollEvents();
        }
    }

    void setRenderOutput(const gs::RenderOutput output) {
        renderOutput_ = output;
    }

    void start() {
        viewer_thread_ = std::thread(&Viewer::run, this);
    }

    void join() {
        if (viewer_thread_.joinable()) {
            viewer_thread_.join();
        }
    }

    glm::mat4 getViewMatrix() const {
        return viewport_.camera.getTransformation();
    }
};



#endif // __VIEWER_H__
