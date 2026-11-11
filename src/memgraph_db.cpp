// Memgraph - model grafowy (Cypher), klient Bolt mgclient.
//
// Wezly:      Game, Mechanic, Theme, Category, Designer, Publisher,
//             Customer, Copy, Rental, Payment
// Krawedzie:  (Game)-[:HAS_MECHANIC]->(Mechanic)    (Game)-[:HAS_THEME]->(Theme)
//             (Game)-[:IN_CATEGORY]->(Category)     (Game)-[:DESIGNED_BY]->(Designer)
//             (Game)-[:PUBLISHED_BY]->(Publisher)   (Copy)-[:COPY_OF]->(Game)
//             (Customer)-[:RATED {rating}]->(Game)
//             (Customer)-[:MADE]->(Rental)-[:FOR_COPY]->(Copy)
//             (Rental)-[:PAID_BY]->(Payment)
#include <mgclient.h>

#include <functional>
#include <stdexcept>

#include "database.h"
#include "scenarios.h"

namespace {

const char* kLabels[] = {"Game", "Mechanic", "Theme", "Category", "Designer", "Publisher",
                         "Customer", "Copy", "Rental", "Payment"};
const char* kEdges[] = {"HAS_MECHANIC", "HAS_THEME", "IN_CATEGORY", "DESIGNED_BY", "PUBLISHED_BY",
                        "COPY_OF", "RATED", "MADE", "FOR_COPY", "PAID_BY"};

// Indeksy pomocnicze (wariant "z indeksami"). Indeksy na :Label(id) sa zawsze -
// pelnia role kluczy glownych i sa potrzebne do laczenia wezlow przy ladowaniu.
const char* kSecondaryIndexes[] = {":Customer(username)", ":Rental(rented_at)", ":Payment(paid_at)",
                                   ":Game(min_players)", ":Mechanic(name)", ":Theme(name)", ":Category(name)"};

void put(mg_map* m, const char* k, int64_t v) { mg_map_insert(m, k, mg_value_make_integer(v)); }
void put(mg_map* m, const char* k, double v) { mg_map_insert(m, k, mg_value_make_float(v)); }
void put(mg_map* m, const char* k, const std::string& v) { mg_map_insert(m, k, mg_value_make_string(v.c_str())); }
void putNull(mg_map* m, const char* k) { mg_map_insert(m, k, mg_value_make_null()); }

class MemgraphDb : public Database {
public:
    ~MemgraphDb() override {
        if (session_) mg_session_destroy(session_);
        mg_finalize();
    }

    std::string name() const override { return "memgraph"; }

    void connect() override {
        mg_init();
        mg_session_params* p = mg_session_params_make();
        std::string host = env("MEMGRAPH_HOST", "localhost");
        mg_session_params_set_host(p, host.c_str());
        mg_session_params_set_port(p, static_cast<uint16_t>(std::stoi(env("MEMGRAPH_PORT", "7688"))));
        mg_session_params_set_sslmode(p, MG_SSLMODE_DISABLE);
        int rc = mg_connect(p, &session_);
        mg_session_params_destroy(p);
        if (rc != 0) throw std::runtime_error(std::string("Memgraph: ") + mg_session_error(session_));
    }

    void resetSchema() override {
        // DROP GRAPH usuwa wszystkie dane i indeksy (wymaga trybu analitycznego).
        run("STORAGE MODE IN_MEMORY_ANALYTICAL");
        run("DROP GRAPH");
        run("STORAGE MODE IN_MEMORY_TRANSACTIONAL");
        for (const char* l : kLabels) run(std::string("CREATE INDEX ON :") + l + "(id)");
    }

    void load(const Dataset& ds) override {
        // Ladowanie w trybie analitycznym (bez transakcji) jest wielokrotnie szybsze.
        run("STORAGE MODE IN_MEMORY_ANALYTICAL");

        unwind("CREATE (:Game {id: r.id, name: r.name, year_published: r.year, weight: r.weight, "
               "avg_rating: r.avg, min_players: r.minp, max_players: r.maxp, playtime_min: r.time, "
               "min_age: r.age, price_per_day: r.price})",
               ds.games.size(), 10, [&](size_t i, mg_map* m) {
                   const Game& g = ds.games[i];
                   put(m, "id", int64_t{g.id});           put(m, "name", g.name);
                   put(m, "year", int64_t{g.year});        put(m, "weight", g.weight);
                   put(m, "avg", g.avgRating);             put(m, "minp", int64_t{g.minPlayers});
                   put(m, "maxp", int64_t{g.maxPlayers});  put(m, "time", int64_t{g.playtime});
                   put(m, "age", int64_t{g.minAge});       put(m, "price", g.pricePerDay);
               });

        loadDictionary("Mechanic", "HAS_MECHANIC", ds.mechanics);
        loadDictionary("Theme", "HAS_THEME", ds.themes);
        loadDictionary("Category", "IN_CATEGORY", ds.categories);
        loadDictionary("Designer", "DESIGNED_BY", ds.designers);
        loadDictionary("Publisher", "PUBLISHED_BY", ds.publishers);

        unwind("CREATE (:Customer {id: r.id, username: r.username, email: r.email, city: r.city, "
               "registered_at: localDateTime(r.reg)})",
               ds.customers.size(), 5, [&](size_t i, mg_map* m) {
                   const Customer& c = ds.customers[i];
                   put(m, "id", int64_t{c.id});  put(m, "username", c.username);
                   put(m, "email", c.email);     put(m, "city", c.city);
                   put(m, "reg", formatTimeIso(c.registeredAt));
               });

        unwind("MATCH (g:Game {id: r.game}) CREATE (:Copy {id: r.id, condition: r.cond, status: r.status, "
               "purchase_date: localDateTime(r.bought)})-[:COPY_OF]->(g)",
               ds.copies.size(), 5, [&](size_t i, mg_map* m) {
                   const Copy& c = ds.copies[i];
                   put(m, "id", int64_t{c.id});  put(m, "game", int64_t{c.gameId});
                   put(m, "cond", c.condition);  put(m, "status", c.status);
                   put(m, "bought", formatTimeIso(c.purchaseDate));
               });

        unwind("MATCH (c:Customer {id: r.c}), (g:Game {id: r.g}) CREATE (c)-[:RATED {id: r.id, rating: r.rating}]->(g)",
               ds.ratings.size(), 4, [&](size_t i, mg_map* m) {
                   const Rating& x = ds.ratings[i];
                   put(m, "id", x.id);  put(m, "c", int64_t{x.customerId});
                   put(m, "g", int64_t{x.gameId});  put(m, "rating", x.rating);
               });

        unwind("MATCH (c:Customer {id: r.c}), (k:Copy {id: r.k}) "
               "CREATE (c)-[:MADE]->(:Rental {id: r.id, rented_at: localDateTime(r.rented), "
               "due_at: localDateTime(r.due), "
               "returned_at: CASE WHEN r.returned IS NULL THEN null ELSE localDateTime(r.returned) END})-[:FOR_COPY]->(k)",
               ds.rentals.size(), 6, [&](size_t i, mg_map* m) {
                   const Rental& x = ds.rentals[i];
                   put(m, "id", x.id);  put(m, "c", int64_t{x.customerId});  put(m, "k", int64_t{x.copyId});
                   put(m, "rented", formatTimeIso(x.rentedAt));  put(m, "due", formatTimeIso(x.dueAt));
                   if (x.returnedAt) put(m, "returned", formatTimeIso(*x.returnedAt));
                   else putNull(m, "returned");
               });

        unwind("MATCH (x:Rental {id: r.rental}) CREATE (x)-[:PAID_BY]->(:Payment {id: r.id, amount: r.amount, "
               "paid_at: localDateTime(r.paid), method: r.method})",
               ds.payments.size(), 5, [&](size_t i, mg_map* m) {
                   const Payment& p = ds.payments[i];
                   put(m, "id", p.id);  put(m, "rental", p.rentalId);  put(m, "amount", p.amount);
                   put(m, "paid", formatTimeIso(p.paidAt));  put(m, "method", p.method);
               });

        run("STORAGE MODE IN_MEMORY_TRANSACTIONAL");
    }

    void createIndexes() override {
        for (const char* idx : kSecondaryIndexes) run(std::string("CREATE INDEX ON ") + idx);
    }

    void dropIndexes() override {
        for (const char* idx : kSecondaryIndexes) {
            try { run(std::string("DROP INDEX ON ") + idx); } catch (const std::exception&) { /* nie istnial */ }
        }
    }

    std::map<std::string, long long> counts() override {
        std::map<std::string, long long> out;
        for (const char* l : kLabels) out[std::string("(:") + l + ")"] = scalar(std::string("MATCH (n:") + l + ") RETURN count(n)");
        for (const char* e : kEdges) out[std::string("[:") + e + "]"] = scalar(std::string("MATCH ()-[r:") + e + "]->() RETURN count(r)");
        return out;
    }

    bool runScenario(const std::string& id, const Dataset& ds, int trial) override {
        if (id == "R1") {
            mg_map* p = mg_map_make_empty(1);
            put(p, "id", int64_t{pickGameId(ds, trial)});
            run("MATCH (g:Game {id: $id}) RETURN g", p);
            mg_map_destroy(p);
            return true;
        }
        return false;   // pozostale scenariusze - kolejny etap
    }

private:
    mg_session* session_ = nullptr;

    // Wykonuje zapytanie i pobiera wszystkie wiersze wyniku. Zwraca liczbe wierszy;
    // jesli `first` != nullptr, zapisuje tam pierwsza kolumne pierwszego wiersza.
    int run(const std::string& q, const mg_map* params = nullptr, int64_t* first = nullptr) {
        if (mg_session_run(session_, q.c_str(), params, nullptr, nullptr, nullptr) < 0 ||
            mg_session_pull(session_, nullptr) < 0)
            throw std::runtime_error("Memgraph (" + q.substr(0, 60) + "): " + mg_session_error(session_));
        mg_result* res;
        int st, rows = 0;
        while ((st = mg_session_fetch(session_, &res)) == 1) {
            if (first && rows == 0) *first = mg_value_integer(mg_list_at(mg_result_row(res), 0));
            ++rows;
        }
        if (st < 0) throw std::runtime_error("Memgraph (" + q.substr(0, 60) + "): " + mg_session_error(session_));
        return rows;
    }

    long long scalar(const std::string& q) {
        int64_t v = 0;
        run(q, nullptr, &v);
        return v;
    }

    // UNWIND $rows AS r <body> - ladowanie paczkami po 5000 wierszy.
    void unwind(const std::string& body, size_t count, uint32_t fields,
                const std::function<void(size_t, mg_map*)>& fill) {
        const size_t batch = 5000;
        const std::string q = "UNWIND $rows AS r " + body;
        for (size_t start = 0; start < count; start += batch) {
            size_t end = std::min(count, start + batch);
            mg_list* list = mg_list_make_empty(static_cast<uint32_t>(end - start));
            for (size_t i = start; i < end; ++i) {
                mg_map* m = mg_map_make_empty(fields);
                fill(i, m);
                mg_list_append(list, mg_value_make_map(m));
            }
            mg_map* params = mg_map_make_empty(1);
            mg_map_insert(params, "rows", mg_value_make_list(list));
            try { run(q, params); } catch (...) { mg_map_destroy(params); throw; }
            mg_map_destroy(params);
        }
    }

    void loadDictionary(const std::string& label, const std::string& edge, const Dictionary& d) {
        unwind("CREATE (:" + label + " {id: r.id, name: r.name})", d.names.size(), 2, [&](size_t i, mg_map* m) {
            put(m, "id", static_cast<int64_t>(i + 1));
            put(m, "name", d.names[i]);
        });
        unwind("MATCH (g:Game {id: r.g}), (x:" + label + " {id: r.x}) CREATE (g)-[:" + edge + "]->(x)",
               d.links.size(), 2, [&](size_t i, mg_map* m) {
                   put(m, "g", int64_t{d.links[i].first});
                   put(m, "x", int64_t{d.links[i].second});
               });
    }
};

}  // namespace

std::unique_ptr<Database> makeMemgraph() { return std::make_unique<MemgraphDb>(); }
