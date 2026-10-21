#include "seamcarving/seam.hpp"

namespace seam {
Seam find_graph_cut(const EnergyMap&) {
    // TODO: construct a seam graph, solve max-flow and decode residual min-cut.
    throw NotImplemented("graph-cut seam finder");
}
} // namespace seam
