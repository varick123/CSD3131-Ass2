#include "resize.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace seam;

struct Args {
    std::map<std::string,std::string> values;
    bool steps=false;
    Args(int argc,char** argv) {
        for(int i=2;i<argc;++i) {
            std::string key=argv[i];
            if(key=="--steps") { if(steps)throw std::invalid_argument("Duplicate --steps.");steps=true;continue; }
            if(key.rfind("--",0)!=0 || i+1>=argc)throw std::invalid_argument("Expected --option value: "+key);
            if(!values.emplace(key,argv[++i]).second)throw std::invalid_argument("Duplicate option: "+key);
        }
    }
    std::string get(const std::string& key,const std::string& fallback="") const {
        auto it=values.find(key);return it==values.end()?fallback:it->second;
    }
    std::string require(const std::string& key) const {
        auto value=get(key);if(value.empty())throw std::invalid_argument("Missing "+key);return value;
    }
    int integer(const std::string& key,int fallback) const {
        auto text=get(key);if(text.empty())return fallback;
        std::size_t used=0;long long value=0;
        try {value=std::stoll(text,&used);}catch(...){throw std::invalid_argument("Invalid integer for "+key);}
        if(used!=text.size() || value<1 || value>2147483647LL)throw std::invalid_argument("Invalid positive integer for "+key);
        return static_cast<int>(value);
    }
    void allow(std::initializer_list<std::string> keys,bool allow_steps=false) const {
        std::set<std::string> accepted(keys);
        for(const auto& pair:values)if(!accepted.count(pair.first))throw std::invalid_argument("Unknown option: "+pair.first);
        if(steps&&!allow_steps)throw std::invalid_argument("--steps is only supported by compare.");
    }
    bool horizontal_first() const {
        auto order=get("--order","vertical-first");
        if(order!="vertical-first"&&order!="horizontal-first")throw std::invalid_argument("Invalid --order.");
        return order=="horizontal-first";
    }
};

static std::string path_join(const std::string& parent,const std::string& file) {
    return (fs::u8path(parent)/fs::u8path(file)).u8string();
}
static fs::path normalized_path(const std::string& path) {
    return fs::weakly_canonical(fs::absolute(fs::u8path(path)));
}
static bool component_equal(const fs::path& a,const fs::path& b) {
#ifdef _WIN32
    const auto x=a.wstring(), y=b.wstring();
    return CompareStringOrdinal(x.c_str(),-1,y.c_str(),-1,TRUE)==CSTR_EQUAL;
#else
    return a==b;
#endif
}
static bool equal_path(const fs::path& a,const fs::path& b) {
    auto left=a.begin(),right=b.begin();
    for(;left!=a.end()&&right!=b.end();++left,++right)
        if(!component_equal(*left,*right))return false;
    return left==a.end()&&right==b.end();
}
static bool same_location(const std::string& a,const std::string& b) {
    const auto left=normalized_path(a),right=normalized_path(b);
    if(equal_path(left,right))return true;
    return fs::exists(left)&&fs::exists(right)&&fs::equivalent(left,right);
}
static bool inside_directory(const std::string& path,const std::string& directory) {
    const auto file=normalized_path(path),parent=normalized_path(directory);
    auto item=file.begin();
    for(const auto& part:parent) {
        if(item==file.end()||!component_equal(*item,part))return false;
        ++item;
    }
    return true;
}
static void reject_input_alias(const std::string& input,const std::string& destination) {
    if(same_location(input,destination))
        throw std::invalid_argument("Destination would overwrite the input image: "+destination);
}
static void require_source_outside(const std::string& input,const std::string& directory) {
    if(inside_directory(input,directory))
        throw std::invalid_argument("Input image must be outside the output/artifacts directory: "+directory);
}
static void require_empty_artifacts(const std::string& directory) {
    const auto path=fs::u8path(directory);
    if(fs::exists(path)&&(!fs::is_directory(path)||!fs::is_empty(path)))
        throw std::invalid_argument("Artifacts directory must be absent or empty: "+directory);
}
static void check_resize_destinations(const std::string& input,const std::string& output,
                                      const std::string& artifacts) {
    reject_input_alias(input,output);
    reject_input_alias(input,output+".json");
    if(fs::is_directory(fs::u8path(output)))
        throw std::invalid_argument("Output must be a PNG file path, not a directory.");
    if(!artifacts.empty()) {
        require_source_outside(input,artifacts);
        require_empty_artifacts(artifacts);
        if(inside_directory(output,artifacts)&&!same_location(output,path_join(artifacts,"result.png")))
            throw std::invalid_argument("Output inside the artifacts directory must be result.png.");
    }
}
static void demo(const std::string& dir) {
    fs::create_directories(fs::u8path(dir));
    const int w=160,h=112;
    for(int scene=0;scene<8;++scene) {
        Image image(h,w);
        for(int r=0;r<h;++r)for(int c=0;c<w;++c) {
            int red=155+r/3, green=205+r/4, blue=235;
            if(scene==0) { // Isolated subject on a smooth background.
                if((c-85)*(c-85)+(r-56)*(r-56)<24*24) {red=220;green=55;blue=45;}
                if(r>90) {red=70;green=135;blue=65;}
            } else if(scene==1) { // Strong straight edges and windows.
                red=220;green=225;blue=225;
                if(c>35&&c<125&&r>14&&r<100) {red=80;green=90;blue=110;
                    if((c%20)>4&&(c%20)<15&&(r%20)>4&&(r%20)<15){red=210;green=225;blue=250;}}
            } else if(scene==2) { // Diagonal edges.
                if(std::abs(c-r)<4||std::abs(c+r-150)<4) {red=30;green=40;blue=50;}
            } else if(scene==3) { // Repeated high-contrast texture.
                int v=((c/7+r/7)%2)?235:25;red=v;green=v;blue=v;
            } else if(scene==4) { // Multiple subjects.
                for(int k=0;k<4;++k)if((c-(25+35*k))*(c-(25+35*k))+(r-(30+15*(k%2)))*(r-(30+15*(k%2)))<13*13){red=40+45*k;green=45;blue=195-30*k;}
                if(r>80){red=85;green=125;blue=70;}
            } else if(scene==5) { // Frame near both borders.
                if(c<5||c>w-6||r<5||r>h-6){red=25;green=25;blue=25;}
            } else if(scene==6) { // Thin bars resemble text strokes.
                red=245;green=245;blue=240;
                if(r>35&&r<78&&((c%18)<4||(r%19)<3)){red=20;green=30;blue=40;}
            } else { // Smooth gradient: many similar paths.
                red=(3*c+r)%256;green=(c+2*r)%256;blue=160;
            }
            image.at(r,c,0)=static_cast<std::uint8_t>(red);
            image.at(r,c,1)=static_cast<std::uint8_t>(green);
            image.at(r,c,2)=static_cast<std::uint8_t>(blue);
        }
        save_png(image,path_join(dir,"scene_"+std::to_string(scene+1)+".png"));
    }
    write_text(path_join(dir,"greedy_counterexample.txt"),"3 3\n1 2 100\n1 100 2\n100 100 1\n");
    std::cout<<"{\"demo_images\":8,\"width\":160,\"height\":112,\"directory\":\""<<json_escape(dir)<<"\"}\n";
}

static void trace(const Args& args) {
    args.allow({"--grid","--method"});
    std::ifstream stream(fs::u8path(args.require("--grid")));
    int h=0,w=0;
    if(!(stream>>h>>w))throw std::invalid_argument("Grid must begin with height width.");
    EnergyMap energy(h,w);
    for(auto& value:energy.values)if(!(stream>>value))throw std::invalid_argument("Grid is truncated or contains a non-integer.");
    std::string extra;if(stream>>extra)throw std::invalid_argument("Unexpected data after the grid.");
    const auto method=parse_method(args.get("--method","dp"));
    std::vector<Cost> table;
    auto path=method==Method::DP?find_dp(energy,&table):find_seam(energy,method);
    std::cout<<"{\"method\":\""<<method_name(method)<<"\",\"cost\":"<<path.cost<<",\"columns\":[";
    for(std::size_t i=0;i<path.columns.size();++i){if(i)std::cout<<",";std::cout<<path.columns[i];}
    std::cout<<"],\"cumulative\":[";
    for(std::size_t i=0;i<table.size();++i){if(i)std::cout<<",";std::cout<<table[i];}
    std::cout<<"]}\n";
}

static void compare(const Args& args) {
    args.allow({"--input","--output-dir","--width","--height","--repeats","--order"},true);
    const auto input=args.require("--input");
    const auto dir=args.require("--output-dir");
    require_source_outside(input,dir);
    if(fs::exists(fs::u8path(dir))&&!fs::is_directory(fs::u8path(dir)))
        throw std::invalid_argument("Comparison output must be a directory.");
    for(const auto& filename:{"original.png","energy.png","timings.csv","comparison.json"})
        reject_input_alias(input,path_join(dir,filename));
    for(const auto& name:{"dp","greedy","graph"}) {
        for(const auto& suffix:{".png","_vertical.png","_horizontal.png"})
            reject_input_alias(input,path_join(dir,std::string(name)+suffix));
        if(args.steps)require_empty_artifacts(path_join(dir,name));
    }
    const auto image=load_image(input);
    const int w=args.integer("--width",image.width),h=args.integer("--height",image.height);
    const int repetitions=args.integer("--repeats",5);
    if(repetitions>100)throw std::invalid_argument("Use at most 100 repetitions.");
    if(w>image.width||h>image.height)throw std::invalid_argument("Targets cannot enlarge the image.");
    const bool hf=args.horizontal_first();
    fs::create_directories(fs::u8path(dir));
    save_png(image,path_join(dir,"original.png"));
    const auto energy=compute_energy(image), horizontal_energy=compute_energy(transpose(image));
    Cost dp_vertical=-1,dp_horizontal=-1;
    std::ostringstream json,csv;
    json<<std::fixed<<std::setprecision(6)<<"{\"input_width\":"<<image.width<<",\"input_height\":"<<image.height
        <<",\"target_width\":"<<w<<",\"target_height\":"<<h<<",\"repeats\":"<<repetitions
        <<",\"order\":\""<<(hf?"horizontal-first":"vertical-first")<<"\",\"methods\":[";
    csv<<"method,repetition,width,height,target_width,target_height,order,energy_ms,search_ms,remove_ms,total_ms,vertical_cost,horizontal_cost\n";
    bool first=true;
    for(auto method:{Method::DP,Method::Greedy,Method::Graph}) {
        const auto name=method_name(method);
        const auto vertical=find_seam(energy,method);
        const auto horizontal=find_seam(horizontal_energy,method);
        if(method==Method::DP){dp_vertical=vertical.cost;dp_horizontal=horizontal.cost;}
        if(method==Method::Graph&&(vertical.cost!=dp_vertical||horizontal.cost!=dp_horizontal))
            throw std::logic_error("DP and graph disagree on the same original energy map.");
        save_png(overlay(image,vertical.columns),path_join(dir,name+"_vertical.png"));
        save_png(transpose(overlay(transpose(image),horizontal.columns)),path_join(dir,name+"_horizontal.png"));
        // Warm-up and every measurement start from independent original-image copies.
        resize(image,w,h,method,hf);
        std::vector<double> samples;
        ResizeResult representative;
        for(int run=0;run<repetitions;++run) {
            auto result=resize(image,w,h,method,hf);
            samples.push_back(result.total_ms());
            csv<<std::fixed<<std::setprecision(6)<<name<<","<<run+1<<","<<image.width<<","<<image.height
               <<","<<w<<","<<h<<","<<(hf?"horizontal-first":"vertical-first")<<","<<result.energy_ms
               <<","<<result.search_ms<<","<<result.remove_ms<<","<<result.total_ms()<<","<<vertical.cost<<","<<horizontal.cost<<"\n";
            if(run==0)representative=std::move(result);
        }
        save_png(representative.image,path_join(dir,name+".png"));
        if(args.steps)resize(image,w,h,method,hf,path_join(dir,name));
        std::sort(samples.begin(),samples.end());
        const double median=samples.size()%2?samples[samples.size()/2]:(samples[samples.size()/2-1]+samples[samples.size()/2])/2;
        auto detail=result_json(representative,method,image.width,image.height,hf);detail.pop_back();
        if(!first)json<<",";first=false;
        json<<detail<<",\"median_ms\":"<<median<<",\"min_ms\":"<<samples.front()<<",\"max_ms\":"<<samples.back()
            <<",\"vertical_cost\":"<<vertical.cost<<",\"horizontal_cost\":"<<horizontal.cost<<",\"output\":\""<<name<<".png\"}";
    }
    json<<"]}";
    write_text(path_join(dir,"timings.csv"),csv.str());
    write_text(path_join(dir,"comparison.json"),json.str());
    save_png(energy_preview(energy),path_join(dir,"energy.png"));
    std::cout<<json.str()<<"\n";
}

static int run(int argc,char** argv) {
    try {
        if(argc<2||std::string(argv[1])=="--help"||std::string(argv[1])=="help") {
            std::cout<<"Seam carving: custom DP, greedy and max-flow/min-cut\n"
                <<"  seamcarve info --input image.png\n"
                <<"  seamcarve demo --output-dir data/demo\n"
                <<"  seamcarve resize --input image.png --output result.png --width 120 --height 90 --method dp [--order vertical-first] [--artifacts frames]\n"
                <<"  seamcarve compare --input image.png --output-dir results --width 120 --height 90 [--repeats 5] [--steps]\n"
                <<"  seamcarve trace --grid data/demo/greedy_counterexample.txt --method dp\n";
            return 0;
        }
        const std::string command=argv[1];const Args args(argc,argv);
        if(command=="demo") {args.allow({"--output-dir"});demo(args.require("--output-dir"));}
        else if(command=="info") {
            args.allow({"--input"});auto image=inspect_image(args.require("--input"));
            std::cout<<"{\"width\":"<<image.width<<",\"height\":"<<image.height<<"}\n";
        } else if(command=="trace") trace(args);
        else if(command=="compare") compare(args);
        else if(command=="resize") {
            args.allow({"--input","--output","--width","--height","--method","--order","--artifacts"});
            const auto input=args.require("--input"),output=args.require("--output");
            const auto artifacts=args.get("--artifacts");
            check_resize_destinations(input,output,artifacts);
            auto image=load_image(input);
            const auto method=parse_method(args.get("--method","dp"));const bool hf=args.horizontal_first();
            auto result=resize(image,args.integer("--width",image.width),args.integer("--height",image.height),method,hf,artifacts);
            save_png(result.image,output);
            auto json=result_json(result,method,image.width,image.height,hf);
            write_text(output+".json",json);std::cout<<json<<"\n";
        } else throw std::invalid_argument("Unknown command: "+command);
        return 0;
    } catch(const std::exception& error) {std::cerr<<"Error: "<<error.what()<<"\n";return 1;}
}

#ifdef _WIN32
int wmain(int argc,wchar_t** wide_argv) {
    std::vector<std::string> storage;
    for(int i=0;i<argc;++i) {
        const int size=WideCharToMultiByte(CP_UTF8,0,wide_argv[i],-1,nullptr,0,nullptr,nullptr);
        if(size<=0)return 1;
        std::string value(static_cast<std::size_t>(size),'\0');
        WideCharToMultiByte(CP_UTF8,0,wide_argv[i],-1,&value[0],size,nullptr,nullptr);
        value.pop_back();storage.push_back(std::move(value));
    }
    std::vector<char*> arguments;
    for(auto& value:storage)arguments.push_back(&value[0]);
    return run(argc,arguments.data());
}
#else
int main(int argc,char** argv) { return run(argc,argv); }
#endif
