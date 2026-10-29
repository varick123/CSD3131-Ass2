#pragma once
#include "seam.h"
#include <string>
#include <vector>

namespace seam {
enum class Method { DP, Greedy, Graph };
Method parse_method(const std::string& name);
std::string method_name(Method method);
Seam find_seam(const EnergyMap& energy, Method method);
struct Step {
    int index = 0, width = 0, height = 0;
    char axis = 'v';
    Seam seam;
    double energy_ms = 0, search_ms = 0, remove_ms = 0;
};
struct ResizeResult {
    Image image;
    std::vector<Step> steps;
    double energy_ms = 0, search_ms = 0, remove_ms = 0;
    double total_ms() const { return energy_ms + search_ms + remove_ms; }
};
ResizeResult resize(const Image& original, int width, int height, Method method,
                    bool horizontal_first = false, const std::string& artifacts = "");
Image overlay(const Image& image, const std::vector<int>& columns);
Image energy_preview(const EnergyMap& energy);
std::string json_escape(const std::string& text);
std::string result_json(const ResizeResult& result, Method method,
                        int original_width, int original_height, bool horizontal_first);
void write_text(const std::string& path, const std::string& text);
} // namespace seam
