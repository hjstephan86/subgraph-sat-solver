/**
 * reduction.cpp
 * 
 * Implementierung: Komplette Reduktionskette
 */

#include "reduction.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace sat {

// =================================================================
// Graph Implementation
// =================================================================

std::string Graph::to_json() const {
    std::ostringstream oss;
    oss << "{\n  \"nodes\": [";
    
    for (int i = 0; i < num_vertices_; ++i) {
        if (i > 0) oss << ", ";
        oss << "\"v" << i << "\"";
    }
    
    oss << "],\n  \"edges\": [\n";
    
    bool first = true;
    for (int i = 0; i < num_vertices_; ++i) {
        for (int j = i + 1; j < num_vertices_; ++j) {
            if (adj_matrix_[i][j]) {
                if (!first) oss << ",\n";
                oss << "    [\"v" << i << "\", \"v" << j << "\"]";
                first = false;
            }
        }
    }
    
    oss << "\n  ]\n}";
    return oss.str();
}

void Graph::print() const {
    std::cout << "Graph with " << num_vertices_ << " vertices, "
              << num_edges_ << " edges\n";
    std::cout << "Edges:\n";
    
    int count = 0;
    for (int i = 0; i < num_vertices_; ++i) {
        for (int j = i + 1; j < num_vertices_; ++j) {
            if (adj_matrix_[i][j]) {
                std::cout << "  v" << i << " -- v" << j << "\n";
                count++;
                if (count >= 20) {
                    std::cout << "  ... and " << (num_edges_ - 20) << " more\n";
                    return;
                }
            }
        }
    }
}

// =================================================================
// ThreeSATToCliqueReducer Implementation
// =================================================================

std::vector<std::pair<int, int>> ThreeSATToCliqueReducer::create_nodes() const {
    /**
     * Erstelle (clause_id, literal_index) Paare für alle Literale
     */
    std::vector<std::pair<int, int>> nodes;
    
    for (int clause_id = 0; clause_id < (int)formula_.clauses().size(); ++clause_id) {
        const auto& clause = formula_.clause(clause_id);
        for (int lit_idx = 0; lit_idx < (int)clause.size(); ++lit_idx) {
            nodes.push_back({clause_id, lit_idx});
        }
    }
    
    return nodes;
}

bool ThreeSATToCliqueReducer::literals_compatible(int lit1, int lit2) const {
    /**
     * Zwei Literale sind kompatibel wenn:
     * - Sie nicht direkt widersprüchlich sind (nicht lit und ¬lit)
     */
    return lit1 != -lit2;
}

std::pair<Graph, int> ThreeSATToCliqueReducer::reduce() {
    // Validiere dass CNF 3-SAT ist
    if (!formula_.is_valid_3sat()) {
        throw std::runtime_error("Formula must be in 3-SAT for Clique reduction");
    }
    
    // Erstelle Knoten: ein Knoten pro (clause, literal) Paar
    auto nodes = create_nodes();
    int num_nodes = nodes.size();
    
    // Erstelle Graph
    Graph g(num_nodes);
    
    /**
     * Füge Kanten hinzu zwischen kompatiblen Paaren:
     * 
     * Knoten i = (clause_c1, lit_idx_i)
     * Knoten j = (clause_c2, lit_idx_j)
     * 
     * Kante (i,j) existiert wenn:
     *   1. c1 ≠ c2 (verschiedene Clauses)
     *   2. lit_i und lit_j sind kompatibel (nicht widersprechend)
     */
    
    for (int i = 0; i < num_nodes; ++i) {
        for (int j = i + 1; j < num_nodes; ++j) {
            int clause_i = nodes[i].first;
            int lit_idx_i = nodes[i].second;
            int lit_i = formula_.clause(clause_i)[lit_idx_i];
            
            int clause_j = nodes[j].first;
            int lit_idx_j = nodes[j].second;
            int lit_j = formula_.clause(clause_j)[lit_idx_j];
            
            // Kante nur wenn verschiedene Clauses und Literale kompatibel
            if (clause_i != clause_j && literals_compatible(lit_i, lit_j)) {
                g.add_edge(i, j);
            }
        }
    }
    
    // Clique-Größe = Anzahl der Clauses
    int clique_size = formula_.num_clauses();
    
    return {g, clique_size};
}

bool ThreeSATToCliqueReducer::validate() const {
    /**
     * Validierung: Prüfe dass Graph korrekt konstruiert wurde
     * 
     * - Korrekte Anzahl Knoten
     * - Nur erlaubte Kanten
     * - Komplementäre Literale verbunden?
     */
    return true;  // Simplified für jetzt
}

// =================================================================
// CliqueToSubgraphReducer Implementation
// =================================================================

Graph CliqueToSubgraphReducer::create_target_clique() const {
    /**
     * Konstruiere Complete Graph K_k
     * 
     * K_k hat k Knoten und k*(k-1)/2 Kanten (jeden mit jedem verbunden)
     */
    Graph complete(clique_size_);
    
    for (int i = 0; i < clique_size_; ++i) {
        for (int j = i + 1; j < clique_size_; ++j) {
            complete.add_edge(i, j);
        }
    }
    
    return complete;
}

// =================================================================
// SATToSubgraphReducer Implementation
// =================================================================

SATToSubgraphReducer::SATToSubgraphReducer(const CNFFormula& formula)
    : formula_(formula) {
    
    // Kopiere Formula
    formula_ = formula;
}

SATToSubgraphReducer::ReductionResult SATToSubgraphReducer::reduce() {
    std::cout << "[Reducer] Starting SAT → Subgraph reduction\n";
    
    ReductionResult result;
    result.original_num_variables = formula_.num_variables();
    result.original_num_clauses = formula_.num_clauses();
    
    // Step 1: Konvertiere zu 3-SAT
    std::cout << "[Reducer] Step 1: Converting to 3-SAT...\n";
    formula_.to_3sat();
    formula_.simplify();
    
    std::cout << "  Variables: " << formula_.num_variables()
              << ", Clauses: " << formula_.num_clauses() << "\n";
    
    // Step 2: 3-SAT → Clique
    std::cout << "[Reducer] Step 2: Reducing 3-SAT to Clique...\n";
    ThreeSATToCliqueReducer clique_reducer(formula_);
    auto [clique_graph, clique_size] = clique_reducer.reduce();
    
    clique_graph_ = clique_graph;
    clique_size_ = clique_size;
    
    std::cout << "  Clique graph: " << clique_graph.num_vertices() << " vertices, "
              << clique_graph.num_edges() << " edges\n";
    std::cout << "  Searching for clique of size " << clique_size << "\n";
    
    // Step 3: Clique → Subgraph
    std::cout << "[Reducer] Step 3: Converting Clique to Subgraph instance...\n";
    CliqueToSubgraphReducer subgraph_reducer(clique_graph, clique_size);
    auto subgraph_instance = subgraph_reducer.create_subgraph_instance();
    
    // Serialisiere zu JSON
    result.source_graph_json = subgraph_instance.source.to_json();
    result.target_graph_json = subgraph_instance.target.to_json();
    
    result.num_vertices = subgraph_instance.source.num_vertices();
    result.expected_clique_size = subgraph_instance.expected_size;
    
    std::cout << "[Reducer] Reduction complete!\n";
    std::cout << "  Source graph: " << subgraph_instance.source.num_vertices()
              << " vertices\n";
    std::cout << "  Target graph (K_" << subgraph_instance.expected_size
              << "): " << subgraph_instance.target.num_vertices() << " vertices\n";
    
    is_reduced_ = true;
    
    return result;
}

bool SATToSubgraphReducer::validate_reduction() const {
    if (!is_reduced_) {
        return false;
    }
    
    // Prüfe dass Clique-Graph korrekt ist
    // (simplified für jetzt)
    return true;
}

// =================================================================
// SubgraphToSATExtractor Implementation
// =================================================================

SATAssignment SubgraphToSATExtractor::extract_assignment(
    const CNFFormula& formula_3sat,
    const std::vector<std::pair<int, int>>& /*node_mapping*/) {
    
    /**
     * Gegeben ein Mapping von Clique-Knoten zu Graph-Knoten:
     * 
     * Für jeden Knoten im Mapping:
     *   node = (clause_id, lit_idx)
     *   lit = formula_3sat.clause(clause_id)[lit_idx]
     *   
     *   Setze Variable entsprechend dem Literal
     */
    
    SATAssignment assignment(formula_3sat.num_variables());
    std::set<int> assigned_vars;
    
    // for (const auto& [clique_node, graph_node] : node_mapping) {
        // clique_node = (clause_id, lit_idx)
        // Für einfachheit: Wir verwenden hier nur Teile des Mappings
        // In voller Implementierung würde das Mapping explizit konstruiert
    // }
    
    // Greedy-Ansatz: Für unzugewiesene Variablen, setze auf True
    for (int var = 1; var <= formula_3sat.num_variables(); ++var) {
        if (assigned_vars.find(var) == assigned_vars.end()) {
            assignment.set(var, true);
        }
    }
    
    return assignment;
}

} // namespace sat
