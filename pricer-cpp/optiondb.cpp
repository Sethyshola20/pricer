#include "optiondb.h"
#include <iostream>

OptionDatabase::OptionDatabase() : db_(nullptr) {}

OptionDatabase::~OptionDatabase() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool OptionDatabase::initialize(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    const char* create_inputs_sql = R"(
        CREATE TABLE IF NOT EXISTS option_inputs (
            input_id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            spot REAL NOT NULL,
            strike REAL NOT NULL,
            rate REAL NOT NULL,
            volatility REAL NOT NULL,
            maturity REAL NOT NULL,
            steps INTEGER NOT NULL,
            type TEXT NOT NULL CHECK (type IN ('call', 'put')),
            UNIQUE(spot, strike, rate, volatility, maturity, steps, type)
        )
    )";
    
    const char* create_outputs_sql = R"(
        CREATE TABLE IF NOT EXISTS option_outputs (
            output_id INTEGER PRIMARY KEY AUTOINCREMENT,
            input_id INTEGER NOT NULL,
            price REAL NOT NULL,
            delta REAL NOT NULL,
            vega REAL NOT NULL,
            calculation_type TEXT NOT NULL CHECK (calculation_type IN ('black_scholes', 'binomial')),
            FOREIGN KEY (input_id) REFERENCES option_inputs (input_id) ON DELETE CASCADE,
            UNIQUE(input_id, calculation_type)
        )
    )";
    
    char* err_msg = nullptr;
    rc = sqlite3_exec(db_, create_inputs_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error (inputs): " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    
    rc = sqlite3_exec(db_, create_outputs_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error (outputs): " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    
    std::cout << "Database initialized successfully" << std::endl;
    return true;
}

int OptionDatabase::store_input(const BSParams& params) {
    const char* sql = R"(
        INSERT OR IGNORE INTO option_inputs 
        (spot, strike, rate, volatility, maturity, steps, type) 
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }
    
    sqlite3_bind_double(stmt, 1, params.S);
    sqlite3_bind_double(stmt, 2, params.K);
    sqlite3_bind_double(stmt, 3, params.r);
    sqlite3_bind_double(stmt, 4, params.sigma);
    sqlite3_bind_double(stmt, 5, params.T);
    sqlite3_bind_int(stmt, 6, params.steps);
    sqlite3_bind_text(stmt, 7, (params.type == OptionType::Call) ? "call" : "put", -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to insert input: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return -1;
    }
    
    int input_id = static_cast<int>(sqlite3_last_insert_rowid(db_));
    sqlite3_finalize(stmt);
    
    if (input_id == 0) {
        input_id = find_existing_input_id(params);
    }
    
    return input_id;
}

bool OptionDatabase::store_output(int input_id, const BSResult& result, const std::string& calculation_type) {
    const char* sql = R"(
        INSERT OR REPLACE INTO option_outputs 
        (input_id, price, delta, vega, calculation_type) 
        VALUES (?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, input_id);
    sqlite3_bind_double(stmt, 2, result.price);
    sqlite3_bind_double(stmt, 3, result.delta);
    sqlite3_bind_double(stmt, 4, result.vega);
    sqlite3_bind_text(stmt, 5, calculation_type.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to insert output: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    
    sqlite3_finalize(stmt);
    return true;
}

int OptionDatabase::find_existing_input_id(const BSParams& params) {
    const char* sql = R"(
        SELECT input_id FROM option_inputs 
        WHERE spot = ? AND strike = ? AND rate = ? AND volatility = ? 
        AND maturity = ? AND steps = ? AND type = ?
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_double(stmt, 1, params.S);
    sqlite3_bind_double(stmt, 2, params.K);
    sqlite3_bind_double(stmt, 3, params.r);
    sqlite3_bind_double(stmt, 4, params.sigma);
    sqlite3_bind_double(stmt, 5, params.T);
    sqlite3_bind_int(stmt, 6, params.steps);
    sqlite3_bind_text(stmt, 7, (params.type == OptionType::Call) ? "call" : "put", -1, SQLITE_STATIC);
    
    int input_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        input_id = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return input_id;
}

void OptionDatabase::print_recent_calculations(int limit) {
    const char* sql = R"(
        SELECT i.input_id, i.timestamp, i.spot, i.strike, i.volatility, 
               i.steps, i.type, o.price, o.delta, o.vega, o.calculation_type
        FROM option_inputs i
        JOIN option_outputs o ON i.input_id = o.input_id
        ORDER BY i.timestamp DESC
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return;
    
    sqlite3_bind_int(stmt, 1, limit);
    
    std::cout << "Recent calculations:" << std::endl;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::cout << "ID: " << sqlite3_column_int(stmt, 0)
                  << ", Time: " << sqlite3_column_text(stmt, 1)
                  << ", Spot: " << sqlite3_column_double(stmt, 2)
                  << ", Strike: " << sqlite3_column_double(stmt, 3)
                  << ", Vol: " << sqlite3_column_double(stmt, 4)
                  << ", Steps: " << sqlite3_column_int(stmt, 5)
                  << ", Type: " << sqlite3_column_text(stmt, 6)
                  << ", Price: " << sqlite3_column_double(stmt, 7)
                  << ", Method: " << sqlite3_column_text(stmt, 10)
                  << std::endl;
    }
    
    sqlite3_finalize(stmt);
}