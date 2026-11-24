#include "seam.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace seam {

Seam find_dp(const EnergyMap& energy, std::vector<Cost>* cumulative) {
    validate_energy(energy);
    if (cumulative == &energy.values) {
        throw std::invalid_argument("Cumulative output must not alias the input energy storage");
    }
    const std::size_t count = checked_cells(energy.height, energy.width);
    const std::size_t width = static_cast<std::size_t>(energy.width);
    std::vector<Cost> previous(energy.values.begin(), energy.values.begin() + width);
    std::vector<Cost> current(width);
    std::vector<std::int8_t> parents(count, 0);
    if (cumulative != nullptr) {
        cumulative->assign(count, 0);
        std::copy(previous.begin(), previous.end(), cumulative->begin());
    }

    // Each state is the cheapest seam prefix ending at this pixel. Only the
    // preceding row of costs is needed, while parent offsets retain the path.
    for (int row = 1; row < energy.height; ++row) {
        const std::size_t offset = static_cast<std::size_t>(row) * width;
        for (int col = 0; col < energy.width; ++col) {
            int best_parent = std::max(0, col - 1);
            Cost best_cost = previous[static_cast<std::size_t>(best_parent)];
            const int last_parent = std::min(energy.width - 1, col + 1);
            for (int candidate = best_parent + 1; candidate <= last_parent; ++candidate) {
                const Cost candidate_cost = previous[static_cast<std::size_t>(candidate)];
                if (candidate_cost < best_cost) {
                    best_cost = candidate_cost;
                    best_parent = candidate;
                }
            }
            const std::size_t index = offset + static_cast<std::size_t>(col);
            current[static_cast<std::size_t>(col)] = checked_add(energy.values[index], best_cost);
            parents[index] = static_cast<std::int8_t>(best_parent - col);
        }
        if (cumulative != nullptr) {
            std::copy(current.begin(), current.end(), cumulative->begin() + offset);
        }
        previous.swap(current);
    }

    int col = 0;
    for (int candidate = 1; candidate < energy.width; ++candidate) {
        if (previous[static_cast<std::size_t>(candidate)] < previous[static_cast<std::size_t>(col)]) {
            col = candidate;
        }
    }
    Seam path;
    path.cost = previous[static_cast<std::size_t>(col)];
    path.columns.resize(static_cast<std::size_t>(energy.height));
    for (int row = energy.height - 1; row >= 0; --row) {
        path.columns[static_cast<std::size_t>(row)] = col;
        if (row > 0) {
            const std::size_t index = static_cast<std::size_t>(row) * width + static_cast<std::size_t>(col);
            col += static_cast<int>(parents[index]);
        }
    }
    validate_seam(energy, path);
    return path;
}

} // namespace seam
