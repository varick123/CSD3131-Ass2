#include "flow.h"
#include "seam.h"

#include <chrono>
#include <limits>
#include <stdexcept>

namespace seam {

Seam find_graph_cut(const EnergyMap& energy) {
    using Clock = std::chrono::steady_clock;
    const auto build_start = Clock::now();
    const Cost energy_sum = validate_energy(energy);
    if (energy_sum == std::numeric_limits<Cost>::max()) {
        throw std::overflow_error("Energy sum leaves no representable constraint capacity");
    }
    const Cost infinity = energy_sum + 1;

    // The size check precedes multiplication and all narrowing conversions.
    const auto rows = static_cast<std::size_t>(energy.height);
    const auto stride = static_cast<std::size_t>(energy.width) + 1;
    const auto largest_vertex_count =
        static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (rows > (largest_vertex_count - 2) / stride) {
        throw std::overflow_error("Seam graph exceeds the supported vertex index range");
    }
    const auto boundaries = rows * stride;
    const int source = static_cast<int>(boundaries);
    const int sink = source + 1;
    const int vertices = sink + 1;
    Dinic graph(vertices);
    const auto node = [stride](int row, int boundary) {
        return static_cast<int>(static_cast<std::size_t>(row) * stride +
                                static_cast<std::size_t>(boundary));
    };

    for (int row = 0; row < energy.height; ++row) {
        graph.add_edge(source, node(row, 0), infinity);
        graph.add_edge(node(row, energy.width), sink, infinity);
        for (int col = 0; col < energy.width; ++col) {
            graph.add_edge(node(row, col), node(row, col + 1), energy.at(row, col));
            graph.add_edge(node(row, col + 1), node(row, col), infinity);
        }
        for (int boundary = 1; boundary <= energy.width; ++boundary) {
            if (row > 0) {
                graph.add_edge(node(row, boundary), node(row - 1, boundary - 1), infinity);
            }
            if (row + 1 < energy.height) {
                graph.add_edge(node(row, boundary), node(row + 1, boundary - 1), infinity);
            }
        }
    }

    Seam result;
    result.graph_vertices = static_cast<std::size_t>(vertices);
    // One count per add_edge call: structural original edges are included;
    // zero-capacity residual partners are excluded.
    result.graph_edges = graph.edge_count();
    const auto flow_start = Clock::now();
    result.graph_build_ms =
        std::chrono::duration<double, std::milli>(flow_start - build_start).count();
    const Cost flow = graph.max_flow(source, sink);
    const auto flow_end = Clock::now();
    result.flow_ms =
        std::chrono::duration<double, std::milli>(flow_end - flow_start).count();

    const auto reached = graph.source_side(source);
    result.columns.reserve(rows);
    for (int row = 0; row < energy.height; ++row) {
        int seam_column = -1;
        bool prefix_ended = false;
        for (int boundary = 0; boundary <= energy.width; ++boundary) {
            if (!reached[static_cast<std::size_t>(node(row, boundary))]) {
                prefix_ended = true;
            } else {
                if (prefix_ended) {
                    throw std::logic_error("Cut violates a row-prefix constraint");
                }
                seam_column = boundary;
            }
        }
        if (seam_column < 0 || seam_column >= energy.width) {
            throw std::logic_error("Cut violates a seam endpoint constraint");
        }
        result.columns.push_back(seam_column);
        result.cost = checked_add(result.cost, energy.at(row, seam_column));
    }
    validate_seam(energy, result);
    if (flow != result.cost) {
        throw std::logic_error("Maximum flow and decoded seam cost disagree");
    }
    return result;
}

} // namespace seam
