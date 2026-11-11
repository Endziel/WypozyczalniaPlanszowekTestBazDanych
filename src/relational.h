#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "dataset.h"

// Wspolne elementy dla PostgreSQL i SQLite (ten sam schemat SQL).

using SqlValue = std::optional<std::string>;   // nullopt = NULL
using SqlRow = std::vector<SqlValue>;

// Widok jednej tabeli na dane z Dataset - bez kopiowania calych danych.
struct TableView {
    std::string name;
    std::vector<std::string> columns;
    size_t rows;
    std::function<void(size_t, SqlRow&)> fill;   // wypelnia i-ty wiersz
};

// Tabele w kolejnosci ladowania (najpierw rodzice, potem tabele z kluczami obcymi).
std::vector<TableView> relationalTables(const Dataset& ds);

std::string readFile(const std::string& path);
std::string sqlDir();
// Nazwy indeksow zdefiniowanych w sql/indexes.sql
std::vector<std::string> indexNames();
