// SQLite - model relacyjny, baza w pliku, biblioteka sqlite3.
#include <sqlite3.h>

#include <cstdio>
#include <stdexcept>

#include "database.h"
#include "relational.h"
#include "scenarios.h"

namespace {

class SqliteDb : public Database {
public:
    ~SqliteDb() override { if (db_) sqlite3_close(db_); }

    std::string name() const override { return "sqlite"; }

    void connect() override { open(); }

    void resetSchema() override {
        // Najprostszy "DROP DATABASE" w SQLite - usuniecie pliku.
        sqlite3_close(db_);
        db_ = nullptr;
        std::remove(path().c_str());
        std::remove((path() + "-wal").c_str());
        std::remove((path() + "-shm").c_str());
        open();
        exec(readFile(sqlDir() + "/schema.sql"));
    }

    void load(const Dataset& ds) override {
        exec("BEGIN");
        for (const TableView& t : relationalTables(ds)) insertTable(t);
        exec("COMMIT");
        exec("ANALYZE");
    }

    void createIndexes() override {
        exec(readFile(sqlDir() + "/indexes.sql"));
        exec("ANALYZE");
    }

    void dropIndexes() override {
        for (const std::string& idx : indexNames()) exec("DROP INDEX IF EXISTS " + idx);
    }

    std::map<std::string, long long> counts() override {
        std::map<std::string, long long> out;
        Dataset empty;
        for (const TableView& t : relationalTables(empty)) {
            sqlite3_stmt* st = prepare("SELECT count(*) FROM " + t.name);
            sqlite3_step(st);
            out[t.name] = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
        return out;
    }

    bool runScenario(const std::string& id, const Dataset& ds, int trial) override {
        if (id == "R1") {
            sqlite3_stmt* st = prepare("SELECT * FROM games WHERE game_id = ?");
            sqlite3_bind_int(st, 1, pickGameId(ds, trial));
            stepAll(st);
            return true;
        }
        return false;   // pozostale scenariusze - kolejny etap
    }

private:
    sqlite3* db_ = nullptr;

    // Domyslnie /tmp - dysk Linuksa. Plik na /mnt/c (dysk Windows) bylby wielokrotnie
    // wolniejszy i zafalszowalby porownanie z pozostalymi bazami.
    static std::string path() { return env("SQLITE_PATH", "/tmp/ztb_planszowki.sqlite"); }

    void open() {
        if (sqlite3_open(path().c_str(), &db_) != SQLITE_OK)
            throw std::runtime_error("SQLite: nie mozna otworzyc " + path());
        exec("PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;");
    }

    void exec(const std::string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "?";
            sqlite3_free(err);
            throw std::runtime_error("SQLite (" + sql.substr(0, 60) + "): " + msg);
        }
    }

    sqlite3_stmt* prepare(const std::string& sql) {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
            throw std::runtime_error("SQLite (" + sql.substr(0, 60) + "): " + sqlite3_errmsg(db_));
        return st;
    }

    // Wykonuje zapytanie i odczytuje wszystkie wiersze wyniku.
    int stepAll(sqlite3_stmt* st) {
        int rows = 0, rc;
        while ((rc = sqlite3_step(st)) == SQLITE_ROW) ++rows;
        sqlite3_finalize(st);
        if (rc != SQLITE_DONE) throw std::runtime_error(std::string("SQLite: ") + sqlite3_errmsg(db_));
        return rows;
    }

    void insertTable(const TableView& t) {
        std::string cols, marks;
        for (const auto& c : t.columns) {
            cols += (cols.empty() ? "" : ",") + c;
            marks += marks.empty() ? "?" : ",?";
        }
        sqlite3_stmt* st = prepare("INSERT INTO " + t.name + " (" + cols + ") VALUES (" + marks + ")");
        SqlRow row;
        for (size_t i = 0; i < t.rows; ++i) {
            t.fill(i, row);
            for (size_t c = 0; c < row.size(); ++c) {
                if (row[c]) sqlite3_bind_text(st, static_cast<int>(c) + 1, row[c]->c_str(), -1, SQLITE_TRANSIENT);
                else sqlite3_bind_null(st, static_cast<int>(c) + 1);
            }
            if (sqlite3_step(st) != SQLITE_DONE)
                throw std::runtime_error("SQLite insert " + t.name + ": " + sqlite3_errmsg(db_));
            sqlite3_reset(st);
        }
        sqlite3_finalize(st);
    }
};

}  // namespace

std::unique_ptr<Database> makeSqlite() { return std::make_unique<SqliteDb>(); }
