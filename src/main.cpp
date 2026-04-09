#include <iostream>
#include <string>
#include <memory>
#include <iomanip>
#include <vector>
#include <limits>
#include <algorithm>

#include "storage/disk_manager.h"
#include "storage/buffer_pool.h"
#include "storage/wal.h"
#include "index/btree.h"
#include "query/lexer.h"
#include "query/parser.h"
#include "query/catalog.h"
#include "query/executor.h"
#include "transaction/lock_manager.h"
#include "transaction/transaction_manager.h"
#include "server/server.h"

using namespace FarhanDB;

// ─────────────────────────────────────────────
//  UTILITIES
// ─────────────────────────────────────────────

void ClearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void PauseContinue() {
    std::cout << "\nPress Enter to continue...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void PrintLine(char c = '-', int len = 52) {
    std::cout << std::string(len, c) << std::endl;
}

// Convert spaces to underscores in table/column names
std::string Sanitize(const std::string& input) {
    std::string result = input;
    std::replace(result.begin(), result.end(), ' ', '_');
    return result;
}

// ─────────────────────────────────────────────
//  BANNER
// ─────────────────────────────────────────────

void PrintBanner() {
    ClearScreen();
    PrintLine('=');
    std::cout << R"(
  ______         _                 ____  ____
 |  ____|       | |               |  _ \|  _ \
 | |__ __ _ _ __| |__   __ _ _ __ | | | | |_) |
 |  __/ _` | '__| '_ \ / _` | '_ \| | | |  _ <
 | | | (_| | |  | | | | (_| | | | | |_| | |_) |
 |_|  \__,_|_|  |_| |_|\__,_|_| |_|____/|____/
)" << std::endl;
    PrintLine('=');
    std::cout << "     Advanced C++ Database Engine v1.0.0" << std::endl;
    PrintLine('=');
    std::cout << std::endl;
}

// ─────────────────────────────────────────────
//  RESULT PRINTER — clean table style
// ─────────────────────────────────────────────

void PrintResults(const ExecutionResult& result) {
    if (!result.success) {
        std::cout << "\n  [ERROR] " << result.message << std::endl;
        return;
    }

    if (!result.rows.empty()) {
        std::vector<size_t> widths;
        for (const auto& name : result.column_names)
            widths.push_back(name.size());
        for (const auto& row : result.rows)
            for (size_t i = 0; i < row.size() && i < widths.size(); i++)
                widths[i] = std::max(widths[i], row[i].size());

        auto printBorder = [&]() {
            std::cout << "  +";
            for (auto w : widths)
                std::cout << std::string(w + 2, '=') << "+";
            std::cout << "\n";
        };

        auto printThinBorder = [&]() {
            std::cout << "  +";
            for (auto w : widths)
                std::cout << std::string(w + 2, '-') << "+";
            std::cout << "\n";
        };

        // Top border
        printBorder();

        // Column headers
        std::cout << "  |";
        for (size_t i = 0; i < result.column_names.size(); i++)
            std::cout << " " << std::left << std::setw(widths[i])
                      << result.column_names[i] << " |";
        std::cout << "\n";

        // Header separator (thick)
        printBorder();

        // Rows
        for (size_t r = 0; r < result.rows.size(); r++) {
            std::cout << "  |";
            for (size_t i = 0; i < result.rows[r].size() && i < widths.size(); i++)
                std::cout << " " << std::left << std::setw(widths[i])
                          << result.rows[r][i] << " |";
            std::cout << "\n";
            // Thin line between rows
            if (r < result.rows.size() - 1) printThinBorder();
        }

        // Bottom border
        printBorder();

        std::cout << "  " << result.rows.size() << " row(s) in set.\n";
    } else {
        std::cout << "\n  [OK] " << result.message << std::endl;
    }
}

// ─────────────────────────────────────────────
//  BEGINNER MODE
// ─────────────────────────────────────────────

void BeginnerCreateTable(Executor* executor, Catalog*) {
    ClearScreen();
    PrintLine('=');
    std::cout << "         CREATE A NEW TABLE" << std::endl;
    PrintLine('=');

    std::string table_name;
    std::cout << "\n  Enter table name: ";
    std::getline(std::cin, table_name);
    table_name = Sanitize(table_name);

    int col_count;
    std::cout << "  How many columns? ";
    std::cin >> col_count;
    std::cin.ignore();

    std::string sql = "CREATE TABLE " + table_name + " (";

    for (int i = 0; i < col_count; i++) {
        std::string col_name, col_type;
        std::cout << "\n  Column " << (i+1) << " name: ";
        std::getline(std::cin, col_name);
        col_name = Sanitize(col_name);

        std::cout << "  Column " << (i+1) << " type (INT or VARCHAR): ";
        std::getline(std::cin, col_type);
        for (auto& c : col_type) c = toupper(c);

        if (col_type == "VARCHAR") {
            int size;
            std::cout << "  Max characters for " << col_name << ": ";
            std::cin >> size;
            std::cin.ignore();
            sql += col_name + " VARCHAR(" + std::to_string(size) + ")";
        } else {
            sql += col_name + " INT";
        }

        std::string is_pk;
        std::cout << "  Is '" << col_name << "' a unique ID column? (yes/no): ";
        std::getline(std::cin, is_pk);
        if (is_pk == "yes" || is_pk == "y") sql += " PRIMARY KEY";

        if (i < col_count - 1) sql += ", ";
    }
    sql += ");";

    std::cout << "\n  Running: " << sql << std::endl;

    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.Tokenize();
        Parser parser(tokens);
        auto   stmt   = parser.Parse();
        auto   result = executor->Execute(stmt);
        PrintResults(result);
    } catch (const std::exception& e) {
        std::cout << "  [ERROR] " << e.what() << std::endl;
    }
    PauseContinue();
}

void BeginnerInsertRecord(Executor* executor, Catalog* catalog) {
    ClearScreen();
    PrintLine('=');
    std::cout << "         INSERT A RECORD" << std::endl;
    PrintLine('=');

    std::string table_name;
    std::cout << "\n  Enter table name: ";
    std::getline(std::cin, table_name);
    table_name = Sanitize(table_name);

    TableSchema* schema = catalog->GetTable(table_name);
    if (!schema) {
        std::cout << "\n  [ERROR] Table '" << table_name << "' does not exist!" << std::endl;
        PauseContinue();
        return;
    }

    std::string sql = "INSERT INTO " + table_name + " VALUES (";
    for (size_t i = 0; i < schema->columns.size(); i++) {
        std::string val;
        std::cout << "  Enter " << schema->columns[i].name
                  << " (" << (schema->columns[i].type == DataType::INT
                              ? "number" : "text") << "): ";
        std::getline(std::cin, val);

        if (schema->columns[i].type == DataType::VARCHAR)
            sql += "'" + val + "'";
        else
            sql += val;

        if (i < schema->columns.size() - 1) sql += ", ";
    }
    sql += ");";

    std::cout << "\n  Running: " << sql << std::endl;

    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.Tokenize();
        Parser parser(tokens);
        auto   stmt   = parser.Parse();
        auto   result = executor->Execute(stmt);
        PrintResults(result);
    } catch (const std::exception& e) {
        std::cout << "  [ERROR] " << e.what() << std::endl;
    }
    PauseContinue();
}

void BeginnerViewTable(Executor* executor) {
    ClearScreen();
    PrintLine('=');
    std::cout << "         VIEW TABLE RECORDS" << std::endl;
    PrintLine('=');

    std::string table_name;
    std::cout << "\n  Enter table name: ";
    std::getline(std::cin, table_name);
    table_name = Sanitize(table_name);

    std::string sql = "SELECT * FROM " + table_name + ";";

    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.Tokenize();
        Parser parser(tokens);
        auto   stmt   = parser.Parse();
        auto   result = executor->Execute(stmt);
        PrintResults(result);
    } catch (const std::exception& e) {
        std::cout << "  [ERROR] " << e.what() << std::endl;
    }
    PauseContinue();
}

void BeginnerDeleteRecord(Executor* executor, Catalog* catalog) {
    ClearScreen();
    PrintLine('=');
    std::cout << "         DELETE A RECORD" << std::endl;
    PrintLine('=');

    std::string table_name;
    std::cout << "\n  Enter table name: ";
    std::getline(std::cin, table_name);
    table_name = Sanitize(table_name);

    TableSchema* schema = catalog->GetTable(table_name);
    if (!schema) {
        std::cout << "\n  [ERROR] Table '" << table_name << "' does not exist!" << std::endl;
        PauseContinue();
        return;
    }

    // Find primary key column
    std::string pk_col = "";
    for (auto& col : schema->columns)
        if (col.is_primary_key) { pk_col = col.name; break; }

    if (pk_col.empty()) {
        std::cout << "\n  [ERROR] This table has no primary key!" << std::endl;
        PauseContinue();
        return;
    }

    // Show current records so user can see IDs
    std::cout << "\n  Current records in '" << table_name << "':" << std::endl;
    try {
        std::string view_sql = "SELECT * FROM " + table_name + ";";
        Lexer  vl(view_sql);
        auto   vt = vl.Tokenize();
        Parser vp(vt);
        auto   vs = vp.Parse();
        auto   vr = executor->Execute(vs);
        PrintResults(vr);
    } catch (...) {
        std::cout << "  (could not load records)" << std::endl;
    }

    std::cout << "\n  Which " << pk_col << " do you want to delete? ";
    std::string col_val;
    std::getline(std::cin, col_val);

    std::string sql = "DELETE FROM " + table_name +
                      " WHERE " + pk_col + " = " + col_val + ";";

    std::cout << "\n  Running: " << sql << std::endl;

    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.Tokenize();
        Parser parser(tokens);
        auto   stmt   = parser.Parse();
        auto   result = executor->Execute(stmt);
        PrintResults(result);
    } catch (const std::exception& e) {
        std::cout << "  [ERROR] " << e.what() << std::endl;
    }
    PauseContinue();
}

void BeginnerDropTable(Executor* executor) {
    ClearScreen();
    PrintLine('=');
    std::cout << "         DROP A TABLE" << std::endl;
    PrintLine('=');

    std::string table_name;
    std::cout << "\n  Enter table name to drop: ";
    std::getline(std::cin, table_name);
    table_name = Sanitize(table_name);

    std::string confirm;
    std::cout << "  Are you sure? This cannot be undone! (yes/no): ";
    std::getline(std::cin, confirm);

    if (confirm != "yes" && confirm != "y") {
        std::cout << "  Cancelled." << std::endl;
        PauseContinue();
        return;
    }

    std::string sql = "DROP TABLE " + table_name + ";";
    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.Tokenize();
        Parser parser(tokens);
        auto   stmt   = parser.Parse();
        auto   result = executor->Execute(stmt);
        PrintResults(result);
    } catch (const std::exception& e) {
        std::cout << "  [ERROR] " << e.what() << std::endl;
    }
    PauseContinue();
}

void BeginnerMode(Executor* executor, Catalog* catalog) {
    while (true) {
        ClearScreen();
        PrintLine('=');
        std::cout << "         FARHANDB - BEGINNER MODE" << std::endl;
        PrintLine('=');
        std::cout << "\n  What would you like to do?\n" << std::endl;
        std::cout << "  1. Create a new table" << std::endl;
        std::cout << "  2. Insert a record" << std::endl;
        std::cout << "  3. View table records" << std::endl;
        std::cout << "  4. Delete a record" << std::endl;
        std::cout << "  5. Drop a table" << std::endl;
        std::cout << "  6. Go back to main menu" << std::endl;
        std::cout << "  7. Exit FarhanDB" << std::endl;
        PrintLine('-');
        std::cout << "  Enter choice: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        switch (choice) {
            case 1: BeginnerCreateTable(executor, catalog); break;
            case 2: BeginnerInsertRecord(executor, catalog); break;
            case 3: BeginnerViewTable(executor); break;
            case 4: BeginnerDeleteRecord(executor, catalog); break;
            case 5: BeginnerDropTable(executor); break;
            case 6: return;
            case 7: exit(0);
            default:
                std::cout << "\n  Invalid choice." << std::endl;
                PauseContinue();
        }
    }
}

// ─────────────────────────────────────────────
//  SQL MODE
// ─────────────────────────────────────────────

void SQLMode(Executor* executor) {
    ClearScreen();
    PrintLine('=');
    std::cout << "         FARHANDB - SQL MODE" << std::endl;
    PrintLine('=');
    std::cout << "\n  Type SQL queries ending with ;" << std::endl;
    std::cout << "  Type 'menu' to go back to main menu." << std::endl;
    std::cout << "  Type 'exit' to quit.\n" << std::endl;
    PrintLine('-');

    std::string line, query;
    std::cout << "\n  farhandb> ";

    while (std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") exit(0);
        if (line == "menu") return;

        query += " " + line;

        if (!query.empty() && query.find(';') != std::string::npos) {
            try {
                Lexer  lexer(query);
                auto   tokens = lexer.Tokenize();
                Parser parser(tokens);
                auto   stmt   = parser.Parse();
                auto   result = executor->Execute(stmt);
                PrintResults(result);
            } catch (const std::exception& e) {
                std::cout << "\n  [ERROR] " << e.what() << std::endl;
            }
            query.clear();
            PrintLine('-');
        }

        std::cout << "  farhandb> ";
    }
}

// ─────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────

int main() {
    auto disk_manager = std::make_unique<DiskManager>("farhandb.db");
    auto bpm          = std::make_unique<BufferPoolManager>(
                            BUFFER_POOL_SIZE, disk_manager.get());
    auto wal          = std::make_unique<WAL>("farhandb.wal");
    auto lock_mgr     = std::make_unique<LockManager>();
    auto txn_mgr      = std::make_unique<TransactionManager>(wal.get());
    auto catalog      = std::make_unique<Catalog>("farhandb.catalog");
    auto executor     = std::make_unique<Executor>(
                            bpm.get(), catalog.get(),
                            txn_mgr.get(), lock_mgr.get());

    while (true) {
        PrintBanner();
        std::cout << "  Welcome! How would you like to use FarhanDB?\n" << std::endl;
        std::cout << "  1. Beginner Mode  (step-by-step menu)" << std::endl;
        std::cout << "  2. SQL Mode       (type SQL queries)" << std::endl;
        std::cout << "  3. Server Mode    (TCP server on port 5555)" << std::endl;
        std::cout << "  4. Exit" << std::endl;
        PrintLine('-');
        std::cout << "  Enter choice: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        switch (choice) {
            case 1: BeginnerMode(executor.get(), catalog.get()); break;
            case 2: SQLMode(executor.get()); break;
            case 3: {
                TCPServer server(executor.get(), 5555);
                server.Start();
                break;
            }
            case 4:
                std::cout << "\n  Goodbye!\n" << std::endl;
                bpm->FlushAllPages();
                wal->Flush();
                return 0;
            default:
                std::cout << "\n  Invalid choice." << std::endl;
                PauseContinue();
        }
    }
}