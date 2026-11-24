#include "flow.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace seam {

Dinic::Dinic(int vertices) {
    if (vertices <= 0) {
        throw std::invalid_argument("A flow graph needs a positive vertex count");
    }
    const auto count = static_cast<std::size_t>(vertices);
    graph_.resize(count);
    level_.resize(count);
    current_.resize(count);
    queue_.reserve(count);
    path_vertices_.reserve(count);
    path_edges_.reserve(count);
    path_minimum_.reserve(count);
}

void Dinic::check_vertex(int vertex) const {
    if (vertex < 0 || static_cast<std::size_t>(vertex) >= graph_.size()) {
        throw std::out_of_range("Flow graph vertex is out of range");
    }
}

void Dinic::add_edge(int from, int to, Cost capacity) {
    check_vertex(from);
    check_vertex(to);
    if (capacity < 0) {
        throw std::invalid_argument("Flow capacities must be nonnegative");
    }
    if (original_edges_ == std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("Flow graph edge count overflow");
    }
    auto& outgoing = graph_[static_cast<std::size_t>(from)];
    auto& incoming = graph_[static_cast<std::size_t>(to)];
    const auto forward_index = outgoing.size();
    const auto reverse_index = incoming.size() + (from == to ? 1U : 0U);
    outgoing.push_back(Edge{to, reverse_index, capacity});
    incoming.push_back(Edge{from, forward_index, 0});
    ++original_edges_;
}

bool Dinic::build_levels(int source, int sink) {
    std::fill(level_.begin(), level_.end(), -1);
    queue_.clear();
    queue_.push_back(source);
    level_[static_cast<std::size_t>(source)] = 0;
    for (std::size_t head = 0; head < queue_.size(); ++head) {
        const int vertex = queue_[head];
        for (const auto& edge : graph_[static_cast<std::size_t>(vertex)]) {
            if (edge.residual > 0 && level_[static_cast<std::size_t>(edge.to)] < 0) {
                level_[static_cast<std::size_t>(edge.to)] =
                    level_[static_cast<std::size_t>(vertex)] + 1;
                queue_.push_back(edge.to);
            }
        }
    }
    return level_[static_cast<std::size_t>(sink)] >= 0;
}

Cost Dinic::augment_path(int source, int sink) {
    path_vertices_.clear();
    path_edges_.clear();
    path_minimum_.clear();
    path_vertices_.push_back(source);
    path_minimum_.push_back(std::numeric_limits<Cost>::max());

    while (!path_vertices_.empty()) {
        const int vertex = path_vertices_.back();
        if (vertex == sink) {
            const Cost amount = path_minimum_.back();
            for (std::size_t depth = 0; depth < path_edges_.size(); ++depth) {
                auto& edge = graph_[static_cast<std::size_t>(path_vertices_[depth])]
                                   [path_edges_[depth]];
                auto& reverse = graph_[static_cast<std::size_t>(edge.to)][edge.reverse];
                // Each pair's two residuals sum to its original capacity.
                // Therefore this addition cannot exceed the validated capacity.
                if (reverse.residual > std::numeric_limits<Cost>::max() - amount) {
                    throw std::overflow_error("Reverse residual capacity overflow");
                }
                edge.residual -= amount;
                reverse.residual += amount;
            }
            return amount;
        }

        auto& candidate = current_[static_cast<std::size_t>(vertex)];
        const auto& outgoing = graph_[static_cast<std::size_t>(vertex)];
        while (candidate < outgoing.size()) {
            const auto& edge = outgoing[candidate];
            if (edge.residual > 0 && level_[static_cast<std::size_t>(edge.to)] ==
                                         level_[static_cast<std::size_t>(vertex)] + 1) {
                break;
            }
            ++candidate;
        }

        if (candidate == outgoing.size()) {
            // No level-respecting route to the sink remains from this vertex.
            // Removing it from this phase avoids revisiting dead branches.
            level_[static_cast<std::size_t>(vertex)] = -1;
            path_vertices_.pop_back();
            path_minimum_.pop_back();
            if (!path_edges_.empty()) {
                path_edges_.pop_back();
                ++current_[static_cast<std::size_t>(path_vertices_.back())];
            }
            continue;
        }

        const auto& edge = outgoing[candidate];
        path_edges_.push_back(candidate);
        path_vertices_.push_back(edge.to);
        path_minimum_.push_back(std::min(path_minimum_.back(), edge.residual));
    }
    return 0;
}

Cost Dinic::max_flow(int source, int sink) {
    check_vertex(source);
    check_vertex(sink);
    if (source == sink) {
        throw std::invalid_argument("Flow source and sink must be different");
    }

    Cost total = 0;
    while (build_levels(source, sink)) {
        std::fill(current_.begin(), current_.end(), 0);
        while (const Cost amount = augment_path(source, sink)) {
            if (total > std::numeric_limits<Cost>::max() - amount) {
                throw std::overflow_error("Maximum flow exceeds signed 64-bit range");
            }
            total += amount;
        }
    }
    return total;
}

std::vector<bool> Dinic::source_side(int source) const {
    check_vertex(source);
    std::vector<bool> reached(graph_.size(), false);
    std::vector<int> pending;
    pending.reserve(graph_.size());
    pending.push_back(source);
    reached[static_cast<std::size_t>(source)] = true;
    for (std::size_t head = 0; head < pending.size(); ++head) {
        for (const auto& edge : graph_[static_cast<std::size_t>(pending[head])]) {
            if (edge.residual > 0 && !reached[static_cast<std::size_t>(edge.to)]) {
                reached[static_cast<std::size_t>(edge.to)] = true;
                pending.push_back(edge.to);
            }
        }
    }
    return reached;
}

std::size_t Dinic::edge_count() const {
    return original_edges_;
}

} // namespace seam
