#include "scenarios.h"

const std::vector<Scenario>& allScenarios() {
    static const std::vector<Scenario> list = {
        // CREATE
        {"C1", 'C', "Dodanie jednego klienta"},
        {"C2", 'C', "Dodanie 1000 wypozyczen (batch)"},
        {"C3", 'C', "Dodanie gry z mechanikami i tematami (transakcja)"},
        {"C4", 'C', "Dodanie 10 000 ocen (batch)"},
        {"C5", 'C', "Wypozyczenie + platnosc (transakcja)"},
        {"C6", 'C', "Dodanie 100 egzemplarzy gry"},
        // READ
        {"R1", 'R', "Gra po identyfikatorze"},
        {"R2", 'R', "Gry z dana mechanika (JOIN)"},
        {"R3", 'R', "10 najczesciej wypozyczanych gier (agregacja)"},
        {"R4", 'R', "Historia wypozyczen klienta"},
        {"R5", 'R', "Gry dla N graczy o trudnosci < X (filtr)"},
        {"R6", 'R', "Srednia ocena gier w kazdym temacie (JOIN + agregacja)"},
        // UPDATE
        {"U1", 'U', "Zmiana ceny jednej gry"},
        {"U2", 'U', "Zwrot gry (ustawienie returned_at)"},
        {"U3", 'U', "Podwyzka cen gier w kategorii o 10%"},
        {"U4", 'U', "Zmiana miasta klienta po nazwie uzytkownika"},
        {"U5", 'U', "Przeliczenie sredniej oceny gry"},
        {"U6", 'U', "Zmiana statusu egzemplarzy w zlym stanie"},
        // DELETE
        {"D1", 'D', "Usuniecie wypozyczenia po identyfikatorze"},
        {"D2", 'D', "Usuniecie wszystkich ocen klienta"},
        {"D3", 'D', "Usuniecie wypozyczen sprzed daty"},
        {"D4", 'D', "Usuniecie gry razem z powiazaniami"},
        {"D5", 'D', "Usuniecie klientow bez wypozyczen i ocen"},
        {"D6", 'D', "Usuniecie platnosci z zakresu dat"},
    };
    return list;
}

int pickGameId(const Dataset& ds, int trial) {
    return ds.games[(static_cast<size_t>(trial) * 7919 + 13) % ds.games.size()].id;
}

int pickCustomerId(const Dataset& ds, int trial) {
    return ds.customers[(static_cast<size_t>(trial) * 7919 + 13) % ds.customers.size()].id;
}
