#ifndef NEON_OUBLIETTE_SDL_RENDERER_H
#define NEON_OUBLIETTE_SDL_RENDERER_H

#include "IRenderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <unordered_map>
#include <array>
#include <vector>
#include <string>

namespace NeonOubliette {

// Glyph slot in the pre-baked ASCII atlas (chars 32-126 inclusive).
// Each glyph is rendered white at init; colorized via SDL_SetTextureColorMod at draw time.
static constexpr int GLYPH_FIRST = 32;
static constexpr int GLYPH_LAST  = 126;
static constexpr int GLYPH_COUNT = GLYPH_LAST - GLYPH_FIRST + 1;

class SDLRenderer : public IRenderer {
public:
    SDLRenderer();
    virtual ~SDLRenderer();

    bool initialize(int window_width_cells, int window_height_cells, bool headless) override;
    void shutdown() override;

    void create_layer(const RenderLayerInfo& info) override;
    void clear_layer(const std::string& layer_name) override;
    
    void draw_tile(const std::string& layer_name, int cell_x, int cell_y, const std::string& utf8_char, ColorRGB color, ColorRGB bg_color) override;
    void draw_text(const std::string& layer_name, int cell_x, int cell_y, std::string_view text, ColorRGB fg, ColorRGB bg) override;

    void render_frame() override;
    
    void poll_input(std::vector<Systems::EngineInputEvent>& out_events) override;

    int get_cell_width() const override { return cell_width_; }
    int get_cell_height() const override { return cell_height_; }

private:
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font*     font_     = nullptr;
    
    int cell_width_  = 10;
    int cell_height_ = 18;

    // ── Glyph atlas ──────────────────────────────────────────────────────────
    // One SDL_Texture per ASCII char, rendered in white so we can tint freely.
    std::array<SDL_Texture*, GLYPH_COUNT> glyph_atlas_{};
    // Actual pixel dimensions for each glyph (may vary for proportional fonts)
    std::array<int, GLYPH_COUNT> glyph_w_{};
    std::array<int, GLYPH_COUNT> glyph_h_{};

    bool build_glyph_atlas();
    void destroy_glyph_atlas();

    // ── Draw command list ─────────────────────────────────────────────────────
    struct DrawCommand {
        int      x, y;
        char     ch;        // single ASCII character (0 = text-only, use full string)
        std::string text;   // used only for multi-char HUD strings
        ColorRGB fg, bg;
        bool     is_tile;   // true = single char from atlas; false = multi-char text
    };
    
    struct Layer {
        RenderLayerInfo           info;
        std::vector<DrawCommand>  commands;
    };
    
    std::unordered_map<std::string, Layer> layers_;
    std::vector<std::string>               sorted_layer_names_;
    
    void sort_layers();
    void flush_draw_command(const DrawCommand& cmd);
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_SDL_RENDERER_H
