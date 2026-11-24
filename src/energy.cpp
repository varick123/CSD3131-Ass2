#include "seam.h"

#include <algorithm>
#include <cstdlib>

namespace seam {

EnergyMap compute_energy(const Image& image) {
    validate_image(image);
    const std::size_t count = checked_cells(image.height, image.width);
    const std::size_t width = static_cast<std::size_t>(image.width);
    std::vector<int> grayscale(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t pixel = index * 3;
        grayscale[index] = (299 * static_cast<int>(image.pixels[pixel]) +
                            587 * static_cast<int>(image.pixels[pixel + 1]) +
                            114 * static_cast<int>(image.pixels[pixel + 2]) + 500) / 1000;
    }

    EnergyMap energy(image.height, image.width);
    for (int row = 0; row < image.height; ++row) {
        const std::size_t above = static_cast<std::size_t>(std::max(0, row - 1)) * width;
        const std::size_t center = static_cast<std::size_t>(row) * width;
        const std::size_t below = static_cast<std::size_t>(std::min(image.height - 1, row + 1)) * width;
        for (int col = 0; col < image.width; ++col) {
            const std::size_t left = static_cast<std::size_t>(std::max(0, col - 1));
            const std::size_t middle = static_cast<std::size_t>(col);
            const std::size_t right = static_cast<std::size_t>(std::min(image.width - 1, col + 1));

            // Fixed 3x3 Sobel kernels do constant work for each output pixel.
            // Signed arithmetic preserves negative gradients before taking absolute values.
            const int gx = -grayscale[above + left] + grayscale[above + right]
                           - 2 * grayscale[center + left] + 2 * grayscale[center + right]
                           - grayscale[below + left] + grayscale[below + right];
            const int gy = -grayscale[above + left] - 2 * grayscale[above + middle]
                           - grayscale[above + right] + grayscale[below + left]
                           + 2 * grayscale[below + middle] + grayscale[below + right];
            energy.values[center + middle] = static_cast<Cost>(std::abs(gx) + std::abs(gy));
        }
    }
    validate_energy(energy);
    return energy;
}

} // namespace seam
