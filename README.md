# FarhanDB 🗄️

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Mac-lightgrey.svg)]()

## About

FarhanDB is a **relational database engine built entirely from scratch in C++17** — no external database libraries used. Every single component including the storage engine, SQL parser, transaction manager, buffer pool, query optimizer, and lock manager is hand-written from the ground up.

This project was built to deeply understand how real-world databases like MySQL and PostgreSQL work internally at the systems level — from raw disk I/O and page management all the way up to SQL parsing and query execution.

> Think of it as a mini database engine that you can actually read, understand and learn from.

**Who is this for?**
- Developers who want to understand database internals
- C++ systems programming enthusiasts
- Students studying database implementation
- Anyone curious about how SQL databases work under the hood

---

## Features

- **Storage Engine** — Page-based storage with slotted page layout
- **Buffer Pool Manager** — LRU eviction policy with pin/unpin mechanism
- **Write-Ahead Log (WAL)** — Crash recovery and durability
- **B+ Tree Index** — Fast point lookups and range queries
- **SQL Parser** — Hand-written lexer and parser supporting core SQL
- **Transaction Manager** — ACID transactions with BEGIN/COMMIT/ROLLBACK
- **Lock Manager** — Row-level and table-level locking (Shared/Exclusive)
- **Aggregate Functions** — COUNT, SUM, AVG, MAX, MIN
- **JOIN Support** — Inner join across multiple tables
- **Query Optimizer** — Primary key scan, predicate pushdown, join reordering
- **Beginner Mode** — Step-by-step menu interface for non-SQL users
- **SQL Mode** — Direct SQL query interface

---

## Supported SQL

```sql
-- Create a table
CREATE TABLE students (id INT PRIMARY KEY, name VARCHAR(50), age INT);

-- Insert records
INSERT INTO students VALUES (1, 'Farhan', 21);

-- Query records
SELECT * FROM students;
SELECT name, age FROM students WHERE age = 21;

-- Update records
UPDATE students SET age = 22 WHERE id = 1;

-- Delete records
DELETE FROM students WHERE id = 1;

-- Drop a table
DROP TABLE students;

-- Aggregate functions
SELECT COUNT(*) FROM students;
SELECT SUM(age) FROM students;
SELECT AVG(age) FROM students;
SELECT MAX(age) FROM students;
SELECT MIN(age) FROM students;

-- JOIN two tables
SELECT * FROM students JOIN grades ON students.id = grades.student_id;

-- Transactions
BEGIN;
INSERT INTO students VALUES (2, 'Ahmed', 19);
COMMIT;
ROLLBACK;
```

---

## Architecture

```
┌─────────────────────────────────────┐
│           User Interface            │
│    (Beginner Mode / SQL Mode)       │
├─────────────────────────────────────┤
│           Query Layer               │
│  Lexer → Parser → Optimizer →       │
│          Executor                   │
├─────────────────────────────────────┤
│         Transaction Layer           │
│  Transaction Manager + Lock Manager │
├─────────────────────────────────────┤
│          Storage Layer              │
│  Buffer Pool → Disk Manager → WAL   │
├─────────────────────────────────────┤
│           Index Layer               │
│           B+ Tree                   │
└─────────────────────────────────────┘
```

---

## Building from Source

### Requirements
- C++17 compiler (GCC 10+ or Clang 10+)
- CMake 3.20+

### Build Steps (Windows)

```cmd
git clone https://github.com/FarhanAlam2001/FarhanDB.git
cd FarhanDB
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
farhandb.exe
```

### Build Steps (Linux/Mac)

```bash
git clone https://github.com/FarhanAlam2001/FarhanDB.git
cd FarhanDB
mkdir build
cd build
cmake ..
cmake --build .
./farhandb
```

---

## Usage

When you run FarhanDB you will be greeted with a mode selection:

```
1. Beginner Mode  (step-by-step menu)
2. SQL Mode       (type SQL queries)
3. Exit
```

### Beginner Mode
Perfect for users who don't know SQL. Just follow the prompts:
- Create tables by answering simple questions
- Insert records field by field
- View records in a formatted table
- Delete records safely by ID only

### SQL Mode
For users who know SQL — type any supported SQL query ending with `;`

Type `menu` to go back, `exit` to quit.

---

## Data Storage

FarhanDB stores all data in 3 files created automatically in the same directory as the executable:

| File | Purpose |
|---|---|
| `farhandb.db` | Actual table data |
| `farhandb.wal` | Transaction log |
| `farhandb.catalog` | Table definitions |

---

## Project Structure

```
FarhanDB/
├── src/
│   ├── storage/
│   │   ├── page.cpp                # Slotted page layout
│   │   ├── disk_manager.cpp        # Raw disk I/O
│   │   ├── buffer_pool.cpp         # LRU buffer pool
│   │   └── wal.cpp                 # Write-ahead log
│   ├── index/
│   │   └── btree.cpp               # B+ Tree index
│   ├── query/
│   │   ├── lexer.cpp               # SQL tokenizer
│   │   ├── parser.cpp              # SQL parser
│   │   ├── catalog.cpp             # Table catalog
│   │   ├── optimizer.cpp           # Query optimizer
│   │   └── executor.cpp            # Query executor
│   ├── transaction/
│   │   ├── lock_manager.cpp        # Locking
│   │   └── transaction_manager.cpp # Transaction management
│   └── main.cpp                    # Entry point + UI
├── include/                        # Header files
├── CMakeLists.txt
└── README.md
```

---

## Roadmap

- [x] Storage engine with buffer pool
- [x] Write-ahead logging
- [x] SQL lexer and parser
- [x] Query executor (SELECT, INSERT, UPDATE, DELETE)
- [x] ACID transactions
- [x] Beginner mode interface
- [x] Multi-page table scanning
- [x] JOIN support
- [x] Aggregate functions (COUNT, SUM, AVG, MAX, MIN)
- [x] Query optimizer
- [x] Network/TCP interface

---

## License

MIT License — free to use, modify and distribute.

---

## Author

Built by **Md Farhan Alam** ([@FarhanAlam2001](https://github.com/FarhanAlam2001)) — a systems programming project demonstrating low-level database internals in C++.
