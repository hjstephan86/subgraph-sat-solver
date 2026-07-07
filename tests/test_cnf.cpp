/**
 * tests/test_cnf.cpp
 * 
 * Unit Tests für CNF Parser und Reduktionen
 */

#include "cnf.h"
#include "reduction.h"
#include <iostream>
#include <sstream>
#include <cassert>

#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace sat;

// =================================================================
// Test Framework Macros
// =================================================================

int test_count = 0;
int passed_count = 0;
int failed_count = 0;

#define TEST(name) \
    { test_count++; std::cout << "Test " << test_count << ": " << name << " ... "; \
        try {

#define PASS() \
        std::cout << "PASS\n"; passed_count++; } \
        catch (const std::exception& e) { \
            std::cout << "FAIL (" << e.what() << ")\n"; failed_count++; } }

#define ASSERT(condition) \
    if (!(condition)) throw std::logic_error("Assertion failed: " #condition)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::logic_error("Assertion failed: " #a " == " #b)


// =================================================================
// Helper Functions
// =================================================================

std::string get_test_path(const std::string& filename) {
#ifdef _WIN32
    char temp_path[MAX_PATH];
    DWORD path_len = GetTempPathA(MAX_PATH, temp_path);
    if (path_len > 0) {
        return std::string(temp_path) + filename;
    }
    return ".\\" + filename; // Fallback: aktuelles Verzeichnis
#else
    return get_test_path(filename); // Linux / macOS Standard
#endif
}

// =================================================================
// Test Cases
// =================================================================

void test_parse_simple_cnf() {
    TEST("Parse simple 2-SAT formula")
    
    std::string dimacs = 
        "c Simple 2-SAT\n"
        "p cnf 2 2\n"
        "1 2 0\n"
        "-1 -2 0\n";
    
    std::istringstream iss(dimacs);
    std::ofstream temp_file(get_test_path("test_simple.cnf"));
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(get_test_path("test_simple.cnf"));
    
    ASSERT_EQ(formula.num_variables(), 2);
    ASSERT_EQ(formula.num_clauses(), 2);
    
    PASS()
}

void test_parse_3sat() {
    TEST("Parse 3-SAT formula")
    
    std::string dimacs = 
        "c 3-SAT example\n"
        "p cnf 3 2\n"
        "1 -3 2 0\n"
        "2 3 -1 0\n";
    
    std::ofstream temp_file(get_test_path("test_3sat.cnf"));
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(get_test_path("test_3sat.cnf"));
    
    ASSERT_EQ(formula.num_variables(), 3);
    ASSERT_EQ(formula.num_clauses(), 2);
    ASSERT(formula.is_valid_3sat());
    
    PASS()
}

void test_to_3sat_conversion() {
    TEST("Convert k-SAT to 3-SAT")
    
    std::string dimacs = 
        "c 4-SAT formula\n"
        "p cnf 4 1\n"
        "1 2 3 4 0\n";
    
    std::ofstream temp_file(get_test_path("test_4sat.cnf"));
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(get_test_path("test_4sat.cnf"));
    
    ASSERT(!formula.is_valid_3sat());
    
    formula.to_3sat();
    
    ASSERT(formula.is_valid_3sat());
    ASSERT(formula.num_clauses() >= 2);  // Sollte in mindestens 2 Clauses aufgeteilt sein
    
    PASS()
}

void test_simplify() {
    TEST("Simplify CNF (remove tautologies)")
    
    // CNF mit Tautologie (a ∨ ¬a)
    std::string dimacs = 
        "c CNF with tautology\n"
        "p cnf 2 3\n"
        "1 -1 0\n"
        "1 2 0\n"
        "-1 -2 0\n";
    
    std::ofstream temp_file(get_test_path("test_simplify.cnf"));
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(get_test_path("test_simplify.cnf"));
    
    int clauses_before = formula.num_clauses();
    formula.simplify();
    int clauses_after = formula.num_clauses();
    
    ASSERT(clauses_after <= clauses_before);
    
    PASS()
}

void test_sat_assignment() {
    TEST("Create and evaluate SAT assignment")
    
    SATAssignment assignment(3);
    
    assignment.set(1, true);
    assignment.set(2, false);
    assignment.set(3, true);
    
    ASSERT(assignment.is_assigned(1));
    ASSERT(assignment.value(1) == true);
    ASSERT(assignment.value(2) == false);
    
    // Test literal evaluation
    ASSERT(assignment.evaluate_literal(1));    // x1 = true
    ASSERT(!assignment.evaluate_literal(-1));  // ¬x1 = false
    ASSERT(!assignment.evaluate_literal(2));   // x2 = false
    ASSERT(assignment.evaluate_literal(-2));   // ¬x2 = true
    
    PASS()
}

void test_clause_evaluation() {
    TEST("Evaluate clauses under assignment")
    
    SATAssignment assignment(3);
    assignment.set(1, true);
    assignment.set(2, true);
    
    // Clause: (x1 ∨ ¬x3) sollte erfüllt sein (x1=true)
    Clause c1 = {1, -3};
    ASSERT(assignment.evaluate_clause(c1));
    
    // Clause: (¬x1 ∨ ¬x2) sollte nicht erfüllt sein
    // (beide Variablen sind true)
    Clause c2 = {-1, -2};
    ASSERT(!assignment.evaluate_clause(c2));
    
    PASS()
}

void test_graph_construction() {
    TEST("Construct and query graph")
    
    Graph g(4);
    
    g.add_edge(0, 1);
    g.add_edge(0, 2);
    g.add_edge(1, 3);
    
    ASSERT_EQ(g.num_vertices(), 4);
    ASSERT_EQ(g.num_edges(), 3);
    
    ASSERT(g.has_edge(0, 1));
    ASSERT(g.has_edge(1, 0));  // Undirected
    ASSERT(!g.has_edge(0, 3));
    
    ASSERT_EQ(g.degree(0), 2);
    ASSERT_EQ(g.degree(1), 2);
    ASSERT_EQ(g.degree(3), 1);
    
    PASS()
}

void test_complete_graph() {
    TEST("Construct complete graph K_3")
    
    Graph complete(3);
    
    // Füge alle Kanten ein
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            complete.add_edge(i, j);
        }
    }
    
    ASSERT_EQ(complete.num_vertices(), 3);
    ASSERT_EQ(complete.num_edges(), 3);  // K_3 hat 3 Kanten
    
    // Alle Knoten sollten Grad 2 haben
    ASSERT_EQ(complete.degree(0), 2);
    ASSERT_EQ(complete.degree(1), 2);
    ASSERT_EQ(complete.degree(2), 2);
    
    PASS()
}

void test_3sat_to_clique_reduction() {
    TEST("Reduce 3-SAT to Clique")
    
    std::string dimacs = 
        "c 3-SAT for clique reduction\n"
        "p cnf 2 2\n"
        "1 2 -1 0\n"
        "-1 2 -2 0\n";
    
    std::ofstream temp_file(get_test_path("test_clique.cnf"));
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(get_test_path("test_clique.cnf"));
    ThreeSATToCliqueReducer reducer(formula);
    
    auto [clique_graph, clique_size] = reducer.reduce();
    
    // Graph sollte Knoten für alle Literale haben
    ASSERT(clique_graph.num_vertices() > 0);
    
    // Clique-Größe sollte Anzahl Clauses sein
    ASSERT_EQ(clique_size, formula.num_clauses());
    
    PASS()
}

void test_json_serialization() {
    TEST("Serialize graph to JSON")
    
    Graph g(2);
    g.add_edge(0, 1);
    
    std::string json = g.to_json();
    
    ASSERT(json.find("\"nodes\"") != std::string::npos);
    ASSERT(json.find("\"edges\"") != std::string::npos);
    ASSERT(json.find("\"v0\"") != std::string::npos);
    ASSERT(json.find("\"v1\"") != std::string::npos);
    
    PASS()
}

// =================================================================
// Main Test Runner
// =================================================================

int main() {
    std::cout << "\n╔════════════════════════════════════════════╗\n";
    std::cout << "║  SAT-Solver Unit Tests                   ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n\n";
    
    // Run all tests
    test_parse_simple_cnf();
    test_parse_3sat();
    test_to_3sat_conversion();
    test_simplify();
    test_sat_assignment();
    test_clause_evaluation();
    test_graph_construction();
    test_complete_graph();
    test_3sat_to_clique_reduction();
    test_json_serialization();
    
    // Summary
    std::cout << "\n╔════════════════════════════════════════════╗\n";
    std::cout << "║  Test Summary                            ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n";
    std::cout << "Total tests: " << test_count << "\n";
    std::cout << "Passed: " << passed_count << "\n";
    std::cout << "Failed: " << failed_count << "\n";
    
    if (failed_count == 0) {
        std::cout << "\n✓ All tests passed!\n\n";
        return 0;
    } else {
        std::cout << "\n✗ Some tests failed!\n\n";
        return 1;
    }
}
