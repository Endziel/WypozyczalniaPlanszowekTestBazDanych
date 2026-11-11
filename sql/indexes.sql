-- Indeksy pomocnicze (wariant "z indeksami"). Klucze glowne istnieja zawsze.
CREATE INDEX idx_ratings_customer   ON ratings(customer_id);
CREATE INDEX idx_ratings_game       ON ratings(game_id);
CREATE INDEX idx_copies_game        ON copies(game_id);
CREATE INDEX idx_rentals_customer   ON rentals(customer_id);
CREATE INDEX idx_rentals_copy       ON rentals(copy_id);
CREATE INDEX idx_rentals_rented_at  ON rentals(rented_at);
CREATE INDEX idx_payments_rental    ON payments(rental_id);
CREATE INDEX idx_payments_paid_at   ON payments(paid_at);
CREATE INDEX idx_customers_username ON customers(username);
CREATE INDEX idx_games_players      ON games(min_players, max_players);
CREATE INDEX idx_gm_mechanic        ON game_mechanics(mechanic_id);
CREATE INDEX idx_gt_theme           ON game_themes(theme_id);
