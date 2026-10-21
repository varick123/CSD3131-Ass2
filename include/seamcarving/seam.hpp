#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace seam {

using Cost = std::int64_t;

// Proposed team convention: row-major, 8-bit RGB (three values per pixel).
// Implementations must validate dimensions and storage before indexing.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
};

struct EnergyMap {
    int width = 0;
    int height = 0;
    std::vector<Cost> values;
};

// Vertical seam: one column per row; consecutive columns differ by at most one.
struct Seam {
    std::vector<int> columns;
    Cost cost = 0;
};

enum class Method { DynamicProgramming, Greedy, GraphCut };

class NotImplemented : public std::logic_error {
public:
    explicit NotImplemented(const std::string& task)
        : std::logic_error("Not implemented: " + task) {}
};

Image load_image(const std::string& path);
void save_image(const Image& image, const std::string& path);
EnergyMap compute_energy(const Image& image);
Seam find_dp(const EnergyMap& energy);
Seam find_greedy(const EnergyMap& energy);
Seam find_graph_cut(const EnergyMap& energy);
Image transpose(const Image& image);
Image remove_vertical(const Image& image, const Seam& seam);
Image resize(const Image& image, int target_width, int target_height, Method method);

} // namespace seam
