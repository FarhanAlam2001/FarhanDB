# FarhanDB 🗄️

A fully functional relational database engine built from scratch in **C++17**, featuring a custom SQL parser, buffer pool manager, B+ Tree index, Write-Ahead Log (WAL), and ACID transaction support.

> Built without any external database libraries — every component is implemented from the ground up.

---

## Features

- **Storage Engine** — Page-based storage with slotted page layout
- **Buffer Pool Manager** — LRU eviction policy with pin/unpin mechanism
- **Write-Ahead Log (WAL)** — Crash recovery and durability
- **B+ Tree Index** — Fast point lookups and range queries
- **SQL Parser** — Hand-written lexer and parser supporting core SQL
- **Transaction Manager** — ACID transactions with BEGIN/COMMIT/ROLLBACK
- **Lock Manager** — Row-level and table-level locking (Shared/Exclusive)
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
│   Lexer → Parser → Executor         │
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

### Build Steps

```bash
git clone https://github.com/FarhanAlam2001/FarhanDB.git
cd FarhanDB
mkdir build
cd build
cmake .. -G "MinGW Makefiles"   # Windows
cmake ..                         # Linux/Mac
cmake --build .
```

### Run

```bash
./farhandb       # Linux/Mac
farhandb.exe     # Windows
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
│   │   ├── page.cpp           # Slotted page layout
│   │   ├── disk_manager.cpp   # Raw disk I/O
│   │   ├── buffer_pool.cpp    # LRU buffer pool
│   │   └── wal.cpp            # Write-ahead log
│   ├── index/
│   │   └── btree.cpp          # B+ Tree index
│   ├── query/
│   │   ├── lexer.cpp          # SQL tokenizer
│   │   ├── parser.cpp         # SQL parser
│   │   ├── catalog.cpp        # Table catalog
│   │   └── executor.cpp       # Query executor
│   ├── transaction/
│   │   ├── lock_manager.cpp   # Locking
│   │   └── transaction_manager.cpp
│   └── main.cpp               # Entry point + UI
├── include/                   # Header files
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
- [ ] Multi-page table scanning
- [ ] JOIN support
- [ ] Aggregate functions (COUNT, SUM, AVG)
- [ ] Query optimizer
- [ ] Network/TCP interface

---

## License

MIT License — free to use, modify and distribute.

---

## Author

Built by **Farhan** — a systems programming project demonstrating low-level database internals in C++.
