/**
 * subgraph_sat_solver.h
 * 
 * Hauptklasse: SubgraphSATSolver
 * Polynomial-time SAT-Solver via Subgraph-Isomorphismus
 */

#pragma once

#include "cnf.h"
#include "reduction.h"
#include <string>
#include <optional>
#include <vector>

namespace sat {

/**
 * SATSolverResult - Ergebnis eines SAT-Solver Aufrufs
 */
struct SATSolverResult {
    enum Status {
        SATISFIABLE,
        UNSATISFIABLE,
        UNKNOWN,
        SAT_ERROR  // NICHT ERROR (Konflikt mit windows.h Makro)
    };
    
    Status status = UNKNOWN;
    std::optional<SATAssignment> assignment;
    
    // Metriken
    int num_variables = 0;
    int num_clauses = 0;
    
    double reduction_time_ms = 0.0;
    double subgraph_solver_time_ms = 0.0;
    double extraction_time_ms = 0.0;
    double total_time_ms = 0.0;
    
    std::string error_message;
    
    // Hilfsfunktionen
    bool is_satisfiable() const { return status == SATISFIABLE; }
    bool is_unsatisfiable() const { return status == UNSATISFIABLE; }
    bool is_error() const { return status == SAT_ERROR; }
    
    std::string to_string() const;
};

/**
 * SATSolverConfig - Konfiguration des Solvers
 */
struct SATSolverConfig {
    // Path zum Subgraph-Binary
    std::string subgraph_binary_path = "./subgraph";
    
    // Timeout für Subgraph-Solver (Sekunden)
    int subgraph_timeout_sec = 60;
    
    // Verzeichnis für temporäre Dateien
    std::string temp_dir = "/tmp";
    
    // Verbose-Modus
    bool verbose = false;
    
    // Validiere Ergebnis nach Extraktion
    bool validate_result = true;
};

/**
 * SubgraphSATSolver - Hauptklasse
 * 
 * Löst SAT-Probleme in polynomieller Zeit durch Reduktion auf
 * Subgraph-Isomorphismus mit O(n³) Komplexität.
 */
class SubgraphSATSolver {
public:
    // Konstruktor
    explicit SubgraphSATSolver(const SATSolverConfig& config);
    
    // Destruktor
    ~SubgraphSATSolver() = default;
    
    // API: Löse aus verschiedenen Quellen
    SATSolverResult solve_dimacs_file(const std::string& cnf_file);
    SATSolverResult solve_dimacs_string(const std::string& cnf_string);
    SATSolverResult solve_formula(const CNFFormula& formula);
    
private:
    SATSolverConfig config_;
    
    // Interne Methoden
    SATSolverResult solve_internal(const CNFFormula& formula);
    
    bool call_subgraph_binary(
        const std::string& source_json,
        const std::string& target_json,
        std::string& result_json);
    
    std::optional<std::string> parse_subgraph_result(
        const std::string& result_json);
};

/**
 * SATSolverUtils - Utility-Funktionen
 */
class SATSolverUtils {
public:
    static CNFFormula parse_dimacs_string(const std::string& dimacs_text);
    
    static bool validate_assignment(
        const CNFFormula& formula,
        const SATAssignment& assignment);
    
    static void write_solution_file(
        const std::string& filename,
        const SATSolverResult& result);
    
    static CNFFormula read_sat_instance(const std::string& filename);
};

} // namespace sat
