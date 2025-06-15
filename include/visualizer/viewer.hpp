#ifndef __VIEWER_H__
#define __VIEWER_H__

#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
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
    int width;
    int height;
    float znear = 0.1f;
    float zfar = 1000.0f;

    glm::mat4 viewmat = glm::mat4(1.0f);
    glm::mat4 K = glm::mat4(1.0f);

    float scaling_modifier = 1.0f;

    bool isTraining = false;
    int renderType = 0; // 0: render 1: gaussian ball 2: depth
    
    glm::vec3 bg_color = glm::vec3(0.0f, 0.0f, 0.0f);
    
};

struct RenderingInfo{
    int iterations_ = 0;
    int total_iterations_ = 0;

    std::vector<float> time_list_;
    std::vector<float> loss_list_;
    std::vector<int> num_splats_list_;

    int freq_ = 10;

    void setProgress(int iter, int total_iterations) {
        iterations_ = iter;
        total_iterations_ = total_iterations;
    }

    void setNumSplats(int num_splats) {
        num_splats_list_.push_back(num_splats);
    }

    void setIterationTime(float time) {
        time_list_.push_back(time);
    }

    void setLoss(float loss) {
        loss_list_.push_back(loss);
    }
};


class Viewer {
    
    std::string title;

    GLFWwindow* window;

    ImGuiWindowFlags window_flags = 0;

    bool any_window_active = false;

    static Viewer* viewer;

    Viewport viewport;

public:

    RenderingConfig config_;
    RenderingInfo info_;

    Viewer(std::string title, int width, int height):
        title(title), viewport(width, height){
        viewer = this;
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

        window = glfwCreateWindow(
            viewer->viewport.windowSize.x, 
            viewer->viewport.windowSize.y, 
            viewer->title.c_str(), NULL, NULL);

        if (window == NULL){
            std::cerr << "Failed to create GLFW window!" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "GLAD init failed" << std::endl;
            glfwTerminate();
            return -1;
        }

        glfwSwapInterval(1); // Enable vsync

        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetKeyCallback(window, keyCallback);

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

        // Setup Dear ImGui style
        // ImGui::StyleColorsDark();
        ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        const char* glsl_version = "#version 430";
        ImGui_ImplGlfw_InitForOpenGL(window, true);
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

    std::vector<float> loss_buffer;
    const int max_loss_points = 200;  // 可视范围最大点数
    int loss_sample_count = 0;        // 当前总采样数

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

        int current_iter = info_.iterations_;
        int total_iters = info_.total_iterations_;

        float fraction = float(current_iter) / float(total_iters);  // 转为 0~1 范围

        char overlay_text[64];
        std::snprintf(overlay_text, sizeof(overlay_text), "%d / %d", current_iter, total_iters);
        ImGui::ProgressBar(fraction, ImVec2(-1, 20), overlay_text);        

        AddLossValue(info_.loss_list_.empty() ? 0.0f : info_.loss_list_.back());

        DrawLossPlot();
        ImGui::Text("num Splats: %d", info_.num_splats_list_.empty() ? 0 : info_.num_splats_list_.back());

        ImGui::End();
        ImGui::PopStyleColor();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    }

    void AddLossValue(float new_loss) {
        if (loss_buffer.size() < max_loss_points) {
            loss_buffer.push_back(new_loss);
        } else {
            loss_buffer[loss_sample_count % max_loss_points] = new_loss;
        }
        loss_sample_count++;
    }

    void DrawLossPlot() {
        if (loss_buffer.empty()) return;

        int visible_count = std::min(loss_sample_count, max_loss_points);
        int offset = (loss_sample_count < max_loss_points) ? 0 : (loss_sample_count % max_loss_points);

        std::vector<float> display_data(visible_count);
        for (int i = 0; i < visible_count; ++i) {
            display_data[i] = loss_buffer[(offset + i) % max_loss_points];
        }

        auto [min_it, max_it] = std::minmax_element(display_data.begin(), display_data.end());
        float min_val = *min_it, max_val = *max_it;

        if (min_val == max_val) {
            min_val -= 1.0f;
            max_val += 1.0f;
        } else {
            float margin = (max_val - min_val) * 0.05f;
            min_val -= margin;
            max_val += margin;
        }

        float latest_loss = display_data.back();
        char loss_label[64];
        std::snprintf(loss_label, sizeof(loss_label), "Loss: %.4f", latest_loss);

        ImGui::PlotLines("##Loss",
                        display_data.data(),
                        static_cast<int>(display_data.size()),
                        0,
                        loss_label,
                        min_val,
                        max_val,
                        ImVec2(-1, 50)); // 高度可调整
    }

    void updateWindowSize() {
        int winW, winH, fbW, fbH;
        glfwGetWindowSize(viewer->window, &winW, &winH);
        glfwGetFramebufferSize(viewer->window, &fbW, &fbH);
        viewer->viewport.windowSize = glm::ivec2(winW, winH);
        viewer->viewport.frameBufferSize = glm::ivec2(fbW, fbH);
        glViewport(0, 0, fbW, fbH);
    }

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        if (viewer->any_window_active)
            return;

        if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_PRESS) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            viewer->viewport.camera.initScreenPos(glm::vec2(xpos, ypos));
        }
    }

    static void cursorPosCallback(GLFWwindow* window, double x, double y) {
        if (viewer->any_window_active)
            return;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            viewer->viewport.camera.translate(glm::vec2(x, y));
        } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            viewer->viewport.camera.rotate(glm::vec2(x, y));
        }
    }

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        if (viewer->any_window_active)
            return;

        float delta = static_cast<float>(yoffset);
        if (std::abs(delta) < 1.0e-2f) return;

        viewer->viewport.camera.zoom(delta);
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
        if(viewer->any_window_active)
            return;

        auto &viewport = viewer->viewport;

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

        while (!glfwWindowShouldClose(window)) {

            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            glClearBufferfv(GL_COLOR, 0, clearColor);

            updateWindowSize();
            
            if(screen_renderer_) {
                torch::Tensor image_uchar = (renderOutput_.image * 255).to(torch::kCPU).to(torch::kU8);
                screen_renderer_->uploadImage(image_uchar.data_ptr<unsigned char>(), renderOutput_.width, renderOutput_.height);
                screen_renderer_->render(shader, viewport);
            }

            configuration();

            glfwSwapBuffers(window);
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
        return viewport.camera.getTransformation();
    }

    gs::RenderOutput renderOutput_;
    std::shared_ptr<ScreenQuadRenderer> screen_renderer_;
    std::thread viewer_thread_;
};



#endif // __VIEWER_H__
