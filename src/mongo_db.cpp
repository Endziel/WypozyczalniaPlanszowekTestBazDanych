// MongoDB - model dokumentowy, sterownik C (libmongoc).
//
// Kolekcje:
//   games     - gra z osadzonymi listami: mechaniki, tematy, kategorie, projektanci,
//               wydawcy oraz egzemplarze (copies)
//   customers - klienci
//   rentals   - wypozyczenie z osadzona platnoscia (payment) i zdenormalizowanym game_id
//   ratings   - oceny
#include <mongoc/mongoc.h>

#include <stdexcept>
#include <unordered_map>

#include "database.h"
#include "scenarios.h"

namespace {

const char* kDb = "planszowki";
const char* kCollections[] = {"games", "customers", "rentals", "ratings"};

void appendStrings(bson_t* doc, const char* key, const std::vector<const std::string*>& items) {
    bson_t arr;
    bson_append_array_begin(doc, key, -1, &arr);
    char buf[16];
    const char* k;
    for (uint32_t i = 0; i < items.size(); ++i) {
        bson_uint32_to_string(i, &k, buf, sizeof buf);
        bson_append_utf8(&arr, k, -1, items[i]->c_str(), -1);
    }
    bson_append_array_end(doc, &arr);
}

int64_t ms(int64_t seconds) { return seconds * 1000; }

// Grupuje powiazania slownika wg gry: game_id -> lista nazw.
std::unordered_map<int, std::vector<const std::string*>> byGame(const Dictionary& d) {
    std::unordered_map<int, std::vector<const std::string*>> m;
    for (auto [game, item] : d.links) m[game].push_back(&d.names[item - 1]);
    return m;
}

class MongoDb : public Database {
public:
    ~MongoDb() override {
        if (client_) mongoc_client_destroy(client_);
        mongoc_cleanup();
    }

    std::string name() const override { return "mongodb"; }

    void connect() override {
        mongoc_init();
        client_ = mongoc_client_new(env("MONGO_URI", "mongodb://localhost:27018").c_str());
        if (!client_) throw std::runtime_error("MongoDB: zly URI");
        bson_t* cmd = BCON_NEW("ping", BCON_INT32(1));
        command(cmd);
        bson_destroy(cmd);
    }

    void resetSchema() override {
        mongoc_database_t* db = mongoc_client_get_database(client_, kDb);
        bson_error_t err;
        if (!mongoc_database_drop(db, &err) && err.code != 26 /* NamespaceNotFound */)
            throw std::runtime_error(std::string("MongoDB drop: ") + err.message);
        mongoc_database_destroy(db);
        // Kolekcje powstaja automatycznie przy pierwszym insercie (brak sztywnego schematu).
    }

    void load(const Dataset& ds) override {
        // --- games (z osadzonymi listami i egzemplarzami) ---
        auto mech = byGame(ds.mechanics), theme = byGame(ds.themes), cat = byGame(ds.categories),
             des = byGame(ds.designers), pub = byGame(ds.publishers);
        std::unordered_map<int, std::vector<const Copy*>> copies;
        for (const Copy& c : ds.copies) copies[c.gameId].push_back(&c);
        std::unordered_map<int, int> gameOfCopy;
        for (const Copy& c : ds.copies) gameOfCopy[c.id] = c.gameId;

        insertAll("games", ds.games.size(), [&](size_t i, bson_t* d) {
            const Game& g = ds.games[i];
            BSON_APPEND_INT32(d, "_id", g.id);
            BSON_APPEND_UTF8(d, "name", g.name.c_str());
            BSON_APPEND_INT32(d, "year_published", g.year);
            BSON_APPEND_DOUBLE(d, "weight", g.weight);
            BSON_APPEND_DOUBLE(d, "avg_rating", g.avgRating);
            BSON_APPEND_INT32(d, "min_players", g.minPlayers);
            BSON_APPEND_INT32(d, "max_players", g.maxPlayers);
            BSON_APPEND_INT32(d, "playtime_min", g.playtime);
            BSON_APPEND_INT32(d, "min_age", g.minAge);
            BSON_APPEND_DOUBLE(d, "price_per_day", g.pricePerDay);
            appendStrings(d, "mechanics", mech[g.id]);
            appendStrings(d, "themes", theme[g.id]);
            appendStrings(d, "categories", cat[g.id]);
            appendStrings(d, "designers", des[g.id]);
            appendStrings(d, "publishers", pub[g.id]);
            bson_t arr, item;
            bson_append_array_begin(d, "copies", -1, &arr);
            char buf[16];
            const char* k;
            uint32_t n = 0;
            for (const Copy* c : copies[g.id]) {
                bson_uint32_to_string(n++, &k, buf, sizeof buf);
                bson_append_document_begin(&arr, k, -1, &item);
                BSON_APPEND_INT32(&item, "copy_id", c->id);
                BSON_APPEND_UTF8(&item, "condition", c->condition.c_str());
                BSON_APPEND_UTF8(&item, "status", c->status.c_str());
                BSON_APPEND_DATE_TIME(&item, "purchase_date", ms(c->purchaseDate));
                bson_append_document_end(&arr, &item);
            }
            bson_append_array_end(d, &arr);
        });

        insertAll("customers", ds.customers.size(), [&](size_t i, bson_t* d) {
            const Customer& c = ds.customers[i];
            BSON_APPEND_INT32(d, "_id", c.id);
            BSON_APPEND_UTF8(d, "username", c.username.c_str());
            BSON_APPEND_UTF8(d, "email", c.email.c_str());
            BSON_APPEND_UTF8(d, "city", c.city.c_str());
            BSON_APPEND_DATE_TIME(d, "registered_at", ms(c.registeredAt));
        });

        insertAll("ratings", ds.ratings.size(), [&](size_t i, bson_t* d) {
            const Rating& r = ds.ratings[i];
            BSON_APPEND_INT64(d, "_id", r.id);
            BSON_APPEND_INT32(d, "customer_id", r.customerId);
            BSON_APPEND_INT32(d, "game_id", r.gameId);
            BSON_APPEND_DOUBLE(d, "rating", r.rating);
        });

        std::unordered_map<int64_t, const Payment*> payOf;
        for (const Payment& p : ds.payments) payOf[p.rentalId] = &p;
        insertAll("rentals", ds.rentals.size(), [&](size_t i, bson_t* d) {
            const Rental& r = ds.rentals[i];
            BSON_APPEND_INT64(d, "_id", r.id);
            BSON_APPEND_INT32(d, "copy_id", r.copyId);
            BSON_APPEND_INT32(d, "game_id", gameOfCopy[r.copyId]);
            BSON_APPEND_INT32(d, "customer_id", r.customerId);
            BSON_APPEND_DATE_TIME(d, "rented_at", ms(r.rentedAt));
            BSON_APPEND_DATE_TIME(d, "due_at", ms(r.dueAt));
            if (r.returnedAt) BSON_APPEND_DATE_TIME(d, "returned_at", ms(*r.returnedAt));
            else BSON_APPEND_NULL(d, "returned_at");
            auto it = payOf.find(r.id);
            if (it != payOf.end()) {
                bson_t p;
                BSON_APPEND_DOCUMENT_BEGIN(d, "payment", &p);
                BSON_APPEND_INT64(&p, "payment_id", it->second->id);
                BSON_APPEND_DOUBLE(&p, "amount", it->second->amount);
                BSON_APPEND_DATE_TIME(&p, "paid_at", ms(it->second->paidAt));
                BSON_APPEND_UTF8(&p, "method", it->second->method.c_str());
                bson_append_document_end(d, &p);
            }
        });
    }

    void createIndexes() override {
        createIndex("ratings", "customer_id");
        createIndex("ratings", "game_id");
        createIndex("rentals", "customer_id");
        createIndex("rentals", "copy_id");
        createIndex("rentals", "rented_at");
        createIndex("rentals", "payment.paid_at");
        createIndex("customers", "username");
        createIndex("games", "mechanics");          // indeks wielokluczowy (tablica)
        createIndex("games", "themes");
        createIndex("games", "copies.copy_id");
        createIndex("games", "min_players");
    }

    void dropIndexes() override {
        for (const char* c : kCollections) {
            bson_t* cmd = BCON_NEW("dropIndexes", BCON_UTF8(c), "index", BCON_UTF8("*"));
            command(cmd, /*ignoreErrors=*/true);
            bson_destroy(cmd);
        }
    }

    std::map<std::string, long long> counts() override {
        std::map<std::string, long long> out;
        for (const char* c : kCollections) {
            mongoc_collection_t* coll = mongoc_client_get_collection(client_, kDb, c);
            bson_error_t err;
            bson_t filter = BSON_INITIALIZER;
            out[c] = mongoc_collection_count_documents(coll, &filter, nullptr, nullptr, nullptr, &err);
            mongoc_collection_destroy(coll);
        }
        return out;
    }

    bool runScenario(const std::string& id, const Dataset& ds, int trial) override {
        if (id == "R1") {
            bson_t* filter = BCON_NEW("_id", BCON_INT32(pickGameId(ds, trial)));
            find("games", filter);
            bson_destroy(filter);
            return true;
        }
        return false;   // pozostale scenariusze - kolejny etap
    }

private:
    mongoc_client_t* client_ = nullptr;

    void command(const bson_t* cmd, bool ignoreErrors = false) {
        bson_error_t err;
        bson_t reply;
        bool ok = mongoc_client_command_simple(client_, kDb, cmd, nullptr, &reply, &err);
        bson_destroy(&reply);
        if (!ok && !ignoreErrors) throw std::runtime_error(std::string("MongoDB: ") + err.message);
    }

    void createIndex(const char* coll, const char* field) {
        std::string name = std::string("idx_") + coll + "_" + field;
        bson_t* cmd = BCON_NEW("createIndexes", BCON_UTF8(coll),
                               "indexes", "[", "{", "key", "{", field, BCON_INT32(1), "}",
                                                    "name", BCON_UTF8(name.c_str()), "}", "]");
        command(cmd);
        bson_destroy(cmd);
    }

    int find(const char* coll, const bson_t* filter) {
        mongoc_collection_t* c = mongoc_client_get_collection(client_, kDb, coll);
        mongoc_cursor_t* cur = mongoc_collection_find_with_opts(c, filter, nullptr, nullptr);
        const bson_t* doc;
        int n = 0;
        while (mongoc_cursor_next(cur, &doc)) ++n;
        bson_error_t err;
        bool failed = mongoc_cursor_error(cur, &err);
        mongoc_cursor_destroy(cur);
        mongoc_collection_destroy(c);
        if (failed) throw std::runtime_error(std::string("MongoDB find: ") + err.message);
        return n;
    }

    // Wstawia dokumenty paczkami po 1000 (insert_many).
    template <class Fill>
    void insertAll(const char* coll, size_t count, Fill fill) {
        mongoc_collection_t* c = mongoc_client_get_collection(client_, kDb, coll);
        const size_t batch = 1000;
        std::vector<bson_t*> docs;
        for (size_t start = 0; start < count; start += batch) {
            size_t end = std::min(count, start + batch);
            docs.clear();
            for (size_t i = start; i < end; ++i) {
                bson_t* d = bson_new();
                fill(i, d);
                docs.push_back(d);
            }
            bson_error_t err;
            bool ok = mongoc_collection_insert_many(c, const_cast<const bson_t**>(docs.data()), docs.size(),
                                                    nullptr, nullptr, &err);
            for (bson_t* d : docs) bson_destroy(d);
            if (!ok) {
                mongoc_collection_destroy(c);
                throw std::runtime_error(std::string("MongoDB insert ") + coll + ": " + err.message);
            }
        }
        mongoc_collection_destroy(c);
    }
};

}  // namespace

std::unique_ptr<Database> makeMongo() { return std::make_unique<MongoDb>(); }
