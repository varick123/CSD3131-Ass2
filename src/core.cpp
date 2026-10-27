#include "seam.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace seam {
namespace {

std::size_t image_storage_size(int height, int width) {
    const std::size_t count = checked_cells(height, width);
    const std::size_t limit = std::vector<std::uint8_t>().max_size();
    if (count > limit / 3) {
        throw std::length_error("Image dimensions exceed RGB storage capacity");
    }
    return count * 3;
}

std::size_t energy_storage_size(int height, int width) {
    const std::size_t count = checked_cells(height, width);
    if (count > std::vector<Cost>().max_size()) {
        throw std::length_error("Energy dimensions exceed storage capacity");
    }
    return count;
}

void validate_energy_storage(const EnergyMap& energy) {
    if (energy.values.size() != energy_storage_size(energy.height, energy.width)) {
        throw std::invalid_argument("Energy storage does not match its dimensions");
    }
}

void validate_columns(int height, int width, const std::vector<int>& columns) {
    if (columns.size() != static_cast<std::size_t>(height)) {
        throw std::invalid_argument("A vertical seam must contain one column per row");
    }
    for (int row = 0; row < height; ++row) {
        const int col = columns[static_cast<std::size_t>(row)];
        if (col < 0 || col >= width) {
            throw std::invalid_argument("Seam column is outside the image");
        }
        if (row > 0) {
            const int difference = col - columns[static_cast<std::size_t>(row - 1)];
            if (difference < -1 || difference > 1) {
                throw std::invalid_argument("Adjacent seam columns must differ by at most one");
            }
        }
    }
}

std::size_t image_index(const Image& image, int row, int col, int channel) {
    validate_image(image);
    if (row < 0 || row >= image.height || col < 0 || col >= image.width ||
        channel < 0 || channel >= 3) {
        throw std::out_of_range("Image index is outside its dimensions");
    }
    return (static_cast<std::size_t>(row) * static_cast<std::size_t>(image.width) +
            static_cast<std::size_t>(col)) * 3 + static_cast<std::size_t>(channel);
}

std::size_t energy_index(const EnergyMap& energy, int row, int col) {
    validate_energy_storage(energy);
    if (row < 0 || row >= energy.height || col < 0 || col >= energy.width) {
        throw std::out_of_range("Energy index is outside its dimensions");
    }
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(energy.width) +
           static_cast<std::size_t>(col);
}

} // namespace

std::size_t checked_cells(int height, int width) {
    if (height <= 0 || width <= 0) {
        throw std::invalid_argument("Image and energy dimensions must be positive");
    }
    const std::size_t h = static_cast<std::size_t>(height);
    const std::size_t w = static_cast<std::size_t>(width);
    if (h > std::numeric_limits<std::size_t>::max() / w) {
        throw std::overflow_error("Pixel count overflows size_t");
    }
    return h * w;
}

Cost checked_add(Cost a, Cost b) {
    if (a < 0 || b < 0) {
        throw std::invalid_argument("Costs must be nonnegative");
    }
    if (a > std::numeric_limits<Cost>::max() - b) {
        throw std::overflow_error("Cost sum exceeds int64 capacity");
    }
    return a + b;
}

Image::Image(int h, int w)
    : height(h), width(w), pixels(image_storage_size(h, w), 0) {}

std::uint8_t& Image::at(int row, int col, int channel) {
    return pixels[image_index(*this, row, col, channel)];
}

std::uint8_t Image::at(int row, int col, int channel) const {
    return pixels[image_index(*this, row, col, channel)];
}

EnergyMap::EnergyMap(int h, int w)
    : height(h), width(w), values(energy_storage_size(h, w), 0) {}

Cost& EnergyMap::at(int row, int col) {
    return values[energy_index(*this, row, col)];
}

Cost EnergyMap::at(int row, int col) const {
    return values[energy_index(*this, row, col)];
}

void validate_image(const Image& image) {
    if (image.pixels.size() != image_storage_size(image.height, image.width)) {
        throw std::invalid_argument("RGB storage does not match image dimensions");
    }
}

Cost validate_energy(const EnergyMap& energy) {
    validate_energy_storage(energy);
    Cost total = 0;
    for (const Cost value : energy.values) {
        total = checked_add(total, value);
    }
    return total;
}

void validate_seam(const EnergyMap& energy, const Seam& path) {
    validate_energy(energy);
    validate_columns(energy.height, energy.width, path.columns);
    Cost sum = 0;
    const std::size_t width = static_cast<std::size_t>(energy.width);
    for (int row = 0; row < energy.height; ++row) {
        sum = checked_add(sum, energy.values[static_cast<std::size_t>(row) * width +
                         static_cast<std::size_t>(path.columns[static_cast<std::size_t>(row)])]);
    }
    if (path.cost != sum) {
        throw std::invalid_argument("Reported seam cost differs from its energy sum");
    }
}

Image transpose(const Image& image) {
    validate_image(image);
    Image result(image.width, image.height);
    const std::size_t old_width = static_cast<std::size_t>(image.width);
    const std::size_t new_width = static_cast<std::size_t>(result.width);
    for (int row = 0; row < image.height; ++row) {
        for (int col = 0; col < image.width; ++col) {
            const std::size_t source =
                (static_cast<std::size_t>(row) * old_width + static_cast<std::size_t>(col)) * 3;
            const std::size_t target =
                (static_cast<std::size_t>(col) * new_width + static_cast<std::size_t>(row)) * 3;
            std::copy_n(image.pixels.data() + source, 3, result.pixels.data() + target);
        }
    }
    return result;
}

EnergyMap transpose(const EnergyMap& energy) {
    validate_energy(energy);
    EnergyMap result(energy.width, energy.height);
    const std::size_t old_width = static_cast<std::size_t>(energy.width);
    const std::size_t new_width = static_cast<std::size_t>(result.width);
    for (int row = 0; row < energy.height; ++row) {
        for (int col = 0; col < energy.width; ++col) {
            result.values[static_cast<std::size_t>(col) * new_width + static_cast<std::size_t>(row)] =
                energy.values[static_cast<std::size_t>(row) * old_width + static_cast<std::size_t>(col)];
        }
    }
    return result;
}

Image remove_vertical(const Image& image, const std::vector<int>& columns) {
    validate_image(image);
    validate_columns(image.height, image.width, columns);
    if (image.width <= 1) {
        throw std::invalid_argument("Cannot remove a seam from an image of width one");
    }
    Image result(image.height, image.width - 1);
    const std::size_t old_stride = static_cast<std::size_t>(image.width) * 3;
    const std::size_t new_stride = static_cast<std::size_t>(result.width) * 3;
    for (int row = 0; row < image.height; ++row) {
        const std::size_t left = static_cast<std::size_t>(columns[static_cast<std::size_t>(row)]) * 3;
        const std::size_t right = old_stride - left - 3;
        const auto* source = image.pixels.data() + static_cast<std::size_t>(row) * old_stride;
        auto* target = result.pixels.data() + static_cast<std::size_t>(row) * new_stride;
        std::copy_n(source, left, target);
        std::copy_n(source + left + 3, right, target + left);
    }
    return result;
}

} // namespace seam
