// Wypozyczalnia gier planszowych - porownanie wydajnosci baz danych.
//
// Uzycie:  ztb_bench [--size N] [--db postgresql,sqlite,mongodb,memgraph] [--trials 3] [--data KATALOG_CSV]
//
// Dla kazdej bazy: czysci schemat -> laduje dane -> scenariusze bez indeksow
// -> tworzy indeksy -> scenariusze z indeksami. Wyniki trafiaja do results/*.csv.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "benchmark.h"
#include "database.h"

namespace {

struct Options {
    size_t size = 10000;
    std::vector<std::string> dbs = {"postgresql", "sqlite", "mongodb", "memgraph"};
    int trials = 3;
    // w Dockerze ustawiane przez DATA_DIR; w WSL dysk C: jest widoczny jako /mnt/c
    std::string dataDir = env("DATA_DIR", "/mnt/c/Studia/Zaawansowane_bazy_danych");
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("Brak wartosci dla " + a);
            return argv[++i];
        };
        if (a == "--size") o.size = std::stoul(value());
        else if (a == "--trials") o.trials = std::stoi(value());
        else if (a == "--data") o.dataDir = value();
        else if (a == "--db") {
            o.dbs.clear();
            std::stringstream ss(value());
            for (std::string s; std::getline(ss, s, ',');) o.dbs.push_back(s);
        } else throw std::runtime_error("Nieznany argument: " + a);
    }
    return o;
}

std::unique_ptr<Database> makeDb(const std::string& name) {
    if (name == "postgresql") return makePostgres();
    if (name == "sqlite") return makeSqlite();
    if (name == "mongodb") return makeMongo();
    if (name == "memgraph") return makeMemgraph();
    throw std::runtime_error("Nieznana baza: " + name);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        Options opt = parseArgs(argc, argv);
#ifndef ZTB_RESULTS_DIR
#define ZTB_RESULTS_DIR "results"
#endif
        const std::string resultsDir = env("RESULTS_DIR", ZTB_RESULTS_DIR);
        std::filesystem::create_directories(resultsDir);
        std::cout << "Katalog wynikow: " << std::filesystem::absolute(resultsDir).string() << "\n";

        std::cout << "Wczytywanie CSV z " << opt.dataDir << " (rozmiar zbioru: " << opt.size << ")...\n";
        Dataset ds;
        double readMs = timeMs([&] { ds = Dataset::load(opt.dataDir, opt.size); });
        std::printf("Wczytano w %.1f s\n", readMs / 1000);
        ds.printSummary();

        std::vector<Measurement> all;
        std::ofstream loadCsv(resultsDir + "/load_times.csv", std::ios::app);

        for (const std::string& name : opt.dbs) {
            std::cout << "\n===== " << name << " =====\n";
            auto db = makeDb(name);
            db->connect();
            db->resetSchema();
            double loadMs = timeMs([&] { db->load(ds); });
            std::printf("Zaladowano dane w %.1f s\n", loadMs / 1000);
            loadCsv << name << ',' << opt.size << ',' << loadMs << '\n';

            for (const auto& [table, n] : db->counts()) std::cout << "  " << table << ": " << n << "\n";

            auto noIdx = runScenarios(*db, ds, opt.size, false, opt.trials);
            db->createIndexes();
            auto withIdx = runScenarios(*db, ds, opt.size, true, opt.trials);
            db->dropIndexes();

            all.insert(all.end(), noIdx.begin(), noIdx.end());
            all.insert(all.end(), withIdx.begin(), withIdx.end());
        }

        appendCsv(resultsDir + "/results.csv", all);
        printSummary(all);
        std::cout << "\nWyniki zapisane w " << resultsDir << "/results.csv\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "BLAD: " << e.what() << "\n";
        return 1;
    }
}
