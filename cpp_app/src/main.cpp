#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "stock/mock_stock_api.hpp"
#include "stock/rust_stock_api.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace {

#if defined(__APPLE__)
constexpr const char* kGlslVersion = "#version 150";
#else
constexpr const char* kGlslVersion = "#version 130";
#endif

constexpr int kInitialWindowWidth = 480;
constexpr int kInitialWindowHeight = 320;
constexpr ImVec2 kPanelPosition{24.0f, 24.0f};
constexpr float kPanelWidth = 420.0f;
constexpr float kGetPriceButtonWidth = 110.0f;
constexpr ImVec4 kClearColor{0.09f, 0.09f, 0.11f, 1.0f};
constexpr ImVec4 kGainColor{0.4f, 1.0f, 0.4f, 1.0f};
constexpr ImVec4 kLossColor{1.0f, 0.4f, 0.4f, 1.0f};
constexpr ImVec4 kErrorColor{1.0f, 0.4f, 0.4f, 1.0f};

[[noreturn]] void ThrowSdlError(const std::string& call) {
    throw std::runtime_error(call + " failed: " + SDL_GetError());
}

class SdlSession {
public:
    SdlSession() {
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            ThrowSdlError("SDL_Init");
        }
    }
    ~SdlSession() { SDL_Quit(); }

    SdlSession(const SdlSession&) = delete;
    SdlSession& operator=(const SdlSession&) = delete;
};

struct WindowDeleter {
    void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
};

struct GlContextDeleter {
    void operator()(SDL_GLContext context) const { SDL_GL_DeleteContext(context); }
};

using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using GlContextPtr = std::unique_ptr<void, GlContextDeleter>;

void SetGlAttributes() {
#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}

WindowPtr CreateWindow() {
    SetGlAttributes();
    WindowPtr window(SDL_CreateWindow(
        "Stock Price", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kInitialWindowWidth,
        kInitialWindowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
    if (!window) {
        ThrowSdlError("SDL_CreateWindow");
    }
    return window;
}

GlContextPtr CreateGlContext(SDL_Window* window) {
    GlContextPtr context(SDL_GL_CreateContext(window));
    if (!context) {
        ThrowSdlError("SDL_GL_CreateContext");
    }
    SDL_GL_MakeCurrent(window, context.get());
    SDL_GL_SetSwapInterval(1);
    return context;
}

/// An SDL window with a current OpenGL context.
class GlWindow {
public:
    GlWindow() : window_(CreateWindow()), context_(CreateGlContext(window_.get())) {}

    [[nodiscard]] SDL_Window* sdl() const { return window_.get(); }
    [[nodiscard]] SDL_GLContext gl_context() const { return context_.get(); }

private:
    WindowPtr window_;
    GlContextPtr context_;
};

class ImGuiSession {
public:
    explicit ImGuiSession(const GlWindow& window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window.sdl(), window.gl_context());
        ImGui_ImplOpenGL3_Init(kGlslVersion);
    }
    ~ImGuiSession() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    ImGuiSession(const ImGuiSession&) = delete;
    ImGuiSession& operator=(const ImGuiSession&) = delete;
};

void ShowQuote(const std::string& symbol, const stock::Quote& quote) {
    ImGui::Text("%s", symbol.c_str());
    ImGui::Text("Price: %.2f", quote.current);
    if (quote.change) {
        ImGui::TextColored(*quote.change >= 0.0 ? kGainColor : kLossColor,
                           "Change: %+.2f (%.2f%%)", *quote.change,
                           quote.percent_change.value_or(0.0));
    }
    ImGui::Text("Open %.2f  High %.2f  Low %.2f  Prev %.2f", quote.open, quote.high, quote.low,
                quote.previous_close);
}

void ShowQuoteResult(const std::string& symbol, const stock::QuoteResult& result) {
    if (const auto* error = std::get_if<std::string>(&result)) {
        ImGui::TextColored(kErrorColor, "Error: %s", error->c_str());
        return;
    }
    ShowQuote(symbol, std::get<stock::Quote>(result));
}

/// The "Stock Price" panel. Lookups run on a background thread so the UI
/// keeps rendering while the HTTP request is in flight.
class StockPriceView {
public:
    explicit StockPriceView(std::unique_ptr<stock::IStockApi> api) : api_(std::move(api)) {}

    void Render() {
        PollLookup();

        ImGui::SetNextWindowPos(kPanelPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(kPanelWidth, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Stock Price");

        if (RenderSymbolInput()) {
            StartLookup();
        }
        ImGui::Separator();
        RenderResult();

        ImGui::End();
    }

private:
    [[nodiscard]] bool IsLoading() const { return pending_.valid(); }

    /// Returns true when the user submits a symbol.
    bool RenderSymbolInput() {
        ImGui::TextUnformatted("Stock symbol");
        ImGui::BeginDisabled(IsLoading());
        ImGui::SetNextItemWidth(-kGetPriceButtonWidth);
        const bool entered = ImGui::InputText(
            "##symbol", symbol_.data(), symbol_.size(),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase |
                ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        const bool clicked = ImGui::Button("Get price");
        ImGui::EndDisabled();
        return entered || clicked;
    }

    void StartLookup() {
        queried_symbol_ = symbol_.data();
        pending_ = std::async(std::launch::async, [api = api_.get(), symbol = queried_symbol_] {
            return api->quote(symbol);
        });
    }

    void PollLookup() {
        if (IsLoading() && pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            result_ = pending_.get();
        }
    }

    void RenderResult() const {
        if (IsLoading()) {
            ImGui::TextDisabled("Loading %s...", queried_symbol_.c_str());
        } else if (result_) {
            ShowQuoteResult(queried_symbol_, *result_);
        } else {
            ImGui::TextDisabled("Enter a symbol and press Get price.");
        }
    }

    std::unique_ptr<stock::IStockApi> api_;
    std::array<char, 32> symbol_ = {'A', 'A', 'P', 'L', '\0'};
    std::string queried_symbol_;
    std::optional<stock::QuoteResult> result_;
    // Declared after `api_` so it is destroyed first: the future's destructor
    // waits for a running lookup, which still uses `api_`.
    std::future<stock::QuoteResult> pending_;
};

std::unique_ptr<stock::IStockApi> CreateStockApi(int argc, char** argv) {
    const bool use_mock = std::any_of(argv + 1, argv + argc, [](const char* arg) {
        return std::string_view(arg) == "--mock";
    });
    if (use_mock) {
        return std::make_unique<stock::MockStockApi>();
    }
    return std::make_unique<stock::RustStockApi>();
}

/// Handles pending events. Returns false once the user closes the window.
bool PollEvents(const GlWindow& window) {
    bool running = true;
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        const bool window_closed = event.type == SDL_WINDOWEVENT &&
                                   event.window.event == SDL_WINDOWEVENT_CLOSE &&
                                   event.window.windowID == SDL_GetWindowID(window.sdl());
        if (event.type == SDL_QUIT || window_closed) {
            running = false;
        }
    }
    return running;
}

void PresentFrame(const GlWindow& window) {
    ImGui::Render();
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window.sdl(), &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(kClearColor.x, kClearColor.y, kClearColor.z, kClearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window.sdl());
}

void RenderFrame(const GlWindow& window, StockPriceView& view) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    view.Render();
    PresentFrame(window);
}

} // namespace

int main(int argc, char** argv) {
    try {
        SdlSession sdl;
        GlWindow window;
        ImGuiSession imgui(window);
        StockPriceView view(CreateStockApi(argc, argv));
        while (PollEvents(window)) {
            RenderFrame(window, view);
        }
        return 0;
    } catch (const std::runtime_error& error) {
        SDL_Log("%s", error.what());
        return 1;
    }
}
