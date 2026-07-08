/**
 * Cnf.h
 * 
 * CNF-Parser und 3-SAT Datenstrukturen
 * 
 * Unterstützt:
 *  - DIMACS CNF Format (.cnf)
 *  - Beliebige k-SAT zu 3-SAT Reduktion
 *  - Clause Manipulation und Normalisierung
 */

#pragma once

#include <vector>
#include <set>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <cassert>

namespace sat {

using Literal = int;      // Positive = variable, Negative = negation
using Clause = std::vector<Literal>;
using Assignment = std::map<int, bool>;  // var -> value

/**
 * Repräsentation einer SAT-Instanz im DIMACS CNF Format
 * 
 * Beispiel (3-SAT):
 *   c This is a comment
 *   p cnf 3 2
 *   1 -3 0
 *   2 3 -1 0
 * 
 * Bedeutung:
 *   p cnf 3 2: 3 Variablen, 2 Clauses
 *   1 -3 0: (x₁ ∨ ¬x₃)
 *   2 3 -1 0: (x₂ ∨ x₃ ∨ ¬x₁)
 */
class CNFFormula {
public:
    struct Statistics {
        int num_variables = 0;
        int num_clauses = 0;
        int num_literals = 0;
        int max_clause_size = 0;
        int min_clause_size = INT_MAX;
        double avg_clause_size = 0.0;
    };
    
    // Konstruktoren
    CNFFormula() = default;
    
    /**
     * Parse DIMACS CNF file
     * Format:
     *   c comment lines
     *   p cnf <num_vars> <num_clauses>
     *   <clause> 0
     *   ...
     */
    explicit CNFFormula(const std::string& dimacs_file);
    
    // Accessor methods
    int num_variables() const { return num_vars_; }
    int num_clauses() const { return clauses_.size(); }
    const std::vector<Clause>& clauses() const { return clauses_; }
    std::vector<Clause>& clauses() { return clauses_; }
    const Clause& clause(size_t i) const { return clauses_[i]; }
    
    // Clause manipulation
    void add_clause(const Clause& c) {
        validate_clause(c);
        clauses_.push_back(c);
    }
    
    void add_clause(Clause&& c) {
        validate_clause(c);
        clauses_.push_back(std::move(c));
    }
    
    void set_num_variables(int n) { num_vars_ = n; }
    
    // Statistiken
    Statistics compute_statistics() const;
    void print_statistics() const;
    
    // Validierung
    bool is_valid_3sat() const;
    
    // Normalisierung
    /**
     * Konvertiere beliebige k-SAT zu 3-SAT
     * Für jede Clause mit > 3 Literalen:
     *   (a ∨ b ∨ c ∨ d) → (a ∨ b ∨ x₁) ∧ (¬x₁ ∨ c ∨ d)
     */
    void to_3sat();
    
    /**
     * Vereinfache CNF:
     *   - Entferne Duplikat-Clauses
     *   - Entferne Tautologien (a ∨ ¬a)
     *   - Unit propagation
     */
    void simplify();
    
    /**
     * Unit Propagation: Wenn Clause hat nur 1 Literal,
     * setze diese Variable und vereinfache
     */
    void unit_propagate();
    
    // Serialisierung
    std::string to_dimacs() const;
    void to_file(const std::string& filename) const;
    
    // Debug
    void print() const;
    
private:
    int num_vars_ = 0;
    std::vector<Clause> clauses_;
    
    void validate_clause(const Clause& c) const;
    void parse_dimacs(const std::string& filename);
};

/**
 * Variable Assignment für SAT-Instanzen
 * Verwaltet Zuweisungen und deren Konsistenz
 */
class SATAssignment {
public:
    SATAssignment(int num_vars = 0)
        : num_vars_(num_vars),
          assignment_(num_vars + 1, UNASSIGNED) {}
    
    enum Value : int { UNASSIGNED = -1, FALSE = 0, TRUE = 1 };
    
    // Setzen von Werten
    void set(int var, bool value) {
        assert(var > 0 && var <= num_vars_);
        assignment_[var] = value ? TRUE : FALSE;
    }
    
    // Abfragen
    bool is_assigned(int var) const {
        assert(var > 0 && var <= num_vars_);
        return assignment_[var] != UNASSIGNED;
    }
    
    bool value(int var) const {
        assert(var > 0 && var <= num_vars_);
        assert(is_assigned(var));
        return assignment_[var] == TRUE;
    }
    
    // Evaluierung
    bool evaluate_literal(Literal lit) const {
        int var = std::abs(lit);
        if (!is_assigned(var)) return true; // Unknown = True (optimistic)
        bool val = value(var);
        return (lit > 0) ? val : !val;
    }
    
    bool evaluate_clause(const Clause& c) const {
        for (Literal lit : c) {
            if (evaluate_literal(lit)) {
                return true;  // Erfüllt
            }
        }
        return false;  // Nicht erfüllt
    }
    
    bool satisfies_formula(const CNFFormula& formula) const {
        for (const auto& clause : formula.clauses()) {
            if (!evaluate_clause(clause)) {
                return false;
            }
        }
        return true;
    }
    
    // Serialisierung
    std::string to_string() const;
    std::vector<std::pair<int, bool>> to_vector() const;
    
    int num_vars() const { return num_vars_; }
    
private:
    int num_vars_;
    std::vector<int> assignment_;
};

/**
 * SAT-Instanz Statistiken & Analyse
 */
class SATAnalyzer {
public:
    explicit SATAnalyzer(const CNFFormula& formula) : formula_(formula) {}
    
    /**
     * Berechne Variable Frequency
     * Variablen die häufiger vorkommen sollten zuerst belegt werden
     */
    std::vector<int> variable_frequency() const;
    
    /**
     * Berechne Pure Literals
     * Wenn Variable nur positiv oder nur negativ, kann direkt belegt werden
     */
    std::vector<int> pure_literals() const;
    
    /**
     * Berechne Clause Polarität
     * Positive/Negative Bias für jede Clause
     */
    std::map<int, int> clause_polarity() const;
    
    /**
     * Berechne Graph-basierte Metriken für Subgraph-Reduktion
     * z.B. Expected Clique Size
     */
    int estimated_clique_size() const;
    
private:
    const CNFFormula& formula_;
};

// Inline Implementations
inline CNFFormula::CNFFormula(const std::string& dimacs_file) {
    parse_dimacs(dimacs_file);
}

inline void CNFFormula::validate_clause(const Clause& c) const {
    for (Literal lit : c) {
        int var = std::abs(lit);
        if (var == 0 || var > num_vars_) {
            throw std::invalid_argument(
                "Literal " + std::to_string(lit) + 
                " invalid for " + std::to_string(num_vars_) + " variables"
            );
        }
    }
}

} // namespace sat
