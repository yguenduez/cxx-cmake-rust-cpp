#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "stock/rust_stock_api.hpp"

#include <array>
#include <memory>
#include <string>

namespace {

void ShowQuote(const std::string& symbol, const stock::QuoteResult& result) {
    if (!result.ok()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error: %s", result.error.c_str());
        return;
    }

    const stock::Quote& quote = *result.quote;
    ImGui::Text("%s", symbol.c_str());
    ImGui::Text("Price: %.2f", quote.current);
    if (quote.change) {
        const float color = *quote.change >= 0.0 ? 0.4f : 1.0f;
        ImGui::TextColored(ImVec4(1.0f - color, 0.4f + color * 0.6f, 0.4f, 1.0f),
                           "Change: %+.2f (%.2f%%)", *quote.change,
                           quote.percent_change.value_or(0.0));
    }
    ImGui::Text("Open %.2f  High %.2f  Low %.2f  Prev %.2f", quote.open, quote.high, quote.low,
                quote.previous_close);
}

} // namespace

int main(int, char**) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow(
        "Stock Price", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 480, 320,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Calls into `rust_lib` through the cxx.rs bridge.
    std::unique_ptr<stock::IStockApi> api = std::make_unique<stock::RustStockApi>();

    std::array<char, 32> symbol = {'A', 'A', 'P', 'L', '\0'};
    stock::QuoteResult result;
    std::string queried_symbol;
    bool has_result = false;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT &&
                       event.window.event == SDL_WINDOWEVENT_CLOSE &&
                       event.window.windowID == SDL_GetWindowID(window)) {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Stock Price");

        ImGui::TextUnformatted("Stock symbol");
        ImGui::SetNextItemWidth(-110.0f);
        bool submit = ImGui::InputText("##symbol", symbol.data(), symbol.size(),
                                       ImGuiInputTextFlags_EnterReturnsTrue |
                                           ImGuiInputTextFlags_CharsUppercase |
                                           ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        submit = ImGui::Button("Get price") || submit;

        if (submit) {
            queried_symbol = symbol.data();
            result = api->quote(queried_symbol);
            has_result = true;
        }

        ImGui::Separator();
        if (has_result) {
            ShowQuote(queried_symbol, result);
        } else {
            ImGui::TextDisabled("Enter a symbol and press Get price.");
        }

        ImGui::End();

        ImGui::Render();
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.09f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
