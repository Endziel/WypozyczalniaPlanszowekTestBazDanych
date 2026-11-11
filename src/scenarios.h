#pragma once
#include <string>
#include <vector>

#include "dataset.h"

struct Scenario {
    std::string id;     // np. "R1"
    char crud;          // 'C', 'R', 'U', 'D'
    std::string description;
};

// 24 scenariusze testowe (po 6 na kazda operacje CRUD).
const std::vector<Scenario>& allScenarios();

// Parametry scenariuszy - kazda proba (trial) dostaje inne, ale powtarzalne wartosci,
// zeby UPDATE/DELETE nie trafialy dwa razy w te same rekordy.
int pickGameId(const Dataset& ds, int trial);
int pickCustomerId(const Dataset& ds, int trial);
