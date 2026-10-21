#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
        std::cout << "Assignment 2 - seam carving starter\n"
                  << "Usage: seamcarve --help | --status\n"
                  << "The build and interfaces are ready; image resizing is not implemented.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--status") {
        std::cout << "STARTER ONLY\n"
                  << "Image I/O: TODO\nEnergy: TODO\nDP: TODO\nGreedy: TODO\n"
                  << "Graph cut: TODO\nResize pipeline: TODO\nEvaluation: TODO\n";
        return 0;
    }
    std::cerr << "Unsupported command. The starter currently supports --help and --status only.\n";
    return 1;
}
