# Wypożyczalnia gier planszowych – porównanie wydajności baz danych

Projekt ZTB: PostgreSQL i SQLite (modele relacyjne) oraz MongoDB (dokumentowy) i Memgraph (grafowy).
Aplikacja testowa jest napisana w C++17. Wszystko działa w Dockerze.

## Uruchomienie

```bash
docker compose up -d postgres mongo memgraph
docker compose build app
docker compose run --rm app --size 10000
```

Opcje:
- `--size N`: liczba ocen i wypożyczeń (domyślnie 10000).
- `--db postgresql,sqlite,mongodb,memgraph`: które bazy testować.
- `--trials 3`: liczba prób każdego scenariusza.

Katalog z plikami CSV to domyślnie `C:/Studia/Zaawansowane_bazy_danych`. Inny można podać przez zmienną `DATA_DIR`.
Wyniki trafiają do `results/results.csv` (czasy scenariuszy) i `results/load_times.csv` (czasy ładowania).

## Uruchomienie w Visual Studio (2022/2026) przez WSL

Visual Studio działa na Windows, ale kompiluje i debuguje program w WSL (Ubuntu). Bazy działają dalej w Dockerze.

1. **Visual Studio Installer** → Modyfikuj → zaznacz komponent **„Programowanie dla systemu Linux i urządzeń osadzonych w języku C++”** (*Linux and embedded development with C++*).
2. **Jednorazowo w terminalu WSL** (Ubuntu), w katalogu projektu:
   ```bash
   cd /mnt/c/sciezka/do/ztb-planszowki
   bash scripts/setup-wsl.sh
   ```
   Skrypt instaluje g++, gdb, CMake, Ninja oraz biblioteki PostgreSQL, SQLite, MongoDB i Memgraph (mgclient).
3. **Bazy** (PowerShell, w katalogu projektu): `docker compose up -d postgres mongo memgraph`
4. **Visual Studio** → *Plik → Otwórz → Folder* → wybierz katalog projektu.
   - Na pasku narzędzi w polu *Cel* wybierz **WSL: Ubuntu**, a jako konfigurację **WSL Release (do pomiarów)** albo **WSL Debug**.
   - Jako element startowy wybierz **ztb_bench - wszystkie bazy, 10 000** (konfiguracje są w `.vs/launch.vs.json`; tam też zmienia się argumenty).
   - F5 uruchamia z debugerem, Ctrl+F5 bez debugera.

Domyślne ustawienia przy uruchamianiu z IDE:
- Dane CSV: `/mnt/c/Studia/Zaawansowane_bazy_danych` (inny katalog: argument `--data`).
- Bazy: `localhost:55432` (PostgreSQL), `localhost:27018` (MongoDB), `localhost:7688` (Memgraph).
- Plik SQLite: `/tmp/ztb_planszowki.sqlite`, czyli na dysku Linuksa. Na `/mnt/c` SQLite działałby kilkadziesiąt razy wolniej.
- Wyniki: katalog `results/` w projekcie.

Pomiary do sprawozdania rób w konfiguracji **Release**. Wersja Debug jest wolniejsza, bo kompilator nie optymalizuje kodu.

## Struktura

| Plik | Rola |
|---|---|
| `sql/schema.sql`, `sql/indexes.sql` | schemat 16 tabel i indeksy pomocnicze (wspólne dla PostgreSQL i SQLite) |
| `src/dataset.*` | wczytanie CSV i wygenerowanie danych wypożyczalni (ziarno 42) |
| `src/database.h` | wspólny interfejs `Database` |
| `src/postgres_db.cpp`, `sqlite_db.cpp`, `mongo_db.cpp`, `memgraph_db.cpp` | implementacje dla 4 baz |
| `src/scenarios.cpp` | lista 24 scenariuszy CRUD |
| `src/benchmark.*` | pomiar czasu (rozgrzewka + N prób) i zapis do CSV |
| `docs/model-danych.md` | diagram ERD oraz modele dokumentowy i grafowy |
| `CMakePresets.json`, `.vs/launch.vs.json` | konfiguracja Visual Studio (WSL) |
| `scripts/setup-wsl.sh` | instalacja narzędzi i bibliotek w WSL |
