#include "csv.h"

#include <stdexcept>

CsvReader::CsvReader(const std::string& path) : in_(path, std::ios::binary) {
    if (!in_) throw std::runtime_error("Nie mozna otworzyc pliku: " + path);
}

bool CsvReader::next(std::vector<std::string>& fields) {
    fields.clear();
    if (!std::getline(in_, line_)) return false;

    std::string field;
    bool quoted = false;
    for (;;) {
        for (size_t i = 0; i < line_.size(); ++i) {
            char c = line_[i];
            if (quoted) {
                if (c == '"') {
                    if (i + 1 < line_.size() && line_[i + 1] == '"') { field += '"'; ++i; }
                    else quoted = false;
                } else {
                    field += c;
                }
            } else if (c == '"') {
                quoted = true;
            } else if (c == ',') {
                fields.push_back(std::move(field));
                field.clear();
            } else if (c != '\r') {
                field += c;
            }
        }
        if (!quoted) break;
        // pole w cudzyslowie zawiera znak nowej linii - doczytaj kolejna linie
        field += '\n';
        if (!std::getline(in_, line_)) break;
    }
    fields.push_back(std::move(field));
    return true;
}
