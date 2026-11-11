#include "relational.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

#include "database.h"

namespace {

std::string num(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

std::string money(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}

TableView dictTable(const std::string& name, const std::string& idCol, const Dictionary& d) {
    return {name, {idCol, "name"}, d.names.size(), [&d](size_t i, SqlRow& r) {
                r = {std::to_string(i + 1), d.names[i]};
            }};
}

TableView linkTable(const std::string& name, const std::string& idCol, const Dictionary& d) {
    return {name, {"game_id", idCol}, d.links.size(), [&d](size_t i, SqlRow& r) {
                r = {std::to_string(d.links[i].first), std::to_string(d.links[i].second)};
            }};
}

}  // namespace

std::string env(const char* name, const std::string& def) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : def;
}

std::vector<TableView> relationalTables(const Dataset& ds) {
    std::vector<TableView> t;
    t.push_back({"games",
                 {"game_id", "name", "year_published", "weight", "avg_rating", "min_players", "max_players",
                  "playtime_min", "min_age", "price_per_day"},
                 ds.games.size(), [&ds](size_t i, SqlRow& r) {
                     const Game& g = ds.games[i];
                     r = {std::to_string(g.id), g.name, std::to_string(g.year), num(g.weight), num(g.avgRating),
                          std::to_string(g.minPlayers), std::to_string(g.maxPlayers), std::to_string(g.playtime),
                          std::to_string(g.minAge), money(g.pricePerDay)};
                 }});
    t.push_back(dictTable("mechanics", "mechanic_id", ds.mechanics));
    t.push_back(dictTable("themes", "theme_id", ds.themes));
    t.push_back(dictTable("categories", "category_id", ds.categories));
    t.push_back(dictTable("designers", "designer_id", ds.designers));
    t.push_back(dictTable("publishers", "publisher_id", ds.publishers));
    t.push_back(linkTable("game_mechanics", "mechanic_id", ds.mechanics));
    t.push_back(linkTable("game_themes", "theme_id", ds.themes));
    t.push_back(linkTable("game_categories", "category_id", ds.categories));
    t.push_back(linkTable("game_designers", "designer_id", ds.designers));
    t.push_back(linkTable("game_publishers", "publisher_id", ds.publishers));
    t.push_back({"customers", {"customer_id", "username", "email", "city", "registered_at"}, ds.customers.size(),
                 [&ds](size_t i, SqlRow& r) {
                     const Customer& c = ds.customers[i];
                     r = {std::to_string(c.id), c.username, c.email, c.city, formatTime(c.registeredAt)};
                 }});
    t.push_back({"ratings", {"rating_id", "customer_id", "game_id", "rating"}, ds.ratings.size(),
                 [&ds](size_t i, SqlRow& r) {
                     const Rating& x = ds.ratings[i];
                     r = {std::to_string(x.id), std::to_string(x.customerId), std::to_string(x.gameId), num(x.rating)};
                 }});
    t.push_back({"copies", {"copy_id", "game_id", "condition", "status", "purchase_date"}, ds.copies.size(),
                 [&ds](size_t i, SqlRow& r) {
                     const Copy& c = ds.copies[i];
                     r = {std::to_string(c.id), std::to_string(c.gameId), c.condition, c.status, formatTime(c.purchaseDate)};
                 }});
    t.push_back({"rentals", {"rental_id", "copy_id", "customer_id", "rented_at", "due_at", "returned_at"},
                 ds.rentals.size(), [&ds](size_t i, SqlRow& r) {
                     const Rental& x = ds.rentals[i];
                     r = {std::to_string(x.id), std::to_string(x.copyId), std::to_string(x.customerId),
                          formatTime(x.rentedAt), formatTime(x.dueAt),
                          x.returnedAt ? SqlValue(formatTime(*x.returnedAt)) : std::nullopt};
                 }});
    t.push_back({"payments", {"payment_id", "rental_id", "amount", "paid_at", "method"}, ds.payments.size(),
                 [&ds](size_t i, SqlRow& r) {
                     const Payment& p = ds.payments[i];
                     r = {std::to_string(p.id), std::to_string(p.rentalId), money(p.amount), formatTime(p.paidAt), p.method};
                 }});
    return t;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Nie mozna otworzyc pliku: " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

#ifndef ZTB_SQL_DIR
#define ZTB_SQL_DIR "sql"
#endif

std::string sqlDir() { return env("SQL_DIR", ZTB_SQL_DIR); }

std::vector<std::string> indexNames() {
    std::string text = readFile(sqlDir() + "/indexes.sql");
    std::regex re(R"(CREATE\s+INDEX\s+(\w+))", std::regex::icase);
    std::vector<std::string> names;
    for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) names.push_back((*it)[1]);
    return names;
}
