#ifndef OPTIONDB_H
#define OPTIONDB_H

#include <sqlite3.h>
#include <string>
#include "types.h"

class OptionDatabase {
    private:
        sqlite3* db_;
        int find_existing_input_id(const BSParams& params);
        
    public:
        OptionDatabase();
        ~OptionDatabase();
        
        bool initialize(const std::string& db_path = "/data/options.db");
        int store_input(const BSParams& params);
        bool store_output(int input_id, const BSResult& result, const std::string& calculation_type);
        void print_recent_calculations(int limit = 10);
};

#endif