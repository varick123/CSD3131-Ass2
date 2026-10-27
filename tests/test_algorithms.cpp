#include "seam.h"
#include "flow.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::size_t assertions = 0;
std::size_t seam_maps = 0;
std::size_t flow_graphs = 0;
std::size_t energy_images = 0;

void require(bool condition, const std::string& message) {
    ++assertions;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void require_throws(Function function, const std::string& message) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, message);
}

seam::EnergyMap energy(int height, int width,
                       const std::vector<seam::Cost>& values) {
    seam::EnergyMap result(height, width);
    require(result.values.size() == values.size(), "test energy shape mismatch");
    result.values = values;
    return result;
}

struct Oracle {
    seam::Cost minimum;
    std::vector<seam::Cost> prefix_minima;
};

// Independently enumerate every legal prefix. This deliberately does not use a
// recurrence, production seam validation, or any production path-finding code.
Oracle enumerate_seams(const seam::EnergyMap& map) {
    const seam::Cost largest = std::numeric_limits<seam::Cost>::max();
    Oracle result{largest, std::vector<seam::Cost>(map.values.size(), largest)};
    std::function<void(int, int, seam::Cost)> visit;
    visit = [&](int row, int column, seam::Cost prefix) {
        const auto index = static_cast<std::size_t>(row) *
                           static_cast<std::size_t>(map.width) +
                           static_cast<std::size_t>(column);
        const seam::Cost total = prefix + map.values[index];
        result.prefix_minima[index] = std::min(result.prefix_minima[index], total);
        if (row + 1 == map.height) {
            result.minimum = std::min(result.minimum, total);
            return;
        }
        for (int next = std::max(0, column - 1);
             next <= std::min(map.width - 1, column + 1); ++next) {
            visit(row + 1, next, total);
        }
    };
    for (int column = 0; column < map.width; ++column) {
        visit(0, column, 0);
    }
    return result;
}

void check_path(const seam::EnergyMap& map, const seam::Seam& path,
                const std::string& label) {
    require(path.columns.size() == static_cast<std::size_t>(map.height),
            label + ": wrong seam length");
    seam::Cost sum = 0;
    for (int row = 0; row < map.height; ++row) {
        const int column = path.columns[static_cast<std::size_t>(row)];
        require(column >= 0 && column < map.width, label + ": column out of bounds");
        if (row > 0) {
            const int previous = path.columns[static_cast<std::size_t>(row - 1)];
            require(column >= previous - 1 && column <= previous + 1,
                    label + ": disconnected seam");
        }
        sum += map.values[static_cast<std::size_t>(row) *
                          static_cast<std::size_t>(map.width) +
                          static_cast<std::size_t>(column)];
    }
    require(path.cost == sum, label + ": incorrect stored seam cost");
    seam::validate_seam(map, path);
}

void check_methods(const seam::EnergyMap& map, const std::string& context) {
    const Oracle oracle = enumerate_seams(map);
    std::vector<seam::Cost> cumulative;
    const seam::Seam dp = seam::find_dp(map, &cumulative);
    const seam::Seam graph = seam::find_graph_cut(map);
    const seam::Seam greedy = seam::find_greedy(map);
    check_path(map, dp, context + " DP");
    check_path(map, graph, context + " graph");
    check_path(map, greedy, context + " greedy");
    require(dp.cost == oracle.minimum, context + ": DP differs from exhaustive optimum");
    require(graph.cost == oracle.minimum,
            context + ": graph differs from exhaustive optimum");
    require(greedy.cost >= oracle.minimum,
            context + ": greedy cost is below exhaustive optimum");
    require(cumulative == oracle.prefix_minima,
            context + ": cumulative DP table differs from exhaustive prefix costs");
    ++seam_maps;
}

void test_seam_oracles() {
    // All binary energy assignments on all 1..3 by 1..3 maps: 682 maps.
    for (int height = 1; height <= 3; ++height) {
        for (int width = 1; width <= 3; ++width) {
            const unsigned cells = static_cast<unsigned>(height * width);
            for (unsigned mask = 0; mask < (1U << cells); ++mask) {
                seam::EnergyMap map(height, width);
                for (unsigned i = 0; i < cells; ++i) {
                    map.values[i] = static_cast<seam::Cost>((mask >> i) & 1U);
                }
                check_methods(map, "binary " + std::to_string(height) + "x" +
                                       std::to_string(width) + " mask=" +
                                       std::to_string(mask));
            }
        }
    }

    constexpr std::uint32_t seed = 0x5EA2026U;
    std::mt19937 generator(seed);
    std::uniform_int_distribution<int> distribution(0, 9);
    for (int height = 1; height <= 5; ++height) {
        for (int width = 1; width <= 5; ++width) {
            for (int sample = 0; sample < 40; ++sample) {
                seam::EnergyMap map(height, width);
                for (auto& value : map.values) {
                    value = distribution(generator);
                }
                check_methods(map, "random seed=" + std::to_string(seed) + " " +
                                       std::to_string(height) + "x" +
                                       std::to_string(width) + " sample=" +
                                       std::to_string(sample));
            }
        }
    }

    const auto counterexample = energy(3, 3, {1, 2, 100, 1, 100, 2, 100, 100, 1});
    check_methods(counterexample, "greedy counterexample");
    require(seam::find_greedy(counterexample).columns == std::vector<int>({0, 0, 0}),
            "greedy counterexample path");
    require(seam::find_greedy(counterexample).cost == 102, "greedy counterexample cost");
    require(seam::find_dp(counterexample).cost == 5, "DP counterexample cost");
    require(seam::find_graph_cut(counterexample).cost == 5, "graph counterexample cost");

    const auto right_edge = energy(4, 3, {9, 9, 0, 9, 9, 0, 9, 9, 0, 9, 9, 0});
    const auto left_edge = energy(4, 3, {0, 9, 9, 0, 9, 9, 0, 9, 9, 0, 9, 9});
    check_methods(right_edge, "right-edge optimum");
    check_methods(left_edge, "left-edge optimum");
    require(seam::find_graph_cut(right_edge).columns == std::vector<int>({2, 2, 2, 2}),
            "graph must permit removing actual last column");
    require(seam::find_graph_cut(left_edge).columns == std::vector<int>({0, 0, 0, 0}),
            "graph must permit removing actual first column");

    const auto equal = energy(3, 4, std::vector<seam::Cost>(12, 7));
    check_methods(equal, "all equal");
    require(seam::find_dp(equal).columns == std::vector<int>({0, 0, 0}),
            "DP leftmost endpoint and predecessor tie rule");
    require(seam::find_greedy(equal).columns == std::vector<int>({0, 0, 0}),
            "greedy leftmost tie rule");
    const auto predecessor_ties = energy(3, 4, {9, 9, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0});
    check_methods(predecessor_ties, "predecessor ties");
    require(seam::find_dp(predecessor_ties).columns == std::vector<int>({2, 1, 0}),
            "DP increasing-column predecessor tie order");
    require(seam::find_greedy(predecessor_ties).columns == std::vector<int>({2, 1, 0}),
            "greedy legal-neighbor tie order");

    std::vector<seam::Cost> cumulative{999};
    const auto hand = energy(2, 3, {1, 2, 3, 3, 2, 1});
    const auto path = seam::find_dp(hand, &cumulative);
    require(cumulative == std::vector<seam::Cost>({1, 2, 3, 4, 3, 3}),
            "hand cumulative table and replacement of output contents");
    require(path.columns == std::vector<int>({0, 1}) && path.cost == 3,
            "hand traceback");
    require(seam::find_dp(hand).cost == path.cost, "optional cumulative output");
    check_methods(hand, "hand table");
}

struct DirectedEdge {
    int from;
    int to;
    seam::Cost capacity;
};

seam::Cost enumerate_cuts(int vertices, const std::vector<DirectedEdge>& edges) {
    seam::Cost best = std::numeric_limits<seam::Cost>::max();
    const unsigned possibilities = 1U << static_cast<unsigned>(vertices - 2);
    for (unsigned mask = 0; mask < possibilities; ++mask) {
        std::vector<bool> source(static_cast<std::size_t>(vertices), false);
        source[0] = true;
        for (int vertex = 1; vertex < vertices - 1; ++vertex) {
            source[static_cast<std::size_t>(vertex)] =
                ((mask >> static_cast<unsigned>(vertex - 1)) & 1U) != 0;
        }
        seam::Cost capacity = 0;
        for (const auto& edge : edges) {
            if (source[static_cast<std::size_t>(edge.from)] &&
                !source[static_cast<std::size_t>(edge.to)]) {
                capacity += edge.capacity;
            }
        }
        best = std::min(best, capacity);
    }
    return best;
}

void check_flow_graph(int vertices, const std::vector<DirectedEdge>& edges,
                      const std::string& label) {
    seam::Dinic graph(vertices);
    for (const auto& edge : edges) {
        graph.add_edge(edge.from, edge.to, edge.capacity);
    }
    const seam::Cost flow = graph.max_flow(0, vertices - 1);
    require(flow == enumerate_cuts(vertices, edges), label + ": max-flow/min-cut mismatch");
    const auto source = graph.source_side(0);
    require(source.size() == static_cast<std::size_t>(vertices), label + ": cut size");
    require(source[0] && !source[static_cast<std::size_t>(vertices - 1)],
            label + ": invalid source/sink partition");
    seam::Cost cut = 0;
    for (const auto& edge : edges) {
        if (source[static_cast<std::size_t>(edge.from)] &&
            !source[static_cast<std::size_t>(edge.to)]) {
            cut += edge.capacity;
        }
    }
    require(cut == flow, label + ": residual source-side cut has incorrect capacity");
    if (!edges.empty()) {
        require(graph.edge_count() == edges.size(), label + ": original-edge count mismatch");
    }
    ++flow_graphs;
}

void test_flow() {
    // With insertion order below, the first blocking flow uses s-a-c-t.
    // Reaching value 2 subsequently requires the reverse residual c-a edge.
    check_flow_graph(6, {{0, 1, 1}, {0, 2, 1}, {1, 3, 1}, {1, 4, 1},
                         {2, 3, 1}, {3, 5, 1}, {4, 5, 1}},
                     "reverse-residual cancellation");
    check_flow_graph(4, {{0, 1, 3}, {0, 1, 4}, {1, 0, 2}, {1, 2, 6},
                         {2, 1, 5}, {2, 3, 6}, {1, 3, 1}},
                     "parallel and opposite original edges");
    check_flow_graph(3, {{0, 1, 0}, {1, 2, 0}}, "zero-capacity edges");
    check_flow_graph(4, {}, "disconnected empty graph");
    check_flow_graph(3, {{0, 0, 8}, {0, 1, 3}, {1, 1, 9}, {1, 2, 2}, {2, 2, 7}},
                     "self-edge residual pairing");

    constexpr std::uint32_t seed = 0xC0172026U;
    std::mt19937 generator(seed);
    for (int vertices = 2; vertices <= 6; ++vertices) {
        for (int sample = 0; sample < 40; ++sample) {
            std::vector<DirectedEdge> edges;
            for (int from = 0; from < vertices; ++from) {
                for (int to = 0; to < vertices; ++to) {
                    if (from == to || generator() % 3U == 0U) {
                        continue;
                    }
                    edges.push_back({from, to, static_cast<seam::Cost>(generator() % 6U)});
                    if (generator() % 7U == 0U) {
                        edges.push_back({from, to,
                                         static_cast<seam::Cost>(generator() % 4U)});
                    }
                }
            }
            check_flow_graph(vertices, edges, "flow seed=" + std::to_string(seed) +
                                                  " vertices=" + std::to_string(vertices) +
                                                  " sample=" + std::to_string(sample));
        }
    }
}

seam::Image gray_image(int height, int width, const std::vector<int>& gray) {
    seam::Image image(height, width);
    require(gray.size() == static_cast<std::size_t>(height * width), "test gray shape");
    for (std::size_t i = 0; i < gray.size(); ++i) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            image.pixels[3 * i + channel] = static_cast<std::uint8_t>(gray[i]);
        }
    }
    return image;
}

std::vector<seam::Cost> reference_energy(const seam::Image& image) {
    std::vector<int> gray(static_cast<std::size_t>(image.height * image.width));
    for (std::size_t i = 0; i < gray.size(); ++i) {
        gray[i] = (299 * image.pixels[3 * i] + 587 * image.pixels[3 * i + 1] +
                   114 * image.pixels[3 * i + 2] + 500) / 1000;
    }
    constexpr int horizontal[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    constexpr int vertical[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
    std::vector<seam::Cost> result(gray.size());
    for (int row = 0; row < image.height; ++row) {
        for (int column = 0; column < image.width; ++column) {
            int gx = 0;
            int gy = 0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    const int rr = std::max(0, std::min(image.height - 1, row + dr));
                    const int cc = std::max(0, std::min(image.width - 1, column + dc));
                    const int intensity = gray[static_cast<std::size_t>(rr * image.width + cc)];
                    gx += intensity * horizontal[dr + 1][dc + 1];
                    gy += intensity * vertical[dr + 1][dc + 1];
                }
            }
            result[static_cast<std::size_t>(row * image.width + column)] =
                static_cast<seam::Cost>(gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy);
        }
    }
    return result;
}

void check_energy(const seam::Image& image, const std::string& label,
                  const std::vector<seam::Cost>* expected = nullptr) {
    const auto actual = seam::compute_energy(image);
    require(actual.height == image.height && actual.width == image.width,
            label + ": energy dimensions");
    require(actual.values == reference_energy(image), label + ": convolution reference");
    if (expected != nullptr) {
        require(actual.values == *expected, label + ": hand-calculated Sobel result");
    }
    ++energy_images;
}

void test_energy() {
    const std::vector<seam::Cost> zero9(9, 0);
    check_energy(gray_image(3, 3, std::vector<int>(9, 173)), "constant image", &zero9);
    const std::vector<seam::Cost> zero1{0};
    check_energy(gray_image(1, 1, {255}), "single pixel", &zero1);
    const std::vector<seam::Cost> descending{40, 80, 40};
    check_energy(gray_image(1, 3, {30, 20, 10}), "negative horizontal gradient", &descending);
    check_energy(gray_image(3, 1, {30, 20, 10}), "negative vertical gradient", &descending);
    const std::vector<seam::Cost> border{120, 120, 120, 120};
    check_energy(gray_image(2, 2, {0, 10, 20, 30}), "replicated 2x2 borders", &border);
    const std::vector<seam::Cost> impulse{510, 510, 510, 510, 0, 510, 510, 510, 510};
    check_energy(gray_image(3, 3, {0, 0, 0, 0, 255, 0, 0, 0, 0}), "center impulse", &impulse);
    seam::Image rgb(1, 3);
    rgb.pixels = {255, 0, 0, 0, 255, 0, 0, 0, 255};
    const std::vector<seam::Cost> rgb_expected{296, 188, 484};
    check_energy(rgb, "RGB grayscale rounding and channel order", &rgb_expected);

    std::mt19937 generator(0x50BE2026U);
    for (int height = 1; height <= 5; ++height) {
        for (int width = 1; width <= 5; ++width) {
            for (int sample = 0; sample < 8; ++sample) {
                seam::Image image(height, width);
                for (auto& channel : image.pixels) {
                    channel = static_cast<std::uint8_t>(generator() % 256U);
                }
                check_energy(image, "random RGB " + std::to_string(height) + "x" +
                                        std::to_string(width) + " sample=" +
                                        std::to_string(sample));
                const auto original = seam::compute_energy(image);
                const auto transposed = seam::compute_energy(seam::transpose(image));
                require(seam::transpose(original).values == transposed.values,
                        "Sobel transpose equivalence");
            }
        }
    }
}

void test_transpose_and_removal() {
    seam::Image image(3, 4);
    for (int row = 0; row < image.height; ++row) {
        for (int column = 0; column < image.width; ++column) {
            for (int channel = 0; channel < 3; ++channel) {
                image.at(row, column, channel) =
                    static_cast<std::uint8_t>(48 * row + 9 * column + channel);
            }
        }
    }
    const auto original = image.pixels;
    const auto transposed = seam::transpose(image);
    require(transposed.height == 4 && transposed.width == 3, "image transpose dimensions");
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
            for (int channel = 0; channel < 3; ++channel) {
                require(transposed.at(column, row, channel) == image.at(row, column, channel),
                        "image transpose coordinates");
            }
        }
    }
    require(seam::transpose(transposed).pixels == original, "image transpose involution");

    const auto map = energy(2, 3, {1, 2, 3, 4, 5, 6});
    const auto map_t = seam::transpose(map);
    require(map_t.height == 3 && map_t.width == 2, "energy transpose dimensions");
    require(map_t.values == std::vector<seam::Cost>({1, 4, 2, 5, 3, 6}),
            "energy transpose coordinates");
    require(seam::transpose(map_t).values == map.values, "energy transpose involution");
    check_methods(map_t, "horizontal transposed map");

    const std::vector<int> columns{3, 2, 1};
    const auto removed = seam::remove_vertical(image, columns);
    require(removed.height == 3 && removed.width == 3, "vertical removal dimensions");
    for (int row = 0; row < image.height; ++row) {
        int output = 0;
        for (int column = 0; column < image.width; ++column) {
            if (column == columns[static_cast<std::size_t>(row)]) {
                continue;
            }
            for (int channel = 0; channel < 3; ++channel) {
                require(removed.at(row, output, channel) == image.at(row, column, channel),
                        "removal preserves survivor order and channels");
            }
            ++output;
        }
    }
    require(image.pixels == original, "remove must not mutate input");

    const std::vector<int> horizontal{0, 1, 2, 1};
    const auto horizontal_removed =
        seam::transpose(seam::remove_vertical(transposed, horizontal));
    require(horizontal_removed.height == 2 && horizontal_removed.width == 4,
            "horizontal removal dimensions");
    for (int column = 0; column < image.width; ++column) {
        int output = 0;
        for (int row = 0; row < image.height; ++row) {
            if (row == horizontal[static_cast<std::size_t>(column)]) {
                continue;
            }
            for (int channel = 0; channel < 3; ++channel) {
                require(horizontal_removed.at(output, column, channel) ==
                            image.at(row, column, channel),
                        "horizontal removal preserves vertical survivor order");
            }
            ++output;
        }
    }

    // Exercise recomputation and both orientations without duplicating controller logic.
    for (int method = 0; method < 3; ++method) {
        seam::Image current = image;
        auto shrink_width = [&]() {
            while (current.width > 1) {
                const auto current_energy = seam::compute_energy(current);
                const auto path = method == 0 ? seam::find_dp(current_energy) :
                                  method == 1 ? seam::find_greedy(current_energy) :
                                                seam::find_graph_cut(current_energy);
                check_path(current_energy, path, "repeated removal");
                current = seam::remove_vertical(current, path.columns);
            }
        };
        shrink_width();
        current = seam::transpose(current);
        shrink_width();
        current = seam::transpose(current);
        require(current.height == 1 && current.width == 1, "repeated both-axis shrink-to-one");
    }
}

void test_invalid_and_overflow() {
    require_throws([] { seam::checked_cells(0, 1); }, "zero height rejected");
    require_throws([] { seam::checked_cells(1, 0); }, "zero width rejected");
    require_throws([] { seam::checked_cells(-1, 2); }, "negative height rejected");
    require_throws([] { seam::checked_cells(2, -1); }, "negative width rejected");
    require_throws([] { seam::Image bad(0, 2); }, "invalid Image constructor");
    require_throws([] { seam::EnergyMap bad(2, -1); }, "invalid EnergyMap constructor");
    require(seam::checked_cells(2, 3) == 6, "checked cell count");

    seam::Image malformed_image;
    require_throws([&] { seam::validate_image(malformed_image); }, "default image rejected");
    malformed_image.height = 2;
    malformed_image.width = 2;
    malformed_image.pixels.resize(11);
    require_throws([&] { seam::validate_image(malformed_image); }, "short RGB storage rejected");
    require_throws([&] { seam::compute_energy(malformed_image); }, "energy validates image storage");
    require_throws([&] { seam::transpose(malformed_image); }, "transpose validates image storage");
    require_throws([&] { seam::remove_vertical(malformed_image, {0, 0}); },
                   "removal validates image storage");
    malformed_image.pixels.resize(13);
    require_throws([&] { seam::validate_image(malformed_image); }, "extra RGB storage rejected");
    malformed_image.height = -2;
    require_throws([&] { seam::validate_image(malformed_image); }, "raw negative image dimension");

    seam::EnergyMap malformed;
    require_throws([&] { seam::validate_energy(malformed); }, "default energy rejected");
    malformed.height = 2;
    malformed.width = 2;
    malformed.values = {1, 2, 3};
    require_throws([&] { seam::validate_energy(malformed); }, "short energy storage rejected");
    require_throws([&] { seam::find_dp(malformed); }, "DP validates energy storage");
    require_throws([&] { seam::find_greedy(malformed); }, "greedy validates energy storage");
    require_throws([&] { seam::find_graph_cut(malformed); }, "graph validates energy storage");
    require_throws([&] { seam::transpose(malformed); }, "energy transpose validates storage");
    malformed.values = {1, 2, 3, 4, 5};
    require_throws([&] { seam::validate_energy(malformed); }, "extra energy storage rejected");
    malformed.width = 0;
    require_throws([&] { seam::validate_energy(malformed); }, "raw zero energy dimension");

    const auto negative = energy(1, 2, {-1, 2});
    require_throws([&] { seam::validate_energy(negative); }, "negative energy rejected");
    require_throws([&] { seam::find_dp(negative); }, "DP negative energy rejected");
    require_throws([&] { seam::find_greedy(negative); }, "greedy negative energy rejected");
    require_throws([&] { seam::find_graph_cut(negative); }, "graph negative energy rejected");

    const seam::Cost largest = std::numeric_limits<seam::Cost>::max();
    require(seam::checked_add(largest, 0) == largest, "exact maximum addition allowed");
    require(seam::checked_add(7, 8) == 15, "ordinary checked addition");
    require_throws([&] { seam::checked_add(largest, 1); }, "addition overflow rejected");
    require_throws([] { seam::checked_add(-1, 0); }, "negative first addition operand rejected");
    require_throws([] { seam::checked_add(0, -1); }, "negative second addition operand rejected");
    const auto maximum = energy(1, 1, {largest});
    require(seam::validate_energy(maximum) == largest, "exact maximum map total allowed");
    require(seam::find_dp(maximum).cost == largest, "DP exact maximum energy");
    require(seam::find_greedy(maximum).cost == largest, "greedy exact maximum energy");
    require_throws([&] { seam::find_graph_cut(maximum); }, "graph INF overflow rejected");
    const auto overflow = energy(1, 2, {largest, 1});
    require_throws([&] { seam::validate_energy(overflow); }, "map total overflow rejected");
    require_throws([&] { seam::find_dp(overflow); }, "DP map total overflow rejected");
    require_throws([&] { seam::find_greedy(overflow); }, "greedy map total overflow rejected");
    require_throws([&] { seam::find_graph_cut(overflow); }, "graph map total overflow rejected");

    auto alias = energy(2, 2, {1, 2, 3, 4});
    const auto alias_before = alias.values;
    require_throws([&] { seam::find_dp(alias, &alias.values); }, "DP output alias rejected");
    require(alias.values == alias_before, "DP output alias rejection preserves input");

    require_throws([] { seam::Dinic invalid(0); }, "zero-vertex flow graph rejected");
    require_throws([] { seam::Dinic invalid(-1); }, "negative-vertex flow graph rejected");
    seam::Dinic flow(3);
    require_throws([&] { flow.add_edge(-1, 1, 1); }, "negative flow vertex rejected");
    require_throws([&] { flow.add_edge(0, 3, 1); }, "out-of-range flow vertex rejected");
    require_throws([&] { flow.add_edge(0, 1, -1); }, "negative flow capacity rejected");
    require_throws([&] { flow.max_flow(0, 0); }, "equal source/sink rejected");
    require_throws([&] { flow.max_flow(0, 3); }, "out-of-range flow sink rejected");
    require_throws([&] { flow.source_side(-1); }, "negative cut source rejected");
    flow.add_edge(0, 1, 2);
    flow.add_edge(1, 2, 2);
    require(flow.max_flow(0, 2) == 2, "first residual max-flow call");
    require(flow.max_flow(0, 2) == 0, "repeated max-flow returns only additional flow");
    flow.add_edge(0, 2, 3);
    require(flow.max_flow(0, 2) == 3, "additional edges preserve existing residual state");

    const auto map = energy(3, 4, std::vector<seam::Cost>(12, 1));
    const auto image = gray_image(3, 4, std::vector<int>(12, 0));
    seam::Seam good;
    good.columns = {0, 1, 2};
    good.cost = 3;
    seam::validate_seam(map, good);
    for (const auto& bad_columns : std::vector<std::vector<int>>{
             {}, {0, 1}, {0, 1, 2, 3}, {-1, 0, 0}, {4, 3, 3}, {0, 3, 3}}) {
        seam::Seam bad;
        bad.columns = bad_columns;
        bad.cost = 3;
        require_throws([&] { seam::validate_seam(map, bad); }, "invalid seam rejected");
        require_throws([&] { seam::remove_vertical(image, bad.columns); },
                       "removal rejects invalid seam");
    }
    good.cost = 4;
    require_throws([&] { seam::validate_seam(map, good); }, "stored cost mismatch rejected");
    const auto narrow = gray_image(2, 1, {1, 2});
    require_throws([&] { seam::remove_vertical(narrow, {0, 0}); },
                   "cannot remove width-one image");
}

} // namespace

int main() {
    try {
        test_seam_oracles();
        test_flow();
        test_energy();
        test_transpose_and_removal();
        test_invalid_and_overflow();
        std::cout << "PASS: " << assertions << " assertions; " << seam_maps
                  << " seam maps checked against exhaustive enumeration; " << flow_graphs
                  << " flow graphs checked against exhaustive cuts; " << energy_images
                  << " Sobel images checked against independent convolution.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << assertions << " assertions: " << error.what() << '\n';
        return 1;
    }
}
