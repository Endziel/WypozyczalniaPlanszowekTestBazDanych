#pragma once
#include <fstream>
#include <string>
#include <vector>

// Prosty czytnik CSV (RFC 4180): przecinki, pola w cudzyslowach, "" jako cudzyslow.
class CsvReader {
public:
    explicit CsvReader(const std::string& path);
    // Wczytuje kolejny wiersz; zwraca false na koncu pliku.
    bool next(std::vector<std::string>& fields);

private:
    std::ifstream in_;
    std::string line_;
};
