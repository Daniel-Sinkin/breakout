/* danielsinkin97@gmail.com */

#include <SDL.h>
#include <glad/glad.h>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl.h"
#include "imgui.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "main.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

using Color = glm::vec3;
using Position = glm::vec2;
using VAO = GLuint;
using VBO = GLuint;
using EBO = GLuint;
using ShaderProgram = GLuint;

struct Global {
    SDL_Window *window = nullptr;
    bool running = false;

    ImGuiIO imgui_io;
    SDL_GLContext gl_context;

    ShaderProgram shader_program;
    VAO paddle_vao;

    const char *window_title = "My Breakout Game";
    int version_major = 0;
    int version_minor = 1;

    Color c_background{1.0f, 0.5f, 0.31f};
    Color c_ball{1.0f, 1.0f, 1.0f};
    Color c_paddle_left{1.0f, 0.0f, 0.0f};
    Color c_paddle_right{0.0f, 0.0f, 1.0f};

    float paddle_pos_left = 0.5f;
    float paddle_pos_right = 0.5f;

    Position position_ball{0.0f, 0.0f};

    float square_vertices[12] = {
        0.5f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f};
    unsigned int square_indices[6] = {
        0, 1, 3,
        1, 2, 3};

    int frame_counter = 0;
    std::chrono::system_clock::time_point run_start_time;
    std::chrono::system_clock::time_point frame_start_time;
    std::chrono::milliseconds runtime;
};
Global global;

auto format_time(std::chrono::system_clock::time_point tp) -> const char * {
    static char buffer[64];

    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm *tm_ptr = std::localtime(&time);

    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_ptr);
    return buffer;
}

auto format_duration(std::chrono::milliseconds duration) -> const char * {
    static char buffer[32];
    using namespace std::chrono;

    auto hrs = duration_cast<hours>(duration);
    duration -= hrs;
    auto mins = duration_cast<minutes>(duration);
    duration -= mins;
    auto secs = duration_cast<seconds>(duration);
    duration -= secs;
    auto millis = duration_cast<milliseconds>(duration);

    std::snprintf(
        buffer, sizeof(buffer),
        "%02lld:%02lld:%02lld.%03lld",
        static_cast<long long>(hrs.count()),
        static_cast<long long>(mins.count()),
        static_cast<long long>(secs.count()),
        static_cast<long long>(millis.count()));

    return buffer;
}

auto _main_imgui() -> void {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(global.window);
    { // Debug
        ImGui::NewFrame();
        ImGui::Begin("Debug");
        ImGui::ColorEdit3("Background", glm::value_ptr(global.c_background));
        ImGui::ColorEdit3("Ball", glm::value_ptr(global.c_ball));
        ImGui::ColorEdit3("Paddle (Left)", glm::value_ptr(global.c_paddle_left));
        ImGui::ColorEdit3("Paddle (Right)", glm::value_ptr(global.c_paddle_right));
        ImGui::Text("Frame Counter: %d", global.frame_counter);
        ImGui::Text("Run Start: %s", format_time(global.run_start_time));
        ImGui::Text("Runtime: %s", format_duration(global.runtime));
        ImGui::SliderFloat("Left Paddle", &global.paddle_pos_left, 0.0f, 1.0f);
        ImGui::SliderFloat("Right Paddle", &global.paddle_pos_right, 0.0f, 1.0f);
        ImGui::Text("Ball Position: (%f, %f)", global.position_ball.y, global.position_ball.x);
        ImGui::End();
    } // Debug

    ImGui::Render();
}

void _main_handle_inputs() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            global.running = false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            global.running = false;
        }
    }
}

void _main_render() {
    glViewport(0, 0, (int)global.imgui_io.DisplaySize.x, (int)global.imgui_io.DisplaySize.y);
    glClearColor(global.c_background.r, global.c_background.g, global.c_background.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(global.shader_program);
    glBindVertexArray(global.paddle_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

/*
Handles the SDL, ImGUI, OpenGL init and linking. Returns true if setup successful, false otherwise
*/
auto setup() -> bool {
    // Initialize SDL2 with video and timer subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "Error: SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
    // Other platforms can use higher versions if supported
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    // Create window with OpenGL context
    global.window = SDL_CreateWindow(
        global.window_title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!global.window) {
        std::cerr << "Error: SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    global.gl_context = SDL_GL_CreateContext(global.window);
    if (!global.gl_context) {
        std::cerr << "Error: SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(global.window);
        SDL_Quit();
        return false;
    }
    SDL_GL_MakeCurrent(global.window, global.gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize GL loader
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    global.imgui_io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    // Initialize ImGui SDL2 + OpenGL3 backends
    ImGui_ImplSDL2_InitForOpenGL(global.window, global.gl_context);
    const char *glsl_version = nullptr;
#ifdef __APPLE__
    glsl_version = "#version 410 core";
#else
    glsl_version = "#version 450 core";
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

auto setup_shader_program() -> void {
    const char *vertexShaderSource = "#version 410 core\n"
                                     "layout (location = 0) in vec3 aPos;\n"
                                     "void main()\n"
                                     "{\n"
                                     "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                     "}\0";
    GLuint vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    const char *fragmentShaderSource = "#version 410 core\n"
                                       "out vec4 FragColor;\n" // ← missing ‘;’ here
                                       "void main()\n"
                                       "{\n"
                                       "    FragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);\n" // ← missing ‘;’ here
                                       "}\n";
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    global.shader_program = glCreateProgram();
    glAttachShader(global.shader_program, vertexShader);
    glAttachShader(global.shader_program, fragmentShader);
    glLinkProgram(global.shader_program);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(global.shader_program, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }
    glUseProgram(global.shader_program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void setup_paddle_vao() {
    // 1) Generate and bind the VAO up front
    glGenVertexArrays(1, &global.paddle_vao);
    glBindVertexArray(global.paddle_vao);

    // 2) Create VBO, upload vertex data, set attribute pointers
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(global.square_vertices),
        global.square_vertices,
        GL_STATIC_DRAW);
    glVertexAttribPointer(
        0, // location = 0 in your vertex shader
        3, // 3 floats per vertex
        GL_FLOAT, GL_FALSE,
        3 * sizeof(float),
        (void *)0);
    glEnableVertexAttribArray(0);

    // 3) Create EBO *while the VAO is still bound*, so the binding is stored in it
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        sizeof(global.square_indices),
        global.square_indices,
        GL_STATIC_DRAW);

    // 4) Unbind VAO (optional but good practice)
    glBindVertexArray(0);
}

void cleanup() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(global.gl_context);
    SDL_DestroyWindow(global.window);
    SDL_Quit();
}

auto main(int argc, char **argv) -> int {
    if (!setup()) return EXIT_FAILURE;

    setup_shader_program();
    setup_paddle_vao();

    global.run_start_time = std::chrono::system_clock::now();

    global.running = true;
    while (global.running) {
        global.frame_start_time = std::chrono::system_clock::now();
        global.runtime = std::chrono::duration_cast<std::chrono::milliseconds>(global.frame_start_time - global.run_start_time);

        _main_handle_inputs();
        _main_imgui();

        _main_render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(global.window);

        global.frame_counter += 1;
    }

    cleanup();

    return EXIT_SUCCESS;
}