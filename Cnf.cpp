/**
 * Cnf.cpp
 * 
 * Implementierung: CNF-Parser, Normalisierung und Analyse
 */

#include "Cnf.h"
#include <iostream>
#include <cctype>

namespace sat {

// =================================================================
// CNFFormula Implementation
// =================================================================

void CNFFormula::parse_dimacs(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::string line;
    int clause_count = 0;
    int expected_clauses = -1;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == 'c') {
            continue;
        }
        
        // Parse problem line: "p cnf <num_vars> <num_clauses>"
        if (line[0] == 'p') {
            std::istringstream iss(line);
            std::string p, cnf;
            int num_clauses;
            
            if (!(iss >> p >> cnf >> num_vars_ >> num_clauses)) {
                throw std::runtime_error("Invalid problem line: " + line);
            }
            
            if (p != "p" || cnf != "cnf") {
                throw std::runtime_error("Expected 'p cnf', got: " + p + " " + cnf);
            }
            
            expected_clauses = num_clauses;
            clauses_.reserve(num_clauses);
            continue;
        }
        
        // Parse clause
        if (num_vars_ <= 0) {
            throw std::runtime_error("Clause before problem line");
        }
        
        Clause clause;
        std::istringstream iss(line);
        int literal;
        
        while (iss >> literal) {
            if (literal == 0) {
                break;  // End of clause
            }
            clause.push_back(literal);
        }
        
        if (!clause.empty()) {
            add_clause(clause);
            clause_count++;
        }
    }
    
    if (expected_clauses >= 0 && clause_count != expected_clauses) {
        std::cerr << "Warning: Expected " << expected_clauses 
                  << " clauses, got " << clause_count << std::endl;
    }
}

CNFFormula::Statistics CNFFormula::compute_statistics() const {
    Statistics stats;
    stats.num_variables = num_vars_;
    stats.num_clauses = clauses_.size();
    
    for (const auto& clause : clauses_) {
        stats.num_literals += clause.size();
        stats.max_clause_size = std::max(stats.max_clause_size, (int)clause.size());
        if (stats.min_clause_size == INT_MAX) {
            stats.min_clause_size = clause.size();
        } else {
            stats.min_clause_size = std::min(stats.min_clause_size, (int)clause.size());
        }
    }
    
    if (!clauses_.empty()) {
        stats.avg_clause_size = (double)stats.num_literals / clauses_.size();
    }
    
    return stats;
}

void CNFFormula::print_statistics() const {
    auto stats = compute_statistics();
    
    std::cout << "\n=== CNF Statistics ===" << std::endl;
    std::cout << "Variables: " << stats.num_variables << std::endl;
    std::cout << "Clauses: " << stats.num_clauses << std::endl;
    std::cout << "Total Literals: " << stats.num_literals << std::endl;
    std::cout << "Clause Size: [" << stats.min_clause_size 
              << ", " << stats.max_clause_size 
              << "], avg = " << stats.avg_clause_size << std::endl;
}

bool CNFFormula::is_valid_3sat() const {
    for (const auto& clause : clauses_) {
        if (clause.size() > 3) {
            return false;
        }
    }
    return true;
}

void CNFFormula::to_3sat() {
    if (is_valid_3sat()) {
        return;  // Already in 3-SAT
    }
    
    /**
     * Tseitin Transformation für k-SAT zu 3-SAT
     * 
     * Für Clause (a ∨ b ∨ c ∨ d) mit 4 Literalen:
     *   Einführe neue Variable x₁
     *   Ersetze durch:
     *     (a ∨ b ∨ x₁)
     *     (¬x₁ ∨ c ∨ d)
     * 
     * Allgemein für Clause mit > 3 Literalen:
     *   Iterativ aufteilen mit neuen Variablen
     */
    
    std::vector<Clause> new_clauses;
    new_clauses.reserve(clauses_.size() * 2);
    
    int new_var_counter = num_vars_ + 1;
    
    for (const auto& clause : clauses_) {
        if (clause.size() <= 3) {
            new_clauses.push_back(clause);
            continue;
        }
        
        // Split großen Clauses
        Clause remaining = clause;
        
        while (remaining.size() > 3) {
            // Nimm erste 2 Literale und neue Variable
            int new_var = new_var_counter++;
            
            Clause c1;
            c1.push_back(remaining[0]);
            c1.push_back(remaining[1]);
            c1.push_back(new_var);
            new_clauses.push_back(c1);
            
            // Entferne die ersten 2 Literale aus remaining
            remaining.erase(remaining.begin(), remaining.begin() + 2);
            remaining.push_back(-new_var);  // Negation der neuen Variable
        }
        
        new_clauses.push_back(remaining);
    }
    
    clauses_ = std::move(new_clauses);
    num_vars_ = new_var_counter - 1;
}

void CNFFormula::simplify() {
    /**
     * Vereinfachungs-Strategien:
     * 1. Entferne Duplikat-Clauses
     * 2. Entferne Tautologien (a ∨ ¬a)
     * 3. Unit Propagation (optional)
     */
    
    std::vector<Clause> simplified;
    std::set<std::set<Literal>> seen;
    
    for (auto& clause : clauses_) {
        // Prüfe auf Tautologie (a ∨ ¬a)
        std::set<Literal> seen_literals;
        bool is_tautology = false;
        
        for (Literal lit : clause) {
            // Prüfe ob das Gegenteil dieses Literals bereits gesehen wurde
            if (seen_literals.count(-lit)) {
                is_tautology = true;
                break;
            }
            seen_literals.insert(lit);
        }
        
        if (is_tautology) {
            continue;  // Tautologien sind immer erfüllt
        }
        
        // Normalisiere Clause (sortiert für Deduplication)
        std::sort(clause.begin(), clause.end());
        
        std::set<Literal> clause_set(clause.begin(), clause.end());
        
        if (seen.find(clause_set) == seen.end()) {
            seen.insert(clause_set);
            simplified.push_back(clause);
        }
    }
    
    clauses_ = std::move(simplified);
}

void CNFFormula::unit_propagate() {
    /**
     * Unit Clause Propagation:
     * Wenn Clause nur 1 Literal hat, muss dieses True sein
     * 
     * Beispiel:
     *   CNF: (a) ∧ (¬a ∨ b) ∧ (¬b ∨ c)
     *   
     *   Unit Clause: (a) → setze a = True
     *   Vereinfache mit a = True:
     *     (¬a ∨ b) wird zu (b)
     *     
     *   Unit Clause: (b) → setze b = True
     *   Vereinfache:
     *     (¬b ∨ c) wird zu (c)
     */
    
    SATAssignment assignment(num_vars_);
    bool changed = true;
    
    while (changed) {
        changed = false;
        
        std::vector<Clause> new_clauses;
        
        for (auto& clause : clauses_) {
            if (clause.size() == 1) {
                // Unit Clause gefunden
                Literal unit_lit = clause[0];
                int var = std::abs(unit_lit);
                
                if (!assignment.is_assigned(var)) {
                    assignment.set(var, unit_lit > 0);
                    changed = true;
                }
            }
            
            // Wende Assignment an
            bool satisfied = false;
            Clause reduced;
            
            for (Literal lit : clause) {
                int var = std::abs(lit);
                
                if (assignment.is_assigned(var)) {
                    bool val = assignment.value(var);
                    bool lit_val = (lit > 0) ? val : !val;
                    
                    if (lit_val) {
                        satisfied = true;
                        break;
                    }
                    // Literal ist false, entferne es
                } else {
                    reduced.push_back(lit);
                }
            }
            
            if (!satisfied) {
                new_clauses.push_back(reduced);
            }
        }
        
        clauses_ = std::move(new_clauses);
    }
}

std::string CNFFormula::to_dimacs() const {
    std::ostringstream oss;
    
    // Header
    oss << "c Generated DIMACS CNF\n";
    oss << "p cnf " << num_vars_ << " " << clauses_.size() << "\n";
    
    // Clauses
    for (const auto& clause : clauses_) {
        for (Literal lit : clause) {
            oss << lit << " ";
        }
        oss << "0\n";
    }
    
    return oss.str();
}

void CNFFormula::to_file(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write file: " + filename);
    }
    file << to_dimacs();
    file.close();
}

void CNFFormula::print() const {    
    std::cout << "\nc CNF Formula\n";
    std::cout << "p cnf " << num_vars_ << " " << clauses_.size() << "\n";
    
    for (size_t i = 0; i < std::min(clauses_.size(), size_t(10)); ++i) {
        const auto& clause = clauses_[i];
        std::cout << "Clause " << (i + 1) << ": (";
        for (size_t j = 0; j < clause.size(); ++j) {
            if (j > 0) std::cout << " ∨ ";
            int var = std::abs(clause[j]);
            std::cout << (clause[j] < 0 ? "¬" : "") << "x" << var;
        }
        std::cout << ")\n";
    }
    
    if (clauses_.size() > 10) {
        std::cout << "... and " << (clauses_.size() - 10) << " more clauses\n";
    }
    
    print_statistics();
}

// =================================================================
// SATAssignment Implementation
// =================================================================

std::string SATAssignment::to_string() const {
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (int var = 1; var <= num_vars_; ++var) {
        if (is_assigned(var)) {
            if (!first) oss << ", ";
            oss << "x" << var << "=" << (value(var) ? "T" : "F");
            first = false;
        }
    }
    
    oss << "}";
    return oss.str();
}

std::vector<std::pair<int, bool>> SATAssignment::to_vector() const {
    std::vector<std::pair<int, bool>> result;
    
    for (int var = 1; var <= num_vars_; ++var) {
        if (is_assigned(var)) {
            result.push_back({var, value(var)});
        }
    }
    
    return result;
}

// =================================================================
// SATAnalyzer Implementation
// =================================================================

std::vector<int> SATAnalyzer::variable_frequency() const {
    std::map<int, int> freq;
    
    // Zähle Vorkommen jeder Variable
    for (const auto& clause : formula_.clauses()) {
        std::set<int> vars_in_clause;
        for (Literal lit : clause) {
            vars_in_clause.insert(std::abs(lit));
        }
        
        for (int var : vars_in_clause) {
            freq[var]++;
        }
    }
    
    // Sortiere nach Häufigkeit
    std::vector<int> result;
    result.reserve(freq.size());
    
    for (const auto& [var, count] : freq) {
        result.push_back(var);
    }
    
    std::sort(
        result.begin(),
        result.end(),
        [&freq](int a, int b) { return freq[a] > freq[b]; }
    );
    
    return result;
}

std::vector<int> SATAnalyzer::pure_literals() const {
    /**
     * Pure Literal: Variable erscheint nur positiv oder nur negativ
     * Diese können direkt auf ihren Erscheinungsform gesetzt werden
     */
    
    std::set<int> positive_vars, negative_vars;
    
    for (const auto& clause : formula_.clauses()) {
        for (Literal lit : clause) {
            int var = std::abs(lit);
            
            if (lit > 0) {
                positive_vars.insert(var);
            } else {
                negative_vars.insert(var);
            }
        }
    }
    
    std::vector<int> pure_literals;
    
    for (int var = 1; var <= formula_.num_variables(); ++var) {
        bool is_positive = positive_vars.count(var) > 0;
        bool is_negative = negative_vars.count(var) > 0;
        
        if (is_positive != is_negative) {
            // Pure: kommt nur in einer Polarität vor
            pure_literals.push_back(var);
        }
    }
    
    return pure_literals;
}

std::map<int, int> SATAnalyzer::clause_polarity() const {
    /**
     * Berechne Polarität für jede Clause:
     * Positive Bias: mehr positive als negative Literale
     * Negative Bias: mehr negative als positive Literale
     */
    
    std::map<int, int> polarity;  // clause_id -> polarity (-1, 0, +1)
    
    int clause_id = 0;
    for (const auto& clause : formula_.clauses()) {
        int pos_count = 0, neg_count = 0;
        
        for (Literal lit : clause) {
            if (lit > 0) pos_count++;
            else neg_count++;
        }
        
        if (pos_count > neg_count) {
            polarity[clause_id] = 1;   // Positive bias
        } else if (neg_count > pos_count) {
            polarity[clause_id] = -1;  // Negative bias
        } else {
            polarity[clause_id] = 0;   // Neutral
        }
        
        clause_id++;
    }
    
    return polarity;
}

int SATAnalyzer::estimated_clique_size() const {
    /**
     * Heuristische Schätzung der Clique-Größe nach Reduktion
     * Basiert auf Clause-Struktur der CNF
     * 
     * Simplified: Max clause size + overhead für Reduktion
     */
    
    int max_clause_size = 0;
    for (const auto& clause : formula_.clauses()) {
        max_clause_size = std::max(max_clause_size, (int)clause.size());
    }
    
    // Grobe Heuristik: Clique-Größe ≈ 3 * max_clause_size
    // (wegen Reduction bei k-SAT zu 3-SAT)
    return std::max(3, 3 * max_clause_size);
}

} // namespace sat
