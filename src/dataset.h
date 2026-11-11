#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Wszystkie dane wypozyczalni trzymane w pamieci. Ten sam zbior jest
// ladowany do kazdej bazy, wiec kazdy silnik dostaje identyczne dane.

struct Game {
    int id;
    std::string name;
    int year;
    double weight;
    double avgRating;
    int minPlayers, maxPlayers;
    int playtime;
    int minAge;
    double pricePerDay;
};

// Slownik + powiazania wiele-do-wielu z grami (np. mechaniki).
struct Dictionary {
    std::vector<std::string> names;               // id = indeks + 1
    std::vector<std::pair<int, int>> links;       // (game_id, item_id)
};

struct Customer {
    int id;
    std::string username, email, city;
    int64_t registeredAt;                         // sekundy od 1970 (UTC)
};

struct Rating {
    int64_t id;
    int customerId;
    int gameId;
    double rating;
};

struct Copy {
    int id;
    int gameId;
    std::string condition, status;
    int64_t purchaseDate;
};

struct Rental {
    int64_t id;
    int copyId;
    int customerId;
    int64_t rentedAt, dueAt;
    std::optional<int64_t> returnedAt;
};

struct Payment {
    int64_t id;
    int64_t rentalId;
    double amount;
    int64_t paidAt;
    std::string method;
};

struct Dataset {
    std::vector<Game> games;
    Dictionary mechanics, themes, categories, designers, publishers;
    std::vector<Customer> customers;
    std::vector<Rating> ratings;
    std::vector<Copy> copies;
    std::vector<Rental> rentals;
    std::vector<Payment> payments;

    // size = liczba ocen i liczba wypozyczen (glowne, "duze" tabele)
    static Dataset load(const std::string& dataDir, size_t size);
    void printSummary() const;
};

// "2024-03-01 12:00:00"
std::string formatTime(int64_t t);
// "2024-03-01T12:00:00" (format dla Cypher localDateTime)
std::string formatTimeIso(int64_t t);
