#include "seamcarving/seam.hpp"

namespace seam {
Image transpose(const Image&) {
    // TODO: transpose rows/columns while preserving complete RGB pixels.
    throw NotImplemented("image transposition");
}
Image remove_vertical(const Image&, const Seam&) {
    // TODO: validate the seam, remove exactly one pixel per row, keep order.
    throw NotImplemented("vertical seam removal");
}
Image resize(const Image&, int, int, Method) {
    // TODO: validate targets; recompute energy after every seam removal.
    // Add horizontal support and document a consistent two-axis removal order.
    throw NotImplemented("repeated vertical/horizontal resizing");
}
} // namespace seam
