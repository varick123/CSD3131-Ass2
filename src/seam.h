#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace seam {

using Cost = std::int64_t;

std::size_t checked_cells(int height, int width);
Cost checked_add(Cost a, Cost b);

// Images are always stored as three 8-bit RGB channels, in row-major order.
struct Image {
    int height = 0;
    int width = 0;
    std::vector<std::uint8_t> pixels;
    Image() = default;
    Image(int height, int width);
    std::uint8_t& at(int row, int col, int channel);
    std::uint8_t at(int row, int col, int channel) const;
};

struct EnergyMap {
    int height = 0;
    int width = 0;
    std::vector<Cost> values;
    EnergyMap() = default;
    EnergyMap(int height, int width);
    Cost& at(int row, int col);
    Cost at(int row, int col) const;
};

struct Seam {
    std::vector<int> columns;
    Cost cost = 0;
    double graph_build_ms = 0;
    double flow_ms = 0;
    std::size_t graph_vertices = 0;
    std::size_t graph_edges = 0;
};

void validate_image(const Image& image);
Cost validate_energy(const EnergyMap& energy);
void validate_seam(const EnergyMap& energy, const Seam& path);
Image transpose(const Image& image);
EnergyMap transpose(const EnergyMap& energy);
Image remove_vertical(const Image& image, const std::vector<int>& columns);

// Integer grayscale: (299*R + 587*G + 114*B + 500)/1000.
// Signed 3x3 Sobel, replicated borders, E = abs(Gx) + abs(Gy).
EnergyMap compute_energy(const Image& image);
Seam find_dp(const EnergyMap& energy, std::vector<Cost>* cumulative = nullptr);
Seam find_greedy(const EnergyMap& energy);
Seam find_graph_cut(const EnergyMap& energy);

struct ImageInfo {
    int width = 0;
    int height = 0;
};
ImageInfo inspect_image(const std::string& path);
Image load_image(const std::string& path);
void save_png(const Image& image, const std::string& path);

} // namespace seam
