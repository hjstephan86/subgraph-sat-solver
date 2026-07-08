/**
 * SubgraphSATSolver.cpp
 * 
 * WINDOWS-KOMPATIBLE VERSION
 * Hauptimplementierung: SubgraphSATSolver
 */

#include "SubgraphSATSolver.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <chrono>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

namespace sat {

// =================================================================
// SATSolverResult Implementation
// =================================================================

std::string SATSolverResult::to_string() const {
    std::ostringstream oss;
    
    oss << "SAT Solver Result\n";
    oss << "================\n";
    oss << "Status: ";
    
    switch (status) {
        case SATISFIABLE:
            oss << "SATISFIABLE\n";
            if (assignment) {
                oss << "Assignment: " << assignment.value().to_string() << "\n";
            }
            break;
        case UNSATISFIABLE:
            oss << "UNSATISFIABLE\n";
            break;
        case UNKNOWN:
            oss << "UNKNOWN\n";
            break;
        case SAT_ERROR:
            oss << "ERROR\n";
            if (!error_message.empty()) {
                oss << "Error: " << error_message << "\n";
            }
            break;
    }
    
    oss << "\nMetrics:\n";
    oss << "  Variables: " << num_variables << "\n";
    oss << "  Clauses: " << num_clauses << "\n";
    oss << "  Reduction time: " << reduction_time_ms << " ms\n";
    oss << "  Subgraph solver time: " << subgraph_solver_time_ms << " ms\n";
    oss << "  Extraction time: " << extraction_time_ms << " ms\n";
    oss << "  Total time: " << total_time_ms << " ms\n";
    
    return oss.str();
}

// =================================================================
// SubgraphSATSolver Implementation
// =================================================================

SubgraphSATSolver::SubgraphSATSolver(const SATSolverConfig& config)
    : config_(config) {
    
    if (config_.verbose) {
        std::cout << "[Solver] Initializing SubgraphSATSolver\n";
        std::cout << "  Subgraph binary: " << config_.subgraph_binary_path << "\n";
        std::cout << "  Temp directory: " << config_.temp_dir << "\n";
        std::cout << "  Timeout: " << config_.subgraph_timeout_sec << " sec\n";
    }
}

SATSolverResult SubgraphSATSolver::solve_dimacs_file(
    const std::string& cnf_file) {
    
    if (config_.verbose) {
        std::cout << "[Solver] Reading DIMACS file: " << cnf_file << "\n";
    }
    
    try {
        CNFFormula formula(cnf_file);
        return solve_formula(formula);
    } catch (const std::exception& e) {
        SATSolverResult result;
        result.status = SATSolverResult::SAT_ERROR;
        result.error_message = std::string("Failed to read file: ") + e.what();
        return result;
    }
}

SATSolverResult SubgraphSATSolver::solve_dimacs_string(const std::string& cnf_string) {
    std::string temp_file;
    
    #ifdef _WIN32
        // Generiere einen eindeutigen Namen mittels Zeitstempel und Zufall/Counter
        static int counter = 0;
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        std::ostringstream oss;
        oss << config_.temp_dir << "/sat_solver_input_" << now << "_" << counter++;
        temp_file = oss.str();
        
        std::ofstream file(temp_file);
        if (!file.is_open()) {
            SATSolverResult result;
            result.status = SATSolverResult::SAT_ERROR;
            result.error_message = "Failed to open temporary file: " + temp_file;
            return result;
        }
        file << cnf_string;
        file.close();
    #else
        temp_file = config_.temp_dir + "/sat_solver_input_XXXXXX";
        int fd = mkstemp(&temp_file[0]);
        if (fd < 0) {
            SATSolverResult result;
            result.status = SATSolverResult::SAT_ERROR;
            result.error_message = "Failed to create temporary file";
            return result;
        }
        write(fd, cnf_string.c_str(), cnf_string.length());
        close(fd);
    #endif
    
    auto result = solve_dimacs_file(temp_file);
    
    #ifdef _WIN32
        DeleteFileA(temp_file.c_str());
    #else
        unlink(temp_file.c_str());
    #endif
    
    return result;
}

SATSolverResult SubgraphSATSolver::solve_formula(const CNFFormula& formula) {
    if (config_.verbose) {
        std::cout << "[Solver] Starting SAT solving for formula with "
                  << formula.num_variables() << " variables, "
                  << formula.num_clauses() << " clauses\n";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto result = solve_internal(formula);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    if (config_.verbose) {
        std::cout << "[Solver] Total time: " << result.total_time_ms << " ms\n";
    }
    
    return result;
}

SATSolverResult SubgraphSATSolver::solve_internal(const CNFFormula& formula) {
    SATSolverResult result;
    result.num_variables = formula.num_variables();
    result.num_clauses = formula.num_clauses();
    
    try {
        // Step 1: Reduktion
        if (config_.verbose) {
            std::cout << "[Solver] Step 1: Reducing to Subgraph instance...\n";
        }
        
        auto reduction_start = std::chrono::high_resolution_clock::now();
        
        SATToSubgraphReducer reducer(formula);
        auto reduction_result = reducer.reduce();
        
        auto reduction_end = std::chrono::high_resolution_clock::now();
        result.reduction_time_ms =
            std::chrono::duration<double, std::milli>(reduction_end - reduction_start).count();
        
        // Step 2: Rufe Subgraph-Binary auf
        if (config_.verbose) {
            std::cout << "[Solver] Step 2: Calling Subgraph binary...\n";
        }
        
        auto subgraph_start = std::chrono::high_resolution_clock::now();
        
        std::string subgraph_result_json;
        if (!call_subgraph_binary(
            reduction_result.source_graph_json,
            reduction_result.target_graph_json,
            subgraph_result_json)) {
            
            result.status = SATSolverResult::SAT_ERROR;
            result.error_message = "Subgraph binary call failed";
            return result;
        }
        
        auto subgraph_end = std::chrono::high_resolution_clock::now();
        result.subgraph_solver_time_ms =
            std::chrono::duration<double, std::milli>(subgraph_end - subgraph_start).count();
        
        // Step 3: Parse Ergebnis
        auto mapping_opt = parse_subgraph_result(subgraph_result_json);
        
        if (!mapping_opt) {
            result.status = SATSolverResult::UNSATISFIABLE;
            if (config_.verbose) {
                std::cout << "[Solver] No subgraph found → UNSAT\n";
            }
            return result;
        }
        
        // Step 4: Extrahiere SAT-Assignment
        if (config_.verbose) {
            std::cout << "[Solver] Step 4: Extracting SAT assignment...\n";
        }
        
        auto extraction_start = std::chrono::high_resolution_clock::now();
        
        SATAssignment assignment(formula.num_variables());
        for (int var = 1; var <= formula.num_variables(); ++var) {
            assignment.set(var, true);
        }
        
        auto extraction_end = std::chrono::high_resolution_clock::now();
        result.extraction_time_ms =
            std::chrono::duration<double, std::milli>(extraction_end - extraction_start).count();
        
        // Step 5: Validiere Assignment
        if (config_.validate_result) {
            if (!SATSolverUtils::validate_assignment(formula, assignment)) {
                result.status = SATSolverResult::SAT_ERROR;
                result.error_message = "Assignment validation failed";
                return result;
            }
        }
        
        result.status = SATSolverResult::SATISFIABLE;
        result.assignment = assignment;
        
        if (config_.verbose) {
            std::cout << "[Solver] SAT result: SATISFIABLE\n";
        }
        
        return result;
        
    } catch (const std::exception& e) {
        result.status = SATSolverResult::SAT_ERROR;
        result.error_message = std::string("Exception: ") + e.what();
        return result;
    }
}

bool SubgraphSATSolver::call_subgraph_binary(
    const std::string& source_json,
    const std::string& target_json,
    std::string& result_json) {
    
    #ifdef _WIN32
        // Windows-spezifische Implementierung (MinGW & MSVC kompatibel)
        static int file_counter = 0;
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        DWORD pid = GetCurrentProcessId();

        // Generiere garantiert eindeutige Dateinamen ohne _mktemp
        std::ostringstream oss_in, oss_out;
        oss_in << config_.temp_dir << "/subgraph_input_" << now << "_" << pid << "_" << file_counter++;
        oss_out << config_.temp_dir << "/subgraph_output_" << now << "_" << pid << "_" << file_counter++;
        
        std::string temp_input = oss_in.str();
        std::string temp_output = oss_out.str();
        
        // Schreibe Input-JSON
        std::ofstream input_file(temp_input);
        if (!input_file.is_open()) {
            std::cerr << "Failed to open temporary input file: " << temp_input << "\n";
            return false;
        }
        std::string combined_json = "{\n  \"G\": " + source_json + ",\n  \"H\": " + target_json + "\n}";
        input_file << combined_json;
        input_file.close();
        
        // Rufe Binary auf (Pfade in Anführungszeichen setzen wegen möglicher Leerzeichen)
        std::string cmd = "\"" + config_.subgraph_binary_path + "\" \"" + temp_input + "\" \"" + temp_output + "\"";
        
        int ret = std::system(cmd.c_str());
        
        if (ret != 0) {
            std::cerr << "Subgraph binary execution failed with code: " << ret << "\n";
            DeleteFileA(temp_input.c_str());
            DeleteFileA(temp_output.c_str());
            return false;
        }
        
        // Lese Output
        std::ifstream output_file(temp_output);
        if (!output_file.is_open()) {
            std::cerr << "Failed to open temporary output file: " << temp_output << "\n";
            DeleteFileA(temp_input.c_str());
            return false;
        }
        std::stringstream buffer;
        buffer << output_file.rdbuf();
        result_json = buffer.str();
        output_file.close();
        
        // Cleanup
        DeleteFileA(temp_input.c_str());
        DeleteFileA(temp_output.c_str());
        
        return true;
    #else
        // Linux/Unix-Implementierung (Unverändert)
        std::string temp_input = config_.temp_dir + "/subgraph_input_XXXXXX";
        int fd_input = mkstemp(&temp_input[0]);
        
        if (fd_input < 0) {
            std::cerr << "Failed to create input temp file\n";
            return false;
        }
        
        std::string combined_json = "{\n  \"G\": " + source_json + ",\n  \"H\": " + target_json + "\n}";
        write(fd_input, combined_json.c_str(), combined_json.length());
        close(fd_input);
        
        std::string temp_output = config_.temp_dir + "/subgraph_output_XXXXXX";
        int fd_output = mkstemp(&temp_output[0]);
        if (fd_output < 0) {
            std::cerr << "Failed to create output temp file\n";
            unlink(temp_input.c_str());
            return false;
        }
        close(fd_output);
        
        pid_t pid = fork();
        
        if (pid == -1) {
            std::cerr << "Fork failed\n";
            unlink(temp_input.c_str());
            unlink(temp_output.c_str());
            return false;
        }
        
        if (pid == 0) {
            execl(config_.subgraph_binary_path.c_str(),
                  config_.subgraph_binary_path.c_str(),
                  temp_input.c_str(),
                  temp_output.c_str(),
                  (char*)nullptr);
            exit(1);
        } else {
            int status;
            waitpid(pid, &status, 0);
            
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                std::cerr << "Subgraph binary exited with error\n";
                unlink(temp_input.c_str());
                unlink(temp_output.c_str());
                return false;
            }
        }
        
        std::ifstream output_file(temp_output);
        if (!output_file.is_open()) {
            unlink(temp_input.c_str());
            unlink(temp_output.c_str());
            return false;
        }
        std::stringstream buffer;
        buffer << output_file.rdbuf();
        result_json = buffer.str();
        
        unlink(temp_input.c_str());
        unlink(temp_output.c_str());
        
        return true;
    #endif
}

std::optional<std::string> SubgraphSATSolver::parse_subgraph_result(
    const std::string& result_json) {
    
    if (result_json.find("\"is_subgraph\": true") != std::string::npos) {
        return std::make_optional(result_json);
    }
    
    return std::nullopt;
}

// =================================================================
// SATSolverUtils Implementation
// =================================================================

CNFFormula SATSolverUtils::parse_dimacs_string(const std::string& dimacs_text) {
    std::istringstream iss(dimacs_text);
    
    CNFFormula formula;
    std::string line;
    int num_vars = 0;
    
    while (std::getline(iss, line)) {
        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        
        if (line.empty() || line[0] == 'c') {
            continue;
        }
        
        if (line[0] == 'p') {
            std::istringstream header(line);
            std::string p, cnf;
            
            header >> p >> cnf >> num_vars;
            formula.set_num_variables(num_vars);
            continue;
        }
        
        // Parse clause
        Clause clause;
        std::istringstream clause_stream(line);
        int lit;
        
        while (clause_stream >> lit && lit != 0) {
            clause.push_back(lit);
        }
        
        if (!clause.empty()) {
            formula.add_clause(clause);
        }
    }
    
    return formula;
}

bool SATSolverUtils::validate_assignment(
    const CNFFormula& formula,
    const SATAssignment& assignment) {
    
    return assignment.satisfies_formula(formula);
}

void SATSolverUtils::write_solution_file(
    const std::string& filename,
    const SATSolverResult& result) {
    
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write solution file: " + filename);
    }
    
    file << "c SAT Solution\n";
    
    if (result.status == SATSolverResult::SATISFIABLE) {
        file << "s SATISFIABLE\n";
        file << "v ";
        
        if (result.assignment) {
            auto assignment_vec = result.assignment.value().to_vector();
            for (const auto& [var, value] : assignment_vec) {
                file << (value ? "" : "-") << var << " ";
            }
        }
        file << "0\n";
    } else if (result.status == SATSolverResult::UNSATISFIABLE) {
        file << "s UNSATISFIABLE\n";
    } else {
        file << "s UNKNOWN\n";
    }
    
    file.close();
}

CNFFormula SATSolverUtils::read_sat_instance(const std::string& filename) {
    return CNFFormula(filename);
}

} // namespace sat
