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

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using Color = glm::vec3;
using Position = glm::vec2;
using VAO = GLuint;
using VBO = GLuint;
using EBO = GLuint;
using Shader = GLuint;
using ShaderProgram = GLuint;
using UniformLocation = GLuint;

void panic(const std::string &message) {
    std::cerr << "PANIC: " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

struct Constants {
    static constexpr std::string_view window_title = "Breakout";
    static constexpr int window_width = 1280;
    static constexpr int window_height = 720;
    static constexpr float aspect_ratio = static_cast<float>(window_width) / window_height;

    static constexpr int n_block_rows = 5;
    static constexpr int n_block_cols = 8;

    static constexpr float block_height = 20.0f;
    static constexpr float block_width = 50.0f;

    static constexpr float paddle_bound_left = 0.05f;
    static constexpr float paddle_bound_right = 0.95f;

    static constexpr float paddle_width = 100.0f;
    static constexpr float paddle_height = 50.0f;

    static constexpr int standard_value = 10;
    static constexpr Color standard_color = Color{0.8f, 0.2f, 0.1f};

    static constexpr int special_value = 25;
    static constexpr Color special_color = Color{0.9f, 0.9f, 0.1f};

    static constexpr std::array<float, 12> square_vertices = {
        1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f};

    static constexpr std::array<unsigned int, 6> square_indices = {
        0, 1, 3,
        1, 2, 3};

    static constexpr const char *fp_shader_dir = "assets/shaders/";
    static constexpr const char *fp_vertex_shader = "assets/shaders/vertex.glsl";
    static constexpr const char *fp_fragment_shader = "assets/shaders/fragment.glsl";
};
struct Block {
    Position position;
    Color color;
    int value;
    bool active;
};
struct GameState {
    int score = 0;
    int lives = 3;

    std::array<std::array<Block, Constants::n_block_cols>, Constants::n_block_rows> blocks;
};
struct Global {
    SDL_Window *window = nullptr;
    bool running = false;

    ImGuiIO imgui_io;
    SDL_GLContext gl_context;

    ShaderProgram shader_program;
    VAO paddle_vao;

    Color c_background{1.0f, 0.5f, 0.31f};
    Color c_ball{1.0f, 1.0f, 1.0f};
    Color c_paddle{1.0f, 0.0f, 0.0f};

    float paddle_pos = 0.5f;
    float paddle_speed = 0.045f;

    Position position_ball{0.0f, 0.0f};

    int frame_counter = 0;
    std::chrono::system_clock::time_point run_start_time;
    std::chrono::system_clock::time_point frame_start_time;
    std::chrono::milliseconds delta_time;
    std::chrono::milliseconds runtime;

    int gl_success;
    char gl_error_buffer[512];

    GameState game;
};
Global global;

auto handle_gl_error(const char *reason) -> void {
    std::cerr << reason << "\n"
              << global.gl_error_buffer << "\n";
    panic("GL Error");
}

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

auto reset_board() -> void {
    for (size_t row = 0; row < Constants::n_block_rows; ++row) {
        for (size_t col = 0; col < Constants::n_block_cols; ++col) {
            bool is_special = row == col;
            int value;
            Color color;
            if (is_special) {
                value = Constants::special_value;
                color = Constants::special_color;
            } else {
                value = Constants::standard_value;
                color = Constants::standard_color;
            }
            Block block = {
                .active = true,
                .position = Position{Constants::block_height, Constants::block_width},
                .value = value,
                .color = color};
            global.game.blocks[row][col] = block;
        };
    }
}

auto destroy_block(size_t row_idx, size_t col_idx) -> void {
    Block &block = global.game.blocks[row_idx][col_idx];
    if (!block.active) throw std::runtime_error("Trying to destroy inactive block!");
    block.active = false;
    global.game.score += block.value;
}

auto _main_imgui() -> void {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(global.window);
    ImGui::NewFrame();
    { // Debug
        ImGui::Begin("Debug");

        ImGui::ColorEdit3("Background", glm::value_ptr(global.c_background));
        ImGui::ColorEdit3("Ball", glm::value_ptr(global.c_ball));
        ImGui::ColorEdit3("Paddle", glm::value_ptr(global.c_paddle));

        ImGui::Text("Frame Counter: %d", global.frame_counter);
        ImGui::Text("Run Start: %s", format_time(global.run_start_time));
        ImGui::Text("Runtime: %s", format_duration(global.runtime));
        ImGui::Text("Runtime Count: %lld", global.runtime.count());
        ImGui::Text("Delta Time (ms): %lld", global.delta_time.count());

        ImGui::SliderFloat("Paddle", &global.paddle_pos, Constants::paddle_bound_left, Constants::paddle_bound_right);

        ImGui::Text("Ball Position: (%f, %f)", global.position_ball.y, global.position_ball.x);
        ImGui::End();
    } // Debug
    { // Debug::Game
        ImGui::Begin("Debug::Game");
        ImGui::Text("Lives: %d", global.game.lives);
        ImGui::Text("Score: %d", global.game.score);
        if (ImGui::Button("Reset")) {
            reset_board();
        }

        float button_size = 30.0f;
        for (int row = 0; row < Constants::n_block_rows; ++row) {
            for (int col = 0; col < Constants::n_block_cols; ++col) {
                Block &block = global.game.blocks[row][col];

                char buf[16];
                std::snprintf(buf, sizeof(buf), "##block_%d_%d", row, col);

                if (!block.active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                } else {
                    Color c_hovered = 0.5f * block.color + 0.5f * Color{1.0f, 1.0f, 1.0f};
                    Color c_active = 0.3f * block.color + 0.7f * Color{1.0f, 1.0f, 1.0f};
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(block.color.r, block.color.g, block.color.b, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c_hovered.r, c_hovered.g, c_hovered.b, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(c_active.r, c_active.g, c_active.b, 1.0f));
                }

                if (ImGui::Button(buf, ImVec2(button_size, button_size))) {
                    if (block.active) {
                        destroy_block(row, col);
                    } else {
                        block.active = true;
                    }
                }

                ImGui::PopStyleColor(3);

                if (col < Constants::n_block_cols - 1)
                    ImGui::SameLine();
            }
        }
        ImGui::End();
    } // Debug::Game

    ImGui::Render();
}

void move_paddle(float move_amount) { // TODO: Make this delta time aware
    float new_pos = global.paddle_pos + move_amount;
    global.paddle_pos = glm::clamp(global.paddle_pos + move_amount, Constants::paddle_bound_left, Constants::paddle_bound_right);
}

void _main_handle_inputs() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT)
            global.running = false;

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                global.running = false;
                break;
            case SDLK_d:
                move_paddle(global.paddle_speed);
                break;
            case SDLK_a:
                move_paddle(-global.paddle_speed);
                break;
            default:
                break;
            }
        }
    }
}

void _main_render() {
    glViewport(0, 0, (int)global.imgui_io.DisplaySize.x, (int)global.imgui_io.DisplaySize.y);
    glClearColor(global.c_background.r, global.c_background.g, global.c_background.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(global.shader_program);
    glBindVertexArray(global.paddle_vao);

    UniformLocation ubo_time = glGetUniformLocation(global.shader_program, "time");
    glUniform1f(ubo_time, (float)global.runtime.count());

    UniformLocation ubo_pos = glGetUniformLocation(global.shader_program, "u_Pos");
    UniformLocation ubo_scale = glGetUniformLocation(global.shader_program, "u_Scale");
    UniformLocation ubo_color = glGetUniformLocation(global.shader_program, "u_Color");

    { // Paddle
        glUniform2f(ubo_pos, 2.0f * (global.paddle_pos - 0.5f), -0.8f);
        glUniform2f(ubo_scale, 6.0f, 3.0f);
        glUniform3f(ubo_color, global.c_paddle.r, global.c_paddle.g, global.c_paddle.b);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    { // Ball
        glUniform2f(ubo_pos, 0.0f, 0.0f);
        glUniform2f(ubo_scale, 2.0f, 2.0f);
        glUniform3f(ubo_color, global.c_ball.r, global.c_ball.g, global.c_ball.b);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    { // Blocks
        glUniform2f(ubo_pos, 0.0f, 0.75f);
        glUniform2f(ubo_scale, 3.0f, 2.0f);
        glUniform3f(ubo_color, 0.0f, 0.0f, 0.0f);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

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

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

    // Create window with OpenGL context
    global.window = SDL_CreateWindow(
        Constants::window_title.data(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        Constants::window_width, Constants::window_height,
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
    glsl_version = "#version 410 core";
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

auto compile_shader_from_file(const char *filepath, GLenum shader_type) -> Shader {
    std::ifstream in(filepath);
    if (!in) {
        std::cerr << "Couldn't open file " << filepath << "\n";
        return 0;
    }
    std::string shader_source_str;
    {
        std::ostringstream ss;
        ss << in.rdbuf();
        shader_source_str = ss.str();
    }
    in.close();
    const char *vertex_shader_source = shader_source_str.c_str();
    Shader shader;
    shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &global.gl_success);
    if (!global.gl_success) {
        glGetShaderInfoLog(shader, 512, nullptr, global.gl_error_buffer);
        handle_gl_error("Shader Compilation Failed.");
    }
    return shader;
}

auto setup_shader_program() -> void {
    Shader vertex_shader = compile_shader_from_file(Constants::fp_vertex_shader, GL_VERTEX_SHADER);
    if (vertex_shader == 0) panic("Failed to compile vertex shader.");
    Shader fragment_shader = compile_shader_from_file(Constants::fp_fragment_shader, GL_FRAGMENT_SHADER);
    if (fragment_shader == 0) panic("Failed to compile fragment shader.");

    global.shader_program = glCreateProgram();

    glAttachShader(global.shader_program, vertex_shader);
    glAttachShader(global.shader_program, fragment_shader);

    glLinkProgram(global.shader_program);
    // Check link errors:
    glGetProgramiv(global.shader_program, GL_LINK_STATUS, &global.gl_success);
    if (!global.gl_success) {
        glGetProgramInfoLog(global.shader_program, 512, nullptr, global.gl_error_buffer);
        panic(std::string("Shader Program Link Failed: ") + global.gl_error_buffer);
    }

    glUseProgram(global.shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    UniformLocation ubo_paddle_width = glGetUniformLocation(global.shader_program, "paddle_width_ratio");
    glUniform1f(ubo_paddle_width, Constants::paddle_width / Constants::window_width);
    UniformLocation ubo_paddle_height = glGetUniformLocation(global.shader_program, "paddle_height_ratio");
    glUniform1f(ubo_paddle_height, Constants::paddle_height / Constants::window_height);
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
        sizeof(Constants::square_vertices),
        Constants::square_vertices.data(),
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
        sizeof(Constants::square_indices),
        Constants::square_indices.data(),
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
    if (!setup()) panic("Setup failed!");

    setup_shader_program();

    setup_paddle_vao();

    reset_board();

    global.run_start_time = std::chrono::system_clock::now();
    global.running = true;
    // For initial delta time computation
    global.frame_start_time = std::chrono::system_clock::now();
    while (global.running) {
        global.delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - global.frame_start_time);
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