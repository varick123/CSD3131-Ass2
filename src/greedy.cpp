#include "seam.h"

#include <algorithm>

namespace seam {

Seam find_greedy(const EnergyMap& energy) {
    validate_energy(energy);
    const std::size_t width = static_cast<std::size_t>(energy.width);
    Seam path;
    path.columns.resize(static_cast<std::size_t>(energy.height));

    int col = 0;
    for (int candidate = 1; candidate < energy.width; ++candidate) {
        if (energy.values[static_cast<std::size_t>(candidate)] < energy.values[static_cast<std::size_t>(col)]) {
            col = candidate;
        }
    }
    path.columns[0] = col;
    path.cost = energy.values[static_cast<std::size_t>(col)];

    // Choose the cheapest legal next pixel without looking at future rows.
    // Ascending candidate order and strict comparisons give leftmost ties.
    for (int row = 1; row < energy.height; ++row) {
        const std::size_t offset = static_cast<std::size_t>(row) * width;
        int best_col = std::max(0, col - 1);
        const int last_col = std::min(energy.width - 1, col + 1);
        for (int candidate = best_col + 1; candidate <= last_col; ++candidate) {
            if (energy.values[offset + static_cast<std::size_t>(candidate)] <
                energy.values[offset + static_cast<std::size_t>(best_col)]) {
                best_col = candidate;
            }
        }
        col = best_col;
        path.columns[static_cast<std::size_t>(row)] = col;
        path.cost = checked_add(path.cost, energy.values[offset + static_cast<std::size_t>(col)]);
    }
    validate_seam(energy, path);
    return path;
}

} // namespace seam
