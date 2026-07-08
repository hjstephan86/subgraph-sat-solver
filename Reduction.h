/**
 * Reduction.h
 * 
 * Reduktions-Algorithmen für die SAT-Lösung via Subgraph-Isomorphismus
 * 
 * Reduktionskette:
 *   SAT → 3-SAT → Clique → Subgraph-Isomorphismus
 */

#pragma once

#include "Cnf.h"
#include <vector>
#include <set>
#include <map>
#include <string>
#include <memory>
#include <cstring>

namespace sat {

/**
 * Graph-Repräsentation für Clique-Reduktion
 * 
 * Der Graph wird als Adjacency Matrix repräsentiert:
 *   adj[i][j] = 1 wenn Kante (i,j) existiert, 0 sonst
 */
class Graph {
public:
    Graph() = default;
    
    explicit Graph(int num_vertices)
        : num_vertices_(num_vertices),
          adj_matrix_(num_vertices, std::vector<bool>(num_vertices, false)) {}
    
    // Accessor
    int num_vertices() const { return num_vertices_; }
    int num_edges() const { return num_edges_; }
    
    // Edge manipulation
    void add_edge(int u, int v) {
        if (u == v) return;  // Keine Self-Loops
        if (u < 0 || u >= num_vertices_ || v < 0 || v >= num_vertices_) {
            throw std::out_of_range("Vertex out of range");
        }
        
        if (!adj_matrix_[u][v]) {
            adj_matrix_[u][v] = true;
            adj_matrix_[v][u] = true;  // Undirected graph
            num_edges_++;
        }
    }
    
    bool has_edge(int u, int v) const {
        if (u < 0 || u >= num_vertices_ || v < 0 || v >= num_vertices_) {
            return false;
        }
        return adj_matrix_[u][v];
    }
    
    std::vector<int> neighbors(int v) const {
        std::vector<int> result;
        for (int i = 0; i < num_vertices_; ++i) {
            if (adj_matrix_[v][i]) {
                result.push_back(i);
            }
        }
        return result;
    }
    
    int degree(int v) const {
        return neighbors(v).size();
    }
    
    // Serialisierung für Subgraph-Binary
    std::string to_json() const;
    
    // Debug
    void print() const;
    
private:
    int num_vertices_ = 0;
    int num_edges_ = 0;
    std::vector<std::vector<bool>> adj_matrix_;
};

/**
 * 3-SAT zu Clique Reduktion
 * 
 * Algorithmus nach Cook (1971):
 * 
 * Gegeben: 3-SAT Instanz mit Variablen x₁,...,xₙ und Clauses C₁,...,Cₘ
 * 
 * Konstruiere Clique-Instanz:
 *   - Knoten: Paare (lit, clause) für jeden Literal in jeder Clause
 *   - Kanten: (lit_i, clause_i) --- (lit_j, clause_j) wenn:
 *     a) i ≠ j (verschiedene Clauses)
 *     b) Keine Konflikt: nicht (lit_i = ¬lit_j)
 * 
 * Clique der Größe m existiert ⟺ 3-SAT Instanz erfüllbar
 */
class ThreeSATToCliqueReducer {
public:
    explicit ThreeSATToCliqueReducer(const CNFFormula& formula)
        : formula_(formula) {}
    
    /**
     * Reduziere 3-SAT zu Clique-Problem
     * 
     * Returns: Pair (G, k) wobei G Graph und k = Clique-Größe
     */
    std::pair<Graph, int> reduce();
    
    /**
     * Validiere Reduction: Verifiziere dass Graph korrekt konstruiert wurde
     */
    bool validate() const;
    
private:
    const CNFFormula& formula_;
    
    /**
     * Intern: Erstelle Knoten für jedes (lit, clause) Paar
     */
    std::vector<std::pair<int, int>> create_nodes() const;
    
    /**
     * Intern: Prüfe ob zwei Literale kompatibel sind
     */
    bool literals_compatible(int lit1, int lit2) const;
};

/**
 * Clique zu Subgraph-Isomorphismus Reduktion
 * 
 * Algorithmus:
 * 
 * Gegeben: Graph G und gesucht: Clique der Größe k
 * 
 * Konstruiere Subgraph-Instanz:
 *   - G_target: Kompletter Graph K_k (Clique als Zielgraph)
 *   - G_source: Das Original-Graph G
 * 
 * Clique der Größe k in G existiert ⟺ K_k ist Subgraph von G
 */
class CliqueToSubgraphReducer {
public:
    CliqueToSubgraphReducer(const Graph& graph, int clique_size)
        : graph_(graph), clique_size_(clique_size) {}
    
    /**
     * Konstruiere Complete Graph (Clique) K_k
     */
    Graph create_target_clique() const;
    
    /**
     * Serialisiere beide Graphen für Subgraph-Binary
     */
    struct SubgraphInstance {
        Graph source;      // G (Originalziel von Clique-Reduktion)
        Graph target;      // K_k (Complete Graph)
        int expected_size; // k (Clique-Größe)
    };
    
    SubgraphInstance create_subgraph_instance() const {
        return SubgraphInstance{
            graph_,
            create_target_clique(),
            clique_size_
        };
    }
    
private:
    const Graph& graph_;
    int clique_size_;
};

/**
 * Complete Pipeline: SAT → 3-SAT → Clique → Subgraph
 * 
 * Orchestriert die gesamte Reduktionskette
 */
class SATToSubgraphReducer {
public:
    explicit SATToSubgraphReducer(const CNFFormula& formula);
    
    /**
     * Führe komplette Reduktion aus
     * 
     * Returns: JSON-Strings für beide Graphen (für Subgraph-Binary)
     */
    struct ReductionResult {
        std::string source_graph_json;  // Original-Graph G
        std::string target_graph_json;  // Complete Graph K_k
        
        int num_vertices = 0;
        int expected_clique_size = 0;
        
        // Statistiken
        int original_num_clauses = 0;
        int original_num_variables = 0;
    };
    
    ReductionResult reduce();
    
    /**
     * Validiere gesamte Reduktion
     */
    bool validate_reduction() const;
    
    // Accessor für Debugging
    const CNFFormula& formula() const { return formula_; }
    const Graph& clique_graph() const { return clique_graph_; }
    int clique_size() const { return clique_size_; }
    
private:
    CNFFormula formula_;
    Graph clique_graph_;
    int clique_size_ = 0;
    
    bool is_reduced_ = false;
};

/**
 * SAT-Lösungs-Extraktion aus Subgraph-Mapping
 * 
 * Wenn Subgraph-Isomorphismus einen Mapping liefert,
 * extrahiere daraus eine SAT-Lösung (Assignment)
 */
class SubgraphToSATExtractor {
public:
    /**
     * Gegeben:
     *   - Originale 3-SAT Instanz
     *   - Mapping der Clique-Knoten auf Graph-Knoten
     * 
     * Extrahiere:
     *   - SAT-Assignment (Variablenbelegung)
     */
    static SATAssignment extract_assignment(
        const CNFFormula& formula_3sat,
        const std::vector<std::pair<int, int>>& node_mapping  // (clause_id, lit_index) -> graph_node
    );
};

} // namespace sat
