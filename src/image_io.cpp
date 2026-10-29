#include "seam.h"
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define STBI_WINDOWS_UTF8
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace seam {
ImageInfo inspect_image(const std::string& path) {
    if (stbi_is_hdr(path.c_str()) || stbi_is_16_bit(path.c_str()))
        throw std::invalid_argument("Only 8-bit images are supported; convert HDR/16-bit inputs first.");
    int w = 0, h = 0, channels = 0;
    if (!stbi_info(path.c_str(), &w, &h, &channels))
        throw std::runtime_error("Cannot inspect image: " + path + " (" +
            (stbi_failure_reason() ? stbi_failure_reason() : "unknown format") + ")");
    checked_cells(h, w);
    return {w, h};
}
Image load_image(const std::string& path) {
    inspect_image(path);
    int w = 0, h = 0, channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> data(
        stbi_load(path.c_str(), &w, &h, &channels, 4), &stbi_image_free);
    if (!data) throw std::runtime_error("Cannot read image: " + path + " (" +
        (stbi_failure_reason() ? stbi_failure_reason() : "unknown format") + ")");
    Image image(h, w);
    // Composite alpha onto white, explicitly and identically for every method.
    for (std::size_t p = 0; p < checked_cells(h, w); ++p) {
        const unsigned alpha = data.get()[4*p+3];
        for (int c = 0; c < 3; ++c)
            image.pixels[3*p+c] = static_cast<std::uint8_t>(
                (data.get()[4*p+c]*alpha + 255*(255-alpha) + 127)/255);
    }
    return image;
}
void save_png(const Image& image, const std::string& path) {
    validate_image(image);
    if (image.width > std::numeric_limits<int>::max()/3)
        throw std::overflow_error("PNG row stride is too large.");
    const auto parent = std::filesystem::u8path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    if (!stbi_write_png(path.c_str(), image.width, image.height, 3,
                        image.pixels.data(), image.width*3))
        throw std::runtime_error("Cannot write PNG: " + path);
}
} // namespace seam
