#pragma once

#include "seam.h"

#include <cstddef>
#include <vector>

namespace seam {

// Dinic's algorithm with an adjacency-list residual graph. All capacities are
// nonnegative signed 64-bit integers. Blocking paths use explicit stacks, so
// their depth does not consume the process call stack.
class Dinic {
public:
    explicit Dinic(int vertices);

    // Adds one original directed edge and its zero-capacity residual partner.
    // Parallel, antiparallel, zero-capacity, and self edges are supported.
    void add_edge(int from, int to, Cost capacity);

    // Returns the additional maximum flow from the current residual state.
    // Calling again with the same endpoints without adding edges returns zero.
    // The endpoints must be different.
    Cost max_flow(int source, int sink);

    std::vector<bool> source_side(int source) const;

    // Counts original directed edges, excluding their residual partners.
    std::size_t edge_count() const;

private:
    struct Edge {
        int to;
        std::size_t reverse;
        Cost residual;
    };

    std::vector<std::vector<Edge>> graph_;
    std::vector<int> level_;
    std::vector<std::size_t> current_;
    std::vector<int> queue_;
    std::vector<int> path_vertices_;
    std::vector<std::size_t> path_edges_;
    std::vector<Cost> path_minimum_;
    std::size_t original_edges_ = 0;

    void check_vertex(int vertex) const;
    bool build_levels(int source, int sink);
    Cost augment_path(int source, int sink);
};

} // namespace seam
