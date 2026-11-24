#include "resize.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace seam {
using Clock = std::chrono::steady_clock;
static double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now()-start).count();
}
Method parse_method(const std::string& name) {
    if (name == "dp") return Method::DP;
    if (name == "greedy") return Method::Greedy;
    if (name == "graph") return Method::Graph;
    throw std::invalid_argument("Method must be dp, greedy or graph.");
}
std::string method_name(Method method) {
    if (method == Method::DP) return "dp";
    if (method == Method::Greedy) return "greedy";
    return "graph";
}
Seam find_seam(const EnergyMap& energy, Method method) {
    if (method == Method::DP) return find_dp(energy);
    if (method == Method::Greedy) return find_greedy(energy);
    return find_graph_cut(energy);
}
Image overlay(const Image& image, const std::vector<int>& columns) {
    validate_image(image);
    if (columns.size() != static_cast<std::size_t>(image.height))
        throw std::invalid_argument("Overlay seam has wrong height.");
    Image output = image;
    for (int r=0; r<image.height; ++r) {
        if (columns[r] < 0 || columns[r] >= image.width)
            throw std::invalid_argument("Overlay seam is out of bounds.");
        output.at(r,columns[r],0)=255;
        output.at(r,columns[r],1)=32;
        output.at(r,columns[r],2)=64;
    }
    return output;
}
Image energy_preview(const EnergyMap& energy) {
    validate_energy(energy);
    Image output(energy.height, energy.width);
    const auto max = *std::max_element(energy.values.begin(), energy.values.end());
    for (std::size_t i=0; i<energy.values.size(); ++i) {
        const auto value = static_cast<std::uint8_t>(max ?
            std::llround(255.0*static_cast<double>(energy.values[i])/static_cast<double>(max)) : 0);
        for (int c=0; c<3; ++c) output.pixels[3*i+c]=value;
    }
    return output;
}
std::string json_escape(const std::string& text) {
    std::ostringstream out;
    for (unsigned char c : text) {
        if(c=='"') out<<"\\\"";
        else if(c=='\\') out<<"\\\\";
        else if(c=='\n') out<<"\\n";
        else if(c=='\r') out<<"\\r";
        else if(c=='\t') out<<"\\t";
        else if(c<32) out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;
        else out<<c;
    }
    return out.str();
}
void write_text(const std::string& path, const std::string& text) {
    auto p=std::filesystem::u8path(path);
    if (!p.parent_path().empty()) std::filesystem::create_directories(p.parent_path());
    std::ofstream stream(p, std::ios::binary);
    stream<<text;
    if (!stream) throw std::runtime_error("Cannot write file: "+path);
}
std::string result_json(const ResizeResult& result, Method method, int ow, int oh, bool hf) {
    std::ostringstream out;
    out<<std::fixed<<std::setprecision(6);
    out<<"{\"method\":\""<<method_name(method)<<"\",\"original_width\":"<<ow
       <<",\"original_height\":"<<oh<<",\"width\":"<<result.image.width
       <<",\"height\":"<<result.image.height<<",\"order\":\""<<(hf?"horizontal-first":"vertical-first")
       <<"\",\"energy_ms\":"<<result.energy_ms<<",\"search_ms\":"<<result.search_ms
       <<",\"remove_ms\":"<<result.remove_ms<<",\"total_ms\":"<<result.total_ms()<<",\"steps\":[";
    bool first=true;
    for(const auto& step : result.steps) {
        if(!first) out<<",";
        first=false;
        out<<"{\"index\":"<<step.index<<",\"axis\":\""<<step.axis<<"\",\"width\":"<<step.width
           <<",\"height\":"<<step.height<<",\"cost\":"<<step.seam.cost
           <<",\"energy_ms\":"<<step.energy_ms<<",\"search_ms\":"<<step.search_ms
           <<",\"remove_ms\":"<<step.remove_ms<<",\"graph_build_ms\":"<<step.seam.graph_build_ms
           <<",\"flow_ms\":"<<step.seam.flow_ms<<",\"vertices\":"<<step.seam.graph_vertices
           <<",\"edges\":"<<step.seam.graph_edges<<",\"columns\":[";
        for(std::size_t i=0;i<step.seam.columns.size();++i) { if(i)out<<",";out<<step.seam.columns[i]; }
        out<<"]}";
    }
    out<<"]}";
    return out.str();
}
ResizeResult resize(const Image& original, int width, int height, Method method,
                    bool horizontal_first, const std::string& artifacts) {
    validate_image(original);
    if (width<1 || height<1 || width>original.width || height>original.height)
        throw std::invalid_argument("Targets must be between 1 and the original image dimensions; enlargement is unsupported.");
    ResizeResult result;
    result.image=original;
    Image all_seams;
    std::vector<std::pair<int,int>> mapping;
    if (!artifacts.empty()) {
        const auto directory = std::filesystem::u8path(artifacts);
        if (std::filesystem::exists(directory) &&
            (!std::filesystem::is_directory(directory) || !std::filesystem::is_empty(directory)))
            throw std::invalid_argument("Artifacts directory must be absent or empty: " + artifacts);
        std::filesystem::create_directories(directory);
        save_png(original,artifacts+"/original.png");
        all_seams=original;
        mapping.reserve(checked_cells(original.height,original.width));
        for(int r=0;r<original.height;++r) for(int c=0;c<original.width;++c) mapping.emplace_back(r,c);
    }
    while(result.image.width>width || result.image.height>height) {
        const bool horizontal=horizontal_first ? result.image.height>height : result.image.width==width;
        Step step;
        step.index=static_cast<int>(result.steps.size())+1;
        step.axis=horizontal?'h':'v';step.width=result.image.width;step.height=result.image.height;
        auto start=Clock::now();
        Image oriented=horizontal?transpose(result.image):Image();
        const Image& input=horizontal?oriented:result.image;
        EnergyMap energy=compute_energy(input);
        step.energy_ms=elapsed(start);
        start=Clock::now();
        step.seam=find_seam(energy,method);
        validate_seam(energy,step.seam);
        step.search_ms=elapsed(start);
        // Save visualization outside the timed algorithm stages.
        std::ostringstream prefix;
        prefix<<artifacts<<"/"<<std::setw(4)<<std::setfill('0')<<step.index;
        if(!artifacts.empty()) {
            auto marked=overlay(input,step.seam.columns);
            auto em=energy_preview(energy);
            save_png(horizontal?transpose(marked):marked,prefix.str()+"_seam.png");
            save_png(horizontal?transpose(em):em,prefix.str()+"_energy.png");
            for(int r=0;r<input.height;++r) {
                int cr=horizontal?step.seam.columns[r]:r;
                int cc=horizontal?r:step.seam.columns[r];
                const auto original_pixel=mapping[static_cast<std::size_t>(cr)*result.image.width+cc];
                all_seams.at(original_pixel.first,original_pixel.second,0)=horizontal?40:255;
                all_seams.at(original_pixel.first,original_pixel.second,1)=horizontal?160:32;
                all_seams.at(original_pixel.first,original_pixel.second,2)=horizontal?255:64;
            }
        }
        start=Clock::now();
        Image removed=remove_vertical(input,step.seam.columns);
        Image next_image=horizontal?transpose(removed):std::move(removed);
        step.remove_ms=elapsed(start);
        if(!artifacts.empty()) {
            std::vector<std::pair<int,int>> next;
            next.reserve(checked_cells(horizontal?step.height-1:step.height,horizontal?step.width:step.width-1));
            if(horizontal) {
                // Different columns can lose different rows: preserve NEW row-major
                // order rather than streaming the survivors of the old layout.
                for(int r=0;r<result.image.height-1;++r) for(int c=0;c<result.image.width;++c) {
                    const int old_row=r>=step.seam.columns[c]?r+1:r;
                    next.push_back(mapping[static_cast<std::size_t>(old_row)*result.image.width+c]);
                }
            } else {
                for(int r=0;r<result.image.height;++r) for(int c=0;c<result.image.width;++c)
                    if(c!=step.seam.columns[r])next.push_back(mapping[static_cast<std::size_t>(r)*result.image.width+c]);
            }
            mapping=std::move(next);
        }
        result.image=std::move(next_image);
        if(!artifacts.empty())save_png(result.image,prefix.str()+"_after.png");
        result.energy_ms+=step.energy_ms;result.search_ms+=step.search_ms;result.remove_ms+=step.remove_ms;
        result.steps.push_back(std::move(step));
    }
    if(!artifacts.empty()) {
        save_png(result.image,artifacts+"/result.png");
        save_png(all_seams,artifacts+"/all_seams.png");
        write_text(artifacts+"/summary.json",result_json(result,method,original.width,original.height,horizontal_first));
    }
    return result;
}
} // namespace seam
