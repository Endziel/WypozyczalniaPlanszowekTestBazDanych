-- Wypozyczalnia gier planszowych - schemat relacyjny (16 tabel).
-- Ten sam plik jest wykonywany w PostgreSQL i SQLite, dlatego uzyte sa
-- tylko typy rozumiane przez oba silniki.
-- Celowo brak indeksow pomocniczych - dodaje je osobno sql/indexes.sql
-- (porownanie "bez indeksow" / "z indeksami").

-- ===== Katalog gier (dane z BoardGameGeek) =====

CREATE TABLE games (
    game_id        INTEGER PRIMARY KEY,
    name           TEXT    NOT NULL,
    year_published INTEGER,
    weight         DOUBLE PRECISION,   -- trudnosc 1-5
    avg_rating     DOUBLE PRECISION,
    min_players    INTEGER,
    max_players    INTEGER,
    playtime_min   INTEGER,
    min_age        INTEGER,
    price_per_day  NUMERIC(8,2) NOT NULL  -- cena wypozyczenia za dobe
);

CREATE TABLE mechanics (
    mechanic_id INTEGER PRIMARY KEY,
    name        TEXT NOT NULL UNIQUE
);

CREATE TABLE game_mechanics (
    game_id     INTEGER NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    mechanic_id INTEGER NOT NULL REFERENCES mechanics(mechanic_id) ON DELETE CASCADE,
    PRIMARY KEY (game_id, mechanic_id)
);

CREATE TABLE themes (
    theme_id INTEGER PRIMARY KEY,
    name     TEXT NOT NULL UNIQUE
);

CREATE TABLE game_themes (
    game_id  INTEGER NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    theme_id INTEGER NOT NULL REFERENCES themes(theme_id) ON DELETE CASCADE,
    PRIMARY KEY (game_id, theme_id)
);

CREATE TABLE categories (
    category_id INTEGER PRIMARY KEY,
    name        TEXT NOT NULL UNIQUE
);

CREATE TABLE game_categories (
    game_id     INTEGER NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    category_id INTEGER NOT NULL REFERENCES categories(category_id) ON DELETE CASCADE,
    PRIMARY KEY (game_id, category_id)
);

CREATE TABLE designers (
    designer_id INTEGER PRIMARY KEY,
    name        TEXT NOT NULL UNIQUE
);

CREATE TABLE game_designers (
    game_id     INTEGER NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    designer_id INTEGER NOT NULL REFERENCES designers(designer_id) ON DELETE CASCADE,
    PRIMARY KEY (game_id, designer_id)
);

CREATE TABLE publishers (
    publisher_id INTEGER PRIMARY KEY,
    name         TEXT NOT NULL UNIQUE
);

CREATE TABLE game_publishers (
    game_id      INTEGER NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    publisher_id INTEGER NOT NULL REFERENCES publishers(publisher_id) ON DELETE CASCADE,
    PRIMARY KEY (game_id, publisher_id)
);

-- ===== Klienci i oceny (uzytkownicy BGG) =====

CREATE TABLE customers (
    customer_id   INTEGER PRIMARY KEY,
    username      TEXT NOT NULL,
    email         TEXT NOT NULL,
    city          TEXT NOT NULL,
    registered_at TIMESTAMP NOT NULL
);

CREATE TABLE ratings (
    rating_id   BIGINT  PRIMARY KEY,
    customer_id INTEGER NOT NULL REFERENCES customers(customer_id) ON DELETE CASCADE,
    game_id     INTEGER NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    rating      DOUBLE PRECISION NOT NULL
);

-- ===== Wypozyczalnia (dane generowane) =====

CREATE TABLE copies (
    copy_id       INTEGER PRIMARY KEY,
    game_id       INTEGER NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    condition     TEXT NOT NULL,        -- new / good / worn
    status        TEXT NOT NULL,        -- available / rented / repair
    purchase_date TIMESTAMP NOT NULL
);

CREATE TABLE rentals (
    rental_id   BIGINT  PRIMARY KEY,
    copy_id     INTEGER NOT NULL REFERENCES copies(copy_id) ON DELETE CASCADE,
    customer_id INTEGER NOT NULL REFERENCES customers(customer_id) ON DELETE CASCADE,
    rented_at   TIMESTAMP NOT NULL,
    due_at      TIMESTAMP NOT NULL,
    returned_at TIMESTAMP              -- NULL = gra jeszcze nie oddana
);

CREATE TABLE payments (
    payment_id BIGINT PRIMARY KEY,
    rental_id  BIGINT NOT NULL REFERENCES rentals(rental_id) ON DELETE CASCADE,
    amount     NUMERIC(8,2) NOT NULL,
    paid_at    TIMESTAMP NOT NULL,
    method     TEXT NOT NULL           -- cash / card / blik
);
