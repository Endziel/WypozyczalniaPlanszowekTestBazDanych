#pragma once
#include <chrono>
#include <string>
#include <vector>

#include "database.h"

struct Measurement {
    std::string db;
    size_t size;
    bool indexed;
    std::string scenario;
    char crud;
    int trial;
    double ms;
};

// Mierzy czas funkcji w milisekundach.
template <class F>
double timeMs(F&& f) {
    auto t0 = std::chrono::steady_clock::now();
    f();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// Uruchamia wszystkie scenariusze `trials` razy (+1 proba rozgrzewkowa, nieliczona).
std::vector<Measurement> runScenarios(Database& db, const Dataset& ds, size_t size, bool indexed, int trials);

// Dopisuje wyniki do pliku CSV (naglowek tworzony, gdy plik jest pusty).
void appendCsv(const std::string& path, const std::vector<Measurement>& rows);

// Tabelka ze srednimi w konsoli.
void printSummary(const std::vector<Measurement>& rows);
