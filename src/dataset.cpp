#include "dataset.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "csv.h"

namespace {

int toInt(const std::string& s, int def = 0) {
    if (s.empty()) return def;
    try { return static_cast<int>(std::stod(s)); } catch (...) { return def; }
}

double toDouble(const std::string& s, double def = 0.0) {
    if (s.empty()) return def;
    try { return std::stod(s); } catch (...) { return def; }
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

int findColumn(const std::vector<std::string>& header, const std::string& name) {
    for (size_t i = 0; i < header.size(); ++i)
        if (header[i] == name) return static_cast<int>(i);
    throw std::runtime_error("Brak kolumny " + name);
}

// Zwraca id elementu slownika (dodaje go, jesli jeszcze nie istnieje).
int dictId(Dictionary& d, std::unordered_map<std::string, int>& ids, const std::string& name) {
    auto it = ids.find(name);
    if (it != ids.end()) return it->second;
    d.names.push_back(name);
    int id = static_cast<int>(d.names.size());
    ids.emplace(name, id);
    return id;
}

// Pliki "szerokie": kolumna BGGId + po jednej kolumnie 0/1 na kazdy element
// slownika. Zamieniamy je na zwykla relacje wiele-do-wielu.
void loadWide(const std::string& path, Dictionary& d, std::unordered_map<std::string, int>& ids,
              const std::unordered_set<int>& gameIds, const std::string& stripPrefix = "") {
    CsvReader csv(path);
    std::vector<std::string> header, row;
    csv.next(header);
    int idCol = findColumn(header, "BGGId");

    std::vector<int> colItem(header.size(), 0);   // 0 = kolumna pomijana
    for (size_t i = 0; i < header.size(); ++i) {
        const std::string& h = header[i];
        if (static_cast<int>(i) == idCol || startsWith(h, "Low-Exp")) continue;
        std::string name = (!stripPrefix.empty() && startsWith(h, stripPrefix)) ? h.substr(stripPrefix.size()) : h;
        colItem[i] = dictId(d, ids, name);
    }

    std::unordered_set<int64_t> seen;   // ochrona przed duplikatami po usunieciu prefiksu
    while (csv.next(row)) {
        if (row.size() != header.size()) continue;
        int gameId = toInt(row[idCol], -1);
        if (!gameIds.count(gameId)) continue;
        for (size_t i = 0; i < row.size(); ++i) {
            if (colItem[i] == 0 || row[i] != "1") continue;
            int64_t key = static_cast<int64_t>(gameId) * 100000 + colItem[i];
            if (seen.insert(key).second) d.links.emplace_back(gameId, colItem[i]);
        }
    }
}

size_t countLines(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("Nie mozna otworzyc pliku: " + path);
    std::vector<char> buf(1 << 20);
    size_t n = 0, got;
    while ((got = std::fread(buf.data(), 1, buf.size(), f)) > 0)
        for (const char* p = buf.data(); (p = static_cast<const char*>(std::memchr(p, '\n', buf.data() + got - p))); ++p) ++n;
    std::fclose(f);
    return n;
}

int64_t makeTime(int y, int mon, int d) {
    std::tm tm{};
    tm.tm_year = y - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = d;
    return static_cast<int64_t>(timegm(&tm));
}

double round2(double v) { return std::round(v * 100.0) / 100.0; }

}  // namespace

std::string formatTime(int64_t t) {
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

std::string formatTimeIso(int64_t t) {
    std::string s = formatTime(t);
    s[10] = 'T';
    return s;
}

Dataset Dataset::load(const std::string& dir, size_t size) {
    Dataset ds;
    std::mt19937_64 rng(42);   // stale ziarno = powtarzalne dane

    // --- gry + kategorie glowne (kolumny Cat:* w games.csv) ---
    std::unordered_set<int> gameIds;
    std::unordered_map<std::string, int> catIds;
    {
        CsvReader csv(dir + "/games.csv");
        std::vector<std::string> h, r;
        csv.next(h);
        int cId = findColumn(h, "BGGId"), cName = findColumn(h, "Name"), cYear = findColumn(h, "YearPublished"),
            cWeight = findColumn(h, "GameWeight"), cAvg = findColumn(h, "AvgRating"),
            cMin = findColumn(h, "MinPlayers"), cMax = findColumn(h, "MaxPlayers"),
            cTime = findColumn(h, "MfgPlaytime"), cAge = findColumn(h, "MfgAgeRec");
        std::vector<std::pair<int, int>> catCols;   // (kolumna, id kategorii)
        for (size_t i = 0; i < h.size(); ++i)
            if (startsWith(h[i], "Cat:")) catCols.emplace_back(static_cast<int>(i), dictId(ds.categories, catIds, h[i].substr(4)));

        while (csv.next(r)) {
            if (r.size() != h.size()) continue;
            Game g;
            g.id = toInt(r[cId], -1);
            if (g.id < 0 || !gameIds.insert(g.id).second) continue;
            g.name = r[cName];
            g.year = toInt(r[cYear]);
            g.weight = toDouble(r[cWeight]);
            g.avgRating = toDouble(r[cAvg]);
            g.minPlayers = toInt(r[cMin]);
            g.maxPlayers = toInt(r[cMax]);
            g.playtime = toInt(r[cTime]);
            g.minAge = toInt(r[cAge]);
            g.pricePerDay = round2(5.0 + g.weight * 3.0);   // trudniejsze gry sa drozsze
            ds.games.push_back(std::move(g));
            for (auto [col, catId] : catCols)
                if (r[col] == "1") ds.categories.links.emplace_back(ds.games.back().id, catId);
        }
    }

    // --- slowniki z plikow "szerokich" ---
    std::unordered_map<std::string, int> mechIds, themeIds, desIds, pubIds;
    loadWide(dir + "/mechanics.csv", ds.mechanics, mechIds, gameIds);
    loadWide(dir + "/themes.csv", ds.themes, themeIds, gameIds, "Theme_");
    loadWide(dir + "/subcategories.csv", ds.categories, catIds, gameIds);
    loadWide(dir + "/designers_reduced.csv", ds.designers, desIds, gameIds);
    loadWide(dir + "/publishers_reduced.csv", ds.publishers, pubIds, gameIds);

    // --- oceny: rownomierna probka `size` wierszy z user_ratings.csv ---
    // (plik jest posortowany wg gry, wiec pierwsze N wierszy to tylko kilka gier)
    static const char* cities[] = {"Krakow", "Warszawa", "Wroclaw", "Gdansk", "Poznan", "Lodz", "Katowice", "Lublin", "Rzeszow", "Tarnow"};
    std::unordered_map<std::string, int> customerIds;
    {
        const std::string path = dir + "/user_ratings.csv";
        size_t total = countLines(path) - 1;
        size_t step = std::max<size_t>(1, total / size);
        CsvReader csv(path);
        std::vector<std::string> h, r;
        csv.next(h);
        int cGame = findColumn(h, "BGGId"), cRating = findColumn(h, "Rating"), cUser = findColumn(h, "Username");
        std::uniform_int_distribution<int64_t> regTime(makeTime(2015, 1, 1), makeTime(2021, 12, 31));
        for (size_t line = 0; csv.next(r) && ds.ratings.size() < size; ++line) {
            if (line % step != 0 || r.size() != h.size()) continue;
            int gameId = toInt(r[cGame], -1);
            const std::string& user = r[cUser];
            if (user.empty() || !gameIds.count(gameId)) continue;

            auto it = customerIds.find(user);
            if (it == customerIds.end()) {
                Customer c;
                c.id = static_cast<int>(ds.customers.size()) + 1;
                c.username = user;
                c.email = "user" + std::to_string(c.id) + "@planszowki.example";
                c.city = cities[rng() % 10];
                c.registeredAt = regTime(rng);
                ds.customers.push_back(c);
                it = customerIds.emplace(user, c.id).first;
            }
            ds.ratings.push_back({static_cast<int64_t>(ds.ratings.size()) + 1, it->second, gameId, toDouble(r[cRating])});
        }
    }

    // --- egzemplarze: 1-3 sztuki kazdej gry ---
    static const char* conditions[] = {"new", "good", "worn"};
    std::uniform_int_distribution<int64_t> buyTime(makeTime(2020, 1, 1), makeTime(2021, 12, 31));
    for (const Game& g : ds.games) {
        int n = 1 + static_cast<int>(rng() % 3);
        for (int k = 0; k < n; ++k)
            ds.copies.push_back({static_cast<int>(ds.copies.size()) + 1, g.id, conditions[rng() % 3], "available", buyTime(rng)});
    }

    // --- wypozyczenia + platnosci ---
    std::unordered_map<int, double> priceOf;
    for (const Game& g : ds.games) priceOf[g.id] = g.pricePerDay;
    static const char* methods[] = {"cash", "card", "blik"};
    std::uniform_int_distribution<int64_t> rentTime(makeTime(2022, 1, 1), makeTime(2025, 12, 31));
    std::uniform_int_distribution<size_t> pickCustomer(0, ds.customers.size() - 1), pickCopy(0, ds.copies.size() - 1);
    const int64_t day = 24 * 3600;
    ds.rentals.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        Rental rent;
        rent.id = static_cast<int64_t>(i) + 1;
        rent.customerId = ds.customers[pickCustomer(rng)].id;
        Copy& copy = ds.copies[pickCopy(rng)];
        rent.copyId = copy.id;
        rent.rentedAt = rentTime(rng);
        rent.dueAt = rent.rentedAt + 7 * day;
        if (rng() % 20 == 0) {          // 5% gier jeszcze nie oddanych
            copy.status = "rented";
        } else {
            int days = 1 + static_cast<int>(rng() % 10);
            rent.returnedAt = rent.rentedAt + days * day;
            ds.payments.push_back({static_cast<int64_t>(ds.payments.size()) + 1, rent.id,
                                   round2(days * priceOf[copy.gameId]), *rent.returnedAt, methods[rng() % 3]});
        }
        ds.rentals.push_back(rent);
    }
    return ds;
}

void Dataset::printSummary() const {
    auto line = [](const char* name, size_t n) { std::cout << "  " << name << ": " << n << "\n"; };
    std::cout << "Zbior danych:\n";
    line("games", games.size());
    line("mechanics", mechanics.names.size());     line("game_mechanics", mechanics.links.size());
    line("themes", themes.names.size());           line("game_themes", themes.links.size());
    line("categories", categories.names.size());   line("game_categories", categories.links.size());
    line("designers", designers.names.size());     line("game_designers", designers.links.size());
    line("publishers", publishers.names.size());   line("game_publishers", publishers.links.size());
    line("customers", customers.size());
    line("ratings", ratings.size());
    line("copies", copies.size());
    line("rentals", rentals.size());
    line("payments", payments.size());
}
