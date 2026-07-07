/**
 * main.cpp
 * 
 * Standalone Subgraph-SAT-Solver CLI
 * 
 * Nutzt die C++-Implementierung der kompletten Reduktionskette:
 *   SAT → 3-SAT → Clique → Subgraph-Isomorphismus
 * 
 * Usage:
 *   subgraph-sat-solver <input.cnf> [options]
 *   subgraph-sat-solver --help
 */

#include "cnf.h"
#include "reduction.h"
#include "subgraph_sat_solver.h"
#include <iostream>
#include <string>
#include <cstring>
#include <iomanip>

using namespace sat;

struct CommandLineArgs {
    std::string input_file;
    std::string output_file;
    std::string subgraph_binary = "./subgraph";
    std::string temp_dir = "/tmp";
    int timeout_sec = 60;
    bool verbose = false;
    bool validate = true;
    bool show_help = false;
    bool show_stats = false;
};

void print_help(const char* program_name) {
    std::cout << "Subgraph-SAT-Solver v1.0\n";
    std::cout << "========================\n\n";
    std::cout << "Usage: " << program_name << " <input.cnf> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <file>        Write solution to file\n";
    std::cout << "  --subgraph-binary <path>   Path to subgraph binary\n";
    std::cout << "                             (default: ./subgraph)\n";
    std::cout << "  --temp-dir <dir>           Temporary directory for JSON files\n";
    std::cout << "                             (default: /tmp)\n";
    std::cout << "  --timeout <seconds>        Timeout for subgraph solver\n";
    std::cout << "                             (default: 60)\n";
    std::cout << "  -v, --verbose              Enable verbose output\n";
    std::cout << "  --no-validate              Skip result validation\n";
    std::cout << "  --stats                    Show input formula statistics\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << program_name << " benchmark.cnf -o solution.txt -v\n";
}

CommandLineArgs parse_arguments(int argc, char* argv[]) {
    CommandLineArgs args;
    
    if (argc < 2) {
        args.show_help = true;
        return args;
    }
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.show_help = true;
            return args;
        }
        else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        }
        else if (arg == "--stats") {
            args.show_stats = true;
        }
        else if (arg == "--no-validate") {
            args.validate = false;
        }
        else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                exit(1);
            }
            args.output_file = argv[++i];
        }
        else if (arg == "--subgraph-binary") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                exit(1);
            }
            args.subgraph_binary = argv[++i];
        }
        else if (arg == "--temp-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                exit(1);
            }
            args.temp_dir = argv[++i];
        }
        else if (arg == "--timeout") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                exit(1);
            }
            args.timeout_sec = std::stoi(argv[++i]);
        }
        else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            exit(1);
        }
        else {
            if (args.input_file.empty()) {
                args.input_file = arg;
            } else {
                std::cerr << "Error: Multiple input files not supported\n";
                exit(1);
            }
        }
    }
    
    if (args.input_file.empty() && !args.show_help) {
        std::cerr << "Error: Input file required\n";
        exit(1);
    }
    
    return args;
}

int main(int argc, char* argv[]) {
    // Parse arguments
    auto args = parse_arguments(argc, argv);
    
    if (args.show_help) {
        print_help(argv[0]);
        return 0;
    }
    
    // Banner
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║   Subgraph-SAT-Solver v1.0             ║\n";
    std::cout << "║   Reduction: SAT → Subgraph-Isom       ║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";
    
    // Configure solver
    SATSolverConfig config;
    config.subgraph_binary_path = args.subgraph_binary;
    config.temp_dir = args.temp_dir;
    config.subgraph_timeout_sec = args.timeout_sec;
    config.verbose = args.verbose;
    config.validate_result = args.validate;
    
    try {
        // Read input
        std::cout << "Reading input file: " << args.input_file << "\n";
        CNFFormula formula(args.input_file);
        
        // Show statistics
        if (args.show_stats) {
            formula.print_statistics();
        }
        
        std::cout << "\n";
        std::cout << "Formula: " << formula.num_variables() << " vars, "
                  << formula.num_clauses() << " clauses\n";
        
        // Create solver
        SubgraphSATSolver solver(config);
        
        // Solve
        std::cout << "\nSolving...\n";
        auto result = solver.solve_formula(formula);
        
        // Print result
        std::cout << "\n" << result.to_string() << "\n";
        
        // Write output if requested
        if (!args.output_file.empty()) {
            std::cout << "Writing solution to: " << args.output_file << "\n";
            SATSolverUtils::write_solution_file(args.output_file, result);
        }
        
        // Exit code
        return (result.status == SATSolverResult::SATISFIABLE) ? 10 : 20;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }
}
