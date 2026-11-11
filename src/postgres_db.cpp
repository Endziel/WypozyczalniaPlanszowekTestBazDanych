// PostgreSQL - model relacyjny, biblioteka libpq.
#include <libpq-fe.h>

#include <stdexcept>

#include "database.h"
#include "relational.h"
#include "scenarios.h"

namespace {

class PostgresDb : public Database {
public:
    ~PostgresDb() override { if (conn_) PQfinish(conn_); }

    std::string name() const override { return "postgresql"; }

    void connect() override {
        conn_ = PQconnectdb(env("PG_CONN", "host=localhost port=55432 dbname=planszowki user=ztb password=ztb").c_str());
        if (PQstatus(conn_) != CONNECTION_OK) throw std::runtime_error(std::string("PostgreSQL: ") + PQerrorMessage(conn_));
        exec("SET client_min_messages = WARNING");
    }

    void resetSchema() override {
        exec("DROP SCHEMA IF EXISTS public CASCADE; CREATE SCHEMA public;");
        exec(readFile(sqlDir() + "/schema.sql"));
    }

    void load(const Dataset& ds) override {
        for (const TableView& t : relationalTables(ds)) copyTable(t);
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
        for (const TableView& t : relationalTables(empty))
            out[t.name] = std::stoll(scalar("SELECT count(*) FROM " + t.name));
        return out;
    }

    bool runScenario(const std::string& id, const Dataset& ds, int trial) override {
        if (id == "R1") {
            query("SELECT * FROM games WHERE game_id = $1", {std::to_string(pickGameId(ds, trial))});
            return true;
        }
        return false;   // pozostale scenariusze - kolejny etap
    }

private:
    PGconn* conn_ = nullptr;

    void check(PGresult* r, const std::string& what) {
        ExecStatusType s = PQresultStatus(r);
        if (s != PGRES_COMMAND_OK && s != PGRES_TUPLES_OK && s != PGRES_COPY_IN) {
            std::string msg = PQresultErrorMessage(r);
            PQclear(r);
            throw std::runtime_error("PostgreSQL (" + what.substr(0, 60) + "): " + msg);
        }
    }

    void exec(const std::string& sql) {
        PGresult* r = PQexec(conn_, sql.c_str());
        check(r, sql);
        PQclear(r);
    }

    std::string scalar(const std::string& sql) {
        PGresult* r = PQexec(conn_, sql.c_str());
        check(r, sql);
        std::string v = PQntuples(r) > 0 ? PQgetvalue(r, 0, 0) : "";
        PQclear(r);
        return v;
    }

    // Zapytanie z parametrami; wynik jest pobierany w calosci (tak jak w realnej aplikacji).
    int query(const std::string& sql, const std::vector<std::string>& params) {
        std::vector<const char*> p;
        for (const auto& s : params) p.push_back(s.c_str());
        PGresult* r = PQexecParams(conn_, sql.c_str(), static_cast<int>(p.size()), nullptr, p.data(), nullptr, nullptr, 0);
        check(r, sql);
        int rows = PQntuples(r);
        PQclear(r);
        return rows;
    }

    // Szybkie ladowanie przez COPY ... FROM STDIN (format tekstowy).
    void copyTable(const TableView& t) {
        std::string cols;
        for (const auto& c : t.columns) cols += (cols.empty() ? "" : ",") + c;
        PGresult* r = PQexec(conn_, ("COPY " + t.name + " (" + cols + ") FROM STDIN").c_str());
        check(r, "COPY " + t.name);
        PQclear(r);

        std::string buf;
        SqlRow row;
        for (size_t i = 0; i < t.rows; ++i) {
            t.fill(i, row);
            for (size_t c = 0; c < row.size(); ++c) {
                if (c) buf += '\t';
                if (!row[c]) { buf += "\\N"; continue; }
                for (char ch : *row[c]) {
                    switch (ch) {
                        case '\\': buf += "\\\\"; break;
                        case '\t': buf += "\\t"; break;
                        case '\n': buf += "\\n"; break;
                        case '\r': buf += "\\r"; break;
                        default: buf += ch;
                    }
                }
            }
            buf += '\n';
            if (buf.size() > (1 << 20)) { put(buf); buf.clear(); }
        }
        if (!buf.empty()) put(buf);
        if (PQputCopyEnd(conn_, nullptr) != 1) throw std::runtime_error(PQerrorMessage(conn_));
        r = PQgetResult(conn_);
        check(r, "COPY " + t.name);
        PQclear(r);
    }

    void put(const std::string& data) {
        if (PQputCopyData(conn_, data.data(), static_cast<int>(data.size())) != 1)
            throw std::runtime_error(std::string("COPY: ") + PQerrorMessage(conn_));
    }
};

}  // namespace

std::unique_ptr<Database> makePostgres() { return std::make_unique<PostgresDb>(); }
