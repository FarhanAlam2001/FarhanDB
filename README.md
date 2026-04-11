# FarhanDB 🗄️

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Mac-lightgrey.svg)]()
[![Version](https://img.shields.io/badge/Version-3.0.0-green.svg)]()
[![GUI](https://img.shields.io/badge/GUI-Qt%206.11-blueviolet.svg)]()

## About

FarhanDB is a **relational database engine built entirely from scratch in C++17** — no external database libraries used. Every single component including the storage engine, SQL parser, transaction manager, buffer pool, query optimizer, and lock manager is hand-written from the ground up.

This project was built to deeply understand how real-world databases like MySQL and PostgreSQL work internally at the systems level — from raw disk I/O and page management all the way up to SQL parsing and query execution.

> Think of it as a mini database engine that you can actually read, understand and learn from.

**v3.0.0 introduces a full Qt6 desktop GUI** with SQL Mode, Beginner Mode, Server Mode, light/dark themes and a live database explorer sidebar.

**Who is this for?**
- Developers who want to understand database internals
- C++ systems programming enthusiasts
- Students studying database implementation
- Anyone curious about how SQL databases work under the hood

---

## 🌍 Unique Position

There is no other open-source database where:

- The entire engine is written from scratch in **C++17**
- A **native Qt6 GUI** is built by the same author as the engine
- **CLI + GUI + TCP Server** ship as one unified package
- The whole thing was built by **a single developer**

FarhanDB occupies a category of one.

---

## GUI Preview

FarhanDB now ships with a full desktop application built with Qt 6:

- **SQL Mode** — Write and run SQL queries with results table and query history
- **Beginner Mode** — No SQL knowledge needed — use forms to create tables, insert, view and delete records
- **Server Mode** — Start a TCP server on port 5555 for remote connections
- **Light Theme** — Frutiger Aero inspired blue design
- **Dark Theme** — Darcula inspired dark design
- **Database Explorer** — Sidebar showing all databases and tables
- **Query History** — Last 100 queries stored per session

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
- **ORDER BY / LIMIT / DISTINCT** — Sorting, pagination and deduplication
- **GROUP BY / HAVING** — Grouping and group filtering
- **OR / AND Conditions** — Multiple WHERE conditions
- **BETWEEN / LIKE** — Range queries and pattern matching
- **NOT NULL / DEFAULT** — Column constraints and default values
- **Subqueries** — WHERE col IN (SELECT ...)
- **Foreign Keys** — Referential integrity enforcement
- **Indexes** — CREATE INDEX on any column
- **UPDATE Multiple Columns** — SET col1 = val1, col2 = val2
- **ALTER TABLE** — ADD COLUMN and DROP COLUMN
- **Multiple Databases** — CREATE DATABASE, USE, SHOW DATABASES, DROP DATABASE
- **SQL Comments** — Support for -- comment syntax
- **TCP Network Server** — Accept SQL connections on port 5555
- **Qt6 Desktop GUI** — Full graphical interface with light/dark themes

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
SELECT * FROM students WHERE age = 19 OR age = 25;
SELECT DISTINCT age FROM students;
SELECT * FROM students ORDER BY age DESC;
SELECT * FROM students LIMIT 5;

-- BETWEEN and LIKE
SELECT * FROM students WHERE age BETWEEN 18 AND 25;
SELECT * FROM students WHERE name LIKE 'F%';
SELECT * FROM students WHERE name LIKE '%han';

-- Update records (single or multiple columns)
UPDATE students SET age = 22 WHERE id = 1;
UPDATE students SET name = 'Ahmed', age = 20 WHERE id = 1;

-- Delete records
DELETE FROM students WHERE id = 1;

-- Aggregate functions
SELECT COUNT(*) FROM students;
SELECT SUM(age) FROM students;
SELECT AVG(age) FROM students;
SELECT MAX(age) FROM students;
SELECT MIN(age) FROM students;

-- GROUP BY and HAVING
SELECT * FROM students GROUP BY age;
SELECT * FROM students GROUP BY age HAVING age > 19;

-- JOIN two tables
SELECT * FROM students JOIN grades ON students.id = grades.student_id;

-- Subqueries
SELECT * FROM students WHERE id IN (SELECT student_id FROM orders);

-- NOT NULL and DEFAULT
CREATE TABLE products (id INT PRIMARY KEY, name VARCHAR(50) NOT NULL, price INT DEFAULT 0);

-- Foreign keys
CREATE TABLE orders (id INT PRIMARY KEY, student_id INT REFERENCES students(id));

-- Indexes
CREATE INDEX idx_name ON students(name);

-- ALTER TABLE
ALTER TABLE students ADD COLUMN email VARCHAR(100);
ALTER TABLE students DROP COLUMN email;

-- Transactions
BEGIN;
INSERT INTO students VALUES (2, 'Ahmed', 19);
COMMIT;
ROLLBACK;

-- Multiple databases
CREATE DATABASE school;
USE school;
SHOW DATABASES;
DROP DATABASE school;
```

---

## Architecture

```
┌─────────────────────────────────────┐
│           Qt6 Desktop GUI           │
│  SQL Mode / Beginner Mode /         │
│  Server Mode / Theme Toggle         │
├─────────────────────────────────────┤
│           User Interface            │
│  Beginner Mode / SQL Mode /         │
│  TCP Server (port 5555)             │
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

### CLI Version Requirements
- C++17 compiler (GCC 10+ or Clang 10+)
- CMake 3.20+

### GUI Version Requirements
- C++17 compiler (GCC 10+ or Clang 10+)
- CMake 3.20+
- Qt 6.11+ with Widgets module

### Build CLI (Windows)

```cmd
git clone https://github.com/FarhanAlam2001/FarhanDB.git
cd FarhanDB
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
farhandb.exe
```

### Build GUI (Windows)

```cmd
git clone -b gui https://github.com/FarhanAlam2001/FarhanDB.git FarhanDB-GUI
cd FarhanDB-GUI
```
Then open `CMakeLists.txt` in Qt Creator and build.

### Build CLI (Linux/Mac)

```bash
git clone https://github.com/FarhanAlam2001/FarhanDB.git
cd FarhanDB
mkdir build && cd build
cmake .. && cmake --build .
./farhandb
```

---

## Usage — CLI

```
1. Beginner Mode   (step-by-step menu)
2. SQL Mode        (type SQL queries + database commands)
3. Server Mode     (TCP server on port 5555)
4. Exit
```

## Usage — GUI

Download the latest release, extract and run `FarhanDB-GUI.exe`.

- **SQL Mode** — Type any SQL query and press Run or Ctrl+Enter
- **Beginner Mode** — Use forms — no SQL needed
- **Server Mode** — Start TCP server and connect remotely
- **Dark/Light toggle** — Switch themes instantly
- **Sidebar** — Double-click a table to auto-run SELECT

---

## Data Storage

Each database gets its own set of files:

| File | Purpose |
|---|---|
| `<dbname>.db` | Actual table data |
| `<dbname>.wal` | Transaction log |
| `<dbname>.catalog` | Table definitions |

---

## Project Structure

```
FarhanDB/
├── src/
│   ├── storage/        # Page layout, disk I/O, buffer pool, WAL
│   ├── index/          # B+ Tree index
│   ├── query/          # Lexer, parser, catalog, optimizer, executor
│   ├── transaction/    # Lock manager, transaction manager
│   ├── server/         # TCP network server
│   ├── ui/             # Qt GUI windows and widgets (gui branch)
│   ├── bridge/         # Qt ↔ FarhanDB engine bridge (gui branch)
│   └── main.cpp
├── include/            # Header files
├── styles/             # QSS theme files (gui branch)
├── icons/              # App icons (gui branch)
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
- [x] ORDER BY, LIMIT, DISTINCT
- [x] GROUP BY, HAVING, OR conditions
- [x] BETWEEN, LIKE, SQL comments
- [x] NOT NULL, DEFAULT constraints
- [x] Subqueries, Foreign keys, Indexes
- [x] UPDATE multiple columns
- [x] ALTER TABLE (ADD/DROP COLUMN)
- [x] Multiple database support
- [x] Network/TCP interface
- [x] Qt6 Desktop GUI with light/dark themes
- [ ] Python client library
- [ ] Real index usage during SELECT
- [ ] Performance benchmarks

---

## License

MIT License — free to use, modify and distribute.

---

## Author

Built by **Md Farhan Alam** ([@FarhanAlam2001](https://github.com/FarhanAlam2001)) — a systems programming project demonstrating low-level database internals in C++.
