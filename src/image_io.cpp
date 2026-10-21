#include "seamcarving/seam.hpp"

namespace seam {
Image load_image(const std::string&) {
    // TODO: choose an image codec/library and convert to the agreed RGB format.
    throw NotImplemented("image loading");
}
void save_image(const Image&, const std::string&) {
    // TODO: save a result and report failures clearly.
    throw NotImplemented("image saving");
}
} // namespace seam
