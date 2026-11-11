#include "benchmark.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>

#include "scenarios.h"

std::vector<Measurement> runScenarios(Database& db, const Dataset& ds, size_t size, bool indexed, int trials) {
    std::vector<Measurement> out;
    for (const Scenario& s : allScenarios()) {
        // proba 0 = rozgrzewka (cache, plany zapytan) - nie zapisujemy jej
        bool implemented = db.runScenario(s.id, ds, 0);
        if (!implemented) continue;
        for (int t = 1; t <= trials; ++t) {
            double ms = timeMs([&] { db.runScenario(s.id, ds, t); });
            out.push_back({db.name(), size, indexed, s.id, s.crud, t, ms});
        }
    }
    return out;
}

void appendCsv(const std::string& path, const std::vector<Measurement>& rows) {
    bool empty;
    {
        std::ifstream in(path);
        empty = !in || in.peek() == std::ifstream::traits_type::eof();
    }
    std::ofstream f(path, std::ios::app);
    if (empty) f << "db,size,indexes,scenario,crud,trial,ms\n";
    for (const auto& m : rows)
        f << m.db << ',' << m.size << ',' << (m.indexed ? "yes" : "no") << ',' << m.scenario << ',' << m.crud << ','
          << m.trial << ',' << m.ms << '\n';
}

void printSummary(const std::vector<Measurement>& rows) {
    // klucz: db | indeksy | scenariusz -> lista czasow
    std::map<std::string, std::vector<double>> groups;
    for (const auto& m : rows) groups[m.db + (m.indexed ? " | idx " : " | --- ") + "| " + m.scenario].push_back(m.ms);

    std::printf("\n%-30s %10s %10s\n", "baza | indeksy | scenariusz", "srednia ms", "odch. std");
    for (const auto& [key, v] : groups) {
        double avg = 0, var = 0;
        for (double x : v) avg += x;
        avg /= v.size();
        for (double x : v) var += (x - avg) * (x - avg);
        double sd = v.size() > 1 ? std::sqrt(var / (v.size() - 1)) : 0.0;
        std::printf("%-30s %10.3f %10.3f\n", key.c_str(), avg, sd);
    }
}
