#pragma once
#include <map>
#include <memory>
#include <string>

#include "dataset.h"

// Wspolny interfejs dla wszystkich testowanych baz.
// Kazda baza implementuje te same operacje, wiec benchmark jest jeden.
class Database {
public:
    virtual ~Database() = default;

    virtual std::string name() const = 0;
    virtual void connect() = 0;
    virtual void resetSchema() = 0;                  // usuwa wszystko i tworzy pusty schemat
    virtual void load(const Dataset& ds) = 0;        // laduje caly zbior danych
    virtual void createIndexes() = 0;                // indeksy pomocnicze (wariant "z indeksami")
    virtual void dropIndexes() = 0;
    virtual std::map<std::string, long long> counts() = 0;   // liczba rekordow w tabelach/kolekcjach

    // Wykonuje jeden scenariusz testowy (np. "R1"). Zwraca false, jesli
    // scenariusz nie jest jeszcze zaimplementowany dla tej bazy.
    virtual bool runScenario(const std::string& id, const Dataset& ds, int trial) = 0;
};

std::unique_ptr<Database> makePostgres();
std::unique_ptr<Database> makeSqlite();
std::unique_ptr<Database> makeMongo();
std::unique_ptr<Database> makeMemgraph();

// Odczyt zmiennej srodowiskowej z wartoscia domyslna.
std::string env(const char* name, const std::string& def);
