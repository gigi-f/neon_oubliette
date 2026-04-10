#include "SDLRenderer.h"
#include <iostream>
#include <algorithm>

namespace NeonOubliette {

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

SDLRenderer::SDLRenderer() = default;

SDLRenderer::~SDLRenderer() {
    shutdown();
}

bool SDLRenderer::initialize(int window_width_cells, int window_height_cells, bool headless) {
    if (headless) return true;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        std::cerr << "[SDL] Init failed: " << SDL_GetError() << "\n";
        return false;
    }
    if (TTF_Init() == -1) {
        std::cerr << "[SDL] TTF_Init failed: " << TTF_GetError() << "\n";
        return false;
    }

    int win_px_w = window_width_cells  * cell_width_;
    int win_px_h = window_height_cells * cell_height_;

    window_ = SDL_CreateWindow("Neon Oubliette",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               win_px_w, win_px_h,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) {
        std::cerr << "[SDL] CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Accelerated + vsync; fall back to software if GPU unavailable
    renderer_ = SDL_CreateRenderer(window_, -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer_) {
            std::cerr << "[SDL] CreateRenderer failed: " << SDL_GetError() << "\n";
            return false;
        }
        std::cerr << "[SDL] Warning: fell back to software renderer\n";
    }

    // Enable alpha blending globally
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // ── Font ─────────────────────────────────────────────────────────────────
    const char* font_paths[] = {
        "/System/Library/Fonts/Monaco.ttf",
        "/opt/homebrew/share/fonts/JetBrainsMonoNerdFont-Regular.ttf",
        nullptr
    };
    for (int i = 0; font_paths[i]; ++i) {
        font_ = TTF_OpenFont(font_paths[i], cell_height_ - 2);
        if (font_) {
            std::cout << "[SDL] Font: " << font_paths[i] << "\n";
            break;
        }
    }
    if (!font_) {
        std::cerr << "[SDL] Warning: no font — tiles will show as blocks\n";
    }

    // ── Glyph atlas ──────────────────────────────────────────────────────────
    if (font_ && !build_glyph_atlas()) {
        std::cerr << "[SDL] Warning: glyph atlas build failed\n";
    }

    return true;
}

void SDLRenderer::shutdown() {
    destroy_glyph_atlas();
    if (font_)     { TTF_CloseFont(font_); font_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow(window_); window_ = nullptr; }
    TTF_Quit();
    SDL_Quit();
}

// ─────────────────────────────────────────────────────────────────────────────
// Glyph Atlas: render ASCII 32-126 once in white; tint at draw time
// ─────────────────────────────────────────────────────────────────────────────

bool SDLRenderer::build_glyph_atlas() {
    SDL_Color white = {255, 255, 255, 255};
    for (int c = GLYPH_FIRST; c <= GLYPH_LAST; ++c) {
        char buf[2] = {(char)c, '\0'};
        SDL_Surface* surf = TTF_RenderText_Solid(font_, buf, white);
        if (!surf) continue;

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_FreeSurface(surf);
        if (!tex) continue;

        // Allow color mod tinting
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        int w, h;
        SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);

        int idx = c - GLYPH_FIRST;
        glyph_atlas_[idx] = tex;
        glyph_w_[idx]     = w;
        glyph_h_[idx]     = h;
    }
    std::cout << "[SDL] Glyph atlas built (" << GLYPH_COUNT << " chars)\n";
    return true;
}

void SDLRenderer::destroy_glyph_atlas() {
    for (auto* tex : glyph_atlas_) {
        if (tex) SDL_DestroyTexture(tex);
    }
    glyph_atlas_.fill(nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Layer management
// ─────────────────────────────────────────────────────────────────────────────

void SDLRenderer::create_layer(const RenderLayerInfo& info) {
    Layer l;
    l.info = info;
    l.commands.reserve(info.width_cells * info.height_cells);
    layers_[info.name] = std::move(l);
    sorted_layer_names_.push_back(info.name);
    sort_layers();
}

void SDLRenderer::sort_layers() {
    std::sort(sorted_layer_names_.begin(), sorted_layer_names_.end(),
        [this](const std::string& a, const std::string& b) {
            return layers_[a].info.z_index < layers_[b].info.z_index;
        });
}

void SDLRenderer::clear_layer(const std::string& layer_name) {
    auto it = layers_.find(layer_name);
    if (it != layers_.end()) {
        it->second.commands.clear();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw commands — just enqueue; actual SDL calls happen in render_frame()
// ─────────────────────────────────────────────────────────────────────────────

void SDLRenderer::draw_tile(const std::string& layer_name, int cell_x, int cell_y,
                             const std::string& utf8_char, ColorRGB color, ColorRGB bg_color) {
    auto it = layers_.find(layer_name);
    if (it == layers_.end()) return;
    char ch = utf8_char.empty() ? '?' : utf8_char[0];
    it->second.commands.push_back({cell_x, cell_y, ch, {}, color, bg_color, true});
}

void SDLRenderer::draw_text(const std::string& layer_name, int cell_x, int cell_y,
                             std::string_view text, ColorRGB fg, ColorRGB bg) {
    auto it = layers_.find(layer_name);
    if (it == layers_.end()) return;
    it->second.commands.push_back({cell_x, cell_y, 0, std::string(text), fg, bg, false});
}

// ─────────────────────────────────────────────────────────────────────────────
// Flush a single draw command — called from render_frame
// ─────────────────────────────────────────────────────────────────────────────

void SDLRenderer::flush_draw_command(const DrawCommand& cmd) {
    const int px = cmd.x * cell_width_;
    const int py = cmd.y * cell_height_;

    if (cmd.is_tile) {
        // ── Background block ─────────────────────────────────────────────────
        if (cmd.bg.a > 0) {
            SDL_SetRenderDrawColor(renderer_, cmd.bg.r, cmd.bg.g, cmd.bg.b, cmd.bg.a);
        } else {
            // Subtle dark tint from fg color so the cell is never fully invisible
            SDL_SetRenderDrawColor(renderer_,
                cmd.fg.r >> 3, cmd.fg.g >> 3, cmd.fg.b >> 3, 210);
        }
        SDL_Rect cell_rect = {px, py, cell_width_, cell_height_};
        SDL_RenderFillRect(renderer_, &cell_rect);

        // ── Glyph from atlas (zero allocations) ──────────────────────────────
        int idx = (unsigned char)cmd.ch - GLYPH_FIRST;
        if (idx >= 0 && idx < GLYPH_COUNT && glyph_atlas_[idx]) {
            SDL_SetTextureColorMod(glyph_atlas_[idx], cmd.fg.r, cmd.fg.g, cmd.fg.b);
            SDL_SetTextureAlphaMod(glyph_atlas_[idx], cmd.fg.a);
            SDL_Rect dst = {px, py + (cell_height_ - glyph_h_[idx]) / 2,
                            glyph_w_[idx], glyph_h_[idx]};
            SDL_RenderCopy(renderer_, glyph_atlas_[idx], nullptr, &dst);
        }
    } else {
        // ── Multi-char text (HUD strings) ─────────────────────────────────────
        // Background strip
        if (cmd.bg.a > 0) {
            int strip_w = (int)cmd.text.size() * cell_width_;
            SDL_Rect bg_rect = {px, py, strip_w, cell_height_};
            SDL_SetRenderDrawColor(renderer_, cmd.bg.r, cmd.bg.g, cmd.bg.b, cmd.bg.a);
            SDL_RenderFillRect(renderer_, &bg_rect);
        }
        // Each char from atlas — still zero per-frame allocations
        int cx = px;
        for (char c : cmd.text) {
            int idx = (unsigned char)c - GLYPH_FIRST;
            if (idx >= 0 && idx < GLYPH_COUNT && glyph_atlas_[idx]) {
                SDL_SetTextureColorMod(glyph_atlas_[idx], cmd.fg.r, cmd.fg.g, cmd.fg.b);
                SDL_SetTextureAlphaMod(glyph_atlas_[idx], cmd.fg.a);
                SDL_Rect dst = {cx, py + (cell_height_ - glyph_h_[idx]) / 2,
                                glyph_w_[idx], glyph_h_[idx]};
                SDL_RenderCopy(renderer_, glyph_atlas_[idx], nullptr, &dst);
                cx += cell_width_;
            } else {
                cx += cell_width_;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// render_frame — one clear, iterate layers, present
// ─────────────────────────────────────────────────────────────────────────────

void SDLRenderer::render_frame() {
    if (!renderer_) return;

    SDL_SetRenderDrawColor(renderer_, 8, 10, 12, 255); // near-black background
    SDL_RenderClear(renderer_);

    for (const auto& name : sorted_layer_names_) {
        const auto& layer = layers_[name];
        for (const auto& cmd : layer.commands) {
            flush_draw_command(cmd);
        }
    }

    SDL_RenderPresent(renderer_);
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

void SDLRenderer::poll_input(std::vector<Systems::EngineInputEvent>& out_events) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            out_events.push_back({'q', true});
            continue;
        }
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool is_press = (event.type == SDL_KEYDOWN);
            uint32_t key_id = 0;
            switch (event.key.keysym.sym) {
                case SDLK_UP:     key_id = Systems::EngineKey::Key_Up;    break;
                case SDLK_DOWN:   key_id = Systems::EngineKey::Key_Down;  break;
                case SDLK_LEFT:   key_id = Systems::EngineKey::Key_Left;  break;
                case SDLK_RIGHT:  key_id = Systems::EngineKey::Key_Right; break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: key_id = Systems::EngineKey::Key_Enter; break;
                case SDLK_ESCAPE: key_id = Systems::EngineKey::Key_Esc;   break;
                case SDLK_TAB:    key_id = Systems::EngineKey::Key_Tab;   break;
                default:
                    if (event.key.keysym.sym >= 32 && event.key.keysym.sym <= 126)
                        key_id = (uint32_t)event.key.keysym.sym;
                    break;
            }
            if (key_id) out_events.push_back({key_id, is_press});
        }
    }
}

} // namespace NeonOubliette
